#pragma once

#include "SpectrumAnalyzer.h"
#include <array>

/**
    Frozen snapshot of the analyzer state — used for A/B compare.
    Filled by the GUI thread by reading current values from the meters.
*/
struct Snapshot
{
    bool  valid = false;

    // Loudness
    float lufsMomentary  = -INFINITY;
    float lufsShortTerm  = -INFINITY;
    float lufsIntegrated = -INFINITY;
    float loudnessRange  = 0.0f;

    // Levels
    float peakDbL        = -INFINITY;
    float peakDbR        = -INFINITY;
    float truePeakMaxDb  = -INFINITY;   // overall max true-peak
    float rmsDbL         = -INFINITY;
    float rmsDbR         = -INFINITY;

    // Spectrum (dBFS per bin)
    std::array<float, SpectrumAnalyzer::kNumBins> spectrumDb {};
    double sampleRate    = 48000.0;

    // Tag
    juce::String label;
};
