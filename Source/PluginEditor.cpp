#include "PluginEditor.h"
#include "ui/OniriqueWindowSize.h"

#include <BinaryData.h>

namespace onirique
{

namespace
{
    const char* const kWindowName = "ONIRIQUE OPTOBOX";
    constexpr float kTipWidth = 300.0f;
}

OptoBoxEditor::OptoBoxEditor (OptoBoxProcessor& p)
    : AudioProcessorEditor (p), processor (p), faceplate (p),
      bar (p.presets, p.apvts, "OPTOBOX", ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize), kWidth, kWidth)
{
    setLookAndFeel (&look);
    addAndMakeVisible (faceplate);
    addAndMakeVisible (bar);
    jassert (nuit::Bar::heightFor (kWidth) == kBarHeight);

    tooltips = std::make_unique<TooltipWindow> (this, 1200);

    licenseOverlay = std::make_unique<onirique::license::Overlay> ("OPTOBOX", processor.license);
    addAndMakeVisible (*licenseOverlay);

    // The front and its bar keep their proportions; the window grows by zooming them, from 75 to 200 %.
    setResizable (true, true);
    setResizeLimits (kWidth * 3 / 4, kTotalHeight * 3 / 4, kWidth * 2, kTotalHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kTotalHeight);

    // taille d'ouverture : la règle commune des plugins ONIRIQUE (ui/OniriqueWindowSize.h)
    const auto opening = onirique::window::openingSize ({}, kWindowName, { kWidth, kTotalHeight },
                                                        { kWidth * 3 / 4, kTotalHeight * 3 / 4 }, { kWidth * 2, kTotalHeight * 2 });
    setSize (opening.x, opening.x * kTotalHeight / kWidth);
}

OptoBoxEditor::~OptoBoxEditor()
{
    onirique::window::rememberOnThisComputer (kWindowName, getWidth(), getHeight());
    setLookAndFeel (nullptr);
}

void OptoBoxEditor::paint (Graphics& g)
{
    g.fillAll (hw::palette::night);
}

void OptoBoxEditor::resized()
{
    const auto scale = (float) getWidth() / (float) kWidth;

    faceplate.setBounds (0, kBarHeight, kWidth, kFrontHeight);
    faceplate.setTransform (AffineTransform::scale (scale));

    // The licence badge has its place in the bar, beside the menu.
    bar.setPillWidth (licenseOverlay != nullptr ? licenseOverlay->pillWidth() : 0);
    bar.place (scale, getLocalBounds());

    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), scale, bar.getPillArea(), Justification::centredRight);
}

//==============================================================================
TextLayout OptoBoxEditor::Look::layoutFor (const String& text, float width)
{
    AttributedString s;
    s.setJustification (Justification::topLeft);
    s.setLineSpacing (2.0f);

    // The control's name in capitals, then what it does.
    const auto dash = text.indexOf (String::fromUTF8 ("\xe2\x80\x94"));

    if (dash > 0)
    {
        s.append (text.substring (0, dash).trim() + "\n", hw::panelFont (11.0f, true).withExtraKerningFactor (0.08f), hw::palette::ember.brighter (0.4f));
        s.append (text.substring (dash + 1).trim(), hw::panelFont (12.5f, false), hw::palette::mist);
    }
    else
    {
        s.append (text, hw::panelFont (12.5f, false), hw::palette::mist);
    }

    TextLayout layout;
    layout.createLayout (s, width);
    return layout;
}

Rectangle<int> OptoBoxEditor::Look::getTooltipBounds (const String& text, Point<int> screenPos, Rectangle<int> parentArea)
{
    const auto layout = layoutFor (text, kTipWidth);
    const auto w = roundToInt (jmin (kTipWidth, layout.getWidth()) + 28.0f);
    const auto h = roundToInt (layout.getHeight() + 22.0f);

    auto x = screenPos.x - w / 2;
    auto y = screenPos.y + 24;

    if (y + h > parentArea.getBottom())
        y = screenPos.y - h - 12;

    return Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
}

void OptoBoxEditor::Look::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    const auto bounds = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setGradientFill (ColourGradient (Colour (0xfa3d0911), 0.0f, 0.0f, Colour (0xfa1c0508), 0.0f, (float) height, false));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Colour (0x38ffd2d6));
    g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

    layoutFor (text, kTipWidth).draw (g, bounds.reduced (14.0f, 11.0f));
}

} // namespace onirique
