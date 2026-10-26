#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// =================================================================================================
//  Backdrop — the still version of the Nocturne field: smeared light on navy, grain and specks.
//  Rendered once per window size (the animated, knob-driven field comes in milestone M3).
// =================================================================================================
namespace keys::ui
{
juce::Image renderBackdrop(int width, int height, float scale = 1.0f);
} // namespace keys::ui
