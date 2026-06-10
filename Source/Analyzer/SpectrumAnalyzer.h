#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>

/**
    Real-time FFT spectrum analyzer.
    Sums all input channels to mono, applies a Hann window, runs a 4096-point FFT
    and exposes magnitude bins (in dB). Audio thread is lock-free; the GUI reads
    a snapshot via copySpectrum().
*/
class SpectrumAnalyzer
{
public:
    static constexpr int kFftOrder   = 12;            // 4096
    static constexpr int kFftSize    = 1 << kFftOrder;
    static constexpr int kNumBins    = kFftSize / 2;

    SpectrumAnalyzer();

    void prepare (double sampleRate);
    void reset();

    // Audio thread.
    void pushBuffer (const juce::AudioBuffer<float>& buffer);

    // GUI thread: copy current bins (in dBFS, smoothed). Returns sample rate.
    double copySpectrum (std::array<float, kNumBins>& dest) const noexcept;

    // GUI thread: capture an immediate snapshot of the spectrum (for A/B compare).
    void   snapshot (std::array<float, kNumBins>& dest) const noexcept { copySpectrum (dest); }

    void setSmoothing (float coefPerFrame) noexcept { smoothing = juce::jlimit (0.0f, 0.99f, coefPerFrame); }

private:
    double sampleRate = 48000.0;

    juce::dsp::FFT  fft       { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> fifo;            // input ring (size kFftSize)
    int                fifoIndex = 0;
    bool               fifoReady = false;

    std::vector<float> fftData;         // 2*kFftSize for complex transform

    // Lock-free double-buffered output bins (dB).
    mutable std::array<std::atomic<float>, kNumBins> binsDb;
    float smoothing = 0.7f;             // higher = slower
};
