// Onirique OptoBox — the VU meter, lit from behind by a cool blue LED
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace onirique::hw
{

using namespace juce;

/** The meter in its black bezel. The face, its scale and its lettering are
    drawn once for the size they are shown at; only the needle moves. */
class VuMeter final : public Component,
                      public SettableTooltipClient
{
public:
    VuMeter();

    /** Where the needle is: 1 is 0 VU, 1.4125 (+3) the end of the scale. */
    void setNeedle (float deflection);

    void paint (Graphics&) override;
    void resized() override { face = {}; }

    /** The face's size, at which the component is laid out. */
    static constexpr float kWidth = 232.0f, kHeight = 118.0f;

private:
    void paintFace (Graphics&) const;
    Point<float> pivot() const;
    float angleFor (float deflection) const;

    Image face;
    float faceScale = 0.0f;
    float needle = 1.0f;
};

} // namespace onirique::hw
