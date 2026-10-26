#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

// =================================================================================================
//  Small, dependency-free math helpers shared by the engine (several adapted from Pocket Looper).
// =================================================================================================
namespace keys::dsp
{
inline constexpr double kPi = std::numbers::pi;
inline constexpr double kTwoPi = 2.0 * std::numbers::pi;

/// Our level faders bottom out here; anything at or below it is silence.
inline constexpr float kMinusInfinityDb = -60.0f;

/// [DSP] Decibels to linear gain: gain = 10^(dB/20).
inline float dbToGain(float db) noexcept
{
    return db <= kMinusInfinityDb ? 0.0f : std::pow(10.0f, db * 0.05f);
}

/// Equal-tempered frequency of a MIDI note (A4 = note 69 = 440 Hz).
inline double noteToHz(double note) noexcept
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

/// Playback-rate ratio for a pitch shift in semitones.
inline double semitonesToRatio(double semitones) noexcept
{
    return std::pow(2.0, semitones / 12.0);
}

/// [DSP] Per-sample multiplier for an exponential decay with time constant `tauSeconds`
/// (the level falls to 1/e, about −8.7 dB, after tau).
inline float decayCoefficient(double tauSeconds, double sampleRate) noexcept
{
    return tauSeconds <= 0.0 ? 0.0f : static_cast<float>(std::exp(-1.0 / (tauSeconds * sampleRate)));
}

/// [DSP] 4-point, 3rd-order Hermite (Catmull-Rom) interpolation between x0 (t = 0) and x1 (t = 1).
/// The standard choice for varispeed sample playback: much flatter than linear, still cheap.
inline float interpolateHermite(float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

/// [DSP] A gentle output safety: linear up to `knee`, then a smooth tanh shoulder that never
/// exceeds 1.0. Chords that sum hot get rounded instead of clipped.
inline float softClip(float x, float knee = 0.8f) noexcept
{
    const float a = std::abs(x);
    if (a <= knee)
        return x;
    const float room = 1.0f - knee;
    const float shaped = knee + room * std::tanh((a - knee) / room);
    return std::copysign(shaped, x);
}
} // namespace keys::dsp
