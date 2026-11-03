#include "PluginEditor.h"

using namespace klxui;

namespace
{
    constexpr int kW = 440, kH = 524;

    juce::String stageWord (double v)
    {
        if (v <= 0.0)   return juce::String (juce::CharPointer_UTF8 ("REPOS"));
        if (v < 35.0)   return juce::String (juce::CharPointer_UTF8 ("MONT\xc3\x89" "E"));
        if (v < 70.0)   return juce::String (juce::CharPointer_UTF8 ("TENSION"));
        if (v < 99.95)  return juce::String (juce::CharPointer_UTF8 ("CLIMAX"));
        return juce::String (juce::CharPointer_UTF8 ("KLIMAX"));
    }
}

// ------------------------------------------------------------------ potard
void KnobLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                 float a0, float a1, juce::Slider& s)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float r = size * 0.5f - 14.0f;
    const float ang = a0 + pos * (a1 - a0);

    // graduations (tous les 10 %)
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (float) i / 10.0f * (a1 - a0);
        const bool lit = (float) i / 10.0f <= pos + 1.0e-4f && pos > 0.0f;
        const float r0 = r + 6.0f, r1 = r + (i % 5 == 0 ? 13.0f : 10.0f);
        g.setColour (lit ? accent.withAlpha (0.85f) : edge);
        g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), 2.0f);
    }

    // piste
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 6.0f, r - 6.0f, 0.0f, a0, a1, true);
    g.setColour (panel2);
    g.strokePath (track, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // arc de valeur + halo qui s'intensifie
    if (pos > 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r - 6.0f, r - 6.0f, 0.0f, a0, ang, true);
        g.setColour (accent.withAlpha (0.05f + 0.20f * pos));
        g.strokePath (arc, juce::PathStrokeType (24.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // corps du potard
    const float rb = r - 26.0f;
    juce::ColourGradient body (panel2.brighter (0.08f), c.x, c.y - rb, juce::Colour (0xff0d0f14), c.x, c.y + rb, false);
    g.setGradientFill (body);
    g.fillEllipse (c.x - rb, c.y - rb, 2 * rb, 2 * rb);
    g.setColour (edge);
    g.drawEllipse (c.x - rb, c.y - rb, 2 * rb, 2 * rb, 1.5f);
    g.setColour (accent.withAlpha (0.10f + 0.35f * pos));
    g.drawEllipse (c.x - rb + 5, c.y - rb + 5, 2 * rb - 10, 2 * rb - 10, 1.0f);

    // repère
    const float i0 = rb - 22.0f, i1 = rb - 6.0f;
    g.setColour (pos > 0.0f ? accent : text);
    g.drawLine (c.x + i0 * std::sin (ang), c.y - i0 * std::cos (ang), c.x + i1 * std::sin (ang), c.y - i1 * std::cos (ang), 4.0f);

    // valeur et étape
    const double v = s.getValue();
    g.setColour (white);
    g.setFont (font (46.0f));
    g.drawText (juce::String (juce::roundToInt (v)) + " %", juce::Rectangle<float> (c.x - rb, c.y - 34.0f, 2 * rb, 52.0f), juce::Justification::centred);
    auto f = font (12.5f);
    f.setExtraKerningFactor (0.18f);
    g.setFont (f);
    g.setColour (pos > 0.0f ? accent : dim);
    g.drawText (stageWord (v), juce::Rectangle<float> (c.x - rb, c.y + 18.0f, 2 * rb, 18.0f), juce::Justification::centred);
}

// ------------------------------------------------------------------ presets
void PresetPill::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (selected)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (bg);
    }
    else
    {
        g.setColour (down ? panel2.brighter (0.1f) : (over ? panel2 : panel));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (over ? accent.withAlpha (0.6f) : edge);
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.2f);
        g.setColour (over ? white : klxui::text);
    }
    auto f = font (11.5f);
    f.setExtraKerningFactor (0.06f);
    g.setFont (f);
    g.drawText (getButtonText(), b, juce::Justification::centred);
}

// ------------------------------------------------------------------ éditeur
KlimaxEditor::KlimaxEditor (KlimaxProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);

    knob.setLookAndFeel (&look);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setMouseDragSensitivity (320);
    knob.setDoubleClickReturnValue (true, 0.0);
    knob.setScrollWheelEnabled (true);
    knob.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    knob.setTitle ("Klimax");
    addAndMakeVisible (knob);
    knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "klimax", knob);
    knob.onValueChange = [this] { repaint (knobArea.expanded (40)); };

    for (int i = 0; i < klx::kNumPresets; ++i)
    {
        auto* pill = pills.add (new PresetPill (i));
        pill->onClick = [this, i] { proc.selectPreset (i); };
        addAndMakeVisible (pill);
    }

    presetAttachment = std::make_unique<juce::ParameterAttachment> (
        *proc.apvts.getParameter ("preset"),
        [this] (float v) { presetChanged (juce::roundToInt (v)); }, nullptr);
    presetAttachment->sendInitialUpdate();

    setSize (kW, kH);
}

KlimaxEditor::~KlimaxEditor()
{
    knob.setLookAndFeel (nullptr);
}

void KlimaxEditor::presetChanged (int index)
{
    currentPreset = juce::jlimit (0, klx::kNumPresets - 1, index);
    for (auto* p : pills)
    {
        p->selected = (p->index == currentPreset);
        p->repaint();
    }
    repaint();
}

void KlimaxEditor::resized()
{
    knobArea = { (kW - 320) / 2, 54, 320, 320 };
    knob.setBounds (knobArea);
    descArea = { 24, 378, kW - 48, 34 };

    const int margin = 20, gap = 8, pw = (kW - 2 * margin - 3 * gap) / 4, ph = 34;
    for (int i = 0; i < pills.size(); ++i)
    {
        const int row = i / 4, col = i % 4;
        pills[i]->setBounds (margin + col * (pw + gap), 424 + row * (ph + gap), pw, ph);
    }
}

void KlimaxEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // halo derrière le potard, qui monte avec la valeur
    const float v = (float) (knob.getValue() / 100.0);
    const auto c = knobArea.toFloat().getCentre();
    juce::ColourGradient glow (accent.withAlpha (0.03f + 0.20f * v * v), c.x, c.y,
                               accent.withAlpha (0.0f), c.x + 210.0f, c.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (c.x - 210.0f, c.y - 210.0f, 420.0f, 420.0f);

    // logo : « mask:: » blanc, « klimax » jaune
    g.setFont (font (24.0f));
    const juce::String a = "mask::", b = "klimax";
    const int wa = juce::GlyphArrangement::getStringWidthInt (font (24.0f), a);
    g.setColour (white);
    g.drawText (a, 22, 14, wa + 4, 30, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText (b, 22 + wa, 14, 200, 30, juce::Justification::centredLeft);

    // nom + description du preset
    {
        const juce::String name = klx::presetName (currentPreset);
        const int wn = juce::GlyphArrangement::getStringWidthInt (font (13.0f), name);
        g.setColour (white);
        g.setFont (font (13.0f));
        g.drawText (name, kW - 22 - wn - 2, 14, wn + 2, 30, juce::Justification::centredRight);
        g.setColour (dim);
        g.setFont (font (10.5f));
        g.drawText ("PRESET", kW - 22 - wn - 70, 14, 62, 30, juce::Justification::centredRight);
    }

    g.setColour (text);
    g.setFont (font (13.0f, false));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 (klx::presetDescription (currentPreset))),
                      descArea, juce::Justification::centred, 2);
}
