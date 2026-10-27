#include "KeysLibrary.h"

#include "BankBuilder.h"
#include "DemoLibrary.h"
#include "SfzLoader.h"

namespace keys
{
namespace
{
// Every bank is matched by loudness (RMS over a note's first second, at velocity 90), so the
// sampled and modeled tines sound equally loud and the bell sits a little under them.
constexpr float kTineLoudness = 0.042f;
constexpr float kBellLoudness = 0.03f;
} // namespace

KeysLibrary::KeysLibrary()
    : juce::Thread("Keys library loader"),
      assets(findAssetsFolder()),
      sampledMap(findSampledTines(assets))
{
    if (sampledMap.existsAsFile())
    {
        // The set's name: its "// bankname: ..." line if it has one, else its folder's name.
        sampledName = sampledMap.getParentDirectory().getFileName();
        for (const auto& line : juce::StringArray::fromLines(sampledMap.loadFileAsString()))
            if (line.containsIgnoreCase("bankname:"))
            {
                sampledName = line.fromFirstOccurrenceOf(":", false, false).trim();
                break;
            }
    }
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
        if (folder.getChildFile("bell").isDirectory() || folder.getChildFile("tines").isDirectory())
            return folder;
    return {};
}

juce::File KeysLibrary::findSampledTines(const juce::File& assetsFolder)
{
    auto folders = assetsFolder.getChildFile("tines").findChildFiles(juce::File::findDirectories, false);
    folders.sort();
    for (const auto& folder : folders)
    {
        auto maps = folder.findChildFiles(juce::File::findFiles, false, "*.sfz");
        maps.sort();
        if (!maps.isEmpty())
            return maps.getFirst();
    }
    return {};
}

void KeysLibrary::run()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    // Whatever plays first comes first: sampled tines when there are some, else the model. The
    // bell layer joins as soon as it's decoded; the other tines last, for switching in the menu.
    if (hasSampledTines())
        loadSampledTines(formats);
    else
        loadModeledTines();
    if (!threadShouldExit())
        loadBells(formats);
    if (!threadShouldExit() && hasSampledTines())
        loadModeledTines();
    loading = false;
}

void KeysLibrary::loadSampledTines(juce::AudioFormatManager& formats)
{
    ownedSampled = loadSfzBank(sampledMap, formats, sampledName, &cancel);
    if (ownedSampled == nullptr)
        return;
    ownedSampled->normalizeLoudness(kTineLoudness); // as loud as the modeled tines
    sampledBank.store(ownedSampled.get(), std::memory_order_release);
}

void KeysLibrary::loadModeledTines()
{
    ownedModeled = buildDemoTines({}, &cancel);
    if (ownedModeled != nullptr)
        ownedModeled->normalizeLoudness(kTineLoudness);
    if (ownedModeled != nullptr)
        modeledBank.store(ownedModeled.get(), std::memory_order_release);
}

void KeysLibrary::loadBells(juce::AudioFormatManager& formats)
{
    ownedCeleste = loadCeleste(formats);
    if (ownedCeleste == nullptr && !threadShouldExit())
    {
        ownedCeleste = buildModeledBell();
        celesteIsModeled = true;
    }
    if (ownedCeleste != nullptr)
        ownedCeleste->normalizeLoudness(kBellLoudness);
    if (ownedCeleste != nullptr)
        celesteBank.store(ownedCeleste.get(), std::memory_order_release);

    if (!threadShouldExit())
        ownedVibes = loadVibes(formats);
    if (ownedVibes != nullptr)
        ownedVibes->normalizeLoudness(kBellLoudness);
    if (ownedVibes != nullptr)
        vibesBank.store(ownedVibes.get(), std::memory_order_release);
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
        {
            trimTail(*sample);
            roots.push_back({ parsed->layer, std::move(sample) });
        }
    }
    if (roots.empty())
        return nullptr;

    auto bank = std::make_unique<SampleBank>("Celeste");
    mapRootsIntoBank(*bank, std::move(roots), { 70, 127 });
    bank->finalize();
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
        {
            trimTail(*sample);
            roots.push_back({ parsed->layer, std::move(sample) });
        }
    }
    if (roots.empty())
        return nullptr;

    auto bank = std::make_unique<SampleBank>("Vibes");
    mapRootsIntoBank(*bank, std::move(roots), { 72, 127 });
    bank->finalize();
    return bank;
}

juce::String KeysLibrary::describe(bool preferSampled) const
{
    const bool sampled = preferSampled && sampledTines() != nullptr;
    if (tines(preferSampled) == nullptr)
        return "Loading tines...";

    juce::StringArray parts { sampled ? sampledName : juce::String("Demo tines") };
    if (celeste() != nullptr)
        parts.add(celesteIsModeled.load() ? "Modeled bell" : "Celeste");
    if (vibes() != nullptr)
        parts.add("Vibes");
    if (isLoading())
        parts.add("loading...");
    return parts.joinIntoString(juce::String::fromUTF8("  \xc2\xb7  "));
}
} // namespace keys
