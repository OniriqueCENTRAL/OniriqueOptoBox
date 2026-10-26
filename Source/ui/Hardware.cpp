#include "Hardware.h"

#include "License/LicenseTheme.h"

namespace onirique::hw
{

Font panelFont (float height, bool bold)
{
   #if JUCE_WINDOWS
    const String family ("Segoe UI");
   #elif JUCE_MAC
    const String family ("Helvetica Neue");
   #else
    const String family ("DejaVu Sans");
   #endif

    return Font (FontOptions (family, height, bold ? Font::bold : Font::plain));
}

Font brandFont (float height)
{
    const onirique::license::theme::Fonts fonts;
    return fonts->displayFont (height);
}

void drawScrew (Graphics& g, Point<float> c, float r, float angle)
{
    // The countersink it sits in.
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), c.x, c.y, Colours::transparentBlack, c.x, c.y + r * 1.5f, true));
    g.fillEllipse (Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (c.translated (0.0f, r * 0.15f)));

    const auto head = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xfff2f2f0), c.x - r * 0.6f, c.y - r * 0.7f,
                                       Colour (0xff5d5d60), c.x + r * 0.8f, c.y + r * 0.9f, true));
    g.fillEllipse (head);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (head, r * 0.12f);

    // The slot, with the light catching its far edge.
    const auto along = Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.86f);
    const auto across = Point<float> (-along.y, along.x) * 0.14f;
    g.setColour (Colour (0xff1c1c1e));
    g.drawLine ({ c - along, c + along }, r * 0.3f);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.drawLine ({ c - along + across, c + along + across }, r * 0.08f);
}

//==============================================================================
Knob::Knob (float r, int numFlutes) : radius (r), flutes (numFlutes)
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (kKnobStart, kKnobEnd, true);
    setMouseDragSensitivity (250);
    setScrollWheelEnabled (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Knob::mouseDown (const MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 2000 : 250);
    Slider::mouseDown (e);
}

void Knob::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto r = radius;
    const auto angle = kKnobStart + (float) valueToProportionOfLength (getValue()) * (kKnobEnd - kKnobStart);

    // The fluted skirt turns with the knob; the light stays where it is.
    Path skirt;
    constexpr int steps = 180;

    for (int i = 0; i <= steps; ++i)
    {
        const auto phi = MathConstants<float>::twoPi * (float) i / (float) steps;
        const auto lobe = std::pow (0.5f + 0.5f * std::cos ((float) flutes * (phi - angle)), 0.7f);
        const auto rr = r * (0.93f + 0.07f * lobe);
        const auto p = onCircle (c, rr, phi);

        if (i == 0)
            skirt.startNewSubPath (p);
        else
            skirt.lineTo (p);
    }

    skirt.closeSubPath();

    DropShadow (Colours::black.withAlpha (0.75f), roundToInt (r * 0.32f), { 0, roundToInt (r * 0.16f) }).drawForPath (g, skirt);

    g.setGradientFill (ColourGradient (Colour (0xff3a3a3c), c.x - r * 0.5f, c.y - r * 0.8f,
                                       Colour (0xff050505), c.x + r * 0.5f, c.y + r * 0.9f, false));
    g.fillPath (skirt);

    // The flutes catch the light along their crests.
    g.saveState();
    g.reduceClipRegion (skirt);

    for (int i = 0; i < flutes; ++i)
    {
        const auto phi = angle + MathConstants<float>::twoPi * (float) i / (float) flutes;
        const auto lit = 0.5f + 0.5f * std::cos (phi + 0.6f);   // facing the light at the upper left
        g.setColour (Colours::white.withAlpha (0.04f + 0.13f * lit * lit));
        g.drawLine ({ onCircle (c, r * 0.8f, phi), onCircle (c, r * 1.0f, phi) }, r * 0.07f);
    }

    g.restoreState();

    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (skirt, PathStrokeType (r * 0.03f));

    // The cap: a dome of glossy bakelite.
    const auto cap = Rectangle<float> (r * 1.56f, r * 1.56f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawEllipse (cap.expanded (r * 0.02f), r * 0.05f);

    g.setGradientFill (ColourGradient (Colour (0xff3c3c3f), c.x - r * 0.25f, c.y - r * 0.4f,
                                       Colour (0xff070707), c.x + r * 0.55f, c.y + r * 0.65f, true));
    g.fillEllipse (cap);

    const auto shine = Rectangle<float> (r * 1.0f, r * 0.62f).withCentre (c.translated (-r * 0.12f, -r * 0.4f));
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), shine.getCentreX(), shine.getY(),
                                       Colours::white.withAlpha (0.0f), shine.getCentreX(), shine.getBottom(), false));
    g.fillEllipse (shine);

    // The pointer: a white line from near the middle out over the skirt.
    const Line<float> pointer (onCircle (c, r * 0.18f, angle), onCircle (c, r * 0.97f, angle));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawLine (pointer.withShortenedStart (-0.5f), r * 0.12f);
    g.setColour (Colour (0xfff4f1ec));
    g.drawLine (pointer, r * 0.075f);
}

//==============================================================================
Stepper::Stepper (RangedAudioParameter& parameter)
    : attachment (parameter, [this] (float value)
                  {
                      index = jlimit (0, numPositions - 1, roundToInt (value));
                      repaint();
                  })
{
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (&parameter))
        numPositions = choice->choices.size();

    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void Stepper::setIndex (int i)
{
    i = jlimit (0, numPositions - 1, i);

    if (i != index)
        attachment.setValueAsCompleteGesture ((float) i);
}

void Stepper::mouseDown (const MouseEvent&)
{
    dragFrom = index;
    dragged = false;
}

void Stepper::mouseDrag (const MouseEvent& e)
{
    const auto travel = e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY();
    const auto steps = travel / 16;

    if (steps != 0)
        dragged = true;

    if (dragged)
        setIndex (dragFrom + steps);
}

void Stepper::mouseUp (const MouseEvent& e)
{
    if (! dragged && e.mouseWasClicked())
        setIndex ((index + 1) % numPositions);
}

void Stepper::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    wheel += (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : -w.deltaY) * (w.isReversed ? -1.0f : 1.0f);

    if (std::abs (wheel) >= 0.12f)
    {
        setIndex (index + (wheel > 0.0f ? 1 : -1));
        wheel = 0.0f;
    }
}

//==============================================================================
Selector::Selector (RangedAudioParameter& parameter, std::vector<float> positionAngles)
    : Stepper (parameter), angles (std::move (positionAngles))
{
}

void Selector::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto r = (float) getWidth() * 0.3f;
    const auto angle = angles[(size_t) jlimit (0, (int) angles.size() - 1, index)];

    // The round base the bar sits on.
    const auto base = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    Path round;
    round.addEllipse (base);
    DropShadow (Colours::black.withAlpha (0.7f), roundToInt (r * 0.35f), { 0, roundToInt (r * 0.2f) }).drawForPath (g, round);
    g.setGradientFill (ColourGradient (Colour (0xff2e2e30), c.x - r * 0.5f, c.y - r * 0.7f,
                                       Colour (0xff060606), c.x + r * 0.6f, c.y + r * 0.8f, false));
    g.fillPath (round);

    // The bar, wider at its tail, pointing at the position.
    Path bar;
    const auto tip = r * 1.32f, tail = r * 0.95f;
    bar.startNewSubPath (-r * 0.2f, -tip);
    bar.quadraticTo (0.0f, -tip - r * 0.12f, r * 0.2f, -tip);
    bar.lineTo (r * 0.34f, tail);
    bar.quadraticTo (0.0f, tail + r * 0.2f, -r * 0.34f, tail);
    bar.closeSubPath();
    bar.applyTransform (AffineTransform::rotation (angle).translated (c));

    DropShadow (Colours::black.withAlpha (0.8f), roundToInt (r * 0.3f), { roundToInt (r * 0.08f), roundToInt (r * 0.25f) }).drawForPath (g, bar);
    g.setGradientFill (ColourGradient (Colour (0xff353538), c.x - r, c.y - r,
                                       Colour (0xff0a0a0a), c.x + r, c.y + r, false));
    g.fillPath (bar);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.strokePath (bar, PathStrokeType (r * 0.05f));

    g.setColour (Colour (0xfff4f1ec));
    g.drawLine ({ onCircle (c, r * 0.05f, angle), onCircle (c, r * 1.25f, angle) }, r * 0.1f);
}

//==============================================================================
Toggle::Toggle (RangedAudioParameter& parameter) : Stepper (parameter)
{
}

void Toggle::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto r = (float) getHeight() * 0.18f;

    // The nut, a hexagon of chrome on a washer.
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (r * 2.9f, r * 2.9f).withCentre (c.translated (0.0f, r * 0.25f)));
    g.setGradientFill (ColourGradient (Colour (0xffd9d9db), c.x - r, c.y - r, Colour (0xff555558), c.x + r, c.y + r, false));
    g.fillEllipse (Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));

    Path nut;
    nut.addPolygon (c, 6, r * 1.05f, MathConstants<float>::pi / 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff4f4f4), c.x - r * 0.6f, c.y - r * 0.8f, Colour (0xff6b6b70), c.x + r, c.y + r, false));
    g.fillPath (nut);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (nut, PathStrokeType (r * 0.08f));

    // The lever, thrown to one side, or out towards us in the middle.
    const auto middle = numPositions == 3 && index == 1;
    const auto side = numPositions == 3 ? (float) (index - 1) : (index == 0 ? -1.0f : 1.0f);

    if (middle)
    {
        const auto ball = Rectangle<float> (r * 1.25f, r * 1.25f).withCentre (c.translated (0.0f, -r * 0.1f));
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (ball.translated (0.0f, r * 0.35f).expanded (r * 0.15f));
        g.setGradientFill (ColourGradient (Colours::white, ball.getX() + r * 0.3f, ball.getY() + r * 0.25f,
                                           Colour (0xff77777c), ball.getRight(), ball.getBottom(), true));
        g.fillEllipse (ball);
        return;
    }

    const auto length = r * 2.9f;
    Path lever;
    lever.startNewSubPath (0.0f, -r * 0.42f);
    lever.lineTo (length, -r * 0.3f);
    lever.lineTo (length, r * 0.3f);
    lever.lineTo (0.0f, r * 0.42f);
    lever.closeSubPath();
    lever.addEllipse (length - r * 0.42f, -r * 0.42f, r * 0.84f, r * 0.84f);
    lever.applyTransform (AffineTransform::scale (side, 1.0f).translated (c));

    DropShadow (Colours::black.withAlpha (0.7f), roundToInt (r * 0.5f), { 0, roundToInt (r * 0.45f) }).drawForPath (g, lever);
    g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - r * 0.45f, Colour (0xff6d6d72), c.x, c.y + r * 0.45f, false));
    g.fillPath (lever);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (lever, PathStrokeType (r * 0.06f));
}

//==============================================================================
Lamp::Lamp (RangedAudioParameter& bypass)
    : attachment (bypass, [this] (float value)
                  {
                      bypassed = value > 0.5f;
                      repaint();
                  })
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void Lamp::mouseDown (const MouseEvent&)
{
    attachment.setValueAsCompleteGesture (bypassed ? 0.0f : 1.0f);
}

void Lamp::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto r = (float) getWidth() * 0.22f;
    const auto lens = Rectangle<float> (r * 1.34f, r * 1.34f).withCentre (c);

    // The light it throws on the panel around it.
    if (! bypassed)
    {
        g.setGradientFill (ColourGradient (Colour (0x80ff3a2a), c.x, c.y, Colour (0x00ff3a2a), c.x + r * 2.2f, c.y, true));
        g.fillEllipse (Rectangle<float> (r * 4.4f, r * 4.4f).withCentre (c));
    }

    // The chrome bezel.
    const auto bezel = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (bezel.expanded (r * 0.12f).translated (0.0f, r * 0.12f));
    g.setGradientFill (ColourGradient (Colour (0xfff0f0f2), c.x - r * 0.7f, c.y - r * 0.8f, Colour (0xff4b4b50), c.x + r * 0.7f, c.y + r * 0.9f, false));
    g.fillEllipse (bezel);
    g.setGradientFill (ColourGradient (Colour (0xff505055), c.x - r * 0.5f, c.y - r * 0.6f, Colour (0xffe6e6e8), c.x + r * 0.5f, c.y + r * 0.6f, false));
    g.fillEllipse (bezel.reduced (r * 0.16f));

    // The jewel.
    if (bypassed)
        g.setGradientFill (ColourGradient (Colour (0xff5a0c0c), c.x - r * 0.2f, c.y - r * 0.25f, Colour (0xff1c0303), c.x + r * 0.6f, c.y + r * 0.6f, true));
    else
        g.setGradientFill (ColourGradient (Colour (0xfffff3ea), c.x, c.y, Colour (0xffb0120e), c.x + r * 0.67f, c.y, true));

    g.fillEllipse (lens);

    if (! bypassed)
    {
        g.setGradientFill (ColourGradient (Colour (0x00ff5040), c.x, c.y, Colour (0xccff5a3c), c.x + r * 0.67f, c.y, true));
        g.fillEllipse (lens);
    }

    // The facets' glint.
    g.setColour (Colours::white.withAlpha (bypassed ? 0.25f : 0.55f));
    g.fillEllipse (Rectangle<float> (r * 0.36f, r * 0.24f).withCentre (c.translated (-r * 0.28f, -r * 0.34f)));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (lens, r * 0.06f);
}

} // namespace onirique::hw
