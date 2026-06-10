#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class AnalizzatoreAudioProcessor;

class TargetPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit TargetPanel (AnalizzatoreAudioProcessor& p);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void onTargetChanged();

    AnalizzatoreAudioProcessor& proc;

    juce::Label    targetLabel;
    juce::ComboBox targetCombo;
    juce::TextButton applySuggested { "Apply to Input Gain" };
    juce::TextButton resetGain      { "Reset" };
    juce::Label    statusLabel;        // delta + true-peak warning, full width

    juce::Label    inputGainLabel;
    juce::Slider   inputGainSlider;
    juce::ToggleButton limiterOn { "Limiter ON" };
    juce::Label    ceilingLabel;
    juce::Slider   ceilingSlider;
    juce::Label    releaseLabel;
    juce::Slider   releaseSlider;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SAtt> ceilingAtt, releaseAtt, inputGainAtt;
    std::unique_ptr<BAtt> limiterAtt;

    int currentTargetIdx = 1;          // default: Spotify
};
