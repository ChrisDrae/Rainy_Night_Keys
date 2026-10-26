#pragma once

#include "engine/KeysEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  Every host-visible parameter of Keys, in one place.
//
//  IDs are part of the plugin's public contract: hosts store automation and MIDI mappings by ID,
//  so NEVER rename an ID once released (docs/ui-design.md lists the planned set).
// =================================================================================================
namespace keys::params
{
namespace id
{
inline constexpr const char* blend = "blend";
inline constexpr const char* bellVoice = "bellVoice";
inline constexpr const char* bellOctave = "bellOctave";
inline constexpr const char* touch = "touch";
inline constexpr const char* tune = "tune";
inline constexpr const char* volume = "volume";
} // namespace id

juce::StringArray bellVoiceChoices();
juce::StringArray bellOctaveChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/// [RT] Reads the current parameter values into the engine's plain struct. Holds raw atomic
/// pointers, looked up once at construction, so reading costs nothing on the audio thread.
class ParameterReader
{
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

private:
    std::atomic<float>* blend;
    std::atomic<float>* bellVoice;
    std::atomic<float>* bellOctave;
    std::atomic<float>* touch;
    std::atomic<float>* tune;
    std::atomic<float>* volume;
};
} // namespace keys::params
