// Onirique OptoBox — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/OniriqueBar.h"
#include "ui/OniriquePhotoDA.h"   // la DA de VOCALBOX autour de la façade

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
    // la façade posée en panneau de verre sur le fond nuit de VOCALBOX, à kMargin des bords
    static constexpr int kMargin = 16, kUnits = kWidth + 2 * kMargin;
    static constexpr int kBarHeight = 50, kTotalHeight = kBarHeight + kFrontHeight + 2 * kMargin;

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

    // la DA de VOCALBOX : le fond nuit, la couleur de l'interface suivie, la lueur sur la commande survolée
    struct GlowLayer final : Component
    {
        Rectangle<float> target;
        float amount = 0.0f;
        void paint (Graphics& g) override
        {
            if (amount > 0.01f)
                da::partGlow (g, target.getCentre(), jmin (target.getWidth(), target.getHeight()) * 0.5f, amount);
        }
    };
    struct Watch final : Timer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    };
    void watchTick();
    GlowLayer glowLayer;
    Watch watch;
    Image daBackdrop;
    int themeSeen = -1;

    /** The licence badge and its activation page. Declared last and added
        last: it lies over everything else. */
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoBoxEditor)
};

} // namespace onirique
