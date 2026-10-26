#pragma once

#include "SampleBank.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>

// =================================================================================================
//  KeysLibrary — every sound Keys can play: the tine bank, the celeste and the vibraphone.
//
//  One library is shared by all Keys instances in a process (use it through
//  juce::SharedResourcePointer<KeysLibrary>), so ten instances in a Live set hold the samples once.
//
//  Loading happens on a background thread: rendering the demo tines and decoding the bell samples
//  takes a second or two, and the host must never wait for it. Each bank is published with an
//  atomic pointer once it is complete, and banks are never changed or freed while the library
//  lives. That is what makes reading them from the audio thread lock-free and safe.
// =================================================================================================
namespace keys
{
class KeysLibrary final : private juce::Thread
{
public:
    KeysLibrary();
    ~KeysLibrary() override;

    // [RT] Safe from any thread. nullptr until that bank has finished loading.
    const SampleBank* tines() const noexcept { return tinesBank.load(std::memory_order_acquire); }
    const SampleBank* celeste() const noexcept { return celesteBank.load(std::memory_order_acquire); }
    const SampleBank* vibes() const noexcept { return vibesBank.load(std::memory_order_acquire); }

    bool isLoading() const noexcept { return loading.load(); }

    /// Message thread: one line for the UI, e.g. "Demo tines · Celeste · Vibes".
    juce::String describe() const;

    /// Where the bell samples were found (empty if they weren't).
    juce::File getAssetsFolder() const { return assets; }

    /// Searches next to the plugin/app binary first, then the source tree of a development build.
    static juce::File findAssetsFolder();

private:
    void run() override;
    std::unique_ptr<SampleBank> loadCeleste(juce::AudioFormatManager& formats) const;
    std::unique_ptr<SampleBank> loadVibes(juce::AudioFormatManager& formats) const;

    // Owned here; written once by the loader thread *before* the matching pointer is published.
    std::unique_ptr<SampleBank> ownedTines, ownedCeleste, ownedVibes;
    std::atomic<const SampleBank*> tinesBank { nullptr }, celesteBank { nullptr }, vibesBank { nullptr };
    std::atomic<bool> loading { true }, cancel { false };
    std::atomic<bool> celesteIsModeled { false };
    juce::File assets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeysLibrary)
};
} // namespace keys
