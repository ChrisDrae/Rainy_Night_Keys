// KeysUiSnapshot: renders the plugin window into a PNG without a host.
//
//     KeysUiSnapshot.exe <out.png> [--demo] [--scale <factor>] [--set <id>=<value>]...
//
// With --demo, a short chord is played through the processor first, so the MIDI monitor and the
// keyboard show something. --set changes a parameter first (plain value, e.g. --set blend=0.8,
// --set bellVoice=1). The docs' screenshots are made with this tool.

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::printf("usage: KeysUiSnapshot <out.png> [--demo] [--scale <factor>] [--set <id>=<value>]...\n");
        return 1;
    }
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    bool demo = false;
    float scale = 2.0f;
    juce::StringPairArray settings;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--demo")
            demo = true;
        else if (arg == "--scale" && i + 1 < argc)
            scale = juce::jlimit(0.5f, 4.0f, juce::String(argv[++i]).getFloatValue());
        else if (arg == "--set" && i + 1 < argc)
        {
            const juce::String pair(argv[++i]);
            settings.set(pair.upToFirstOccurrenceOf("=", false, false), pair.fromFirstOccurrenceOf("=", false, false));
        }
    }

    auto processor = std::make_unique<KeysProcessor>();
    processor->prepareToPlay(48000.0, 512);
    for (const auto& id : settings.getAllKeys())
        if (auto* parameter = processor->getState().getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(settings[id].getFloatValue()));

    // Give the library a moment to render the tines.
    for (int i = 0; i < 100 && processor->getLibrary().isLoading(); ++i)
        juce::Thread::sleep(50);

    if (demo)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        for (int note : { 41, 57, 64, 67, 72 })
            midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(96)), 0);
        processor->processBlock(buffer, midi);
        for (int b = 0; b < 20; ++b)
        {
            juce::MidiBuffer none;
            processor->processBlock(buffer, none);
        }
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
        editor->setVisible(true);
        // Let the editor's timer run a few times so the header and monitor are filled in.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        out.deleteFile();
        juce::FileOutputStream stream(out);
        juce::PNGImageFormat png;
        if (!stream.openedOk() || !png.writeImageToStream(image, stream))
        {
            std::printf("could not write %s\n", out.getFullPathName().toRawUTF8());
            return 1;
        }
        std::printf("wrote %s\n", out.getFullPathName().toRawUTF8());
    }
    processor.reset();
    return 0;
}
