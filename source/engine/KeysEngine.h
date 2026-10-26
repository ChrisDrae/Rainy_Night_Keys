#pragma once

#include "SampleBank.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

// =================================================================================================
//  KeysEngine — turns MIDI into sound: voices, the sustain pedal, the Tine ↔ Bell blend.
//
//  Every played key gets one *voice*. A voice holds two sample players, one for the tine layer and
//  one for the bell layer, which run side by side. The engine mixes all tine players into one bus
//  and all bell players into another, then blends the two buses with the Blend control, so moving
//  Blend changes notes that are already ringing, too.
//
//  [RT] process() never allocates, locks or does I/O. All buffers are sized in prepare().
// =================================================================================================
namespace keys
{
enum class BellVoice
{
    celeste = 0,
    vibes = 1,
    both = 2, ///< vibes below middle C, celeste from middle C up
};

struct EngineParams
{
    float blend = 0.35f;  ///< 0 = tines only, 0.5 = both at full, 1 = bell only
    BellVoice bellVoice = BellVoice::celeste;
    int bellOctave = 0;   ///< 0 or +1
    int bellLowestKey = 41; ///< the bell fades out over the 6 keys below this (F2)
    float touch = 0.5f;   ///< velocity curve: 0 = even, 1 = very dynamic
    float tuneCents = 0.0f;
    float volumeDb = 0.0f;
};

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

    void pushEvent(const MidiEventInfo& e) noexcept;
    static MidiEventInfo unpack(uint32_t packed) noexcept;
};

class KeysEngine
{
public:
    static constexpr int kPolyphony = 64;  ///< voices that may sound at full level
    static constexpr int kVoiceSlots = 80; ///< extra slots let stolen voices fade out instead of clicking

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
        double baseStep = 1.0; ///< playback rate before the global tune
        float gain = 0.0f;
        float envelope = 1.0f;
        float releaseCoef = 1.0f;
        float releaseTau = 0.1f;
        bool releasing = false;

        bool isActive() const noexcept { return sample != nullptr; }
        void stop() noexcept { sample = nullptr; }
        void release(float coef) noexcept;
        /// Adds this player into the bus; returns false when it has finished.
        bool render(float* left, float* right, int numSamples, double tuneRatio) noexcept;
    };

    struct Voice
    {
        int note = -1;
        bool keyDown = false;   ///< the key is still held
        bool sustained = false; ///< key released, but the pedal holds the note
        bool stolen = false;
        uint64_t age = 0;
        Player tine, bell;

        bool isActive() const noexcept { return tine.isActive() || bell.isActive(); }
        bool isReleasing() const noexcept { return (!tine.isActive() || tine.releasing) && (!bell.isActive() || bell.releasing); }
    };

    void handleMidi(const juce::MidiMessage& message, const EngineParams& params, const Banks& banks) noexcept;
    void noteOn(int note, int velocity, const EngineParams& params, const Banks& banks) noexcept;
    void noteOff(int note) noexcept;
    void setSustain(bool down) noexcept;
    void releaseVoice(Voice& voice, float tauOverride = 0.0f) noexcept;
    Voice& allocateVoice() noexcept;
    void startPlayer(Player& player, const Zone& zone, int playedNote, int velocity, float gain, float releaseTau) noexcept;
    void renderSpan(float* left, float* right, int start, int numSamples, double tuneRatio) noexcept;

    std::array<Voice, kVoiceSlots> voices {};
    juce::AudioBuffer<float> tineBus, bellBus; // scratch, sized in prepare()
    juce::SmoothedValue<float> tineGain { 1.0f }, bellGain { 0.0f }, outputGain { 1.0f };
    double sampleRate = 48000.0;
    int maxBlock = 512;
    uint64_t noteCounter = 0;
    bool sustainDown = false;
    bool snapSmoothers = true;
    EngineStatus engineStatus;
};
} // namespace keys
