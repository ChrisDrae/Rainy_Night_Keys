#pragma once

// =================================================================================================
//  EngineParams — every control of Keys as plain values, read once per block on the audio thread.
//
//  The defaults are the plain instrument: every colour knob at 0 bypasses its stage exactly, and
//  the signal mixer passes the DI alone. (The plugin's own defaults, the "Dusk Ballad" sound, live
//  in Parameters.cpp.)
// =================================================================================================
namespace keys
{
enum class BellVoice
{
    celeste = 0,
    vibes = 1,
    both = 2, ///< vibes below middle C, celeste from middle C up
};

enum class WarmthCharacter
{
    tube = 0,       ///< soft, slightly asymmetric: even harmonics
    transistor = 1, ///< firmer, symmetric: odd harmonics
};

enum class MotionMode
{
    vibrato = 0, ///< suitcase-style stereo pan
    chorus = 1,
    both = 2,
};

enum class SpaceType
{
    spring = 0,
    plate = 1,
    room = 2,
};

struct EngineParams
{
    // --- playing ----------------------------------------------------------------------------------
    float blend = 0.35f;    ///< 0 = tines only, 0.5 = both at full, 1 = bell only
    BellVoice bellVoice = BellVoice::celeste;
    int bellOctave = 0;     ///< 0 or +1
    int bellLowestKey = 41; ///< the bell fades out over the 6 keys below this (F2)
    float touch = 0.5f;     ///< velocity curve: 0 = even, 1 = very dynamic
    float tuneCents = 0.0f;
    float volumeDb = 0.0f;

    // --- colour knobs, 0..1 (0 = stage bypassed) --------------------------------------------------
    float warmth = 0.0f;
    float bark = 0.0f;
    float body = 0.0f;
    float patina = 0.0f;
    float motion = 0.0f;
    float space = 0.0f;
    float dust = 0.0f;
    float mechanics = 0.0f;

    // --- signal mixer, 0..1 -----------------------------------------------------------------------
    float signalDi = 1.0f;
    float signalAmp = 0.0f;
    float signalRoom = 0.0f;

    // --- fine controls (0.5 = the knob's own mapping) ---------------------------------------------
    float warmthDrive = 0.5f;
    float warmthTilt = 0.5f;
    WarmthCharacter warmthCharacter = WarmthCharacter::tube;

    float barkVelocity = 0.5f; ///< how much harder playing adds to the bark
    float tineDing = 0.5f;     ///< the attack's bright "ding": 0 = softened, 0.5 = as sampled, 1 = doubled

    float bodyResonance = 0.5f;
    float bodyDouble = 0.35f;
    float bodyWidth = 0.5f;

    bool wow = true;
    bool flutter = true;
    bool narrow = true;

    MotionMode motionMode = MotionMode::vibrato;
    float motionRateHz = 1.6f;

    SpaceType spaceType = SpaceType::plate;
    float spacePreDelayMs = 24.0f;
    float spaceDecaySeconds = 2.0f;
    float spaceTone = 0.55f;
};
} // namespace keys
