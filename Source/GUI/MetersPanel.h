#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HelpBadge.h"

class AnalizzatoreAudioProcessor;
struct AnalyzerStage;

class MetersPanel : public juce::Component,
                    private juce::Timer
{
public:
    enum class Side { Input, Output };

    MetersPanel (AnalizzatoreAudioProcessor& p, Side side);
    ~MetersPanel() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    struct Layout
    {
        juce::Rectangle<int> header;
        juce::Rectangle<int> sLoudnessTitle;
        juce::Rectangle<int> rowMom, rowShort, rowInt, rowLra;
        juce::Rectangle<int> sLevelsTitle;
        juce::Rectangle<int> rowTp, rowGr;
        juce::Rectangle<int> sBarsTitle;
        juce::Rectangle<int> barPkL, barPkR, barRmsL, barRmsR;
        float scale = 1.0f;
    };
    Layout computeLayout() const;

    void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& title, float scale);
    void drawValueRow     (juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& label, const juce::String& value,
                           juce::Colour valueColour, float scale, bool largeValue,
                           bool reserveBadge);
    void drawLevelBar     (juce::Graphics& g, juce::Rectangle<int> r,
                           float dbValue, float minDb, float maxDb,
                           const juce::String& label, float scale);

    AnalizzatoreAudioProcessor& proc;
    Side side;
    AnalyzerStage* stage = nullptr;

    HelpBadge bMomentary, bShort, bIntegrated, bLra;
    HelpBadge bTruePeak,  bGr;
    HelpBadge bBars;        // single badge for the PEAK/RMS section
};
