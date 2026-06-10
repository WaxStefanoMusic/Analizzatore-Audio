#include "TargetPanel.h"
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../StreamingTargets.h"

TargetPanel::TargetPanel (AnalizzatoreAudioProcessor& p) : proc (p)
{
    targetLabel.setText ("Target:", juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (targetLabel);

    auto names = StreamingTargets::names();
    for (int i = 0; i < names.size(); ++i)
        targetCombo.addItem (names[i], i + 1);
    targetCombo.setSelectedId (currentTargetIdx + 1, juce::dontSendNotification);
    targetCombo.onChange = [this] { onTargetChanged(); };
    targetCombo.setTooltip (
        "Preset di loudness e true-peak per i principali servizi streaming. "
        "Selezionando un preset il ceiling del limiter viene impostato "
        "automaticamente al valore raccomandato dalla piattaforma.");
    addAndMakeVisible (targetCombo);

    statusLabel.setColour (juce::Label::textColourId, Theme::text);
    statusLabel.setColour (juce::Label::backgroundColourId, Theme::panelLight);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    applySuggested.setColour (juce::TextButton::buttonColourId, Theme::accent.withAlpha (0.4f));
    applySuggested.setColour (juce::TextButton::textColourOnId,  Theme::text);
    applySuggested.setColour (juce::TextButton::textColourOffId, Theme::text);
    applySuggested.setTooltip (
        "Calcola la differenza tra l'OUTPUT integrated LUFS e il target del "
        "servizio selezionato, e la somma all'Input Gain. Per avere un valore "
        "affidabile misura prima il brano intero (oppure premi Reset Meas. "
        "e lascialo girare di nuovo).");
    applySuggested.onClick = [this]
    {
        const auto& t = StreamingTargets::byIndex (currentTargetIdx);
        // Use OUTPUT integrated LUFS — that's what reaches the streaming service.
        const float outLufs = proc.getOutputStage().lufs.getIntegrated();
        if (! std::isfinite (outLufs)) return;
        const float delta = t.lufsTarget - outLufs;
        const float currentGainDb = proc.apvts.getRawParameterValue ("inputGain")->load();
        const float newGainDb = juce::jlimit (-24.0f, 24.0f, currentGainDb + delta);
        if (auto* p = proc.apvts.getParameter ("inputGain"))
        {
            const auto norm = p->convertTo0to1 (newGainDb);
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    };
    addAndMakeVisible (applySuggested);

    resetGain.setColour (juce::TextButton::buttonColourId, Theme::panelLight);
    resetGain.setColour (juce::TextButton::textColourOnId,  Theme::text);
    resetGain.setColour (juce::TextButton::textColourOffId, Theme::text);
    resetGain.setTooltip (juce::String::fromUTF8 (
        "Riporta l'Input Gain a 0 dB."));
    resetGain.onClick = [this]
    {
        if (auto* p = proc.apvts.getParameter ("inputGain"))
        {
            const auto norm = p->convertTo0to1 (0.0f);
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    };
    addAndMakeVisible (resetGain);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setColour (juce::Slider::trackColourId, Theme::accent);
        s.setColour (juce::Slider::backgroundColourId, Theme::bg);
        s.setColour (juce::Slider::thumbColourId, Theme::text);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, Theme::panelLight);
        s.setColour (juce::Slider::textBoxTextColourId, Theme::text);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, Theme::textDim);
        addAndMakeVisible (l);
    };
    setupSlider (inputGainSlider, inputGainLabel, "Input Gain");
    setupSlider (ceilingSlider,   ceilingLabel,   "Ceiling");
    setupSlider (releaseSlider,   releaseLabel,   "Release");

    // Unita' di misura mostrate accanto al valore nel text box dello slider.
    inputGainSlider.setTextValueSuffix (" dB");
    inputGainSlider.setNumDecimalPlacesToDisplay (2);
    ceilingSlider.setTextValueSuffix   (" dBTP");
    ceilingSlider.setNumDecimalPlacesToDisplay (2);
    releaseSlider.setTextValueSuffix   (" ms");
    releaseSlider.setNumDecimalPlacesToDisplay (1);

    // I tooltip che contengono accenti italiani vengono wrappati con
    // juce::String::fromUTF8 per garantire una interpretazione UTF-8 esplicita,
    // a prescindere dal codepage di compilazione attivo.
    inputGainSlider.setTooltip (juce::String::fromUTF8 (
        "Gain applicato al segnale in ingresso, da -24 a +24 dB. Modifica "
        "sia la loudness percepita sia il livello che entra nel limiter. "
        "Usa il pulsante 'Apply to Input Gain' per centrarlo automaticamente "
        "sul target streaming selezionato."));
    ceilingSlider.setTooltip (juce::String::fromUTF8 (
        "Soglia massima del limiter true-peak in dBTP. Valori tipici: "
        "-1 dBTP per Spotify, YouTube, Apple Music e Tidal; "
        "-2 dBTP per Spotify Loud e Amazon Music. "
        "Selezionando un preset streaming questo valore viene impostato "
        "automaticamente."));
    releaseSlider.setTooltip (juce::String::fromUTF8 (
        "Tempo di rilascio del limiter, da 5 a 500 ms. "
        "Release brevi (5-30 ms) reagiscono ai transienti ma possono creare "
        "distorsione armonica sui bassi più pronunciati; release lunghe "
        "(150-500 ms) suonano più pulite ma riducono di più la dinamica "
        "complessiva. 50-100 ms è un buon punto di partenza per la musica "
        "moderna."));

    limiterOn.setColour (juce::ToggleButton::textColourId, Theme::text);
    limiterOn.setTooltip (juce::String::fromUTF8 (
        "Attiva o disattiva il limiter true-peak. "
        "Quando è attivo, il segnale viene limitato al ceiling impostato; "
        "quando è spento il segnale passa inalterato, e l'OUTPUT mostra "
        "esattamente l'INPUT (utile per misurare la loudness sorgente "
        "senza alcun processing)."));
    addAndMakeVisible (limiterOn);

    inputGainAtt = std::make_unique<SAtt> (proc.apvts, "inputGain", inputGainSlider);
    ceilingAtt   = std::make_unique<SAtt> (proc.apvts, "ceilingDb", ceilingSlider);
    releaseAtt   = std::make_unique<SAtt> (proc.apvts, "releaseMs", releaseSlider);
    limiterAtt   = std::make_unique<BAtt> (proc.apvts, "limiterOn", limiterOn);

    onTargetChanged();
    startTimerHz (4);
}

void TargetPanel::onTargetChanged()
{
    currentTargetIdx = targetCombo.getSelectedId() - 1;
    const auto& t = StreamingTargets::byIndex (currentTargetIdx);
    if (auto* p = proc.apvts.getParameter ("ceilingDb"))
    {
        const auto norm = p->convertTo0to1 (t.truePeakDb);
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
    repaint();
}

void TargetPanel::resized()
{
    const float s = Scale::forArea (getLocalBounds(), 420, 220);
    auto area = getLocalBounds().reduced (Scale::px (8, s));

    const int titleH    = Scale::px (14, s);
    const int rowH      = Scale::px (24, s);
    const int statusH   = Scale::px (22, s);
    const int gap       = Scale::px (4,  s);
    const int sectionGap= Scale::px (8,  s);
    const int labelW    = Scale::px (78, s);
    const int textBoxW  = Scale::px (72, s);
    const int textBoxH  = Scale::px (18, s);

    inputGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxW, textBoxH);
    ceilingSlider.setTextBoxStyle  (juce::Slider::TextBoxRight, false, textBoxW, textBoxH);
    releaseSlider.setTextBoxStyle  (juce::Slider::TextBoxRight, false, textBoxW, textBoxH);

    targetLabel.setFont    (Scale::font (11.0f, s));
    inputGainLabel.setFont (Scale::font (11.0f, s));
    ceilingLabel.setFont   (Scale::font (11.0f, s));
    releaseLabel.setFont   (Scale::font (11.0f, s));
    statusLabel.setFont    (Scale::font (12.0f, s, true));

    area.removeFromTop (titleH);
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        targetLabel.setBounds (row.removeFromLeft (Scale::px (44, s)));
        row.removeFromLeft (gap);
        targetCombo.setBounds (row.removeFromLeft (Scale::px (140, s)));
        row.removeFromLeft (gap);
        // Reset gain occupa spazio fisso a destra; Apply prende il resto.
        resetGain.setBounds (row.removeFromRight (Scale::px (60, s)));
        row.removeFromRight (gap);
        applySuggested.setBounds (row);
    }
    area.removeFromTop (gap);

    statusLabel.setBounds (area.removeFromTop (statusH));

    area.removeFromTop (sectionGap);

    {
        auto row = area.removeFromTop (rowH);
        inputGainLabel.setBounds  (row.removeFromLeft (labelW));
        inputGainSlider.setBounds (row);
    }
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        limiterOn.setBounds (row.removeFromLeft (Scale::px (130, s)));
    }
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        ceilingLabel.setBounds  (row.removeFromLeft (labelW));
        ceilingSlider.setBounds (row);
    }
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        releaseLabel.setBounds  (row.removeFromLeft (labelW));
        releaseSlider.setBounds (row);
    }
}

void TargetPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    const float s = Scale::forArea (getLocalBounds(), 420, 220);
    const int pad = Scale::px (8, s);

    auto title = getLocalBounds().reduced (pad).removeFromTop (Scale::px (14, s));
    g.setColour (Theme::textDim);
    g.setFont (Scale::font (9.5f, s, true));
    g.drawText ("STREAMING TARGET", title, juce::Justification::centredLeft);
    g.setColour (Theme::grid);
    g.drawHorizontalLine (title.getBottom() - Scale::px (1, s),
                          (float) title.getX(), (float) title.getRight());

    // Update status label content & colour — uses OUTPUT integrated LUFS,
    // which is what the streaming platform actually receives.
    const auto& t = StreamingTargets::byIndex (currentTargetIdx);
    const float outL  = proc.getOutputStage().lufs.getIntegrated();
    const float tpMax = proc.getOutputStage().truePeak.getMaxTruePeakDb();
    juce::String txt;
    juce::Colour col = Theme::textDim;

    if (! std::isfinite (outL))
    {
        txt = "  Misurazione Integrated in corso...";
    }
    else
    {
        const float delta = t.lufsTarget - outL;
        const juce::String dStr = (delta >= 0 ? "+" : "") + juce::String (delta, 1);
        txt = "  OUTPUT vs " + t.name + " (" + juce::String (t.lufsTarget, 1)
              + " LUFS): differenza " + dStr + " dB";
        if (std::abs (delta) <= 0.5f) col = Theme::green;
        else if (std::abs (delta) <= 1.5f) col = Theme::amber;
        else col = Theme::red;

        if (std::isfinite (tpMax) && tpMax > t.truePeakDb)
            txt += "    [!] TP " + juce::String (tpMax, 2) + " > "
                   + juce::String (t.truePeakDb, 1) + " dBTP";
    }
    statusLabel.setText (txt, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, col);
}
