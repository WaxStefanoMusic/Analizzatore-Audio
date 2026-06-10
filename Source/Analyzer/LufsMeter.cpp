#include "LufsMeter.h"
#include <algorithm>
#include <cmath>
#include <numeric>

void LufsMeter::prepare (double sr, int channels)
{
    sampleRate  = sr;
    numChannels = juce::jmax (1, channels);

    preFilters.clear();
    rlbFilters.clear();
    preFilters.resize ((size_t) numChannels);
    rlbFilters.resize ((size_t) numChannels);

    designKWeighting();

    juce::dsp::ProcessSpec spec { sampleRate, 512u, 1u };
    for (auto& f : preFilters) f.prepare (spec);
    for (auto& f : rlbFilters) f.prepare (spec);

    hopSamples       = (int) std::round (sampleRate * 0.1); // 100 ms
    hopSamplesFilled = 0;
    hopSumSq.assign ((size_t) numChannels, 0.0);

    reset();
}

void LufsMeter::reset()
{
    for (auto& f : preFilters) f.reset();
    for (auto& f : rlbFilters) f.reset();

    std::fill (hopSumSq.begin(), hopSumSq.end(), 0.0);
    hopSamplesFilled = 0;

    hopHistory.clear();
    blockLoudnesses.clear();
    stHistoryForLra.clear();
    hopsSinceLastBlock   = 0;
    hopsSinceLastStBlock = 0;

    momentaryLufs.store  (-INFINITY);
    shortTermLufs.store  (-INFINITY);
    integratedLufs.store (-INFINITY);
    loudnessRange.store  (0.0f);
}

void LufsMeter::designKWeighting()
{
    // BS.1770-4 K-weighting: high-shelf at ~1681 Hz then high-pass at ~38 Hz.
    // Coefficients via JUCE's RBJ-style design (matches reference within rounding).
    const float fsShelf = 1681.974450955533f;
    const float qShelf  = 0.7071752369554196f;
    const float gainDbShelf = 3.999843853973347f;
    const float gainShelf   = juce::Decibels::decibelsToGain (gainDbShelf);

    const float fsHp = 38.13547087602444f;
    const float qHp  = 0.5003270373238773f;

    auto preCoef = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, fsShelf, qShelf, gainShelf);
    auto rlbCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, fsHp, qHp);

    for (auto& f : preFilters) f.coefficients = preCoef;
    for (auto& f : rlbFilters) f.coefficients = rlbCoef;
}

float LufsMeter::channelWeight (int ch, int total) noexcept
{
    // ITU-R BS.1770-4: L/R/C = 1.0, LFE = 0, surrounds (Ls/Rs) = 1.41.
    // For mono/stereo just 1.0 per channel.
    if (total <= 2) return 1.0f;
    if (total >= 5)
    {
        // 5.1 layout: L R C LFE Ls Rs
        switch (ch)
        {
            case 0: case 1: case 2: return 1.0f;
            case 3:                 return 0.0f;     // LFE
            case 4: case 5:         return 1.41f;
            default:                return 1.0f;
        }
    }
    return 1.0f;
}

void LufsMeter::process (const juce::AudioBuffer<float>& buffer)
{
    const int channels = juce::jmin (buffer.getNumChannels(), numChannels);
    const int numSamples = buffer.getNumSamples();
    if (channels == 0 || numSamples == 0) return;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float x = buffer.getSample (ch, n);
            x = preFilters[(size_t) ch].processSample (x);
            x = rlbFilters[(size_t) ch].processSample (x);
            hopSumSq[(size_t) ch] += (double) x * (double) x;
        }
        ++hopSamplesFilled;

        if (hopSamplesFilled >= hopSamples)
        {
            // Convert sum of squares to mean-square per channel for this 100 ms hop
            std::vector<double> meanSq ((size_t) channels);
            for (int ch = 0; ch < channels; ++ch)
                meanSq[(size_t) ch] = hopSumSq[(size_t) ch] / (double) hopSamples;

            hopHistory.push_back (std::move (meanSq));
            // Keep enough history for short-term (30 hops) — older hops not needed for sliding meters
            while ((int) hopHistory.size() > kShortTermHops)
                hopHistory.pop_front();

            std::fill (hopSumSq.begin(), hopSumSq.end(), 0.0);
            hopSamplesFilled = 0;

            // Sliding momentary (last 4 hops) and short-term (last 30 hops)
            momentaryLufs.store (computeLoudnessFromHops (kMomentaryHops));
            shortTermLufs.store (computeLoudnessFromHops (kShortTermHops));

            // Integrated 400 ms block fires every hop once we have at least 4 hops
            if ((int) hopHistory.size() >= kMomentaryHops)
            {
                float blockL = computeLoudnessFromHops (kMomentaryHops);
                if (std::isfinite (blockL))
                    blockLoudnesses.push_back (blockL);
                recomputeIntegrated();
            }

            // LRA short-term block (3 s) every 1 s = 10 hops
            ++hopsSinceLastStBlock;
            if (hopsSinceLastStBlock >= 10 && (int) hopHistory.size() >= kShortTermHops)
            {
                hopsSinceLastStBlock = 0;
                float st = computeLoudnessFromHops (kShortTermHops);
                if (std::isfinite (st))
                    stHistoryForLra.push_back (st);
                recomputeLra();
            }
        }
    }
}

float LufsMeter::computeLoudnessFromHops (int numHops) const
{
    const int available = (int) hopHistory.size();
    if (available < numHops) return -INFINITY;

    const int chs = (int) hopHistory.front().size();
    std::vector<double> sumSqPerCh ((size_t) chs, 0.0);

    for (int i = available - numHops; i < available; ++i)
        for (int ch = 0; ch < chs; ++ch)
            sumSqPerCh[(size_t) ch] += hopHistory[(size_t) i][(size_t) ch];

    double total = 0.0;
    for (int ch = 0; ch < chs; ++ch)
    {
        const double meanSq = sumSqPerCh[(size_t) ch] / (double) numHops;
        total += channelWeight (ch, chs) * meanSq;
    }

    if (total <= 0.0) return -INFINITY;
    return (float) (-0.691 + 10.0 * std::log10 (total));
}

void LufsMeter::recomputeIntegrated()
{
    if (blockLoudnesses.empty()) { integratedLufs.store (-INFINITY); return; }

    // Absolute gate at -70 LUFS
    std::vector<float> absGated;
    absGated.reserve (blockLoudnesses.size());
    for (float l : blockLoudnesses)
        if (l >= kAbsoluteGate) absGated.push_back (l);

    if (absGated.empty()) { integratedLufs.store (-INFINITY); return; }

    // Mean of absolute-gated, in mean-square domain
    auto meanLufs = [] (const std::vector<float>& v) -> float
    {
        double s = 0.0;
        for (float l : v) s += std::pow (10.0, ((double) l + 0.691) / 10.0);
        return (float) (-0.691 + 10.0 * std::log10 (s / (double) v.size()));
    };

    const float ungated = meanLufs (absGated);
    const float relGate = ungated + kRelativeGate;

    std::vector<float> bothGated;
    bothGated.reserve (absGated.size());
    for (float l : absGated)
        if (l >= relGate) bothGated.push_back (l);

    if (bothGated.empty()) { integratedLufs.store (ungated); return; }
    integratedLufs.store (meanLufs (bothGated));
}

void LufsMeter::recomputeLra()
{
    // EBU Tech 3342: LRA = P95 - P10 over short-term blocks gated at -20 LU below ungated mean,
    // with absolute gate -70 LUFS.
    if (stHistoryForLra.size() < 2) { loudnessRange.store (0.0f); return; }

    std::vector<float> absGated;
    for (float l : stHistoryForLra)
        if (l >= kAbsoluteGate) absGated.push_back (l);
    if (absGated.empty()) { loudnessRange.store (0.0f); return; }

    double sum = 0.0;
    for (float l : absGated)
        sum += std::pow (10.0, ((double) l + 0.691) / 10.0);
    const float ungated = (float) (-0.691 + 10.0 * std::log10 (sum / (double) absGated.size()));

    const float gate = ungated - 20.0f;
    std::vector<float> bothGated;
    for (float l : absGated)
        if (l >= gate) bothGated.push_back (l);
    if (bothGated.size() < 2) { loudnessRange.store (0.0f); return; }

    std::sort (bothGated.begin(), bothGated.end());
    auto pct = [&] (float p) -> float
    {
        const float idx = p * (float) (bothGated.size() - 1);
        const int lo = (int) std::floor (idx);
        const int hi = std::min ((int) bothGated.size() - 1, lo + 1);
        const float t = idx - (float) lo;
        return bothGated[(size_t) lo] * (1.0f - t) + bothGated[(size_t) hi] * t;
    };
    loudnessRange.store (pct (0.95f) - pct (0.10f));
}
