// Onirique OptoBox — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/OniriqueBar.h"

namespace onirique
{

/** The two units of the rack, the compressor and its mix unit, as one front
    panel that keeps its proportions, and the bar of the range over it
    (ui/OniriqueBar.h): the window scales them as one. */
class OptoBoxEditor final : public AudioProcessorEditor
{
public:
    explicit OptoBoxEditor (OptoBoxProcessor&);
    ~OptoBoxEditor() override;

    void paint (Graphics&) override;
    void resized() override;

    static constexpr int kWidth = (int) Faceplate::kWidth, kFrontHeight = (int) Faceplate::kHeight;
    static constexpr int kBarHeight = 50, kTotalHeight = kBarHeight + kFrontHeight;

private:
    /** The descriptions shown on a control after a moment: a dark red card,
        as across the range. */
    struct Look final : LookAndFeel_V4
    {
        Rectangle<int> getTooltipBounds (const String&, Point<int> screenPos, Rectangle<int> parentArea) override;
        void drawTooltip (Graphics&, const String&, int width, int height) override;

        static TextLayout layoutFor (const String&, float width);
    };

    OptoBoxProcessor& processor;
    Look look;
    Faceplate faceplate;
    nuit::Bar bar;

    std::unique_ptr<TooltipWindow> tooltips;

    /** The licence badge and its activation page. Declared last and added
        last: it lies over everything else. */
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoBoxEditor)
};

} // namespace onirique
