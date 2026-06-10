#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <deque>

/**
    BS.1770-4 / EBU R128 loudness meter.
    Computes Momentary (400 ms), Short-term (3 s), Integrated (gated)
    and Loudness Range (LRA).
*/
class LufsMeter
{
public:
    LufsMeter() = default;

    void prepare (double sampleRate, int numChannels);
    void reset();

    // Process a stereo (or multichannel) interleaved-by-channel JUCE buffer.
    void process (const juce::AudioBuffer<float>& buffer);

    // All values in LUFS. Returns -INFINITY when not yet measurable.
    float getMomentary()  const noexcept { return momentaryLufs.load(); }
    float getShortTerm()  const noexcept { return shortTermLufs.load(); }
    float getIntegrated() const noexcept { return integratedLufs.load(); }
    float getLoudnessRange() const noexcept { return loudnessRange.load(); }

private:
    static constexpr float kAbsoluteGate = -70.0f;   // LUFS
    static constexpr float kRelativeGate = -10.0f;   // LU below ungated mean

    double sampleRate = 48000.0;
    int    numChannels = 2;

    // K-weighting: pre-filter (high-shelf) + RLB (high-pass) per channel
    std::vector<juce::dsp::IIR::Filter<float>> preFilters;
    std::vector<juce::dsp::IIR::Filter<float>> rlbFilters;

    // Accumulator for one 100 ms hop (sum of K-weighted squared samples per channel)
    std::vector<double> hopSumSq;
    int hopSamples       = 0;     // samples per 100 ms
    int hopSamplesFilled = 0;

    // History of per-channel mean-square per hop (for sliding momentary / short-term)
    std::deque<std::vector<double>> hopHistory;     // size in hops; each entry: meanSq per ch
    static constexpr int kMomentaryHops = 4;        // 400 ms
    static constexpr int kShortTermHops = 30;       // 3 s
    static constexpr int kLraHops       = 30;       // 3 s short-term blocks for LRA

    // Integrated: per-block (400 ms) loudness values, kept since last reset
    std::vector<float> blockLoudnesses;             // for integrated LUFS gating
    std::vector<float> stHistoryForLra;             // short-term blocks (every 1 s) for LRA

    int hopsSinceLastBlock = 0;     // for integrated 400 ms blocks (every 1 hop = 100 ms hop)
    int hopsSinceLastStBlock = 0;   // for LRA blocks (every 10 hops = 1 s)

    std::atomic<float> momentaryLufs   { -INFINITY };
    std::atomic<float> shortTermLufs   { -INFINITY };
    std::atomic<float> integratedLufs  { -INFINITY };
    std::atomic<float> loudnessRange   { 0.0f };

    void designKWeighting();
    static float channelWeight (int chIndex, int totalChannels) noexcept;
    float computeLoudnessFromHops (int numHops) const;
    void recomputeIntegrated();
    void recomputeLra();
};
