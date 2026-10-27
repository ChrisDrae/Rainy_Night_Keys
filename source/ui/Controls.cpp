#include "Controls.h"

namespace keys::ui
{
namespace
{
juce::String percentText(float v)
{
    return juce::String(juce::roundToInt(v * 100.0f));
}

/// The value line under a control: mono-ish, dim, only while its tile is hot.
void drawValue(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour(colours::fgDim);
    g.setFont(valueFont(10.5f, 0.0f));
    g.drawText(text, area, juce::Justification::centred, false);
}

void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    drawCaps(g, text, area.toFloat(), labelFont(8.5f, 0.2f), colours::fgDim);
}
} // namespace

// =================================================================================================
//  Tile
// =================================================================================================
Tile::Tile(juce::Colour h) : hue(h)
{
    addMouseListener(this, true); // hear about the mouse over our controls, too
}

Tile::~Tile()
{
    removeMouseListener(this);
}

void Tile::setLive(bool isLive)
{
    if (live == isLive)
        return;
    live = isLive;
    repaint();
}

void Tile::updateHover()
{
    const bool over = isMouseOver(true);
    if (over == hover)
        return;
    hover = over;
    repaint();
}

void Tile::paint(juce::Graphics& g)
{
    const auto inner = getInner().toFloat();
    constexpr float radius = 18.0f;
    if (live)
    {
        // The glow: box-shadow 0 0 34px -10px in the hue, then a thin hue line inside the edge.
        for (int i = 6; i >= 1; --i)
        {
            const float spread = static_cast<float>(i) * 2.6f - 4.0f;
            g.setColour(hue.withAlpha(0.07f));
            g.fillRoundedRectangle(inner.expanded(spread), radius + spread);
        }
    }
    g.setColour(colours::tile);
    g.fillRoundedRectangle(inner, radius);
    if (live)
    {
        g.setColour(hue.withAlpha(0.6f));
        g.drawRoundedRectangle(inner.reduced(0.5f), radius, 1.0f);
    }
}

// =================================================================================================
//  HueLabel
// =================================================================================================
HueLabel::HueLabel(juce::String t, juce::Colour h, bool hasDot) : text(std::move(t)), hue(h), dot(hasDot)
{
    setInterceptsMouseClicks(false, false);
}

void HueLabel::setFineButton(std::function<void()> callback)
{
    onClick = std::move(callback);
    setInterceptsMouseClicks(onClick != nullptr, false);
    setMouseCursor(onClick != nullptr ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    setTitle(text + " fine controls");
}

void HueLabel::setExpanded(bool isExpanded)
{
    if (expanded != isExpanded)
    {
        expanded = isExpanded;
        repaint();
    }
}

int HueLabel::getPreferredWidth() const
{
    const auto font = labelFont(12.5f, 0.1f);
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text.toUpperCase());
    return juce::roundToInt(textWidth + (dot ? 13.0f : 0.0f) + (onClick != nullptr ? 17.0f : 0.0f) + 8.0f);
}

void HueLabel::paint(juce::Graphics& g)
{
    const auto font = labelFont(12.5f, 0.1f);
    const float textWidth = juce::GlyphArrangement::getStringWidth(font, text.toUpperCase());
    const float total = textWidth + (dot ? 13.0f : 0.0f) + (onClick != nullptr ? 17.0f : 0.0f);
    float x = (static_cast<float>(getWidth()) - total) * 0.5f;
    const float cy = static_cast<float>(getHeight()) * 0.5f;

    if (onClick != nullptr && isMouseOver())
    {
        g.setColour(colours::fg.withAlpha(0.06f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().withSizeKeepingCentre(total + 12.0f, static_cast<float>(getHeight())), 6.0f);
    }

    if (dot)
    {
        const auto d = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ x + 2.5f, cy });
        g.setColour(hue.withAlpha(0.35f));
        g.fillEllipse(d.expanded(3.0f));
        g.setColour(hue);
        g.fillEllipse(d);
        x += 13.0f;
    }
    g.setColour(colours::fg);
    g.setFont(font);
    g.drawText(text.toUpperCase(), juce::Rectangle<float>(x, 0.0f, textWidth + 4.0f, static_cast<float>(getHeight())),
               juce::Justification::centredLeft, false);
    if (onClick != nullptr)
    {
        const float ax = x + textWidth + 9.0f;
        juce::Path arrow;
        arrow.addTriangle(ax - 3.0f, cy - 1.5f, ax + 3.0f, cy - 1.5f, ax, cy + 2.5f);
        g.setColour(expanded ? colours::red : colours::fgDim);
        g.fillPath(arrow);
    }
}

void HueLabel::mouseUp(const juce::MouseEvent& e)
{
    if (onClick != nullptr && getLocalBounds().contains(e.getPosition()))
        onClick();
}

// =================================================================================================
//  ParamSlider
// =================================================================================================
ParamSlider::ParamSlider(Apvts& state, const juce::String& parameterId, Tile* tile, juce::Slider::SliderStyle style)
    : parameter(*state.getParameter(parameterId)),
      attachment(state, parameterId, *this)
{
    setSliderStyle(style);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setDoubleClickReturnValue(true, parameter.convertFrom0to1(parameter.getDefaultValue()));
    setTitle(parameter.getName(64));
    if (style == juce::Slider::RotaryHorizontalVerticalDrag)
    {
        setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity(200);
        // Hold Shift for fine steps (velocity mode, slowed right down).
        setVelocityModeParameters(0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    }
    if (tile != nullptr)
    {
        onDragStart = [tile] { tile->setLive(true); };
        onDragEnd = [tile] { tile->setLive(false); };
    }
    onValueChange = [this] { if (auto* p = getParentComponent()) p->repaint(); };
}

// =================================================================================================
//  KnobColumn
// =================================================================================================
KnobColumn::KnobColumn(Apvts& state, const juce::String& parameterId, const juce::String& name, Tile& t,
                       bool outline, int size)
    : label(name, t.getHue()),
      knob(state, parameterId, &t, juce::Slider::RotaryHorizontalVerticalDrag),
      tile(t),
      knobSize(size)
{
    knob.getProperties().set("style", outline ? "outline" : "ring");
    format = percentText;
    addAndMakeVisible(knob);
    addAndMakeVisible(label);
}

void KnobColumn::resized()
{
    auto area = getLocalBounds();
    knob.setBounds(area.removeFromTop(knobSize).withSizeKeepingCentre(knobSize, knobSize));
    area.removeFromTop(8);
    label.setBounds(area.removeFromTop(16).withSizeKeepingCentre(std::min(area.getWidth(), label.getPreferredWidth() + 12), 16));
}

void KnobColumn::paint(juce::Graphics& g)
{
    if (format && tile.isHot())
        drawValue(g, getLocalBounds().removeFromBottom(12), format(static_cast<float>(knob.valueToProportionOfLength(knob.getValue()))));
}

// =================================================================================================
//  FaderColumn
// =================================================================================================
FaderColumn::FaderColumn(Apvts& state, const juce::String& parameterId, const juce::String& name, Tile& t,
                         int height, bool detent, juce::String captionTop, juce::String captionBottom, bool hueDot)
    : label(name, t.getHue(), hueDot),
      fader(state, parameterId, &t, juce::Slider::LinearVertical),
      tile(t),
      faderHeight(height),
      top(std::move(captionTop)),
      bottom(std::move(captionBottom))
{
    fader.getProperties().set("detent", detent);
    format = percentText;
    addAndMakeVisible(fader);
    addAndMakeVisible(label);
}

void FaderColumn::resized()
{
    auto area = getLocalBounds();
    if (top.isNotEmpty())
        area.removeFromTop(10 + 6);
    fader.setBounds(area.removeFromTop(faderHeight).withSizeKeepingCentre(44, faderHeight));
    if (bottom.isNotEmpty())
        area.removeFromTop(6 + 10);
    area.removeFromTop(8);
    label.setBounds(area.removeFromTop(16).withSizeKeepingCentre(std::min(area.getWidth(), label.getPreferredWidth() + 12), 16));
}

void FaderColumn::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    if (top.isNotEmpty())
        drawCaption(g, area.removeFromTop(10), top);
    if (bottom.isNotEmpty())
        drawCaption(g, fader.getBounds().translated(0, fader.getHeight() + 6).withHeight(10).withX(0).withWidth(getWidth()), bottom);
    if (format && tile.isHot())
        drawValue(g, getLocalBounds().removeFromBottom(12), format(static_cast<float>(fader.valueToProportionOfLength(fader.getValue()))));
}

// =================================================================================================
//  CaptionedChoice
// =================================================================================================
CaptionedChoice::CaptionedChoice(Apvts& state, const juce::String& parameterId, const juce::String& text)
    : caption(text)
{
    auto* parameter = state.getParameter(parameterId);
    box.addItemList(parameter->getAllValueStrings(), 1);
    box.setJustificationType(juce::Justification::centred);
    box.setWantsKeyboardFocus(false); // the computer keyboard plays notes
    box.setTitle(parameter->getName(64));
    attachment = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box);
    addAndMakeVisible(box);
}

void CaptionedChoice::resized()
{
    box.setBounds(getLocalBounds().removeFromTop(31));
}

void CaptionedChoice::paint(juce::Graphics& g)
{
    drawCaption(g, getLocalBounds().removeFromBottom(11), caption);
}

// =================================================================================================
//  Toggles and buttons
// =================================================================================================
ParamToggle::ParamToggle(Apvts& state, const juce::String& parameterId, const juce::String& text, const char* style)
    : juce::ToggleButton(text),
      attachment(state, parameterId, *this)
{
    getProperties().set("style", style);
    setWantsKeyboardFocus(false);
}

CheckToggle::CheckToggle(const juce::String& text, const char* style) : juce::ToggleButton(text)
{
    getProperties().set("style", style);
    setWantsKeyboardFocus(false);
}

MenuButton::MenuButton() : juce::Button("Menu")
{
    setWantsKeyboardFocus(false);
    setTooltip("Menu: library, tuning, touch, MIDI, credits");
}

void MenuButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(colours::tile);
    g.fillRoundedRectangle(area, 9.0f);
    g.setColour(highlighted || down || getToggleState() ? colours::fg : colours::rule);
    g.drawRoundedRectangle(area, 9.0f, 1.5f);
    g.setColour(colours::fg);
    const auto c = area.getCentre();
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle(juce::Rectangle<float>(13.0f, 1.6f).withCentre(c.translated(0.0f, static_cast<float>(i) * 4.5f)), 0.8f);
}

// =================================================================================================
//  LibraryChip
// =================================================================================================
void LibraryChip::setText(const juce::String& newText)
{
    if (newText != text)
    {
        text = newText;
        repaint();
    }
}

void LibraryChip::setMidiLight(float level)
{
    if (std::abs(level - light) > 0.01f)
    {
        light = level;
        repaint();
    }
}

int LibraryChip::getPreferredWidth() const
{
    return juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(9.5f, 0.18f), text.toUpperCase())) + 22 + 18;
}

void LibraryChip::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(colours::line);
    g.drawRoundedRectangle(area, area.getHeight() * 0.5f, 1.5f);

    // The MIDI light: dim red, flashing on every MIDI message.
    const auto lamp = juce::Rectangle<float>(6.0f, 6.0f).withCentre({ area.getX() + 13.0f, area.getCentreY() });
    if (light > 0.05f)
    {
        g.setColour(colours::red.withAlpha(0.35f * light));
        g.fillEllipse(lamp.expanded(4.0f));
    }
    g.setColour(colours::red.withAlpha(0.3f + 0.7f * light));
    g.fillEllipse(lamp);

    drawCaps(g, text, area.withTrimmedLeft(22.0f).withTrimmedRight(10.0f), labelFont(9.5f, 0.18f), colours::fgDim,
             juce::Justification::centredLeft);
}

// =================================================================================================
//  PresetBox
// =================================================================================================
PresetBox::PresetBox()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle("Preset");
}

void PresetBox::setName(const juce::String& newName)
{
    if (newName != name)
    {
        name = newName;
        repaint();
    }
}

void PresetBox::resized()
{
    auto area = getLocalBounds();
    previousArea = area.removeFromLeft(32);
    nextArea = area.removeFromRight(32);
    nameArea = area;
}

void PresetBox::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(colours::tile);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(colours::rule);
    g.drawRoundedRectangle(area, 10.0f, 1.5f);

    const auto mouse = getMouseXYRelative();
    const bool over = isMouseOver();
    for (const auto& [r, left] : { std::pair { previousArea, true }, std::pair { nextArea, false } })
    {
        const auto c = r.toFloat().getCentre();
        juce::Path arrow;
        if (left)
            arrow.addTriangle(c.x + 3.0f, c.y - 4.5f, c.x + 3.0f, c.y + 4.5f, c.x - 4.0f, c.y);
        else
            arrow.addTriangle(c.x - 3.0f, c.y - 4.5f, c.x - 3.0f, c.y + 4.5f, c.x + 4.0f, c.y);
        g.setColour(over && r.contains(mouse) ? colours::red : colours::fg);
        g.fillPath(arrow);
    }
    g.setColour(colours::fg);
    g.setFont(valueFont(14.0f, 0.02f));
    g.drawText(name, nameArea, juce::Justification::centred, true);
}

void PresetBox::mouseUp(const juce::MouseEvent& e)
{
    if (!getLocalBounds().contains(e.getPosition()))
        return;
    if (previousArea.contains(e.getPosition()) && onPrevious)
        onPrevious();
    else if (nextArea.contains(e.getPosition()) && onNext)
        onNext();
    else if (onOpenList)
        onOpenList();
}

// =================================================================================================
//  TitleView and CaptionView
// =================================================================================================
TitleView::TitleView()
{
    setInterceptsMouseClicks(false, false);
    setBufferedToImage(true); // the glow is a blur: render it once, not every frame
    setTitle("Keys");
}

void TitleView::resized()
{
    juce::GlyphArrangement arrangement;
    arrangement.addLineOfText(titleFont(34.0f), "KEYS", 0.0f, 0.0f);
    glyphs.clear();
    arrangement.createPath(glyphs);
    const auto b = glyphs.getBounds();
    glyphs.applyTransform(juce::AffineTransform::translation(static_cast<float>(getWidth()) * 0.5f - b.getCentreX(),
                                                             static_cast<float>(getHeight()) * 0.5f - b.getCentreY()));
}

void TitleView::paint(juce::Graphics& g)
{
    juce::DropShadow(colours::red.withAlpha(0.35f), 24, {}).drawForPath(g, glyphs); // text-shadow: 0 0 24px
    g.setColour(colours::red);
    g.fillPath(glyphs);
}

CaptionView::CaptionView(juce::String t, juce::Font f, juce::Colour c) : text(std::move(t)), font(std::move(f)), colour(c)
{
    setInterceptsMouseClicks(false, false);
}

void CaptionView::paint(juce::Graphics& g)
{
    drawCaps(g, text, getLocalBounds().toFloat(), font, colour);
}
} // namespace keys::ui
