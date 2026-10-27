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
inline const juce::Colour fgSoft { 0xffc9c5bf };   // body text in popovers
inline const juce::Colour fgDim { 0xff8e96a1 };
inline const juce::Colour rule { 0x8cebe7e1 };     // fg at 55 %
inline const juce::Colour line { 0x29ebe7e1 };     // fg at 16 %
inline const juce::Colour red { 0xffec3a40 };
inline const juce::Colour keysStrip { 0xf004060a }; // rgba(4, 6, 10, .94)

// Each colour knob owns a hue: its dot, its tile's glow, and its light behind the controls.
inline const juce::Colour warmth { 0xffd64a3a };
inline const juce::Colour bark { 0xffe8263f };
inline const juce::Colour body { 0xffb02a84 };
inline const juce::Colour patina { 0xff93708a };
inline const juce::Colour motion { 0xff6a46dc };
inline const juce::Colour space { 0xff2856b0 };
inline const juce::Colour bell { 0xfff4e3c3 };  // celeste: pale gold
inline const juce::Colour vibes { 0xffbfe3ff }; // vibraphone: ice blue
inline const juce::Colour dust { 0xffcdbfa6 };
} // namespace colours

/// Fonts (Archivo, SIL OFL), loaded once from the binary. `tracking` is letter spacing in ems.
juce::Font titleFont(float height);                         ///< ExtraBold, wide tracking: the "K E Y S" title
juce::Font labelFont(float height, float tracking = 0.1f);  ///< Bold expanded: labels and captions
juce::Font valueFont(float height, float tracking = 0.02f); ///< SemiBold semi-expanded: values, menus

/// Draws `text` with letter spacing, the way the mockup's upper-case labels look.
void drawCaps(juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
              juce::Colour colour, juce::Justification justification = juce::Justification::centred);

class KeysLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KeysLookAndFeel();

    /// Knobs. The slider's "style" property picks the look: "outline" (the big three: thin ring,
    /// red pointer, eleven ticks) or anything else (a red ring with 21 ticks). Ticks light up to
    /// the value, or out from the centre when the "bipolar" property is set.
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    /// Vertical: a slim rail with a red cap ("detent" marks the middle). Horizontal: a rail with a
    /// slim red thumb, as in the fine-control popovers.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;

    /// Toggles. The button's "style" property: "dot" (a round light, Patina's switches) or
    /// "check" (a square that turns red: Keys, Pedal, Still background).
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&, juce::Label&) override;
};
} // namespace keys::ui
