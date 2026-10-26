#include "KeysLibrary.h"

#include "BankBuilder.h"
#include "DemoLibrary.h"

namespace keys
{
KeysLibrary::KeysLibrary() : juce::Thread("Keys library loader"), assets(findAssetsFolder())
{
    startThread(juce::Thread::Priority::normal);
}

KeysLibrary::~KeysLibrary()
{
    cancel = true;
    stopThread(10000);
}

juce::File KeysLibrary::findAssetsFolder()
{
    // 1) Installed: a "KeysAssets" folder next to the app / plugin binary, or inside the bundle.
    const auto binary = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    const juce::File candidates[] = {
        binary.getSiblingFile("KeysAssets"),
        binary.getChildFile("Contents").getChildFile("Resources").getChildFile("KeysAssets"),
        binary.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("KeysAssets"),
#ifdef KEYS_DEV_ASSETS_DIR
        // 2) Development build: the repository's assets folder.
        juce::File(KEYS_DEV_ASSETS_DIR),
#endif
    };
    for (const auto& folder : candidates)
        if (folder.getChildFile("bell").isDirectory())
            return folder;
    return {};
}

void KeysLibrary::run()
{
    // Tines first: they're the instrument. The bell layer joins as soon as it's decoded.
    DemoTineOptions options;
    ownedTines = buildDemoTines(options, &cancel);
    if (ownedTines == nullptr || threadShouldExit())
    {
        loading = false;
        return;
    }
    tinesBank.store(ownedTines.get(), std::memory_order_release);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    ownedCeleste = loadCeleste(formats);
    if (ownedCeleste == nullptr && !threadShouldExit())
    {
        ownedCeleste = buildModeledBell();
        celesteIsModeled = true;
    }
    if (ownedCeleste != nullptr)
        celesteBank.store(ownedCeleste.get(), std::memory_order_release);

    if (!threadShouldExit())
        ownedVibes = loadVibes(formats);
    if (ownedVibes != nullptr)
        vibesBank.store(ownedVibes.get(), std::memory_order_release);

    loading = false;
}

std::unique_ptr<SampleBank> KeysLibrary::loadCeleste(juce::AudioFormatManager& formats) const
{
    const auto folder = assets.getChildFile("bell").getChildFile("celeste");
    if (!folder.isDirectory())
        return nullptr;

    std::vector<RootSample> roots;
    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.flac;*.wav"))
    {
        if (threadShouldExit())
            return nullptr;
        const auto parsed = parseCelesteFileName(file.getFileName().toStdString());
        if (!parsed)
            continue; // e.g. celeste-mech.flac (mechanical noise, used later)
        if (auto sample = loadSampleFile(formats, file, parsed->note))
            roots.push_back({ parsed->layer, std::move(sample) });
    }
    if (roots.empty())
        return nullptr;

    auto bank = std::make_unique<SampleBank>("Celeste");
    mapRootsIntoBank(*bank, std::move(roots), { 70, 127 });
    bank->finalize();
    bank->normalizeTo(0.22f);
    return bank;
}

std::unique_ptr<SampleBank> KeysLibrary::loadVibes(juce::AudioFormatManager& formats) const
{
    const auto folder = assets.getChildFile("bell").getChildFile("vibes");
    if (!folder.isDirectory())
        return nullptr;

    std::vector<RootSample> roots;
    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.wav;*.flac"))
    {
        if (threadShouldExit())
            return nullptr;
        const auto parsed = parseVcslFileName(file.getFileName().toStdString());
        if (!parsed)
            continue;
        if (auto sample = loadSampleFile(formats, file, parsed->note))
            roots.push_back({ parsed->layer, std::move(sample) });
    }
    if (roots.empty())
        return nullptr;

    auto bank = std::make_unique<SampleBank>("Vibes");
    mapRootsIntoBank(*bank, std::move(roots), { 72, 127 });
    bank->finalize();
    bank->normalizeTo(0.22f);
    return bank;
}

juce::String KeysLibrary::describe() const
{
    if (tines() == nullptr)
        return "Loading tines...";

    juce::StringArray parts { "Demo tines" };
    if (celeste() != nullptr)
        parts.add(celesteIsModeled.load() ? "Modeled bell" : "Celeste");
    if (vibes() != nullptr)
        parts.add("Vibes");
    if (isLoading())
        parts.add("loading...");
    return parts.joinIntoString(juce::String::fromUTF8("  \xc2\xb7  "));
}
} // namespace keys
