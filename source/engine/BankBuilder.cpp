#include "BankBuilder.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace keys
{
namespace
{
std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t end = text.find(separator, start);
        if (end == std::string_view::npos)
        {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b)
{
    return a.size() == b.size()
           && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y)
                         { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

std::string_view stripExtension(std::string_view fileName)
{
    const size_t dot = fileName.rfind('.');
    return dot == std::string_view::npos ? fileName : fileName.substr(0, dot);
}
} // namespace

int parseNoteName(std::string_view text) noexcept
{
    if (text.size() < 2)
        return -1;

    static constexpr int letterToSemitone[] = { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
    const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
    if (letter < 'a' || letter > 'g')
        return -1;

    int semitone = letterToSemitone[letter - 'a'];
    size_t i = 1;
    if (text[i] == '#')
    {
        ++semitone;
        ++i;
    }
    else if (text[i] == 'b' && i + 1 < text.size() && (std::isdigit(static_cast<unsigned char>(text[i + 1])) || text[i + 1] == '-'))
    {
        --semitone;
        ++i;
    }

    bool negative = false;
    if (i < text.size() && text[i] == '-')
    {
        negative = true;
        ++i;
    }
    if (i >= text.size())
        return -1;

    int octave = 0;
    for (; i < text.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[i])))
            return -1;
        octave = octave * 10 + (text[i] - '0');
    }
    if (negative)
        octave = -octave;

    const int note = (octave + 1) * 12 + semitone;
    return note >= 0 && note <= 127 ? note : -1;
}

std::optional<ParsedSampleName> parseCelesteFileName(std::string_view fileName) noexcept
{
    const auto parts = split(stripExtension(fileName), '-');
    if (parts.size() != 3 || !equalsIgnoreCase(parts[0], "celeste"))
        return std::nullopt;

    ParsedSampleName parsed;
    parsed.note = parseNoteName(parts[1]);
    if (parsed.note < 0)
        return std::nullopt;
    if (equalsIgnoreCase(parts[2], "soft"))
        parsed.layer = 0;
    else if (equalsIgnoreCase(parts[2], "hard"))
        parsed.layer = 1;
    else
        return std::nullopt;
    return parsed;
}

std::optional<ParsedSampleName> parseVcslFileName(std::string_view fileName) noexcept
{
    // Vibes_soft_A2_v1_rr1_Main
    const auto parts = split(stripExtension(fileName), '_');
    if (parts.size() < 4)
        return std::nullopt;

    ParsedSampleName parsed;
    parsed.note = parseNoteName(parts[2]);
    if (parsed.note < 0)
        return std::nullopt;

    const auto layer = parts[3];
    if (layer.size() < 2 || std::tolower(static_cast<unsigned char>(layer[0])) != 'v')
        return std::nullopt;
    int number = 0;
    for (size_t i = 1; i < layer.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(layer[i])))
            return std::nullopt;
        number = number * 10 + (layer[i] - '0');
    }
    if (number < 1)
        return std::nullopt;
    parsed.layer = number - 1;
    return parsed;
}

void mapRootsIntoBank(SampleBank& bank, std::vector<RootSample> samples, const std::vector<int>& layerTopVelocities)
{
    if (samples.empty() || layerTopVelocities.empty())
        return;

    // Distinct roots, low to high: the key splits sit halfway between neighbours.
    std::vector<int> roots;
    for (const auto& s : samples)
        roots.push_back(s.sample->rootNote);
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    std::map<int, std::pair<int, int>> keyRange; // root -> (lo, hi)
    for (size_t i = 0; i < roots.size(); ++i)
    {
        const int lo = i == 0 ? 0 : (roots[i - 1] + roots[i]) / 2 + 1;
        const int hi = i + 1 == roots.size() ? 127 : (roots[i] + roots[i + 1]) / 2;
        keyRange[roots[i]] = { lo, hi };
    }

    const int numLayers = static_cast<int>(layerTopVelocities.size());
    for (auto& s : samples)
    {
        const int layer = std::clamp(s.layer, 0, numLayers - 1);
        const auto [lo, hi] = keyRange[s.sample->rootNote];

        Zone zone;
        zone.loNote = lo;
        zone.hiNote = hi;
        zone.loVel = layer == 0 ? 1 : layerTopVelocities[static_cast<size_t>(layer - 1)] + 1;
        zone.hiVel = layerTopVelocities[static_cast<size_t>(layer)];
        zone.refVelocity = static_cast<float>(zone.hiVel);
        zone.sample = &bank.addSample(std::move(s.sample));
        bank.addZone(zone);
    }
}

std::unique_ptr<Sample> loadSampleFile(juce::AudioFormatManager& formats, const juce::File& file, int rootNote)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    auto sample = std::make_unique<Sample>();
    const int channels = std::clamp(static_cast<int>(reader->numChannels), 1, 2);
    const int length = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 60 * 192000));
    sample->audio.setSize(channels, length);
    if (!reader->read(&sample->audio, 0, length, 0, true, channels > 1))
        return nullptr;
    sample->sampleRate = reader->sampleRate;
    sample->rootNote = rootNote;
    sample->name = file.getFileName();
    return sample;
}
} // namespace keys
