#pragma once

#include "ColourChain.h"
#include "EngineParams.h"
#include "Filters.h"
#include "SampleBank.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

// =================================================================================================
//  KeysEngine — turns MIDI into sound: voices, the sustain pedal, the Tine ↔ Bell blend, the colour.
//
//  Every played key gets one *voice*. A voice holds sample players for the tine layer (plus an
//  optional detuned double, Body's "Double") and for the bell layer, which run side by side. Each
//  voice shapes its own tine: the attack's ding, then the bark of a pickup driven harder by harder
//  playing. Then it is placed across the stereo field by its key (Body's "Width").
//
//  The engine mixes all tine players into one bus and all bell players into another, then blends
//  the two buses with the Blend control, so moving Blend changes notes that are already ringing.
//  Key, damper and pedal noises (Mechanics) are added, and the result runs through the
//  ColourChain (signal mixer, Body, Warmth, Motion, Patina, Space, Dust) and the output level.
//
//  [RT] process() never allocates, locks or does I/O. All buffers are sized in prepare().
// =================================================================================================
namespace keys
{
/// Everything the audio thread may play from, for one block.
struct Banks
{
    const SampleBank* tines = nullptr;
    const SampleBank* celeste = nullptr;
    const SampleBank* vibes = nullptr;
};

/// A MIDI event as the UI's monitor shows it.
struct MidiEventInfo
{
    enum class Type : uint8_t
    {
        noteOn,
        noteOff,
        controller,
        other
    };
    Type type = Type::other;
    uint8_t channel = 1;
    uint8_t number = 0; ///< note or controller number
    uint8_t value = 0;  ///< velocity or controller value
};

/// What the light behind the controls reacts to: notes starting and stopping, and the pedal.
struct NoteEvent
{
    enum class Type : uint8_t
    {
        noteOn,
        noteOff,
        pedalDown,
        pedalUp
    };
    Type type = Type::noteOn;
    uint8_t note = 0;
    uint8_t velocity = 0;
};

/// A lock-free queue from the audio thread (one writer) to the UI (one reader). When nobody reads
/// it, new events are dropped rather than blocking.
class NoteEventQueue
{
public:
    static constexpr int kCapacity = 256;

    /// [RT] Audio thread.
    void push(const NoteEvent& e) noexcept;
    /// UI thread. Returns how many events were copied into `out`.
    int pop(NoteEvent* out, int maxEvents) noexcept;

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<NoteEvent, kCapacity> items {};
};

/// What the editor shows. Written by the audio thread, read by the UI timer.
struct EngineStatus
{
    std::atomic<int> lastNote { -1 };
    std::atomic<int> lastVelocity { 0 };
    std::atomic<uint32_t> midiEventCount { 0 };
    std::atomic<bool> sustainPedal { false };
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> outputPeak { 0.0f };

    /// Last MIDI events, newest at (writeIndex - 1) % size. Each slot is packed into one atomic.
    static constexpr int kHistory = 16;
    std::array<std::atomic<uint32_t>, kHistory> history {};
    std::atomic<uint32_t> historyWrite { 0 };

    NoteEventQueue notes;

    void pushEvent(const MidiEventInfo& e) noexcept;
    static MidiEventInfo unpack(uint32_t packed) noexcept;
};

class KeysEngine
{
public:
    static constexpr int kPolyphony = 64;  ///< voices that may sound at full level
    static constexpr int kVoiceSlots = 80; ///< extra slots let stolen voices fade out instead of clicking
    static constexpr int kThumps = 16;     ///< mechanical noises that can sound at once

    KeysEngine();

    /// Message thread, before playback (and whenever the rate or block size changes).
    void prepare(double sampleRate, int maximumBlockSize);

    /// [RT] Renders one block: clears `buffer`, plays the MIDI in it, writes stereo (or mono) out.
    void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const EngineParams& params,
                 const Banks& banks) noexcept;

    /// [RT] Releases every voice (as if every key and the pedal were let go).
    void allNotesOff() noexcept;

    EngineStatus& status() noexcept { return engineStatus; }
    int countActiveVoices() const noexcept;

private:
    struct Player
    {
        const Sample* sample = nullptr;
        double position = 0.0;
        double baseStep = 1.0; ///< playback rate before tuning and wow
        float gain = 0.0f;
        float envelope = 1.0f;
        float releaseCoef = 1.0f;
        float releaseTau = 0.1f;
        bool releasing = false;

        bool isActive() const noexcept { return sample != nullptr; }
        void stop() noexcept { sample = nullptr; }
        void release(float coef) noexcept;
        /// Adds this player into left/right, `ratio` being the per-sample pitch ratio. A level of 0
        /// only moves the play position on (so a double stays in step). Returns false when done.
        bool render(float* left, float* right, int numSamples, const float* ratio, float level) noexcept;
    };

    struct Voice
    {
        int note = -1;
        bool keyDown = false;   ///< the key is still held
        bool sustained = false; ///< key released, but the pedal holds the note
        bool stolen = false;
        uint64_t age = 0;
        float velocity = 0.0f; ///< 0..1, after the Touch curve
        float barkReference = 0.25f; ///< a loud note's peak in this voice's tine bank
        Player tine, tineDouble, bell;

        // Per-voice colour state.
        float panLeft = 1.0f, panRight = 1.0f;
        float ding = 0.0f; ///< the attack envelope Tine ding acts on
        std::array<dsp::OnePole, 2> dingSplit;
        std::array<dsp::DcBlocker, 2> barkDc;

        bool isActive() const noexcept { return tine.isActive() || tineDouble.isActive() || bell.isActive(); }
        bool isReleasing() const noexcept
        {
            return (!tine.isActive() || tine.releasing) && (!tineDouble.isActive() || tineDouble.releasing)
                   && (!bell.isActive() || bell.releasing);
        }
    };

    /// A burst of band-passed noise: a key landing, a damper falling, the pedal.
    struct Thump
    {
        dsp::Biquad filter;
        float level = 0.0f, envelope = 0.0f, attackStep = 0.0f, decay = 0.0f;
        bool attacking = false, active = false;
    };

    void handleMidi(const juce::MidiMessage& message, const EngineParams& params, const Banks& banks) noexcept;
    void noteOn(int note, int velocity, const EngineParams& params, const Banks& banks) noexcept;
    void noteOff(int note, const EngineParams& params) noexcept;
    void setSustain(bool down, const EngineParams& params) noexcept;
    void releaseVoice(Voice& voice, float tauOverride = 0.0f) noexcept;
    Voice& allocateVoice() noexcept;
    void startPlayer(Player& player, const Zone& zone, int playedNote, int velocity, float gain,
                     float releaseTau) noexcept;
    void triggerThump(float level, double hz, double q, double tauSeconds) noexcept;
    void renderSpan(float* left, float* right, int start, int numSamples, const EngineParams& params) noexcept;
    void renderVoice(Voice& voice, int numSamples, const EngineParams& params) noexcept;
    void preparePitch(int numSamples, const EngineParams& params) noexcept;

    std::array<Voice, kVoiceSlots> voices {};
    std::array<Thump, kThumps> thumps {};
    juce::AudioBuffer<float> tineBus, bellBus, voiceScratch, chunkMix; // sized in prepare()
    std::vector<float> pitchRatio, doubleRatio, thumpBus;
    juce::SmoothedValue<float> tineGain { 1.0f }, bellGain { 0.0f }, outputGain { 1.0f };
    ColourChain colour;

    // Tape wow and flutter (Patina), as a shared pitch wobble for every voice.
    dsp::Lfo wowLfo, wowDrift, flutterLfo;
    float wowCents = 0.0f, flutterCents = 0.0f;
    float mechanicsScale = 1.0f; ///< key noises follow the tines' level (see process())
    dsp::Noise noise { 0xC0FFEE };

    double sampleRate = 48000.0;
    int maxBlock = 512;
    uint64_t noteCounter = 0;
    bool sustainDown = false;
    bool snapSmoothers = true;
    EngineStatus engineStatus;
};
} // namespace keys
