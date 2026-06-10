#include "SpectrumPanel.h"
#include "Theme.h"
#include "../PluginProcessor.h"

SpectrumPanel::SpectrumPanel (AnalizzatoreAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f;
constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;

float SpectrumPanel::freqToX (float hz, float w)
{
    const float minL = std::log10 (kMinHz);
    const float maxL = std::log10 (kMaxHz);
    const float l = std::log10 (juce::jlimit (kMinHz, kMaxHz, hz));
    return (l - minL) / (maxL - minL) * w;
}

float SpectrumPanel::dbToY (float db, float h, float minDb, float maxDb)
{
    const float t = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return h - t * h;
}

void SpectrumPanel::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float s = Scale::forArea (getLocalBounds(), 700, 280);
    g.setColour (Theme::grid);
    const float w = area.getWidth();
    const float h = area.getHeight();
    static const float decades[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    g.setFont (Scale::font (10.0f, s));
    for (float f : decades)
    {
        const float x = area.getX() + freqToX (f, w);
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Theme::textDim);
        const juce::String label = (f >= 1000.0f)
            ? juce::String ((int) (f / 1000.0f)) + "k"
            : juce::String ((int) f);
        g.drawText (label,
                    (int) x - Scale::px (16, s),
                    (int) area.getBottom() - Scale::px (14, s),
                    Scale::px (32, s), Scale::px (12, s),
                    juce::Justification::centred);
    }
    for (float db = 0.0f; db >= kMinDb; db -= 12.0f)
    {
        const float y = area.getY() + dbToY (db, h, kMinDb, kMaxDb);
        g.setColour (Theme::grid);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (Theme::textDim);
        g.drawText (juce::String ((int) db),
                    (int) area.getX() + Scale::px (2, s),
                    (int) y - Scale::px (6, s),
                    Scale::px (32, s), Scale::px (12, s),
                    juce::Justification::left);
    }
}

void SpectrumPanel::drawCurve (juce::Graphics& g, juce::Rectangle<float> area,
                               const std::array<float, SpectrumAnalyzer::kNumBins>& bins,
                               double sr, juce::Colour colour, float strokeWidth)
{
    if (sr <= 0.0) return;
    const float w = area.getWidth();
    const float h = area.getHeight();
    const float ox = area.getX();
    const float oy = area.getY();
    const float binHz = (float) (sr / (double) SpectrumAnalyzer::kFftSize);

    juce::Path p;
    bool started = false;

    for (int px = 0; px < (int) w; ++px)
    {
        const float t  = (float) px / w;
        const float minL = std::log10 (kMinHz);
        const float maxL = std::log10 (kMaxHz);
        const float fLo = std::pow (10.0f, minL + t * (maxL - minL));
        const float t2  = (float) (px + 1) / w;
        const float fHi = std::pow (10.0f, minL + t2 * (maxL - minL));

        const int binLo = juce::jlimit (1, SpectrumAnalyzer::kNumBins - 1,
                                        (int) std::floor (fLo / binHz));
        const int binHi = juce::jlimit (binLo, SpectrumAnalyzer::kNumBins - 1,
                                        (int) std::ceil (fHi / binHz));

        float maxDb = -200.0f;
        for (int i = binLo; i <= binHi; ++i)
            if (bins[(size_t) i] > maxDb) maxDb = bins[(size_t) i];

        const float x = ox + (float) px;
        const float y = oy + dbToY (maxDb, h, kMinDb, kMaxDb);

        if (! started) { p.startNewSubPath (x, y); started = true; }
        else            p.lineTo (x, y);
    }

    g.setColour (colour.withAlpha (0.18f));
    juce::Path filled = p;
    filled.lineTo (area.getRight(), area.getBottom());
    filled.lineTo (area.getX(),     area.getBottom());
    filled.closeSubPath();
    g.fillPath (filled);

    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (strokeWidth));
}

void SpectrumPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    const float s = Scale::forArea (getLocalBounds(), 700, 280);
    auto area = getLocalBounds().toFloat().reduced (Scale::pxf (8.0f, s));

    drawGrid (g, area);

    const float strokeBase = 1.5f;

    if (showA && proc.snapA().valid)
        drawCurve (g, area, proc.snapA().spectrumDb, proc.snapA().sampleRate,
                   Theme::aColour, Scale::pxf (strokeBase, s));

    if (showB && proc.snapB().valid)
        drawCurve (g, area, proc.snapB().spectrumDb, proc.snapB().sampleRate,
                   Theme::bColour, Scale::pxf (strokeBase, s));

    // Live INPUT spectrum (the source signal pre-limiter)
    std::array<float, SpectrumAnalyzer::kNumBins> liveIn {};
    const double sr = proc.getInputStage().spectrum.copySpectrum (liveIn);
    drawCurve (g, area, liveIn, sr, Theme::accent, Scale::pxf (1.8f, s));

    g.setColour (Theme::textDim);
    g.setFont (Scale::font (11.0f, s));
    g.drawText ("SPECTRUM (log Hz / dBFS)",
                area.removeFromTop (Scale::pxf (14.0f, s)).toNearestInt(),
                juce::Justification::centredRight);
}
