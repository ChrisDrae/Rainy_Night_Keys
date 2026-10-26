#include "TineModel.h"

#include "DspMath.h"

#include <algorithm>
#include <cmath>

namespace keys
{
namespace
{
/// 0 at the bottom of a 73-key Mk II (E1 = 28), 1 at the top (E7 = 100).
double registerOf(int note) noexcept
{
    return std::clamp((note - 28) / 72.0, 0.0, 1.0);
}

/// A tiny deterministic random generator (xorshift32), so renders are repeatable.
struct Rng
{
    uint32_t state;
    explicit Rng(uint32_t seed) : state(seed == 0 ? 0x9E3779B9u : seed) {}
    float next() noexcept // −1 .. 1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) / 2147483648.0f - 1.0f;
    }
};

/// One exponentially decaying sine partial, advanced sample by sample without calling exp().
struct Partial
{
    double phase = 0.0, increment = 0.0;
    double level = 0.0, decay = 1.0;

    Partial(double hz, double amplitude, double tauSeconds, double sampleRate)
        : increment(dsp::kTwoPi * hz / sampleRate),
          level(amplitude),
          decay(std::exp(-1.0 / (std::max(1.0e-4, tauSeconds) * sampleRate)))
    {
    }

    double next() noexcept
    {
        const double value = level * std::sin(phase);
        phase += increment;
        if (phase > dsp::kTwoPi)
            phase -= dsp::kTwoPi;
        level *= decay;
        return value;
    }
};

/// Soft roll-off of overtones that would sit above the pickup's useful range.
double bandWeight(double hz, double cornerHz) noexcept
{
    const double r = hz / cornerHz;
    return 1.0 / (1.0 + r * r);
}
} // namespace

double tineDecaySeconds(int note) noexcept
{
    // Bass tines sing for ~9 s, the top octave for under 1.5 s.
    return 9.0 * std::pow(1.4 / 9.0, registerOf(note));
}

double tineRenderSeconds(int note) noexcept
{
    return 8.0 + (2.5 - 8.0) * registerOf(note);
}

void renderTine(float* out, int numFrames, double sampleRate, int note, float velocity, uint32_t seed,
                const TineSettings& settings)
{
    if (out == nullptr || numFrames <= 0)
        return;

    Rng rng(seed * 2654435761u + static_cast<uint32_t>(note) * 40503u + 17u);
    const double v = std::clamp(static_cast<double>(velocity), 0.02, 1.0);
    const double reg = registerOf(note);
    const double f0 = dsp::noteToHz(note);
    const double t60 = tineDecaySeconds(note);
    const double tau = t60 / 6.9; // −60 dB = e^−6.9

    // --- tine displacement: fundamental (two-stage decay) + attack overtones + tonebar ------------
    const double amplitude = std::pow(v, 1.1);
    const double fastShare = 0.3 + 0.25 * v; // harder hits lose more energy in the first moments
    Partial fundamentalSlow(f0, amplitude * (1.0 - fastShare), tau, sampleRate);
    Partial fundamentalFast(f0, amplitude * fastShare, tau * 0.18, sampleRate);

    const double dingRatio = 6.27 * (1.0 + 0.012 * rng.next());
    const double dingHz = f0 * dingRatio;
    Partial ding(dingHz, settings.ding * 0.045 * std::pow(v, 1.5) * bandWeight(dingHz, 7000.0),
                 0.035 + 0.22 * (1.0 - reg), sampleRate);

    const double thirdHz = f0 * 17.55;
    Partial third(thirdHz, settings.ding * 0.008 * v * v * bandWeight(thirdHz, 6000.0), 0.015, sampleRate);

    // The tonebar starts silent and takes ~50 ms to pick up energy from the tine.
    const double beatCents = 0.8 + 0.6 * rng.next();
    Partial tonebar(f0 * dsp::semitonesToRatio(beatCents / 100.0), amplitude * 0.12, tau * 1.3, sampleRate);
    const double tonebarRise = std::exp(-1.0 / (0.05 * sampleRate));

    // The hammer stays on the tine for a moment: softer hits are rounder (slower contact).
    const double contactTau = 0.0006 + 0.0028 * (1.0 - v);
    const double contactCoef = std::exp(-1.0 / (contactTau * sampleRate));

    // --- pickup --------------------------------------------------------------------------------------
    const double k = settings.pickupDrive * (1.0 + 0.35 * (1.0 - reg)); // bass tines swing wider
    const double offset = settings.pickupOffset;
    const double sech = 1.0 / std::cosh(k * offset);
    const double smallSignalGain = k * sech * sech;
    const double derivativeScale = sampleRate / (dsp::kTwoPi * f0) / smallSignalGain;

    // --- acoustic hammer thump: a short, dark noise burst ----------------------------------------------
    const double thumpLevel = settings.hammerNoise * 0.03 * std::pow(v, 1.5);
    const double thumpDecay = std::exp(-1.0 / (0.007 * sampleRate));
    const double thumpLp = 1.0 - std::exp(-dsp::kTwoPi * 1200.0 / sampleRate);

    // --- output tone: harder playing is brighter --------------------------------------------------------
    const double toneLp = 1.0 - std::exp(-dsp::kTwoPi * (3800.0 + 3600.0 * v) / sampleRate);

    double contact = 1.0, tonebarEnv = 1.0, previousFlux = std::tanh(k * offset);
    double thumpEnv = 1.0, thumpState = 0.0, toneState = 0.0;
    const int fadeFrames = std::min(numFrames / 4, static_cast<int>(0.05 * sampleRate));

    for (int i = 0; i < numFrames; ++i)
    {
        contact *= contactCoef;
        tonebarEnv *= tonebarRise;
        const double attack = 1.0 - contact;

        const double x = attack * (fundamentalSlow.next() + fundamentalFast.next() + ding.next() + third.next())
                         + (1.0 - tonebarEnv) * tonebar.next();

        const double flux = std::tanh(k * (x + offset));
        double y = (flux - previousFlux) * derivativeScale;
        previousFlux = flux;

        thumpState += (rng.next() - thumpState) * thumpLp;
        y += thumpLevel * thumpEnv * thumpState;
        thumpEnv *= thumpDecay;

        toneState += (y - toneState) * toneLp;
        double sample = toneState;
        if (i >= numFrames - fadeFrames)
            sample *= static_cast<double>(numFrames - i) / fadeFrames;
        out[i] = static_cast<float>(sample);
    }
}

void renderModeledBell(float* out, int numFrames, double sampleRate, int note, float velocity, uint32_t seed)
{
    if (out == nullptr || numFrames <= 0)
        return;

    Rng rng(seed * 747796405u + static_cast<uint32_t>(note));
    const double v = std::clamp(static_cast<double>(velocity), 0.02, 1.0);
    const double f0 = dsp::noteToHz(note);
    const double pitchDecay = std::pow(440.0 / f0, 0.35);

    // A celeste bar over its resonator: a nearly pure fundamental, a short bright strike.
    Partial fundamental(f0, v, 1.1 * pitchDecay, sampleRate);
    Partial second(f0 * 4.0, 0.22 * v * bandWeight(f0 * 4.0, 9000.0), 0.09 * pitchDecay, sampleRate);
    Partial third(f0 * 9.2, 0.08 * v * bandWeight(f0 * 9.2, 9000.0), 0.03, sampleRate);
    const double contactCoef = std::exp(-1.0 / (0.0008 * sampleRate));
    double contact = 1.0, clickEnv = 0.02 * v;
    const double clickDecay = std::exp(-1.0 / (0.004 * sampleRate));
    const int fadeFrames = std::min(numFrames / 4, static_cast<int>(0.05 * sampleRate));

    for (int i = 0; i < numFrames; ++i)
    {
        contact *= contactCoef;
        double y = (1.0 - contact) * (fundamental.next() + second.next() + third.next());
        y += clickEnv * rng.next();
        clickEnv *= clickDecay;
        if (i >= numFrames - fadeFrames)
            y *= static_cast<double>(numFrames - i) / fadeFrames;
        out[i] = static_cast<float>(y);
    }
}
} // namespace keys
