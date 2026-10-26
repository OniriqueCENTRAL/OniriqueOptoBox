// Onirique OptoBox — plugin entry point
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseGuard.h"
#include "dsp/Opto.h"
#include "ui/OniriquePresets.h"

namespace onirique
{

using namespace juce;

/** The plugin instance: the front panel's controls in, a block through the
    compressor (keyed by the sidechain bus when the panel says EXT.), and
    what the editor needs to draw the meter and the lamp. It also keeps the
    presets and the A/B comparison the bar over the front shows. */
class OptoBoxProcessor final : public AudioProcessor
{
public:
    OptoBoxProcessor();
    ~OptoBoxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override     { return false; }
    bool producesMidi() const override    { return false; }
    bool isMidiEffect() const override    { return false; }

    // The slowest release, once the input has stopped.
    double getTailLengthSeconds() const override { return 1.0; }

    AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    int getNumPrograms() override            { return 1; }
    int getCurrentProgram() override         { return 0; }
    void setCurrentProgram (int) override    {}
    const String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** The panel as the parameters stand now. */
    obx::Settings readSettings() const noexcept;

    /** Where the VU needle is (1 is 0 VU), and the reduction, for the editor. */
    float getNeedle() const noexcept { return needle.load (std::memory_order_relaxed); }
    float getReductionDb() const noexcept { return reduction.load (std::memory_order_relaxed); }

    /** Whether the host has given the sidechain bus any channels. */
    bool hasSidechain() const noexcept { return sidechainConnected.load (std::memory_order_relaxed); }

    AudioProcessorValueTreeState apvts;

    /** The stock presets, the user's, the A/B comparison: what the bar over the front shows. */
    PresetBank presets;

    /** The licence ONIRIQUE CENTRAL wrote on this machine for this plugin, under
        the identifier the plugin has in its catalogue. */
    onirique::license::Guard license { "onirique-optobox" };

private:
    obx::Compressor compressor;

    struct Raw;
    std::unique_ptr<Raw> raw;
    AudioProcessorParameter* bypassParameter = nullptr;

    std::atomic<float> needle { 1.0f }, reduction { 0.0f };
    std::atomic<bool> sidechainConnected { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoBoxProcessor)
};

} // namespace onirique
