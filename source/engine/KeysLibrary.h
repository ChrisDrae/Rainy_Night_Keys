#pragma once

#include "SampleBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>

// =================================================================================================
//  KeysLibrary — every sound Keys can play: the tines (sampled and modeled), the celeste and the
//  vibraphone.
//
//  One library is shared by all Keys instances in a process (use it through
//  juce::SharedResourcePointer<KeysLibrary>), so ten instances in a Live set hold the samples once.
//
//  The tines come in two kinds. *Sampled* tines are a real Rhodes, read from an SFZ map in
//  assets/tines/<name>/ (scripts/fetch-rhodes-samples.ps1 fetches one; your own recordings go
//  there too). *Modeled* tines are rendered from the physical model, so Keys always plays.
//
//  Loading happens on a background thread: decoding samples and rendering the model takes a second
//  or two, and the host must never wait for it. Each bank is published with an atomic pointer once
//  it is complete, and banks are never changed or freed while the library lives. That is what makes
//  reading them from the audio thread lock-free and safe.
// =================================================================================================
namespace keys
{
class KeysLibrary final : private juce::Thread
{
public:
    KeysLibrary();
    ~KeysLibrary() override;

    // [RT] Safe from any thread. nullptr until that bank has finished loading.
    const SampleBank* modeledTines() const noexcept { return modeledBank.load(std::memory_order_acquire); }
    const SampleBank* sampledTines() const noexcept { return sampledBank.load(std::memory_order_acquire); }
    const SampleBank* celeste() const noexcept { return celesteBank.load(std::memory_order_acquire); }
    const SampleBank* vibes() const noexcept { return vibesBank.load(std::memory_order_acquire); }

    /// [RT] The tines to play: the sampled ones when wanted and loaded, otherwise the modeled ones.
    const SampleBank* tines(bool preferSampled) const noexcept
    {
        const SampleBank* sampled = sampledTines();
        return preferSampled && sampled != nullptr ? sampled : modeledTines();
    }

    bool isLoading() const noexcept { return loading.load(); }

    /// True when a sampled tine map was found on disk (it may still be loading).
    bool hasSampledTines() const noexcept { return sampledMap.existsAsFile(); }
    /// The sampled set's name, from its SFZ ("// bankname: ...") or its folder.
    juce::String getSampledName() const { return sampledName; }

    /// Message thread: one line for the UI, e.g. "jRhodes3d · Celeste · Vibes".
    juce::String describe(bool preferSampled) const;

    /// Where the samples were found (empty if they weren't).
    juce::File getAssetsFolder() const { return assets; }

    /// Searches next to the plugin/app binary first, then the source tree of a development build.
    static juce::File findAssetsFolder();
    /// The first .sfz in assets/tines/<any folder>/, or an empty File.
    static juce::File findSampledTines(const juce::File& assetsFolder);

private:
    void run() override;
    void loadSampledTines(juce::AudioFormatManager& formats);
    void loadModeledTines();
    void loadBells(juce::AudioFormatManager& formats);
    std::unique_ptr<SampleBank> loadCeleste(juce::AudioFormatManager& formats) const;
    std::unique_ptr<SampleBank> loadVibes(juce::AudioFormatManager& formats) const;

    // Owned here; written once by the loader thread *before* the matching pointer is published.
    std::unique_ptr<SampleBank> ownedModeled, ownedSampled, ownedCeleste, ownedVibes;
    std::atomic<const SampleBank*> modeledBank { nullptr }, sampledBank { nullptr }, celesteBank { nullptr },
        vibesBank { nullptr };
    std::atomic<bool> loading { true }, cancel { false };
    std::atomic<bool> celesteIsModeled { false };
    juce::File assets, sampledMap;
    juce::String sampledName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysLibrary)
};
} // namespace keys
