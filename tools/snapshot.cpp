// Renders the editor to a PNG without a host, with a sound running through
// it so the needle is somewhere, so a change to the interface can be looked
// at straight from the build. Also puts the processor through what a host
// does to it.
//
//   optobox_snapshot out.png [width height] [--set id=value ...] [--browser | --menu]
//   optobox_snapshot --selftest
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "TestSignal.h"

#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

std::unique_ptr<onirique::OptoBoxProcessor> makeProcessor()
{
    return std::unique_ptr<onirique::OptoBoxProcessor> (static_cast<onirique::OptoBoxProcessor*> (createPluginFilter()));
}

void set (onirique::OptoBoxProcessor& p, const juce::String& id, float value)
{
    if (auto* parameter = p.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Plays a voice into the plugin, then takes the picture with the needle
    where the sound has left it. Everything happens here and now: a command
    line tool has no message loop on macOS, so no timer would ever fire. */
bool render (const juce::File& file, int width, int height, const juce::StringArray& settings, const juce::String& panel)
{
    auto processor = makeProcessor();

    for (const auto& s : settings)
        set (*processor, s.upToFirstOccurrenceOf ("=", false, false), s.fromFirstOccurrenceOf ("=", false, false).getFloatValue());

    processor->setPlayConfigDetails (2, 2, kSr, kBlock);
    processor->prepareToPlay (kSr, kBlock);

    // A voice that peaks at -6 dBFS, some 12 dB over 0 VU.
    auto signal = mdxtest::makeVoice (kSr, 1.2, 110.0);
    const auto peak = *std::max_element (signal.begin(), signal.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });

    for (auto& v : signal)
        v *= 0.5f / std::abs (peak);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    for (size_t at = 0; at + kBlock <= signal.size(); at += kBlock)
    {
        buffer.copyFrom (0, 0, signal.data() + at, kBlock);
        buffer.copyFrom (1, 0, signal.data() + at, kBlock);
        processor->processBlock (buffer, midi);
    }

    std::printf ("    input peaks at %.1f dBFS; %.1f dB of reduction; the needle at %.3f\n",
                 20.0 * std::log10 (0.5),
                 processor->getReductionDb(), processor->getNeedle());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    editor->setSize (width, height);

    std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
    {
        // This machine may hold no licence for the plugin: the activation
        // page is not what is being looked at.
        if (dynamic_cast<onirique::license::Overlay*> (&c) != nullptr)
            c.setVisible (false);

        if (auto* vu = dynamic_cast<onirique::hw::VuMeter*> (&c))
            vu->setNeedle (processor->getNeedle());

        // The bar's panels, opened as a click would.
        if (auto* bar = dynamic_cast<onirique::nuit::Bar*> (&c))
        {
            if (panel == "--browser")
                bar->openBrowser();
            else if (panel == "--menu")
                bar->openMenu();
        }

        for (auto* child : c.getChildren())
            visit (*child);
    };

    visit (*editor);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    file.deleteFile();

    if (auto stream = file.createOutputStream())
        return juce::PNGImageFormat().writeImageToStream (image, *stream);

    return false;
}

/** Plays a signal through a processor as a host would, block by block, and
    gives back the first channel. key: what goes into the sidechain bus. */
std::vector<float> playThrough (onirique::OptoBoxProcessor& host, const std::vector<float>& x, int channelsIn,
                                const std::vector<float>* key = nullptr)
{
    const auto keyed = key != nullptr;
    const auto total = juce::jmax (2, channelsIn) + (keyed ? 2 : 0);
    juce::AudioBuffer<float> buffer (total, kBlock);
    juce::MidiBuffer midi;
    std::vector<float> out;

    for (size_t at = 0; at + kBlock <= x.size(); at += kBlock)
    {
        buffer.clear();

        for (int c = 0; c < channelsIn; ++c)
            buffer.copyFrom (c, 0, x.data() + at, kBlock);

        if (keyed)
            for (int c = 0; c < 2; ++c)
                buffer.copyFrom (total - 2 + c, 0, key->data() + at, kBlock);

        host.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + kBlock);
    }

    return out;
}

/** The processor as a host drives it. Returns the number of things found wrong. */
int selfTest()
{
    int failures = 0;

    auto check = [&failures] (bool ok, const char* what)
    {
        std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
        failures += ok ? 0 : 1;
    };

    auto processor = makeProcessor();
    auto get = [] (onirique::OptoBoxProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto text = [] (onirique::OptoBoxProcessor& p, const char* id) { return p.apvts.getParameter (id)->getCurrentValueAsText(); };

    // The panel's defaults, and how the host reads them.
    check (text (*processor, "threshold") == "0.0 dB" && text (*processor, "output") == "0.0 dB", "threshold and output start at 0 dB");
    set (*processor, "threshold", obx::kThresholdOffDb);
    set (*processor, "output", obx::kOutputOffDb);
    check (text (*processor, "threshold") == "off" && text (*processor, "output") == "off", "their ends read off");
    set (*processor, "ratio", 4.0f);
    set (*processor, "release", 2500.0f);
    std::printf ("    ratio %s, release %s, attack %s, parallel %s\n", text (*processor, "ratio").toRawUTF8(),
                 text (*processor, "release").toRawUTF8(), text (*processor, "attack").toRawUTF8(), text (*processor, "parallel").toRawUTF8());
    check (text (*processor, "ratio") == "4.0:1" && text (*processor, "release") == "2.50 s", "ratio and release read as on the panel");

    // The knobs' marks are where the hardware has them, and their values are what they say.
    auto* threshold = processor->apvts.getParameter ("threshold");
    check (std::abs (threshold->convertFrom0to1 (0.5f) + 20.0f) < 0.01f, "threshold: -20 straight up");
    check (std::abs (threshold->convertTo0to1 (0.0f) - 35.0f / 260.0f) < 0.001f, "and 0 at the second mark");

    // The saved state.
    set (*processor, "threshold", -17.0f);
    set (*processor, "generation", 1.0f);
    set (*processor, "timing", 1.0f);
    set (*processor, "lowcut", 2.0f);
    juce::MemoryBlock saved;
    processor->getStateInformation (saved);
    auto second = makeProcessor();
    second->setStateInformation (saved.getData(), (int) saved.getSize());
    check (std::abs (get (*second, "threshold") + 17.0f) < 0.01f && get (*second, "generation") > 0.5f
               && std::abs (get (*second, "timing") - 1.0f) < 0.01f && std::abs (get (*second, "lowcut") - 2.0f) < 0.01f,
           "a restored state has every setting");

    // The presets: a stock one sets what it names and leaves the meter as it was; Default puts it all back.
    {
        auto host = makeProcessor();
        set (*host, "meter", 0.0f);
        host->presets.load ("Snare Snap");
        check (host->presets.getCurrentName() == "Snare Snap" && get (*host, "timing") < 0.5f && get (*host, "generation") > 0.5f
                   && std::abs (get (*host, "threshold") - 6.0f) < 0.01f,
               "a stock preset sets what it names");
        check (get (*host, "meter") < 0.5f, "and leaves the meter on what it was showing");
        host->presets.load ("Default");
        check (get (*host, "generation") < 0.5f && std::abs (get (*host, "timing") - 2.0f) < 0.01f && std::abs (get (*host, "threshold")) < 0.01f,
               "Default puts everything back");
        check (host->presets.getStockNames().size() >= 12, "there are stock presets");

        // A and B, kept in the saved state.
        host->presets.load ("Lead Vocal");
        host->presets.switchToSlot (1);
        host->presets.load ("Mastering Touch");
        juce::MemoryBlock state;
        host->getStateInformation (state);
        auto restored = makeProcessor();
        restored->setStateInformation (state.getData(), (int) state.getSize());
        check (restored->presets.getSlot() == 1 && restored->presets.getCurrentName() == "Mastering Touch", "a restored state is on B, with its preset");
        restored->presets.switchToSlot (0);
        check (restored->presets.getCurrentName() == "Lead Vocal" && std::abs (get (*restored, "threshold") + 1.0f) < 0.01f, "and has A too");

        // Every stock preset, played loud: none blows up.
        const auto loud = mdxtest::makeNoise (1.0, kSr, 0.9f);
        double highest = 0.0;

        for (const auto& name : host->presets.getStockNames())
        {
            auto player = makeProcessor();
            player->presets.load (name);
            player->setPlayConfigDetails (2, 2, kSr, kBlock);
            player->prepareToPlay (kSr, kBlock);

            for (auto v : playThrough (*player, loud, 2))
                highest = std::isfinite (v) ? std::max (highest, (double) std::abs (v)) : 1.0e9;
        }

        std::printf ("    the loudest any stock preset gets on noise at -1 dB: %.1f dBFS\n", 20.0 * std::log10 (highest + 1.0e-12));
        check (highest < 4.0, "no stock preset blows up");

        // At a usual level (a voice at -18 dBFS RMS), the make-up of each preset gives back about what it takes.
        auto voice = mdxtest::makeVoice (kSr, 3.0, 147.0);
        const auto voiceRms = mdxtest::rmsDb (voice.data(), voice.size());

        for (auto& v : voice)
            v *= (float) std::pow (10.0, (-18.0 - voiceRms) / 20.0);

        double furthest = 0.0;
        juce::String levels;

        for (const auto& name : host->presets.getStockNames())
        {
            // Default is the panel as it comes, and the duck has nothing in its key.
            if (name == "Default" || name == "Sidechain Duck")
                continue;

            auto player = makeProcessor();
            player->presets.load (name);
            player->setPlayConfigDetails (2, 2, kSr, kBlock);
            player->prepareToPlay (kSr, kBlock);

            const auto y = playThrough (*player, voice, 2);
            const auto half = y.size() / 2;
            const auto change = mdxtest::rmsDb (y.data() + half, y.size() - half) - mdxtest::rmsDb (voice.data() + half, y.size() - half);
            levels << name << " " << juce::String (change, 1) << ", ";
            furthest = std::max (furthest, std::abs (change));
        }

        std::printf ("    level change at -18 dBFS: %s\n", levels.dropLastCharacters (2).toRawUTF8());
        check (furthest < 1.5, "each stock preset keeps about the level it was given");
    }

    // The buses a host may offer.
    {
        auto host = makeProcessor();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (host->checkBusesLayoutSupported (layout), "stereo with a stereo sidechain");

        layout.inputBuses.getReference (1) = juce::AudioChannelSet::disabled();
        check (host->checkBusesLayoutSupported (layout), "stereo without a sidechain");

        layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
        check (host->checkBusesLayoutSupported (layout), "mono into stereo");

        layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
        check (! host->checkBusesLayoutSupported (layout), "two channels are not folded into one");
        check (host->getLatencySamples() == 0, "there is no latency");
    }

    const auto voice = mdxtest::makeVoice (kSr, 2.0, 220.0);

    // Bypassed, the input comes out as it went in, on one channel and on two.
    for (int channels : { 1, 2 })
    {
        auto host = makeProcessor();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::disabled());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        host->setBusesLayout (layout);
        set (*host, "bypass", 1.0f);
        set (*host, "threshold", -30.0f);
        host->setRateAndBufferSizeDetails (kSr, kBlock);
        host->prepareToPlay (kSr, kBlock);

        const auto y = playThrough (*host, voice, channels);
        double worst = 0.0;

        for (size_t i = 0; i < y.size(); ++i)
            worst = std::max (worst, (double) std::abs (y[i] - voice[i]));

        check (worst < 1.0e-6, channels == 1 ? "bypassed, one channel comes out on both untouched" : "and two");
    }

    // The sidechain bus: on EXT. a loud key brings a quiet input down.
    {
        auto host = makeProcessor();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (host->setBusesLayout (layout), "the sidechain bus can be switched on");
        set (*host, "scbus", 1.0f);
        set (*host, "threshold", -20.0f);
        host->setRateAndBufferSizeDetails (kSr, kBlock);
        host->prepareToPlay (kSr, kBlock);

        const auto quiet = mdxtest::makeTone (2.0, kSr, 440.0, 0.01f);
        const auto loud = mdxtest::makeTone (2.0, kSr, 100.0, 0.5f);
        playThrough (*host, quiet, 2, &loud);
        std::printf ("    a -6 dBFS key on a -40 dBFS input: %.1f dB of reduction\n", host->getReductionDb());
        check (host->hasSidechain() && host->getReductionDb() > 10.0f, "the key compresses the input");
    }

    // Loud noise, every way the panel can be set to be extreme: nothing blows up.
    {
        const auto loud = mdxtest::makeNoise (1.0, kSr, 1.0f);
        double highest = 0.0;

        for (float timing : { 0.0f, 1.0f, 2.0f })
            for (float generation : { 0.0f, 1.0f })
            {
                auto host = makeProcessor();
                set (*host, "timing", timing);
                set (*host, "generation", generation);
                set (*host, "output", 30.0f);
                set (*host, "attack", 0.5f);
                set (*host, "release", 50.0f);
                host->setPlayConfigDetails (2, 2, kSr, kBlock);
                host->prepareToPlay (kSr, kBlock);

                for (auto v : playThrough (*host, loud, 2))
                    highest = std::isfinite (v) ? std::max (highest, (double) std::abs (v)) : 1.0e9;
            }

        std::printf ("    full-scale noise with +30 dB of gain: %.1f dBFS at most\n", 20.0 * std::log10 (highest + 1.0e-12));
        check (highest < 16.0, "nothing blows up");
    }

    std::printf (failures == 0 ? "\nall passed\n" : "\n%d FAILED\n", failures);
    return failures;
}
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return 2;

    const juce::ScopedJuceInitialiser_GUI gui;

    if (juce::String (argv[1]) == "--selftest")
        return selfTest();

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    int width = onirique::OptoBoxEditor::kWidth, height = onirique::OptoBoxEditor::kTotalHeight, next = 2;

    if (argc > 3 && juce::String (argv[2]).containsOnly ("0123456789"))
    {
        width = juce::String (argv[2]).getIntValue();
        height = juce::String (argv[3]).getIntValue();
        next = 4;
    }

    juce::StringArray settings;
    juce::String panel;

    for (int i = next; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--set" && i + 1 < argc)
            settings.add (argv[++i]);
        else if (arg == "--browser" || arg == "--menu")
            panel = arg;
    }

    return render (file, width, height, settings, panel) ? 0 : 1;
}
