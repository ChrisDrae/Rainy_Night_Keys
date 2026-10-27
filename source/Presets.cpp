#include "Presets.h"

#include "Parameters.h"

namespace keys
{
namespace
{
constexpr double kGlideSeconds = 0.52;
const juce::Identifier presetProperty { "preset" };

Preset make(const char* name, std::map<juce::String, float> values)
{
    return { name, std::move(values), {} };
}

/// Ease in and out (a smoothstep-like quadratic), so the knobs start and land gently.
float ease(float u)
{
    return u < 0.5f ? 2.0f * u * u : 1.0f - std::pow(-2.0f * u + 2.0f, 2.0f) * 0.5f;
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    using namespace params;
    static const std::vector<Preset> list = {
        make("Dusk Ballad", {}), // the defaults
        make("Sunday Tape", { { id::blend, 0.22f }, { id::warmth, 0.58f }, { id::bark, 0.28f }, { id::body, 0.52f },
                              { id::patina, 0.80f }, { id::motion, 0.18f }, { id::space, 0.30f }, { id::dust, 0.48f },
                              { id::mech, 0.42f }, { id::sigDi, 0.80f }, { id::sigAmp, 0.30f }, { id::sigRoom, 0.35f },
                              { id::spaceType, 2.0f } }),
        make("Glass Lullaby", { { id::blend, 0.74f }, { id::warmth, 0.30f }, { id::bark, 0.08f }, { id::body, 0.42f },
                                { id::patina, 0.18f }, { id::motion, 0.26f }, { id::space, 0.66f }, { id::dust, 0.08f },
                                { id::mech, 0.25f }, { id::sigDi, 0.90f }, { id::sigAmp, 0.10f }, { id::sigRoom, 0.30f },
                                { id::bellOctave, 1.0f }, { id::patinaFlutter, 0.0f } }),
        make("Basement Soul", { { id::blend, 0.10f }, { id::warmth, 0.72f }, { id::bark, 0.78f }, { id::body, 0.72f },
                                { id::patina, 0.24f }, { id::motion, 0.58f }, { id::space, 0.20f }, { id::dust, 0.14f },
                                { id::mech, 0.45f }, { id::sigDi, 0.60f }, { id::sigAmp, 0.75f }, { id::sigRoom, 0.30f },
                                { id::spaceType, 0.0f }, { id::motionRate, 4.0f } }),
        make("Snowglobe", { { id::blend, 0.86f }, { id::warmth, 0.22f }, { id::bark, 0.04f }, { id::body, 0.36f },
                            { id::patina, 0.30f }, { id::motion, 0.50f }, { id::space, 0.82f }, { id::dust, 0.22f },
                            { id::mech, 0.20f }, { id::sigDi, 0.90f }, { id::sigAmp, 0.05f }, { id::sigRoom, 0.40f },
                            { id::bellVoice, 2.0f }, { id::motionMode, 1.0f }, { id::motionRate, 1.0f } }),
        make("Late Bus", { { id::blend, 0.46f }, { id::warmth, 0.50f }, { id::bark, 0.36f }, { id::body, 0.60f },
                           { id::patina, 0.56f }, { id::motion, 0.62f }, { id::space, 0.55f }, { id::dust, 0.30f },
                           { id::mech, 0.35f }, { id::sigDi, 0.75f }, { id::sigAmp, 0.40f }, { id::sigRoom, 0.30f },
                           { id::bellVoice, 1.0f }, { id::motionMode, 2.0f }, { id::spaceType, 0.0f } }),
        make("Bare DI", { { id::blend, 0.0f }, { id::warmth, 0.0f }, { id::bark, 0.0f }, { id::body, 0.0f },
                          { id::patina, 0.0f }, { id::motion, 0.0f }, { id::space, 0.0f }, { id::dust, 0.0f },
                          { id::mech, 0.30f }, { id::sigDi, 1.0f }, { id::sigAmp, 0.0f }, { id::sigRoom, 0.0f } }),
    };
    return list;
}

bool isPlayerSetting(const juce::String& parameterId)
{
    return parameterId == params::id::tune || parameterId == params::id::touch || parameterId == params::id::volume;
}

// =================================================================================================
//  PresetManager
// =================================================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s) : state(s)
{
    rescanUserPresets();
}

PresetManager::~PresetManager()
{
    stopTimer();
}

int PresetManager::getNumFactoryPresets() const noexcept
{
    return static_cast<int>(factoryPresets().size());
}

juce::String PresetManager::getCurrentName() const
{
    return presets[static_cast<size_t>(current)].name;
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Keys").getChildFile("Presets");
}

void PresetManager::rescanUserPresets()
{
    const auto currentName = presets.empty() ? juce::String() : getCurrentName();
    presets = factoryPresets();

    auto files = userFolder().findChildFiles(juce::File::findFiles, false, "*.keyspreset");
    files.sort();
    for (const auto& file : files)
    {
        const auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !xml->hasTagName("KeysPreset"))
            continue;
        Preset preset { xml->getStringAttribute("name", file.getFileNameWithoutExtension()), {}, file };
        for (const auto* child : xml->getChildWithTagNameIterator("PARAM"))
            preset.values[child->getStringAttribute("id")] = static_cast<float>(child->getDoubleAttribute("value"));
        presets.push_back(std::move(preset));
    }

    current = 0;
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].name == currentName)
            current = i;
}

void PresetManager::load(int index, bool withGlide)
{
    if (index < 0 || index >= getNumPresets())
        return;
    finishGlide();
    current = index;
    const auto& preset = presets[static_cast<size_t>(index)];
    state.state.setProperty(presetProperty, preset.name, nullptr);

    for (auto* p : state.processor.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p);
        if (parameter == nullptr || isPlayerSetting(parameter->getParameterID()))
            continue;
        const auto it = preset.values.find(parameter->getParameterID());
        const float target = it != preset.values.end() ? parameter->convertTo0to1(it->second)
                                                       : parameter->getDefaultValue();
        const float from = parameter->getValue();
        if (std::abs(target - from) < 1.0e-6f)
            continue;

        // Only continuous controls glide; a choice or a switch jumps straight to its new value.
        const bool continuous = dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr;
        if (withGlide && continuous)
        {
            glide.push_back({ parameter, from, target });
            parameter->beginChangeGesture();
        }
        else
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(target);
            parameter->endChangeGesture();
        }
    }

    if (!glide.empty())
    {
        glideStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60);
    }
    if (onPresetChanged)
        onPresetChanged();
}

void PresetManager::timerCallback()
{
    const float u = static_cast<float>(
        std::clamp((juce::Time::getMillisecondCounterHiRes() - glideStart) / (kGlideSeconds * 1000.0), 0.0, 1.0));
    const float e = ease(u);
    for (const auto& move : glide)
        move.parameter->setValueNotifyingHost(move.from + (move.to - move.from) * e);
    if (u >= 1.0f)
        finishGlide();
}

void PresetManager::finishGlide()
{
    stopTimer();
    for (const auto& move : glide)
    {
        move.parameter->setValueNotifyingHost(move.to);
        move.parameter->endChangeGesture();
    }
    glide.clear();
}

juce::File PresetManager::saveUserPreset(const juce::String& name)
{
    const auto cleanName = name.trim().isEmpty() ? juce::String("My preset") : name.trim();
    const auto folder = userFolder();
    if (!folder.createDirectory())
        return {};

    juce::XmlElement xml("KeysPreset");
    xml.setAttribute("name", cleanName);
    xml.setAttribute("version", 1);
    for (auto* p : state.processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (!isPlayerSetting(parameter->getParameterID()))
            {
                auto* child = xml.createNewChildElement("PARAM");
                child->setAttribute("id", parameter->getParameterID());
                child->setAttribute("value", parameter->convertFrom0to1(parameter->getValue()));
            }

    const auto file = folder.getChildFile(juce::File::createLegalFileName(cleanName) + ".keyspreset");
    if (!xml.writeTo(file))
        return {};

    rescanUserPresets();
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].file == file)
            current = i;
    state.state.setProperty(presetProperty, cleanName, nullptr);
    if (onPresetChanged)
        onPresetChanged();
    return file;
}

void PresetManager::restoreSelection(const juce::String& name)
{
    for (int i = 0; i < getNumPresets(); ++i)
        if (presets[static_cast<size_t>(i)].name == name)
        {
            current = i;
            if (onPresetChanged)
                onPresetChanged();
            return;
        }
}
} // namespace keys
