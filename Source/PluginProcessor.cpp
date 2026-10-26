#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace onirique
{

namespace
{
    using Attributes = AudioParameterFloatAttributes;

    /** A range that is even to the ear: each doubling takes the same travel. */
    NormalisableRange<float> logRange (float low, float high)
    {
        return { low, high,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float a, float b, float v) { return jlimit (a, b, v); } };
    }

    /** The output and threshold knobs are not even: their marks are where the
        hardware has them, and the travel between two marks is even. */
    constexpr float kMarks[] = { 0.0f, 35.0f / 260.0f, 82.0f / 260.0f, 0.5f, 178.0f / 260.0f, 1.0f };

    NormalisableRange<float> markedRange (std::array<float, 6> values)
    {
        const auto low = *std::min_element (values.begin(), values.end());
        const auto high = *std::max_element (values.begin(), values.end());

        return { low, high,
                 [values] (float, float, float t)
                 {
                     t = jlimit (0.0f, 1.0f, t);

                     for (size_t i = 0; i + 1 < values.size(); ++i)
                         if (t <= kMarks[i + 1])
                             return jmap (t, kMarks[i], kMarks[i + 1], values[i], values[i + 1]);

                     return values.back();
                 },
                 [values] (float, float, float v)
                 {
                     for (size_t i = 0; i + 1 < values.size(); ++i)
                     {
                         const auto a = values[i], b = values[i + 1];

                         if ((v >= jmin (a, b) && v <= jmax (a, b)) && a != b)
                             return jmap (v, a, b, kMarks[i], kMarks[i + 1]);
                     }

                     return std::abs (v - values.front()) < std::abs (v - values.back()) ? 0.0f : 1.0f;
                 },
                 [] (float a, float b, float v) { return jlimit (a, b, v); } };
    }

    float parseNumber (const String& text)
    {
        return text.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getFloatValue();
    }

    Attributes output()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              if (v <= obx::kOutputOffDb + 0.05f)
                                                  return String ("off");
                                              const auto text = String (v, 1);
                                              return (v > 0.04f ? "+" : "") + text + " dB";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              return t.containsIgnoreCase ("off") ? obx::kOutputOffDb : parseNumber (t);
                                          });
    }

    Attributes threshold()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              if (v >= obx::kThresholdOffDb - 0.05f)
                                                  return String ("off");
                                              return (v > 0.04f ? "+" : "") + String (v, 1) + " dB";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              return t.containsIgnoreCase ("off") ? obx::kThresholdOffDb : parseNumber (t);
                                          });
    }

    Attributes ratio()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t.upToFirstOccurrenceOf (":", false, false)); });
    }

    Attributes milliseconds()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              if (v >= 1000.0f)
                                                  return String (v / 1000.0f, 2) + " s";
                                              return String (v, v < 10.0f ? 1 : 0) + " ms";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              const auto v = parseNumber (t);
                                              return t.containsIgnoreCase ("ms") || ! t.containsIgnoreCase ("s") ? v : v * 1000.0f;
                                          });
    }

    Attributes parallel()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              const auto n = roundToInt (v);
                                              return n >= 100 ? String ("wet") : n <= 0 ? String ("dry") : String (n) + " % wet";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              if (t.containsIgnoreCase ("dry")) return 0.0f;
                                              if (t.trim().equalsIgnoreCase ("wet")) return 100.0f;
                                              return parseNumber (t);
                                          });
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout layout;
        const obx::Settings defaults;

        auto number = [&layout] (const String& id, const String& name, NormalisableRange<float> range,
                                 float value, const Attributes& attributes)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, value, attributes));
        };

        auto choice = [&layout] (const String& id, const String& name, const StringArray& options, int value)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, options, value));
        };

        // The compressor, as its front panel lays it out.
        number ("output", "Output Volume", markedRange ({ obx::kOutputOffDb, -10.0f, 0.0f, 10.0f, 20.0f, 30.0f }),
                defaults.outputDb, output());
        number ("ratio", "Ratio", logRange (2.0f, 10.0f), defaults.ratio, ratio());
        number ("threshold", "Threshold", markedRange ({ obx::kThresholdOffDb, 0.0f, -10.0f, -20.0f, -30.0f, -40.0f }),
                defaults.thresholdDb, threshold());
        choice ("meter", "Meter", { "Input", "GR", "Output" }, (int) defaults.meter);
        number ("attack", "Attack", logRange (0.5f, 300.0f), defaults.attackMs, milliseconds());
        number ("release", "Release", logRange (50.0f, 10000.0f), defaults.releaseMs, milliseconds());
        choice ("timing", "Attack/Release Select", { "Fixed", "Fix./Man.", "Manual" }, (int) defaults.timing);
        choice ("scbus", "Sidechain Bus Select", { "Int.", "Ext." }, defaults.externalSidechain ? 1 : 0);

        // The strip under it.
        choice ("generation", "Generation", { "Vintage mk I", "Modern mk II" }, (int) defaults.generation);
        choice ("lowcut", "Sidechain Low Cut", { "Off", "80 Hz", "220 Hz" }, defaults.lowCut);
        number ("parallel", "Parallel Comp.", { 0.0f, 100.0f, 1.0f }, defaults.parallel * 100.0f, parallel());

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

        return layout;
    }
}

namespace
{
    enum Tags { vocal = 1, drums = 2, bass = 4, instruments = 8, mix = 16, mastering = 32 };

    // Starting points, from the uses the unit is known for and the settings its manual gives: drums on FIXED
    // for 2 or 3 dB, bass, piano, guitars and vocals on MANUAL with the attack near 2 o'clock (about 50 ms)
    // and the release near 10 o'clock (about 200 ms). Each threshold is where a voice at -18 dBFS RMS (0 VU)
    // is brought down by the amount the preset is for, and the output gives that back. Choices by position:
    // timing 0 fixed, 1 fix./man., 2 manual; generation 0 mk I, 1 mk II; lowcut 0 off, 1 80 Hz, 2 220 Hz.
    std::vector<PresetBank::Stock> stockPresets()
    {
        return {
            { "Default", 0, {} },

            { "Lead Vocal", vocal, { { "threshold", -1.0f }, { "ratio", 3.5f }, { "attack", 50.0f }, { "release", 200.0f },
                                     { "output", 5.0f } } },
            { "Silky Vocal", vocal, { { "generation", 1.0f }, { "threshold", 4.0f }, { "ratio", 2.5f }, { "attack", 25.0f },
                                      { "release", 600.0f }, { "lowcut", 1.0f }, { "output", 4.0f } } },
            { "Vocal Leveler", vocal, { { "timing", 1.0f }, { "threshold", -2.0f }, { "ratio", 4.0f }, { "attack", 120.0f },
                                        { "release", 2500.0f }, { "output", 8.0f } } },

            { "Snare Snap", drums, { { "timing", 0.0f }, { "generation", 1.0f }, { "threshold", 6.0f }, { "output", 3.0f } } },
            { "Kick Control", drums, { { "timing", 0.0f }, { "threshold", 4.0f }, { "ratio", 3.0f }, { "output", 3.0f } } },
            { "Drum Bus Glue", drums | mix, { { "timing", 1.0f }, { "threshold", 5.0f }, { "ratio", 2.0f }, { "attack", 30.0f },
                                              { "release", 300.0f }, { "lowcut", 1.0f }, { "output", 3.0f } } },
            { "Parallel Smash", drums, { { "threshold", -9.0f }, { "ratio", 10.0f }, { "attack", 0.5f }, { "release", 100.0f },
                                         { "output", 16.0f }, { "parallel", 35.0f } } },

            { "Bass Hold", bass, { { "threshold", -2.0f }, { "ratio", 6.0f }, { "attack", 50.0f }, { "release", 200.0f },
                                   { "output", 6.5f } } },
            { "Bass Smooth", bass, { { "timing", 0.0f }, { "threshold", 3.0f }, { "output", 4.0f } } },

            { "Acoustic Guitar", instruments, { { "generation", 1.0f }, { "threshold", 2.0f }, { "ratio", 3.0f }, { "attack", 50.0f },
                                                { "release", 200.0f }, { "output", 4.0f } } },
            { "Piano Even", instruments, { { "threshold", 1.0f }, { "ratio", 2.5f }, { "attack", 40.0f }, { "release", 500.0f },
                                           { "output", 4.0f } } },

            { "Mix Bus Opto", mix | mastering, { { "generation", 1.0f }, { "timing", 1.0f }, { "threshold", 6.0f }, { "ratio", 2.0f },
                                                 { "attack", 30.0f }, { "release", 1200.0f }, { "lowcut", 2.0f }, { "output", 2.0f } } },
            { "Mastering Touch", mastering, { { "generation", 1.0f }, { "threshold", 7.0f }, { "ratio", 2.0f }, { "attack", 50.0f },
                                              { "release", 2000.0f }, { "lowcut", 2.0f }, { "output", 1.5f } } },
            { "Tubes Only", mix | instruments | mastering, { { "threshold", obx::kThresholdOffDb } } },
            { "Sidechain Duck", mix, { { "generation", 1.0f }, { "scbus", 1.0f }, { "threshold", -6.0f }, { "ratio", 6.0f },
                                       { "attack", 5.0f }, { "release", 300.0f } } },
        };
    }
}

/** The parameters' values, looked up once. */
struct OptoBoxProcessor::Raw
{
    explicit Raw (AudioProcessorValueTreeState& state)
    {
        auto get = [&state] (const String& id) { return state.getRawParameterValue (id); };

        output = get ("output");         threshold = get ("threshold");   ratio = get ("ratio");
        attack = get ("attack");         release = get ("release");       timing = get ("timing");
        meter = get ("meter");           scbus = get ("scbus");           generation = get ("generation");
        lowcut = get ("lowcut");         parallel = get ("parallel");     bypass = get ("bypass");
    }

    std::atomic<float> *output, *threshold, *ratio, *attack, *release, *timing, *meter, *scbus,
                       *generation, *lowcut, *parallel, *bypass;
};

//==============================================================================
OptoBoxProcessor::OptoBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "OptoBox", createLayout()),
      // What the meter shows is how it is being looked at, not what it does to the sound.
      presets (apvts, "OptoBox", stockPresets(), { "Vocal", "Drums", "Bass", "Instruments", "Mix", "Mastering" }, { "bypass", "meter" }),
      raw (std::make_unique<Raw> (apvts))
{
    bypassParameter = apvts.getParameter ("bypass");
}

OptoBoxProcessor::~OptoBoxProcessor() = default;

bool OptoBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    // One channel can be spread over two; two channels cannot be folded
    // into one.
    if (in != out && ! (in == AudioChannelSet::mono() && out == AudioChannelSet::stereo()))
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& key = layouts.getChannelSet (true, 1);
        return key.isDisabled() || key == AudioChannelSet::mono() || key == AudioChannelSet::stereo();
    }

    return true;
}

obx::Settings OptoBoxProcessor::readSettings() const noexcept
{
    obx::Settings s;
    const auto& r = *raw;

    s.outputDb = r.output->load();
    s.thresholdDb = r.threshold->load();
    s.ratio = r.ratio->load();
    s.attackMs = r.attack->load();
    s.releaseMs = r.release->load();
    s.timing = (obx::Timing) jlimit (0, 2, roundToInt (r.timing->load()));
    s.meter = (obx::Meter) jlimit (0, 2, roundToInt (r.meter->load()));
    s.externalSidechain = r.scbus->load() > 0.5f;
    s.generation = r.generation->load() > 0.5f ? obx::Generation::modern : obx::Generation::vintage;
    s.lowCut = jlimit (0, 2, roundToInt (r.lowcut->load()));
    s.parallel = r.parallel->load() * 0.01f;
    s.bypass = r.bypass->load() > 0.5f;

    return s;
}

void OptoBoxProcessor::prepareToPlay (double sampleRate, int)
{
    license.refresh();
    compressor.setSettings (readSettings());
    compressor.prepare (sampleRate, getMainBusNumInputChannels());
}

void OptoBoxProcessor::reset()
{
    compressor.reset();
}

void OptoBoxProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    auto mainOut = getBusBuffer (buffer, false, 0);
    const auto numIn = jmin (getMainBusNumInputChannels(), (int) obx::kMaxChannels);
    const auto numOut = jmin (mainOut.getNumChannels(), (int) obx::kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    if (numIn < 1 || numOut < 1)
        return;

    // The sidechain bus, when the host has given it channels.
    const float* key[obx::kMaxChannels] = {};
    int numKey = 0;

    if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
    {
        const auto keyBuffer = getBusBuffer (buffer, true, 1);
        numKey = jmin (keyBuffer.getNumChannels(), (int) obx::kMaxChannels);

        for (int c = 0; c < numKey; ++c)
            key[c] = keyBuffer.getReadPointer (c);
    }

    sidechainConnected.store (numKey > 0, std::memory_order_relaxed);

    // The main input and output share their first channels.
    const float* in[obx::kMaxChannels] = {};
    float* out[obx::kMaxChannels] = {};

    for (int c = 0; c < numIn; ++c)
    {
        in[c] = buffer.getReadPointer (c);
        out[c] = buffer.getWritePointer (c);
    }

    compressor.setSettings (readSettings());
    compressor.process (in, out, numSamples, key, numKey);

    // One channel in, two out: the second is the first.
    for (int c = numIn; c < numOut; ++c)
        mainOut.copyFrom (c, 0, mainOut, 0, 0, numSamples);

    // Last, and over every output: what an unlicensed plugin lets through.
    license.process (mainOut, getSampleRate(), isNonRealtime());

    needle.store (compressor.needle(), std::memory_order_relaxed);
    reduction.store (compressor.reductionDb(), std::memory_order_relaxed);
}

//==============================================================================
void OptoBoxProcessor::getStateInformation (MemoryBlock& destination)
{
    auto state = apvts.copyState();
    presets.writeState (state);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void OptoBoxProcessor::setStateInformation (const void* data, int size)
{
    if (const auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        presets.readState (state);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* OptoBoxProcessor::createEditor()
{
    return new OptoBoxEditor (*this);
}

} // namespace onirique

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new onirique::OptoBoxProcessor();
}
