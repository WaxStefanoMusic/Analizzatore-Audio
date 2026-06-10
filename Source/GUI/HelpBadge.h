#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

/**
    A small circular "?" badge that shows a tooltip on hover and a longer
    explanation popup on click. Used inline next to metric labels.
*/
class HelpBadge : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    HelpBadge() = default;

    /** Set the short tooltip (shown on hover) and the longer detailed body
        (shown when the badge is clicked).                                       */
    void setHelp (const juce::String& title,
                  const juce::String& shortText,
                  const juce::String& longText)
    {
        helpTitle = title;
        helpLong  = longText;
        setTooltip (shortText);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = std::min (r.getWidth(), r.getHeight()) * 0.5f;
        const auto centre  = r.getCentre();

        const bool hover = isMouseOver();
        g.setColour (hover ? Theme::accent : Theme::textDim.withAlpha (0.8f));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour (Theme::bg);
        g.setFont (juce::Font (juce::FontOptions (radius * 1.4f, juce::Font::bold)));
        g.drawText ("?", getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (helpLong.isEmpty()) return;
        auto opts = juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::InfoIcon)
                        .withTitle    (helpTitle.isEmpty() ? juce::String ("Info") : helpTitle)
                        .withMessage  (helpLong)
                        .withButton   ("OK");
        juce::AlertWindow::showAsync (opts, nullptr);
    }

private:
    juce::String helpTitle, helpLong;
};
