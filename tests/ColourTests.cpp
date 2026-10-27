#include "engine/ColourChain.h"
#include "engine/DemoLibrary.h"
#include "engine/KeysEngine.h"
#include "engine/Reverb.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace keys;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

/// A test signal: a decaying chord-like mix of sines (left and right a little different).
std::vector<float> testSignal(int length, int channel)
{
    std::vector<float> s(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i)
    {
        const double t = i / kRate;
        const double env = std::exp(-t * 2.0);
        s[static_cast<size_t>(i)] = static_cast<float>(
            env * (0.12 * std::sin(2 * dsp::kPi * 220.0 * t) + 0.08 * std::sin(2 * dsp::kPi * 330.0 * t + channel)
                   + 0.05 * std::sin(2 * dsp::kPi * 1250.0 * t)));
    }
    return s;
}

struct ChainRun
{
    std::vector<float> left, right;
};

/// Runs the colour chain over the test signal (plus `tail` samples of silence) in blocks.
ChainRun runChain(const EngineParams& params, int length = 24000, int tail = 0)
{
    ColourChain chain;
    chain.prepare(kRate, kBlock);
    ChainRun run { testSignal(length, 0), testSignal(length, 1) };
    run.left.resize(static_cast<size_t>(length + tail), 0.0f);
    run.right.resize(static_cast<size_t>(length + tail), 0.0f);
    for (int start = 0; start < length + tail; start += kBlock)
    {
        const int n = std::min(kBlock, length + tail - start);
        chain.process(run.left.data() + start, run.right.data() + start, n, params);
    }
    return run;
}

double rms(const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min(to, x.size());
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, to - from)));
}

double rmsDifference(const ChainRun& a, const std::vector<float>& refLeft)
{
    double sum = 0.0;
    for (size_t i = 0; i < refLeft.size(); ++i)
        sum += std::pow(static_cast<double>(a.left[i]) - refLeft[i], 2.0);
    return std::sqrt(sum / static_cast<double>(refLeft.size()));
}

bool allFinite(const ChainRun& r, float limit = 4.0f)
{
    for (size_t i = 0; i < r.left.size(); ++i)
        if (!std::isfinite(r.left[i]) || !std::isfinite(r.right[i]) || std::abs(r.left[i]) > limit
            || std::abs(r.right[i]) > limit)
            return false;
    return true;
}

// --- The full engine, with a small bank, for the per-voice features --------------------------------
const SampleBank& smallTines()
{
    static const auto bank = []
    {
        DemoTineOptions options;
        options.lowestRoot = 30;
        options.highestRoot = 99;
        options.rootStep = 12;
        options.layerTopVelocities = { 64, 127 };
        options.maxSeconds = 1.0;
        options.threads = 2;
        return buildDemoTines(options);
    }();
    return *bank;
}

struct EngineRun
{
    std::vector<float> left, right;
};

EngineRun playNote(EngineParams params, int note, int velocity, int blocks = 40)
{
    KeysEngine engine;
    engine.prepare(kRate, kBlock);
    const Banks banks { &smallTines(), nullptr, nullptr };
    juce::AudioBuffer<float> buffer(2, kBlock);
    EngineRun run;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(velocity)), 0);
        engine.process(buffer, midi, params, banks);
        for (int i = 0; i < kBlock; ++i)
        {
            run.left.push_back(buffer.getSample(0, i));
            run.right.push_back(buffer.getSample(1, i));
        }
    }
    return run;
}

double difference(const EngineRun& a, const EngineRun& b, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min({ to, a.left.size(), b.left.size() });
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += std::pow(static_cast<double>(a.left[i]) - b.left[i], 2.0) + std::pow(static_cast<double>(a.right[i]) - b.right[i], 2.0);
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, to - from)));
}

/// Energy above ~4 kHz (a first difference stands in for a high-pass).
double brightness(const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from + 1; i < std::min(to, x.size()); ++i)
        sum += std::pow(static_cast<double>(x[i]) - x[i - 1], 2.0);
    return sum;
}
} // namespace

// =================================================================================================
//  The colour chain
// =================================================================================================
TEST_CASE("With every knob at 0 the colour chain is an exact bypass", "[colour]")
{
    const EngineParams bare;
    const auto dry = testSignal(24000, 0);
    const auto run = runChain(bare);
    for (size_t i = 0; i < dry.size(); ++i)
        REQUIRE(run.left[i] == dry[i]);
}

TEST_CASE("Each colour stage changes the sound and stays finite", "[colour]")
{
    const auto dry = testSignal(24000, 0);
    struct Case
    {
        const char* name;
        void (*set)(EngineParams&);
    };
    const Case cases[] = {
        { "warmth", [](EngineParams& p) { p.warmth = 0.7f; } },
        { "warmth transistor", [](EngineParams& p) { p.warmth = 0.7f; p.warmthCharacter = WarmthCharacter::transistor; } },
        { "body", [](EngineParams& p) { p.body = 0.7f; } },
        { "patina", [](EngineParams& p) { p.patina = 0.7f; } },
        { "motion vibrato", [](EngineParams& p) { p.motion = 0.7f; p.motionMode = MotionMode::vibrato; } },
        { "motion chorus", [](EngineParams& p) { p.motion = 0.7f; p.motionMode = MotionMode::chorus; } },
        { "space plate", [](EngineParams& p) { p.space = 0.7f; p.spaceType = SpaceType::plate; } },
        { "space spring", [](EngineParams& p) { p.space = 0.7f; p.spaceType = SpaceType::spring; } },
        { "space room", [](EngineParams& p) { p.space = 0.7f; p.spaceType = SpaceType::room; } },
        { "amp", [](EngineParams& p) { p.signalAmp = 0.7f; } },
        { "room mic", [](EngineParams& p) { p.signalRoom = 0.7f; } },
        { "dust", [](EngineParams& p) { p.dust = 0.7f; } },
    };
    for (const auto& c : cases)
    {
        INFO(c.name);
        EngineParams params;
        c.set(params);
        const auto run = runChain(params);
        CHECK(allFinite(run));
        CHECK(rmsDifference(run, dry) > 0.002);
    }
}

TEST_CASE("Every knob at full stays finite and in range", "[colour]")
{
    EngineParams p;
    p.warmth = p.body = p.patina = p.motion = p.space = p.dust = 1.0f;
    p.signalAmp = p.signalRoom = 1.0f;
    p.warmthDrive = p.warmthTilt = p.bodyResonance = 1.0f;
    p.motionMode = MotionMode::both;
    p.motionRateHz = 8.0f;
    p.spaceDecaySeconds = 4.0f;
    p.spaceTone = 1.0f;
    const auto run = runChain(p, 48000, 48000 * 3);
    CHECK(allFinite(run, 6.0f));
}

TEST_CASE("Colour stages keep the level roughly even", "[colour]")
{
    // A typical setting shouldn't jump the level: within ±6 dB of the dry signal.
    EngineParams p;
    p.warmth = 0.62f;
    p.body = 0.58f;
    p.patina = 0.35f;
    p.motion = 0.32f;
    p.space = 0.42f;
    p.signalDi = 0.85f;
    p.signalAmp = 0.35f;
    p.signalRoom = 0.25f;
    const auto dry = testSignal(24000, 0);
    const auto run = runChain(p);
    const double ratioDb = 20.0 * std::log10(rms(run.left) / rms(dry));
    INFO("level change " << ratioDb << " dB");
    CHECK(std::abs(ratioDb) < 6.0);
}

TEST_CASE("Turning a knob back to 0 returns to the bypass", "[colour]")
{
    ColourChain chain;
    chain.prepare(kRate, kBlock);
    EngineParams p;
    p.warmth = 0.8f;
    p.space = 0.6f;
    std::vector<float> l(kBlock), r(kBlock);
    for (int b = 0; b < 20; ++b)
    {
        std::fill(l.begin(), l.end(), 0.1f);
        std::fill(r.begin(), r.end(), 0.1f);
        chain.process(l.data(), r.data(), kBlock, p);
    }
    p.warmth = 0.0f;
    p.space = 0.0f;
    for (int b = 0; b < 400; ++b) // two seconds: the glides settle
    {
        std::fill(l.begin(), l.end(), 0.1f);
        std::fill(r.begin(), r.end(), 0.1f);
        chain.process(l.data(), r.data(), kBlock, p);
    }
    for (float v : l)
        CHECK(v == 0.1f);
}

TEST_CASE("Dust hisses even in silence, and 0 dust is silent", "[colour]")
{
    ColourChain chain;
    chain.prepare(kRate, kBlock);
    EngineParams p;
    std::vector<float> l(kBlock, 0.0f), r(kBlock, 0.0f);
    chain.process(l.data(), r.data(), kBlock, p);
    CHECK(rms(l) == 0.0);

    p.dust = 0.6f;
    double total = 0.0;
    for (int b = 0; b < 200; ++b)
    {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
        chain.process(l.data(), r.data(), kBlock, p);
        total += rms(l);
    }
    CHECK(total / 200.0 > 1.0e-5);
    CHECK(total / 200.0 < 0.05);
}

TEST_CASE("The anti-aliased saturator follows the plain curve on slow signals", "[colour]")
{
    dsp::Saturator plain;
    dsp::SmoothSaturator smooth;
    plain.set(dsp::Saturator::Curve::tube, 0.8f);
    smooth.set(dsp::Saturator::Curve::tube, 0.8f);
    float worst = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        const auto x = static_cast<float>(0.4 * std::sin(2.0 * dsp::kPi * 100.0 * i / kRate));
        const float a = plain.process(x), b = smooth.process(x, 0);
        if (i > 10)
            worst = std::max(worst, std::abs(a - b));
    }
    CHECK(worst < 0.01f); // only half a sample of delay apart

    smooth.set(dsp::Saturator::Curve::tube, 0.0f); // amount 0: the input, untouched
    for (float x : { -0.5f, -0.01f, 0.0f, 0.2f, 0.9f })
        CHECK(smooth.process(x, 1) == x);
}

// =================================================================================================
//  The reverb
// =================================================================================================
TEST_CASE("The reverb tail decays at its decay time, for every type", "[reverb]")
{
    for (auto type : { SpaceType::plate, SpaceType::room, SpaceType::spring })
    {
        INFO("type " << static_cast<int>(type));
        Reverb reverb;
        reverb.prepare(kRate);
        reverb.setType(type);
        reverb.setDecay(1.0f);
        reverb.setTone(0.6f);
        reverb.setPreDelay(0.0f);
        reverb.reset();

        const int length = static_cast<int>(kRate * 2.0);
        std::vector<float> in(static_cast<size_t>(length), 0.0f), outL(static_cast<size_t>(length)), outR(static_cast<size_t>(length));
        for (int i = 0; i < 480; ++i) // a 10 ms burst
            in[static_cast<size_t>(i)] = (i % 7 < 3) ? 0.5f : -0.5f;
        reverb.process(in.data(), in.data(), outL.data(), outR.data(), length);

        bool finite = true;
        for (float v : outL)
            finite = finite && std::isfinite(v);
        REQUIRE(finite);

        const auto window = [&](double fromSeconds)
        {
            const auto from = static_cast<size_t>(fromSeconds * kRate);
            return rms(outL, from, from + static_cast<size_t>(0.1 * kRate));
        };
        const double early = window(0.05);
        const double atDecay = window(1.05); // 1 s later: should be ~60 dB down
        const double dropDb = 20.0 * std::log10(early / std::max(1.0e-12, atDecay));
        INFO("drop over the decay time: " << dropDb << " dB");
        CHECK(early > 1.0e-4);
        CHECK(dropDb > 35.0);
        CHECK(dropDb < 90.0);
    }
}

TEST_CASE("A long, bright reverb stays stable", "[reverb]")
{
    Reverb reverb;
    reverb.prepare(kRate);
    reverb.setType(SpaceType::plate);
    reverb.setDecay(8.0f);
    reverb.setTone(1.0f);
    std::vector<float> in(48000, 0.0f), l(48000), r(48000);
    in[0] = 1.0f;
    float peak = 0.0f;
    for (int b = 0; b < 20; ++b) // 20 s
    {
        reverb.process(in.data(), in.data(), l.data(), r.data(), 48000);
        in[0] = 0.0f;
        for (float v : l)
            peak = std::max(peak, std::abs(v));
    }
    CHECK(std::isfinite(peak));
    CHECK(peak < 2.0f);
}

// =================================================================================================
//  Per-voice colour: bark, ding, double, width, wow and flutter, mechanics
// =================================================================================================
TEST_CASE("Body's width puts low keys left and high keys right", "[voice]")
{
    EngineParams p;
    p.body = 1.0f;
    p.bodyWidth = 1.0f;
    p.bodyDouble = 0.0f;
    const auto low = playNote(p, 36, 100, 20);
    const auto high = playNote(p, 92, 100, 20);
    CHECK(rms(low.left) > 1.5 * rms(low.right));
    CHECK(rms(high.right) > 1.5 * rms(high.left));
}

TEST_CASE("Bark bends harder notes more", "[voice]")
{
    auto barkShare = [](int velocity)
    {
        EngineParams clean, barking;
        barking.bark = 1.0f;
        const auto a = playNote(clean, 52, velocity, 10);
        const auto b = playNote(barking, 52, velocity, 10);
        return difference(a, b) / rms(a.left);
    };
    const double soft = barkShare(40), hard = barkShare(127);
    INFO("bark share soft " << soft << ", hard " << hard);
    CHECK(hard > 0.05);
    CHECK(hard > 1.5 * soft);
}

TEST_CASE("Tine ding brightens or softens the attack", "[voice]")
{
    auto attack = [](float ding)
    {
        EngineParams p;
        p.tineDing = ding;
        const auto run = playNote(p, 64, 110, 4);
        return brightness(run.left, 0, static_cast<size_t>(0.02 * kRate));
    };
    const double soft = attack(0.0f), neutral = attack(0.5f), bright = attack(1.0f);
    CHECK(soft < neutral);
    CHECK(bright > neutral);
}

TEST_CASE("Body's double adds a second, detuned tine", "[voice]")
{
    EngineParams single, doubled; // the same Body, so only Double differs
    single.body = doubled.body = 1.0f;
    single.bodyWidth = doubled.bodyWidth = 0.0f;
    single.bodyDouble = 0.0f;
    doubled.bodyDouble = 1.0f;
    const auto a = playNote(single, 60, 100, 40);
    const auto b = playNote(doubled, 60, 100, 40);
    CHECK(difference(a, b, 4800) > 0.002);
}

TEST_CASE("Patina's wow and flutter bend the pitch", "[voice]")
{
    EngineParams steady, wobbly;
    steady.patina = wobbly.patina = 0.8f;
    steady.wow = steady.flutter = false;
    const auto a = playNote(steady, 60, 100, 120);
    const auto b = playNote(wobbly, 60, 100, 120);
    CHECK(difference(a, b, 2400) > 0.001);
}

TEST_CASE("Mechanics adds key noise on the attack", "[voice]")
{
    EngineParams quiet, noisy;
    noisy.mechanics = 1.0f;
    const auto a = playNote(quiet, 60, 120, 4);
    const auto b = playNote(noisy, 60, 120, 4);
    CHECK(difference(a, b, 0, static_cast<size_t>(0.05 * kRate)) > 0.001);
}

TEST_CASE("Bell from key mutes the bell below its range", "[voice]")
{
    static const auto bell = buildModeledBell(kRate);
    auto bellLevel = [](int lowestKey)
    {
        KeysEngine engine;
        engine.prepare(kRate, kBlock);
        EngineParams p;
        p.blend = 1.0f; // bell only
        p.bellLowestKey = lowestKey;
        const Banks banks { nullptr, bell.get(), nullptr };
        juce::AudioBuffer<float> buffer(2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 48, static_cast<juce::uint8>(100)), 0);
        float peak = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            engine.process(buffer, b == 0 ? midi : juce::MidiBuffer(), p, banks);
            peak = std::max(peak, buffer.getMagnitude(0, kBlock));
        }
        return peak;
    };
    CHECK(bellLevel(36) > 0.01f); // C3 is above C2: the bell plays
    CHECK(bellLevel(60) < 1.0e-4f); // the bell starts at C4: C3 is silent
}

TEST_CASE("Notes and the pedal reach the light's queue", "[voice]")
{
    KeysEngine engine;
    engine.prepare(kRate, kBlock);
    const Banks banks { &smallTines(), nullptr, nullptr };
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(90)), 10);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 20);
    engine.process(buffer, midi, EngineParams {}, banks);

    std::array<NoteEvent, 8> events {};
    const int n = engine.status().notes.pop(events.data(), 8);
    REQUIRE(n == 3);
    CHECK(events[0].type == NoteEvent::Type::pedalDown);
    CHECK(events[1].type == NoteEvent::Type::noteOn);
    CHECK(events[1].note == 64);
    CHECK(events[2].type == NoteEvent::Type::noteOff);
    CHECK(engine.status().notes.pop(events.data(), 8) == 0);
}

TEST_CASE("A full Dusk Ballad chord stays finite and below full scale", "[voice]")
{
    EngineParams p;
    p.blend = 0.38f;
    p.warmth = 0.62f;
    p.bark = 0.3f;
    p.body = 0.58f;
    p.patina = 0.35f;
    p.motion = 0.32f;
    p.space = 0.42f;
    p.dust = 0.18f;
    p.mechanics = 0.35f;
    p.signalDi = 0.85f;
    p.signalAmp = 0.35f;
    p.signalRoom = 0.25f;

    KeysEngine engine;
    engine.prepare(kRate, kBlock);
    static const auto bell = buildModeledBell(kRate);
    const Banks banks { &smallTines(), bell.get(), nullptr };
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer chord;
    for (int note : { 41, 57, 64, 67, 72 })
        chord.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(120)), 0);
    float peak = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        engine.process(buffer, b == 0 ? chord : juce::MidiBuffer(), p, banks);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
            {
                REQUIRE(std::isfinite(buffer.getSample(ch, i)));
                peak = std::max(peak, std::abs(buffer.getSample(ch, i)));
            }
    }
    CHECK(peak > 0.05f);
    CHECK(peak <= 1.0f);
}

TEST_CASE("Benchmark: ten seconds of a ten-note Dusk Ballad chord", "[.bench]")
{
    EngineParams p;
    p.blend = 0.38f;
    p.warmth = 0.62f;
    p.bark = 0.3f;
    p.body = 0.58f;
    p.patina = 0.35f;
    p.motion = 0.32f;
    p.space = 0.42f;
    p.dust = 0.18f;
    p.mechanics = 0.35f;
    p.signalDi = 0.85f;
    p.signalAmp = 0.35f;
    p.signalRoom = 0.25f;

    KeysEngine engine;
    engine.prepare(kRate, kBlock);
    static const auto bell = buildModeledBell(kRate);
    const Banks banks { &smallTines(), bell.get(), nullptr };
    juce::AudioBuffer<float> buffer(2, kBlock);
    const auto blocks = static_cast<int>(10.0 * kRate / kBlock);
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b % 40 == 0) // a new ten-note chord every ~0.2 s
            for (int k = 0; k < 10; ++k)
                midi.addEvent(juce::MidiMessage::noteOn(1, 40 + k * 4 + (b / 40) % 3, static_cast<juce::uint8>(100)), k);
        engine.process(buffer, midi, p, banks);
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - start;
    WARN("10 s of audio in " << ms << " ms: " << (ms / 100.0) << " % of one core, "
                             << engine.countActiveVoices() << " voices at the end");
}

TEST_CASE("Analyse: saturator aliasing on a high tone", "[.analyse]")
{
    // A 3.1 kHz sine at a loud note's level through each curve; energy away from the harmonics of
    // 3.1 kHz is aliasing (harmonics above 24 kHz folding back down).
    constexpr int size = 1 << 15;
    const double f0 = 3100.0;
    for (auto [name, curve, amount] : { std::tuple { "bark 0.5", dsp::Saturator::Curve::bark, 0.5f },
                                        { "bark 1.0", dsp::Saturator::Curve::bark, 1.0f },
                                        { "tube 0.6", dsp::Saturator::Curve::tube, 0.6f },
                                        { "transistor 0.6", dsp::Saturator::Curve::transistor, 0.6f },
                                        { "amp (tube 0.9)", dsp::Saturator::Curve::tube, 0.9f } })
    {
        for (bool smooth : { false, true })
        {
        dsp::Saturator sat;
        sat.set(curve, amount);
        dsp::SmoothSaturator smoothSat;
        smoothSat.set(curve, amount);
        std::vector<double> y(size);
        for (int i = 0; i < size; ++i)
        {
            const auto x = static_cast<float>(0.3 * std::sin(2.0 * dsp::kPi * f0 * i / kRate));
            y[static_cast<size_t>(i)] = smooth ? smoothSat.process(x, 0) : sat.process(x);
        }
        double harmonic = 0.0, other = 0.0;
        for (int bin = 1; bin < size / 2; bin += 1)
        {
            const double f = bin * kRate / size;
            // Goertzel-free: direct DFT is too slow at every bin, so sample every 4th bin.
            if (bin % 4 != 0)
                continue;
            double re = 0.0, im = 0.0;
            for (int i = 0; i < size; i += 1)
            {
                const double w = 0.5 - 0.5 * std::cos(2.0 * dsp::kPi * i / size);
                re += w * y[static_cast<size_t>(i)] * std::cos(2.0 * dsp::kPi * bin * i / size);
                im -= w * y[static_cast<size_t>(i)] * std::sin(2.0 * dsp::kPi * bin * i / size);
            }
            const double p = re * re + im * im;
            const double k = f / f0;
            (std::abs(k - std::round(k)) * f0 < 40.0 ? harmonic : other) += p;
        }
        WARN(name << (smooth ? " (ADAA)" : "") << ": aliasing " << 10.0 * std::log10(other / harmonic)
                  << " dB below the harmonics");
        }
    }
}
