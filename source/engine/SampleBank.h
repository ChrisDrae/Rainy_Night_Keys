#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <memory>
#include <vector>

// =================================================================================================
//  SampleBank — a set of recorded (or rendered) notes plus the map that says which one to play.
//
//  A *Sample* is one audio file: a note recorded at one pitch and one strength. A *Zone* says where
//  on the keyboard and at which velocities a sample is used. When a key is played, find() answers
//  "which zone?" from a precomputed 128 × 128 table, so the audio thread never searches.
//
//  Banks are built on a loader thread and never change after finalize(). The audio thread only
//  reads them, which is what makes sharing them without locks safe.
// =================================================================================================
namespace keys
{
struct Sample
{
    juce::AudioBuffer<float> audio; ///< 1 (mono) or 2 (stereo) channels
    double sampleRate = 48000.0;
    int rootNote = 60;              ///< the MIDI note this sample sounds at
    juce::String name;
};

struct Zone
{
    const Sample* sample = nullptr;
    int loNote = 0, hiNote = 127;   ///< keys this zone covers (inclusive)
    int loVel = 1, hiVel = 127;     ///< velocities this zone covers (inclusive)
    float refVelocity = 127.0f;     ///< the velocity the sample represents; softer notes play it quieter
};

class SampleBank
{
public:
    explicit SampleBank(juce::String bankName) : name(std::move(bankName)) {}

    /// Loader thread. Takes ownership; the returned reference stays valid for the bank's lifetime.
    const Sample& addSample(std::unique_ptr<Sample> sample);
    void addZone(const Zone& zone);

    /// Loader thread, once, after all zones are added: builds the lookup table.
    void finalize();

    /// [RT] The zone for this key and velocity (1..127), or nullptr if the bank is empty. Keys and
    /// velocities outside every zone get the nearest zone, so a bank always answers.
    const Zone* find(int note, int velocity) const noexcept
    {
        const int index = table[static_cast<size_t>(std::clamp(note, 0, 127) * 128 + std::clamp(velocity, 0, 127))];
        return index < 0 ? nullptr : &zones[static_cast<size_t>(index)];
    }

    /// Overall level trim for the whole bank (linear). Set by normalizeTo().
    float gain() const noexcept { return bankGain; }

    /// Loader thread: scales the bank so its loudest layer peaks around `targetPeak` (median over
    /// all samples in that layer), keeping every sample's level relative to the others.
    void normalizeTo(float targetPeak);

    const juce::String& getName() const noexcept { return name; }
    const std::vector<Zone>& getZones() const noexcept { return zones; }
    int getNumSamples() const noexcept { return static_cast<int>(samples.size()); }
    int lowestRoot() const noexcept { return minRoot; }
    int highestRoot() const noexcept { return maxRoot; }
    size_t memoryBytes() const noexcept;

private:
    juce::String name;
    std::vector<std::unique_ptr<Sample>> samples;
    std::vector<Zone> zones;
    std::array<int16_t, 128 * 128> table {};
    float bankGain = 1.0f;
    int minRoot = 127, maxRoot = 0;
};
} // namespace keys
