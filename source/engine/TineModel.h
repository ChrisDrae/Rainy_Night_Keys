#pragma once

#include <cstdint>

// =================================================================================================
//  TineModel — a physically inspired model of one Mk II note, used to render the demo tines.
//
//  What happens in the real instrument, and how the model copies it:
//
//    1. A neoprene hammer strikes a thin steel *tine*. The tine rings mostly at its fundamental,
//       plus a short, bright inharmonic overtone (the "ding" of the attack, ~6.3× the fundamental
//       for a clamped bar) that dies within a fraction of a second.
//    2. The tine is bolted to a heavier *tonebar* tuned to the same pitch, which keeps the note
//       singing. Its tiny mistuning against the tine gives a slow, living beat.
//    3. An electromagnetic *pickup* sits next to the tine tip. Its output voltage is the rate of
//       change of the magnetic flux, and the flux depends nonlinearly on how close the tine is:
//       y = d/dt f(x + offset). Small swings are almost linear (round, bell-like); big swings,
//       from hard playing, are distorted asymmetrically, so harmonics appear. That's the *bark*.
//       The offset is the tine's alignment to the pickup, which a technician sets by "voicing".
//
//  The model renders at a fixed rate into a mono buffer. It is deterministic for a given seed.
// =================================================================================================
namespace keys
{
struct TineSettings
{
    float pickupOffset = 0.22f; ///< voicing: alignment of tine and pickup (more = more bark asymmetry)
    float pickupDrive = 1.35f;  ///< how close the pickup is: drive into the nonlinearity
    float ding = 1.0f;          ///< level of the attack overtones
    float hammerNoise = 1.0f;   ///< level of the acoustic hammer thump
};

/// How long to render a note before it has faded to near silence (seconds).
double tineRenderSeconds(int note) noexcept;

/// The fundamental's −60 dB decay time for this key (seconds).
double tineDecaySeconds(int note) noexcept;

/// Renders one note into `out` (mono, `numFrames` long). `velocity` is 0..1.
void renderTine(float* out, int numFrames, double sampleRate, int note, float velocity, uint32_t seed,
                const TineSettings& settings = {});

/// Renders a simple modeled celeste note (struck steel bar over a wooden resonator). Only used when
/// the celeste samples are missing.
void renderModeledBell(float* out, int numFrames, double sampleRate, int note, float velocity, uint32_t seed);
} // namespace keys
