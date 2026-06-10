#include "ABPanel.h"
#include "SpectrumPanel.h"
#include "Theme.h"
#include "../PluginProcessor.h"

ABPanel::ABPanel (AnalizzatoreAudioProcessor& p, SpectrumPanel& sp)
    : proc (p), spectrum (sp)
{
    auto styleBtn = [] (juce::TextButton& b, juce::Colour bg)
    {
        b.setColour (juce::TextButton::buttonColourId, bg);
        b.setColour (juce::TextButton::textColourOnId,  Theme::text);
        b.setColour (juce::TextButton::textColourOffId, Theme::text);
    };
    styleBtn (captureA,  Theme::aColour.withAlpha (0.45f));
    styleBtn (captureB,  Theme::bColour.withAlpha (0.45f));
    styleBtn (resetMeas, Theme::panelLight);

    addAndMakeVisible (captureA);
    addAndMakeVisible (captureB);
    addAndMakeVisible (showA);
    addAndMakeVisible (showB);
    addAndMakeVisible (resetMeas);
    addAndMakeVisible (bAB);

    showA.setColour (juce::ToggleButton::textColourId, Theme::aColour);
    showB.setColour (juce::ToggleButton::textColourId, Theme::bColour);
    showA.setToggleState (true,  juce::dontSendNotification);
    showB.setToggleState (true,  juce::dontSendNotification);

    captureA.setTooltip (
        "Cattura uno snapshot dello stato attuale del segnale INPUT "
        "(LUFS, peak, RMS, spettro) come riferimento A. Procedura tipica: "
        "ascolta il primo brano finché il valore Integrated è stabile, "
        "quindi premi Capture A.");
    captureB.setTooltip (
        "Cattura uno snapshot del segnale INPUT come riferimento B. "
        "Carica un secondo brano nel DAW e premi Capture B per "
        "confrontarlo con A nella tabella qui sotto e nelle curve dello "
        "spettro in alto.");
    showA.setTooltip ("Mostra o nascondi la curva spettro dello snapshot A nel grafico.");
    showB.setTooltip ("Mostra o nascondi la curva spettro dello snapshot B nel grafico.");
    resetMeas.setTooltip (juce::String::fromUTF8 (
        "Cancella gli snapshot A e B (solo questa sezione). "
        "Non tocca le misurazioni dei pannelli INPUT e OUTPUT — "
        "per quelle usa il pulsante 'Reset' centrale sotto allo spettro."));

    bAB.setHelp (
        "A / B Comparison",
        "Cattura due 'fotografie' del segnale INPUT per confrontare due brani fianco a fianco.",
        "A / B Comparison: serve a confrontare due brani sorgente fianco a "
        "fianco, vedendo a colpo d'occhio chi è più forte, chi ha più "
        "dinamica, chi ha più bassi, chi spinge di più sui transienti, ecc.\n\n"
        "Cosa cattura:\n"
        "Quando premi 'Capture A' (o B), viene memorizzato uno snapshot dello "
        "stato corrente del pannello INPUT, cioè del segnale sorgente prima "
        "del limiter. Lo snapshot include:\n"
        "  - LUFS Integrated, Short-term, Momentary\n"
        "  - Loudness Range (LRA)\n"
        "  - True Peak massimo\n"
        "  - RMS L e R\n"
        "  - una copia della curva di spettro corrente\n\n"
        "Perché INPUT e non OUTPUT:\n"
        "Confronti le caratteristiche dei brani sorgente, indipendentemente "
        "dal limiter (che è una scelta di mastering tua, non una proprietà "
        "del brano). Per confrontare due render finali esporta i due file "
        "e analizzali uno alla volta con il limiter spento.\n\n"
        "Procedura tipica:\n"
        "  1. Premi il 'Reset' centrale (sotto allo spettro) per azzerare "
        "le misurazioni INPUT/OUTPUT.\n"
        "  2. Carica e ascolta il primo brano nel DAW finché Integrated è "
        "stabile (in genere serve l'intero brano, o almeno 30-60 secondi "
        "rappresentativi).\n"
        "  3. Premi 'Capture A'.\n"
        "  4. Premi di nuovo il 'Reset' centrale, carica il secondo brano "
        "e ripeti.\n"
        "  5. Premi 'Capture B'.\n"
        "  6. Confronta i valori nella tabella e le due curve di spettro "
        "sovrapposte (A in arancione, B in viola).\n\n"
        "I toggle 'Show A' e 'Show B' permettono di nascondere o mostrare "
        "le curve di spettro nel grafico in alto, lasciando solo la curva "
        "live blu dell'INPUT corrente.\n\n"
        "Il pulsante 'Reset' di questa sezione cancella solo gli snapshot "
        "A e B (la tabella e le curve), senza toccare le misurazioni "
        "INPUT/OUTPUT.");

    captureA.onClick  = [this] { proc.captureSnapshot (true);  repaint(); };
    captureB.onClick  = [this] { proc.captureSnapshot (false); repaint(); };
    showA.onClick     = [this] { spectrum.setShowA (showA.getToggleState()); };
    showB.onClick     = [this] { spectrum.setShowB (showB.getToggleState()); };
    resetMeas.onClick = [this] { proc.clearABSnapshots(); repaint(); };

    spectrum.setShowA (true);
    spectrum.setShowB (true);

    startTimerHz (10);
}

void ABPanel::resized()
{
    const float s = Scale::forArea (getLocalBounds(), 460, 220);
    auto area = getLocalBounds().reduced (Scale::px (8, s));

    const int titleH = Scale::px (14, s);
    const int rowH   = Scale::px (24, s);
    const int gap    = Scale::px (4,  s);

    // Position the help badge right after the section title text.
    auto titleArea = area.removeFromTop (titleH);
    {
        const auto titleFont = Scale::font (9.5f, s, true);
        const int textW = juce::GlyphArrangement::getStringWidthInt (titleFont,
                                                                     "A / B  COMPARISON");
        const int badgeSize = Scale::px (12, s);
        const int x = titleArea.getX() + textW + Scale::px (5, s);
        const int y = titleArea.getCentreY() - badgeSize / 2;
        bAB.setBounds (x, y, badgeSize, badgeSize);
    }
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        const int half = (row.getWidth() - gap) / 2;
        captureA.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (gap);
        captureB.setBounds (row);
    }
    area.removeFromTop (gap);

    {
        auto row = area.removeFromTop (rowH);
        const int third = (row.getWidth() - gap * 2) / 3;
        showA.setBounds     (row.removeFromLeft (third)); row.removeFromLeft (gap);
        showB.setBounds     (row.removeFromLeft (third)); row.removeFromLeft (gap);
        resetMeas.setBounds (row);
    }
}

static juce::String fmt (float v, const juce::String& unit, int dec = 1)
{
    if (! std::isfinite (v) || v <= -100.0f) return "-inf " + unit;
    return juce::String (v, dec) + " " + unit;
}

void ABPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    const float s    = Scale::forArea (getLocalBounds(), 460, 220);
    const int pad    = Scale::px (8,  s);
    const int titleH = Scale::px (14, s);
    const int gap    = Scale::px (4,  s);
    const int rowH   = Scale::px (24, s);

    auto title = getLocalBounds().reduced (pad).removeFromTop (titleH);
    g.setColour (Theme::textDim);
    g.setFont (Scale::font (9.5f, s, true));
    g.drawText ("A / B  COMPARISON", title, juce::Justification::centredLeft);
    g.setColour (Theme::grid);
    g.drawHorizontalLine (title.getBottom() - Scale::px (1, s),
                          (float) title.getX(), (float) title.getRight());

    auto area = getLocalBounds().reduced (pad);
    area.removeFromTop (titleH);
    area.removeFromTop (gap);
    area.removeFromTop (rowH);
    area.removeFromTop (gap);
    area.removeFromTop (rowH);
    area.removeFromTop (Scale::px (8, s));

    auto& A = proc.snapA();
    auto& B = proc.snapB();

    const int tableRowH = Scale::px (18, s);
    const int colMetric = Scale::px (110, s);
    const int remaining = area.getWidth() - colMetric;
    const int colW = remaining / 3;

    {
        auto hdr = area.removeFromTop (tableRowH);
        g.setColour (Theme::textDim);
        g.setFont (Scale::font (9.0f, s, true));
        g.drawText ("METRICA", hdr.removeFromLeft (colMetric),
                    juce::Justification::centredLeft);
        g.setColour (Theme::aColour);
        g.drawText ("A", hdr.removeFromLeft (colW), juce::Justification::centred);
        g.setColour (Theme::bColour);
        g.drawText ("B", hdr.removeFromLeft (colW), juce::Justification::centred);
        g.setColour (Theme::textDim);
        g.drawText ("B - A", hdr, juce::Justification::centred);
    }

    auto drawRow = [&] (int rowIdx, const juce::String& metric,
                        const juce::String& aStr, const juce::String& bStr,
                        const juce::String& dStr,
                        juce::Colour aCol, juce::Colour bCol)
    {
        auto row = area.removeFromTop (tableRowH);
        if ((rowIdx & 1) == 0)
        {
            g.setColour (Theme::panelLight.withAlpha (0.5f));
            g.fillRect (row);
        }
        g.setFont (Scale::font (11.0f, s));
        g.setColour (Theme::text);
        g.drawFittedText (metric, row.removeFromLeft (colMetric)
                              .reduced (Scale::px (3, s), 0),
                          juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (aCol);
        g.drawFittedText (aStr, row.removeFromLeft (colW),
                          juce::Justification::centred, 1, 0.8f);
        g.setColour (bCol);
        g.drawFittedText (bStr, row.removeFromLeft (colW),
                          juce::Justification::centred, 1, 0.8f);
        g.setColour (Theme::textDim);
        g.drawFittedText (dStr, row, juce::Justification::centred, 1, 0.8f);
    };

    auto delta = [] (float a, float b) -> juce::String
    {
        if (! std::isfinite (a) || ! std::isfinite (b)) return "-";
        const float d = b - a;
        return juce::String (d >= 0 ? "+" : "") + juce::String (d, 1);
    };

    auto col = [&] (const Snapshot& sn, juce::Colour base)
    {
        return sn.valid ? base : Theme::textDim;
    };

    int row = 0;
    drawRow (row++, "LUFS Integrated",
             fmt (A.lufsIntegrated, "LUFS"), fmt (B.lufsIntegrated, "LUFS"),
             delta (A.lufsIntegrated, B.lufsIntegrated),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "LUFS Short-term",
             fmt (A.lufsShortTerm, "LUFS"), fmt (B.lufsShortTerm, "LUFS"),
             delta (A.lufsShortTerm, B.lufsShortTerm),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "LUFS Momentary",
             fmt (A.lufsMomentary, "LUFS"), fmt (B.lufsMomentary, "LUFS"),
             delta (A.lufsMomentary, B.lufsMomentary),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "LRA",
             fmt (A.loudnessRange, "LU"),  fmt (B.loudnessRange, "LU"),
             delta (A.loudnessRange, B.loudnessRange),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "True Peak (max)",
             fmt (A.truePeakMaxDb, "dBTP"), fmt (B.truePeakMaxDb, "dBTP"),
             delta (A.truePeakMaxDb, B.truePeakMaxDb),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "RMS L",
             fmt (A.rmsDbL, "dB"), fmt (B.rmsDbL, "dB"),
             delta (A.rmsDbL, B.rmsDbL),
             col (A, Theme::aColour), col (B, Theme::bColour));
    drawRow (row++, "RMS R",
             fmt (A.rmsDbR, "dB"), fmt (B.rmsDbR, "dB"),
             delta (A.rmsDbR, B.rmsDbR),
             col (A, Theme::aColour), col (B, Theme::bColour));
}
