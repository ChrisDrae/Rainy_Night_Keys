#pragma once

#include "SampleBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <optional>
#include <string_view>
#include <vector>

// =================================================================================================
//  BankBuilder — turns a folder of note samples into a SampleBank.
//
//  Sample sets name their files after the note and the dynamic ("celeste-g#4-hard.flac",
//  "Vibes_soft_A2_v1_rr1_Main.wav"). We parse those names, then spread each sample over the keys
//  between its neighbours: a sample recorded every major third covers about two keys either side.
// =================================================================================================
namespace keys
{
/// "c4" → 60, "g#4" → 68, "A2" → 45, "Bb3" → 58 (C4 = middle C = 60). Returns -1 if unreadable.
int parseNoteName(std::string_view text) noexcept;

struct ParsedSampleName
{
    int note = -1;
    int layer = 0; ///< velocity layer, 0 = softest
};

/// "celeste-<note>-<soft|hard>.flac" (Sonatina Symphonic Orchestra's celeste).
std::optional<ParsedSampleName> parseCelesteFileName(std::string_view fileName) noexcept;

/// "Vibes_soft_<note>_v<n>_rr<m>_Main.wav" (Versilian Community Sample Library).
std::optional<ParsedSampleName> parseVcslFileName(std::string_view fileName) noexcept;

/// One sample waiting to be placed on the keyboard.
struct RootSample
{
    int layer = 0;
    std::unique_ptr<Sample> sample;
};

/// Adds the samples to `bank` with key ranges split halfway between neighbouring roots, and velocity
/// ranges taken from `layerTopVelocities` (layer i covers previous top + 1 .. layerTopVelocities[i]).
/// Each sample's refVelocity is its layer's top. Call bank.finalize() afterwards.
void mapRootsIntoBank(SampleBank& bank, std::vector<RootSample> samples, const std::vector<int>& layerTopVelocities);

/// Reads a whole audio file (WAV, FLAC, AIFF) into a Sample. Returns nullptr on failure.
std::unique_ptr<Sample> loadSampleFile(juce::AudioFormatManager& formats, const juce::File& file, int rootNote);
} // namespace keys
