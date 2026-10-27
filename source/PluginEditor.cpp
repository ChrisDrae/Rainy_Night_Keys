#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
    #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace colours = keys::ui::colours;
namespace params = keys::params;
using namespace keys::ui;

namespace
{
// [C++] JUCE reads a plain "..." literal as ASCII, so non-ASCII text (the dot) is spelled as UTF-8 bytes.
const juce::String dot = juce::String::fromUTF8(" \xc2\xb7 ");

const juce::Identifier keysVisibleProperty { "keysVisible" };
const juce::Identifier stillProperty { "stillBackground" };
const juce::Identifier scaleProperty { "uiScale" };

/// The computer keys, from C upwards: the home row is white keys, the row above is black keys.
constexpr char kComputerKeys[] = "awsedftgyhujkolp;";

int keyCodeFor(char c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

/// Blend's value line: which layer leads, or "both at full" in the middle.
juce::String blendText(float v)
{
    if (v < 0.49f)
        return "tine" + dot + "bell " + juce::String(juce::roundToInt(v * 200.0f)) + "%";
    if (v > 0.51f)
        return "bell" + dot + "tine " + juce::String(juce::roundToInt((1.0f - v) * 200.0f)) + "%";
    return "both at full";
}

/// `width` × `height`, centred in `area`.
juce::Rectangle<int> centred(juce::Rectangle<int> area, int width, int height)
{
    return area.withSizeKeepingCentre(std::min(width, area.getWidth()), std::min(height, area.getHeight()));
}

/// Splits `row` into columns with the given proportions (CSS grid "fr" units) and gaps.
std::vector<juce::Rectangle<int>> columns(juce::Rectangle<int> row, int gap, std::initializer_list<float> fractions)
{
    float total = 0.0f;
    for (float f : fractions)
        total += f;
    const float available = static_cast<float>(row.getWidth() - gap * (static_cast<int>(fractions.size()) - 1));
    std::vector<juce::Rectangle<int>> result;
    float x = static_cast<float>(row.getX());
    for (float f : fractions)
    {
        const float w = available * f / total;
        result.push_back(juce::Rectangle<int>(juce::roundToInt(x), row.getY(), juce::roundToInt(x + w) - juce::roundToInt(x), row.getHeight()));
        x += w + static_cast<float>(gap);
    }
    return result;
}
} // namespace

KeysEditor::KeysEditor(KeysProcessor& p)
    : AudioProcessorEditor(p),
      keysProcessor(p),
      blendTile(colours::bell),
      warmthTile(colours::warmth),
      barkTile(colours::bark),
      bodyTile(colours::body),
      levelTile(colours::fg),
      dustTile(colours::dust),
      bellTile(colours::bell),
      mechTile(colours::fg),
      patinaTile(colours::patina),
      motionTile(colours::motion),
      spaceTile(colours::space),
      signalTile(colours::fg),
      bellLabel("Bell", colours::bell),
      signalCaption("Signal", labelFont(8.5f, 0.2f), colours::fgDim),
      footerCaption("Tine & Bell", labelFont(10.0f, 0.32f), colours::fgDim),
      keysStrip(p.keyboardState)
{
    setLookAndFeel(&lookAndFeel);
    auto& state = p.getState();

    addAndMakeVisible(content);
    content.addAndMakeVisible(field);
    for (auto* c : std::initializer_list<juce::Component*> { &menuButton, &chip, &presetBox, &title })
        content.addAndMakeVisible(c);
    for (auto* tile : { &blendTile, &warmthTile, &barkTile, &bodyTile, &levelTile, &dustTile, &bellTile, &mechTile,
                        &patinaTile, &motionTile, &spaceTile, &signalTile })
        content.addAndMakeVisible(tile);

    // Row 1: Blend, the big three, Level.
    blend = std::make_unique<FaderColumn>(state, params::id::blend, "Blend", blendTile, 116, true, "Bell", "Tine");
    blend->format = blendText;
    blendTile.addAndMakeVisible(*blend);

    auto& volume = *state.getParameter(params::id::volume);
    level = std::make_unique<FaderColumn>(state, params::id::volume, "Level", levelTile, 116, false);
    level->format = [&volume](float) { return volume.getCurrentValueAsText() + " dB"; };
    levelTile.addAndMakeVisible(*level);

    warmth = std::make_unique<KnobColumn>(state, params::id::warmth, "Warmth", warmthTile, true, 116);
    bark = std::make_unique<KnobColumn>(state, params::id::bark, "Bark", barkTile, true, 116);
    body = std::make_unique<KnobColumn>(state, params::id::body, "Body", bodyTile, true, 116);
    warmthTile.addAndMakeVisible(*warmth);
    barkTile.addAndMakeVisible(*bark);
    bodyTile.addAndMakeVisible(*body);
    warmth->label.setFineButton([this] { toggleFine("warmth"); });
    bark->label.setFineButton([this] { toggleFine("bark"); });
    body->label.setFineButton([this] { toggleFine("body"); });

    // Row 2: Dust, the bell, Mechanics.
    dust = std::make_unique<KnobColumn>(state, params::id::dust, "Dust", dustTile, false, 58);
    mech = std::make_unique<KnobColumn>(state, params::id::mech, "Mechanics", mechTile, false, 58);
    dustTile.addAndMakeVisible(*dust);
    mechTile.addAndMakeVisible(*mech);

    bellVoice = std::make_unique<CaptionedChoice>(state, params::id::bellVoice, "Voice");
    bellOctave = std::make_unique<CaptionedChoice>(state, params::id::bellOctave, "Octave");
    bellRange = std::make_unique<CaptionedChoice>(state, params::id::bellRange, "From key");
    for (auto* c : std::initializer_list<juce::Component*> { &bellLabel, bellVoice.get(), bellOctave.get(), bellRange.get() })
        bellTile.addAndMakeVisible(c);

    // Row 3: Patina, Motion, Space, the signal mixer.
    patina = std::make_unique<KnobColumn>(state, params::id::patina, "Patina", patinaTile, false, 58);
    wow = std::make_unique<ParamToggle>(state, params::id::patinaWow, "Wow", "dot");
    flutter = std::make_unique<ParamToggle>(state, params::id::patinaFlutter, "Flutter", "dot");
    narrow = std::make_unique<ParamToggle>(state, params::id::patinaNarrow, "Narrow", "dot");
    for (auto* c : std::initializer_list<juce::Component*> { patina.get(), wow.get(), flutter.get(), narrow.get() })
        patinaTile.addAndMakeVisible(c);

    motion = std::make_unique<KnobColumn>(state, params::id::motion, "Motion", motionTile, false, 58);
    motionMode = std::make_unique<CaptionedChoice>(state, params::id::motionMode, "Mode");
    motionRate = std::make_unique<CaptionedChoice>(state, params::id::motionRate, "Rate");
    for (auto* c : std::initializer_list<juce::Component*> { motion.get(), motionMode.get(), motionRate.get() })
        motionTile.addAndMakeVisible(c);

    space = std::make_unique<KnobColumn>(state, params::id::space, "Space", spaceTile, false, 58);
    space->label.setFineButton([this] { toggleFine("space"); });
    spaceType = std::make_unique<CaptionedChoice>(state, params::id::spaceType, "Type");
    spaceTile.addAndMakeVisible(*space);
    spaceTile.addAndMakeVisible(*spaceType);

    const char* signalIds[] = { params::id::sigDi, params::id::sigAmp, params::id::sigRoom };
    const char* signalNames[] = { "DI", "Amp", "Room" };
    for (size_t i = 0; i < signal.size(); ++i)
    {
        signal[i] = std::make_unique<FaderColumn>(state, signalIds[i], signalNames[i], signalTile, 82, false,
                                                  juce::String(), juce::String(), false);
        signal[i]->format = nullptr;
        signalTile.addAndMakeVisible(*signal[i]);
    }
    signalTile.addAndMakeVisible(signalCaption);

    // Footer and keys.
    for (auto* c : std::initializer_list<juce::Component*> { &keysToggle, &pedalToggle, &footerCaption, &keysStrip })
        content.addAndMakeVisible(c);
    keysToggle.setTooltip("Show or hide the on-screen keys");
    pedalToggle.setTooltip("The sustain pedal (or hold Space)");
    keysToggle.onClick = [this] { setKeysVisible(keysToggle.getToggleState()); };
    pedalToggle.onClick = [this] { keysProcessor.setUiPedal(pedalToggle.getToggleState()); };

    // Header.
    menuButton.onClick = [this] { toggleMenu(); };
    presetBox.onPrevious = [this] { keysProcessor.getPresets().previous(); };
    presetBox.onNext = [this] { keysProcessor.getPresets().next(); };
    presetBox.onOpenList = [this] { showPresetList(); };
    keysProcessor.getPresets().onPresetChanged = [this] { presetBox.setName(keysProcessor.getPresets().getCurrentName()); };
    presetBox.setName(keysProcessor.getPresets().getCurrentName());

    // Window state, remembered with the session.
    const bool standalone = p.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    keysVisible = state.state.getProperty(keysVisibleProperty, standalone);
    still = state.state.getProperty(stillProperty, false);
    keysToggle.setToggleState(keysVisible, juce::dontSendNotification);
    keysStrip.setVisible(keysVisible);
    field.setStill(still);

    // The light behind everything redraws 30 times a second, and with it everything on top. Cache
    // the rest as images, so a frame only re-renders what actually changed.
    for (auto* c : std::initializer_list<juce::Component*> { &menuButton, &chip, &presetBox, &blendTile, &warmthTile,
                                                             &barkTile, &bodyTile, &levelTile, &dustTile, &bellTile,
                                                             &mechTile, &patinaTile, &motionTile, &spaceTile, &signalTile,
                                                             &keysToggle, &pedalToggle, &footerCaption, &keysStrip })
        c->setBufferedToImage(true);

    setWantsKeyboardFocus(true);
    enableAllMidiInputs();

    // Size: the remembered scale, or as large as fits the screen (up to 100 %).
    double scale = state.state.getProperty(scaleProperty, 0.0);
    if (scale <= 0.0)
    {
        scale = 1.0;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = std::min({ 1.0, (display->userArea.getHeight() - 90.0) / designHeight(),
                               (display->userArea.getWidth() - 40.0) / kDesignWidth });
    }
    scale = juce::jlimit(0.6, 1.6, scale);
    setResizable(true, true);
    updateConstraints();
    layout();
    setSize(juce::roundToInt(kDesignWidth * scale), juce::roundToInt(designHeight() * scale));

    // Notes played while the window was closed are old news: don't bloom them all at once.
    std::array<keys::NoteEvent, 64> stale;
    while (p.getStatus().notes.pop(stale.data(), static_cast<int>(stale.size())) > 0)
    {
    }

    lastFrame = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(30);
}

KeysEditor::~KeysEditor()
{
    stopTimer();
    keysProcessor.getPresets().onPresetChanged = nullptr;
    releaseComputerKeys();
    fine.reset();
    menu.reset();
    setLookAndFeel(nullptr);
}

// -------------------------------------------------------------------------------------------------
//  Layout
// -------------------------------------------------------------------------------------------------
void KeysEditor::updateConstraints()
{
    const int h = designHeight();
    if (auto* fixedAspect = getConstrainer())
        fixedAspect->setFixedAspectRatio(static_cast<double>(kDesignWidth) / h);
    setResizeLimits(juce::roundToInt(kDesignWidth * 0.6), juce::roundToInt(h * 0.6), juce::roundToInt(kDesignWidth * 1.6),
                    juce::roundToInt(h * 1.6));
}

void KeysEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth);
    content.setTransform(juce::AffineTransform::scale(scale));
    keysProcessor.getState().state.setProperty(scaleProperty, scale, nullptr);
}

void KeysEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::night);
}

void KeysEditor::layout()
{
    constexpr int W = kDesignWidth, gap = 14;
    content.setBounds(0, 0, W, designHeight());
    field.setBounds(content.getLocalBounds());

    // Header.
    menuButton.setBounds(16, 14, 36, 36);
    chip.setBounds(62, 16, std::min(chip.getPreferredWidth(), 520), 32);
    presetBox.setBounds(W - 16 - 232, 14, 232, 36);
    title.setBounds(W / 2 - 160, 40, 320, 72); // room around the letters for their glow

    const auto rows = juce::Rectangle<int>(22, 0, W - 44, 0);
    auto setTile = [](Tile& tile, juce::Rectangle<int> inner) { tile.setInnerBounds(inner); };

    // Row 1.
    {
        const auto c = columns(rows.withY(104).withHeight(236), gap, { 0.8f, 2.0f, 2.0f, 2.0f, 0.8f });
        setTile(blendTile, c[0]);
        setTile(warmthTile, c[1]);
        setTile(barkTile, c[2]);
        setTile(bodyTile, c[3]);
        setTile(levelTile, c[4]);
        blend->setBounds(centred(blendTile.getInner(), blendTile.getInner().getWidth(), 192));
        level->setBounds(centred(levelTile.getInner(), levelTile.getInner().getWidth(), 160));
        for (auto [column, tile] : { std::pair { warmth.get(), &warmthTile }, std::pair { bark.get(), &barkTile },
                                     std::pair { body.get(), &bodyTile } })
            column->setBounds(centred(tile->getInner(), tile->getInner().getWidth() - 20, column->getPreferredHeight()));
    }

    // Row 2.
    {
        const auto c = columns(rows.withY(354).withHeight(128), gap, { 1.5f, 4.3f, 1.5f });
        setTile(dustTile, c[0]);
        setTile(bellTile, c[1]);
        setTile(mechTile, c[2]);
        dust->setBounds(centred(dustTile.getInner(), dustTile.getInner().getWidth() - 20, dust->getPreferredHeight()));
        mech->setBounds(centred(mechTile.getInner(), mechTile.getInner().getWidth() - 20, mech->getPreferredHeight()));

        auto panel = centred(bellTile.getInner(), 3 * 124 + 2 * 18, 16 + 12 + CaptionedChoice::kHeight);
        bellLabel.setBounds(panel.removeFromTop(16).withSizeKeepingCentre(bellLabel.getPreferredWidth() + 12, 16));
        panel.removeFromTop(12);
        for (auto* choice : { bellVoice.get(), bellOctave.get(), bellRange.get() })
        {
            choice->setBounds(panel.removeFromLeft(124));
            panel.removeFromLeft(18);
        }
    }

    // Row 3.
    {
        const auto c = columns(rows.withY(496).withHeight(152), gap, { 1.9f, 2.0f, 1.9f, 1.6f });
        setTile(patinaTile, c[0]);
        setTile(motionTile, c[1]);
        setTile(spaceTile, c[2]);
        setTile(signalTile, c[3]);

        // A knob beside its options, the pair centred in the tile.
        auto split = [](Tile& tile, juce::Component& knob, int knobWidth, int knobHeight, int sideWidth, int sideHeight)
        {
            auto area = centred(tile.getInner(), knobWidth + 14 + sideWidth, std::max(knobHeight, sideHeight));
            knob.setBounds(centred(area.removeFromLeft(knobWidth), knobWidth, knobHeight));
            area.removeFromLeft(14);
            return centred(area, sideWidth, sideHeight);
        };

        auto dots = split(patinaTile, *patina, 92, patina->getPreferredHeight(), 112, 3 * 20 + 2 * 10);
        for (auto* toggle : { wow.get(), flutter.get(), narrow.get() })
        {
            toggle->setBounds(dots.removeFromTop(20));
            dots.removeFromTop(10);
        }

        auto choices = split(motionTile, *motion, 92, motion->getPreferredHeight(), 112, 2 * CaptionedChoice::kHeight + 8);
        motionMode->setBounds(choices.removeFromTop(CaptionedChoice::kHeight));
        choices.removeFromTop(8);
        motionRate->setBounds(choices.removeFromTop(CaptionedChoice::kHeight));

        spaceType->setBounds(split(spaceTile, *space, 104, space->getPreferredHeight(), 112, CaptionedChoice::kHeight));

        auto mixer = centred(signalTile.getInner(), 3 * 56 + 2 * 6, 82 + 8 + 16 + 10 + 11);
        auto faders = mixer.removeFromTop(82 + 8 + 16);
        for (auto& column : signal)
        {
            column->setBounds(faders.removeFromLeft(56));
            faders.removeFromLeft(6);
        }
        mixer.removeFromTop(10);
        signalCaption.setBounds(mixer.removeFromTop(11));
    }

    // Footer and keys.
    keysToggle.setBounds(24, 662, 96, 26);
    pedalToggle.setBounds(W - 24 - 92, 662, 92, 26);
    footerCaption.setBounds(W / 2 - 150, 662, 300, 26);
    keysStrip.setBounds(0, kDesignHeight, W, kKeysHeight);
}

// -------------------------------------------------------------------------------------------------
//  The animation and status timer
// -------------------------------------------------------------------------------------------------
void KeysEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = (now - lastFrame) * 0.001;
    lastFrame = now;
    animate(dt);

    auto& status = keysProcessor.getStatus();
    const auto library = keysProcessor.getLibrary().describe(keysProcessor.getSampledTines());
    chip.setText(library);
    if (chip.getWidth() != std::min(chip.getPreferredWidth(), 520))
        chip.setSize(std::min(chip.getPreferredWidth(), 520), chip.getHeight());

    const auto count = status.midiEventCount.load(std::memory_order_relaxed);
    midiLight = count != lastEventCount ? 1.0f : midiLight * 0.85f;
    lastEventCount = count;
    chip.setMidiLight(midiLight);

    pedalToggle.setToggleState(status.sustainPedal.load(std::memory_order_relaxed), juce::dontSendNotification);

    if (++ticks % 60 == 0) // every 2 s: pick up keyboards plugged in while running
        enableAllMidiInputs();

    // The standalone app plays from the computer keyboard as soon as it opens.
    if (ticks < 30 && keysProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone && isShowing()
        && !hasKeyboardFocus(true) && fine == nullptr && menu == nullptr)
        grabKeyboardFocus();
}

void KeysEditor::animate(double seconds)
{
    // Notes and the pedal, from the audio thread, become glows and sparkles.
    std::array<keys::NoteEvent, 64> events;
    for (int n; (n = keysProcessor.getStatus().notes.pop(events.data(), static_cast<int>(events.size()))) > 0;)
        for (int i = 0; i < n; ++i)
        {
            const auto& e = events[static_cast<size_t>(i)];
            switch (e.type)
            {
                case keys::NoteEvent::Type::noteOn: field.noteOn(e.note, static_cast<float>(e.velocity) / 127.0f); break;
                case keys::NoteEvent::Type::noteOff: field.noteOff(e.note); break;
                case keys::NoteEvent::Type::pedalDown: field.setPedal(true); break;
                case keys::NoteEvent::Type::pedalUp: field.setPedal(false); break;
            }
        }

    auto& state = keysProcessor.getState();
    auto value = [&state](const char* id) { return state.getRawParameterValue(id)->load(std::memory_order_relaxed); };
    ColourField::Inputs in;
    in.warmth = value(params::id::warmth);
    in.bark = value(params::id::bark);
    in.body = value(params::id::body);
    in.patina = value(params::id::patina);
    in.motion = value(params::id::motion);
    in.space = value(params::id::space);
    in.dust = value(params::id::dust);
    in.blend = value(params::id::blend);
    in.bellVoice = juce::roundToInt(value(params::id::bellVoice));
    in.bellLowestKey = params::bellRangeNote(juce::roundToInt(value(params::id::bellRange)));
    field.setInputs(in);
    field.advance(seconds);
}

void KeysEditor::enableAllMidiInputs()
{
#if JucePlugin_Build_Standalone
    // Standalone only: switch on every connected MIDI input (the settings dialog can still turn
    // them off), and remember their names for the menu.
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
        midiInputText = names.isEmpty() ? "No MIDI input found" : "In: " + names.joinIntoString(", ");
        chip.setTooltip("MIDI " + midiInputText);
        return;
    }
#endif
    midiInputText = "MIDI from the host";
    chip.setTooltip(midiInputText);
}

// -------------------------------------------------------------------------------------------------
//  Popovers
// -------------------------------------------------------------------------------------------------
void KeysEditor::toggleFine(const juce::String& key)
{
    const bool wasOpen = fine != nullptr && fine->getKey() == key;
    closeFine();
    closeMenu();
    if (wasOpen)
        return;

    struct Owner
    {
        Tile* tile;
        HueLabel* label;
    };
    const Owner owner = key == "warmth" ? Owner { &warmthTile, &warmth->label }
                        : key == "bark" ? Owner { &barkTile, &bark->label }
                        : key == "body" ? Owner { &bodyTile, &body->label }
                                        : Owner { &spaceTile, &space->label };

    fine = std::make_unique<FinePanel>(keysProcessor.getState(), key);
    fine->setBufferedToImage(true);
    fine->setOpener(owner.label);
    fine->onDismiss = [this] { closeFine(); };
    owner.label->setExpanded(true);

    // Below its tile, or above it when there's no room.
    const auto tileArea = owner.tile->getBounds().reduced(Tile::kMargin);
    const int w = FinePanel::kWidth, h = fine->getPreferredHeight() - 2 * Popover::kMargin;
    int x = tileArea.getCentreX() - w / 2, y = tileArea.getBottom() + 8;
    if (y + h > content.getHeight() - 8)
        y = tileArea.getY() - h - 8;
    x = juce::jlimit(8, content.getWidth() - w - 8, x);
    fine->setBounds(juce::Rectangle<int>(x, std::max(8, y), w, h).expanded(Popover::kMargin));
    content.addAndMakeVisible(*fine);
}

void KeysEditor::closeFine()
{
    if (fine == nullptr)
        return;
    for (auto* column : { warmth.get(), bark.get(), body.get(), space.get() })
        column->label.setExpanded(false);
    fine.reset();
}

void KeysEditor::toggleMenu()
{
    const bool wasOpen = menu != nullptr;
    closeFine();
    closeMenu();
    if (wasOpen)
        return;

    menu = std::make_unique<MenuPanel>(keysProcessor.getState(), keysProcessor.getStatus());
    menu->setBufferedToImage(true);
    menu->setOpener(&menuButton);
    menu->onDismiss = [this] { closeMenu(); };
    auto& library = keysProcessor.getLibrary();
    menu->libraryText = [this] { return keysProcessor.getLibrary().describe(keysProcessor.getSampledTines()); };
    menu->libraryNote = [&library]
    {
        return library.hasSampledTines() ? juce::String("Rhodes samples by Jeff Learman (CC BY-NC)")
                                         : juce::String("Sampled tines: run scripts/fetch-rhodes-samples.ps1");
    };
    const bool sampled = library.hasSampledTines();
    menu->sampledTines.setTooltip(sampled ? library.getSampledName() : juce::String("Run scripts/fetch-rhodes-samples.ps1"));
    menu->sampledTines.setEnabled(sampled);
    menu->sampledTines.setToggleState(sampled && keysProcessor.getSampledTines(), juce::dontSendNotification);
    menu->modeledTines.setToggleState(!menu->sampledTines.getToggleState(), juce::dontSendNotification);
    menu->sampledTines.onClick = [this] { keysProcessor.setSampledTines(menu->sampledTines.getToggleState()); };
    menu->midiInputText = [this] { return midiInputText; };
    menu->still.setToggleState(still, juce::dontSendNotification);
    menu->still.onClick = [this]
    {
        still = menu->still.getToggleState();
        field.setStill(still);
        keysProcessor.getState().state.setProperty(stillProperty, still, nullptr);
    };
    menu->setBounds(juce::Rectangle<int>(16, 58, MenuPanel::kWidth, menu->getPreferredHeight() - 2 * Popover::kMargin)
                        .expanded(Popover::kMargin));
    content.addAndMakeVisible(*menu);
    menuButton.setToggleState(true, juce::dontSendNotification);
}

void KeysEditor::closeMenu()
{
    menu.reset();
    menuButton.setToggleState(false, juce::dontSendNotification);
}

void KeysEditor::openFinePanel(const juce::String& key)
{
    if (fine == nullptr || fine->getKey() != key)
        toggleFine(key);
}

std::vector<juce::Rectangle<int>> KeysEditor::getLandmarks() const
{
    const auto scale = juce::AffineTransform::scale(static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth));
    auto inner = [](const Tile& tile) { return tile.getBounds().reduced(Tile::kMargin); };
    const juce::Rectangle<int> areas[] = {
        menuButton.getBounds(),                       // 1 menu
        chip.getBounds(),                             // 2 library and MIDI light
        presetBox.getBounds(),                        // 3 presets
        inner(blendTile),                             // 4 blend
        inner(warmthTile).getUnion(inner(bodyTile)),  // 5 warmth, bark, body
        inner(levelTile),                             // 6 level
        inner(dustTile),                              // 7 dust
        inner(bellTile),                              // 8 bell
        inner(mechTile),                              // 9 mechanics
        inner(patinaTile),                            // 10 patina
        inner(motionTile),                            // 11 motion
        inner(spaceTile),                             // 12 space
        inner(signalTile),                            // 13 signal
        keysToggle.getBounds(),                       // 14 keys on and off
        pedalToggle.getBounds(),                      // 15 pedal
        keysStrip.getBounds(),                        // 16 keyboard
    };
    std::vector<juce::Rectangle<int>> result;
    for (const auto& r : areas)
        result.push_back(r.toFloat().transformedBy(scale).getSmallestIntegerContainer());
    return result;
}

void KeysEditor::openMenu()
{
    if (menu == nullptr)
        toggleMenu();
}

// -------------------------------------------------------------------------------------------------
//  Presets
// -------------------------------------------------------------------------------------------------
void KeysEditor::showPresetList()
{
    auto& presets = keysProcessor.getPresets();
    presets.rescanUserPresets();

    juce::PopupMenu list;
    list.addSectionHeader("Factory");
    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        if (i == presets.getNumFactoryPresets())
            list.addSectionHeader("Yours");
        list.addItem(i + 1, presets.getPreset(i).name, true, i == presets.getCurrentIndex());
    }
    list.addSeparator();
    list.addItem(1000, juce::String::fromUTF8("Save preset\xe2\x80\xa6"));
    list.addItem(1001, "Open the presets folder");

    juce::Component::SafePointer<KeysEditor> safe(this);
    list.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBox).withMinimumWidth(presetBox.getWidth())
                           .withStandardItemHeight(26),
                       [safe](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           auto& manager = safe->keysProcessor.getPresets();
                           if (result == 1000)
                               safe->savePresetDialog();
                           else if (result == 1001)
                           {
                               keys::PresetManager::userFolder().createDirectory();
                               keys::PresetManager::userFolder().startAsProcess();
                           }
                           else
                               manager.load(result - 1);
                       });
}

void KeysEditor::savePresetDialog()
{
    auto* window = new juce::AlertWindow("Save preset", "Name this sound. It's saved in Documents/Keys/Presets.",
                                         juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", keysProcessor.getPresets().getCurrentName(), "Name");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<KeysEditor> safe(this);
    window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int result)
    {
        if (result == 1 && safe != nullptr)
            safe->keysProcessor.getPresets().saveUserPreset(window->getTextEditorContents("name"));
    }), true);
}

// -------------------------------------------------------------------------------------------------
//  The keys strip
// -------------------------------------------------------------------------------------------------
void KeysEditor::setKeysVisible(bool visible)
{
    keysVisible = visible;
    keysProcessor.getState().state.setProperty(keysVisibleProperty, visible, nullptr);
    keysToggle.setToggleState(visible, juce::dontSendNotification);
    keysStrip.setVisible(visible);
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignWidth);
    updateConstraints();
    layout();
    setSize(getWidth(), juce::roundToInt(static_cast<float>(designHeight()) * scale));
}

// -------------------------------------------------------------------------------------------------
//  The computer keyboard
// -------------------------------------------------------------------------------------------------
bool KeysEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        closeFine();
        closeMenu();
        return true;
    }
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false;

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (!spacePedal)
        {
            spacePedal = true;
            keysProcessor.setUiPedal(true);
        }
        return true;
    }

    const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (c == 'z' || c == 'x')
    {
        shiftComputerOctave(c == 'z' ? -1 : 1);
        return true;
    }
    for (const char* k = kComputerKeys; *k != 0; ++k)
        if (c == static_cast<juce::juce_wchar>(*k))
            return true; // the note itself starts in keyStateChanged
    return false;
}

bool KeysEditor::keyStateChanged(bool)
{
    const auto mods = juce::ModifierKeys::currentModifiers;
    const bool modified = mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown();
    bool used = false;
    for (size_t i = 0; i + 1 < sizeof(kComputerKeys); ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(keyCodeFor(kComputerKeys[i])) && !modified;
        if (down == computerDown[i])
            continue;
        computerDown[i] = down;
        if (down)
        {
            computerNote[i] = computerBase + static_cast<int>(i);
            keysProcessor.keyboardState.noteOn(1, computerNote[i], 0.72f);
        }
        else
        {
            keysProcessor.keyboardState.noteOff(1, computerNote[i], 0.0f);
        }
        used = true;
    }
    if (spacePedal && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
    {
        spacePedal = false;
        keysProcessor.setUiPedal(false);
        used = true;
    }
    return used;
}

void KeysEditor::focusLost(FocusChangeType)
{
    releaseComputerKeys();
}

void KeysEditor::releaseComputerKeys()
{
    for (size_t i = 0; i < computerDown.size(); ++i)
        if (computerDown[i])
        {
            computerDown[i] = false;
            keysProcessor.keyboardState.noteOff(1, computerNote[i], 0.0f);
        }
    if (spacePedal)
    {
        spacePedal = false;
        keysProcessor.setUiPedal(false);
    }
}

void KeysEditor::shiftComputerOctave(int octaves)
{
    releaseComputerKeys();
    computerBase = juce::jlimit(24, 84, computerBase + 12 * octaves);
    keysStrip.setComputerKeyRange(computerBase, static_cast<int>(sizeof(kComputerKeys)) - 2);
}
