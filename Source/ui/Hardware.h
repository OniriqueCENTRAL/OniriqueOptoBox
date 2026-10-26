// Onirique OptoBox — the parts of the front panel: knobs, switches, the lamp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace onirique::hw
{

using namespace juce;

//==============================================================================
// The panel is painted in the red of the Onirique range instead of the
// original's blue; everything mounted on it is as on the hardware.
namespace palette
{
    inline const Colour night     { 0xff0d0809 };
    inline const Colour plateTop  { 0xff6e0c19 };
    inline const Colour plate     { 0xff5c0812 };
    inline const Colour plateLow  { 0xff46050d };
    inline const Colour ink       { 0xfffff0f1 };    // the silkscreen
    inline const Colour ember     { 0xffe23444 };
    inline const Colour deep2     { 0xff2a0a0e };
    inline const Colour mist      { 0xfff6eeee };
}

/** The sans-serif of the silkscreen, named outright: the "default" placeholder
    does not survive being put in FontOptions. */
Font panelFont (float height, bool bold = true);

/** The wide, heavy face the range writes its name in. */
Font brandFont (float height);

/** Angles of every rotary control: 0 is straight up, clockwise. */
constexpr float kKnobStart = -130.0f * MathConstants<float>::pi / 180.0f;
constexpr float kKnobEnd = 130.0f * MathConstants<float>::pi / 180.0f;

inline Point<float> onCircle (Point<float> centre, float radius, float angle)
{
    return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
}

/** A slotted chrome screw, at an angle of its own. */
void drawScrew (Graphics&, Point<float> centre, float radius, float angle);

//==============================================================================
/** The large black knob with its fluted skirt and a white line to point
    with; or the smaller one of the mix unit. Dragged up or across; Shift
    for fine steps, a double click for where it started. */
class Knob final : public Slider
{
public:
    Knob (float radius, int flutes);

    void paint (Graphics&) override;
    void mouseDown (const MouseEvent&) override;

    /** The knob proper, in its own bounds: the rest is room for its shadow. */
    float radius;

private:
    int flutes;
};

//==============================================================================
/** A control that sits in one of a few positions: a rotary switch or a
    toggle. A click moves it on to the next position (round again after the
    last); a drag or the wheel moves it either way. */
class Stepper : public Component,
                public SettableTooltipClient
{
public:
    explicit Stepper (RangedAudioParameter&);

    int getIndex() const noexcept { return index; }
    int getNumPositions() const noexcept { return numPositions; }

    /** Moves it to a position, as one change the host can undo. */
    void setIndex (int);

    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;

protected:
    int index = 0;
    int numPositions = 2;

private:
    ParameterAttachment attachment;
    int dragFrom = 0;
    bool dragged = false;
    float wheel = 0.0f;
};

/** The black rotary switch with its pointer bar: one angle per position. */
class Selector final : public Stepper
{
public:
    Selector (RangedAudioParameter&, std::vector<float> angles);

    void paint (Graphics&) override;

    const std::vector<float> angles;
};

/** A chrome bat-handle toggle, thrown to the left or the right (and for
    three positions, straight out in between). */
class Toggle final : public Stepper
{
public:
    explicit Toggle (RangedAudioParameter&);

    void paint (Graphics&) override;
};

//==============================================================================
/** The red jewel lamp. On the hardware it says the unit is on; here, lit,
    that the plugin is in the circuit: a click bypasses it. */
class Lamp final : public Component,
                   public SettableTooltipClient
{
public:
    explicit Lamp (RangedAudioParameter& bypass);

    void paint (Graphics&) override;
    void mouseDown (const MouseEvent&) override;

private:
    ParameterAttachment attachment;
    bool bypassed = false;
};

} // namespace onirique::hw
