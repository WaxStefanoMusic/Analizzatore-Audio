#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Analyzer/SpectrumAnalyzer.h"
#include "../Analyzer/Snapshot.h"

class AnalizzatoreAudioProcessor;

class SpectrumPanel : public juce::Component,
                      private juce::Timer
{
public:
    explicit SpectrumPanel (AnalizzatoreAudioProcessor& p);

    void paint (juce::Graphics& g) override;
    void resized() override {}

    void setShowA (bool s) { showA = s; repaint(); }
    void setShowB (bool s) { showB = s; repaint(); }

private:
    void timerCallback() override { repaint(); }

    void drawCurve (juce::Graphics& g, juce::Rectangle<float> area,
                    const std::array<float, SpectrumAnalyzer::kNumBins>& bins,
                    double sr, juce::Colour colour, float strokeWidth);
    void drawGrid  (juce::Graphics& g, juce::Rectangle<float> area);

    static float freqToX (float hz, float w);
    static float dbToY   (float db, float h, float minDb, float maxDb);

    AnalizzatoreAudioProcessor& proc;
    bool showA = false, showB = false;
};
