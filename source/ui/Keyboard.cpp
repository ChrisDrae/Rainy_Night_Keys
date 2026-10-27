#include "Keyboard.h"

#include "KeysLookAndFeel.h"

namespace keys::ui
{
KeysKeyboard::KeysKeyboard(juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setAvailableRange(kLowest, kHighest);
    setOctaveForMiddleC(4);
    setScrollButtonsVisible(false);
    setBlackNoteWidthProportion(0.62f);
    setBlackNoteLengthProportion(0.6f);
    setVelocity(0.7f, true); // lower on a key is louder
    clearKeyMappings();      // the editor plays the computer keyboard itself (with Z/X and the pedal)
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setOpaque(false);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setTitle("On-screen keyboard");
}

void KeysKeyboard::setComputerKeyRange(int lowest, int span)
{
    computerLowest = lowest;
    computerSpan = span;
    repaint();
}

void KeysKeyboard::resized()
{
    // 43 white keys from E1 to E7 fill the width exactly.
    setKeyWidth(static_cast<float>(getWidth()) / 43.0f);
    juce::MidiKeyboardComponent::resized();
}

void KeysKeyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                 juce::Colour, juce::Colour)
{
    const auto key = area.withTrimmedRight(1.0f);
    juce::Path shape;
    shape.addRoundedRectangle(key.getX(), key.getY(), key.getWidth(), key.getHeight(), 3.0f, 3.0f, false, false, true, true);
    g.setGradientFill(isDown ? juce::ColourGradient::vertical(juce::Colour(0xfff6cfd0), juce::Colour(0xffe2686d), key)
                             : juce::ColourGradient::vertical(juce::Colour(0xffd3d9de), juce::Colour(0xffa9b2ba), key));
    g.fillPath(shape);
    if (isOver && !isDown)
    {
        g.setColour(colours::red.withAlpha(0.12f));
        g.fillPath(shape);
    }

    // The computer keyboard's reach: a thin red line along the top.
    if (note >= computerLowest && note <= computerLowest + computerSpan)
    {
        g.setColour(colours::red.withAlpha(0.8f));
        g.fillRect(key.withHeight(2.0f));
    }

    g.setColour(juce::Colour(0xcc04060a));
    g.fillRect(area.withLeft(area.getRight() - 1.0f));

    if (note % 12 == 0)
    {
        g.setColour(juce::Colour(0xff56606a));
        g.setFont(valueFont(8.5f, 0.0f));
        g.drawText(getWhiteNoteText(note), key.withTrimmedBottom(4.0f), juce::Justification::centredBottom, false);
    }
}

void KeysKeyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    juce::Path shape;
    shape.addRoundedRectangle(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 2.0f, 2.0f, false, false, true, true);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); // box-shadow: 0 2px 3px
    g.fillRoundedRectangle(area.translated(0.0f, 2.0f).expanded(0.5f, 0.0f), 2.0f);
    g.setGradientFill(isDown ? juce::ColourGradient::vertical(juce::Colour(0xff8a2329), juce::Colour(0xff4a0f13), area)
                             : juce::ColourGradient::vertical(juce::Colour(0xff1a1f26), juce::Colour(0xff07090c), area));
    g.fillPath(shape);
    if (isOver && !isDown)
    {
        g.setColour(colours::red.withAlpha(0.18f));
        g.fillPath(shape);
    }
}

// =================================================================================================
//  KeysStrip
// =================================================================================================
KeysStrip::KeysStrip(juce::MidiKeyboardState& state) : keyboard(state)
{
    addAndMakeVisible(keyboard);
    setComputerKeyRange(48, 16);
}

void KeysStrip::setComputerKeyRange(int lowest, int span)
{
    keyboard.setComputerKeyRange(lowest, span);
    const juce::String dash = juce::String::fromUTF8(" \xe2\x80\x93 "), dot = juce::String::fromUTF8(" \xc2\xb7 ");
    rangeText = "Computer keys " + juce::MidiMessage::getMidiNoteName(lowest, true, true, 4) + dash
                + juce::MidiMessage::getMidiNoteName(lowest + span, true, true, 4) + dot + "Z / X octave";
    repaint();
}

void KeysStrip::resized()
{
    keyboard.setBounds(getLocalBounds().reduced(22, 0).withTrimmedTop(26).withTrimmedBottom(14));
}

void KeysStrip::paint(juce::Graphics& g)
{
    g.fillAll(colours::keysStrip);
    const auto meta = getLocalBounds().reduced(22, 0).removeFromTop(26).toFloat();
    const auto font = labelFont(9.5f, 0.16f);
    drawCaps(g, rangeText, meta, font, colours::fgDim, juce::Justification::centredLeft);
    drawCaps(g, "Hold space for the pedal", meta.withTrimmedRight(font.getHeight() * 0.16f), font, colours::fgDim,
             juce::Justification::centredRight);
}
} // namespace keys::ui
