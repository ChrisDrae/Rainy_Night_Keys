#include "engine/DemoLibrary.h"
#include "engine/KeysEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace keys;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

/// A small demo bank (a few roots, short renders) so the tests stay quick.
const SampleBank& testTines()
{
    static const auto bank = []
    {
        DemoTineOptions options;
        options.lowestRoot = 48;
        options.highestRoot = 72;
        options.rootStep = 6;
        options.layerTopVelocities = { 64, 127 };
        options.maxSeconds = 1.5;
        options.threads = 2;
        return buildDemoTines(options);
    }();
    return *bank;
}

const SampleBank& testBell()
{
    static const auto bank = buildModeledBell(kRate);
    return *bank;
}

struct Harness
{
    KeysEngine engine;
    EngineParams params;
    Banks banks { &testTines(), &testBell(), nullptr };
    juce::AudioBuffer<float> buffer { 2, kBlock };

    Harness() { engine.prepare(kRate, kBlock); }

    /// Runs one block with the given MIDI; returns its peak level.
    float block(const juce::MidiBuffer& midi = {})
    {
        engine.process(buffer, midi, params, banks);
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
            {
                const float v = buffer.getSample(ch, i);
                REQUIRE(std::isfinite(v));
                peak = std::max(peak, std::abs(v));
            }
        return peak;
    }

    float run(int blocks)
    {
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
            peak = std::max(peak, block());
        return peak;
    }

    static juce::MidiBuffer one(const juce::MidiMessage& m)
    {
        juce::MidiBuffer midi;
        midi.addEvent(m, 0);
        return midi;
    }
};
} // namespace

TEST_CASE("Silence without MIDI", "[engine]")
{
    Harness h;
    CHECK(h.run(10) == 0.0f);
}

TEST_CASE("A note sounds, then stops after note-off", "[engine]")
{
    Harness h;
    h.block(Harness::one(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100))));
    CHECK(h.run(20) > 0.01f);
    CHECK(h.engine.countActiveVoices() == 1);

    h.block(Harness::one(juce::MidiMessage::noteOff(1, 60)));
    h.run(static_cast<int>(1.6 * kRate / kBlock)); // dampers: tine ~0.1 s, bell ~0.2 s time constants
    CHECK(h.run(4) < 1.0e-4f);
    CHECK(h.engine.countActiveVoices() == 0);
}

TEST_CASE("The sustain pedal holds notes until it is lifted", "[engine]")
{
    Harness h;
    h.block(Harness::one(juce::MidiMessage::controllerEvent(1, 64, 127)));
    h.block(Harness::one(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(90))));
    h.block(Harness::one(juce::MidiMessage::noteOff(1, 64)));
    h.run(static_cast<int>(0.4 * kRate / kBlock));
    CHECK(h.run(2) > 0.005f); // still ringing: the pedal holds it
    CHECK(h.engine.status().sustainPedal.load());

    h.block(Harness::one(juce::MidiMessage::controllerEvent(1, 64, 0)));
    h.run(static_cast<int>(1.6 * kRate / kBlock));
    CHECK(h.engine.countActiveVoices() == 0);
}

TEST_CASE("Harder notes are louder", "[engine]")
{
    auto peakFor = [](int velocity)
    {
        Harness h;
        h.block(Harness::one(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(velocity))));
        return h.run(20);
    };
    CHECK(peakFor(120) > 1.5f * peakFor(30));
}

TEST_CASE("Blend at 0 plays only tines, at 1 only the bell", "[engine]")
{
    auto peakWith = [](float blend, Banks banks)
    {
        Harness h;
        h.params.blend = blend;
        h.banks = banks;
        h.block(Harness::one(juce::MidiMessage::noteOn(1, 72, static_cast<juce::uint8>(100))));
        return h.run(30);
    };
    const Banks onlyTines { &testTines(), nullptr, nullptr };
    const Banks onlyBell { nullptr, &testBell(), nullptr };
    CHECK(peakWith(0.0f, onlyBell) < 1.0e-3f); // bell muted at blend 0
    CHECK(peakWith(1.0f, onlyTines) < 1.0e-3f); // tines muted at blend 1
    CHECK(peakWith(0.5f, onlyTines) > 0.01f);
    CHECK(peakWith(0.5f, onlyBell) > 0.01f);
}

TEST_CASE("Polyphony is capped and many notes stay finite", "[engine]")
{
    Harness h;
    juce::MidiBuffer midi;
    for (int i = 0; i < 100; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 30 + (i % 70), static_cast<juce::uint8>(100)), i);
    h.block(midi);
    CHECK(h.engine.countActiveVoices() <= KeysEngine::kVoiceSlots);
    CHECK(h.run(10) <= 1.0f); // the output soft clip keeps it in range
}

TEST_CASE("Re-striking a held note doesn't pile up voices", "[engine]")
{
    Harness h;
    for (int i = 0; i < 10; ++i)
    {
        h.block(Harness::one(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100))));
        h.run(2);
    }
    h.run(static_cast<int>(0.3 * kRate / kBlock));
    CHECK(h.engine.countActiveVoices() <= 2);
}

TEST_CASE("No sound until a bank exists", "[engine]")
{
    Harness h;
    h.banks = {};
    h.block(Harness::one(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100))));
    CHECK(h.run(5) == 0.0f);
}

TEST_CASE("MIDI events reach the status for the UI", "[engine]")
{
    Harness h;
    h.block(Harness::one(juce::MidiMessage::noteOn(3, 61, static_cast<juce::uint8>(77))));
    auto& status = h.engine.status();
    CHECK(status.lastNote.load() == 61);
    CHECK(status.lastVelocity.load() == 77);
    CHECK(status.midiEventCount.load() == 1);
    const auto e = EngineStatus::unpack(status.history[0].load());
    CHECK(e.type == MidiEventInfo::Type::noteOn);
    CHECK(e.channel == 3);
    CHECK(e.number == 61);
}
