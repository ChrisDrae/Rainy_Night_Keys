#include "Parameters.h"

namespace keys::params
{
juce::StringArray bellVoiceChoices()
{
    return { "Celeste", "Vibes", "Both" };
}

juce::StringArray bellOctaveChoices()
{
    return { "0", "+1" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float v, int) { return String(juce::roundToInt(v * 100.0f)); });

    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { id::blend, 1 }, "Blend",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.35f, percent));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::bellVoice, 1 }, "Bell Voice",
                                                      bellVoiceChoices(), 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { id::bellOctave, 1 }, "Bell Octave",
                                                      bellOctaveChoices(), 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { id::touch, 1 }, "Touch",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.5f, percent));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::tune, 1 }, "Tune", NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("ct").withStringFromValueFunction(
            [](float v, int) { return (v > 0.05f ? "+" : "") + String(v, 1); })));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::volume, 1 }, "Volume", NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 2.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction(
            [](float v, int) { return v <= -59.95f ? String("-inf") : String(v, 1); })));
    return layout;
}

ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState& state)
    : blend(state.getRawParameterValue(id::blend)),
      bellVoice(state.getRawParameterValue(id::bellVoice)),
      bellOctave(state.getRawParameterValue(id::bellOctave)),
      touch(state.getRawParameterValue(id::touch)),
      tune(state.getRawParameterValue(id::tune)),
      volume(state.getRawParameterValue(id::volume))
{
    jassert(blend && bellVoice && bellOctave && touch && tune && volume);
}

EngineParams ParameterReader::read() const noexcept
{
    EngineParams p;
    p.blend = blend->load(std::memory_order_relaxed);
    p.bellVoice = static_cast<BellVoice>(std::clamp(juce::roundToInt(bellVoice->load(std::memory_order_relaxed)), 0, 2));
    p.bellOctave = juce::roundToInt(bellOctave->load(std::memory_order_relaxed));
    p.touch = touch->load(std::memory_order_relaxed);
    p.tuneCents = tune->load(std::memory_order_relaxed);
    p.volumeDb = volume->load(std::memory_order_relaxed);
    return p;
}
} // namespace keys::params
