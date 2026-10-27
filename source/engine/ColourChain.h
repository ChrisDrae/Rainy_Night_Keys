#pragma once

#include "EngineParams.h"
#include "Filters.h"
#include "Reverb.h"

#include <array>
#include <vector>

// =================================================================================================
//  ColourChain — everything that happens to the instrument's sound after the keys are played.
//
//    voices ─▶ Signal (DI · Amp · Room) ─▶ Body ─▶ Warmth ─▶ Motion ─▶ Patina ─▶ Space ─▶ + Dust ─▶
//
//    Signal  three "microphones" on one instrument: the dry DI, an amp (a Fender-style voicing that
//            breaks up a little on loud chords), and a room microphone (a short room reverb).
//    Body    low-mid fullness: a shelf at 170 Hz and a resonance at 330 Hz.
//    Warmth  a preamp: gentle saturation (tube or transistor), lows lifted, top softened.
//    Motion  suitcase-style stereo vibrato (the sound swings left and right), chorus, or both.
//    Patina  worn tape: saturation and a narrowing band. (Its wow and flutter bend the voices'
//            pitch, so they live in KeysEngine.)
//    Space   spring, plate or room reverb, with pre-delay, decay and tone.
//    Dust    tape hiss and vinyl crackle, added under everything.
//
//  Every stage whose knob is at 0 is skipped entirely: with the defaults of EngineParams the chain
//  is an exact bypass. Controls glide over ~30 ms and filters are re-tuned every kStep samples, so
//  moving a knob never clicks.
//
//  [RT] process() never allocates. prepare() sizes everything.
// =================================================================================================
namespace keys
{
class ColourChain
{
public:
    static constexpr int kStep = 32; ///< samples between control updates

    void prepare(double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    /// [RT] Processes stereo audio in place.
    void process(float* left, float* right, int numSamples, const EngineParams& params) noexcept;

private:
    struct Stage
    {
        bool active = false;
        /// True on the sub-block a stage switches on, so it can clear stale filter state.
        bool update(bool shouldRun) noexcept
        {
            const bool starting = shouldRun && !active;
            active = shouldRun;
            return starting;
        }
    };

    void setTargets(const EngineParams& params) noexcept;
    void snapAll() noexcept;
    void advanceControls() noexcept;

    void processSignal(float* l, float* r, int n) noexcept;
    void processBody(float* l, float* r, int n) noexcept;
    void processWarmth(float* l, float* r, int n) noexcept;
    void processMotion(float* l, float* r, int n) noexcept;
    void processPatina(float* l, float* r, int n) noexcept;
    void processSpace(float* l, float* r, int n) noexcept;
    void processDust(float* l, float* r, int n) noexcept;

    double sampleRate = 48000.0;
    bool snapNext = true;

    // Signal mixer
    Stage signalStage;
    dsp::Glide di, amp, room;
    float diFrom = 1.0f, ampFrom = 0.0f, roomFrom = 0.0f;
    dsp::SmoothSaturator ampDrive;
    /// A Twin-style amp: speaker low end, the tone stack's mid scoop, presence, speaker roll-off.
    std::array<std::array<dsp::Biquad, 6>, 2> ampVoicing;
    Reverb roomMic;
    std::vector<float> roomL, roomR;

    // Body
    Stage bodyStage;
    dsp::Glide body, bodyResonance;
    std::array<dsp::Biquad, 2> bodyLow, bodyMid;

    // Warmth
    Stage warmthStage;
    dsp::Glide warmth, warmthDrive, warmthTilt;
    WarmthCharacter character = WarmthCharacter::tube;
    dsp::SmoothSaturator warmthSaturator;
    std::array<dsp::Biquad, 2> warmthLow, warmthHigh;

    // Motion
    Stage motionStage;
    dsp::Glide vibratoDepth, chorusWet, chorusDepth;
    float vibratoFrom = 0.0f, chorusWetFrom = 0.0f;
    dsp::Lfo vibratoLfo, chorusLfo;
    std::array<dsp::DelayLine, 2> chorusDelay;

    // Patina
    Stage patinaStage;
    dsp::Glide patina, narrowing;
    dsp::SmoothSaturator tapeSaturator;
    std::array<dsp::Biquad, 2> tapeHighPass, tapeLowPass;

    // Space
    Stage spaceStage;
    dsp::Glide spaceWet;
    float spaceWetFrom = 0.0f;
    Reverb reverb;
    SpaceType wantedSpaceType = SpaceType::plate;
    float spaceSwitchGain = 1.0f; ///< dips to 0 while the reverb changes type
    float spacePreDelay = 24.0f, spaceDecay = 2.0f, spaceTone = 0.55f;
    std::vector<float> wetL, wetR;

    // Dust
    Stage dustStage;
    dsp::Glide dust;
    float dustFrom = 0.0f;
    dsp::Noise noise { 0x51f15e };
    std::array<float, 2> hissPrevious {};
    std::array<dsp::OnePole, 2> hissTone;
    std::array<std::array<float, 3>, 2> crackle {}; ///< clicks in progress, per channel
};
} // namespace keys
