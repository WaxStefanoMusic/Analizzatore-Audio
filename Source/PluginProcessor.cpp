#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamID
{
    static constexpr auto inputGain      = "inputGain";
    static constexpr auto limiterEnabled = "limiterOn";
    static constexpr auto ceiling        = "ceilingDb";
    static constexpr auto release        = "releaseMs";
}

// ============================================================================
//  AnalyzerStage
// ============================================================================
void AnalyzerStage::prepare (double sr, int channels, int blockSize, int rmsWindowSamples)
{
    lufs.prepare     (sr, channels);
    truePeak.prepare (sr, channels, blockSize);
    spectrum.prepare (sr);

    rmsWindow = juce::jmax (1, rmsWindowSamples);
    rmsRunning.assign ((size_t) channels, 0.0f);
    rmsDb = std::vector<std::atomic<float>> ((size_t) channels);
    for (auto& v : rmsDb) v.store (-INFINITY);
}

void AnalyzerStage::reset()
{
    lufs.reset();
    truePeak.reset();
    spectrum.reset();
    for (auto& v : rmsRunning) v = 0.0f;
    for (auto& v : rmsDb)      v.store (-INFINITY);
}

void AnalyzerStage::process (const juce::AudioBuffer<float>& buffer)
{
    const int channels = juce::jmin (buffer.getNumChannels(), (int) rmsRunning.size());
    if (channels == 0) return;

    lufs.process     (buffer);
    truePeak.process (buffer);
    spectrum.pushBuffer (buffer);

    const int numSamples = buffer.getNumSamples();
    const float coef = 1.0f - std::exp (-1.0f / (float) rmsWindow);
    for (int ch = 0; ch < channels; ++ch)
    {
        float ms = rmsRunning[(size_t) ch];
        const float* d = buffer.getReadPointer (ch);
        for (int n = 0; n < numSamples; ++n)
            ms += (d[n] * d[n] - ms) * coef;
        rmsRunning[(size_t) ch] = ms;
        rmsDb[(size_t) ch].store (
            juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (0.0f, ms)), -120.0f));
    }
}

float AnalyzerStage::getRmsDb (int ch) const noexcept
{
    if (ch < 0 || ch >= (int) rmsDb.size()) return -INFINITY;
    return rmsDb[(size_t) ch].load();
}

// ============================================================================
//  Parameters
// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AnalizzatoreAudioProcessor::makeParameters()
{
    using P    = juce::AudioParameterFloat;
    using B    = juce::AudioParameterBool;
    using R    = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<P> (
        juce::ParameterID { ParamID::inputGain, 1 },
        "Input Gain",
        R (-24.0f, 24.0f, 0.01f),
        0.0f,
        Attr().withLabel ("dB")));

    layout.add (std::make_unique<B> (
        juce::ParameterID { ParamID::limiterEnabled, 1 },
        "Limiter",
        false));

    layout.add (std::make_unique<P> (
        juce::ParameterID { ParamID::ceiling, 1 },
        "Ceiling",
        R (-6.0f, 0.0f, 0.01f),
        -1.0f,
        Attr().withLabel ("dBTP")));

    layout.add (std::make_unique<P> (
        juce::ParameterID { ParamID::release, 1 },
        "Release",
        R (5.0f, 500.0f, 0.1f, 0.4f),
        80.0f,
        Attr().withLabel ("ms")));

    return layout;
}

// ============================================================================
//  Processor
// ============================================================================
AnalizzatoreAudioProcessor::AnalizzatoreAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", makeParameters())
{
    paramInputGain      = apvts.getRawParameterValue (ParamID::inputGain);
    paramLimiterEnabled = apvts.getRawParameterValue (ParamID::limiterEnabled);
    paramCeiling        = apvts.getRawParameterValue (ParamID::ceiling);
    paramRelease        = apvts.getRawParameterValue (ParamID::release);
}

bool AnalizzatoreAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainIn.isDisabled() || mainOut.isDisabled()) return false;
    if (mainIn != mainOut) return false;
    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void AnalizzatoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = getTotalNumOutputChannels();
    const int rmsWin   = juce::jmax (1, (int) std::round (sampleRate * 0.3));

    inputStage.prepare  (sampleRate, channels, samplesPerBlock, rmsWin);
    outputStage.prepare (sampleRate, channels, samplesPerBlock, rmsWin);
    limiter.prepare     (sampleRate, channels, samplesPerBlock);

    setLatencySamples (limiter.getLatencySamples());
}

void AnalizzatoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // 1) Input gain
    const float inGainDb = paramInputGain != nullptr ? paramInputGain->load() : 0.0f;
    if (std::abs (inGainDb) > 0.0001f)
        buffer.applyGain (juce::Decibels::decibelsToGain (inGainDb));

    // 2) Limiter parameter sync
    limiter.setEnabled  (paramLimiterEnabled != nullptr && paramLimiterEnabled->load() > 0.5f);
    limiter.setCeilingDb (paramCeiling != nullptr ? paramCeiling->load() : -1.0f);
    limiter.setReleaseMs (paramRelease != nullptr ? paramRelease->load() : 80.0f);

    // 3) INPUT analysis (after input gain, before limiter)
    inputStage.process (buffer);

    // 4) Limiter (modifies buffer in place)
    limiter.process (buffer);

    // 5) OUTPUT analysis (post-limiter)
    outputStage.process (buffer);

    // 6) Keep latency reporting in sync with current limiter state
    const int reported = getLatencySamples();
    const int actual   = limiter.getLatencySamples();
    if (reported != actual)
        setLatencySamples (actual);
}

void AnalizzatoreAudioProcessor::resetMeasurements()
{
    inputStage.reset();
    outputStage.reset();
}

void AnalizzatoreAudioProcessor::captureSnapshot (bool intoA)
{
    Snapshot& s = intoA ? snapshotA : snapshotB;
    s.valid = true;
    s.label = intoA ? "A" : "B";
    s.lufsMomentary  = inputStage.lufs.getMomentary();
    s.lufsShortTerm  = inputStage.lufs.getShortTerm();
    s.lufsIntegrated = inputStage.lufs.getIntegrated();
    s.loudnessRange  = inputStage.lufs.getLoudnessRange();
    s.peakDbL        = inputStage.truePeak.getPeakDb (0);
    s.peakDbR        = inputStage.truePeak.getPeakDb (1);
    s.truePeakMaxDb  = inputStage.truePeak.getMaxTruePeakDb();
    s.rmsDbL         = inputStage.getRmsDb (0);
    s.rmsDbR         = inputStage.getRmsDb (1);
    s.sampleRate     = inputStage.spectrum.copySpectrum (s.spectrumDb);
}

void AnalizzatoreAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state = apvts.copyState();
    state.setProperty ("editorWidth",  savedEditorWidth,  nullptr);
    state.setProperty ("editorHeight", savedEditorHeight, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnalizzatoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            savedEditorWidth  = (int) tree.getProperty ("editorWidth",  0);
            savedEditorHeight = (int) tree.getProperty ("editorHeight", 0);
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* AnalizzatoreAudioProcessor::createEditor()
{
    return new AnalizzatoreAudioEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnalizzatoreAudioProcessor();
}
