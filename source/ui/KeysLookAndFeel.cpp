#include "KeysLookAndFeel.h"

#include <BinaryData.h>

namespace keys::ui
{
namespace
{
struct Typefaces
{
    juce::Typeface::Ptr extraBold = juce::Typeface::createSystemTypefaceFor(
        KeysFonts::ArchivoExpandedExtraBold_ttf, static_cast<size_t>(KeysFonts::ArchivoExpandedExtraBold_ttfSize));
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor(
        KeysFonts::ArchivoExpandedBold_ttf, static_cast<size_t>(KeysFonts::ArchivoExpandedBold_ttfSize));
    juce::Typeface::Ptr semiBold = juce::Typeface::createSystemTypefaceFor(
        KeysFonts::ArchivoSemiExpandedSemiBold_ttf, static_cast<size_t>(KeysFonts::ArchivoSemiExpandedSemiBold_ttfSize));
};

const Typefaces& typefaces()
{
    static const Typefaces instance; // [C++] a function-local static is created once, thread-safely
    return instance;
}

juce::Font makeFont(const juce::Typeface::Ptr& face, float height, float tracking)
{
    if (face == nullptr)
        return juce::Font(juce::FontOptions(height, juce::Font::bold)).withExtraKerningFactor(tracking);
    return juce::Font(juce::FontOptions(face).withHeight(height).withKerningFactor(tracking));
}

juce::Rectangle<float> floatBounds(int x, int y, int w, int h)
{
    return { static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h) };
}
} // namespace

juce::Font titleFont(float height) { return makeFont(typefaces().extraBold, height, 0.62f); }
juce::Font labelFont(float height, float tracking) { return makeFont(typefaces().bold, height, tracking); }
juce::Font valueFont(float height, float tracking) { return makeFont(typefaces().semiBold, height, tracking); }

void drawCaps(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
              juce::Colour colour, juce::Justification justification)
{
    // Letter spacing also follows the last letter; shift centred text by half of it so it looks centred.
    if (justification.testFlags(juce::Justification::horizontallyCentred))
        area = area.translated(font.getExtraKerningFactor() * font.getHeight() * 0.5f, 0.0f);
    g.setColour(colour);
    g.setFont(font);
    g.drawText(text.toUpperCase(), area, justification, false);
}

KeysLookAndFeel::KeysLookAndFeel()
{
    setColour(juce::ComboBox::textColourId, colours::fg);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, colours::rule);
    setColour(juce::ComboBox::arrowColourId, colours::fg);
    setColour(juce::PopupMenu::backgroundColourId, colours::pop);
    setColour(juce::PopupMenu::textColourId, colours::fg);
    setColour(juce::PopupMenu::headerTextColourId, colours::fgDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::red.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::fg);
    setColour(juce::Label::textColourId, colours::fg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::backgroundColourId, colours::pop);
    setColour(juce::TooltipWindow::textColourId, colours::fg);
    setColour(juce::TooltipWindow::outlineColourId, colours::line);
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.3f));
    setColour(juce::TextEditor::textColourId, colours::fg);
    setColour(juce::TextEditor::outlineColourId, colours::rule);
    setColour(juce::TextEditor::focusedOutlineColourId, colours::fg);
    setColour(juce::AlertWindow::backgroundColourId, colours::pop);
    setColour(juce::AlertWindow::textColourId, colours::fg);
    setColour(juce::AlertWindow::outlineColourId, colours::line);
    setColour(juce::TextButton::buttonColourId, colours::tile);
    setColour(juce::TextButton::textColourOffId, colours::fg);
}

// -------------------------------------------------------------------------------------------------
//  Knobs
// -------------------------------------------------------------------------------------------------
void KeysLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = floatBounds(x, y, width, height);
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getProperties()["bipolar"];
    const bool outline = slider.getProperties()["style"].toString() == "outline";
    const float middle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;

    // The mockup draws knobs on a 120 (outline) or 80 (ring) unit square; scale those units here.
    const float unit = std::min(bounds.getWidth(), bounds.getHeight()) / (outline ? 120.0f : 80.0f);
    auto pointAt = [centre](float a, float r) { return centre.translated(r * std::sin(a), -r * std::cos(a)); };

    // Ticks: lit up to the value (or between the centre and the value, for bipolar knobs).
    const int numTicks = outline ? 11 : 21;
    const float tickInner = (outline ? 49.0f : 31.0f) * unit, tickOuter = (outline ? 56.0f : 36.0f) * unit;
    for (int i = 0; i < numTicks; ++i)
    {
        const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * static_cast<float>(i) / static_cast<float>(numTicks - 1);
        const bool lit = bipolar ? (a >= std::min(middle, angle) - 0.01f && a <= std::max(middle, angle) + 0.01f)
                                 : a <= angle + 0.01f;
        g.setColour(colours::fg.withAlpha(lit ? 0.95f : 0.3f));
        g.drawLine(juce::Line<float>(pointAt(a, tickInner), pointAt(a, tickOuter)), (outline ? 2.2f : 1.6f) * unit);
    }

    const float ringRadius = (outline ? 39.0f : 23.0f) * unit;
    const auto ring = juce::Rectangle<float>(ringRadius * 2.0f, ringRadius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(outline ? 0.16f : 0.24f));
    g.fillEllipse(ring);
    g.setColour(outline ? colours::fg.withAlpha(0.8f) : colours::red);
    g.drawEllipse(ring, (outline ? 1.6f : 2.8f) * unit);

    juce::Path pointer;
    pointer.startNewSubPath(pointAt(angle, (outline ? 4.0f : 0.0f) * unit));
    pointer.lineTo(pointAt(angle, (outline ? 36.0f : 20.0f) * unit));
    g.setColour(colours::red);
    g.strokePath(pointer, juce::PathStrokeType((outline ? 3.2f : 2.8f) * unit, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(colours::fg.withAlpha(0.6f));
        g.drawEllipse(ring.expanded(3.0f * unit), 1.0f);
    }
}

// -------------------------------------------------------------------------------------------------
//  Faders
// -------------------------------------------------------------------------------------------------
juce::Slider::SliderLayout KeysLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = slider.getLocalBounds();
    if (slider.getSliderStyle() == juce::Slider::LinearVertical)
        layout.sliderBounds = layout.sliderBounds.reduced(0, 3); // the cap's half-height
    else if (slider.getSliderStyle() == juce::Slider::LinearHorizontal)
        layout.sliderBounds = layout.sliderBounds.reduced(3, 0);
    return layout;
}

void KeysLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = floatBounds(x, y, width, height);
    const bool detent = slider.getProperties()["detent"];

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = bounds.getCentreX();
        g.setColour(colours::rule);
        g.fillRect(juce::Rectangle<float>(cx - 1.0f, bounds.getY() - 3.0f, 2.0f, bounds.getHeight() + 6.0f));
        if (detent)
            g.fillRect(juce::Rectangle<float>(cx - 8.0f, bounds.getCentreY() - 0.75f, 16.0f, 1.5f));

        const auto cap = juce::Rectangle<float>(32.0f, 6.0f).withCentre({ cx, sliderPos });
        for (int i = 3; i >= 1; --i) // a soft red glow (box-shadow: 0 0 14px)
        {
            g.setColour(colours::red.withAlpha(0.09f));
            g.fillRoundedRectangle(cap.expanded(static_cast<float>(i) * 2.5f), 3.0f + static_cast<float>(i));
        }
        g.setColour(colours::red);
        g.fillRoundedRectangle(cap, 1.0f);
        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(colours::fg.withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.expanded(2.0f, 5.0f), 6.0f, 1.0f);
        }
        return;
    }

    const float cy = bounds.getCentreY();
    g.setColour(colours::rule);
    g.fillRect(juce::Rectangle<float>(bounds.getX() - 3.0f, cy - 1.0f, bounds.getWidth() + 6.0f, 2.0f));
    if (detent)
        g.fillRect(juce::Rectangle<float>(bounds.getCentreX() - 0.75f, cy - 5.0f, 1.5f, 10.0f));
    g.setColour(colours::red);
    g.fillRoundedRectangle(juce::Rectangle<float>(6.0f, 18.0f).withCentre({ sliderPos, cy }), 1.0f);
}

// -------------------------------------------------------------------------------------------------
//  Toggles
// -------------------------------------------------------------------------------------------------
void KeysLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    const bool dot = button.getProperties()["style"].toString() == "dot";
    auto area = button.getLocalBounds().toFloat();
    const float alpha = button.isEnabled() ? 1.0f : 0.45f;

    if (dot)
    {
        const auto light = juce::Rectangle<float>(13.0f, 13.0f).withCentre({ area.getX() + 8.5f, area.getCentreY() });
        if (on)
        {
            g.setColour(colours::fg.withAlpha(alpha));
            g.fillEllipse(light);
        }
        g.setColour(colours::fg.withAlpha(alpha * (highlighted ? 1.0f : 0.95f)));
        g.drawEllipse(light.reduced(0.8f), 1.6f);
        drawCaps(g, button.getButtonText(), area.withTrimmedLeft(25.0f), labelFont(11.0f, 0.12f), colours::fg.withAlpha(alpha),
                 juce::Justification::centredLeft);
        return;
    }

    const auto box = juce::Rectangle<float>(16.0f, 16.0f).withCentre({ area.getX() + 10.0f, area.getCentreY() });
    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour(colours::red.withAlpha(0.08f));
            g.fillRoundedRectangle(box.expanded(static_cast<float>(i) * 2.0f), 3.0f + static_cast<float>(i));
        }
        g.setColour(colours::red.withAlpha(alpha));
        g.fillRoundedRectangle(box, 3.0f);
    }
    else
    {
        g.setColour((highlighted ? colours::fg : colours::rule).withAlpha(alpha));
        g.drawRoundedRectangle(box.reduced(0.8f), 3.0f, 1.6f);
    }
    drawCaps(g, button.getButtonText(), area.withTrimmedLeft(28.0f), labelFont(10.5f, 0.14f), colours::fg.withAlpha(alpha),
             juce::Justification::centredLeft);
}

// -------------------------------------------------------------------------------------------------
//  Dropdowns and menus
// -------------------------------------------------------------------------------------------------
void KeysLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                   juce::ComboBox& box)
{
    const auto bounds = floatBounds(0, 0, width, height).reduced(0.75f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(box.isMouseOver(true) || box.isPopupActive() ? colours::fg.withAlpha(0.85f) : colours::rule);
    g.drawRoundedRectangle(bounds, 10.0f, 1.5f);

    // ▾
    const float ax = bounds.getRight() - 15.0f, ay = bounds.getCentreY();
    juce::Path arrow;
    arrow.addTriangle(ax - 3.5f, ay - 1.75f, ax + 3.5f, ay - 1.75f, ax, ay + 2.75f);
    g.setColour(colours::fg);
    g.fillPath(arrow);
}

void KeysLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

juce::Font KeysLookAndFeel::getComboBoxFont(juce::ComboBox&) { return valueFont(12.5f, 0.03f); }
juce::Font KeysLookAndFeel::getPopupMenuFont() { return valueFont(13.5f); }

void KeysLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(colours::pop);
    g.setColour(colours::line);
    g.drawRect(0, 0, width, height, 1);
}

juce::PopupMenu::Options KeysLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label)
{
    return LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label).withStandardItemHeight(26);
}
} // namespace keys::ui
