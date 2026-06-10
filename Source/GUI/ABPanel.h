#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HelpBadge.h"

class AnalizzatoreAudioProcessor;
class SpectrumPanel;

class ABPanel : public juce::Component,
                private juce::Timer
{
public:
    ABPanel (AnalizzatoreAudioProcessor& p, SpectrumPanel& spectrumPanel);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    AnalizzatoreAudioProcessor& proc;
    SpectrumPanel& spectrum;

    juce::TextButton captureA { "Capture A" };
    juce::TextButton captureB { "Capture B" };
    juce::ToggleButton showA  { "Show A" };
    juce::ToggleButton showB  { "Show B" };
    juce::TextButton resetMeas { "Reset" };

    HelpBadge bAB;          // help badge next to the section title
};
