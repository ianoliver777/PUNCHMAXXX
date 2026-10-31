#include "PluginProcessor.h"
#include "PluginEditor.h"

PunchmaxxxAudioProcessor::PunchmaxxxAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PUNCHMAXXX", pmx::createLayout())
{
    using namespace pmx::ids;
    pCompOn = apvts.getRawParameterValue (compOn);   pThresh = apvts.getRawParameterValue (thresh);
    pRatio  = apvts.getRawParameterValue (ratio);    pCAtk   = apvts.getRawParameterValue (compAttack);
    pCRel   = apvts.getRawParameterValue (compRelease);
    pTransOn= apvts.getRawParameterValue (transOn);  pTAtk   = apvts.getRawParameterValue (transAttack);
    pTSus   = apvts.getRawParameterValue (transSustain);
    pMaxOn  = apvts.getRawParameterValue (maxOn);    pCeil   = apvts.getRawParameterValue (ceiling);
    pDrive  = apvts.getRawParameterValue (drive);    pMix    = apvts.getRawParameterValue (mix);
    pOs     = apvts.getRawParameterValue (oversample); pOut  = apvts.getRawParameterValue (output);
}

bool PunchmaxxxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void PunchmaxxxAudioProcessor::Chain::prepare (double fs)
{
    comp.prepare (fs);
    trans.prepare (fs);
    for (auto* s : { &compAmt, &transAmt, &mixAmt, &tAtk, &tSus })
        s->reset (fs, 0.03);
    reset();
}

void PunchmaxxxAudioProcessor::Chain::reset()
{
    comp.reset(); trans.reset();
}

void PunchmaxxxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    for (int i = 1; i < 3; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numCh, (size_t) i, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
        oversamplers[(size_t) i]->initProcessing ((size_t) samplesPerBlock);
        // JUCE's "integer latency" mode uses a fractional-delay filter (not linear phase).
        // Instead we round UP and add the remainder as a whole-sample delay at the oversampled rate.
        const float raw = oversamplers[(size_t) i]->getLatencyInSamples();
        osLatency[(size_t) i] = (int) std::ceil (raw - 1.0e-4f);
        const int factor = 1 << i;
        osFracDelay[(size_t) i].prepare (numCh, 2 * factor + 2);
        osFracDelay[(size_t) i].setDelay (juce::roundToInt (((float) osLatency[(size_t) i] - raw) * (float) factor));
    }
    osLatency[0] = 0;
    maxOsLatency = juce::jmax (osLatency[1], osLatency[2]);

    lookaheadBase = juce::jmax (8, juce::roundToInt (sampleRate * 0.0015)); // 1.5 ms
    for (int i = 0; i < 3; ++i)
    {
        chains[(size_t) i].factor = 1 << i;
        chains[(size_t) i].prepare (sampleRate * (1 << i));
    }
    limiter.prepare (lookaheadBase, sampleRate, numCh);
    for (auto* sv : { &maxAmt, &driveGain, &ceilLin }) sv->reset (sampleRate, 0.03);
    maxAmt.setCurrentAndTargetValue (*pMaxOn > 0.5f ? 1.0f : 0.0f);
    driveGain.setCurrentAndTargetValue (pmx::dbToGain (*pDrive * 0.18f));
    ceilLin.setCurrentAndTargetValue (pmx::dbToGain (*pCeil));

    alignDelay.prepare (numCh, maxOsLatency + 4);
    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (pmx::dbToGain (pOut->load()));

    graphInterval = juce::jmax (1, juce::roundToInt (sampleRate / 100.0)); // 100 points / sec
    currentOs = -1;

    // constant latency in every oversampling mode -> no phasing, no host re-sync
    setLatencySamples (maxOsLatency + lookaheadBase);
}

//==============================================================================
void PunchmaxxxAudioProcessor::pushGraphPoint (float in, float out, float gr)
{
    int s1, n1, s2, n2;
    graphFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) graphData[(size_t) s1] = { in, out, gr };
    graphFifo.finishedWrite (n1 + n2);
}

void PunchmaxxxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels(), getTotalNumInputChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);
    if (numSamples == 0 || numCh == 0) return;

    // ---- input meter
    float inPk = 0.0f;
    for (int c = 0; c < numCh; ++c) inPk = juce::jmax (inPk, buffer.getMagnitude (c, 0, numSamples));
    inLevel = juce::jmax (inPk, inLevel.load());

    // ---- oversampling mode switch (everything pre-allocated)
    const int os = juce::jlimit (0, 2, (int) pOs->load());
    if (os != currentOs)
    {
        if (os > 0) { oversamplers[(size_t) os]->reset(); osFracDelay[(size_t) os].reset(); }
        alignDelay.reset();
        alignDelay.setDelay (maxOsLatency - osLatency[(size_t) os]);
        auto& nc = chains[(size_t) os];
        nc.reset();
        nc.compAmt.setCurrentAndTargetValue (*pCompOn > 0.5f ? 1.0f : 0.0f);
        nc.transAmt.setCurrentAndTargetValue (*pTransOn > 0.5f ? 1.0f : 0.0f);
        nc.mixAmt.setCurrentAndTargetValue (*pMix * 0.01f);
        nc.tAtk.setCurrentAndTargetValue (*pTAtk * 0.01f);
        nc.tSus.setCurrentAndTargetValue (*pTSus * 0.01f);
        currentOs = os;
    }

    auto& ch = chains[(size_t) os];
    ch.comp.setParams (pThresh->load(), pRatio->load(), pCAtk->load(), pCRel->load());
    ch.compAmt.setTargetValue  (*pCompOn  > 0.5f ? 1.0f : 0.0f);
    ch.transAmt.setTargetValue (*pTransOn > 0.5f ? 1.0f : 0.0f);
    ch.mixAmt.setTargetValue   (*pMix * 0.01f);
    maxAmt.setTargetValue      (*pMaxOn   > 0.5f ? 1.0f : 0.0f);
    driveGain.setTargetValue   (pmx::dbToGain (*pDrive * 0.18f));   // 0..100 % -> 0..+18 dB
    ceilLin.setTargetValue     (pmx::dbToGain (*pCeil));
    ch.tAtk.setTargetValue     (*pTAtk * 0.01f);
    ch.tSus.setTargetValue     (*pTSus * 0.01f);

    alignDelay.process (buffer, numSamples);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
    juce::dsp::AudioBlock<float> work = os > 0 ? oversamplers[(size_t) os]->processSamplesUp (block) : block;

    const int n = (int) work.getNumSamples();
    float* data[2] = { work.getChannelPointer (0), numCh > 1 ? work.getChannelPointer (1) : nullptr };
    if (os > 0) osFracDelay[(size_t) os].process (data, numCh, n);
    float frame[2], dry[2];
    float minGr = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float linked = 0.0f;
        for (int c = 0; c < numCh; ++c) { frame[c] = dry[c] = data[c][i]; linked = juce::jmax (linked, std::abs (frame[c])); }

        // 1) compressor
        const float gC = ch.comp.process (linked, ch.compAmt.getNextValue());
        linked *= gC;
        // 2) transient shaper
        const float gT = ch.trans.process (linked, ch.tAtk.getNextValue(), ch.tSus.getNextValue(), ch.transAmt.getNextValue());
        // 3) parallel mix (dry is sample-aligned: gain-only processing = zero phase)
        const float m = ch.mixAmt.getNextValue();
        const float wetGain = gC * gT;
        for (int c = 0; c < numCh; ++c)
            data[c][i] = dry[c] + (frame[c] * wetGain - dry[c]) * m;
        minGr = juce::jmin (minGr, ch.comp.getGrDb());
    }

    if (os > 0) oversamplers[(size_t) os]->processSamplesDown (block);

    // 4) maximizer at base rate: drive -> true-peak aware lookahead limiter.
    //    Running it after downsampling guarantees the ceiling at the output.
    {
        float* out[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
        float limGr = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float mAmt = maxAmt.getNextValue();
            const float drv = 1.0f + (driveGain.getNextValue() - 1.0f) * mAmt;
            for (int c = 0; c < numCh; ++c) frame[c] = out[c][i] * drv;
            limiter.process (frame, numCh, ceilLin.getNextValue(), mAmt);
            for (int c = 0; c < numCh; ++c) out[c][i] = frame[c];
            limGr = juce::jmin (limGr, pmx::gainToDb (limiter.getLastGain()));
        }
        minGr += limGr;
    }

    // ---- output gain + meters
    outGain.setTargetValue (pmx::dbToGain (pOut->load()));
    if (outGain.isSmoothing())
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outGain.getNextValue();
            for (int c = 0; c < numCh; ++c) buffer.getWritePointer (c)[i] *= g;
        }
    else
        buffer.applyGain (0, numSamples, outGain.getTargetValue());

    if (numCh == 1 && buffer.getNumChannels() > 1 && getTotalNumOutputChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    float outPk = 0.0f;
    for (int c = 0; c < numCh; ++c) outPk = juce::jmax (outPk, buffer.getMagnitude (c, 0, numSamples));
    outLevel  = juce::jmax (outPk, outLevel.load());
    grLevelDb = juce::jmin (minGr, grLevelDb.load());

    graphIn  = juce::jmax (graphIn, inPk);
    graphOut = juce::jmax (graphOut, outPk);
    graphGr  = juce::jmin (graphGr, minGr);
    graphCounter += numSamples;
    bool pushed = false;
    while (graphCounter >= graphInterval)
    {
        graphCounter -= graphInterval;
        pushGraphPoint (pmx::gainToDb (graphIn), pmx::gainToDb (graphOut), graphGr);
        pushed = true;
    }
    if (pushed) { graphIn = graphOut = 0.0f; graphGr = 0.0f; }
}

//==============================================================================
void PunchmaxxxAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    presets.writeToState (state);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PunchmaxxxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presets.readFromState (tree);
        }
}

juce::AudioProcessorEditor* PunchmaxxxAudioProcessor::createEditor()
{
    return new PunchmaxxxAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PunchmaxxxAudioProcessor();
}
