#pragma once

#include "DspMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// =================================================================================================
//  Small building blocks for the colour chain: biquad EQs, one-pole filters, saturation curves,
//  an LFO and a noise source. Header-only, no allocation, safe to use on the audio thread.
// =================================================================================================
namespace keys::dsp
{
/// [DSP] Second-order IIR filter (transposed direct form II), with the usual "Audio EQ Cookbook"
/// designs (Robert Bristow-Johnson). One instance filters one channel.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    float process(float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    void setLowPass(double hz, double q, double sampleRate) noexcept { design(Kind::lowPass, hz, q, 0.0, sampleRate); }
    void setHighPass(double hz, double q, double sampleRate) noexcept { design(Kind::highPass, hz, q, 0.0, sampleRate); }
    void setPeak(double hz, double q, double db, double sampleRate) noexcept { design(Kind::peak, hz, q, db, sampleRate); }
    void setBandPass(double hz, double q, double sampleRate) noexcept { design(Kind::bandPass, hz, q, 0.0, sampleRate); }
    void setLowShelf(double hz, double db, double sampleRate) noexcept { design(Kind::lowShelf, hz, 0.7071, db, sampleRate); }
    void setHighShelf(double hz, double db, double sampleRate) noexcept { design(Kind::highShelf, hz, 0.7071, db, sampleRate); }

private:
    enum class Kind
    {
        lowPass,
        highPass,
        bandPass,
        peak,
        lowShelf,
        highShelf
    };

    void design(Kind kind, double hz, double q, double db, double sampleRate) noexcept
    {
        hz = std::clamp(hz, 10.0, 0.45 * sampleRate);
        const double w = kTwoPi * hz / sampleRate;
        const double cw = std::cos(w), sw = std::sin(w);
        const double alpha = sw / (2.0 * std::max(0.05, q));
        const double A = std::pow(10.0, db / 40.0);
        double c0 = 1, c1 = 0, c2 = 0, d0 = 1, d1 = 0, d2 = 0;
        switch (kind)
        {
            case Kind::lowPass:
                c0 = (1 - cw) / 2, c1 = 1 - cw, c2 = (1 - cw) / 2;
                d0 = 1 + alpha, d1 = -2 * cw, d2 = 1 - alpha;
                break;
            case Kind::highPass:
                c0 = (1 + cw) / 2, c1 = -(1 + cw), c2 = (1 + cw) / 2;
                d0 = 1 + alpha, d1 = -2 * cw, d2 = 1 - alpha;
                break;
            case Kind::bandPass: // 0 dB at the centre frequency
                c0 = alpha, c1 = 0, c2 = -alpha;
                d0 = 1 + alpha, d1 = -2 * cw, d2 = 1 - alpha;
                break;
            case Kind::peak:
                c0 = 1 + alpha * A, c1 = -2 * cw, c2 = 1 - alpha * A;
                d0 = 1 + alpha / A, d1 = -2 * cw, d2 = 1 - alpha / A;
                break;
            case Kind::lowShelf:
            {
                const double s = 2 * std::sqrt(A) * alpha;
                c0 = A * ((A + 1) - (A - 1) * cw + s), c1 = 2 * A * ((A - 1) - (A + 1) * cw),
                c2 = A * ((A + 1) - (A - 1) * cw - s);
                d0 = (A + 1) + (A - 1) * cw + s, d1 = -2 * ((A - 1) + (A + 1) * cw), d2 = (A + 1) + (A - 1) * cw - s;
                break;
            }
            case Kind::highShelf:
            {
                const double s = 2 * std::sqrt(A) * alpha;
                c0 = A * ((A + 1) + (A - 1) * cw + s), c1 = -2 * A * ((A - 1) + (A + 1) * cw),
                c2 = A * ((A + 1) + (A - 1) * cw - s);
                d0 = (A + 1) - (A - 1) * cw + s, d1 = 2 * ((A - 1) - (A + 1) * cw), d2 = (A + 1) - (A - 1) * cw - s;
                break;
            }
        }
        b0 = static_cast<float>(c0 / d0);
        b1 = static_cast<float>(c1 / d0);
        b2 = static_cast<float>(c2 / d0);
        a1 = static_cast<float>(d1 / d0);
        a2 = static_cast<float>(d2 / d0);
    }
};

/// [DSP] One-pole low-pass: y += a · (x − y). Also the smoother behind every live control.
struct OnePole
{
    float a = 1.0f, y = 0.0f;

    void setCutoff(double hz, double sampleRate) noexcept
    {
        a = static_cast<float>(1.0 - std::exp(-kTwoPi * std::clamp(hz, 1.0, 0.49 * sampleRate) / sampleRate));
    }
    float process(float x) noexcept { return y += a * (x - y); }
    void reset(float value = 0.0f) noexcept { y = value; }
};

/// [DSP] DC blocker: a first-order high-pass around 10 Hz.
struct DcBlocker
{
    float r = 0.9987f, x1 = 0.0f, y1 = 0.0f;

    void prepare(double sampleRate) noexcept { r = static_cast<float>(1.0 - kTwoPi * 10.0 / sampleRate); }
    float process(float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

/// [DSP] tanh for audio: a rational approximation, within 2 % of std::tanh and several times
/// cheaper. Exact at 0, odd, and it saturates smoothly at ±1.
inline float fastTanh(float x) noexcept
{
    x = std::clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/// [DSP] A saturation curve that leaves quiet signals untouched.
///
/// A tanh whose knee sits near `kReference` (a typical note's peak) divided by the drive, scaled
/// so its slope at silence is exactly 1, then mixed with the dry signal by `amount`. A `bias`
/// shifts the curve sideways so the two halves of the wave bend differently: that asymmetry adds
/// even harmonics (the "bark" of a tine close to its pickup, or the warmth of a tube). The
/// constant part is removed, so silence stays silent, and the output is bounded at any drive.
struct Saturator
{
    static constexpr float kReference = 0.25f;

    enum class Curve
    {
        tube,       ///< gentle, slightly asymmetric
        transistor, ///< firmer and symmetric: odd harmonics
        bark        ///< strongly asymmetric: a tine near its pickup
    };

    float inputGain = 1.0f, bias = 0.0f, centre = 0.0f, norm = 1.0f, mix = 0.0f;

    /// amount 0 = clean (an exact bypass), 1 = heavily driven. `reference` is the level the
    /// curve is tuned for (a loud note's peak): the knee sits there, divided by the drive.
    void set(Curve curve, float amount, float reference = kReference) noexcept
    {
        amount = std::clamp(amount, 0.0f, 1.5f);
        mix = std::clamp(amount * 1.4f, 0.0f, 1.0f);
        float drive = 0.6f + amount * 1.6f;
        if (curve == Curve::transistor)
            drive *= 1.4f;
        bias = curve == Curve::bark ? amount * 0.5f : curve == Curve::tube ? amount * 0.15f : 0.0f;
        centre = fastTanh(bias);
        constexpr float h = 1.0e-3f;
        const float slope = (fastTanh(bias + h) - fastTanh(bias - h)) / (2.0f * h);
        inputGain = drive / std::max(1.0e-4f, reference);
        norm = 1.0f / (inputGain * std::max(1.0e-3f, slope));
    }

    bool isActive() const noexcept { return mix > 0.0f; }

    float process(float x) const noexcept
    {
        const float shaped = (fastTanh(inputGain * x + bias) - centre) * norm;
        return x + (shaped - x) * mix;
    }
};

/// [DSP] The Saturator's curve with first-order antiderivative anti-aliasing (ADAA; Parker,
/// Zavalishin & Le Bivic, DAFx 2016). Instead of the curve's value at each sample it outputs the
/// curve's *average* between the previous sample and this one: the difference of its
/// antiderivative divided by the step. Harmonics that would fold back from above Nyquist come out
/// much lower, for one logarithm per sample. The price is half a sample of delay.
struct SmoothSaturator
{
    Saturator curve;
    std::array<double, 2> previous {}; ///< the last curve input, per channel

    void set(Saturator::Curve type, float amount, float reference = Saturator::kReference) noexcept
    {
        curve.set(type, amount, reference);
    }
    void reset() noexcept { previous = {}; }

    float process(float x, size_t channel) noexcept
    {
        const double u = static_cast<double>(curve.inputGain) * x + curve.bias;
        double& p = previous[channel];
        const double step = u - p;
        const double average = std::abs(step) > 1.0e-5
                                   ? (antiderivative(u) - antiderivative(p)) / step
                                   : static_cast<double>(fastTanh(static_cast<float>(0.5 * (u + p))));
        p = u;
        const float shaped = static_cast<float>((average - curve.centre) * curve.norm);
        return x + (shaped - x) * curve.mix;
    }

    /// The integral of fastTanh: u^2/18 + 4/3 ln(9u^2 + 27) inside +-3, then a straight line.
    static double antiderivative(double u) noexcept
    {
        const double a = std::abs(u);
        if (a > 3.0)
            return 6.7428403 + (a - 3.0); // F(3), then the slope of the flat part
        return u * u / 18.0 + (4.0 / 3.0) * std::log(9.0 * u * u + 27.0);
    }
};

/// A circular delay buffer (power-of-two size, so wrapping is a bit mask).
struct DelayLine
{
    std::vector<float> buffer;
    int mask = 0, write = 0;

    /// Message thread: room for delays up to `maxSamples`.
    void allocate(int maxSamples)
    {
        int size = 4;
        while (size < maxSamples + 4)
            size <<= 1;
        buffer.assign(static_cast<size_t>(size), 0.0f);
        mask = size - 1;
        write = 0;
    }
    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        write = 0;
    }
    void push(float x) noexcept
    {
        buffer[static_cast<size_t>(write)] = x;
        write = (write + 1) & mask;
    }
    /// The sample pushed `delay` samples ago (1 = the most recent).
    float read(int delay) const noexcept { return buffer[static_cast<size_t>((write - delay) & mask)]; }
    /// [DSP] A fractional delay, read with Hermite interpolation (smooth enough to modulate).
    float readFractional(float delay) const noexcept
    {
        delay = std::max(1.0f, delay);
        const int whole = static_cast<int>(delay);
        const float t = delay - static_cast<float>(whole);
        return interpolateHermite(read(whole - 1 < 1 ? 1 : whole - 1), read(whole), read(whole + 1), read(whole + 2), t);
    }
};

/// [DSP] Sine LFO with a running phase (0..1).
struct Lfo
{
    double phase = 0.0, increment = 0.0;

    void setRate(double hz, double sampleRate) noexcept { increment = hz / sampleRate; }
    float next() noexcept
    {
        const float value = static_cast<float>(std::sin(kTwoPi * phase));
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
        return value;
    }
    float valueAt(double phaseOffset) const noexcept { return static_cast<float>(std::sin(kTwoPi * (phase + phaseOffset))); }
};

/// White noise from a 32-bit xorshift: deterministic, branch-free, real-time safe.
struct Noise
{
    uint32_t state = 0x1234567u;

    explicit Noise(uint32_t seed = 0x1234567u) : state(seed == 0 ? 0x9E3779B9u : seed) {}
    float next() noexcept // −1 .. 1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) * (1.0f / 2147483648.0f) - 1.0f;
    }
    float unit() noexcept { return 0.5f * (next() + 1.0f); } // 0 .. 1
};

/// A parameter that glides to its target. Advanced once per sub-block, so filter coefficients can
/// follow it without zipper noise.
struct Glide
{
    float value = 0.0f, target = 0.0f, coef = 0.1f;

    /// `stepSamples` is how many samples each advance() covers.
    void prepare(double sampleRate, int stepSamples, double seconds = 0.03) noexcept
    {
        coef = static_cast<float>(1.0 - std::exp(-static_cast<double>(stepSamples) / (seconds * sampleRate)));
    }
    void snap(float v) noexcept { value = target = v; }
    void advance() noexcept
    {
        value += (target - value) * coef;
        if (std::abs(target - value) < 1.0e-5f)
            value = target;
    }
    bool settledAt(float v) const noexcept { return value == v && target == v; }
};
} // namespace keys::dsp
