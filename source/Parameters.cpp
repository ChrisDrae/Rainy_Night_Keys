#include "Parameters.h"

namespace keys::params
{
namespace
{
constexpr int kBellRangeNotes[] = { 36, 41, 48, 53, 60 }; // C2, F2, C3, F3, C4
constexpr float kMotionRates[] = { 0.5f, 1.0f, 1.6f, 2.5f, 4.0f, 6.0f, 8.0f };

int choiceIndex(const std::atomic<float>* value, int count) noexcept
{
    return std::clamp(juce::roundToInt(value->load(std::memory_order_relaxed)), 0, count - 1);
}

float load(const std::atomic<float>* value) noexcept
{
    return value->load(std::memory_order_relaxed);
}
} // namespace

juce::StringArray bellVoiceChoices() { return { "Celeste", "Vibes", "Both" }; }
juce::StringArray bellOctaveChoices() { return { "0", "+1" }; }
juce::StringArray warmthCharChoices() { return { "Tube", "Transistor" }; }
juce::StringArray motionModeChoices() { return { "Vibrato", "Chorus", "Both" }; }
juce::StringArray spaceTypeChoices() { return { "Spring", "Plate", "Room" }; }

juce::StringArray bellRangeChoices()
{
    juce::StringArray names;
    for (int note : kBellRangeNotes)
        names.add(juce::MidiMessage::getMidiNoteName(note, true, true, 4));
    return names;
}

int bellRangeNote(int index)
{
    return kBellRangeNotes[std::clamp(index, 0, static_cast<int>(std::size(kBellRangeNotes)) - 1)];
}

juce::StringArray motionRateChoices()
{
    juce::StringArray names;
    for (float hz : kMotionRates)
        names.add(juce::String(hz, hz == std::floor(hz) ? 0 : 1) + " Hz");
    return names;
}

float motionRateHz(int index)
{
    return kMotionRates[std::clamp(index, 0, static_cast<int>(std::size(kMotionRates)) - 1)];
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percent = AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float v, int) { return String(juce::roundToInt(v * 100.0f)); });

    // A 0..1 control shown as 0..100. Defaults are the "Dusk Ballad" preset.
    auto unit = [&](const char* parameterId, const char* name, float defaultValue)
    {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID { parameterId, 1 }, name,
                                                         NormalisableRange<float>(0.0f, 1.0f), defaultValue, percent));
    };
    auto choice = [&](const char* parameterId, const char* name, const StringArray& choices, int defaultIndex)
    { layout.add(std::make_unique<AudioParameterChoice>(ParameterID { parameterId, 1 }, name, choices, defaultIndex)); };
    auto toggle = [&](const char* parameterId, const char* name, bool defaultValue)
    { layout.add(std::make_unique<AudioParameterBool>(ParameterID { parameterId, 1 }, name, defaultValue)); };

    // Playing
    unit(id::blend, "Blend", 0.38f);
    choice(id::bellVoice, "Bell Voice", bellVoiceChoices(), 0);
    choice(id::bellOctave, "Bell Octave", bellOctaveChoices(), 0);
    choice(id::bellRange, "Bell From Key", bellRangeChoices(), 1);
    unit(id::touch, "Touch", 0.5f);
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::tune, 1 }, "Tune", NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("ct").withStringFromValueFunction(
            [](float v, int) { return (v > 0.05f ? "+" : "") + String(v, 1); })));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::volume, 1 }, "Volume", NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 2.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction(
            [](float v, int) { return v <= -59.95f ? String("-inf") : String(v, 1); })));

    // Colour knobs
    unit(id::warmth, "Warmth", 0.62f);
    unit(id::bark, "Bark", 0.30f);
    unit(id::body, "Body", 0.58f);
    unit(id::patina, "Patina", 0.35f);
    unit(id::motion, "Motion", 0.32f);
    unit(id::space, "Space", 0.42f);
    unit(id::dust, "Dust", 0.18f);
    unit(id::mech, "Mechanics", 0.35f);

    // Signal mixer
    unit(id::sigDi, "Signal DI", 0.85f);
    unit(id::sigAmp, "Signal Amp", 0.35f);
    unit(id::sigRoom, "Signal Room", 0.25f);

    // Fine controls
    unit(id::warmthDrive, "Warmth Drive", 0.5f);
    unit(id::warmthTilt, "Warmth Tilt", 0.5f);
    choice(id::warmthChar, "Warmth Character", warmthCharChoices(), 0);
    unit(id::barkVel, "Bark Velocity", 0.5f);
    unit(id::barkDing, "Tine Ding", 0.5f);
    unit(id::bodyRes, "Body Resonance", 0.5f);
    unit(id::bodyDouble, "Body Double", 0.35f);
    unit(id::bodyWidth, "Body Width", 0.5f);
    toggle(id::patinaWow, "Patina Wow", true);
    toggle(id::patinaFlutter, "Patina Flutter", true);
    toggle(id::patinaNarrow, "Patina Narrow", true);
    choice(id::motionMode, "Motion Mode", motionModeChoices(), 0);
    choice(id::motionRate, "Motion Rate", motionRateChoices(), 2);
    choice(id::spaceType, "Space Type", spaceTypeChoices(), 1);
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::spacePre, 1 }, "Space Pre-delay", NormalisableRange<float>(0.0f, 120.0f, 1.0f), 24.0f,
        AudioParameterFloatAttributes().withLabel("ms").withStringFromValueFunction(
            [](float v, int) { return String(juce::roundToInt(v)); })));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID { id::spaceDecay, 1 }, "Space Decay", NormalisableRange<float>(0.5f, 4.0f, 0.01f), 2.0f,
        AudioParameterFloatAttributes().withLabel("s").withStringFromValueFunction(
            [](float v, int) { return String(v, 1); })));
    unit(id::spaceTone, "Space Tone", 0.55f);
    return layout;
}

ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState& s) : state(s) {}

std::atomic<float>* ParameterReader::get(const char* parameterId) const
{
    auto* value = state.getRawParameterValue(parameterId);
    jassert(value != nullptr); // every ID in id:: must be in createLayout()
    return value;
}

EngineParams ParameterReader::read() const noexcept
{
    EngineParams p;
    p.blend = load(blend);
    p.bellVoice = static_cast<BellVoice>(choiceIndex(bellVoice, 3));
    p.bellOctave = choiceIndex(bellOctave, 2);
    p.bellLowestKey = bellRangeNote(choiceIndex(bellRange, static_cast<int>(std::size(kBellRangeNotes))));
    p.touch = load(touch);
    p.tuneCents = load(tune);
    p.volumeDb = load(volume);

    p.warmth = load(warmth);
    p.bark = load(bark);
    p.body = load(body);
    p.patina = load(patina);
    p.motion = load(motion);
    p.space = load(space);
    p.dust = load(dust);
    p.mechanics = load(mech);

    p.signalDi = load(sigDi);
    p.signalAmp = load(sigAmp);
    p.signalRoom = load(sigRoom);

    p.warmthDrive = load(warmthDrive);
    p.warmthTilt = load(warmthTilt);
    p.warmthCharacter = static_cast<WarmthCharacter>(choiceIndex(warmthChar, 2));
    p.barkVelocity = load(barkVel);
    p.tineDing = load(barkDing);
    p.bodyResonance = load(bodyRes);
    p.bodyDouble = load(bodyDouble);
    p.bodyWidth = load(bodyWidth);
    p.wow = load(patinaWow) > 0.5f;
    p.flutter = load(patinaFlutter) > 0.5f;
    p.narrow = load(patinaNarrow) > 0.5f;
    p.motionMode = static_cast<MotionMode>(choiceIndex(motionMode, 3));
    p.motionRateHz = motionRateHz(choiceIndex(motionRate, static_cast<int>(std::size(kMotionRates))));
    p.spaceType = static_cast<SpaceType>(choiceIndex(spaceType, 3));
    p.spacePreDelayMs = load(spacePre);
    p.spaceDecaySeconds = load(spaceDecay);
    p.spaceTone = load(spaceTone);
    return p;
}
} // namespace keys::params
