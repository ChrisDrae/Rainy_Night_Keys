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
} // namespace

juce::Font titleFont(float height) { return makeFont(typefaces().extraBold, height, 0.62f); }
juce::Font labelFont(float height) { return makeFont(typefaces().bold, height, 0.1f); }
juce::Font valueFont(float height) { return makeFont(typefaces().semiBold, height, 0.02f); }

KeysLookAndFeel::KeysLookAndFeel()
{
    setColour(juce::ComboBox::textColourId, colours::fg);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, colours::rule);
    setColour(juce::ComboBox::arrowColourId, colours::fg);
    setColour(juce::PopupMenu::backgroundColourId, colours::pop);
    setColour(juce::PopupMenu::textColourId, colours::fg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::red.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::fg);
    setColour(juce::Label::textColourId, colours::fg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void KeysLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                                               static_cast<float>(height));
    const float outer = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool bipolar = slider.getProperties()["bipolar"];
    const float middle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;

    auto pointAt = [centre](float a, float r)
    { return centre.translated(r * std::sin(a), -r * std::cos(a)); };

    // Ticks: lit up to the value (or between the centre and the value, for bipolar knobs).
    constexpr int numTicks = 21;
    for (int i = 0; i < numTicks; ++i)
    {
        const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * static_cast<float>(i) / (numTicks - 1);
        const bool lit = bipolar ? (a >= std::min(middle, angle) - 0.01f && a <= std::max(middle, angle) + 0.01f)
                                 : a <= angle + 0.01f;
        g.setColour(colours::fg.withAlpha(lit ? 0.95f : 0.3f));
        g.drawLine(juce::Line<float>(pointAt(a, outer * 0.78f), pointAt(a, outer * 0.97f)), 1.6f);
    }

    const float ring = outer * 0.6f;
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillEllipse(juce::Rectangle<float>(ring * 2.0f, ring * 2.0f).withCentre(centre));
    g.setColour(colours::red);
    g.drawEllipse(juce::Rectangle<float>(ring * 2.0f, ring * 2.0f).withCentre(centre), 2.6f);

    juce::Path pointer;
    pointer.startNewSubPath(centre);
    pointer.lineTo(pointAt(angle, ring * 0.86f));
    g.strokePath(pointer, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void KeysLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                                               static_cast<float>(height));
    const bool detent = slider.getProperties()["detent"];

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = bounds.getCentreX();
        g.setColour(colours::rule);
        g.fillRect(juce::Rectangle<float>(cx - 1.0f, bounds.getY(), 2.0f, bounds.getHeight()));
        if (detent)
            g.fillRect(juce::Rectangle<float>(cx - 8.0f, bounds.getCentreY() - 0.75f, 16.0f, 1.5f));

        const auto cap = juce::Rectangle<float>(32.0f, 6.0f).withCentre({ cx, sliderPos });
        g.setColour(colours::red.withAlpha(0.35f));
        g.fillRoundedRectangle(cap.expanded(3.0f, 2.0f), 3.0f);
        g.setColour(colours::red);
        g.fillRoundedRectangle(cap, 1.0f);
        return;
    }

    const float cy = bounds.getCentreY();
    g.setColour(colours::rule);
    g.fillRect(juce::Rectangle<float>(bounds.getX(), cy - 1.0f, bounds.getWidth(), 2.0f));
    g.setColour(colours::red);
    g.fillRoundedRectangle(juce::Rectangle<float>(6.0f, 18.0f).withCentre({ sliderPos, cy }), 1.0f);
}

void KeysLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                   juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds, 9.0f);
    g.setColour(box.hasKeyboardFocus(true) ? colours::fg : colours::rule);
    g.drawRoundedRectangle(bounds, 9.0f, 1.5f);

    // ▾
    const float ax = bounds.getRight() - 16.0f, ay = bounds.getCentreY();
    juce::Path arrow;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(colours::fg);
    g.fillPath(arrow);
}

void KeysLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

juce::Font KeysLookAndFeel::getComboBoxFont(juce::ComboBox&) { return valueFont(13.5f); }
juce::Font KeysLookAndFeel::getPopupMenuFont() { return valueFont(14.0f); }

void KeysLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(colours::pop);
    g.setColour(colours::line);
    g.drawRect(0, 0, width, height, 1);
}
} // namespace keys::ui
