#include "SfzLoader.h"

#include "BankBuilder.h"
#include "DspMath.h"

#include <map>

namespace keys
{
namespace
{
using Opcodes = std::map<juce::String, juce::String>;

/// Removes // line comments and /* block */ comments, and preprocessor lines (#define...).
juce::String stripComments(const juce::String& text)
{
    juce::String out;
    out.preallocateBytes(static_cast<size_t>(text.getNumBytesAsUTF8()));
    for (auto line : juce::StringArray::fromLines(text))
    {
        if (line.trimStart().startsWithChar('#'))
            continue;
        const int comment = line.indexOf("//");
        out << (comment >= 0 ? line.substring(0, comment) : line) << "\n";
    }
    while (true)
    {
        const int start = out.indexOf("/*");
        if (start < 0)
            break;
        const int end = out.indexOf(start + 2, "*/");
        out = out.substring(0, start) + " " + (end < 0 ? juce::String() : out.substring(end + 2));
    }
    return out;
}

/// A key: a MIDI number ("41") or a note name ("F2", "Ab1", "c#4"). Returns -1 if unreadable.
int parseKey(const juce::String& value)
{
    const auto v = value.trim();
    if (v.isEmpty())
        return -1;
    if (v.containsOnly("-0123456789"))
        return juce::jlimit(-1, 127, v.getIntValue());
    return parseNoteName(v.toStdString());
}
} // namespace

std::vector<SfzRegion> parseSfz(const juce::String& text)
{
    // Headers may touch their first opcode ("<region>sample=x"): give them room before splitting.
    auto spaced = stripComments(text).replace("<", " <").replace(">", "> ");
    juce::StringArray tokens;
    tokens.addTokens(spaced, " \t\r\n", "");
    tokens.removeEmptyStrings();

    Opcodes control, global, master, group, region, ignored;
    Opcodes* current = &ignored;
    bool inRegion = false;
    juce::String lastKey;
    std::vector<SfzRegion> regions;

    auto finishRegion = [&]
    {
        if (!inRegion)
            return;
        inRegion = false;
        Opcodes merged = global;
        for (const auto* level : { &master, &group, &region })
            for (const auto& [k, v] : *level)
                merged[k] = v;
        const auto sample = merged["sample"].trim();
        if (sample.isEmpty())
            return;

        SfzRegion r;
        r.sample = (control["default_path"].trim() + sample).replaceCharacter('\\', '/');
        if (const int key = parseKey(merged["key"]); key >= 0)
            r.loKey = r.hiKey = r.keyCenter = key;
        if (const int lo = parseKey(merged["lokey"]); lo >= 0)
            r.loKey = lo;
        if (const int hi = parseKey(merged["hikey"]); hi >= 0)
            r.hiKey = hi;
        if (const int centre = parseKey(merged["pitch_keycenter"]); centre >= 0)
            r.keyCenter = centre;
        if (merged.count("lovel") > 0)
            r.loVel = juce::jlimit(1, 127, merged["lovel"].getIntValue());
        if (merged.count("hivel") > 0)
            r.hiVel = juce::jlimit(1, 127, merged["hivel"].getIntValue());
        if (r.loKey <= r.hiKey && r.loVel <= r.hiVel)
            regions.push_back(r);
    };

    for (const auto& token : tokens)
    {
        if (token.startsWithChar('<') && token.endsWithChar('>'))
        {
            finishRegion();
            const auto header = token.substring(1, token.length() - 1).toLowerCase();
            lastKey = {};
            if (header == "control")
                current = &control;
            else if (header == "global")
            {
                global.clear();
                master.clear();
                group.clear();
                current = &global;
            }
            else if (header == "master")
            {
                master.clear();
                group.clear();
                current = &master;
            }
            else if (header == "group")
            {
                group.clear();
                current = &group;
            }
            else if (header == "region")
            {
                region.clear();
                current = &region;
                inRegion = true;
            }
            else
            {
                ignored.clear(); // <curve>, <effect>, <midi>...: not ours
                current = &ignored;
            }
            continue;
        }

        const int equals = token.indexOfChar('=');
        if (equals > 0)
        {
            lastKey = token.substring(0, equals).trim();
            (*current)[lastKey] = token.substring(equals + 1);
        }
        else if (lastKey.isNotEmpty())
        {
            (*current)[lastKey] << " " << token; // a value with spaces, e.g. a sample's file name
        }
    }
    finishRegion();
    return regions;
}

void trimTail(Sample& sample, float floorDb)
{
    auto& audio = sample.audio;
    const int length = audio.getNumSamples();
    const float peak = length > 0 ? audio.getMagnitude(0, length) : 0.0f;
    if (peak <= 0.0f)
        return;

    const float floor = peak * dsp::dbToGain(floorDb);
    int last = 0;
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        const float* x = audio.getReadPointer(ch);
        for (int i = length - 1; i > last; --i)
            if (std::abs(x[i]) > floor)
            {
                last = i;
                break;
            }
    }

    const int keep = std::min(length, last + static_cast<int>(0.05 * sample.sampleRate) + 1);
    if (keep >= length)
    {
        // The recording stops while the note still rings (long bass notes often do): fade out its
        // last half second, so a held note ends softly instead of with a cut.
        const int fade = std::min(length / 10, static_cast<int>(0.5 * sample.sampleRate));
        audio.applyGainRamp(length - fade, fade, 1.0f, 0.0f);
        return;
    }
    const int fade = std::min(keep, static_cast<int>(0.01 * sample.sampleRate));
    juce::AudioBuffer<float> trimmed(audio.getNumChannels(), keep);
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        trimmed.copyFrom(ch, 0, audio, ch, 0, keep);
    trimmed.applyGainRamp(keep - fade, fade, 1.0f, 0.0f);
    audio = std::move(trimmed);
}

std::unique_ptr<SampleBank> loadSfzBank(const juce::File& sfzFile, juce::AudioFormatManager& formats,
                                        const juce::String& bankName, const std::atomic<bool>* cancel)
{
    const auto regions = parseSfz(sfzFile.loadFileAsString());
    if (regions.empty())
        return nullptr;

    auto bank = std::make_unique<SampleBank>(bankName);
    std::map<std::pair<juce::String, int>, const Sample*> loaded; // (file, root) -> sample
    for (const auto& r : regions)
    {
        if (cancel != nullptr && cancel->load())
            return nullptr;

        const Sample*& sample = loaded[{ r.sample, r.keyCenter }];
        if (sample == nullptr)
        {
            auto fresh = loadSampleFile(formats, sfzFile.getParentDirectory().getChildFile(r.sample), r.keyCenter);
            if (fresh == nullptr)
                continue;
            trimTail(*fresh);
            sample = &bank->addSample(std::move(fresh));
        }

        Zone zone;
        zone.sample = sample;
        zone.loNote = r.loKey;
        zone.hiNote = r.hiKey;
        zone.loVel = r.loVel;
        zone.hiVel = r.hiVel;
        zone.refVelocity = static_cast<float>(r.hiVel);
        bank->addZone(zone);
    }

    if (bank->getZones().empty())
        return nullptr;
    bank->finalize();
    return bank;
}
} // namespace keys
