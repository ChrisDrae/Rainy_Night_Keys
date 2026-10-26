#include "KeysEngine.h"

#include "DspMath.h"

#include <algorithm>
#include <cmath>

namespace keys
{
// =================================================================================================
//  EngineStatus
// =================================================================================================
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
//  Player: one sample being read at a (possibly fractional) rate
// =================================================================================================
void KeysEngine::Player::release(float coef) noexcept
{
    if (!isActive())
        return;
    releasing = true;
    releaseCoef = std::min(releaseCoef, coef); // a faster release always wins
}

bool KeysEngine::Player::render(float* left, float* right, int numSamples, double tuneRatio) noexcept
{
    const auto& audio = sample->audio;
    const int length = audio.getNumSamples();
    const float* l = audio.getReadPointer(0);
    const float* r = audio.getNumChannels() > 1 ? audio.getReadPointer(1) : l;
    const double step = baseStep * tuneRatio;

    for (int i = 0; i < numSamples; ++i)
    {
        const int index = static_cast<int>(position);
        if (index + 2 >= length || envelope < 1.0e-3f) // released below −60 dB: done
        {
            stop();
            return false;
        }
        const float t = static_cast<float>(position - index);
        const int prev = index > 0 ? index - 1 : 0;
        const float g = gain * envelope;
        left[i] += g * dsp::interpolateHermite(l[prev], l[index], l[index + 1], l[index + 2], t);
        right[i] += g * dsp::interpolateHermite(r[prev], r[index], r[index + 1], r[index + 2], t);
        position += step;
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
    tineGain.reset(sampleRate, 0.03);
    bellGain.reset(sampleRate, 0.03);
    outputGain.reset(sampleRate, 0.03);
    for (auto& voice : voices)
        voice = Voice {};
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
    const float blend = std::clamp(params.blend, 0.0f, 1.0f);
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
    const double tuneRatio = dsp::semitonesToRatio(params.tuneCents / 100.0);

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    // Render up to each MIDI event, then apply it: notes start on the exact sample they were played.
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPosition = std::clamp(metadata.samplePosition, 0, numSamples);
        renderSpan(left, right, position, eventPosition - position, tuneRatio);
        position = eventPosition;
        if (metadata.numBytes <= 3) // channel messages only: building a SysEx MidiMessage would allocate
            handleMidi(metadata.getMessage(), params, banks);
    }
    renderSpan(left, right, position, numSamples - position, tuneRatio);

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max(peak, buffer.getMagnitude(ch, 0, numSamples));
    engineStatus.outputPeak.store(peak, std::memory_order_relaxed);
    engineStatus.activeVoices.store(countActiveVoices(), std::memory_order_relaxed);
}

void KeysEngine::renderSpan(float* left, float* right, int start, int numSamples, double tuneRatio) noexcept
{
    while (numSamples > 0)
    {
        const int chunk = std::min(numSamples, maxBlock);
        tineBus.clear(0, chunk);
        bellBus.clear(0, chunk);
        float* tl = tineBus.getWritePointer(0);
        float* tr = tineBus.getWritePointer(1);
        float* bl = bellBus.getWritePointer(0);
        float* br = bellBus.getWritePointer(1);

        for (auto& voice : voices)
        {
            if (voice.tine.isActive())
                voice.tine.render(tl, tr, chunk, tuneRatio);
            if (voice.bell.isActive())
                voice.bell.render(bl, br, chunk, tuneRatio);
            if (!voice.isActive())
                voice.note = -1;
        }

        for (int i = 0; i < chunk; ++i)
        {
            const float tg = tineGain.getNextValue();
            const float bg = bellGain.getNextValue();
            const float og = outputGain.getNextValue();
            const float l = dsp::softClip((tl[i] * tg + bl[i] * bg) * og);
            const float r = dsp::softClip((tr[i] * tg + br[i] * bg) * og);
            if (right != nullptr)
            {
                left[start + i] = l;
                right[start + i] = r;
            }
            else
            {
                left[start + i] = 0.5f * (l + r);
            }
        }
        start += chunk;
        numSamples -= chunk;
    }
}

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
        noteOff(message.getNoteNumber());
    }
    else if (message.isController())
    {
        info.type = MidiEventInfo::Type::controller;
        info.number = static_cast<uint8_t>(message.getControllerNumber());
        info.value = static_cast<uint8_t>(message.getControllerValue());
        if (message.isSustainPedalOn())
            setSustain(true);
        else if (message.isSustainPedalOff())
            setSustain(false);
        else if (message.isAllNotesOff())
            allNotesOff();
        else if (message.isAllSoundOff())
        {
            for (auto& voice : voices)
                voice = Voice {};
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
        noteOff(note);
        return;
    }

    // Re-striking a ringing key: the old note gets out of the way quickly, the new one starts fresh.
    for (auto& voice : voices)
        if (voice.isActive() && voice.note == note && !voice.stolen)
            releaseVoice(voice, 0.03f);

    // Touch: bend the velocity curve. Layers are chosen with the bent velocity, too.
    const float exponent = 0.45f + (1.8f - 0.45f) * std::clamp(params.touch, 0.0f, 1.0f);
    const float curved = std::pow(static_cast<float>(velocity) / 127.0f, exponent);
    const int layerVelocity = std::clamp(static_cast<int>(std::lround(curved * 127.0f)), 1, 127);

    Voice& voice = allocateVoice();
    voice = Voice {};
    voice.note = note;
    voice.keyDown = true;
    voice.age = ++noteCounter;

    // Tine layer.
    if (banks.tines != nullptr)
        if (const Zone* zone = banks.tines->find(note, layerVelocity))
        {
            const double reg = std::clamp((note - 28) / 72.0, 0.0, 1.0);
            const float releaseTau = static_cast<float>(0.05 + 0.10 * (1.0 - reg)); // bass dampers are slower
            startPlayer(voice.tine, *zone, note, layerVelocity, banks.tines->gain(), releaseTau);
        }

    // Bell layer: which bell, which octave, and a fade-out below its lowest key.
    const float rangeFade = std::clamp((note - params.bellLowestKey + 6) / 6.0f, 0.0f, 1.0f);
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
                startPlayer(voice.bell, *zone, bellNote, layerVelocity, bank->gain() * rangeFade,
                            bank == banks.vibes ? 0.45f : 0.18f);
        }
    }

    if (!voice.isActive())
        voice.note = -1;
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

void KeysEngine::noteOff(int note) noexcept
{
    for (auto& voice : voices)
    {
        if (!voice.isActive() || voice.note != note || !voice.keyDown)
            continue;
        voice.keyDown = false;
        if (sustainDown)
            voice.sustained = true;
        else
            releaseVoice(voice);
    }
}

void KeysEngine::setSustain(bool down) noexcept
{
    sustainDown = down;
    engineStatus.sustainPedal.store(down, std::memory_order_relaxed);
    if (down)
        return;
    for (auto& voice : voices)
        if (voice.isActive() && voice.sustained && !voice.keyDown)
            releaseVoice(voice);
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
    voice.tine.release(dsp::decayCoefficient(tauOverride > 0.0f ? tauOverride : voice.tine.releaseTau, sampleRate));
    voice.bell.release(dsp::decayCoefficient(tauOverride > 0.0f ? tauOverride : voice.bell.releaseTau, sampleRate));
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
