#include "engine/DspMath.h"
#include "engine/TineModel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace keys;

namespace
{
constexpr double kRate = 48000.0;

std::vector<float> render(int note, float velocity, double seconds)
{
    std::vector<float> out(static_cast<size_t>(seconds * kRate));
    renderTine(out.data(), static_cast<int>(out.size()), kRate, note, velocity, 1);
    return out;
}

/// Energy at one frequency (Goertzel), over a window of the signal.
double magnitudeAt(const std::vector<float>& x, double hz, size_t start, size_t length)
{
    const double w = dsp::kTwoPi * hz / kRate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = start; i < start + length && i < x.size(); ++i)
    {
        const double s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2);
}

double rms(const std::vector<float>& x, size_t start, size_t length)
{
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = start; i < start + length && i < x.size(); ++i, ++n)
        sum += static_cast<double>(x[i]) * x[i];
    return n == 0 ? 0.0 : std::sqrt(sum / static_cast<double>(n));
}
} // namespace

TEST_CASE("A rendered tine is finite, audible and not clipping", "[tine]")
{
    for (int note : { 28, 45, 60, 69, 84, 100 })
    {
        const auto x = render(note, 0.8f, 1.0);
        float peak = 0.0f;
        for (float v : x)
        {
            REQUIRE(std::isfinite(v));
            peak = std::max(peak, std::abs(v));
        }
        INFO("note " << note);
        CHECK(peak > 0.05f);
        CHECK(peak < 2.0f);
    }
}

TEST_CASE("The fundamental is at the played pitch", "[tine]")
{
    const auto x = render(69, 0.6f, 1.0); // A4 = 440 Hz
    const size_t start = 4800, length = 19200; // skip the attack, analyse 0.4 s
    const double at = magnitudeAt(x, 440.0, start, length);
    CHECK(at > 4.0 * magnitudeAt(x, 415.3, start, length)); // a semitone below
    CHECK(at > 4.0 * magnitudeAt(x, 466.2, start, length)); // a semitone above
}

TEST_CASE("Playing harder adds bark: more second harmonic relative to the fundamental", "[tine]")
{
    auto ratio = [](float velocity)
    {
        const auto x = render(48, velocity, 0.6);
        const double f0 = dsp::noteToHz(48);
        const size_t start = 2400, length = 14400;
        return magnitudeAt(x, 2.0 * f0, start, length) / magnitudeAt(x, f0, start, length);
    };
    CHECK(ratio(1.0f) > 1.5 * ratio(0.25f));
}

TEST_CASE("Notes decay: bass rings longer than treble", "[tine]")
{
    auto remaining = [](int note)
    {
        const auto x = render(note, 0.7f, 2.0);
        return rms(x, 86400, 4800) / rms(x, 2400, 4800); // level at 1.8 s relative to 50 ms
    };
    CHECK(remaining(36) > remaining(84));
    CHECK(remaining(84) < 0.1);
}

TEST_CASE("Rendering is deterministic for a seed", "[tine]")
{
    CHECK(render(60, 0.5f, 0.2) == render(60, 0.5f, 0.2));
}
