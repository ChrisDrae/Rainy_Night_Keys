#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// =================================================================================================
//  KeysKeyboard — the on-screen keys, E1–E7 like a 73-key Mk II, in the Nocturne style: pale keys
//  that blush red when played, and a thin red line over the keys your computer keyboard reaches.
//  Lower on a key is louder.
// =================================================================================================
namespace keys::ui
{
class KeysKeyboard final : public juce::MidiKeyboardComponent
{
public:
    static constexpr int kLowest = 28;  // E1
    static constexpr int kHighest = 100; // E7

    explicit KeysKeyboard(juce::MidiKeyboardState& state);

    /// Marks the keys the computer keyboard plays (`lowest` .. `lowest` + span).
    void setComputerKeyRange(int lowest, int span);

    void resized() override;

private:
    void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour noteFillColour) override;

    int computerLowest = 48, computerSpan = 16;
};

/// The strip along the bottom: the keyboard, with a line of hints above it.
class KeysStrip final : public juce::Component
{
public:
    explicit KeysStrip(juce::MidiKeyboardState& state);

    KeysKeyboard keyboard;
    /// Shows which notes the computer keyboard plays.
    void setComputerKeyRange(int lowest, int span);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String rangeText;
};
} // namespace keys::ui
