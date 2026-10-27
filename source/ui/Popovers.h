#pragma once

#include "Controls.h"

#include <vector>

namespace keys
{
struct EngineStatus;
}

// =================================================================================================
//  Popovers: the ▾ fine controls of Warmth, Bark, Body and Space, and the ☰ menu.
//
//  A popover closes when you click anywhere outside it (the click still does its job), press
//  Escape, or click ✕ / its opener again.
// =================================================================================================
namespace keys::ui
{
class Popover : public juce::Component
{
public:
    static constexpr int kMargin = 16; ///< room around the panel for its shadow

    Popover();
    ~Popover() override;

    /// Clicks on the opener don't count as "outside" (it toggles the popover itself).
    void setOpener(juce::Component* opener) { openerComponent = opener; }
    std::function<void()> onDismiss;

    juce::Rectangle<int> getPanel() const { return getLocalBounds().reduced(kMargin); }
    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override { return getPanel().contains(x, y); }

protected:
    void dismiss();

private:
    struct OutsideClick final : juce::MouseListener
    {
        explicit OutsideClick(Popover& p) : owner(p) {}
        void mouseDown(const juce::MouseEvent&) override;
        Popover& owner;
    };
    OutsideClick outsideClick { *this };
    juce::Component::SafePointer<juce::Component> openerComponent;
};

/// One row of a popover: a caption and value over a horizontal slider.
class FineSlider final : public juce::Component
{
public:
    FineSlider(Apvts& state, const juce::String& parameterId, const juce::String& caption);

    std::function<juce::String()> format; ///< value text; default: the parameter's own text
    ParamSlider slider;

    static constexpr int kHeight = 12 + 4 + 22;
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    juce::String caption;
};

/// The ▾ panel for one colour knob.
class FinePanel final : public Popover
{
public:
    /// `key`: "warmth", "bark", "body" or "space".
    FinePanel(Apvts& state, const juce::String& key);

    const juce::String& getKey() const noexcept { return key; }
    int getPreferredHeight() const;
    static constexpr int kWidth = 250;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String key, title;
    std::vector<std::unique_ptr<juce::Component>> rows;
    std::unique_ptr<juce::Button> closer; // the ✕
};

/// The ☰ menu: the library, Tune and Touch, the still background, the MIDI monitor and credits.
class MenuPanel final : public Popover, public juce::Timer
{
public:
    MenuPanel(Apvts& state, const keys::EngineStatus& status);

    std::function<juce::String()> libraryText, libraryNote, midiInputText;
    CheckToggle sampledTines { "Sampled", "dot" }, modeledTines { "Modeled", "dot" };
    CheckToggle still { "Still background" };

    static constexpr int kWidth = 310;
    int getPreferredHeight() const { return kHeight + 2 * kMargin; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override { repaint(); }
    void visibilityChanged() override
    {
        if (isVisible())
            startTimerHz(10);
        else
            stopTimer();
    }

private:
    static constexpr int kHeight = 508;
    const keys::EngineStatus& status;
    FineSlider tune, touch;
    juce::Rectangle<int> librarySection, midiSection, creditsSection;
};
} // namespace keys::ui
