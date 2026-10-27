#pragma once

#include "EngineParams.h"
#include "Filters.h"

#include <array>

// =================================================================================================
//  Reverb — the Space knob's room, and the Room microphone of the signal mixer.
//
//  Plate and Room are a *feedback delay network* (FDN): eight delay lines whose outputs are mixed
//  together (a Householder matrix: every line feeds every other) and fed back, a little quieter
//  and a little darker each time round. Echoes multiply until they blur into a smooth tail.
//  Before the network, four allpass filters smear the input so even a sharp attack enters as a
//  wash. Two of the lines are modulated very slowly, which keeps long tails from ringing metallic.
//
//    Plate  long lines, bright, dense from the start.
//    Room   short lines, plus distinct early reflections off nearby walls.
//    Spring two dispersive loops: each pass delays low frequencies more than high ones, so every
//           echo comes back as a falling "boing" chirp, the sound of a spring tank. A darker FDN
//           underneath adds the density of a real tank's several springs.
//
//  [RT] process() never allocates. prepare() sizes every buffer for the largest setting.
// =================================================================================================
namespace keys
{
class Reverb
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    /// Changing the type re-tunes the delay lines; call reset() (or fade out) around it.
    void setType(SpaceType type) noexcept;
    SpaceType getType() const noexcept { return type; }

    /// −60 dB decay time of the tail, 0.2..8 s.
    void setDecay(float seconds) noexcept;
    /// 0 = dark, 1 = bright.
    void setTone(float tone) noexcept;
    /// 0..120 ms. Changes glide, so moving it never clicks.
    void setPreDelay(float milliseconds) noexcept;

    /// [RT] Writes only the reverberated signal (no dry) into outL / outR.
    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    static constexpr int kLines = 8;
    static constexpr float kMaxPreDelayMs = 120.0f;

private:
    using Delay = dsp::DelayLine;

    struct Allpass
    {
        Delay delay;
        int length = 1;
        float gain = 0.6f;

        float process(float x) noexcept
        {
            const float d = delay.read(length);
            const float v = x + gain * d;
            delay.push(v);
            return d - gain * v;
        }
    };

    /// A first-order allpass "stretched" over K samples: a chain of these makes the spring chirp.
    struct StretchedAllpass
    {
        static constexpr int kMaxK = 16;
        std::array<float, kMaxK> xs {}, ys {};
        int index = 0;

        float process(float x, float a, int k) noexcept
        {
            const int past = (index - k) & (kMaxK - 1);
            const float y = a * x + xs[static_cast<size_t>(past)] - a * ys[static_cast<size_t>(past)];
            xs[static_cast<size_t>(index)] = x;
            ys[static_cast<size_t>(index)] = y;
            index = (index + 1) & (kMaxK - 1);
            return y;
        }
        void clear() noexcept
        {
            xs.fill(0.0f);
            ys.fill(0.0f);
        }
    };

    struct SpringLoop
    {
        static constexpr int kStages = 24;
        std::array<StretchedAllpass, kStages> chain;
        Delay delay;
        int length = 1;
        float feedback = 0.0f, state = 0.0f;
        dsp::OnePole lowPass1, lowPass2, highPassTrack;
    };

    void updateGains() noexcept;

    double sampleRate = 48000.0;
    SpaceType type = SpaceType::plate;
    float decaySeconds = 2.0f, tone = 0.55f;

    Delay preDelay;
    dsp::Glide preDelaySamples;
    std::array<Allpass, 4> diffusers;
    std::array<Delay, kLines> lines;
    std::array<int, kLines> lineLength {};
    std::array<float, kLines> lineGain {};
    std::array<dsp::OnePole, kLines> damping;
    dsp::Lfo modA, modB;
    float modDepth = 0.0f;
    float earlyLevel = 0.0f, tailLevel = 1.0f;
    std::array<int, 8> earlyTaps {};

    std::array<SpringLoop, 2> springs;
    int springK = 5;
    float springLevel = 0.0f;

    std::array<dsp::OnePole, 2> outputTone;
};
} // namespace keys
