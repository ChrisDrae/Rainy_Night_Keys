// KeysRender: plays a phrase through every factory preset and writes one WAV per preset.
//
//     KeysRender.exe <folder> [--tines sampled|modeled] [--preset <n>] [--set <id>=<value>]...
//
// The phrase is the Nocturne demo's (Gm9, C13, Fmaj9, Dm9, a little melody, Bbmaj7 at 72 bpm, with
// the pedal changed on every chord). For each preset it prints the level (RMS and peak), how often
// the output safety limiter had to round off a peak, and how bright the result is, so presets can
// be compared and balanced by numbers as well as by ear.

#include "PluginProcessor.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

struct Note
{
    double start, length;
    int note;
    float velocity;
};

std::vector<Note> phrase(double& totalSeconds, std::vector<double>& pedalChanges)
{
    const double b = 60.0 / 72.0;
    std::vector<Note> notes;
    auto chord = [&](double t, std::initializer_list<int> keys, double dur, float vel = 0.62f)
    {
        int i = 0;
        const int last = static_cast<int>(keys.size()) - 1;
        for (int k : keys)
        {
            notes.push_back({ t + i * 0.02, dur - i * 0.02, k, vel + (i == last ? 0.1f : 0.0f) });
            ++i;
        }
        pedalChanges.push_back(t);
    };
    chord(0.0, { 43, 58, 62, 65, 69 }, 2 * b);        // Gm9
    chord(2 * b, { 48, 58, 64, 69, 74 }, 2 * b);      // C13
    chord(4 * b, { 41, 57, 64, 67, 72 }, 2 * b, 0.58f); // Fmaj9
    chord(6 * b, { 50, 60, 65, 69, 76 }, 2 * b, 0.6f);  // Dm9
    for (auto [t, n, d] : { std::tuple { 7.5, 79, 0.5 }, { 8.0, 81, 0.5 }, { 8.5, 84, 1.0 }, { 9.5, 76, 0.5 } })
        notes.push_back({ t * b, d * b, n, 0.7f });
    chord(10 * b, { 46, 57, 62, 65, 69 }, 4 * b, 0.55f); // Bbmaj7
    notes.push_back({ 10 * b, 4 * b, 77, 0.72f });
    totalSeconds = 14 * b + 4.0; // and let it ring out
    return notes;
}

double toDb(double gain)
{
    return 20.0 * std::log10(std::max(gain, 1.0e-9));
}
} // namespace

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf("usage: KeysRender <folder> [--tines sampled|modeled] [--preset <n>]\n");
        return 1;
    }
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    folder.createDirectory();
    bool sampled = true;
    int only = -1;
    juce::StringPairArray settings; // applied on top of each preset
    for (int i = 2; i + 1 < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--tines")
            sampled = juce::String(argv[++i]) != "modeled";
        else if (arg == "--preset")
            only = juce::String(argv[++i]).getIntValue();
        else if (arg == "--set")
        {
            const juce::String pair(argv[++i]);
            settings.set(pair.upToFirstOccurrenceOf("=", false, false), pair.fromFirstOccurrenceOf("=", false, false));
        }
    }

    KeysProcessor processor;
    processor.setPlayConfigDetails(0, 2, kRate, kBlock);
    for (int i = 0; i < 400 && processor.getLibrary().isLoading(); ++i)
        juce::Thread::sleep(50);
    processor.setSampledTines(sampled);
    std::printf("tines: %s\n\n", processor.getLibrary().describe(sampled).toRawUTF8());
    std::printf("%-15s %8s %8s %8s %9s %10s\n", "preset", "RMS dB", "peak dB", "loud dB", "limited", "centroid");

    double totalSeconds = 0.0;
    std::vector<double> pedalChanges;
    const auto notes = phrase(totalSeconds, pedalChanges);
    const int totalSamples = static_cast<int>(totalSeconds * kRate);

    auto& presets = processor.getPresets();
    for (int p = 0; p < presets.getNumFactoryPresets(); ++p)
    {
        if (only >= 0 && p != only)
            continue;
        presets.load(p, false);
        for (const auto& id : settings.getAllKeys())
            if (auto* parameter = processor.getState().getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(settings[id].getFloatValue()));
        processor.prepareToPlay(kRate, kBlock);

        // Every MIDI event, in time order.
        juce::MidiBuffer events;
        auto at = [](double seconds) { return static_cast<int>(seconds * kRate); };
        for (const auto& n : notes)
        {
            events.addEvent(juce::MidiMessage::noteOn(1, n.note, static_cast<juce::uint8>(juce::jlimit(1, 127, juce::roundToInt(n.velocity * 127.0f)))), at(n.start));
            events.addEvent(juce::MidiMessage::noteOff(1, n.note), at(n.start + n.length));
        }
        for (size_t i = 0; i < pedalChanges.size(); ++i)
        {
            if (i > 0)
                events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), at(pedalChanges[i] - 0.01));
            events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), at(pedalChanges[i] + 0.05));
        }
        events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), at(totalSeconds - 2.0));

        juce::AudioBuffer<float> out(2, totalSamples);
        juce::AudioBuffer<float> block(2, kBlock);
        for (int start = 0; start < totalSamples; start += kBlock)
        {
            const int n = std::min(kBlock, totalSamples - start);
            juce::MidiBuffer midi;
            midi.addEvents(events, start, n, -start);
            block.setSize(2, n, false, false, true);
            processor.processBlock(block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom(ch, start, block, ch, 0, n);
        }

        // Numbers: overall RMS, peak, the loudest 400 ms window, time spent above the limiter's knee,
        // and the spectral centroid of the whole render (a rough "brightness").
        double sum = 0.0, peak = 0.0, loudest = 0.0, limited = 0.0;
        const int window = static_cast<int>(0.4 * kRate);
        std::vector<double> windowSums(static_cast<size_t>(totalSamples / window + 1), 0.0);
        for (int i = 0; i < totalSamples; ++i)
        {
            const double l = out.getSample(0, i), r = out.getSample(1, i);
            const double m = 0.5 * (l * l + r * r);
            sum += m;
            windowSums[static_cast<size_t>(i / window)] += m;
            peak = std::max({ peak, std::abs(l), std::abs(r) });
            if (std::abs(l) > 0.8 || std::abs(r) > 0.8)
                limited += 1.0;
        }
        for (double w : windowSums)
            loudest = std::max(loudest, w / window);

        juce::dsp::FFT fft(14);
        const int size = 1 << 14;
        std::vector<float> spectrum(static_cast<size_t>(size) * 2);
        std::vector<double> power(static_cast<size_t>(size / 2), 0.0);
        for (int start = 0; start + size <= totalSamples; start += size)
        {
            std::fill(spectrum.begin(), spectrum.end(), 0.0f);
            for (int i = 0; i < size; ++i)
            {
                const float hann = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * i / size);
                spectrum[static_cast<size_t>(i)] = hann * 0.5f * (out.getSample(0, start + i) + out.getSample(1, start + i));
            }
            fft.performFrequencyOnlyForwardTransform(spectrum.data());
            for (int k = 0; k < size / 2; ++k)
                power[static_cast<size_t>(k)] += static_cast<double>(spectrum[static_cast<size_t>(k)]) * spectrum[static_cast<size_t>(k)];
        }
        double weighted = 0.0, total = 0.0;
        for (int k = 1; k < size / 2; ++k)
        {
            weighted += power[static_cast<size_t>(k)] * k * kRate / size;
            total += power[static_cast<size_t>(k)];
        }

        const auto name = presets.getPreset(p).name;
        std::printf("%-15s %8.1f %8.1f %8.1f %8.2f%% %8.0f Hz\n", name.toRawUTF8(), toDb(std::sqrt(sum / totalSamples)),
                    toDb(peak), toDb(std::sqrt(loudest)), 100.0 * limited / totalSamples, weighted / std::max(1.0e-12, total));

        const auto file = folder.getChildFile(juce::String(p + 1).paddedLeft('0', 2) + " " + name + (sampled ? "" : " (modeled)") + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
        if (auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(kRate).withNumChannels(2).withBitsPerSample(24)))
            writer->writeFromAudioSampleBuffer(out, 0, totalSamples);
    }
    std::printf("\nwrote %s\n", folder.getFullPathName().toRawUTF8());
    return 0;
}
