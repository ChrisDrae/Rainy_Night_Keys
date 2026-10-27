#pragma once

#include "SampleBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>
#include <vector>

// =================================================================================================
//  SfzLoader — reads a sample map in SFZ format (https://sfzformat.com) into a SampleBank.
//
//  SFZ is the plain-text format most free sample sets use, and the easiest way to describe your
//  own recordings: one line per sample, saying which keys and velocities it covers. For example
//
//      <group> lovel=1 hivel=64
//      <region> sample=tines/E2_soft.flac lokey=C2 hikey=F#2 pitch_keycenter=E2
//
//  Only the opcodes a key map needs are read: sample, key / lokey / hikey, pitch_keycenter,
//  lovel / hivel, and default_path. Headers <global>, <master>, <group> and <region> nest the way
//  SFZ says (a region inherits its group's opcodes, and so on). Everything else is ignored, so
//  effects, envelopes and controllers in a map don't stop it from loading.
// =================================================================================================
namespace keys
{
struct SfzRegion
{
    juce::String sample; ///< path, relative to the .sfz file (default_path already applied)
    int loKey = 0, hiKey = 127;
    int keyCenter = 60;
    int loVel = 1, hiVel = 127;
};

/// Parses SFZ text into its regions (those without a sample are dropped).
std::vector<SfzRegion> parseSfz(const juce::String& text);

/// Loads a whole SFZ map. Samples used by several regions are loaded once; silent tails are
/// trimmed. Returns nullptr if nothing could be loaded (or `cancel` became true).
std::unique_ptr<SampleBank> loadSfzBank(const juce::File& sfzFile, juce::AudioFormatManager& formats,
                                        const juce::String& bankName, const std::atomic<bool>* cancel = nullptr);

/// Cuts a sample's tail once it has faded below `floorDb` (relative to its peak), with a short
/// fade, so long recordings don't hold seconds of silence in memory. A sample that is still
/// ringing when the recording stops gets a half-second fade-out instead, so it never ends in a cut.
void trimTail(Sample& sample, float floorDb = -80.0f);
} // namespace keys
