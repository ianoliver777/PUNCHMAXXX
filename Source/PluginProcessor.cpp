#include "PluginProcessor.h"
#include "PluginEditor.h"

PunchmaxxxProcessor::PunchmaxxxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PUNCHMAXXX", pmx::createLayout()),
      presets (apvts)
{
    using namespace pmx;
    pTransient = apvts.getRawParameterValue (ID::transient);
    pPunch     = apvts.getRawParameterValue (ID::punch);
    pBody      = apvts.getRawParameterValue (ID::body);
    pMode      = apvts.getRawParameterValue (ID::mode);
    pShape     = apvts.getRawParameterValue (ID::shape);
    pFocus     = apvts.getRawParameterValue (ID::focus);
    pDrive     = apvts.getRawParameterValue (ID::drive);
    pSat       = apvts.getRawParameterValue (ID::saturation);
    pLow       = apvts.getRawParameterValue (ID::toneLow);
    pHigh      = apvts.getRawParameterValue (ID::toneHigh);
    pWidth     = apvts.getRawParameterValue (ID::width);
    pMix       = apvts.getRawParameterValue (ID::mix);
    pOutput    = apvts.getRawParameterValue (ID::output);
}

bool PunchmaxxxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

pmx::PunchEngine::Settings PunchmaxxxProcessor::readSettings() const
{
    pmx::PunchEngine::Settings s;
    s.transient  = pTransient->load();
    s.punch      = pPunch->load();
    s.body       = pBody->load();
    s.mode       = (pmx::Mode) juce::jlimit (0, 3, (int) pMode->load());
    s.shape      = (pmx::Shape) juce::jlimit (0, 3, (int) pShape->load());
    s.focusHz    = pFocus->load();
    s.drive      = pDrive->load();
    s.saturation = pSat->load();
    s.lowDb      = pLow->load();
    s.highDb     = pHigh->load();
    s.width      = pWidth->load();
    return s;
}

void PunchmaxxxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int chans = juce::jmax (1, getTotalNumOutputChannels());
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock, chans);

    const int latency = engine.getLatencySamples();
    setLatencySamples (latency);

    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) chans });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency ((float) latency);
    dryWet.setWetMixProportion (pMix->load() * 0.01f);
    dryWet.reset();

    outGain.reset (sampleRate, 0.03);
    const float db = pOutput->load();
    outGain.setCurrentAndTargetValue (db <= pmx::kOutputFloorDb ? 0.0f : juce::Decibels::decibelsToGain (db));

    inputCopy.setSize (1, samplesPerBlock);
    // ~1.6 s of history across the display at any sample rate
    displayDecim = juce::jmax (16, juce::roundToInt (sampleRate * 1.6 / 600.0));
    decimCount = 0;
    pending = {};
}

void PunchmaxxxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0 || chans == 0)
        return;

    // keep a mono copy of the input for the display
    if (inputCopy.getNumSamples() < n)
        inputCopy.setSize (1, n, false, false, true);
    auto* in = inputCopy.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        in[i] = chans > 1 ? 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)) : buffer.getSample (0, i);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
    dryWet.setWetMixProportion (pMix->load() * 0.01f);
    dryWet.pushDrySamples (block);

    engine.setSettings (readSettings());
    engine.process (buffer);

    dryWet.mixWetSamples (block);

    const float db = pOutput->load();
    outGain.setTargetValue (db <= pmx::kOutputFloorDb ? 0.0f : juce::Decibels::decibelsToGain (db));
    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < chans; ++c)
            buffer.getWritePointer (c)[i] *= g;
        peakL = juce::jmax (peakL, std::abs (buffer.getSample (0, i)));
        peakR = juce::jmax (peakR, std::abs (buffer.getSample (chans > 1 ? 1 : 0, i)));
    }

    // meters: hold the max until the UI reads (and resets) it
    auto bump = [] (std::atomic<float>& a, float v) { float cur = a.load(); while (v > cur && ! a.compare_exchange_weak (cur, v)) {} };
    bump (meterL, peakL);
    bump (meterR, peakR);

    pushDisplay (in, buffer, n);
}

void PunchmaxxxProcessor::pushDisplay (const float* inMono, const juce::AudioBuffer<float>& out, int n)
{
    const float* gain = engine.getGainTrace();
    const int chans = juce::jmin (2, out.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        pending.inPeak = juce::jmax (pending.inPeak, std::abs (inMono[i]));
        float o = std::abs (out.getSample (0, i));
        if (chans > 1) o = juce::jmax (o, std::abs (out.getSample (1, i)));
        pending.outPeak = juce::jmax (pending.outPeak, o);
        if (std::abs (gain[i]) > std::abs (pending.gainDb))
            pending.gainDb = gain[i];

        if (++decimCount >= displayDecim)
        {
            decimCount = 0;
            const auto scope = displayFifo.write (1);
            if (scope.blockSize1 > 0) displayFrames[(size_t) scope.startIndex1] = pending;
            else if (scope.blockSize2 > 0) displayFrames[(size_t) scope.startIndex2] = pending;
            pending = {};
        }
    }
}

void PunchmaxxxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presets.writeToState (state);
    state.setProperty ("editorWidth", savedEditorWidth.load(), nullptr);
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PunchmaxxxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    {
        pmx::PresetManager::ScopedLoad sl (presets);
        apvts.replaceState (tree);
    }
    presets.readFromState (tree);
    savedEditorWidth = (int) tree.getProperty ("editorWidth", 0);
}

juce::AudioProcessorEditor* PunchmaxxxProcessor::createEditor()
{
    return new PunchmaxxxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PunchmaxxxProcessor();
}
