#pragma once
#include <JuceHeader.h>
#include "DSP.h"
#include "Params.h"
#include "PresetManager.h"

class PunchmaxxxAudioProcessor : public juce::AudioProcessor
{
public:
    PunchmaxxxAudioProcessor();
    ~PunchmaxxxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // the preset browser handles programs; expose a single program to hosts
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.getCurrentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };

    // ---- metering (audio -> UI) ----
    struct GraphPoint { float inDb, outDb, grDb; };
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, grLevelDb { 0.0f };
    juce::AbstractFifo graphFifo { 2048 };
    std::array<GraphPoint, 2048> graphData {};

private:
    struct Chain
    {
        pmx::Compressor comp;
        pmx::TransientShaper trans;
        juce::SmoothedValue<float> compAmt, transAmt, mixAmt, tAtk, tSus;
        float lastGr = 0.0f;
        int factor = 1;
        void prepare (double fs);
        void reset();
    };

    void pushGraphPoint (float in, float out, float gr);

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers; // [0] unused (off)
    std::array<Chain, 3> chains;
    std::array<int, 3> osLatency { 0, 0, 0 };
    std::array<pmx::AlignDelay, 3> osFracDelay;   // integer delay *inside* the oversampled domain = exact fractional base-rate compensation (stays linear-phase)
    pmx::AlignDelay alignDelay;
    pmx::Limiter limiter;                       // runs at base rate, true-peak aware
    juce::SmoothedValue<float> outGain, maxAmt, driveGain, ceilLin;
    int currentOs = -1, maxOsLatency = 0, lookaheadBase = 64;
    int graphInterval = 441, graphCounter = 0;
    float graphIn = 0, graphOut = 0, graphGr = 0;

    std::atomic<float>* pCompOn, *pThresh, *pRatio, *pCAtk, *pCRel, *pTransOn, *pTAtk, *pTSus,
                      *pMaxOn, *pCeil, *pDrive, *pMix, *pOs, *pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchmaxxxAudioProcessor)
};
