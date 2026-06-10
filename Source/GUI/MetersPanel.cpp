#include "MetersPanel.h"
#include "Theme.h"
#include "../PluginProcessor.h"

namespace
{
    juce::String fmtDb (float v, int decimals = 1)
    {
        if (! std::isfinite (v) || v <= -100.0f) return "-inf";
        return juce::String (v, decimals) + " dB";
    }
    juce::String fmtLufs (float v, int decimals = 1)
    {
        if (! std::isfinite (v) || v <= -100.0f) return "-inf";
        return juce::String (v, decimals) + " LUFS";
    }
}

MetersPanel::MetersPanel (AnalizzatoreAudioProcessor& p, Side s)
    : proc (p), side (s)
{
    stage = (side == Side::Input) ? &proc.getInputStage() : &proc.getOutputStage();

    // ---- Help texts (Italian) -------------------------------------------------
    bMomentary.setHelp (
        "Momentary (LUFS-M)",
        "Loudness istantanea calcolata sugli ultimi 400 ms.",
        "Loudness Momentary (ITU-R BS.1770-4): valore medio della loudness "
        "K-weighted calcolato sugli ultimi 400 ms del segnale.\n\n"
        "È il valore che segue più rapidamente l'audio: utile per vedere "
        "quanto il pezzo suoni forte istante per istante, ma è troppo "
        "instabile per stabilire la loudness complessiva del brano (per "
        "quella serve l'Integrated).");

    bShort.setHelp (
        "Short-term (LUFS-S)",
        "Loudness media calcolata sugli ultimi 3 secondi.",
        "Loudness Short-term (ITU-R BS.1770-4): media K-weighted sugli "
        "ultimi 3 secondi del segnale.\n\n"
        "Più stabile della Momentary. Utile per vedere come variano le "
        "sezioni del brano (per esempio strofa vs ritornello) e per "
        "controllare la dinamica percepita durante l'ascolto.");

    bIntegrated.setHelp (
        "Integrated (LUFS-I)",
        "Loudness media (con gating) dell'intero brano. Valore usato dai servizi streaming.",
        "Loudness Integrated (ITU-R BS.1770-4 / EBU R128): media della "
        "loudness K-weighted dall'inizio della misurazione fino a ora, "
        "calcolata su blocchi da 400 ms con sovrapposizione del 75%.\n\n"
        "Sono applicati due gate per escludere le parti irrilevanti:\n"
        "  - gate assoluto a -70 LUFS, per ignorare il silenzio;\n"
        "  - gate relativo a -10 LU sotto la media non-gated, per ignorare "
        "le parti molto più basse del programma principale.\n\n"
        "Questo è il valore che i servizi streaming confrontano con i loro "
        "target di normalizzazione: Spotify e YouTube -14 LUFS, Apple Music "
        "-16 LUFS, broadcast EBU R128 -23 LUFS.\n\n"
        "Premi 'Reset Meas.' prima di iniziare un nuovo brano, altrimenti "
        "il valore continua ad accumulare le misurazioni precedenti.");

    bLra.setHelp (
        "Loudness Range (LRA)",
        "Differenza in LU tra le parti più tranquille e quelle più forti del brano.",
        "Loudness Range / LRA (EBU Tech 3342): misura la dinamica percepita "
        "del brano nel suo complesso.\n\n"
        "È calcolato come differenza tra il 95-esimo e il 10-esimo "
        "percentile dei blocchi short-term (3 s con hop di 1 s), dopo aver "
        "applicato un gate assoluto a -70 LUFS e un gate relativo a -20 LU.\n\n"
        "Valori indicativi:\n"
        "  -  4-7 LU: brano molto compresso (pop e EDM moderni);\n"
        "  -  8-12 LU: mix dinamico (pop e rock ben masterizzato);\n"
        "  - 15+ LU: classica, jazz live, registrazioni acustiche.\n\n"
        "I servizi streaming normalizzano la loudness ma non la dinamica: "
        "un LRA alto resta alto e viceversa.");

    bTruePeak.setHelp (
        "True Peak (dBTP)",
        "Picco del segnale ricostruito con oversampling 4x.",
        "True Peak (ITU-R BS.1770 Annex 2): stima del picco del segnale "
        "analogico ricostruito a partire dal PCM, ottenuta tramite "
        "oversampling a 4 volte la sample rate originale.\n\n"
        "Perché serve: il sample peak normale guarda solo i campioni "
        "discreti del PCM, mentre i picchi reali del segnale possono "
        "trovarsi tra due campioni (inter-sample peaks). Un file con "
        "sample peak = 0 dBFS può tranquillamente avere True Peak "
        "superiore a +1 dBTP, e in quel caso andrà in clipping sul DAC "
        "di un dispositivo consumer o quando viene convertito in MP3/AAC.\n\n"
        "Soglie di sicurezza per lo streaming:\n"
        "  - Spotify, YouTube, Tidal, Apple Music: -1 dBTP;\n"
        "  - Spotify Loud, Amazon Music: -2 dBTP.\n\n"
        "Se il valore diventa rosso (sopra -1 dBTP) abbassa il ceiling "
        "del limiter oppure riduci l'input gain.");

    bGr.setHelp (
        "Gain Reduction (GR)",
        "Quanti dB il limiter sta togliendo al segnale per restare sotto il ceiling.",
        "Gain Reduction: l'attenuazione che il limiter true-peak sta "
        "applicando in questo istante per impedire al segnale di superare "
        "il ceiling impostato.\n\n"
        "Come leggerla:\n"
        "  -   0.0 dB: limiter inattivo, segnale già sotto la soglia;\n"
        "  -  -0.5 / -2 dB: il limiter lavora in modo trasparente;\n"
        "  -  -3 / -6 dB: limiting marcato, il brano perde dinamica;\n"
        "  -   < -6 dB: stai forzando troppo. Per ridurre il limiting puoi "
        "alzare il ceiling, abbassare l'input gain, oppure inserire una "
        "compressione a monte che ammorbidisca i picchi prima che "
        "raggiungano il limiter.\n\n"
        "Nel pannello INPUT questo valore non è significativo (il limiter "
        "agisce dopo l'analisi INPUT, e prima di OUTPUT).");

    bBars.setHelp (
        "Peak / RMS bars",
        "Picco istantaneo (true-peak) e valore RMS per i canali L e R.",
        "PK L / PK R: picco istantaneo per canale, oversampled 4x (true-"
        "peak vero). Sale istantaneamente e decade lentamente per essere "
        "facile da leggere a occhio.\n\n"
        "RMS L / RMS R: valore quadratico medio su una finestra di circa "
        "300 ms. Indica la potenza media percepita del segnale, ed è "
        "molto più affidabile del peak per giudicare il volume soggettivo.\n\n"
        "Codice colore: verde fino a -6 dB, arancione tra -6 e -1 dB, "
        "rosso sopra -1 dB.");

    addAndMakeVisible (bMomentary);
    addAndMakeVisible (bShort);
    addAndMakeVisible (bIntegrated);
    addAndMakeVisible (bLra);
    addAndMakeVisible (bTruePeak);
    addAndMakeVisible (bGr);
    addAndMakeVisible (bBars);

    startTimerHz (30);
}

MetersPanel::Layout MetersPanel::computeLayout() const
{
    Layout L;
    const float s = Scale::forArea (getLocalBounds(), 240, 480);
    L.scale = s;

    const int pad        = Scale::px (8,  s);
    const int gap        = Scale::px (3,  s);
    const int sectionGap = Scale::px (7,  s);
    const int titleH     = Scale::px (14, s);
    const int rowH       = Scale::px (24, s);
    const int headerH    = Scale::px (20, s);

    auto area = getLocalBounds().reduced (pad);
    L.header = area.removeFromTop (headerH);
    area.removeFromTop (gap);

    L.sLoudnessTitle = area.removeFromTop (titleH);
    area.removeFromTop (gap);
    L.rowMom   = area.removeFromTop (rowH); area.removeFromTop (gap);
    L.rowShort = area.removeFromTop (rowH); area.removeFromTop (gap);
    L.rowInt   = area.removeFromTop (rowH); area.removeFromTop (gap);
    L.rowLra   = area.removeFromTop (rowH);
    area.removeFromTop (sectionGap);

    L.sLevelsTitle = area.removeFromTop (titleH);
    area.removeFromTop (gap);
    L.rowTp = area.removeFromTop (rowH); area.removeFromTop (gap);
    L.rowGr = area.removeFromTop (rowH);
    area.removeFromTop (sectionGap);

    L.sBarsTitle = area.removeFromTop (titleH);
    area.removeFromTop (gap);

    const int barH = juce::jmax (Scale::px (18, s),
                                 (area.getHeight() - gap * 3) / 4);
    L.barPkL  = area.removeFromTop (barH); area.removeFromTop (gap);
    L.barPkR  = area.removeFromTop (barH); area.removeFromTop (gap);
    L.barRmsL = area.removeFromTop (barH); area.removeFromTop (gap);
    L.barRmsR = area.removeFromTop (barH);
    return L;
}

void MetersPanel::resized()
{
    const Layout L = computeLayout();
    const float s = L.scale;
    const int badgeSize = Scale::px (14, s);
    const int labelW    = Scale::px (66, s);

    auto badgeForRow = [&] (juce::Rectangle<int> row) -> juce::Rectangle<int>
    {
        const int inset = Scale::px (8, s);
        auto inner = row.reduced (inset, Scale::px (3, s));
        const int x = inner.getX() + labelW;
        const int y = inner.getCentreY() - badgeSize / 2;
        return { x, y, badgeSize, badgeSize };
    };

    bMomentary.setBounds  (badgeForRow (L.rowMom));
    bShort.setBounds      (badgeForRow (L.rowShort));
    bIntegrated.setBounds (badgeForRow (L.rowInt));
    bLra.setBounds        (badgeForRow (L.rowLra));
    bTruePeak.setBounds   (badgeForRow (L.rowTp));
    bGr.setBounds         (badgeForRow (L.rowGr));

    const int titleBadgeY = L.sBarsTitle.getCentreY() - badgeSize / 2;
    bBars.setBounds (L.sBarsTitle.getRight() - badgeSize,
                     titleBadgeY, badgeSize, badgeSize);
}

void MetersPanel::drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r,
                                    const juce::String& title, float s)
{
    g.setColour (Theme::textDim);
    g.setFont (Scale::font (9.5f, s, true));
    g.drawText (title, r, juce::Justification::centredLeft);
    g.setColour (Theme::grid);
    g.drawHorizontalLine (r.getBottom() - Scale::px (1, s),
                          (float) r.getX(), (float) r.getRight());
}

void MetersPanel::drawValueRow (juce::Graphics& g, juce::Rectangle<int> r,
                                const juce::String& label, const juce::String& value,
                                juce::Colour valueColour, float s,
                                bool largeValue, bool reserveBadge)
{
    g.setColour (Theme::panelLight);
    g.fillRoundedRectangle (r.toFloat(), Scale::pxf (3.0f, s));

    const int inset = Scale::px (8, s);
    const int labelW = Scale::px (66, s);
    const int badgeReserve = reserveBadge ? Scale::px (20, s) : 0;
    auto inner = r.reduced (inset, Scale::px (3, s));

    g.setColour (Theme::textDim);
    g.setFont (Scale::font (11.0f, s));
    g.drawText (label, inner.removeFromLeft (labelW), juce::Justification::centredLeft);

    inner.removeFromLeft (badgeReserve);

    g.setColour (valueColour);
    g.setFont (Scale::font (largeValue ? 16.0f : 13.5f, s, true));
    g.drawFittedText (value, inner, juce::Justification::centredRight, 1, 0.8f);
}

void MetersPanel::drawLevelBar (juce::Graphics& g, juce::Rectangle<int> r,
                                float dbValue, float minDb, float maxDb,
                                const juce::String& label, float s)
{
    g.setColour (Theme::panelLight);
    g.fillRoundedRectangle (r.toFloat(), Scale::pxf (3.0f, s));

    auto labelArea = r.removeFromLeft (Scale::px (34, s));
    g.setColour (Theme::textDim);
    g.setFont (Scale::font (9.5f, s, true));
    g.drawText (label, labelArea, juce::Justification::centred);

    auto valueArea = r.removeFromRight (Scale::px (54, s));

    auto barArea = r.reduced (Scale::px (3, s), Scale::px (3, s));
    g.setColour (Theme::bg);
    g.fillRoundedRectangle (barArea.toFloat(), Scale::pxf (2.0f, s));

    if (std::isfinite (dbValue))
    {
        const float t = juce::jlimit (0.0f, 1.0f, (dbValue - minDb) / (maxDb - minDb));
        auto fill = barArea.withWidth ((int) ((float) barArea.getWidth() * t));

        juce::Colour c = Theme::green;
        if (dbValue > -6.0f) c = Theme::amber;
        if (dbValue > -1.0f) c = Theme::red;
        g.setColour (c);
        g.fillRoundedRectangle (fill.toFloat(), Scale::pxf (2.0f, s));
    }

    g.setColour (Theme::text);
    g.setFont (Scale::font (10.5f, s));
    g.drawFittedText (fmtDb (dbValue, 1), valueArea, juce::Justification::centredRight, 1, 0.8f);
}

void MetersPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    if (stage == nullptr) return;

    const Layout L = computeLayout();
    const float s = L.scale;
    auto& l = stage->lufs;

    // ---- Big header: INPUT or OUTPUT ----------------------------------------
    const juce::String headerText = (side == Side::Input) ? "INPUT" : "OUTPUT";
    const juce::Colour headerColour = (side == Side::Input)
        ? Theme::accent.withAlpha (0.85f)
        : Theme::green.withAlpha (0.85f);

    g.setColour (headerColour);
    g.setFont (Scale::font (14.0f, s, true));
    g.drawText (headerText, L.header, juce::Justification::centredLeft);

    g.setColour (Theme::textDim);
    g.setFont (Scale::font (9.5f, s));
    const juce::String headerSub = (side == Side::Input)
        ? "pre-limiter"
        : "post-limiter";
    g.drawText (headerSub, L.header, juce::Justification::centredRight);

    // ---- LOUDNESS section ---------------------------------------------------
    drawSectionTitle (g, L.sLoudnessTitle, "LOUDNESS", s);
    drawValueRow (g, L.rowMom,   "Momentary",  fmtLufs (l.getMomentary()),  Theme::accent, s, false, true);
    drawValueRow (g, L.rowShort, "Short-term", fmtLufs (l.getShortTerm()),  Theme::accent, s, false, true);
    drawValueRow (g, L.rowInt,   "Integrated", fmtLufs (l.getIntegrated()), Theme::green,  s, true,  true);
    drawValueRow (g, L.rowLra,   "LRA",        juce::String (l.getLoudnessRange(), 1) + " LU",
                  Theme::text, s, false, true);

    // ---- LEVELS section -----------------------------------------------------
    drawSectionTitle (g, L.sLevelsTitle, "LEVELS", s);

    const float tpMax = stage->truePeak.getMaxTruePeakDb();
    drawValueRow (g, L.rowTp, "True Peak",
                  std::isfinite (tpMax) ? juce::String (tpMax, 2) + " dBTP"
                                        : juce::String ("-inf"),
                  tpMax > -1.0f ? Theme::red : Theme::green, s, false, true);

    // Gain Reduction is only meaningful on the OUTPUT stage (after the limiter)
    const float gr = (side == Side::Output) ? proc.getLimiter().getGainReductionDb() : 0.0f;
    const juce::String grLabel = (side == Side::Output) ? "Gain Red." : "Gain Red.";
    const juce::String grValue = (side == Side::Output)
        ? fmtDb (gr, 2)
        : juce::String ("n/a");
    const juce::Colour grColour = (side == Side::Output)
        ? (gr < -0.05f ? Theme::amber : Theme::textDim)
        : Theme::textDim;
    drawValueRow (g, L.rowGr, grLabel, grValue, grColour, s, false, true);

    // ---- BARS section -------------------------------------------------------
    drawSectionTitle (g, L.sBarsTitle, "PEAK / RMS", s);
    drawLevelBar (g, L.barPkL,  stage->truePeak.getPeakDb (0), -60.0f, 0.0f, "PK L",  s);
    drawLevelBar (g, L.barPkR,  stage->truePeak.getPeakDb (1), -60.0f, 0.0f, "PK R",  s);
    drawLevelBar (g, L.barRmsL, stage->getRmsDb (0),           -60.0f, 0.0f, "RMS L", s);
    drawLevelBar (g, L.barRmsR, stage->getRmsDb (1),           -60.0f, 0.0f, "RMS R", s);
}
