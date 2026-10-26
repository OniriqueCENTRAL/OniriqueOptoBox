#include "VuMeter.h"
#include "Hardware.h"

namespace onirique::hw
{

namespace
{
    constexpr float kBezel = 9.0f;
    constexpr float kRadius = 112.0f;                 // of the scale, from the pivot
    constexpr float kSweep = 47.0f * MathConstants<float>::pi / 180.0f;
    constexpr float kFullScale = 1.41254f;            // +3 VU

    Rectangle<float> window()
    {
        return Rectangle<float> (0.0f, 0.0f, VuMeter::kWidth, VuMeter::kHeight).reduced (kBezel);
    }

    /** The face's script, after the maker's name on the original. */
    Font scriptFont (float height)
    {
       #if JUCE_MAC
        return Font (FontOptions ("Snell Roundhand", height, Font::bold));
       #elif JUCE_WINDOWS
        return Font (FontOptions ("Segoe Script", height * 0.8f, Font::plain));
       #else
        return Font (FontOptions (height, Font::italic));
       #endif
    }
}

VuMeter::VuMeter()
{
    setInterceptsMouseClicks (false, false);
}

Point<float> VuMeter::pivot() const
{
    return { kWidth * 0.5f, window().getBottom() + 44.0f };
}

float VuMeter::angleFor (float deflection) const
{
    const auto f = jlimit (-0.02f, 1.04f, deflection / kFullScale);
    return -kSweep + f * 2.0f * kSweep;
}

void VuMeter::setNeedle (float deflection)
{
    // A needle that has not moved by a hair is not worth a frame.
    if (std::abs (deflection - needle) < 0.0005f)
        return;

    needle = deflection;
    repaint (window().toNearestInt());
}

void VuMeter::paintFace (Graphics& g) const
{
    const auto bounds = Rectangle<float> (0.0f, 0.0f, kWidth, kHeight);
    const auto glass = window();
    const auto p = pivot();

    // The bezel: black, bevelled, with the window cut in it.
    g.setColour (Colour (0xff050505));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setGradientFill (ColourGradient (Colour (0xff2a2a2c), 0.0f, 0.0f, Colour (0xff060606), 0.0f, kHeight, false));
    g.fillRoundedRectangle (bounds.reduced (1.5f), 4.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (6.0f, 2.2f, kWidth - 6.0f, 2.2f, 1.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (glass.expanded (2.5f), 3.5f);

    // The face: cream paper, lit from below by a cool LED.
    g.saveState();
    g.reduceClipRegion (glass.toNearestInt());

    g.setColour (Colour (0xfff1e6c8));
    g.fillRect (glass);

    const auto led = Point<float> (glass.getCentreX(), glass.getBottom() + 6.0f);
    g.setGradientFill (ColourGradient (Colour (0x5c86b8ff), led.x, led.y, Colour (0x0c9cc4ff), led.x, glass.getY() - 30.0f, true));
    g.fillRect (glass);
    g.setGradientFill (ColourGradient (Colour (0x48eef4ff), led.x, led.y - 16.0f, Colour (0x00eef4ff), led.x + 64.0f, led.y - 16.0f, true));
    g.fillRect (glass);

    // Darker towards the corners, where the light does not reach.
    g.setGradientFill (ColourGradient (Colours::transparentBlack, glass.getCentreX(), glass.getCentreY() + 20.0f,
                                       Colour (0x48201810), glass.getRight() + 10.0f, glass.getY() - 10.0f, true));
    g.fillRect (glass);

    const auto ink = Colour (0xff141416);
    const auto red = Colour (0xffd42a2a);

    auto at = [&p] (float radius, float deflection, float sweepFrom = -kSweep)
    {
        const auto f = deflection / kFullScale;
        return onCircle (p, radius, sweepFrom + f * 2.0f * kSweep);
    };

    auto arc = [&p] (float radius, float from, float to)
    {
        Path a;
        a.addCentredArc (p.x, p.y, radius, radius, 0.0f,
                         -kSweep + from / kFullScale * 2.0f * kSweep, -kSweep + to / kFullScale * 2.0f * kSweep, true);
        return a;
    };

    auto toDeflection = [] (float vu) { return std::pow (10.0f, vu / 20.0f); };

    // The scale: a fine line to 0, the red band beyond it.
    g.setColour (ink);
    g.strokePath (arc (kRadius, toDeflection (-20.0f), 1.0f), PathStrokeType (0.9f));
    g.setColour (red);
    g.strokePath (arc (kRadius + 2.2f, 1.0f, kFullScale), PathStrokeType (4.2f));

    struct Mark { float vu; const char* text; };
    static constexpr Mark marks[] = {
        { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" }, { -3.0f, "3" }, { -2.0f, "2" },
        { -1.0f, "1" }, { 0.0f, "0" }, { 1.0f, "1" }, { 2.0f, "2" }, { 3.0f, "3" }
    };

    g.setFont (panelFont (11.5f, false));

    for (const auto& m : marks)
    {
        const auto d = toDeflection (m.vu);
        g.setColour (m.vu > 0.0f ? red : ink);
        g.drawLine ({ at (kRadius - 1.0f, d), at (kRadius + 7.5f, d) }, m.vu == 0.0f ? 1.6f : 1.2f);

        const auto label = at (kRadius + 15.5f, d);
        g.drawText (m.text, Rectangle<float> (24.0f, 13.0f).withCentre (label), Justification::centred, false);
    }

    for (float vu : { -15.0f, -0.5f, 0.5f, 1.5f, 2.5f })
    {
        const auto d = toDeflection (vu);
        g.setColour (vu > 0.0f ? red : ink);
        g.drawLine ({ at (kRadius - 1.0f, d), at (kRadius + 4.0f, d) }, 0.9f);
    }

    // Under it, the scale in percent of 0 VU.
    g.setFont (panelFont (6.8f, false));
    g.setColour (ink.withAlpha (0.8f));

    for (int pct = 0; pct <= 100; pct += 20)
    {
        const auto d = (float) pct / 100.0f;
        g.drawLine ({ at (kRadius - 6.0f, d), at (kRadius - 1.0f, d) }, 0.8f);
        g.drawText (String (pct), Rectangle<float> (18.0f, 9.0f).withCentre (at (kRadius - 12.0f, d)), Justification::centred, false);
    }

    g.setFont (panelFont (17.0f, true));
    g.setColour (ink);
    g.drawText (String::fromUTF8 ("\xe2\x80\x93"), Rectangle<float> (16.0f, 16.0f).withCentre (at (kRadius + 4.0f, -0.07f)), Justification::centred, false);
    g.setColour (red);
    g.drawText ("+", Rectangle<float> (16.0f, 16.0f).withCentre (at (kRadius + 4.0f, kFullScale * 1.07f)), Justification::centred, false);

    // The lettering.
    g.setColour (ink.withAlpha (0.85f));
    g.setFont (scriptFont (21.0f));
    g.drawText ("Onirique", Rectangle<float> (130.0f, 26.0f).withCentre ({ glass.getCentreX() + 2.0f, glass.getY() + 70.0f }),
                Justification::centred, false);

    g.setColour (ink);
    g.setFont (panelFont (13.0f, true).withExtraKerningFactor (0.04f));
    g.drawText ("VU", Rectangle<float> (glass.getRight() - 40.0f, glass.getBottom() - 22.0f, 32.0f, 16.0f), Justification::centredRight, false);

    // The bezel's shadow on the face, along the top and the sides.
    g.setGradientFill (ColourGradient (Colour (0x80000000), 0.0f, glass.getY(), Colour (0x00000000), 0.0f, glass.getY() + 12.0f, false));
    g.fillRect (glass.withHeight (12.0f));
    g.setGradientFill (ColourGradient (Colour (0x50000000), glass.getX(), 0.0f, Colour (0x00000000), glass.getX() + 8.0f, 0.0f, false));
    g.fillRect (glass.withWidth (8.0f));
    g.setGradientFill (ColourGradient (Colour (0x50000000), glass.getRight(), 0.0f, Colour (0x00000000), glass.getRight() - 8.0f, 0.0f, false));
    g.fillRect (glass.withTrimmedLeft (glass.getWidth() - 8.0f));

    g.restoreState();

    // The LED's light, caught by the lower lip of the bezel.
    g.setGradientFill (ColourGradient (Colour (0x707fb4ff), glass.getCentreX(), glass.getBottom(), Colour (0x007fb4ff), glass.getX() + 20.0f, glass.getBottom(), false));
    g.fillRect (Rectangle<float> (glass.getX(), glass.getBottom() + 0.5f, glass.getWidth(), 1.6f));
}

void VuMeter::paint (Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (face.isNull() || std::abs (scale - faceScale) > 0.001f)
    {
        faceScale = scale;
        face = Image (Image::ARGB, jmax (1, roundToInt (kWidth * scale)), jmax (1, roundToInt (kHeight * scale)), true);
        Graphics fg (face);
        fg.addTransform (AffineTransform::scale (scale));
        paintFace (fg);
    }

    g.drawImageTransformed (face, AffineTransform::scale (1.0f / faceScale));

    // The needle, and its shadow on the lit face.
    const auto glass = window();
    const auto p = pivot();
    const auto angle = angleFor (needle);
    const auto tip = onCircle (p, kRadius + 9.0f, angle);

    g.saveState();
    g.reduceClipRegion (glass.toNearestInt());

    g.setColour (Colour (0x30000a20));
    g.drawLine ({ p.translated (2.5f, 3.0f), tip.translated (2.5f, 3.0f) }, 1.8f);
    g.setColour (Colour (0xff101012));
    g.drawLine ({ p, tip }, 1.25f);

    // The glass over it all.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), glass.getX(), glass.getY(),
                                       Colours::white.withAlpha (0.0f), glass.getX() + glass.getWidth() * 0.45f, glass.getBottom(), false));
    g.fillRect (glass);

    g.restoreState();
}

} // namespace onirique::hw
