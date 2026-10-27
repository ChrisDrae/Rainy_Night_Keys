#include "Parameters.h"
#include "Presets.h"

#include <catch2/catch_test_macros.hpp>

using namespace keys;

namespace
{
/// The smallest AudioProcessor that can hold the parameters.
struct TestProcessor final : juce::AudioProcessor
{
    TestProcessor() : state(*this, nullptr, "Keys", params::createLayout()) {}

    const juce::String getName() const override { return "Test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState state;
};

float plain(juce::AudioProcessorValueTreeState& state, const char* id)
{
    return state.getRawParameterValue(id)->load();
}
} // namespace

TEST_CASE("The defaults are the Dusk Ballad sound", "[params]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestProcessor p;
    const auto e = params::ParameterReader(p.state).read();
    CHECK(e.blend == 0.38f);
    CHECK(e.warmth == 0.62f);
    CHECK(e.bark == 0.30f);
    CHECK(e.body == 0.58f);
    CHECK(e.spaceType == SpaceType::plate);
    CHECK(e.bellLowestKey == 41); // F2
    CHECK(e.motionRateHz == 1.6f);
    CHECK(e.wow);
}

TEST_CASE("Every factory preset names real parameters with values in range", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestProcessor p;
    REQUIRE(factoryPresets().size() == 7);
    CHECK(factoryPresets().front().name == "Dusk Ballad");
    CHECK(factoryPresets().back().name == "Bare DI");
    for (const auto& preset : factoryPresets())
        for (const auto& [id, value] : preset.values)
        {
            INFO(preset.name << ": " << id);
            auto* parameter = p.state.getParameter(id);
            REQUIRE(parameter != nullptr);
            CHECK(!isPlayerSetting(id));
            const auto range = parameter->getNormalisableRange();
            CHECK(value >= range.start);
            CHECK(value <= range.end);
        }
}

TEST_CASE("Loading a preset sets its values and resets the rest, but not tune or touch", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestProcessor p;
    PresetManager presets(p.state);

    // Move a few things, including the player's own settings.
    p.state.getParameter(params::id::tune)->setValueNotifyingHost(0.8f);
    p.state.getParameter(params::id::dust)->setValueNotifyingHost(0.9f);

    presets.load(3, false); // Basement Soul
    CHECK(presets.getCurrentName() == "Basement Soul");
    CHECK(plain(p.state, params::id::bark) == 0.78f);
    CHECK(plain(p.state, params::id::spaceType) == 0.0f); // Spring
    CHECK(plain(p.state, params::id::motionRate) == 4.0f);
    CHECK(plain(p.state, params::id::dust) == 0.14f);
    CHECK(plain(p.state, params::id::bodyDouble) == 0.35f); // not in the preset: back to its default
    CHECK(std::abs(p.state.getParameter(params::id::tune)->getValue() - 0.8f) < 1.0e-4f); // the player's tuning stays

    presets.load(6, false); // Bare DI: every colour knob off
    const auto e = params::ParameterReader(p.state).read();
    CHECK(e.warmth == 0.0f);
    CHECK(e.space == 0.0f);
    CHECK(e.signalDi == 1.0f);
    CHECK(e.signalAmp == 0.0f);
}

TEST_CASE("Choices map to the engine's values", "[params]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TestProcessor p;
    p.state.getParameter(params::id::bellRange)->setValueNotifyingHost(
        p.state.getParameter(params::id::bellRange)->convertTo0to1(4.0f));
    p.state.getParameter(params::id::motionMode)->setValueNotifyingHost(1.0f);
    p.state.getParameter(params::id::patinaNarrow)->setValueNotifyingHost(0.0f);
    const auto e = params::ParameterReader(p.state).read();
    CHECK(e.bellLowestKey == 60); // C4
    CHECK(e.motionMode == MotionMode::both);
    CHECK_FALSE(e.narrow);
    CHECK(params::bellRangeChoices()[1] == "F2");
    CHECK(params::motionRateChoices()[2] == "1.6 Hz");
}
