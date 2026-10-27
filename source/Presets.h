#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <vector>

// =================================================================================================
//  Presets — the factory sounds, user presets saved as files, and the glide between them.
//
//  A preset sets every *sound* parameter. Parameters it doesn't list go back to their defaults, so
//  a preset always sounds the same, whatever was loaded before. Tune, Touch and Level are the
//  player's settings (the band's tuning, the keyboard's feel, the mix) and presets leave them alone.
//
//  Switching glides the knobs to their new places over about half a second, so the sound and the
//  light behind the controls morph instead of jumping. Choices (Plate → Spring) switch at once.
//
//  User presets are XML files in Documents/Keys/Presets. Message thread only.
// =================================================================================================
namespace keys
{
struct Preset
{
    juce::String name;
    std::map<juce::String, float> values; ///< parameter ID → plain value (a choice's index)
    juce::File file;                      ///< empty for factory presets
};

/// The seven factory presets, Dusk Ballad (the defaults) first.
const std::vector<Preset>& factoryPresets();

/// True for the parameters presets don't touch: tune, touch, volume.
bool isPlayerSetting(const juce::String& parameterId);

class PresetManager final : private juce::Timer
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);
    ~PresetManager() override;

    int getNumPresets() const noexcept { return static_cast<int>(presets.size()); }
    int getNumFactoryPresets() const noexcept;
    const Preset& getPreset(int index) const { return presets[static_cast<size_t>(index)]; }
    int getCurrentIndex() const noexcept { return current; }
    juce::String getCurrentName() const;

    /// Loads a preset; with `glide`, the knobs move there over ~0.5 s.
    void load(int index, bool glide = true);
    void next() { load((current + 1) % getNumPresets()); }
    void previous() { load((current + getNumPresets() - 1) % getNumPresets()); }

    /// Saves the current sound as a user preset (overwrites one with the same name).
    juce::File saveUserPreset(const juce::String& name);
    static juce::File userFolder();
    void rescanUserPresets();

    /// Restores the current preset's name after the host loads a session (no parameters change).
    void restoreSelection(const juce::String& name);

    /// Called (on the message thread) when the current preset changes.
    std::function<void()> onPresetChanged;

private:
    void timerCallback() override;
    void finishGlide();

    juce::AudioProcessorValueTreeState& state;
    std::vector<Preset> presets;
    int current = 0;

    struct Move
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float from = 0.0f, to = 0.0f; // normalised
    };
    std::vector<Move> glide;
    double glideStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
} // namespace keys
