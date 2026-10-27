#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
const juce::Identifier tinesProperty { "tines" }; // "sampled" or "modeled"
}

KeysProcessor::KeysProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Keys", keys::params::createLayout()),
      parameters(apvts)
{
}

void KeysProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    engine.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    keyboardState.reset();
}

bool KeysProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void KeysProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Merge the on-screen keyboard into the incoming MIDI (and light its keys for incoming notes).
    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);

    // The window's pedal (button or space bar) becomes an ordinary sustain-pedal message.
    if (const bool pedal = uiPedal.load(std::memory_order_relaxed); pedal != uiPedalSent)
    {
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, pedal ? 127 : 0), 0);
        uiPedalSent = pedal;
    }

    const keys::Banks banks { library->tines(sampledTines.load(std::memory_order_relaxed)), library->celeste(),
                              library->vibes() };
    engine.process(buffer, midi, parameters.read(), banks);
    midi.clear();
}

juce::AudioProcessorEditor* KeysProcessor::createEditor()
{
    return new KeysEditor(*this);
}

void KeysProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void KeysProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            sampledTines.store(apvts.state.getProperty(tinesProperty, "sampled").toString() != "modeled");
            const auto name = apvts.state.getProperty("preset").toString();
            juce::MessageManager::callAsync([this, alive = juce::WeakReference<KeysProcessor>(this), name]
            {
                if (alive != nullptr)
                    presets.restoreSelection(name);
            });
        }
}

void KeysProcessor::setSampledTines(bool useSamples)
{
    sampledTines.store(useSamples);
    apvts.state.setProperty(tinesProperty, useSamples ? "sampled" : "modeled", nullptr);
}

int KeysProcessor::getNumPrograms()
{
    return static_cast<int>(keys::factoryPresets().size());
}

int KeysProcessor::getCurrentProgram()
{
    return std::min(presets.getCurrentIndex(), getNumPrograms() - 1);
}

void KeysProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    if (juce::MessageManager::existsAndIsCurrentThread())
        presets.load(index, false);
    else
        juce::MessageManager::callAsync([this, alive = juce::WeakReference<KeysProcessor>(this), index]
        {
            if (alive != nullptr)
                presets.load(index, false);
        });
}

const juce::String KeysProcessor::getProgramName(int index)
{
    return index >= 0 && index < getNumPrograms() ? keys::factoryPresets()[static_cast<size_t>(index)].name : juce::String();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeysProcessor();
}
