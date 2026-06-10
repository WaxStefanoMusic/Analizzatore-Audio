#include "PluginEditor.h"
#include "GUI/Theme.h"

AnalizzatoreAudioEditor::AnalizzatoreAudioEditor (AnalizzatoreAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      spectrumPanel (p),
      inputMeters  (p, MetersPanel::Side::Input),
      outputMeters (p, MetersPanel::Side::Output),
      abPanel      (p, spectrumPanel),
      targetPanel  (p)
{
    addAndMakeVisible (spectrumPanel);
    addAndMakeVisible (inputMeters);
    addAndMakeVisible (outputMeters);
    addAndMakeVisible (abPanel);
    addAndMakeVisible (targetPanel);

    resetMetersBtn.setColour (juce::TextButton::buttonColourId,
                              Theme::accent.withAlpha (0.35f));
    resetMetersBtn.setColour (juce::TextButton::textColourOnId,  Theme::text);
    resetMetersBtn.setColour (juce::TextButton::textColourOffId, Theme::text);
    resetMetersBtn.setTooltip (juce::String::fromUTF8 (
        "Azzera le misurazioni INPUT e OUTPUT (Integrated LUFS, LRA, "
        "True Peak max, RMS). Premilo prima di iniziare un nuovo brano "
        "per misurarlo da zero. Non tocca gli snapshot A/B."));
    resetMetersBtn.onClick = [this] { proc.resetMeasurements(); };
    addAndMakeVisible (resetMetersBtn);

    // Compact-friendly resize range: usable on a 1366x768 laptop with room for
    // the DAW around it, scales proportionally up to 5K.
    setResizable (true, true);
    setResizeLimits (720, 520, 6000, 4000);
    getConstrainer()->setFixedAspectRatio (0.0);

    const auto bounds = chooseInitialBounds();
    setSize (bounds.getWidth(), bounds.getHeight());

    // Verifica asincrona di nuove release (esegue una sola volta per processo).
    UpdateChecker::checkAsync (this);
}

AnalizzatoreAudioEditor::~AnalizzatoreAudioEditor()
{
    proc.savedEditorWidth  = getWidth();
    proc.savedEditorHeight = getHeight();
}

juce::Rectangle<int> AnalizzatoreAudioEditor::chooseInitialBounds() const
{
    if (proc.savedEditorWidth > 0 && proc.savedEditorHeight > 0)
        return { proc.savedEditorWidth, proc.savedEditorHeight };

    // Default footprint: ~half of a 1080p screen, comfortable on a single
    // monitor while leaving room for the DAW around it.
    constexpr int designW = 1040, designH = 660;
    constexpr float designRatio = (float) designW / (float) designH;

    auto* primary = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (primary == nullptr)
        return { designW, designH };

    auto userArea = primary->userArea;
    const float maxW = (float) userArea.getWidth()  * 0.65f;
    const float maxH = (float) userArea.getHeight() * 0.75f;
    const float targetW = std::min (maxW, maxH * designRatio);
    const float targetH = targetW / designRatio;

    int w = juce::jlimit (720, 6000, (int) std::round (targetW));
    int h = juce::jlimit (520, 4000, (int) std::round (targetH));

    if (w > 1400) { w = 1400; h = (int) std::round ((float) w / designRatio); }
    return { w, h };
}

void AnalizzatoreAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    const float s = Scale::forArea (getLocalBounds(), 1040, 660);
    auto header = getLocalBounds().removeFromTop (Scale::px (32, s));
    g.setColour (Theme::text);
    g.setFont (Scale::font (18.0f, s, true));
    g.drawText ("ANALIZZATORE AUDIO",
                header.reduced (Scale::px (12, s), 0),
                juce::Justification::centredLeft);
}

void AnalizzatoreAudioEditor::paintOverChildren (juce::Graphics& g)
{
    // Linee di collegamento tra il bottone Reset centrale e i pannelli
    // INPUT (sx) / OUTPUT (dx). Disegnate qui, sopra ai children, cosi'
    // possono entrare dentro l'area dei pannelli fino al loro centro
    // orizzontale, dove un breve tratto verticale punta verso l'alto.
    const auto rb = resetMetersBtn.getBounds();
    if (rb.isEmpty()) return;

    const float s     = Scale::forArea (getLocalBounds(), 1040, 660);
    const float yMid  = (float) rb.getCentreY();
    const float gap   = Scale::pxf (6.0f, s);
    const float thick = Scale::pxf (1.2f, s);
    const float vUp   = Scale::pxf (10.0f, s);   // tratto verticale verso l'alto
    const auto col    = Theme::accent.withAlpha (0.55f);

    g.setColour (col);

    // ---- Linea SX: bottone -> centro orizzontale del pannello INPUT --------
    const float lxStart = (float) rb.getX() - gap;
    const float lxEnd   = (float) inputMeters.getBounds().getCentreX();
    if (lxStart > lxEnd)
    {
        g.drawLine (lxEnd, yMid, lxStart, yMid, thick);
        // Tratto verticale solo verso l'alto, ancorato al centro del pannello.
        g.drawLine (lxEnd, yMid - vUp, lxEnd, yMid, thick);
    }

    // ---- Linea DX: bottone -> centro orizzontale del pannello OUTPUT -------
    const float rxStart = (float) rb.getRight() + gap;
    const float rxEnd   = (float) outputMeters.getBounds().getCentreX();
    if (rxEnd > rxStart)
    {
        g.drawLine (rxStart, yMid, rxEnd, yMid, thick);
        g.drawLine (rxEnd, yMid - vUp, rxEnd, yMid, thick);
    }
}

void AnalizzatoreAudioEditor::resized()
{
    const float s = Scale::forArea (getLocalBounds(), 1040, 660);
    auto area = getLocalBounds();
    area.removeFromTop (Scale::px (32, s));     // header
    const int pad = Scale::px (6, s);
    area.reduce (pad, pad);

    // ---- Bottom row: A/B + Target (split 50/50) -----------------------------
    const int bottomH = juce::jlimit (Scale::px (220, s),
                                      Scale::px (560, s),
                                      (int) ((float) area.getHeight() * 0.34f));
    auto bottom = area.removeFromBottom (bottomH);
    area.removeFromBottom (pad);

    const int abW = (int) ((float) (bottom.getWidth() - pad) * 0.50f);
    abPanel.setBounds (bottom.removeFromLeft (abW));
    bottom.removeFromLeft (pad);
    targetPanel.setBounds (bottom);

    // ---- Reset meters strip (centered between spectrum and bottom row) ------
    const int resetStripH = Scale::px (28, s);
    auto resetStrip = area.removeFromBottom (resetStripH);
    area.removeFromBottom (pad);

    {
        const int btnW = Scale::px (160, s);
        const int btnH = Scale::px (24, s);
        const int x = resetStrip.getCentreX() - btnW / 2;
        const int y = resetStrip.getCentreY() - btnH / 2;
        resetMetersBtn.setBounds (x, y, btnW, btnH);
    }

    // ---- Top row: INPUT meters | Spectrum | OUTPUT meters -------------------
    const int metersW = juce::jlimit (Scale::px (200, s),
                                      Scale::px (340, s),
                                      (int) ((float) area.getWidth() * 0.23f));

    inputMeters.setBounds (area.removeFromLeft (metersW));
    area.removeFromLeft (pad);
    outputMeters.setBounds (area.removeFromRight (metersW));
    area.removeFromRight (pad);

    spectrumPanel.setBounds (area);
}
