#include "engine/KeysLibrary.h"
#include "engine/SfzLoader.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace keys;

namespace
{
/// Writes a short mono sine (then silence) as a 16-bit WAV.
void writeTone(const juce::File& file, double hz, double seconds, double silence = 0.0)
{
    const double rate = 44100.0;
    juce::AudioBuffer<float> buffer(1, static_cast<int>((seconds + silence) * rate));
    buffer.clear();
    for (int i = 0; i < static_cast<int>(seconds * rate); ++i)
        buffer.setSample(0, i, 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265 * hz * i / rate)));
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(rate).withNumChannels(1).withBitsPerSample(16));
    REQUIRE(writer != nullptr);
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}
} // namespace

TEST_CASE("SFZ regions inherit their group's opcodes and read note names", "[sfz]")
{
    const auto regions = parseSfz(R"(
        // a comment
        <control> default_path=samples/
        <global> hivel=127
        <group> lovel=1 hivel=64 ampeg_release=0.3
        <region> sample=E2 soft.flac lokey=C2 hikey=F#2 pitch_keycenter=E2
        <region>sample=A2_soft.flac lokey=g2 hikey=Bb2 pitch_keycenter=45 /* block */
        <group> lovel=65
        <region> sample=x.wav key=c4
        <curve> v000=0 v127=1
        <region> lokey=1 hikey=2
    )");
    REQUIRE(regions.size() == 3); // the last region has no sample
    CHECK(regions[0].sample == "samples/E2 soft.flac");
    CHECK(regions[0].loKey == 36);
    CHECK(regions[0].hiKey == 42);
    CHECK(regions[0].keyCenter == 40);
    CHECK(regions[0].hiVel == 64);
    CHECK(regions[1].loKey == 43);
    CHECK(regions[1].hiKey == 46);
    CHECK(regions[1].keyCenter == 45);
    CHECK(regions[2].loKey == 60);
    CHECK(regions[2].hiKey == 60);
    CHECK(regions[2].keyCenter == 60);
    CHECK(regions[2].loVel == 65);
    CHECK(regions[2].hiVel == 127); // from <global>
}

TEST_CASE("An SFZ map loads into a bank, sharing samples between regions", "[sfz]")
{
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("KeysSfzTest");
    folder.deleteRecursively();
    writeTone(folder.getChildFile("wav/low.wav"), 110.0, 0.2, 1.0);
    writeTone(folder.getChildFile("wav/high.wav"), 440.0, 0.2);
    const auto map = folder.getChildFile("test.sfz");
    map.replaceWithText(R"(<control> default_path=wav/
        <group> lovel=1 hivel=80
        <region> sample=low.wav lokey=0 hikey=60 pitch_keycenter=A2
        <region> sample=high.wav lokey=61 hikey=127 pitch_keycenter=A4
        <group> lovel=81 hivel=127
        <region> sample=low.wav lokey=0 hikey=60 pitch_keycenter=A2
        <region> sample=missing.wav lokey=61 hikey=127 pitch_keycenter=A4
    )");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto bank = loadSfzBank(map, formats, "test");
    REQUIRE(bank != nullptr);
    CHECK(bank->getNumSamples() == 2);   // low.wav once, high.wav once, the missing file skipped
    CHECK(bank->getZones().size() == 3);

    const Zone* soft = bank->find(45, 40);
    REQUIRE(soft != nullptr);
    CHECK(soft->sample->rootNote == 45);
    CHECK(soft->refVelocity == 80.0f);
    CHECK(bank->find(72, 100)->sample->rootNote == 69); // no loud high sample: the soft one stands in

    // The second of silence after the tone was trimmed away (keeping a 50 ms tail).
    CHECK(soft->sample->audio.getNumSamples() < static_cast<int>(0.3 * 44100));
    folder.deleteRecursively();
}

TEST_CASE("The fetched Rhodes set covers the whole keyboard", "[sfz]")
{
    const auto map = KeysLibrary::findSampledTines(KeysLibrary::findAssetsFolder());
    if (!map.existsAsFile())
    {
        WARN("No sampled tines (run scripts/fetch-rhodes-samples.ps1): skipped");
        return;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto bank = loadSfzBank(map, formats, "rhodes");
    REQUIRE(bank != nullptr);
    CHECK(bank->getNumSamples() >= 60);
    for (int note = 28; note <= 100; ++note)
        for (int velocity : { 1, 40, 64, 90, 110, 127 })
        {
            const Zone* zone = bank->find(note, velocity);
            REQUIRE(zone != nullptr);
            CHECK(std::abs(zone->sample->rootNote - note) <= 4); // never stretched far
            CHECK(velocity >= zone->loVel);
            CHECK(velocity <= zone->hiVel);
        }
    size_t bytes = bank->memoryBytes();
    WARN("Rhodes set in memory: " << bytes / (1024 * 1024) << " MB in " << bank->getNumSamples() << " samples");
}

