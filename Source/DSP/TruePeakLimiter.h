#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>

/**
    Look-ahead, true-peak limiter (4x oversampled).
    Hard ceiling in dBTP, configurable release.
    Reports current gain reduction (dB) and reports its sample latency
    so the host can compensate.
*/
class TruePeakLimiter
{
public:
    TruePeakLimiter() = default;

    void prepare (double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    void setCeilingDb (float db) noexcept;
    void setReleaseMs (float ms) noexcept;
    void setEnabled   (bool e)   noexcept { enabled.store (e); }

    bool  isEnabled() const noexcept    { return enabled.load(); }
    float getCeilingDb() const noexcept { return ceilingDb.load(); }
    float getReleaseMs() const noexcept { return releaseMs.load(); }

    void process (juce::AudioBuffer<float>& buffer);

    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    int   getLatencySamples() const noexcept; // total latency at host sample rate

private:
    static constexpr int kOsLog2 = 2;          // 4x oversampling
    static constexpr int kOsFactor = 1 << kOsLog2;

    double sampleRate = 48000.0;
    int    numChannels = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Lookahead (oversampled-domain ring buffer)
    int lookaheadSamplesUp = 0;
    std::vector<std::vector<float>> delayLines;   // [ch][lookaheadSamplesUp]
    std::vector<float> absHistory;                // [lookaheadSamplesUp] max-abs across channels per sample
    int writeIdx = 0;

    // Envelope state
    float envelope    = 1.0f;       // current applied gain
    float releaseCoef = 0.0f;       // per oversampled sample
    std::atomic<float> ceilingLin   { 0.891251f }; // -1 dB
    std::atomic<float> ceilingDb    { -1.0f };
    std::atomic<float> releaseMs    { 80.0f };
    std::atomic<bool>  enabled      { true };
    std::atomic<float> gainReductionDb { 0.0f };

    void recomputeReleaseCoef();
};
