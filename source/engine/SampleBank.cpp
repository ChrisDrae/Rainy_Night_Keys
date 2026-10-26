#include "SampleBank.h"

#include <algorithm>
#include <climits>

namespace keys
{
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
    int topVelocity = 0;
    for (const auto& zone : zones)
        topVelocity = std::max(topVelocity, zone.hiVel);

    std::vector<float> peaks;
    for (const auto& zone : zones)
        if (zone.hiVel == topVelocity)
            peaks.push_back(zone.sample->audio.getMagnitude(0, zone.sample->audio.getNumSamples()));

    if (peaks.empty())
        return;
    std::nth_element(peaks.begin(), peaks.begin() + static_cast<std::ptrdiff_t>(peaks.size() / 2), peaks.end());
    const float median = peaks[peaks.size() / 2];
    bankGain = median > 1.0e-6f ? targetPeak / median : 1.0f;
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
