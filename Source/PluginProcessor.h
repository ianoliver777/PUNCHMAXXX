#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "DSP/PunchEngine.h"

/** One column of the scrolling display: peak in, peak out, applied gain (dB). */
struct DisplayFrame
{
    float inPeak = 0.0f, outPeak = 0.0f, gainDb = 0.0f;
};

class PunchmaxxxProcessor : public juce::AudioProcessor
{
public:
    PunchmaxxxProcessor();
    ~PunchmaxxxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // The host only sees one "program"; real preset handling lives in PresetManager.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.getCurrentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pmx::PresetManager presets;

    // ---- UI feeds (lock-free) ----
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };
    juce::AbstractFifo displayFifo { 2048 };
    std::array<DisplayFrame, 2048> displayFrames;
    int getDisplayDecimation() const { return displayDecim; }

    /** Editor size is part of the saved state so it reopens the way you left it. */
    std::atomic<int> savedEditorWidth { 0 };

private:
    pmx::PunchEngine::Settings readSettings() const;
    void pushDisplay (const float* inMono, const juce::AudioBuffer<float>& out, int n);

    pmx::PunchEngine engine;
    juce::dsp::DryWetMixer<float> dryWet { 4096 };
    juce::SmoothedValue<float> outGain;
    juce::AudioBuffer<float> inputCopy;

    int displayDecim = 128, decimCount = 0;
    DisplayFrame pending;

    std::atomic<float>* pTransient {}, * pPunch {}, * pBody {}, * pMode {}, * pShape {}, * pFocus {}, * pDrive {},
                      * pSat {}, * pLow {}, * pHigh {}, * pWidth {}, * pMix {}, * pOutput {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchmaxxxProcessor)
};
