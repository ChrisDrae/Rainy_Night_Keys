#pragma once

#include "Parameters.h"
#include "engine/KeysEngine.h"
#include "engine/KeysLibrary.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  KeysProcessor — the object the host (or the Standalone app) talks to.
//
//  It stays thin: parameters in, MIDI in, the engine renders. The sample library is shared by every
//  instance in the process and loads itself in the background; until the tines are ready, notes
//  are silent (it takes about a second).
// =================================================================================================
class KeysProcessor final : public juce::AudioProcessor
{
public:
    KeysProcessor();
    ~KeysProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // [RT] Called on the audio thread, once per block.
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- for the editor ---------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    keys::EngineStatus& getStatus() noexcept { return engine.status(); }
    const keys::KeysLibrary& getLibrary() const noexcept { return *library; }

    /// The on-screen keyboard and incoming MIDI meet here, so keys light up for both.
    juce::MidiKeyboardState keyboardState;

private:
    juce::AudioProcessorValueTreeState apvts;
    keys::params::ParameterReader parameters; // reads apvts: declared after it
    juce::SharedResourcePointer<keys::KeysLibrary> library;
    keys::KeysEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysProcessor)
};
