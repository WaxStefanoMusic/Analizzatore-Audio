#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/MetersPanel.h"
#include "GUI/SpectrumPanel.h"
#include "GUI/ABPanel.h"
#include "GUI/TargetPanel.h"
#include "UpdateChecker.h"

class AnalizzatoreAudioEditor : public juce::AudioProcessorEditor
{
public:
    explicit AnalizzatoreAudioEditor (AnalizzatoreAudioProcessor& p);
    ~AnalizzatoreAudioEditor() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Rectangle<int> chooseInitialBounds() const;

    AnalizzatoreAudioProcessor& proc;

    juce::TooltipWindow tooltipWindow { this, 350 };

    SpectrumPanel spectrumPanel;
    MetersPanel   inputMeters;
    MetersPanel   outputMeters;
    ABPanel       abPanel;
    TargetPanel   targetPanel;

    // Bottone centrale tra spettro e bottom row: resetta INPUT/OUTPUT meters.
    juce::TextButton resetMetersBtn { "Reset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalizzatoreAudioEditor)
};
