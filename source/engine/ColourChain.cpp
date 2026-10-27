#include "ColourChain.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace keys
{
namespace
{
constexpr float kSqrt2 = 1.41421356f;
constexpr float kQuarterPi = 0.78539816f;

/// The value at sample i of a linear ramp from `from` to `to` over n samples.
inline float ramp(float from, float to, int i, int n) noexcept
{
    return from + (to - from) * static_cast<float>(i + 1) / static_cast<float>(n);
}
} // namespace

void ColourChain::prepare(double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    const auto block = static_cast<size_t>(std::max(kStep, maximumBlockSize));
    roomL.assign(block, 0.0f);
    roomR.assign(block, 0.0f);
    wetL.assign(block, 0.0f);
    wetR.assign(block, 0.0f);

    for (auto* glide : { &di, &amp, &room, &body, &bodyResonance, &warmth, &warmthDrive, &warmthTilt, &vibratoDepth,
                         &chorusWet, &chorusDepth, &patina, &narrowing, &spaceWet, &dust })
        glide->prepare(sampleRate, kStep, 0.03);

    // The amp: a preamp that stays clean on soft playing and breaks up a little on loud chords,
    // then a Fender-style voicing (a Rhodes through a Twin): the speaker's low thump, the tone
    // stack's scooped mids, a presence lift, and the steep roll-off of a 12" speaker.
    ampDrive.set(dsp::Saturator::Curve::tube, 0.55f);
    for (size_t ch = 0; ch < 2; ++ch)
    {
        auto& v = ampVoicing[ch];
        v[0].setHighPass(75.0, 0.7071, sampleRate);
        v[1].setPeak(120.0, 0.8, 2.0, sampleRate);
        v[2].setPeak(500.0, 0.7, -3.0, sampleRate);
        v[3].setPeak(2600.0, 1.0, 2.5, sampleRate);
        v[4].setLowPass(5200.0, 0.7071, sampleRate);
        v[5].setLowPass(7500.0, 0.6, sampleRate);
        hissTone[ch].setCutoff(9000.0, sampleRate);
    }

    // The room microphone: a small room, mostly early reflections.
    roomMic.prepare(sampleRate);
    roomMic.setType(SpaceType::room);
    roomMic.setDecay(0.45f);
    roomMic.setTone(0.45f);
    roomMic.setPreDelay(0.0f);

    reverb.prepare(sampleRate);
    for (auto& d : chorusDelay)
        d.allocate(static_cast<int>(0.05 * sampleRate));

    reset();
}

void ColourChain::reset() noexcept
{
    for (auto* stage : { &signalStage, &bodyStage, &warmthStage, &motionStage, &patinaStage, &spaceStage, &dustStage })
        stage->active = false;
    snapNext = true;
    spaceSwitchGain = 1.0f;
    roomMic.reset();
    reverb.reset();
    for (auto& d : chorusDelay)
        d.clear();
    for (size_t ch = 0; ch < 2; ++ch)
    {
        for (auto& f : ampVoicing[ch])
            f.reset();
        for (auto* f : { &bodyLow[ch], &bodyMid[ch], &warmthLow[ch], &warmthHigh[ch], &tapeHighPass[ch], &tapeLowPass[ch] })
            f->reset();
        hissTone[ch].reset();
        hissPrevious[ch] = 0.0f;
        crackle[ch] = {};
    }
}

void ColourChain::setTargets(const EngineParams& p) noexcept
{
    di.target = std::clamp(p.signalDi, 0.0f, 1.0f);
    amp.target = std::clamp(p.signalAmp, 0.0f, 1.0f);
    room.target = std::clamp(p.signalRoom, 0.0f, 1.0f);

    body.target = std::clamp(p.body, 0.0f, 1.0f);
    bodyResonance.target = std::clamp(p.bodyResonance, 0.0f, 1.0f);

    warmth.target = std::clamp(p.warmth, 0.0f, 1.0f);
    warmthDrive.target = std::clamp(p.warmthDrive, 0.0f, 1.0f);
    warmthTilt.target = std::clamp(p.warmthTilt, 0.0f, 1.0f);
    character = p.warmthCharacter;

    const float motionAmount = std::clamp(p.motion, 0.0f, 1.0f);
    vibratoDepth.target = p.motionMode != MotionMode::chorus ? motionAmount * 0.9f : 0.0f;
    chorusWet.target = p.motionMode != MotionMode::vibrato ? std::min(1.0f, motionAmount * 1.1f) * 0.7f : 0.0f;
    chorusDepth.target = 0.001f + motionAmount * 0.0035f; // seconds
    const double rate = std::clamp(static_cast<double>(p.motionRateHz), 0.1, 12.0);
    vibratoLfo.setRate(rate, sampleRate);
    chorusLfo.setRate(0.25 + rate * 0.12, sampleRate);

    patina.target = std::clamp(p.patina, 0.0f, 1.0f);
    narrowing.target = p.narrow ? patina.target : 0.0f;

    spaceWet.target = std::pow(std::clamp(p.space, 0.0f, 1.0f), 1.3f) * 1.1f;
    wantedSpaceType = p.spaceType;
    spacePreDelay = p.spacePreDelayMs;
    spaceDecay = p.spaceDecaySeconds;
    spaceTone = p.spaceTone;

    const float d = std::clamp(p.dust, 0.0f, 1.0f);
    dust.target = d * d * 0.16f;
}

void ColourChain::snapAll() noexcept
{
    for (auto* glide : { &di, &amp, &room, &body, &bodyResonance, &warmth, &warmthDrive, &warmthTilt, &vibratoDepth,
                         &chorusWet, &chorusDepth, &patina, &narrowing, &spaceWet, &dust })
        glide->snap(glide->target);
}

void ColourChain::advanceControls() noexcept
{
    for (auto* glide : { &di, &amp, &room, &body, &bodyResonance, &warmth, &warmthDrive, &warmthTilt, &vibratoDepth,
                         &chorusWet, &chorusDepth, &patina, &narrowing, &spaceWet, &dust })
        glide->advance();
}

void ColourChain::process(float* left, float* right, int numSamples, const EngineParams& params) noexcept
{
    jassert(static_cast<size_t>(numSamples) <= roomL.size());
    setTargets(params);
    if (snapNext)
    {
        snapAll();
        snapNext = false;
    }

    for (int start = 0; start < numSamples; start += kStep)
    {
        const int n = std::min(kStep, numSamples - start);
        float* l = left + start;
        float* r = right + start;

        diFrom = di.value;
        ampFrom = amp.value;
        roomFrom = room.value;
        vibratoFrom = vibratoDepth.value;
        chorusWetFrom = chorusWet.value;
        spaceWetFrom = spaceWet.value * spaceSwitchGain;
        dustFrom = dust.value;
        advanceControls();

        processSignal(l, r, n);
        processBody(l, r, n);
        processWarmth(l, r, n);
        processMotion(l, r, n);
        processPatina(l, r, n);
        processSpace(l, r, n);
        processDust(l, r, n);
    }
}

// -------------------------------------------------------------------------------------------------
//  Signal: DI · Amp · Room
// -------------------------------------------------------------------------------------------------
void ColourChain::processSignal(float* l, float* r, int n) noexcept
{
    const bool run = !(di.settledAt(1.0f) && amp.settledAt(0.0f) && room.settledAt(0.0f));
    if (signalStage.update(run))
    {
        roomMic.reset();
        ampDrive.reset();
        for (auto& voicing : ampVoicing)
            for (auto& f : voicing)
                f.reset();
    }
    if (!run)
        return;

    const bool useAmp = amp.value > 0.0f || ampFrom > 0.0f;
    const bool useRoom = room.value > 0.0f || roomFrom > 0.0f;
    if (useRoom)
        roomMic.process(l, r, roomL.data(), roomR.data(), n);

    for (int i = 0; i < n; ++i)
    {
        const float gDi = ramp(diFrom, di.value, i, n);
        float outL = gDi * l[i], outR = gDi * r[i];
        if (useAmp)
        {
            const float gAmp = 0.8f * ramp(ampFrom, amp.value, i, n);
            float a[2] = { ampDrive.process(l[i], 0), ampDrive.process(r[i], 1) };
            for (size_t ch = 0; ch < 2; ++ch)
                for (auto& f : ampVoicing[ch])
                    a[ch] = f.process(a[ch]);
            outL += gAmp * a[0];
            outR += gAmp * a[1];
        }
        if (useRoom)
        {
            const float gRoom = 1.4f * ramp(roomFrom, room.value, i, n);
            outL += gRoom * roomL[static_cast<size_t>(i)];
            outR += gRoom * roomR[static_cast<size_t>(i)];
        }
        l[i] = outL;
        r[i] = outR;
    }
}

// -------------------------------------------------------------------------------------------------
//  Body: low-mid fullness
// -------------------------------------------------------------------------------------------------
void ColourChain::processBody(float* l, float* r, int n) noexcept
{
    const bool run = !body.settledAt(0.0f);
    if (bodyStage.update(run))
        for (size_t ch = 0; ch < 2; ++ch)
        {
            bodyLow[ch].reset();
            bodyMid[ch].reset();
        }
    if (!run)
        return;

    const double lowDb = body.value * 6.0;
    const double midDb = body.value * bodyResonance.value * 8.0;
    for (size_t ch = 0; ch < 2; ++ch)
    {
        bodyLow[ch].setLowShelf(170.0, lowDb, sampleRate);
        bodyMid[ch].setPeak(330.0, 0.9, midDb, sampleRate);
    }
    const float makeup = dsp::dbToGain(-body.value * 3.3f); // keeps the overall level about even
    for (int i = 0; i < n; ++i)
    {
        l[i] = makeup * bodyMid[0].process(bodyLow[0].process(l[i]));
        r[i] = makeup * bodyMid[1].process(bodyLow[1].process(r[i]));
    }
}

// -------------------------------------------------------------------------------------------------
//  Warmth: a preamp
// -------------------------------------------------------------------------------------------------
void ColourChain::processWarmth(float* l, float* r, int n) noexcept
{
    const bool run = !warmth.settledAt(0.0f);
    if (warmthStage.update(run))
    {
        warmthSaturator.reset();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            warmthLow[ch].reset();
            warmthHigh[ch].reset();
        }
    }
    if (!run)
        return;

    // Drive and Tilt at 0.5 are the knob's own mapping; 0 and 1 halve or double it.
    warmthSaturator.set(character == WarmthCharacter::tube ? dsp::Saturator::Curve::tube
                                                           : dsp::Saturator::Curve::transistor,
                        warmth.value * warmthDrive.value * 2.0f * 0.55f);
    for (size_t ch = 0; ch < 2; ++ch)
    {
        warmthLow[ch].setLowShelf(260.0, warmth.value * 3.5, sampleRate);
        warmthHigh[ch].setHighShelf(3200.0, -warmth.value * warmthTilt.value * 2.0 * 7.0, sampleRate);
    }
    for (int i = 0; i < n; ++i)
    {
        l[i] = warmthHigh[0].process(warmthLow[0].process(warmthSaturator.process(l[i], 0)));
        r[i] = warmthHigh[1].process(warmthLow[1].process(warmthSaturator.process(r[i], 1)));
    }
}

// -------------------------------------------------------------------------------------------------
//  Motion: stereo vibrato and chorus
// -------------------------------------------------------------------------------------------------
void ColourChain::processMotion(float* l, float* r, int n) noexcept
{
    const bool run = !(vibratoDepth.settledAt(0.0f) && chorusWet.settledAt(0.0f));
    if (motionStage.update(run))
        for (auto& d : chorusDelay)
            d.clear();
    if (!run)
        return;

    const float baseDelay = static_cast<float>(0.012 * sampleRate);
    const float sweep = static_cast<float>(chorusDepth.value * sampleRate);
    for (int i = 0; i < n; ++i)
    {
        // Vibrato: an equal-power pan that swings with the LFO (a Rhodes suitcase's "vibrato").
        const float depth = ramp(vibratoFrom, vibratoDepth.value, i, n);
        const float theta = kQuarterPi * (1.0f + depth * vibratoLfo.next());
        float xl = l[i] * kSqrt2 * std::cos(theta);
        float xr = r[i] * kSqrt2 * std::sin(theta);

        // Chorus: a copy whose delay sweeps slowly, the two sides a quarter cycle apart.
        chorusDelay[0].push(xl);
        chorusDelay[1].push(xr);
        const float wet = ramp(chorusWetFrom, chorusWet.value, i, n);
        if (wet > 0.0f)
        {
            const float dl = chorusDelay[0].readFractional(baseDelay + sweep * chorusLfo.next());
            const float dr = chorusDelay[1].readFractional(baseDelay + sweep * chorusLfo.valueAt(0.25));
            const float norm = 1.0f / (1.0f + 0.35f * wet);
            xl = (xl + wet * dl) * norm;
            xr = (xr + wet * dr) * norm;
        }
        l[i] = xl;
        r[i] = xr;
    }
}

// -------------------------------------------------------------------------------------------------
//  Patina: worn tape
// -------------------------------------------------------------------------------------------------
void ColourChain::processPatina(float* l, float* r, int n) noexcept
{
    const bool run = !patina.settledAt(0.0f);
    if (patinaStage.update(run))
    {
        tapeSaturator.reset();
        for (size_t ch = 0; ch < 2; ++ch)
        {
            tapeHighPass[ch].reset();
            tapeLowPass[ch].reset();
        }
    }
    if (!run)
        return;

    tapeSaturator.set(dsp::Saturator::Curve::tube, patina.value * 0.7f);
    const double age = narrowing.value;
    for (size_t ch = 0; ch < 2; ++ch)
    {
        tapeHighPass[ch].setHighPass(20.0 + age * 170.0, 0.7071, sampleRate);
        tapeLowPass[ch].setLowPass(18000.0 * std::pow(3200.0 / 18000.0, age), 0.6, sampleRate);
    }
    // The tape's saturation and narrower band cost level: give it back, so Patina changes the
    // colour, not the loudness.
    const float makeup = dsp::dbToGain(patina.value * 3.2f);
    for (int i = 0; i < n; ++i)
    {
        l[i] = makeup * tapeLowPass[0].process(tapeHighPass[0].process(tapeSaturator.process(l[i], 0)));
        r[i] = makeup * tapeLowPass[1].process(tapeHighPass[1].process(tapeSaturator.process(r[i], 1)));
    }
}

// -------------------------------------------------------------------------------------------------
//  Space: reverb
// -------------------------------------------------------------------------------------------------
void ColourChain::processSpace(float* l, float* r, int n) noexcept
{
    const bool run = !spaceWet.settledAt(0.0f);
    if (spaceStage.update(run))
    {
        reverb.setType(wantedSpaceType);
        reverb.reset();
        spaceSwitchGain = 1.0f;
        spaceWetFrom = 0.0f;
    }
    if (!run)
        return;

    reverb.setDecay(spaceDecay);
    reverb.setTone(spaceTone);
    reverb.setPreDelay(spacePreDelay);

    // A new type re-tunes every delay line, which would click: fade the reverb out, switch, fade in.
    const float fadeStep = static_cast<float>(kStep / (0.03 * sampleRate));
    if (wantedSpaceType != reverb.getType())
    {
        if (spaceSwitchGain <= 0.0f)
        {
            reverb.setType(wantedSpaceType);
            reverb.reset();
        }
        else
        {
            spaceSwitchGain = std::max(0.0f, spaceSwitchGain - fadeStep);
        }
    }
    else
    {
        spaceSwitchGain = std::min(1.0f, spaceSwitchGain + fadeStep);
    }

    reverb.process(l, r, wetL.data(), wetR.data(), n);
    const float wetTo = spaceWet.value * spaceSwitchGain;
    for (int i = 0; i < n; ++i)
    {
        const float g = ramp(spaceWetFrom, wetTo, i, n);
        l[i] += g * wetL[static_cast<size_t>(i)];
        r[i] += g * wetR[static_cast<size_t>(i)];
    }
}

// -------------------------------------------------------------------------------------------------
//  Dust: hiss and crackle
// -------------------------------------------------------------------------------------------------
void ColourChain::processDust(float* l, float* r, int n) noexcept
{
    const bool run = !dust.settledAt(0.0f);
    dustStage.update(run);
    if (!run)
        return;

    // More dust, more crackle: from ~3 to ~13 clicks a second.
    const float amount = std::sqrt(dust.value / 0.16f);
    const float clickChance = static_cast<float>((3.0 + 10.0 * amount) / sampleRate);
    float* channels[2] = { l, r };
    for (int i = 0; i < n; ++i)
    {
        const float g = ramp(dustFrom, dust.value, i, n);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float white = noise.next();
            const float hiss = hissTone[ch].process((white - hissPrevious[ch]) * 0.11f);
            hissPrevious[ch] = white;

            auto& click = crackle[ch];
            if (noise.unit() < clickChance)
            {
                const float u = noise.unit();
                const float a = u * u * 0.8f * (noise.next() < 0.0f ? -1.0f : 1.0f);
                click[0] += a;
                click[1] -= 0.6f * a;
                click[2] += 0.25f * a;
            }
            const float pop = click[0];
            click[0] = click[1];
            click[1] = click[2];
            click[2] = 0.0f;

            channels[ch][i] += g * (hiss + pop);
        }
    }
}
} // namespace keys
