// KeysUiSnapshot: renders the plugin window into a PNG without a host.
//
//     KeysUiSnapshot.exe <out.png> [--demo] [--keys] [--menu] [--fine <warmth|bark|body|space>]
//                       [--preset <n>] [--annotate] [--bench] [--scale <factor>] [--set <id>=<value>]...
//
// With --demo, a short chord is played through the processor first, so the light blooms and the
// keyboard shows the notes. --keys shows the on-screen keys, --menu opens the menu, --fine opens a
// knob's fine controls, --preset loads a factory preset (0 = Dusk Ballad), --annotate numbers the
// window's areas the way the user guide does, --bench times the light and a full repaint. --set
// changes a parameter (plain value, e.g. --set blend=0.8, --set bellVoice=1). The docs'
// screenshots are made with this tool.

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::printf("usage: KeysUiSnapshot <out.png> [--demo] [--keys] [--menu] [--fine <key>] [--preset <n>] "
                    "[--annotate] [--bench] [--scale <factor>] [--set <id>=<value>]...\n");
        return 1;
    }
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    bool demo = false, keys = false, menu = false, bench = false, annotate = false;
    juce::String fine;
    int preset = -1;
    float scale = 2.0f;
    juce::StringPairArray settings;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--demo")
            demo = true;
        else if (arg == "--keys")
            keys = true;
        else if (arg == "--bench")
            bench = true;
        else if (arg == "--annotate")
            annotate = true;
        else if (arg == "--menu")
            menu = true;
        else if (arg == "--fine" && i + 1 < argc)
            fine = argv[++i];
        else if (arg == "--preset" && i + 1 < argc)
            preset = juce::String(argv[++i]).getIntValue();
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
    if (preset >= 0)
        processor->getPresets().load(preset, false);
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
        auto* keysEditor = dynamic_cast<KeysEditor*>(editor.get());
        if (keysEditor != nullptr)
        {
            keysEditor->setSize(KeysEditor::kDesignWidth, keysEditor->getHeight() * KeysEditor::kDesignWidth / keysEditor->getWidth());
            keysEditor->setKeysVisible(keys);
            if (menu)
                keysEditor->openMenu();
            if (fine.isNotEmpty())
                keysEditor->openFinePanel(fine);
        }
        editor->setVisible(true);

        if (bench && keysEditor != nullptr)
        {
            // How long one frame of the light takes to compute, and to paint (software renderer:
            // slower than the Direct2D one a real window uses, so this is an upper bound).
            auto start = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 300; ++i)
                keysEditor->animate(1.0 / 30.0);
            const double light = (juce::Time::getMillisecondCounterHiRes() - start) / 300.0;
            // Paint into a software image (a snapshot's Direct2D image costs far more than painting).
            auto paintTime = [](juce::Component& part, int repeats)
            {
                juce::Image target(juce::Image::ARGB, std::max(1, part.getWidth()), std::max(1, part.getHeight()), true,
                                   juce::SoftwareImageType());
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < repeats; ++i)
                {
                    juce::Graphics g(target);
                    part.paintEntireComponent(g, true);
                }
                return (juce::Time::getMillisecondCounterHiRes() - t0) / repeats;
            };
            const double paint = paintTime(*editor, 30);
            std::printf("light %.2f ms per frame, full repaint %.2f ms (software, 1x)\n", light, paint);

            // The same, one part at a time.
            auto timePart = [&](juce::Component& part) { return paintTime(part, 10); };
            std::function<void(juce::Component&, int)> walk = [&](juce::Component& c, int depth)
            {
                if (depth > 3 || !c.isVisible())
                    return;
                const double ms = timePart(c);
                if (ms > 0.5)
                    std::printf("%*s%s %s: %.2f ms\n", depth * 2, "", typeid(c).name(), c.getTitle().toRawUTF8(), ms);
                for (auto* child : c.getChildren())
                    walk(*child, depth + 1);
            };
            walk(*editor, 0);
        }
        // Let the editor's timer run a few times so the header and monitor are filled in.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(400);

        auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        if (annotate && keysEditor != nullptr)
        {
            // Numbered red badges on each area, for the user guide.
            juce::Graphics g(image);
            g.addTransform(juce::AffineTransform::scale(scale));
            int number = 1;
            for (const auto& area : keysEditor->getLandmarks())
            {
                const auto bounds = keysEditor->getLocalBounds().toFloat().reduced(14.0f);
                const auto corner = area.toFloat().getTopLeft().translated(-4.0f, -4.0f);
                const auto badge = juce::Rectangle<float>(22.0f, 22.0f).withCentre(
                    { juce::jlimit(bounds.getX(), bounds.getRight(), corner.x), juce::jlimit(bounds.getY(), bounds.getBottom(), corner.y) });
                g.setColour(juce::Colours::black.withAlpha(0.5f));
                g.fillEllipse(badge.expanded(2.0f));
                g.setColour(juce::Colour(0xffec3a40));
                g.fillEllipse(badge);
                g.setColour(juce::Colours::white);
                g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
                g.drawText(juce::String(number++), badge, juce::Justification::centred, false);
            }
        }
        out.deleteFile();
        juce::FileOutputStream stream(out);
        // .jpg for the docs (film grain makes PNGs several times larger), PNG otherwise.
        juce::PNGImageFormat png;
        juce::JPEGImageFormat jpeg;
        jpeg.setQuality(0.9f);
        juce::ImageFileFormat& format = out.hasFileExtension("jpg;jpeg") ? static_cast<juce::ImageFileFormat&>(jpeg) : png;
        if (!stream.openedOk() || !format.writeImageToStream(image, stream))
        {
            std::printf("could not write %s\n", out.getFullPathName().toRawUTF8());
            return 1;
        }
        std::printf("wrote %s\n", out.getFullPathName().toRawUTF8());
    }
    processor.reset();
    return 0;
}
