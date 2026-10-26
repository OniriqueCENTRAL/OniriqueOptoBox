// Onirique OptoBox — the front panel: the compressor, and the mix unit under it
#pragma once

#include "../PluginProcessor.h"
#include "Hardware.h"
#include "VuMeter.h"

namespace onirique
{

/** Laid out as the hardware is, at one size: the editor scales it as a
    whole. The panel, its screws and its lettering are drawn once for the
    size they are shown at; a word printed by a switch or a knob is also a
    place to click, that puts it there. */
class Faceplate final : public Component,
                        private Timer
{
public:
    static constexpr float kWidth = 1100.0f, kHeight = 412.0f;
    static constexpr float kStripTop = 300.0f;   // the mix unit's panel, under the compressor's

    explicit Faceplate (OptoBoxProcessor&);
    ~Faceplate() override;

    void paint (Graphics&) override;
    void paintOverChildren (Graphics&) override;

    void mouseMove (const MouseEvent&) override;
    void mouseDown (const MouseEvent&) override;

private:
    using SliderAttachment = AudioProcessorValueTreeState::SliderAttachment;

    struct Text
    {
        String text;
        Point<float> centre;
        float size;
        int style;                 // a position, a control's name, or the maker's
        Justification justification = Justification::centred;
    };

    /** A printed word that sets its control when clicked. */
    struct Spot
    {
        Rectangle<float> area;
        hw::Stepper* stepper = nullptr;
        int index = 0;
        RangedAudioParameter* knob = nullptr;
        float value = 0.0f;
    };

    void timerCallback() override;

    void renderPlates (Graphics&) const;
    void renderPrint (Graphics&) const;
    void addText (const String&, Point<float> centre, float size, int style,
                  Justification = Justification::centred);
    void addSpot (const String&, Point<float> centre, float size, hw::Stepper*, int index);
    void placeKnob (hw::Knob&, const char* id, Point<float> centre);
    void markKnob (const char* id, Point<float> centre, float radius,
                   std::initializer_list<std::pair<float, const char*>> marks);
    void placeSelector (hw::Selector&, Point<float> centre, std::initializer_list<const char*> positions,
                        float labelRadius);
    const Spot* spotAt (Point<float>) const;
    Rectangle<float> bubbleFor (const hw::Knob&) const;

    OptoBoxProcessor& processor;

    hw::Knob output { 33.0f, 9 }, ratio { 33.0f, 9 }, threshold { 33.0f, 9 },
             attack { 33.0f, 9 }, release { 33.0f, 9 }, parallel { 25.0f, 8 };
    std::unique_ptr<hw::Selector> meter, timing, bus;
    std::unique_ptr<hw::Toggle> generation, lowCut;
    std::unique_ptr<hw::Lamp> lamp;
    hw::VuMeter vu;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<Text> texts;
    std::vector<Spot> spots;
    std::vector<std::pair<Point<float>, float>> dots;   // the marks around the knobs
    std::vector<std::pair<Point<float>, float>> screws;

    Image panel;
    float panelScale = 0.0f;

    // The value of the knob under the pointer, or being turned.
    hw::Knob* shownKnob = nullptr;
    String shownValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Faceplate)
};

} // namespace onirique
