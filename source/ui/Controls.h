#pragma once

#include "KeysLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

// =================================================================================================
//  The building blocks of the window: tiles and the controls inside them.
//
//  Every control is attached to a host parameter, so automation, MIDI mapping and presets move it.
//  Interaction is the same everywhere: drag up or down (Shift for fine steps), double-click to
//  reset, scroll wheel. A tile glows in its control's hue while one of its controls is moving,
//  and its values show while the mouse is over it.
// =================================================================================================
namespace keys::ui
{
using Apvts = juce::AudioProcessorValueTreeState;

/// A soft dark tile. Its component is kMargin larger than the visible tile on every side, so the
/// glow can spill outside; clicks in that margin pass through to whatever is underneath.
class Tile : public juce::Component
{
public:
    static constexpr int kMargin = 18;

    explicit Tile(juce::Colour hue = colours::fg);
    ~Tile() override;

    juce::Rectangle<int> getInner() const { return getLocalBounds().reduced(kMargin); }
    void setInnerBounds(juce::Rectangle<int> inner) { setBounds(inner.expanded(kMargin)); }

    void setLive(bool isLive);
    /// True while the mouse is over the tile or one of its controls is moving: values show.
    bool isHot() const noexcept { return live || hover; }
    juce::Colour getHue() const noexcept { return hue; }

    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override { return getInner().contains(x, y); }
    void mouseEnter(const juce::MouseEvent&) override { updateHover(); }
    void mouseExit(const juce::MouseEvent&) override { updateHover(); }

private:
    void updateHover();

    juce::Colour hue;
    bool live = false, hover = false;
};

/// A control's name, with its hue dot and, when it has fine controls, a ▾ that opens them.
class HueLabel final : public juce::Component
{
public:
    HueLabel(juce::String text, juce::Colour hue, bool hasDot = true);

    /// Makes the label a button with a ▾. `onClick` gets called on click.
    void setFineButton(std::function<void()> onClick);
    void setExpanded(bool isExpanded);
    bool isFineButton() const noexcept { return onClick != nullptr; }
    /// Natural width at the given height.
    int getPreferredWidth() const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::String text;
    juce::Colour hue;
    bool dot = true, expanded = false;
    std::function<void()> onClick;
};

/// A slider bound to a parameter, with the house interaction (Shift for fine steps, double-click
/// to reset to the parameter's default, scroll wheel), lighting up its tile while it moves.
class ParamSlider final : public juce::Slider
{
public:
    ParamSlider(Apvts& state, const juce::String& parameterId, Tile* tile, juce::Slider::SliderStyle style);

    juce::RangedAudioParameter& getParameter() const noexcept { return parameter; }

private:
    juce::RangedAudioParameter& parameter;
    Apvts::SliderAttachment attachment; // declared last: it's destroyed first
};

/// A knob above its label and value: "outline" (the big three) or "ring" style.
class KnobColumn final : public juce::Component
{
public:
    KnobColumn(Apvts& state, const juce::String& parameterId, const juce::String& name, Tile& tile, bool outline,
               int knobSize);

    std::function<juce::String(float)> format; ///< value text (0..1 in); default 0..100
    HueLabel label;
    ParamSlider knob;

    int getPreferredHeight() const noexcept { return knobSize + 8 + 16 + 8 + 12; }
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    Tile& tile;
    int knobSize;
};

/// A vertical fader with an optional caption above and below, then its label and value.
class FaderColumn final : public juce::Component
{
public:
    FaderColumn(Apvts& state, const juce::String& parameterId, const juce::String& name, Tile& tile, int faderHeight,
                bool detent, juce::String captionTop = {}, juce::String captionBottom = {}, bool hueDot = true);

    std::function<juce::String(float)> format; ///< value text (0..1 in); none when empty
    HueLabel label;
    ParamSlider fader;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    Tile& tile;
    int faderHeight;
    juce::String top, bottom;
};

/// A dropdown with a small caption under it (Voice, Octave, Mode...).
class CaptionedChoice final : public juce::Component
{
public:
    CaptionedChoice(Apvts& state, const juce::String& parameterId, const juce::String& caption);

    juce::ComboBox box;
    void resized() override;
    void paint(juce::Graphics&) override;
    static constexpr int kHeight = 31 + 6 + 11;

private:
    juce::String caption;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment;
};

/// A toggle bound to a bool parameter, drawn as a round light ("dot") or a red square ("check").
class ParamToggle final : public juce::ToggleButton
{
public:
    ParamToggle(Apvts& state, const juce::String& parameterId, const juce::String& text, const char* style);

private:
    Apvts::ButtonAttachment attachment;
};

/// A plain toggle in the house style (not bound to a parameter).
class CheckToggle final : public juce::ToggleButton
{
public:
    CheckToggle(const juce::String& text, const char* style = "check");
};

/// The square ☰ button.
class MenuButton final : public juce::Button
{
public:
    MenuButton();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
};

/// The rounded "chip" in the header: which sounds are loaded, plus a MIDI light.
class LibraryChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setText(const juce::String& newText);
    void setMidiLight(float level);
    int getPreferredWidth() const;
    void paint(juce::Graphics&) override;

private:
    juce::String text;
    float light = 0.0f;
};

/// The red "K E Y S" title, with a soft red glow.
class TitleView final : public juce::Component
{
public:
    TitleView();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Path glyphs;
};

/// A line of spaced capitals (the footer's "Tine & Bell").
class CaptionView final : public juce::Component
{
public:
    CaptionView(juce::String text, juce::Font font, juce::Colour colour);
    void paint(juce::Graphics&) override;

private:
    juce::String text;
    juce::Font font;
    juce::Colour colour;
};

/// ◀ preset name ▶. Clicking the name opens the list.
class PresetBox final : public juce::Component
{
public:
    PresetBox();
    void setName(const juce::String& newName);

    std::function<void()> onPrevious, onNext, onOpenList;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<int> previousArea, nextArea, nameArea;
    juce::String name;
};
} // namespace keys::ui
