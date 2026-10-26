#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// =================================================================================================
//  The "Nocturne" look: deep navy, soft dark tiles, off-white lines and one signal red.
//  (docs/mockup/nocturne.html is the reference.)
// =================================================================================================
namespace keys::ui
{
namespace colours
{
inline const juce::Colour night { 0xff06090f };
inline const juce::Colour tile { 0x57030609 };     // rgba(3, 6, 9, .34)
inline const juce::Colour pop { 0xf7060a10 };
inline const juce::Colour fg { 0xffebe7e1 };
inline const juce::Colour fgDim { 0xff8e96a1 };
inline const juce::Colour rule { 0x8cebe7e1 };     // fg at 55 %
inline const juce::Colour line { 0x29ebe7e1 };     // fg at 16 %
inline const juce::Colour red { 0xffec3a40 };
inline const juce::Colour gold { 0xfff4e3c3 };
} // namespace colours

/// Fonts (Archivo, SIL OFL), loaded once from the binary.
juce::Font titleFont(float height);   ///< ExtraBold, wide tracking: the "K E Y S" title
juce::Font labelFont(float height);   ///< Bold expanded caps: control labels
juce::Font valueFont(float height);   ///< SemiBold semi-expanded: values, menus

class KeysLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KeysLookAndFeel();

    /// Ring knob: red ring and pointer, tick marks that light up to the value.
    /// Set the slider's "bipolar" property to light ticks from the centre instead.
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    /// Slim rail with a red cap. Set the slider's "detent" property to mark the middle.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
};
} // namespace keys::ui
