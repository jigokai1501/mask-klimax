#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Le cœur DSP (Opus) compare volontairement des flottants à des valeurs exactes (état neutre) :
// on fait taire cet avertissement ici, sans modifier le moteur.
#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wfloat-equal"
 #pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#include "Dsp/Engine.h"
#if defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#endif

class KlimaxProcessor : public juce::AudioProcessor
{
public:
    KlimaxProcessor();
    ~KlimaxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "mask::klimax"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    // Presets exposés aussi comme « programmes » (menu de presets de Logic)
    int getNumPrograms() override { return klx::kNumPresets; }
    int getCurrentProgram() override { return getPresetIndex(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return klx::presetName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int getPresetIndex() const noexcept { return juce::jlimit (0, klx::kNumPresets - 1, (int) std::lround (presetParam->load())); }
    void selectPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    klx::Engine engine;
    std::atomic<float>* knobParam = nullptr;
    std::atomic<float>* presetParam = nullptr;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KlimaxProcessor)
};
