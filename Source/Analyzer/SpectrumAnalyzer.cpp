#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    fifo.assign (kFftSize, 0.0f);
    fftData.assign ((size_t) (kFftSize * 2), 0.0f);
    for (auto& b : binsDb) b.store (-120.0f);
}

void SpectrumAnalyzer::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void SpectrumAnalyzer::reset()
{
    std::fill (fifo.begin(), fifo.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    fifoIndex = 0;
    fifoReady = false;
    for (auto& b : binsDb) b.store (-120.0f);
}

void SpectrumAnalyzer::pushBuffer (const juce::AudioBuffer<float>& buffer)
{
    const int channels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (channels == 0 || numSamples == 0) return;

    for (int n = 0; n < numSamples; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            mono += buffer.getSample (ch, n);
        if (channels > 0) mono /= (float) channels;

        fifo[(size_t) fifoIndex] = mono;
        ++fifoIndex;

        if (fifoIndex >= kFftSize)
        {
            fifoIndex = 0;

            // Copy fifo into fftData (real part only) and apply window
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int i = 0; i < kFftSize; ++i)
                fftData[(size_t) i] = fifo[(size_t) i];

            window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            // Convert to dB and smooth into atomic bins
            const float scale = 1.0f / (float) kFftSize;
            for (int i = 0; i < kNumBins; ++i)
            {
                const float mag = fftData[(size_t) i] * scale;
                const float db  = juce::Decibels::gainToDecibels (mag, -120.0f);
                const float prev = binsDb[(size_t) i].load();
                const float next = juce::jmax (db, prev * smoothing + db * (1.0f - smoothing));
                // Use peak-hold-with-decay style: rise instantly, fall slowly
                const float decayed = prev - 0.5f; // dB per FFT frame
                binsDb[(size_t) i].store (juce::jmax (db, juce::jmax (decayed, next)));
            }
            fifoReady = true;
        }
    }
}

double SpectrumAnalyzer::copySpectrum (std::array<float, kNumBins>& dest) const noexcept
{
    for (int i = 0; i < kNumBins; ++i)
        dest[(size_t) i] = binsDb[(size_t) i].load();
    return sampleRate;
}
