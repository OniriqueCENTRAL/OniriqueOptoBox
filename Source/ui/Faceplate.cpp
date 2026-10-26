#include "Faceplate.h"

namespace onirique
{

namespace
{
    using hw::onCircle;
    namespace palette = hw::palette;

    float degrees (float d) { return d * MathConstants<float>::pi / 180.0f; }
    String utf8 (const char* text) { return String::fromUTF8 (text); }

    // Where everything is, after the hardware's own front panel.
    constexpr Point<float> kOutput { 242.0f, 74.0f }, kRatio { 408.0f, 74.0f }, kThreshold { 570.0f, 74.0f },
                           kMeter { 730.0f, 72.0f };
    constexpr Point<float> kAttack { 242.0f, 220.0f }, kRelease { 408.0f, 220.0f }, kTiming { 570.0f, 228.0f },
                           kBus { 730.0f, 228.0f };
    constexpr Point<float> kLamp { 888.0f, 214.0f };
    constexpr Point<float> kGeneration { 86.0f, 352.0f }, kLowCut { 214.0f, 352.0f }, kParallel { 858.0f, 348.0f };
    const Rectangle<float> kVu { 823.0f, 26.0f, hw::VuMeter::kWidth, hw::VuMeter::kHeight };

    constexpr float kBrandSize = 24.0f;

    enum Style { lower = 0, caps = 1, brand = 2 };

    Font fontFor (int style, float size)
    {
        if (style == brand)
            return hw::brandFont (size);

        const auto f = hw::panelFont (size, true);
        return style == caps ? f.withExtraKerningFactor (0.07f) : f;
    }

    float widthOf (const Font& font, const String& text)
    {
        return GlyphArrangement::getStringWidth (font, text);
    }

    /** Where a line of text goes, from the point it hangs on. */
    Rectangle<float> boxFor (const String& text, const Font& font, Point<float> at, Justification justification)
    {
        const auto w = widthOf (font, text) + 4.0f, h = font.getHeight() * 1.2f;
        auto box = Rectangle<float> (w, h).withCentre (at);

        if (justification.testFlags (Justification::left))
            box.setX (at.x - 2.0f);
        else if (justification.testFlags (Justification::right))
            box.setX (at.x - w + 2.0f);

        return box;
    }
}

//==============================================================================
Faceplate::Faceplate (OptoBoxProcessor& p) : processor (p)
{
    auto& state = p.apvts;
    auto param = [&state] (const char* id) -> RangedAudioParameter& { return *state.getParameter (id); };

    setSize ((int) kWidth, (int) kHeight);

    // The compressor.
    placeKnob (output, "output", kOutput);
    placeKnob (ratio, "ratio", kRatio);
    placeKnob (threshold, "threshold", kThreshold);
    placeKnob (attack, "attack", kAttack);
    placeKnob (release, "release", kRelease);

    meter = std::make_unique<hw::Selector> (param ("meter"), std::vector<float> { degrees (-40.0f), 0.0f, degrees (40.0f) });
    timing = std::make_unique<hw::Selector> (param ("timing"), std::vector<float> { degrees (-45.0f), 0.0f, degrees (45.0f) });
    bus = std::make_unique<hw::Selector> (param ("scbus"), std::vector<float> { degrees (-35.0f), degrees (35.0f) });

    placeSelector (*meter, kMeter, { "input", "GR", "output" }, 44.0f);
    placeSelector (*timing, kTiming, { "fixed", "fix./man.", "manual" }, 46.0f);
    placeSelector (*bus, kBus, { "int.", "ext." }, 44.0f);

    lamp = std::make_unique<hw::Lamp> (param ("bypass"));
    addAndMakeVisible (*lamp);
    lamp->setBounds (Rectangle<int> (64, 64).withCentre (kLamp.toInt()));

    addAndMakeVisible (vu);
    vu.setBounds (kVu.toNearestInt());

    const auto brandWidth = widthOf (fontFor (brand, kBrandSize), "ONIRIQUE");
    addText ("ONIRIQUE", { 24.0f, 116.0f }, kBrandSize, brand, Justification::centredLeft);
    addText ("COMPRESSOR", { 25.0f, 145.0f }, 10.5f, caps, Justification::centredLeft);
    addText ("OB 1", { 24.0f + brandWidth, 145.0f }, 10.5f, caps, Justification::centredRight);
    addText ("OPTOBOX", { 25.0f, 166.0f }, 10.5f, caps, Justification::centredLeft);
    addText ("MK I  /  II", { 24.0f + brandWidth, 166.0f }, 10.5f, caps, Justification::centredRight);

    addText ("OUTPUT", { kOutput.x, 131.0f }, 11.0f, caps);
    addText ("VOLUME", { kOutput.x, 143.0f }, 11.0f, caps);
    addText ("RATIO", { kRatio.x, 135.0f }, 11.0f, caps);
    addText ("THRESHOLD", { kThreshold.x, 135.0f }, 11.0f, caps);
    addText ("METER", { kMeter.x, 135.0f }, 11.0f, caps);
    addText ("ATTACK", { kAttack.x, 279.0f }, 11.0f, caps);
    addText ("RELEASE", { kRelease.x, 279.0f }, 11.0f, caps);
    addText ("attack/release", { kTiming.x, 271.0f }, 10.5f, lower);
    addText ("SELECT", { kTiming.x, 283.0f }, 11.0f, caps);
    addText ("sidechain", { kBus.x, 271.0f }, 10.5f, lower);
    addText ("BUS SELECT", { kBus.x, 283.0f }, 11.0f, caps);

    markKnob ("output", kOutput, output.radius,
              { { obx::kOutputOffDb, "off" }, { -10.0f, "-10" }, { 0.0f, "0" }, { 10.0f, "+10" }, { 20.0f, "+20" }, { 30.0f, "+30" } });
    markKnob ("threshold", kThreshold, threshold.radius,
              { { obx::kThresholdOffDb, "off" }, { 0.0f, "0" }, { -10.0f, "-10" }, { -20.0f, "-20" }, { -30.0f, "-30" }, { -40.0f, "-40" } });
    markKnob ("ratio", kRatio, ratio.radius, { { 2.0f, "2:1" }, { 10.0f, "10:1" } });
    markKnob ("attack", kAttack, attack.radius, { { 0.5f, "fast" }, { 300.0f, "slow" } });
    markKnob ("release", kRelease, release.radius, { { 50.0f, "fast" }, { 10000.0f, "slow" } });

    // The mix unit.
    generation = std::make_unique<hw::Toggle> (param ("generation"));
    lowCut = std::make_unique<hw::Toggle> (param ("lowcut"));

    for (auto [toggle, at] : { std::pair { generation.get(), kGeneration }, std::pair { lowCut.get(), kLowCut } })
    {
        addAndMakeVisible (*toggle);
        toggle->setBounds (Rectangle<int> (44, 44).withCentre (at.toInt()));
    }

    addSpot ("vintage", { kGeneration.x - 33.0f, kGeneration.y - 6.0f }, 10.0f, generation.get(), 0);
    addSpot ("mk I", { kGeneration.x - 33.0f, kGeneration.y + 6.0f }, 10.0f, generation.get(), 0);
    addSpot ("modern", { kGeneration.x + 33.0f, kGeneration.y - 6.0f }, 10.0f, generation.get(), 1);
    addSpot ("mk II", { kGeneration.x + 33.0f, kGeneration.y + 6.0f }, 10.0f, generation.get(), 1);
    addText ("OB 1", { kGeneration.x, 383.0f }, 11.0f, caps);
    addText ("GENERATION", { kGeneration.x, 395.0f }, 11.0f, caps);

    addSpot ("off", { kLowCut.x - 33.0f, kLowCut.y }, 10.0f, lowCut.get(), 0);
    addSpot ("80 Hz", { kLowCut.x, kLowCut.y - 21.0f }, 10.0f, lowCut.get(), 1);
    addSpot ("220", { kLowCut.x + 33.0f, kLowCut.y }, 10.0f, lowCut.get(), 2);
    addText ("sidechain", { kLowCut.x, 383.0f }, 10.5f, lower);
    addText ("LOW CUT", { kLowCut.x, 395.0f }, 11.0f, caps);

    placeKnob (parallel, "parallel", kParallel);
    markKnob ("parallel", kParallel, parallel.radius, { { 0.0f, "dry" }, { 100.0f, "wet" } });
    addText ("PARALLEL COMP.", { kParallel.x, 394.0f }, 11.0f, caps);

    addText ("ONIRIQUE", { 1010.0f, 344.0f }, 14.0f, brand);
    addText ("MIX UNIT   MU-1", { 1010.0f, 362.0f }, 8.5f, caps);
    addText (utf8 ("OPTOBOX   \xc2\xb7   OB 1"), { 1010.0f, 374.0f }, 8.5f, caps);

    screws = { { { 22.0f, 33.0f }, 0.4f }, { { 22.0f, 254.0f }, 1.9f }, { { 1078.0f, 33.0f }, 2.6f },
               { { 1078.0f, 254.0f }, 0.9f }, { { 22.0f, 328.0f }, 1.2f }, { { 1078.0f, 328.0f }, 2.2f } };

    // What each control does, after a moment on it.
    output.setTooltip (utf8 ("OUTPUT VOLUME \xe2\x80\x94 le gain apr\xc3\xa8s la compression, de off \xc3\xa0 +30 dB, "
                             "rendu par l'\xc3\xa9tage \xc3\xa0 lampe : pouss\xc3\xa9 fort, il colore."));
    ratio.setTooltip (utf8 ("RATIO \xe2\x80\x94 de 2:1 \xc3\xa0 10:1, avec le genou tr\xc3\xa8s doux de la cellule optique."));
    threshold.setTooltip (utf8 ("THRESHOLD \xe2\x80\x94 le niveau o\xc3\xb9 la compression atteint 1 dB, de off (+20) \xc3\xa0 -40 dB "
                                "sous 0 VU (0 VU = -18 dBFS)."));
    attack.setTooltip (utf8 ("ATTACK \xe2\x80\x94 0,5 \xc3\xa0 300 ms sur manual. Sur fix./man., le temps au-del\xc3\xa0 duquel "
                             "un pic n'est plus l\xc3\xa2" "ch\xc3\xa9 vite."));
    release.setTooltip (utf8 ("RELEASE \xe2\x80\x94 0,05 \xc3\xa0 10 s sur manual. La cellule y ajoute son propre retour, "
                              "plus lent apr\xc3\xa8s une longue compression."));
    meter->setTooltip (utf8 ("METER \xe2\x80\x94 ce que montre le VU : l'entr\xc3\xa9" "e, GR la r\xc3\xa9" "duction de gain "
                             "(l'aiguille descend depuis 0 VU), ou la sortie."));
    timing->setTooltip (utf8 ("ATTACK/RELEASE SELECT \xe2\x80\x94 fixed : 1 ms et 50 ms, la cellule fait le reste selon la musique. "
                              "fix./man. : les pics courts repartent vite, le reste suit les boutons. manual : ATTACK et RELEASE."));
    bus->setTooltip (utf8 ("SIDECHAIN BUS SELECT \xe2\x80\x94 int. : le compresseur \xc3\xa9" "coute ce qu'il traite. ext. : "
                           "il \xc3\xa9" "coute l'entr\xc3\xa9" "e sidechain du DAW (rien de branch\xc3\xa9, rien ne compresse)."));
    generation->setTooltip (utf8 ("GENERATION \xe2\x80\x94 vintage mk I : la cellule d'origine, plus lente, qui garde la m\xc3\xa9moire "
                                  "des passages forts ; plus de lampe, de transfo, un haut plus doux. modern mk II : plus vive, "
                                  "plus propre, plus ouverte."));
    lowCut->setTooltip (utf8 ("SIDECHAIN LOW CUT \xe2\x80\x94 retire les graves de ce que le d\xc3\xa9tecteur entend, \xc3\xa0 80 ou "
                              "220 Hz, sans toucher au son : la basse ne fait plus pomper le reste."));
    parallel.setTooltip (utf8 ("PARALLEL COMP. \xe2\x80\x94 m\xc3\xa9lange le son d'origine (dry) au son compress\xc3\xa9 (wet)."));
    lamp->setTooltip (utf8 ("Allum\xc3\xa9, le plugin est en circuit. Un clic le met en bypass."));
    vu.setTooltip (utf8 ("Le VU-m\xc3\xa8tre : 0 VU = -18 dBFS."));

    startTimerHz (60);
}

Faceplate::~Faceplate() = default;

//==============================================================================
void Faceplate::addText (const String& text, Point<float> centre, float size, int style, Justification justification)
{
    texts.push_back ({ text, centre, size, style, justification });
}

void Faceplate::addSpot (const String& text, Point<float> centre, float size, hw::Stepper* stepper, int index)
{
    const auto justification = std::abs (centre.x - (float) stepper->getBounds().getCentreX()) < 4.0f ? Justification::centred
                             : centre.x < (float) stepper->getBounds().getCentreX() ? Justification::centredRight
                                                                                    : Justification::centredLeft;
    addText (text, centre, size, lower, justification);

    Spot spot;
    spot.area = boxFor (text, fontFor (lower, size), centre, justification).expanded (4.0f, 2.0f);
    spot.stepper = stepper;
    spot.index = index;
    spots.push_back (spot);
}

void Faceplate::placeKnob (hw::Knob& knob, const char* id, Point<float> centre)
{
    addAndMakeVisible (knob);
    const auto size = roundToInt (knob.radius * 2.7f);
    knob.setBounds (Rectangle<int> (size, size).withCentre (centre.toInt()));
    attachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, id, knob));

    auto* parameter = processor.apvts.getParameter (id);
    knob.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    knob.getProperties().set ("parameter", id);
}

void Faceplate::markKnob (const char* id, Point<float> centre, float radius,
                          std::initializer_list<std::pair<float, const char*>> marks)
{
    auto* parameter = processor.apvts.getParameter (id);

    for (const auto& [value, text] : marks)
    {
        const auto angle = hw::kKnobStart + parameter->convertTo0to1 (value) * (hw::kKnobEnd - hw::kKnobStart);
        dots.push_back ({ onCircle (centre, radius + 8.5f, angle), 2.3f });

        // Beside the dot on the flanks, above it at the top.
        const auto flank = std::abs (angle) > degrees (60.0f);
        const auto justification = ! flank ? Justification::centred
                                 : angle < 0.0f ? Justification::centredRight : Justification::centredLeft;
        auto at = onCircle (centre, radius + (flank ? 14.0f : 20.0f), angle);

        if (flank)
            at.y += 1.0f;

        addText (text, at, 10.0f, lower, justification);

        Spot spot;
        spot.area = boxFor (text, fontFor (lower, 10.0f), at, justification).expanded (3.0f, 2.0f);
        spot.knob = parameter;
        spot.value = value;
        spots.push_back (spot);
    }
}

void Faceplate::placeSelector (hw::Selector& selector, Point<float> centre, std::initializer_list<const char*> positions,
                               float labelRadius)
{
    addAndMakeVisible (selector);
    selector.setBounds (Rectangle<int> (70, 70).withCentre (centre.toInt()));

    int i = 0;

    for (const auto* name : positions)
    {
        const auto angle = selector.angles[(size_t) i];
        const auto at = onCircle (centre, labelRadius, angle * 1.35f);
        const auto lines = StringArray::fromLines (name);

        for (int line = 0; line < lines.size(); ++line)
        {
            const auto y = at.y - 10.5f * (float) (lines.size() - 1 - line);
            addSpot (lines[line], { at.x, y }, 10.0f, &selector, i);
        }

        ++i;
    }
}

const Faceplate::Spot* Faceplate::spotAt (Point<float> position) const
{
    for (const auto& spot : spots)
        if (spot.area.contains (position))
            return &spot;

    return nullptr;
}

//==============================================================================
void Faceplate::renderPlates (Graphics& g) const
{
    g.fillAll (palette::night);

    auto plate = [&g] (Rectangle<float> r, float shade)
    {
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);

        ColourGradient ground (palette::plateTop.darker (shade), 0.0f, r.getY(), palette::plateLow.darker (shade), 0.0f, r.getBottom(), false);
        ground.addColour (0.4, palette::plate.darker (shade));
        g.setGradientFill (ground);
        g.fillRoundedRectangle (r, 3.0f);

        g.saveState();
        g.reduceClipRegion (r.toNearestInt());

        // The room's light, from above, and the ends of the rack in shadow.
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.075f), r.getCentreX(), r.getY() - r.getHeight() * 0.5f,
                                           Colours::white.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 0.55f, r.getY() - r.getHeight() * 0.5f, true));
        g.fillRect (r);

        for (auto side : { 0, 1 })
        {
            const auto edge = side == 0 ? r.getX() : r.getRight();
            const auto inward = side == 0 ? 90.0f : -90.0f;
            g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.22f), edge, 0.0f, Colours::transparentBlack, edge + inward, 0.0f, false));
            g.fillRect (r);
        }

        g.restoreState();

        // Its edges, a little rounded off.
        g.setColour (Colours::white.withAlpha (0.2f));
        g.drawLine (r.getX() + 3.0f, r.getY() + 0.6f, r.getRight() - 3.0f, r.getY() + 0.6f, 1.2f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + 0.6f, r.getY() + 3.0f, r.getX() + 0.6f, r.getBottom() - 3.0f, 1.0f);
        g.drawLine (r.getRight() - 0.6f, r.getY() + 3.0f, r.getRight() - 0.6f, r.getBottom() - 3.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.drawLine (r.getX() + 3.0f, r.getBottom() - 0.6f, r.getRight() - 3.0f, r.getBottom() - 0.6f, 1.2f);
    };

    plate ({ 0.0f, 0.0f, kWidth, kStripTop - 4.0f }, 0.0f);
    plate ({ 0.0f, kStripTop, kWidth, kHeight - kStripTop }, 0.08f);
}

void Faceplate::renderPrint (Graphics& g) const
{
    // The meter sits in a cut-out, the lamp in its hole.
    Path cut;
    cut.addRoundedRectangle (kVu, 5.0f);
    DropShadow (Colours::black.withAlpha (0.8f), 10, { 0, 3 }).drawForPath (g, cut);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawLine (kVu.getX() + 4.0f, kVu.getBottom() + 1.5f, kVu.getRight() - 4.0f, kVu.getBottom() + 1.5f, 1.0f);

    // The silkscreen.
    for (const auto& dot : dots)
    {
        g.setColour (palette::ink);
        g.fillEllipse (Rectangle<float> (dot.second * 2.0f, dot.second * 2.0f).withCentre (dot.first));
    }

    for (const auto& t : texts)
    {
        const auto font = fontFor (t.style, t.size);
        g.setFont (font);
        g.setColour (palette::ink.withAlpha (t.style == brand ? 0.97f : 0.93f));
        g.drawText (t.text, boxFor (t.text, font, t.centre, t.justification), t.justification, false);
    }

    for (const auto& [centre, angle] : screws)
        hw::drawScrew (g, centre, 6.5f, angle);
}

void Faceplate::paint (Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (panel.isNull() || std::abs (scale - panelScale) > 0.001f)
    {
        panelScale = scale;
        panel = Image (Image::ARGB, roundToInt (kWidth * scale), roundToInt (kHeight * scale), true);

        {
            Graphics pg (panel);
            pg.addTransform (AffineTransform::scale (scale));
            renderPlates (pg);
        }

        // The grain of the powder coat.
        {
            Image::BitmapData data (panel, Image::BitmapData::readWrite);
            uint32 seed = 0x2545f491u;

            for (int y = 0; y < data.height; ++y)
                for (int x = 0; x < data.width; ++x)
                {
                    seed = seed * 1664525u + 1013904223u;
                    const auto n = (int) (seed >> 28) - 8;
                    auto* px = reinterpret_cast<PixelARGB*> (data.getPixelPointer (x, y));
                    px->setARGB (px->getAlpha(), (uint8) jlimit (0, 255, px->getRed() + n),
                                 (uint8) jlimit (0, 255, px->getGreen() + n / 2), (uint8) jlimit (0, 255, px->getBlue() + n / 2));
                }
        }

        Graphics pg (panel);
        pg.addTransform (AffineTransform::scale (scale));
        renderPrint (pg);
    }

    g.drawImageTransformed (panel, AffineTransform::scale (1.0f / panelScale));
}

//==============================================================================
Rectangle<float> Faceplate::bubbleFor (const hw::Knob& knob) const
{
    const auto font = hw::panelFont (11.5f, true);
    const auto c = knob.getBounds().toFloat().getCentre();
    return Rectangle<float> (widthOf (font, shownValue) + 30.0f, 20.0f).withCentre ({ c.x, c.y - knob.radius - 20.0f });
}

void Faceplate::paintOverChildren (Graphics& g)
{
    if (shownKnob == nullptr || shownValue.isEmpty())
        return;

    // The value, in the capsule every Onirique plugin shows it in.
    const auto r = bubbleFor (*shownKnob);
    const auto radius = r.getHeight() * 0.5f;

    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), radius);
    g.setColour (palette::deep2.withAlpha (0.97f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (palette::ember.withAlpha (0.75f));
    g.drawRoundedRectangle (r, radius, 1.0f);

    const auto led = Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 11.0f, r.getCentreY() });
    g.setColour (palette::ember.withAlpha (0.35f));
    g.fillEllipse (led.expanded (2.5f));
    g.setColour (palette::ember);
    g.fillEllipse (led);

    g.setColour (palette::mist);
    g.setFont (hw::panelFont (11.5f, true));
    g.drawText (shownValue, r.withTrimmedLeft (18.0f).withTrimmedRight (6.0f), Justification::centred, false);
}

void Faceplate::timerCallback()
{
    vu.setNeedle (processor.getNeedle());

    hw::Knob* active = nullptr;

    for (auto* knob : { &output, &ratio, &threshold, &attack, &release, &parallel })
        if (knob->isMouseOverOrDragging())
            active = knob;

    String text;

    if (active != nullptr)
        if (auto* parameter = processor.apvts.getParameter (active->getProperties()["parameter"].toString()))
            text = parameter->getCurrentValueAsText();

    if (active != shownKnob || text != shownValue)
    {
        if (shownKnob != nullptr)
            repaint (bubbleFor (*shownKnob).expanded (4.0f).toNearestInt());

        shownKnob = active;
        shownValue = text;

        if (shownKnob != nullptr)
            repaint (bubbleFor (*shownKnob).expanded (4.0f).toNearestInt());
    }
}

void Faceplate::mouseMove (const MouseEvent& e)
{
    setMouseCursor (spotAt (e.position) != nullptr ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void Faceplate::mouseDown (const MouseEvent& e)
{
    const auto* spot = spotAt (e.position);

    if (spot == nullptr)
        return;

    if (spot->stepper != nullptr)
    {
        spot->stepper->setIndex (spot->index);
    }
    else if (spot->knob != nullptr)
    {
        spot->knob->beginChangeGesture();
        spot->knob->setValueNotifyingHost (spot->knob->convertTo0to1 (spot->value));
        spot->knob->endChangeGesture();
    }
}

} // namespace onirique
