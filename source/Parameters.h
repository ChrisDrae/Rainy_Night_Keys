#pragma once

#include "engine/EngineParams.h"

#include <juce_audio_processors/juce_audio_processors.h>

// =================================================================================================
//  Every host-visible parameter of Keys, in one place.
//
//  IDs are part of the plugin's public contract: hosts store automation and MIDI mappings by ID,
//  so NEVER rename an ID once released (docs/ui-design.md lists them).
// =================================================================================================
namespace keys::params
{
namespace id
{
// Playing
inline constexpr const char* blend = "blend";
inline constexpr const char* bellVoice = "bellVoice";
inline constexpr const char* bellOctave = "bellOctave";
inline constexpr const char* bellRange = "bellRange";
inline constexpr const char* touch = "touch";
inline constexpr const char* tune = "tune";
inline constexpr const char* volume = "volume";

// Colour knobs
inline constexpr const char* warmth = "warmth";
inline constexpr const char* bark = "bark";
inline constexpr const char* body = "body";
inline constexpr const char* patina = "patina";
inline constexpr const char* motion = "motion";
inline constexpr const char* space = "space";
inline constexpr const char* dust = "dust";
inline constexpr const char* mech = "mech";

// Signal mixer
inline constexpr const char* sigDi = "sigDi";
inline constexpr const char* sigAmp = "sigAmp";
inline constexpr const char* sigRoom = "sigRoom";

// Fine controls
inline constexpr const char* warmthDrive = "warmthDrive";
inline constexpr const char* warmthTilt = "warmthTilt";
inline constexpr const char* warmthChar = "warmthChar";
inline constexpr const char* barkVel = "barkVel";
inline constexpr const char* barkDing = "barkDing";
inline constexpr const char* bodyRes = "bodyRes";
inline constexpr const char* bodyDouble = "bodyDouble";
inline constexpr const char* bodyWidth = "bodyWidth";
inline constexpr const char* patinaWow = "patinaWow";
inline constexpr const char* patinaFlutter = "patinaFlutter";
inline constexpr const char* patinaNarrow = "patinaNarrow";
inline constexpr const char* motionMode = "motionMode";
inline constexpr const char* motionRate = "motionRate";
inline constexpr const char* spaceType = "spaceType";
inline constexpr const char* spacePre = "spacePre";
inline constexpr const char* spaceDecay = "spaceDecay";
inline constexpr const char* spaceTone = "spaceTone";
} // namespace id

juce::StringArray bellVoiceChoices();
juce::StringArray bellOctaveChoices();
juce::StringArray bellRangeChoices(); ///< note names, e.g. "F2"
int bellRangeNote(int index);         ///< the MIDI note of a bellRange choice
juce::StringArray warmthCharChoices();
juce::StringArray motionModeChoices();
juce::StringArray motionRateChoices(); ///< e.g. "1.6 Hz"
float motionRateHz(int index);
juce::StringArray spaceTypeChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/// [RT] Reads the current parameter values into the engine's plain struct. Holds raw atomic
/// pointers, looked up once at construction, so reading costs nothing on the audio thread.
class ParameterReader
{
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

private:
    std::atomic<float>* get(const char* parameterId) const;

    juce::AudioProcessorValueTreeState& state;
    std::atomic<float>* blend = get(id::blend);
    std::atomic<float>* bellVoice = get(id::bellVoice);
    std::atomic<float>* bellOctave = get(id::bellOctave);
    std::atomic<float>* bellRange = get(id::bellRange);
    std::atomic<float>* touch = get(id::touch);
    std::atomic<float>* tune = get(id::tune);
    std::atomic<float>* volume = get(id::volume);
    std::atomic<float>* warmth = get(id::warmth);
    std::atomic<float>* bark = get(id::bark);
    std::atomic<float>* body = get(id::body);
    std::atomic<float>* patina = get(id::patina);
    std::atomic<float>* motion = get(id::motion);
    std::atomic<float>* space = get(id::space);
    std::atomic<float>* dust = get(id::dust);
    std::atomic<float>* mech = get(id::mech);
    std::atomic<float>* sigDi = get(id::sigDi);
    std::atomic<float>* sigAmp = get(id::sigAmp);
    std::atomic<float>* sigRoom = get(id::sigRoom);
    std::atomic<float>* warmthDrive = get(id::warmthDrive);
    std::atomic<float>* warmthTilt = get(id::warmthTilt);
    std::atomic<float>* warmthChar = get(id::warmthChar);
    std::atomic<float>* barkVel = get(id::barkVel);
    std::atomic<float>* barkDing = get(id::barkDing);
    std::atomic<float>* bodyRes = get(id::bodyRes);
    std::atomic<float>* bodyDouble = get(id::bodyDouble);
    std::atomic<float>* bodyWidth = get(id::bodyWidth);
    std::atomic<float>* patinaWow = get(id::patinaWow);
    std::atomic<float>* patinaFlutter = get(id::patinaFlutter);
    std::atomic<float>* patinaNarrow = get(id::patinaNarrow);
    std::atomic<float>* motionMode = get(id::motionMode);
    std::atomic<float>* motionRate = get(id::motionRate);
    std::atomic<float>* spaceType = get(id::spaceType);
    std::atomic<float>* spacePre = get(id::spacePre);
    std::atomic<float>* spaceDecay = get(id::spaceDecay);
    std::atomic<float>* spaceTone = get(id::spaceTone);
};
} // namespace keys::params
