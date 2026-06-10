#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <memory>

/**
    True-peak detection via 4x oversampling (ITU-R BS.1770 Annex 2 spec).
    Holds peak with optional decay so a meter can read a slowly-falling value.
*/
class TruePeakDetector
{
public:
    TruePeakDetector() = default;

    void prepare (double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    void process (const juce::AudioBuffer<float>& buffer);

    // Returns linear peak per channel (with hold/decay) and dBTP convenience.
    float getPeakLinear (int ch) const noexcept
    {
        if (ch < 0 || ch >= (int) heldPeaks.size()) return 0.0f;
        return heldPeaks[(size_t) ch].load();
    }
    float getPeakDb (int ch) const noexcept
    {
        return juce::Decibels::gainToDecibels (getPeakLinear (ch), -120.0f);
    }

    // Maximum across all channels since last reset (does NOT decay).
    float getMaxTruePeakDb() const noexcept
    {
        return juce::Decibels::gainToDecibels (maxAllTime.load(), -120.0f);
    }

    void setDecayDbPerSecond (float db) noexcept { decayDbPerSec = db; }

private:
    static constexpr int kOversamplingFactor = 2; // power of 2; 2^2 = 4x
    double sampleRate = 48000.0;
    int    numChannels = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<std::atomic<float>> heldPeaks;
    std::atomic<float> maxAllTime { 0.0f };

    float decayDbPerSec = 12.0f;   // meter ballistics (decay only; rise is instantaneous)
};
