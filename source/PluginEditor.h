#pragma once

#include "PluginProcessor.h"
#include "ui/ColourField.h"
#include "ui/Controls.h"
#include "ui/Keyboard.h"
#include "ui/KeysLookAndFeel.h"
#include "ui/Popovers.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

// =================================================================================================
//  KeysEditor — the plugin window, "Nocturne" (docs/mockup/nocturne.html).
//
//    header    ☰ menu · library chip with a MIDI light · ◀ preset ▶
//    row 1     Blend · Warmth · Bark · Body · Level
//    row 2     Dust · Bell (voice, octave, from key) · Mechanics
//    row 3     Patina (wow, flutter, narrow) · Motion (mode, rate) · Space (type) · Signal (DI, Amp, Room)
//    footer    Keys · Tine & Bell · Pedal
//    keys      73 keys, E1–E7 (can be hidden)
//
//  Behind it all, the ColourField: light that takes its colour from the knobs and blooms with
//  every note. Everything is laid out at a design size of 1040 × 700 (plus 112 for the keys) and
//  scaled as a whole when the window is resized.
//
//  The computer keyboard plays notes too: A W S E D F T G Y H U J K O L P ; from C3, Z / X shift
//  the octave, and holding Space is the sustain pedal.
// =================================================================================================
class KeysEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KeysEditor(KeysProcessor&);
    ~KeysEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;

    static constexpr int kDesignWidth = 1040;
    static constexpr int kDesignHeight = 700;
    static constexpr int kKeysHeight = 112;

    // --- for the screenshot tool -------------------------------------------------------------------
    void openFinePanel(const juce::String& key);
    void openMenu();
    void setKeysVisible(bool visible);
    /// Runs one frame of the animation (normally the timer does this).
    void animate(double seconds);
    /// The window's main areas, in window coordinates, in the order the user guide numbers them.
    std::vector<juce::Rectangle<int>> getLandmarks() const;

private:
    void timerCallback() override;
    void layout();
    void enableAllMidiInputs();
    int designHeight() const noexcept { return kDesignHeight + (keysVisible ? kKeysHeight : 0); }
    void updateConstraints();

    void toggleFine(const juce::String& key);
    void closeFine();
    void toggleMenu();
    void closeMenu();
    void showPresetList();
    void savePresetDialog();

    void releaseComputerKeys();
    void shiftComputerOctave(int octaves);

    KeysProcessor& keysProcessor;
    keys::ui::KeysLookAndFeel lookAndFeel; // must outlive every component that uses it: declared first
    juce::TooltipWindow tooltips { this, 700 };

    juce::Component content; // the window at design size; scaled by a transform
    keys::ui::ColourField field;

    keys::ui::MenuButton menuButton;
    keys::ui::LibraryChip chip;
    keys::ui::PresetBox presetBox;
    keys::ui::TitleView title;

    keys::ui::Tile blendTile, warmthTile, barkTile, bodyTile, levelTile;
    keys::ui::Tile dustTile, bellTile, mechTile;
    keys::ui::Tile patinaTile, motionTile, spaceTile, signalTile;

    std::unique_ptr<keys::ui::FaderColumn> blend, level;
    std::unique_ptr<keys::ui::KnobColumn> warmth, bark, body, dust, mech, patina, motion, space;
    keys::ui::HueLabel bellLabel;
    std::unique_ptr<keys::ui::CaptionedChoice> bellVoice, bellOctave, bellRange, motionMode, motionRate, spaceType;
    std::unique_ptr<keys::ui::ParamToggle> wow, flutter, narrow;
    std::array<std::unique_ptr<keys::ui::FaderColumn>, 3> signal;
    keys::ui::CaptionView signalCaption;

    keys::ui::CheckToggle keysToggle { "Keys" }, pedalToggle { "Pedal" };
    keys::ui::CaptionView footerCaption;
    keys::ui::KeysStrip keysStrip;

    std::unique_ptr<keys::ui::FinePanel> fine;
    std::unique_ptr<keys::ui::MenuPanel> menu;

    bool keysVisible = true, still = false;
    double lastFrame = 0.0;
    uint32_t lastEventCount = 0;
    float midiLight = 0.0f;
    int ticks = 0;
    juce::String midiInputText;

    // The computer keyboard.
    int computerBase = 48; // C3
    std::array<bool, 17> computerDown {};
    std::array<int, 17> computerNote {};
    bool spacePedal = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysEditor)
};
