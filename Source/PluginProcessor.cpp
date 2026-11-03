#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout KlimaxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "klimax", 1 }, "Klimax",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.,").replaceCharacter (',', '.').getFloatValue(); })));

    juce::StringArray names;
    for (int i = 0; i < klx::kNumPresets; ++i) names.add (klx::presetName (i));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "preset", 1 }, "Preset", names, 1,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    return layout;
}

KlimaxProcessor::KlimaxProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KLIMAX", createLayout())
{
    knobParam   = apvts.getRawParameterValue ("klimax");
    presetParam = apvts.getRawParameterValue ("preset");
    engine.prepare (48000.0);
    setLatencySamples (engine.latencySamples());
}

bool KlimaxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();
    return false;
}

void KlimaxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setPresetImmediate (getPresetIndex());
    setLatencySamples (engine.latencySamples());
    scratch.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
}

void KlimaxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0) return;

    klx::Engine::Transport tr;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tr.bpm = *b;
            if (auto p = pos->getPpqPosition()) { tr.ppq = *p; tr.hasPpq = true; }
            tr.playing = pos->getIsPlaying();
        }

    engine.setPreset (getPresetIndex());
    const float knob = juce::jlimit (0.0f, 1.0f, knobParam->load() * 0.01f);

    if (numOut >= 2)
    {
        if (numIn < 2) buffer.copyFrom (1, 0, buffer, 0, 0, n);   // mono → stéréo
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, knob, tr);
    }
    else
    {
        // mono → mono : traitement stéréo interne, sortie = (G + D) / 2
        float* L = buffer.getWritePointer (0);
        for (int off = 0; off < n;)
        {
            const int m = juce::jmin (n - off, (int) scratch.size());
            std::copy (L + off, L + off + m, scratch.begin());
            engine.process (L + off, scratch.data(), m, knob, tr);
            for (int i = 0; i < m; ++i) L[off + i] = 0.5f * (L[off + i] + scratch[(size_t) i]);
            off += m;
            tr.ppq += (double) m / getSampleRate() * tr.bpm / 60.0;
        }
    }
}

void KlimaxProcessor::selectPreset (int index)
{
    if (auto* p = apvts.getParameter ("preset"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, klx::kNumPresets - 1, index)));
        p->endChangeGesture();
    }
}

void KlimaxProcessor::setCurrentProgram (int index) { selectPreset (index); }

juce::AudioProcessorEditor* KlimaxProcessor::createEditor() { return new KlimaxEditor (*this); }

void KlimaxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KlimaxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KlimaxProcessor(); }
