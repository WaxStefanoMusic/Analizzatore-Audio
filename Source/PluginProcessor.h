#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Analyzer/LufsMeter.h"
#include "Analyzer/TruePeakDetector.h"
#include "Analyzer/SpectrumAnalyzer.h"
#include "Analyzer/Snapshot.h"
#include "DSP/TruePeakLimiter.h"

/**
    Bundle of all the meters/analyzers tapping a single point in the signal
    chain. We instantiate two of these — one BEFORE the limiter (INPUT)
    and one AFTER the limiter (OUTPUT) — so the GUI can show the comparison.
*/
struct AnalyzerStage
{
    LufsMeter        lufs;
    TruePeakDetector truePeak;
    SpectrumAnalyzer spectrum;

    std::vector<std::atomic<float>> rmsDb;       // size = numChannels
    std::vector<float>              rmsRunning;  // squared running mean per ch
    int                             rmsWindow = 0;

    void prepare (double sr, int channels, int blockSize, int rmsWindowSamples);
    void reset();
    void process (const juce::AudioBuffer<float>& buffer);

    float getRmsDb (int ch) const noexcept;
};

class AnalizzatoreAudioProcessor : public juce::AudioProcessor
{
public:
    AnalizzatoreAudioProcessor();
    ~AnalizzatoreAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----- Public access for the editor -----
    juce::AudioProcessorValueTreeState apvts;

    AnalyzerStage&    getInputStage()  noexcept { return inputStage;  }
    AnalyzerStage&    getOutputStage() noexcept { return outputStage; }
    TruePeakLimiter&  getLimiter()     noexcept { return limiter;     }

    void resetMeasurements();

    // A/B snapshots — captured from the INPUT stage (the source signal).
    Snapshot& snapA() noexcept { return snapshotA; }
    Snapshot& snapB() noexcept { return snapshotB; }
    void captureSnapshot (bool intoA);
    void clearABSnapshots() noexcept
    {
        snapshotA = Snapshot {};
        snapshotB = Snapshot {};
    }

    // Persistent editor size
    int savedEditorWidth  = 0;
    int savedEditorHeight = 0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();

    AnalyzerStage   inputStage;
    AnalyzerStage   outputStage;
    TruePeakLimiter limiter;

    std::atomic<float>* paramLimiterEnabled = nullptr;
    std::atomic<float>* paramCeiling        = nullptr;
    std::atomic<float>* paramRelease        = nullptr;
    std::atomic<float>* paramInputGain      = nullptr;

    Snapshot snapshotA, snapshotB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalizzatoreAudioProcessor)
};
