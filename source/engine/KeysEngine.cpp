#include "KeysEngine.h"

#include "DspMath.h"

#include <algorithm>
#include <cmath>

namespace keys
{
namespace
{
/// The level the per-voice effects were tuned at: a loud note peaking here gets the bark and the
/// key noise as designed; banks at other levels are scaled to match (SampleBank::fortePeak()).
constexpr float kReferenceFortePeak = 0.32f;
constexpr double kDoubleDetuneCents = 4.0; ///< Body's double: a second tine, a few cents sharp
constexpr double kDingTau = 0.025;         ///< how long Tine ding acts on the attack (seconds)
constexpr double kDingSplitHz = 1500.0;    ///< above this, the attack counts as "ding"

float unit(float x) noexcept
{
    return std::clamp(x, 0.0f, 1.0f);
}
} // namespace

// =================================================================================================
//  NoteEventQueue and EngineStatus
// =================================================================================================
void NoteEventQueue::push(const NoteEvent& e) noexcept
{
    const auto scope = fifo.write(1);
    if (scope.blockSize1 > 0)
        items[static_cast<size_t>(scope.startIndex1)] = e;
    else if (scope.blockSize2 > 0)
        items[static_cast<size_t>(scope.startIndex2)] = e;
}

int NoteEventQueue::pop(NoteEvent* out, int maxEvents) noexcept
{
    const auto scope = fifo.read(std::min(maxEvents, fifo.getNumReady()));
    int count = 0;
    for (int i = 0; i < scope.blockSize1; ++i)
        out[count++] = items[static_cast<size_t>(scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i)
        out[count++] = items[static_cast<size_t>(scope.startIndex2 + i)];
    return count;
}

void EngineStatus::pushEvent(const MidiEventInfo& e) noexcept
{
    const uint32_t packed = static_cast<uint32_t>(e.type) | (static_cast<uint32_t>(e.channel) << 8)
                            | (static_cast<uint32_t>(e.number) << 16) | (static_cast<uint32_t>(e.value) << 24);
    const uint32_t index = historyWrite.load(std::memory_order_relaxed);
    history[index % kHistory].store(packed, std::memory_order_relaxed);
    historyWrite.store(index + 1, std::memory_order_release);
}

MidiEventInfo EngineStatus::unpack(uint32_t packed) noexcept
{
    MidiEventInfo e;
    e.type = static_cast<MidiEventInfo::Type>(packed & 0xff);
    e.channel = static_cast<uint8_t>((packed >> 8) & 0xff);
    e.number = static_cast<uint8_t>((packed >> 16) & 0xff);
    e.value = static_cast<uint8_t>((packed >> 24) & 0xff);
    return e;
}

// =================================================================================================
//  Player: one sample being read at a (possibly fractional, possibly wobbling) rate
// =================================================================================================
void KeysEngine::Player::release(float coef) noexcept
{
    if (!isActive())
        return;
    releasing = true;
    releaseCoef = std::min(releaseCoef, coef); // a faster release always wins
}

bool KeysEngine::Player::render(float* left, float* right, int numSamples, const float* ratio, float level) noexcept
{
    const auto& audio = sample->audio;
    const int length = audio.getNumSamples();
    const float* l = audio.getReadPointer(0);
    const bool stereo = audio.getNumChannels() > 1;
    const float* r = stereo ? audio.getReadPointer(1) : l;

    for (int i = 0; i < numSamples; ++i)
    {
        const int index = static_cast<int>(position);
        if (index + 2 >= length || envelope < 1.0e-3f) // released below −60 dB: done
        {
            stop();
            return false;
        }
        if (level > 0.0f)
        {
            const float t = static_cast<float>(position - index);
            const int prev = index > 0 ? index - 1 : 0;
            const float g = gain * envelope * level;
            const float vl = g * dsp::interpolateHermite(l[prev], l[index], l[index + 1], l[index + 2], t);
            left[i] += vl;
            right[i] += stereo ? g * dsp::interpolateHermite(r[prev], r[index], r[index + 1], r[index + 2], t) : vl;
        }
        position += baseStep * static_cast<double>(ratio[i]);
        if (releasing)
            envelope *= releaseCoef;
    }
    return true;
}

// =================================================================================================
//  Engine
// =================================================================================================
KeysEngine::KeysEngine()
{
    prepare(48000.0, 512);
}

void KeysEngine::prepare(double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlock = std::max(32, maximumBlockSize);
    tineBus.setSize(2, maxBlock, false, true, false);
    bellBus.setSize(2, maxBlock, false, true, false);
    voiceScratch.setSize(2, maxBlock, false, true, false);
    chunkMix.setSize(2, maxBlock, false, true, false);
    pitchRatio.assign(static_cast<size_t>(maxBlock), 1.0f);
    doubleRatio.assign(static_cast<size_t>(maxBlock), 1.0f);
    thumpBus.assign(static_cast<size_t>(maxBlock), 0.0f);
    tineGain.reset(sampleRate, 0.03);
    bellGain.reset(sampleRate, 0.03);
    outputGain.reset(sampleRate, 0.03);
    colour.prepare(sampleRate, maxBlock);

    wowLfo.setRate(0.55, sampleRate);
    wowDrift.setRate(0.23, sampleRate);
    flutterLfo.setRate(6.8, sampleRate);
    wowCents = flutterCents = 0.0f;

    for (auto& voice : voices)
        voice = Voice {};
    for (auto& thump : thumps)
        thump = Thump {};
    sustainDown = false;
    snapSmoothers = true;
}

int KeysEngine::countActiveVoices() const noexcept
{
    int count = 0;
    for (const auto& voice : voices)
        if (voice.isActive())
            ++count;
    return count;
}

void KeysEngine::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const EngineParams& params,
                         const Banks& banks) noexcept
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples == 0 || buffer.getNumChannels() == 0)
        return;

    // Blend law: the middle plays both layers at full; each end is one layer alone.
    const float blend = unit(params.blend);
    tineGain.setTargetValue(blend <= 0.5f ? 1.0f : (1.0f - blend) * 2.0f);
    bellGain.setTargetValue(blend >= 0.5f ? 1.0f : blend * 2.0f);
    outputGain.setTargetValue(dsp::dbToGain(params.volumeDb));
    if (snapSmoothers) // first block after prepare(): start at the targets instead of gliding there
    {
        tineGain.setCurrentAndTargetValue(tineGain.getTargetValue());
        bellGain.setCurrentAndTargetValue(bellGain.getTargetValue());
        outputGain.setCurrentAndTargetValue(outputGain.getTargetValue());
        snapSmoothers = false;
    }

    if (banks.tines != nullptr && banks.tines->fortePeak() > 0.0f)
        mechanicsScale = std::clamp(banks.tines->fortePeak() / kReferenceFortePeak, 0.05f, 4.0f);

    float* outLeft = buffer.getWritePointer(0);
    float* outRight = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    float* mixLeft = chunkMix.getWritePointer(0);
    float* mixRight = chunkMix.getWritePointer(1);

    // The voices and the colour chain work in stereo, in chunks of at most maxBlock samples.
    float peak = 0.0f;
    auto event = midi.cbegin();
    for (int chunkStart = 0; chunkStart < numSamples; chunkStart += maxBlock)
    {
        const int chunkEnd = std::min(numSamples, chunkStart + maxBlock);
        const int chunkLength = chunkEnd - chunkStart;
        chunkMix.clear(0, chunkLength);

        // Render up to each MIDI event, then apply it: notes start on the exact sample they were played.
        int cursor = chunkStart;
        const bool lastChunk = chunkEnd == numSamples;
        while (event != midi.cend() && ((*event).samplePosition < chunkEnd || lastChunk))
        {
            const auto metadata = *event;
            const int eventPosition = std::clamp(metadata.samplePosition, cursor, chunkEnd);
            renderSpan(mixLeft, mixRight, cursor - chunkStart, eventPosition - cursor, params);
            cursor = eventPosition;
            if (metadata.numBytes <= 3) // channel messages only: building a SysEx MidiMessage would allocate
                handleMidi(metadata.getMessage(), params, banks);
            ++event;
        }
        renderSpan(mixLeft, mixRight, cursor - chunkStart, chunkEnd - cursor, params);

        colour.process(mixLeft, mixRight, chunkLength, params);

        for (int i = 0; i < chunkLength; ++i)
        {
            const float g = outputGain.getNextValue();
            const float l = dsp::softClip(mixLeft[i] * g);
            const float r = dsp::softClip(mixRight[i] * g);
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            if (outRight != nullptr)
            {
                outLeft[chunkStart + i] = l;
                outRight[chunkStart + i] = r;
            }
            else
            {
                outLeft[chunkStart + i] = 0.5f * (l + r);
            }
        }
    }

    engineStatus.outputPeak.store(peak, std::memory_order_relaxed);
    engineStatus.activeVoices.store(countActiveVoices(), std::memory_order_relaxed);
}

// -------------------------------------------------------------------------------------------------
//  Rendering
// -------------------------------------------------------------------------------------------------
void KeysEngine::preparePitch(int numSamples, const EngineParams& params) noexcept
{
    // Patina's tape wow (slow, a little irregular) and flutter (fast), in cents, gliding to the
    // knob over ~50 ms. With both off, every sample just gets the Tune ratio.
    const float patina = unit(params.patina);
    // Up to ±10 cents of wow (a well-worn cassette) and ±2.5 of flutter at full Patina.
    const float wowTarget = params.wow ? patina * 10.0f : 0.0f;
    const float flutterTarget = params.flutter ? patina * 2.5f : 0.0f;
    const float follow = std::min(1.0f, static_cast<float>(numSamples / (0.05 * sampleRate)));
    const float wowFrom = wowCents, flutterFrom = flutterCents;
    wowCents += (wowTarget - wowCents) * follow;
    flutterCents += (flutterTarget - flutterCents) * follow;
    if (std::abs(wowCents - wowTarget) < 1.0e-3f)
        wowCents = wowTarget;
    if (std::abs(flutterCents - flutterTarget) < 1.0e-3f)
        flutterCents = flutterTarget;

    const float tuneRatio = static_cast<float>(dsp::semitonesToRatio(params.tuneCents / 100.0));
    const float doubleDetune = static_cast<float>(dsp::semitonesToRatio(kDoubleDetuneCents / 100.0));
    const bool wobble = wowCents > 0.0f || flutterCents > 0.0f || wowFrom > 0.0f || flutterFrom > 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float ratio = tuneRatio;
        if (wobble)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(numSamples);
            const float wow = wowFrom + (wowCents - wowFrom) * t;
            const float flutter = flutterFrom + (flutterCents - flutterFrom) * t;
            const float cents = wow * (0.75f * wowLfo.next() + 0.25f * wowDrift.next()) + flutter * flutterLfo.next();
            ratio *= std::exp2(cents / 1200.0f);
        }
        pitchRatio[static_cast<size_t>(i)] = ratio;
        doubleRatio[static_cast<size_t>(i)] = ratio * doubleDetune;
    }
}

void KeysEngine::renderSpan(float* left, float* right, int start, int numSamples, const EngineParams& params) noexcept
{
    if (numSamples <= 0)
        return;

    tineBus.clear(0, numSamples);
    bellBus.clear(0, numSamples);
    std::fill(thumpBus.begin(), thumpBus.begin() + numSamples, 0.0f);
    preparePitch(numSamples, params);

    for (auto& voice : voices)
    {
        if (voice.isActive())
            renderVoice(voice, numSamples, params);
        if (!voice.isActive())
            voice.note = -1;
    }

    for (auto& thump : thumps)
    {
        if (!thump.active)
            continue;
        for (int i = 0; i < numSamples; ++i)
        {
            if (thump.attacking)
            {
                thump.envelope += thump.attackStep;
                if (thump.envelope >= 1.0f)
                {
                    thump.envelope = 1.0f;
                    thump.attacking = false;
                }
            }
            else
            {
                thump.envelope *= thump.decay;
                if (thump.envelope < 1.0e-4f)
                {
                    thump.active = false;
                    break;
                }
            }
            thumpBus[static_cast<size_t>(i)] += thump.filter.process(noise.next()) * thump.envelope * thump.level;
        }
    }

    const float* tl = tineBus.getReadPointer(0);
    const float* tr = tineBus.getReadPointer(1);
    const float* bl = bellBus.getReadPointer(0);
    const float* br = bellBus.getReadPointer(1);
    for (int i = 0; i < numSamples; ++i)
    {
        const float tg = tineGain.getNextValue();
        const float bg = bellGain.getNextValue();
        const float mech = thumpBus[static_cast<size_t>(i)];
        left[start + i] += tl[i] * tg + bl[i] * bg + mech;
        right[start + i] += tr[i] * tg + br[i] * bg + mech;
    }
}

void KeysEngine::renderVoice(Voice& voice, int n, const EngineParams& params) noexcept
{
    float* sl = voiceScratch.getWritePointer(0);
    float* sr = voiceScratch.getWritePointer(1);

    // Body's width: low keys lean left, high keys right. A balance, so the centre keeps its level.
    const float spread = unit(params.bodyWidth) * 2.0f * unit(params.body) * 0.7f;
    const float pan = std::clamp((static_cast<float>(voice.note) - 64.0f) / 36.0f, -1.0f, 1.0f) * spread;
    const float fromLeft = voice.panLeft, fromRight = voice.panRight;
    voice.panLeft = pan > 0.0f ? 1.0f - pan : 1.0f;
    voice.panRight = pan < 0.0f ? 1.0f + pan : 1.0f;

    auto addPanned = [&](juce::AudioBuffer<float>& bus)
    {
        float* bl = bus.getWritePointer(0);
        float* br = bus.getWritePointer(1);
        if (fromLeft == 1.0f && fromRight == 1.0f && voice.panLeft == 1.0f && voice.panRight == 1.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                bl[i] += sl[i];
                br[i] += sr[i];
            }
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(n);
            bl[i] += sl[i] * (fromLeft + (voice.panLeft - fromLeft) * t);
            br[i] += sr[i] * (fromRight + (voice.panRight - fromRight) * t);
        }
    };

    // --- Tine layer: the tine (and its double), then the ding, then the bark -------------------------
    if (voice.tine.isActive() || voice.tineDouble.isActive())
    {
        std::fill(sl, sl + n, 0.0f);
        std::fill(sr, sr + n, 0.0f);
        const float doubled = unit(params.body) * unit(params.bodyDouble);
        if (voice.tine.isActive())
            voice.tine.render(sl, sr, n, pitchRatio.data(), 1.0f - 0.3f * doubled);
        if (voice.tineDouble.isActive())
            voice.tineDouble.render(sl, sr, n, doubleRatio.data(), 0.7f * doubled);

        // Tine ding: lift (or soften) what's above ~1.5 kHz during the first few milliseconds.
        const float ding = (unit(params.tineDing) - 0.5f) * 2.0f;
        if (voice.ding > 1.0e-3f)
        {
            const float decay = dsp::decayCoefficient(kDingTau, sampleRate);
            if (ding != 0.0f)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float amount = ding * voice.ding;
                    voice.ding *= decay;
                    sl[i] += amount * (sl[i] - voice.dingSplit[0].process(sl[i]));
                    sr[i] += amount * (sr[i] - voice.dingSplit[1].process(sr[i]));
                }
            }
            else
            {
                voice.ding *= std::pow(decay, static_cast<float>(n));
            }
        }

        // Bark: the pickup's asymmetric bend, deeper for harder notes. Only the bend's difference
        // is DC-blocked, so the clean signal passes untouched.
        // Treble tines growl less than the middle and bass ones: half as much at the top.
        const float treble = std::clamp((static_cast<float>(voice.note) - 60.0f) / 36.0f, 0.0f, 1.0f);
        const float bark = unit(params.bark) * (0.35f + 1.3f * unit(params.barkVelocity) * voice.velocity)
                           * (1.0f - 0.5f * treble);
        if (bark > 0.0f)
        {
            dsp::Saturator pickup;
            pickup.set(dsp::Saturator::Curve::bark, bark, voice.barkReference);
            for (int i = 0; i < n; ++i)
            {
                sl[i] += voice.barkDc[0].process(pickup.process(sl[i]) - sl[i]);
                sr[i] += voice.barkDc[1].process(pickup.process(sr[i]) - sr[i]);
            }
        }
        addPanned(tineBus);
    }

    // --- Bell layer -----------------------------------------------------------------------------------
    if (voice.bell.isActive())
    {
        std::fill(sl, sl + n, 0.0f);
        std::fill(sr, sr + n, 0.0f);
        voice.bell.render(sl, sr, n, pitchRatio.data(), 1.0f);
        addPanned(bellBus);
    }
}

void KeysEngine::triggerThump(float level, double hz, double q, double tauSeconds) noexcept
{
    if (level <= 1.0e-5f)
        return;
    Thump* slot = &thumps[0];
    for (auto& thump : thumps)
    {
        if (!thump.active)
        {
            slot = &thump;
            break;
        }
        if (thump.envelope < slot->envelope)
            slot = &thump;
    }
    slot->filter.setBandPass(hz, q, sampleRate);
    slot->filter.reset();
    slot->level = level * mechanicsScale;
    slot->envelope = 0.0f;
    slot->attackStep = static_cast<float>(1.0 / (0.002 * sampleRate));
    slot->decay = dsp::decayCoefficient(tauSeconds, sampleRate);
    slot->attacking = true;
    slot->active = true;
}

// -------------------------------------------------------------------------------------------------
//  MIDI
// -------------------------------------------------------------------------------------------------
void KeysEngine::handleMidi(const juce::MidiMessage& message, const EngineParams& params, const Banks& banks) noexcept
{
    MidiEventInfo info;
    info.channel = static_cast<uint8_t>(std::max(1, message.getChannel()));

    if (message.isNoteOn())
    {
        info.type = MidiEventInfo::Type::noteOn;
        info.number = static_cast<uint8_t>(message.getNoteNumber());
        info.value = message.getVelocity();
        engineStatus.lastNote.store(message.getNoteNumber(), std::memory_order_relaxed);
        engineStatus.lastVelocity.store(message.getVelocity(), std::memory_order_relaxed);
        noteOn(message.getNoteNumber(), message.getVelocity(), params, banks);
    }
    else if (message.isNoteOff())
    {
        info.type = MidiEventInfo::Type::noteOff;
        info.number = static_cast<uint8_t>(message.getNoteNumber());
        info.value = message.getVelocity();
        noteOff(message.getNoteNumber(), params);
    }
    else if (message.isController())
    {
        info.type = MidiEventInfo::Type::controller;
        info.number = static_cast<uint8_t>(message.getControllerNumber());
        info.value = static_cast<uint8_t>(message.getControllerValue());
        if (message.isSustainPedalOn())
            setSustain(true, params);
        else if (message.isSustainPedalOff())
            setSustain(false, params);
        else if (message.isAllNotesOff())
            allNotesOff();
        else if (message.isAllSoundOff())
        {
            for (auto& voice : voices)
                voice = Voice {};
            for (auto& thump : thumps)
                thump.active = false;
            sustainDown = false;
            engineStatus.sustainPedal.store(false);
        }
    }
    else
    {
        return; // clock, aftertouch, pitch bend...: not shown, not used
    }

    engineStatus.pushEvent(info);
    engineStatus.midiEventCount.fetch_add(1, std::memory_order_relaxed);
}

void KeysEngine::noteOn(int note, int velocity, const EngineParams& params, const Banks& banks) noexcept
{
    if (velocity <= 0)
    {
        noteOff(note, params);
        return;
    }

    // Re-striking a ringing key: the old note gets out of the way quickly, the new one starts fresh.
    for (auto& voice : voices)
        if (voice.isActive() && voice.note == note && !voice.stolen)
            releaseVoice(voice, 0.03f);

    // Touch: bend the velocity curve. Layers are chosen with the bent velocity, too.
    const float exponent = 0.45f + (1.8f - 0.45f) * unit(params.touch);
    const float curved = std::pow(static_cast<float>(velocity) / 127.0f, exponent);
    const int layerVelocity = std::clamp(static_cast<int>(std::lround(curved * 127.0f)), 1, 127);

    Voice& voice = allocateVoice();
    voice = Voice {};
    voice.note = note;
    voice.keyDown = true;
    voice.age = ++noteCounter;
    voice.velocity = curved;
    voice.ding = 1.0f;
    for (auto& split : voice.dingSplit)
        split.setCutoff(kDingSplitHz, sampleRate);
    for (auto& dc : voice.barkDc)
        dc.prepare(sampleRate);

    // Tine layer, plus Body's double: the same key one velocity layer softer, a few cents sharp.
    if (banks.tines != nullptr)
        if (const Zone* zone = banks.tines->find(note, layerVelocity))
        {
            const double reg = std::clamp((note - 28) / 72.0, 0.0, 1.0);
            const float releaseTau = static_cast<float>(0.05 + 0.10 * (1.0 - reg)); // bass dampers are slower
            startPlayer(voice.tine, *zone, note, layerVelocity, banks.tines->gain(), releaseTau);
            if (banks.tines->fortePeak() > 0.0f)
                voice.barkReference = dsp::Saturator::kReference * banks.tines->fortePeak() / kReferenceFortePeak;
            const Zone* doubleZone = banks.tines->find(note, std::max(1, layerVelocity - 24));
            startPlayer(voice.tineDouble, doubleZone != nullptr ? *doubleZone : *zone, note, layerVelocity,
                        banks.tines->gain(), releaseTau);
        }

    // Bell layer: which bell, which octave, and a fade-out below its lowest key.
    const float rangeFade = std::clamp((note - params.bellLowestKey + 6) / 6.0f, 0.0f, 1.0f);
    bool celeste = false;
    if (rangeFade > 0.0f)
    {
        const bool useVibes = params.bellVoice == BellVoice::vibes || (params.bellVoice == BellVoice::both && note < 60);
        const SampleBank* bank = useVibes ? banks.vibes : banks.celeste;
        if (bank == nullptr)
            bank = useVibes ? banks.celeste : banks.vibes;
        if (bank != nullptr)
        {
            int bellNote = note + 12 * std::clamp(params.bellOctave, 0, 1);
            // Keep the bell inside its natural range, folding by octaves (a celeste starts at middle C).
            while (bellNote < bank->lowestRoot() - 2)
                bellNote += 12;
            while (bellNote > bank->highestRoot() + 6)
                bellNote -= 12;
            if (const Zone* zone = bank->find(bellNote, layerVelocity))
            {
                startPlayer(voice.bell, *zone, bellNote, layerVelocity, bank->gain() * rangeFade,
                            bank == banks.vibes ? 0.45f : 0.18f);
                celeste = bank == banks.celeste;
            }
        }
    }

    if (!voice.isActive())
    {
        voice.note = -1;
        return;
    }

    // Mechanics: the key landing (and, on a celeste, its own little hammer).
    const float mech = unit(params.mechanics);
    triggerThump(0.7f * mech * std::pow(curved, 1.5f), 180.0, 0.9, 0.02);
    if (celeste)
        triggerThump(0.06f * mech * curved * rangeFade * bellGain.getTargetValue(), 2400.0, 1.4, 0.006);

    engineStatus.notes.push({ NoteEvent::Type::noteOn, static_cast<uint8_t>(note), static_cast<uint8_t>(layerVelocity) });
}

void KeysEngine::startPlayer(Player& player, const Zone& zone, int playedNote, int velocity, float gain,
                             float releaseTau) noexcept
{
    const Sample& sample = *zone.sample;
    player = Player {};
    player.sample = &sample;
    player.position = 0.0;
    player.baseStep = (sample.sampleRate / sampleRate) * dsp::semitonesToRatio(playedNote - sample.rootNote);
    // Softer than the layer's reference velocity: the same sample, played quieter.
    const float within = std::clamp(static_cast<float>(velocity) / std::max(1.0f, zone.refVelocity), 0.02f, 1.0f);
    player.gain = gain * within;
    player.releaseTau = releaseTau;
}

void KeysEngine::noteOff(int note, const EngineParams& params) noexcept
{
    bool damped = false;
    for (auto& voice : voices)
    {
        if (!voice.isActive() || voice.note != note || !voice.keyDown)
            continue;
        voice.keyDown = false;
        if (sustainDown)
        {
            voice.sustained = true;
        }
        else
        {
            releaseVoice(voice);
            damped = true;
        }
    }
    if (damped)
        triggerThump(0.25f * unit(params.mechanics), 320.0, 0.9, 0.03);
    engineStatus.notes.push({ NoteEvent::Type::noteOff, static_cast<uint8_t>(note), 0 });
}

void KeysEngine::setSustain(bool down, const EngineParams& params) noexcept
{
    if (down == sustainDown)
        return;
    sustainDown = down;
    engineStatus.sustainPedal.store(down, std::memory_order_relaxed);
    engineStatus.notes.push({ down ? NoteEvent::Type::pedalDown : NoteEvent::Type::pedalUp, 0, 0 });

    const float mech = unit(params.mechanics);
    if (down)
    {
        triggerThump(0.6f * mech, 120.0, 0.7, 0.05); // the dampers lift off every tine
        return;
    }

    int damped = 0;
    for (auto& voice : voices)
        if (voice.isActive() && voice.sustained && !voice.keyDown)
        {
            releaseVoice(voice);
            ++damped;
        }
    triggerThump(0.5f * mech, 90.0, 0.7, 0.08); // the pedal returns
    if (damped > 0)
        triggerThump(0.25f * mech * std::min(1.0f, 0.4f + 0.15f * static_cast<float>(damped)), 320.0, 0.9, 0.03);
}

void KeysEngine::allNotesOff() noexcept
{
    sustainDown = false;
    engineStatus.sustainPedal.store(false, std::memory_order_relaxed);
    for (auto& voice : voices)
    {
        voice.keyDown = false;
        voice.sustained = false;
        if (voice.isActive())
            releaseVoice(voice);
    }
}

void KeysEngine::releaseVoice(Voice& voice, float tauOverride) noexcept
{
    voice.keyDown = false;
    voice.sustained = false;
    for (auto* player : { &voice.tine, &voice.tineDouble, &voice.bell })
        player->release(dsp::decayCoefficient(tauOverride > 0.0f ? tauOverride : player->releaseTau, sampleRate));
}

KeysEngine::Voice& KeysEngine::allocateVoice() noexcept
{
    // Over the polyphony limit? Fade out the oldest note (released ones first) to make room.
    int sounding = 0;
    for (const auto& voice : voices)
        if (voice.isActive() && !voice.stolen)
            ++sounding;

    if (sounding >= kPolyphony)
    {
        Voice* victim = nullptr;
        for (auto& voice : voices)
        {
            if (!voice.isActive() || voice.stolen)
                continue;
            const bool better = victim == nullptr || (voice.isReleasing() && !victim->isReleasing())
                                || (voice.isReleasing() == victim->isReleasing() && voice.age < victim->age);
            if (better)
                victim = &voice;
        }
        if (victim != nullptr)
        {
            victim->stolen = true;
            releaseVoice(*victim, 0.012f);
        }
    }

    for (auto& voice : voices)
        if (!voice.isActive())
            return voice;

    // Every slot busy (only with very fast playing): reuse the oldest fading voice outright.
    Voice* oldest = &voices[0];
    for (auto& voice : voices)
        if (voice.age < oldest->age)
            oldest = &voice;
    return *oldest;
}
} // namespace keys
