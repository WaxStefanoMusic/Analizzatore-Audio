#include "TruePeakDetector.h"

void TruePeakDetector::prepare (double sr, int channels, int maxBlock)
{
    sampleRate  = sr;
    numChannels = juce::jmax (1, channels);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels,
        kOversamplingFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampling->initProcessing ((size_t) maxBlock);

    heldPeaks = std::vector<std::atomic<float>> ((size_t) numChannels);
    reset();
}

void TruePeakDetector::reset()
{
    if (oversampling) oversampling->reset();
    for (auto& p : heldPeaks) p.store (0.0f);
    maxAllTime.store (0.0f);
}

void TruePeakDetector::process (const juce::AudioBuffer<float>& buffer)
{
    if (! oversampling) return;
    const int channels   = juce::jmin (buffer.getNumChannels(), numChannels);
    const int numSamples = buffer.getNumSamples();
    if (channels == 0 || numSamples == 0) return;

    // Read-only upsample for analysis. We deliberately do NOT call
    // processSamplesDown: it would overwrite the caller's buffer and corrupt
    // the signal for the analyzers and limiter that run after us.
    juce::dsp::AudioBlock<const float> constBlock (
        buffer.getArrayOfReadPointers(), (size_t) channels, (size_t) numSamples);
    auto upBlock = oversampling->processSamplesUp (constBlock);

    const int upSamples = (int) upBlock.getNumSamples();
    const float decayPerBlockDb = decayDbPerSec * (float) numSamples / (float) sampleRate;

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* data = upBlock.getChannelPointer ((size_t) ch);
        float blockMax = 0.0f;
        for (int n = 0; n < upSamples; ++n)
        {
            const float a = std::abs (data[n]);
            if (a > blockMax) blockMax = a;
        }

        // Apply decay then update with this block's max
        const float current = heldPeaks[(size_t) ch].load();
        const float decayed = juce::Decibels::decibelsToGain (
            juce::Decibels::gainToDecibels (current, -120.0f) - decayPerBlockDb);
        const float next = juce::jmax (decayed, blockMax);
        heldPeaks[(size_t) ch].store (next);

        float allTime = maxAllTime.load();
        if (blockMax > allTime) maxAllTime.store (blockMax);
    }
}
