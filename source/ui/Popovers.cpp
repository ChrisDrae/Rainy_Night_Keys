#include "Popovers.h"

#include "Parameters.h"
#include "engine/KeysEngine.h"

namespace keys::ui
{
namespace
{
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");

void drawHeading(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    drawCaps(g, text, area.toFloat(), labelFont(9.5f, 0.2f), colours::fgDim, juce::Justification::centredLeft);
}

juce::String noteName(int note)
{
    return note < 0 ? juce::String::fromUTF8("\xe2\x80\x94") : juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}
} // namespace

/// The ✕ in a popover's corner.
class CloseButton final : public juce::Button
{
public:
    CloseButton() : juce::Button("Close") { setWantsKeyboardFocus(false); }
    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        if (highlighted)
        {
            g.setColour(colours::fg.withAlpha(0.08f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        }
        g.setColour(highlighted ? colours::fg : colours::fgDim);
        g.drawLine(c.x - 4.0f, c.y - 4.0f, c.x + 4.0f, c.y + 4.0f, 1.4f);
        g.drawLine(c.x - 4.0f, c.y + 4.0f, c.x + 4.0f, c.y - 4.0f, 1.4f);
    }
};

// =================================================================================================
//  Popover
// =================================================================================================
Popover::Popover()
{
    juce::Desktop::getInstance().addGlobalMouseListener(&outsideClick);
}

Popover::~Popover()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(&outsideClick);
}

void Popover::OutsideClick::mouseDown(const juce::MouseEvent& e)
{
    auto* clicked = e.eventComponent;
    if (clicked == nullptr || !owner.isShowing())
        return;
    // Clicks in other windows (a dropdown's list, a dialog) belong to them, not to "outside".
    if (clicked->getTopLevelComponent() != owner.getTopLevelComponent())
        return;
    if (clicked == &owner || owner.isParentOf(clicked))
        return;
    if (auto* opener = owner.openerComponent.getComponent(); opener != nullptr && (clicked == opener || opener->isParentOf(clicked)))
        return;
    owner.dismiss();
}

void Popover::dismiss()
{
    juce::Component::SafePointer<Popover> safe(this);
    juce::MessageManager::callAsync([safe]
    {
        if (safe != nullptr && safe->onDismiss)
            safe->onDismiss();
    });
}

void Popover::paint(juce::Graphics& g)
{
    const auto panel = getPanel().toFloat();
    // box-shadow: 0 24px 48px rgba(0, 0, 0, .7), approximated with a few soft layers.
    for (int i = 5; i >= 1; --i)
    {
        const float spread = static_cast<float>(i) * 3.0f;
        g.setColour(juce::Colours::black.withAlpha(0.1f));
        g.fillRoundedRectangle(panel.translated(0.0f, static_cast<float>(i) * 1.6f).expanded(spread), 14.0f + spread);
    }
    g.setColour(colours::pop);
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(colours::line);
    g.drawRoundedRectangle(panel.reduced(0.5f), 14.0f, 1.0f);
}

// =================================================================================================
//  FineSlider
// =================================================================================================
FineSlider::FineSlider(Apvts& state, const juce::String& parameterId, const juce::String& text)
    : slider(state, parameterId, nullptr, juce::Slider::LinearHorizontal),
      caption(text)
{
    format = [this]
    {
        const auto& p = slider.getParameter();
        const auto label = p.getLabel();
        return p.getCurrentValueAsText() + (label.isEmpty() ? juce::String() : " " + label);
    };
    addAndMakeVisible(slider);
}

void FineSlider::resized()
{
    slider.setBounds(getLocalBounds().removeFromBottom(22));
}

void FineSlider::paint(juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop(12).toFloat();
    drawCaps(g, caption, top, labelFont(9.5f, 0.18f), colours::fgDim, juce::Justification::centredLeft);
    g.setColour(colours::fg);
    g.setFont(valueFont(10.5f, 0.0f));
    g.drawText(format(), top, juce::Justification::centredRight, false);
}

// =================================================================================================
//  FinePanel
// =================================================================================================
FinePanel::FinePanel(Apvts& state, const juce::String& k) : key(k)
{
    using namespace params;
    struct Row
    {
        const char* id;
        const char* caption;
        bool choice = false;
    };
    std::vector<Row> spec;
    if (key == "warmth")
    {
        title = "Warmth";
        spec = { { id::warmthDrive, "Drive" }, { id::warmthTilt, "Tilt" }, { id::warmthChar, "Character", true } };
    }
    else if (key == "bark")
    {
        title = "Bark";
        spec = { { id::barkVel, "Velocity to bark" }, { id::barkDing, "Tine ding" } };
    }
    else if (key == "body")
    {
        title = "Body";
        spec = { { id::bodyRes, "Resonance" }, { id::bodyDouble, "Double" }, { id::bodyWidth, "Width" } };
    }
    else
    {
        title = "Space";
        spec = { { id::spacePre, "Pre-delay" }, { id::spaceDecay, "Decay" }, { id::spaceTone, "Tone" } };
    }

    for (const auto& row : spec)
    {
        if (row.choice)
            rows.push_back(std::make_unique<CaptionedChoice>(state, row.id, row.caption));
        else
            rows.push_back(std::make_unique<FineSlider>(state, row.id, row.caption));
        addAndMakeVisible(*rows.back());
    }
    setTitle(title + " fine controls");

    auto closeButton = std::make_unique<CloseButton>();
    closeButton->onClick = [this] { dismiss(); };
    addAndMakeVisible(*closeButton);
    closer = std::move(closeButton);
}

int FinePanel::getPreferredHeight() const
{
    int height = 2 * kMargin + 14 + 26 + 14;
    for (const auto& row : rows)
        height += 12 + (dynamic_cast<CaptionedChoice*>(row.get()) != nullptr ? CaptionedChoice::kHeight : FineSlider::kHeight);
    return height;
}

void FinePanel::resized()
{
    auto area = getPanel().reduced(14);
    auto head = area.removeFromTop(26);
    closer->setBounds(head.removeFromRight(26));
    for (const auto& row : rows)
    {
        area.removeFromTop(12);
        const bool choice = dynamic_cast<CaptionedChoice*>(row.get()) != nullptr;
        row->setBounds(area.removeFromTop(choice ? CaptionedChoice::kHeight : FineSlider::kHeight));
    }
}

void FinePanel::paint(juce::Graphics& g)
{
    Popover::paint(g);
    auto head = getPanel().reduced(14).removeFromTop(26).toFloat();
    drawCaps(g, title, head, labelFont(12.0f, 0.12f), colours::fg, juce::Justification::centredLeft);
}

// =================================================================================================
//  MenuPanel
// =================================================================================================
MenuPanel::MenuPanel(Apvts& state, const keys::EngineStatus& engineStatus)
    : status(engineStatus),
      tune(state, params::id::tune, "Tune"),
      touch(state, params::id::touch, "Touch")
{
    setTitle("Menu");
    touch.format = [this]
    {
        const float v = static_cast<float>(touch.slider.getValue());
        return juce::String(v < 0.35f ? "even" : v > 0.65f ? "dynamic" : "natural");
    };
    for (auto* choice : { &sampledTines, &modeledTines })
    {
        choice->setRadioGroupId(1);
        addAndMakeVisible(*choice);
    }
    addAndMakeVisible(tune);
    addAndMakeVisible(touch);
    addAndMakeVisible(still);
}

void MenuPanel::resized()
{
    auto area = getPanel().reduced(14);
    librarySection = area.removeFromTop(84);
    auto choices = librarySection.withTrimmedTop(12 + 8).withHeight(22);
    sampledTines.setBounds(choices.removeFromLeft(178));
    modeledTines.setBounds(choices);
    area.removeFromTop(14);
    tune.setBounds(area.removeFromTop(FineSlider::kHeight));
    area.removeFromTop(12);
    touch.setBounds(area.removeFromTop(FineSlider::kHeight));
    area.removeFromTop(14);
    still.setBounds(area.removeFromTop(22).withWidth(200));
    area.removeFromTop(16);
    midiSection = area.removeFromTop(132);
    area.removeFromTop(14);
    creditsSection = area;
}

void MenuPanel::paint(juce::Graphics& g)
{
    Popover::paint(g);
    const auto soft = valueFont(12.5f);

    // Library: which tines (the toggles), what's loaded, where the samples come from.
    {
        auto area = librarySection;
        drawHeading(g, area.removeFromTop(12), "Library");
        area.removeFromTop(8 + 22 + 6);
        g.setFont(soft);
        g.setColour(colours::fgSoft);
        g.drawText(libraryText ? libraryText() : juce::String("Demo tines"), area.removeFromTop(18),
                   juce::Justification::centredLeft, true);
        area.removeFromTop(2);
        g.setColour(colours::fgDim);
        g.setFont(valueFont(11.0f));
        g.drawText(libraryNote ? libraryNote() : juce::String(), area.removeFromTop(16), juce::Justification::centredLeft, true);
    }

    // MIDI: the inputs, the latest events, pedal and voices. (Your keyboard's test bench.)
    {
        auto area = midiSection;
        auto head = area.removeFromTop(12);
        drawHeading(g, head, "MIDI");
        area.removeFromTop(6);
        g.setFont(valueFont(11.5f));
        g.setColour(colours::fgSoft);
        g.drawFittedText(midiInputText ? midiInputText() : juce::String(), area.removeFromTop(30), juce::Justification::topLeft, 2, 1.0f);

        const uint32_t written = status.historyWrite.load(std::memory_order_acquire);
        const int shown = static_cast<int>(std::min<uint32_t>(written, 4));
        g.setFont(valueFont(11.5f, 0.0f));
        if (shown == 0)
        {
            g.setColour(colours::fgDim);
            g.drawText("Play your keyboard, or the keys below.", area.removeFromTop(16), juce::Justification::centredLeft, false);
        }
        for (int k = 0; k < shown; ++k)
        {
            const auto packed = status.history[(written - 1 - static_cast<uint32_t>(k)) % keys::EngineStatus::kHistory].load(std::memory_order_relaxed);
            const auto e = keys::EngineStatus::unpack(packed);
            juce::String text;
            switch (e.type)
            {
                case keys::MidiEventInfo::Type::noteOn: text << "NOTE ON   " << noteName(e.number) << "   vel " << static_cast<int>(e.value); break;
                case keys::MidiEventInfo::Type::noteOff: text << "NOTE OFF  " << noteName(e.number); break;
                case keys::MidiEventInfo::Type::controller:
                    text << "CC " << static_cast<int>(e.number) << (e.number == 64 ? " (sustain)" : "") << "   " << static_cast<int>(e.value);
                    break;
                case keys::MidiEventInfo::Type::other: text << "other"; break;
            }
            text << "   ch " << static_cast<int>(e.channel);
            g.setColour(colours::fg.withAlpha(1.0f - 0.18f * static_cast<float>(k)));
            g.drawText(text, area.removeFromTop(16), juce::Justification::centredLeft, false);
        }
        const bool pedal = status.sustainPedal.load(std::memory_order_relaxed);
        g.setColour(pedal ? colours::red : colours::fgDim);
        g.drawText(juce::String(pedal ? "Pedal down" : "Pedal up") + dot + juce::String(status.activeVoices.load()) + " voices",
                   midiSection.withTop(midiSection.getBottom() - 16), juce::Justification::centredLeft, false);
    }

    // Credits
    {
        auto area = creditsSection;
        drawHeading(g, area.removeFromTop(12), "Credits");
        area.removeFromTop(6);
        g.setColour(colours::fgDim);
        g.setFont(valueFont(11.5f));
        g.drawFittedText("Rhodes samples: jRhodes3d by Jeff Learman (CC BY-NC 4.0). Celeste by stamperadam on "
                         "freesound.org, via Sonatina Symphonic Orchestra (CC0). Vibraphone from the Versilian "
                         "Community Sample Library (CC0). Archivo type by Omnibus-Type (SIL OFL).",
                         area, juce::Justification::topLeft, 7, 1.0f);
    }
}
} // namespace keys::ui
