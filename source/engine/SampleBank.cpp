#include "SampleBank.h"

#include <algorithm>
#include <climits>

namespace keys
{
namespace
{
/// The median peak over the samples of the loudest velocity layer.
float medianTopPeak(const std::vector<Zone>& zones)
{
    int topVelocity = 0;
    for (const auto& zone : zones)
        topVelocity = std::max(topVelocity, zone.hiVel);

    std::vector<float> peaks;
    for (const auto& zone : zones)
        if (zone.hiVel == topVelocity)
            peaks.push_back(zone.sample->audio.getMagnitude(0, zone.sample->audio.getNumSamples()));

    if (peaks.empty())
        return 0.0f;
    std::nth_element(peaks.begin(), peaks.begin() + static_cast<std::ptrdiff_t>(peaks.size() / 2), peaks.end());
    return peaks[peaks.size() / 2];
}
} // namespace

const Sample& SampleBank::addSample(std::unique_ptr<Sample> sample)
{
    jassert(sample != nullptr);
    samples.push_back(std::move(sample));
    return *samples.back();
}

void SampleBank::addZone(const Zone& zone)
{
    jassert(zone.sample != nullptr);
    zones.push_back(zone);
    minRoot = std::min(minRoot, zone.sample->rootNote);
    maxRoot = std::max(maxRoot, zone.sample->rootNote);
}

void SampleBank::finalize()
{
    topLayerPeak = medianTopPeak(zones);
    table.fill(-1);
    if (zones.empty())
        return;

    // For every key and velocity, pick the zone that contains it; if none does, the nearest one.
    // Key distance outweighs velocity distance: a missing velocity layer borrows the neighbour
    // layer of the *same* key before it borrows another key.
    auto distance = [](int x, int lo, int hi) { return x < lo ? lo - x : (x > hi ? x - hi : 0); };

    for (int note = 0; note < 128; ++note)
    {
        for (int velocity = 0; velocity < 128; ++velocity)
        {
            int best = -1;
            int bestScore = INT_MAX;
            for (size_t z = 0; z < zones.size(); ++z)
            {
                const auto& zone = zones[z];
                const int score = distance(note, zone.loNote, zone.hiNote) * 1000
                                  + distance(std::max(1, velocity), zone.loVel, zone.hiVel);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = static_cast<int>(z);
                }
            }
            table[static_cast<size_t>(note * 128 + velocity)] = static_cast<int16_t>(best);
        }
    }
}


void SampleBank::normalizeTo(float targetPeak)
{
    const float median = medianTopPeak(zones);
    bankGain = median > 1.0e-6f ? targetPeak / median : 1.0f;
}

void SampleBank::normalizeLoudness(float targetRms, int velocity)
{
    std::vector<float> levels;
    for (const auto& sample : samples)
    {
        const Zone* zone = find(sample->rootNote, velocity);
        if (zone == nullptr || zone->sample != sample.get())
            continue; // this sample isn't what that key plays at this velocity
        const auto& audio = sample->audio;
        const int length = std::min(audio.getNumSamples(), static_cast<int>(sample->sampleRate));
        if (length <= 0)
            continue;
        float sum = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            sum += audio.getRMSLevel(ch, 0, length);
        const float within = std::clamp(static_cast<float>(velocity) / std::max(1.0f, zone->refVelocity), 0.02f, 1.0f);
        levels.push_back(within * sum / static_cast<float>(audio.getNumChannels()));
    }
    if (levels.empty())
        return;
    std::nth_element(levels.begin(), levels.begin() + static_cast<std::ptrdiff_t>(levels.size() / 2), levels.end());
    const float median = levels[levels.size() / 2];
    bankGain = median > 1.0e-6f ? targetRms / median : 1.0f;
}

size_t SampleBank::memoryBytes() const noexcept
{
    size_t bytes = 0;
    for (const auto& sample : samples)
        bytes += static_cast<size_t>(sample->audio.getNumChannels()) * static_cast<size_t>(sample->audio.getNumSamples())
                 * sizeof(float);
    return bytes;
}
} // namespace keys
