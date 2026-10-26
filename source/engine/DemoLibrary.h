#pragma once

#include "SampleBank.h"

#include <atomic>
#include <memory>
#include <vector>

// =================================================================================================
//  DemoLibrary — sample banks rendered from models, so Keys plays before any real recording exists.
//
//  The demo tines go through exactly the same SampleBank / Zone / voice path as the real Mk II
//  library will: when the recordings arrive, only the bank's contents change.
// =================================================================================================
namespace keys
{
struct DemoTineOptions
{
    double sampleRate = 48000.0;
    int lowestRoot = 28;   ///< E1, bottom of a 73-key Mk II
    int highestRoot = 100; ///< E7
    int rootStep = 3;      ///< a sample every minor third: at most ±1 semitone of pitch shift
    std::vector<int> layerTopVelocities { 40, 75, 105, 127 };
    double maxSeconds = 8.0; ///< cap on render length (tests use short renders)
    int threads = 0;         ///< 0 = one per CPU core
};

/// Renders the demo tine bank. Pass `cancel` to stop early (returns nullptr when cancelled).
std::unique_ptr<SampleBank> buildDemoTines(const DemoTineOptions& options = {},
                                           const std::atomic<bool>* cancel = nullptr);

/// A modeled celeste, used only when the celeste samples can't be found.
std::unique_ptr<SampleBank> buildModeledBell(double sampleRate = 48000.0);
} // namespace keys
