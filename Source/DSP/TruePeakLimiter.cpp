#include "TruePeakLimiter.h"

void TruePeakLimiter::prepare (double sr, int channels, int maxBlock)
{
    sampleRate  = sr;
    numChannels = juce::jmax (1, channels);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels,
        kOsLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampling->initProcessing ((size_t) maxBlock);

    // 1.5 ms of lookahead (in oversampled domain)
    lookaheadSamplesUp = juce::jmax (16, (int) std::ceil (sampleRate * (double) kOsFactor * 0.0015));

    delayLines.assign ((size_t) numChannels,
                       std::vector<float> ((size_t) lookaheadSamplesUp, 0.0f));
    absHistory.assign ((size_t) lookaheadSamplesUp, 0.0f);
    writeIdx  = 0;
    envelope  = 1.0f;
    recomputeReleaseCoef();

    gainReductionDb.store (0.0f);
}

void TruePeakLimiter::reset()
{
    if (oversampling) oversampling->reset();
    for (auto& dl : delayLines) std::fill (dl.begin(), dl.end(), 0.0f);
    std::fill (absHistory.begin(), absHistory.end(), 0.0f);
    writeIdx = 0;
    envelope = 1.0f;
    gainReductionDb.store (0.0f);
}

void TruePeakLimiter::setCeilingDb (float db) noexcept
{
    ceilingDb.store (db);
    ceilingLin.store (juce::Decibels::decibelsToGain (db));
}

void TruePeakLimiter::setReleaseMs (float ms) noexcept
{
    releaseMs.store (juce::jmax (1.0f, ms));
    recomputeReleaseCoef();
}

void TruePeakLimiter::recomputeReleaseCoef()
{
    // One-pole release: time constant tau = release / -ln(0.1) so we reach 90% in 'release' ms.
    const double srUp = sampleRate * (double) kOsFactor;
    const double tauSamples = (double) releaseMs.load() * 0.001 * srUp / 2.302585093; // ln(10)
    releaseCoef = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, tauSamples)));
}

int TruePeakLimiter::getLatencySamples() const noexcept
{
    int osLatency = oversampling ? (int) oversampling->getLatencyInSamples() : 0;
    int lookaheadBase = lookaheadSamplesUp / kOsFactor;
    return osLatency + lookaheadBase;
}

void TruePeakLimiter::process (juce::AudioBuffer<float>& buffer)
{
    if (! oversampling) return;
    const int channels   = juce::jmin (buffer.getNumChannels(), numChannels);
    const int numSamples = buffer.getNumSamples();
    if (channels == 0 || numSamples == 0) return;

    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = oversampling->processSamplesUp (block);
    const int upSamples = (int) upBlock.getNumSamples();

    if (! enabled.load())
    {
        // Pass-through (still oversample/downsample to keep latency stable & state coherent)
        oversampling->processSamplesDown (block);
        gainReductionDb.store (0.0f);
        return;
    }

    const float ceiling = ceilingLin.load();
    const int   lookN = lookaheadSamplesUp;
    float minEnvelope = 1.0f;

    for (int n = 0; n < upSamples; ++n)
    {
        // Find max |x| across channels at this sample
        float maxAbs = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float v = std::abs (upBlock.getSample (ch, n));
            if (v > maxAbs) maxAbs = v;
        }

        // Push current sample to delay lines and absHistory
        for (int ch = 0; ch < channels; ++ch)
            delayLines[(size_t) ch][(size_t) writeIdx] = upBlock.getSample (ch, n);
        absHistory[(size_t) writeIdx] = maxAbs;

        // Required gain across the lookahead window (linear scan; window is short)
        float windowMax = 0.0f;
        for (int i = 0; i < lookN; ++i)
            if (absHistory[(size_t) i] > windowMax)
                windowMax = absHistory[(size_t) i];

        const float requiredGain = (windowMax > ceiling) ? (ceiling / windowMax) : 1.0f;

        // Attack instant, release one-pole toward 1.0 (or toward requiredGain)
        if (requiredGain < envelope)
            envelope = requiredGain;
        else
            envelope = envelope + (requiredGain - envelope) * releaseCoef;

        if (envelope < minEnvelope) minEnvelope = envelope;

        // Read oldest sample from delay (the one we are about to overwrite next)
        const int readIdx = (writeIdx + 1) % lookN;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float out = delayLines[(size_t) ch][(size_t) readIdx] * envelope;
            // Final hard ceiling guard for safety (ISP could still nudge above due to OS reconstruction)
            const float clamped = juce::jlimit (-ceiling, ceiling, out);
            upBlock.setSample (ch, n, clamped);
        }

        writeIdx = (writeIdx + 1) % lookN;
    }

    oversampling->processSamplesDown (block);
    gainReductionDb.store (juce::Decibels::gainToDecibels (minEnvelope, -60.0f));
}
