#include "PluginEditor.h"

#include "ui/Backdrop.h"

#if JucePlugin_Build_Standalone
    #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace colours = keys::ui::colours;
namespace params = keys::params;

namespace
{
// [C++] JUCE reads a plain "..." literal as ASCII, so non-ASCII text (the dot, the dash) is spelled as UTF-8 bytes.
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");
const juce::String dash = juce::String::fromUTF8("\xe2\x80\x94");

juce::String noteName(int note)
{
    return note < 0 ? dash : juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}

void setupRotary(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity(220);
}

void setupVertical(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}
} // namespace

KeysEditor::KeysEditor(KeysProcessor& p)
    : AudioProcessorEditor(p),
      keysProcessor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    setupVertical(blend);
    blend.getProperties().set("detent", true);
    setupVertical(level);
    setupRotary(touch);
    setupRotary(tune);
    tune.getProperties().set("bipolar", true);

    bellVoice.addItemList(params::bellVoiceChoices(), 1);
    bellOctave.addItemList(params::bellOctaveChoices(), 1);
    for (auto* combo : { &bellVoice, &bellOctave })
        combo->setJustificationType(juce::Justification::centred);

    for (auto* slider : { &blend, &level, &touch, &tune })
    {
        slider->onValueChange = [this] { repaint(); };
        addAndMakeVisible(*slider);
    }
    addAndMakeVisible(bellVoice);
    addAndMakeVisible(bellOctave);

    keyboard.setAvailableRange(28, 100); // E1–E7: the 73 keys of a Mk II
    keyboard.setOctaveForMiddleC(4);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffcdd4da));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff0d1015));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff06090f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::red.withAlpha(0.2f));
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::red.withAlpha(0.8f));
    keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(0xff56606a));
    keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::night);
    keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, colours::fg);
    addAndMakeVisible(keyboard);

    auto& state = p.getState();
    blendAttachment = std::make_unique<Apvts::SliderAttachment>(state, params::id::blend, blend);
    levelAttachment = std::make_unique<Apvts::SliderAttachment>(state, params::id::volume, level);
    touchAttachment = std::make_unique<Apvts::SliderAttachment>(state, params::id::touch, touch);
    tuneAttachment = std::make_unique<Apvts::SliderAttachment>(state, params::id::tune, tune);
    bellVoiceAttachment = std::make_unique<Apvts::ComboBoxAttachment>(state, params::id::bellVoice, bellVoice);
    bellOctaveAttachment = std::make_unique<Apvts::ComboBoxAttachment>(state, params::id::bellOctave, bellOctave);

    blend.setDoubleClickReturnValue(true, 0.35);
    level.setDoubleClickReturnValue(true, 0.0);
    touch.setDoubleClickReturnValue(true, 0.5);
    tune.setDoubleClickReturnValue(true, 0.0);

    enableAllMidiInputs();
    setSize(kWidth, kHeight);
    startTimerHz(30);
}

KeysEditor::~KeysEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KeysEditor::enableAllMidiInputs()
{
#if JucePlugin_Build_Standalone
    // Standalone only: switch on every connected MIDI input (the settings dialog can still turn
    // them off), and remember their names for the header.
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        auto& devices = holder->deviceManager;
        juce::StringArray names;
        for (const auto& input : juce::MidiInput::getAvailableDevices())
        {
            if (!devices.isMidiInputDeviceEnabled(input.identifier))
                devices.setMidiInputDeviceEnabled(input.identifier, true);
            names.add(input.name);
        }
        midiInputText = names.isEmpty() ? "No MIDI input found" : names.joinIntoString(", ");
        return;
    }
#endif
    midiInputText = "MIDI from the host";
}

juce::String KeysEditor::parameterText(const char* id) const
{
    if (auto* parameter = keysProcessor.getState().getParameter(id))
        return parameter->getCurrentValueAsText() + (parameter->getLabel().isEmpty() ? "" : " " + parameter->getLabel());
    return {};
}

void KeysEditor::timerCallback()
{
    auto& status = keysProcessor.getStatus();

    const auto library = keysProcessor.getLibrary().describe();
    if (library != libraryText)
    {
        libraryText = library;
        repaint(chipArea);
    }

    const auto count = status.midiEventCount.load(std::memory_order_relaxed);
    if (count != lastEventCount)
    {
        lastEventCount = count;
        midiLamp = 1.0f;
    }
    else
    {
        midiLamp *= 0.85f;
    }

    meter = std::max(status.outputPeak.load(std::memory_order_relaxed), meter * 0.88f);

    if (++timerTicks % 60 == 0) // every 2 s: pick up keyboards plugged in while running
        enableAllMidiInputs();

    repaint(midiArea);
    repaint(monitorArea);
}

void KeysEditor::resized()
{
    backdrop = keys::ui::renderBackdrop(getWidth(), getHeight(), juce::jlimit(1.0f, 2.0f, juce::Component::getApproximateScaleFactorForComponent(this)));

    auto area = getLocalBounds().reduced(22, 0);
    auto header = area.removeFromTop(56).withTrimmedTop(14);
    chipArea = header.removeFromLeft(320).withHeight(32);
    midiArea = header.removeFromRight(400).withHeight(32);

    titleArea = area.removeFromTop(48);
    area.removeFromTop(10);

    auto tiles = area.removeFromTop(236);
    const int gap = 14;
    const int total = tiles.getWidth() - gap * 4;
    auto take = [&](float share)
    {
        auto r = tiles.removeFromLeft(juce::roundToInt(static_cast<float>(total) * share));
        tiles.removeFromLeft(gap);
        return r;
    };
    blendTile = take(0.14f);
    bellTile = take(0.34f);
    touchTile = take(0.19f);
    tuneTile = take(0.19f);
    levelTile = tiles;

    // Controls inside the tiles (captions and values are painted).
    blend.setBounds(blendTile.withSizeKeepingCentre(48, 110).translated(0, -16));
    level.setBounds(levelTile.withSizeKeepingCentre(48, 140).translated(0, -12));
    touch.setBounds(touchTile.withSizeKeepingCentre(92, 92).translated(0, -18));
    tune.setBounds(tuneTile.withSizeKeepingCentre(92, 92).translated(0, -18));
    auto combos = bellTile.withSizeKeepingCentre(bellTile.getWidth() - 40, 40).translated(0, 4);
    bellVoice.setBounds(combos.removeFromLeft(combos.getWidth() / 2).reduced(8, 0));
    bellOctave.setBounds(combos.reduced(8, 0));

    area.removeFromTop(14);
    monitorArea = area.removeFromTop(112);
    area.removeFromTop(14);

    auto keysArea = area.removeFromTop(108);
    keyboard.setBounds(keysArea);
    keyboard.setKeyWidth(static_cast<float>(keysArea.getWidth()) / 43.0f); // 43 white keys from E1 to E7
}

void KeysEditor::drawTile(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(colours::tile);
    g.fillRoundedRectangle(area.toFloat(), 18.0f);
}

void KeysEditor::paint(juce::Graphics& g)
{
    g.drawImage(backdrop, getLocalBounds().toFloat());

    // Header chips.
    auto drawChip = [&g](juce::Rectangle<int> r, const juce::String& text, juce::Colour textColour, int leftPad = 14)
    {
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.toFloat().reduced(0.75f), 16.0f, 1.5f);
        g.setColour(textColour);
        g.setFont(keys::ui::labelFont(10.5f));
        g.drawFittedText(text.toUpperCase(), r.withTrimmedLeft(leftPad).withTrimmedRight(14), juce::Justification::centredLeft, 1);
    };
    drawChip(chipArea, libraryText, colours::fgDim);

    {
        const auto lamp = midiArea.withWidth(32).toFloat();
        drawChip(midiArea, "MIDI in" + dot + midiInputText, colours::fg, 34);
        const auto light = juce::Rectangle<float>(9.0f, 9.0f).withCentre(lamp.getCentre().translated(6.0f, 0.0f));
        g.setColour(colours::red.withAlpha(0.25f + 0.75f * midiLamp));
        g.fillEllipse(light);
        if (midiLamp > 0.05f)
        {
            g.setColour(colours::red.withAlpha(0.35f * midiLamp));
            g.fillEllipse(light.expanded(5.0f));
        }
    }

    // Title.
    g.setColour(colours::red);
    g.setFont(keys::ui::titleFont(32.0f));
    g.drawText("KEYS", titleArea.translated(10, 0), juce::Justification::centred);

    // Tiles, captions and values.
    for (const auto& tile : { blendTile, bellTile, touchTile, tuneTile, levelTile })
        drawTile(g, tile);

    auto caption = [&g](juce::Rectangle<int> r, const juce::String& text, float size = 9.0f)
    {
        g.setColour(colours::fgDim);
        g.setFont(keys::ui::labelFont(size));
        g.drawText(text.toUpperCase(), r, juce::Justification::centred);
    };
    auto label = [&g](juce::Rectangle<int> tile, const juce::String& text, const juce::String& value)
    {
        g.setColour(colours::fg);
        g.setFont(keys::ui::labelFont(12.5f));
        g.drawText(text.toUpperCase(), tile.withTrimmedTop(tile.getHeight() - 52).withHeight(18), juce::Justification::centred);
        g.setColour(colours::fgDim);
        g.setFont(keys::ui::valueFont(11.0f));
        g.drawText(value, tile.withTrimmedTop(tile.getHeight() - 32).withHeight(16), juce::Justification::centred);
    };

    caption(blendTile.withHeight(30).translated(0, 8), "Bell");
    caption(blend.getBounds().withY(blend.getBottom() + 2).withHeight(14).withX(blendTile.getX()).withWidth(blendTile.getWidth()), "Tine");
    const float b = static_cast<float>(blend.getValue());
    label(blendTile, "Blend", b < 0.49f ? "tine" + dot + "bell " + juce::String(juce::roundToInt(b * 200)) + "%"
                              : b > 0.51f ? "bell" + dot + "tine " + juce::String(juce::roundToInt((1.0f - b) * 200)) + "%"
                                          : "both at full");

    g.setColour(colours::fg);
    g.setFont(keys::ui::labelFont(12.5f));
    g.drawText("BELL", bellTile.withHeight(40).translated(0, 22), juce::Justification::centred);
    caption(bellVoice.getBounds().translated(0, bellVoice.getHeight() + 2).withHeight(16), "Voice");
    caption(bellOctave.getBounds().translated(0, bellOctave.getHeight() + 2).withHeight(16), "Octave");
    caption(bellTile.withTrimmedTop(bellTile.getHeight() - 36).withHeight(16), "Celeste (CC0)" + dot + "Vibraphone (CC0)", 8.0f);

    label(touchTile, "Touch", touch.getValue() < 0.35 ? "even" : touch.getValue() > 0.65 ? "dynamic" : "natural");
    label(tuneTile, "Tune", parameterText(params::id::tune));
    label(levelTile, "Level", parameterText(params::id::volume));

    drawTile(g, monitorArea);
    drawMonitor(g, monitorArea);
}

void KeysEditor::drawMonitor(juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto& status = keysProcessor.getStatus();
    auto inner = area.reduced(20, 14);

    // Left: the last note, big, with its velocity.
    auto left = inner.removeFromLeft(200);
    g.setColour(colours::fgDim);
    g.setFont(keys::ui::labelFont(9.0f));
    g.drawText("LAST NOTE", left.removeFromTop(14), juce::Justification::centredLeft);
    const int note = status.lastNote.load(std::memory_order_relaxed);
    const int velocity = status.lastVelocity.load(std::memory_order_relaxed);
    g.setColour(colours::fg);
    g.setFont(keys::ui::titleFont(30.0f).withExtraKerningFactor(0.05f));
    g.drawText(noteName(note), left.removeFromTop(44), juce::Justification::centredLeft);
    g.setColour(colours::fgDim);
    g.setFont(keys::ui::valueFont(11.0f));
    g.drawText("VELOCITY " + (note < 0 ? dash : juce::String(velocity)), left.removeFromTop(16), juce::Justification::centredLeft);
    auto bar = left.removeFromTop(10).withWidth(160).toFloat().withHeight(4.0f).translated(0.0f, 4.0f);
    g.setColour(colours::line);
    g.fillRect(bar);
    g.setColour(colours::red);
    g.fillRect(bar.withWidth(bar.getWidth() * static_cast<float>(velocity) / 127.0f));

    // Right: pedal, voices, output level.
    auto right = inner.removeFromRight(170);
    auto row = [&](const juce::String& name, const juce::String& value, bool highlight)
    {
        auto r = right.removeFromTop(26);
        g.setColour(colours::fgDim);
        g.setFont(keys::ui::labelFont(9.0f));
        g.drawText(name, r.removeFromLeft(70), juce::Justification::centredLeft);
        g.setColour(highlight ? colours::red : colours::fg);
        g.setFont(keys::ui::valueFont(12.0f));
        g.drawText(value, r, juce::Justification::centredLeft);
    };
    const bool pedal = status.sustainPedal.load(std::memory_order_relaxed);
    row("PEDAL", pedal ? "DOWN" : "up", pedal);
    row("VOICES", juce::String(status.activeVoices.load(std::memory_order_relaxed)), false);
    {
        auto r = right.removeFromTop(26);
        g.setColour(colours::fgDim);
        g.setFont(keys::ui::labelFont(9.0f));
        g.drawText("OUTPUT", r.removeFromLeft(70), juce::Justification::centredLeft);
        auto m = r.withHeight(4).withY(r.getCentreY() - 2).toFloat();
        g.setColour(colours::line);
        g.fillRect(m);
        const float db = juce::Decibels::gainToDecibels(meter, -60.0f);
        g.setColour(meter > 0.97f ? colours::red : colours::fg);
        g.fillRect(m.withWidth(m.getWidth() * juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f)));
    }

    // Middle: the latest MIDI events.
    inner.removeFromLeft(10);
    inner.removeFromRight(20);
    g.setColour(colours::fgDim);
    g.setFont(keys::ui::labelFont(9.0f));
    g.drawText("MIDI EVENTS", inner.removeFromTop(14), juce::Justification::centredLeft);
    const uint32_t written = status.historyWrite.load(std::memory_order_acquire);
    const int shown = static_cast<int>(std::min<uint32_t>(written, 4));
    if (shown == 0)
    {
        g.setColour(colours::fgDim);
        g.setFont(keys::ui::valueFont(12.0f));
        g.drawText("Play your keyboard, or the keys below.", inner.removeFromTop(20), juce::Justification::centredLeft);
    }
    for (int k = 0; k < shown; ++k)
    {
        const auto packed = status.history[(written - 1 - static_cast<uint32_t>(k)) % keys::EngineStatus::kHistory].load(std::memory_order_relaxed);
        const auto e = keys::EngineStatus::unpack(packed);
        juce::String text;
        switch (e.type)
        {
            case keys::MidiEventInfo::Type::noteOn: text << "NOTE ON    " << noteName(e.number) << "    vel " << static_cast<int>(e.value); break;
            case keys::MidiEventInfo::Type::noteOff: text << "NOTE OFF   " << noteName(e.number); break;
            case keys::MidiEventInfo::Type::controller:
                text << "CC " << static_cast<int>(e.number) << (e.number == 64 ? " (sustain)" : "") << "    " << static_cast<int>(e.value);
                break;
            case keys::MidiEventInfo::Type::other: text << "other"; break;
        }
        text << "    ch " << static_cast<int>(e.channel);
        g.setColour(colours::fg.withAlpha(1.0f - 0.16f * static_cast<float>(k)));
        g.setFont(keys::ui::valueFont(12.0f));
        g.drawText(text, inner.removeFromTop(16), juce::Justification::centredLeft);
    }
}
