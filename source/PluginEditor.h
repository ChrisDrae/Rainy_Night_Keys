#pragma once

#include "PluginProcessor.h"
#include "ui/KeysLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

// =================================================================================================
//  KeysEditor — the plugin window (milestone M1: the playable demo).
//
//  Tiles for Blend, the bell, Touch, Tune and Level; a MIDI monitor to check your keyboard; and a
//  73-key on-screen keyboard (E1–E7, like a Mk II). In the Standalone app it also switches on every
//  connected MIDI input, so a keyboard works without visiting the settings.
// =================================================================================================
class KeysEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KeysEditor(KeysProcessor&);
    ~KeysEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 900;
    static constexpr int kHeight = 620;

private:
    void timerCallback() override;
    void enableAllMidiInputs();
    void drawTile(juce::Graphics&, juce::Rectangle<int> area) const;
    void drawMonitor(juce::Graphics&, juce::Rectangle<int> area) const;
    juce::String parameterText(const char* id) const;

    KeysProcessor& keysProcessor;
    keys::ui::KeysLookAndFeel lookAndFeel; // must outlive every component that uses it: declared first

    juce::Slider blend, level, touch, tune;
    juce::ComboBox bellVoice, bellOctave;
    juce::MidiKeyboardComponent keyboard;

    // Attachments last: they're destroyed first, before the controls they point to.
    using Apvts = juce::AudioProcessorValueTreeState;
    std::unique_ptr<Apvts::SliderAttachment> blendAttachment, levelAttachment, touchAttachment, tuneAttachment;
    std::unique_ptr<Apvts::ComboBoxAttachment> bellVoiceAttachment, bellOctaveAttachment;

    juce::Image backdrop;
    juce::Rectangle<int> chipArea, midiArea, titleArea, monitorArea;
    juce::Rectangle<int> blendTile, bellTile, touchTile, tuneTile, levelTile;

    juce::String libraryText, midiInputText;
    uint32_t lastEventCount = 0;
    float midiLamp = 0.0f, meter = 0.0f;
    int timerTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysEditor)
};
