#include "Reverb.h"

#include <cmath>

namespace keys
{
namespace
{
// Delay times in milliseconds. The FDN lines are spread out and share no common factors, so their
// echoes never line up into a pitched ring. The diffusers are Dattorro's plate input diffusers.
constexpr float kLineMs[Reverb::kLines] = { 31.71f, 37.11f, 40.93f, 44.63f, 50.27f, 56.79f, 61.13f, 68.71f };
constexpr float kDiffuserMs[4] = { 4.771f, 3.595f, 12.73f, 9.307f };
constexpr float kEarlyMs[8] = { 4.3f, 7.9f, 11.3f, 15.7f, 19.1f, 23.9f, 29.3f, 34.7f };
constexpr float kEarlyGain[8] = { 0.84f, -0.72f, 0.66f, -0.58f, 0.5f, -0.43f, 0.37f, -0.3f };
constexpr float kSpringMs[2] = { 33.3f, 38.9f };
constexpr float kSpringCoefficient = -0.55f; // negative: low frequencies are delayed most

int toSamples(float milliseconds, double sampleRate)
{
    return std::max(1, static_cast<int>(std::lround(milliseconds * 0.001 * sampleRate)));
}

float lineScale(SpaceType type)
{
    switch (type)
    {
        case SpaceType::room: return 0.45f;
        case SpaceType::spring: return 0.7f;
        case SpaceType::plate: break;
    }
    return 1.0f;
}
} // namespace

void Reverb::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    preDelay.allocate(toSamples(kMaxPreDelayMs + 40.0f, sampleRate)); // + room for the early taps
    preDelaySamples.prepare(sampleRate, 1, 0.2);
    for (size_t i = 0; i < diffusers.size(); ++i)
        diffusers[i].delay.allocate(toSamples(kDiffuserMs[i], sampleRate) + 2);
    for (size_t i = 0; i < lines.size(); ++i)
        lines[i].allocate(toSamples(kLineMs[i] + 2.0f, sampleRate) + 4); // + modulation headroom
    for (auto& spring : springs)
        spring.delay.allocate(toSamples(kSpringMs[1] + 2.0f, sampleRate));

    // Stretch the spring's allpasses so their chirp sits below ~4.5 kHz, like a real tank.
    springK = std::clamp(static_cast<int>(std::lround(sampleRate / 9000.0)), 1, StretchedAllpass::kMaxK - 1);
    modA.setRate(0.61, sampleRate);
    modB.setRate(0.83, sampleRate);

    setType(type);
    reset();
}

void Reverb::reset() noexcept
{
    preDelay.clear();
    preDelaySamples.snap(preDelaySamples.target);
    for (auto& d : diffusers)
        d.delay.clear();
    for (auto& line : lines)
        line.clear();
    for (auto& d : damping)
        d.reset();
    for (auto& spring : springs)
    {
        for (auto& stage : spring.chain)
            stage.clear();
        spring.delay.clear();
        spring.state = 0.0f;
        spring.lowPass1.reset();
        spring.lowPass2.reset();
        spring.highPassTrack.reset();
    }
    for (auto& t : outputTone)
        t.reset();
}

void Reverb::setType(SpaceType newType) noexcept
{
    type = newType;
    const float scale = lineScale(type);
    for (size_t i = 0; i < lines.size(); ++i)
        lineLength[i] = toSamples(kLineMs[i] * scale, sampleRate);

    const bool room = type == SpaceType::room, plate = type == SpaceType::plate;
    const float diffuserGain[4] = { plate ? 0.75f : 0.6f, plate ? 0.75f : 0.6f, plate ? 0.625f : 0.5f,
                                    plate ? 0.625f : 0.5f };
    for (size_t i = 0; i < diffusers.size(); ++i)
    {
        diffusers[i].length = toSamples(kDiffuserMs[i] * (room ? 0.6f : 1.0f), sampleRate);
        diffusers[i].gain = diffuserGain[i];
    }
    for (size_t i = 0; i < earlyTaps.size(); ++i)
        earlyTaps[i] = toSamples(kEarlyMs[i] * (room ? 1.0f : 0.6f), sampleRate);
    for (size_t i = 0; i < springs.size(); ++i)
        springs[i].length = toSamples(kSpringMs[i], sampleRate);

    modDepth = static_cast<float>((plate ? 0.3 : room ? 0.12 : 0.2) * 0.001 * sampleRate);
    earlyLevel = room ? 0.55f : plate ? 0.12f : 0.0f;
    tailLevel = room ? 0.75f : plate ? 1.0f : 0.4f;
    springLevel = type == SpaceType::spring ? 0.8f : 0.0f;
    updateGains();
}

void Reverb::setDecay(float seconds) noexcept
{
    seconds = std::clamp(seconds, 0.2f, 8.0f);
    if (seconds != decaySeconds)
    {
        decaySeconds = seconds;
        updateGains();
    }
}

void Reverb::setTone(float newTone) noexcept
{
    newTone = std::clamp(newTone, 0.0f, 1.0f);
    if (newTone != tone)
    {
        tone = newTone;
        updateGains();
    }
}

void Reverb::setPreDelay(float milliseconds) noexcept
{
    preDelaySamples.target = static_cast<float>(std::clamp(milliseconds, 0.0f, kMaxPreDelayMs) * 0.001 * sampleRate);
}

void Reverb::updateGains() noexcept
{
    // Each pass through a line of length L must lose 60 dB per T60: g = 10^(−3 · L / (T60 · fs)).
    const double t60 = decaySeconds * sampleRate;
    for (size_t i = 0; i < lines.size(); ++i)
        lineGain[i] = static_cast<float>(std::pow(10.0, -3.0 * lineLength[i] / t60));

    const double dampHz = type == SpaceType::room     ? 2500.0 + tone * 7000.0
                          : type == SpaceType::spring ? 2200.0 + tone * 3000.0
                                                      : 3500.0 + tone * 10000.0;
    for (auto& d : damping)
        d.setCutoff(dampHz, sampleRate);

    // The spring loop is longer than its delay: the allpass chain adds its own (low-frequency) delay.
    const double chainDelay = SpringLoop::kStages * springK * (1.0 - kSpringCoefficient) / (1.0 + kSpringCoefficient) * 0.5;
    for (auto& spring : springs)
    {
        spring.feedback = static_cast<float>(std::pow(10.0, -3.0 * (spring.length + chainDelay) / t60));
        spring.lowPass1.setCutoff(4200.0 + tone * 1500.0, sampleRate);
        spring.lowPass2.setCutoff(5200.0, sampleRate);
        spring.highPassTrack.setCutoff(110.0, sampleRate);
    }

    for (auto& t : outputTone)
        t.setCutoff(1800.0 * std::pow(6.0, tone), sampleRate);
}

void Reverb::process(const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    constexpr float householder = 2.0f / kLines;
    constexpr float inputGain = 0.35f; // ≈ 1/√8: spreads the input's energy over the eight lines
    constexpr float inputSign[kLines] = { 1, -1, 1, -1, -1, 1, -1, 1 };

    for (int i = 0; i < numSamples; ++i)
    {
        preDelaySamples.advance();
        preDelay.push(0.5f * (inL[i] + inR[i]));
        const float delay = std::max(1.0f, preDelaySamples.value);
        const float x = preDelay.readFractional(delay);

        float left = 0.0f, right = 0.0f;

        // Early reflections, straight from the pre-delayed input.
        if (earlyLevel > 0.0f)
        {
            const int base = static_cast<int>(delay);
            for (size_t t = 0; t < earlyTaps.size(); ++t)
            {
                const float v = preDelay.read(base + earlyTaps[t]) * kEarlyGain[t];
                (t % 2 == 0 ? left : right) += v;
                (t % 2 == 0 ? right : left) += 0.3f * v;
            }
            left *= earlyLevel;
            right *= earlyLevel;
        }

        // The tail: diffuse the input, then circulate it through the eight lines.
        float diffused = x;
        for (auto& d : diffusers)
            diffused = d.process(diffused);

        std::array<float, kLines> y;
        const float wobbleA = modDepth * (1.0f + modA.next());
        const float wobbleB = modDepth * (1.0f + modB.next());
        float sum = 0.0f;
        for (size_t l = 0; l < lines.size(); ++l)
        {
            const float wobble = l == 0 ? wobbleA : l == 4 ? wobbleB : 0.0f;
            const float out = wobble > 0.0f ? lines[l].readFractional(static_cast<float>(lineLength[l]) + wobble)
                                            : lines[l].read(lineLength[l]);
            y[l] = damping[l].process(out) * lineGain[l];
            sum += y[l];
        }
        const float feedback = sum * householder;
        for (size_t l = 0; l < lines.size(); ++l)
            lines[l].push(y[l] - feedback + diffused * inputSign[l] * inputGain);

        left += tailLevel * 0.5f * (y[0] - y[2] + y[4] - y[6]);
        right += tailLevel * 0.5f * (y[1] - y[3] + y[5] - y[7]);

        // Spring: each pass through the allpass chain smears the echo into a falling chirp.
        if (springLevel > 0.0f)
        {
            for (size_t s = 0; s < springs.size(); ++s)
            {
                auto& spring = springs[s];
                float v = 0.6f * x + spring.feedback * spring.state;
                for (auto& stage : spring.chain)
                    v = stage.process(v, kSpringCoefficient, springK);
                v = spring.lowPass2.process(spring.lowPass1.process(v));
                v -= spring.highPassTrack.process(v);
                spring.delay.push(v);
                spring.state = spring.delay.read(spring.length);
            }
            left += springLevel * springs[0].state;
            right += springLevel * springs[1].state;
        }

        outL[i] = outputTone[0].process(left);
        outR[i] = outputTone[1].process(right);
    }
}
} // namespace keys
