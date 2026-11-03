#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace klxui
{
    const juce::Colour bg      { 0xff08090c };
    const juce::Colour panel   { 0xff12141a };
    const juce::Colour panel2  { 0xff1a1d25 };
    const juce::Colour edge    { 0xff262a35 };
    const juce::Colour white   { 0xfff4f6fb };
    const juce::Colour dim     { 0xff7d8494 };
    const juce::Colour text    { 0xffc9cdd6 };
    const juce::Colour accent  { 0xffffe04a };   // jaune : couleur dominante de mask::klimax

    inline juce::Font font (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class KnobLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class PresetPill : public juce::Button
{
public:
    PresetPill (int i) : juce::Button (klx::presetName (i)), index (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    const int index;
    bool selected = false;
};

class KlimaxEditor : public juce::AudioProcessorEditor
{
public:
    explicit KlimaxEditor (KlimaxProcessor&);
    ~KlimaxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void presetChanged (int index);

    KlimaxProcessor& proc;
    KnobLook look;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachment;
    std::unique_ptr<juce::ParameterAttachment> presetAttachment;
    juce::OwnedArray<PresetPill> pills;
    int currentPreset = 1;
    juce::Rectangle<int> knobArea, descArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KlimaxEditor)
};
