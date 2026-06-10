# Analizzatore Audio — VST3 per FL Studio

Plugin VST3 (e Standalone) per analisi audio in tempo reale: **Peak / RMS / LUFS / Spettro / A·B / Target Streaming + True-Peak Limiter**.

## Funzionalità

- **Loudness BS.1770-4 / EBU R128**: Momentary (400 ms), Short-term (3 s), Integrated (gated), LRA
- **True-Peak (dBTP)** con oversampling 4× — peak L/R + max storico
- **RMS** (finestra ~300 ms) per canale
- **Spettro** FFT 4096 con scala log-Hz / dB e smoothing
- **A/B compare**: cattura due snapshot e confronta tutti i valori + le curve di spettro sovrapposte
- **Preset target streaming**: Spotify (–14 / –1 dBTP), Spotify Loud, YouTube, Apple Music, Tidal, Amazon, SoundCloud, EBU R128
  - Mostra il delta vs target e ha un pulsante **Apply to Input Gain** per centrare il valore
- **True-Peak Limiter** con lookahead, ceiling e release, latency-compensated tramite `setLatencySamples`

L'analisi è **pre-limiter**: vedi i livelli della sorgente, non l'output dopo il limiting.

## Layout finestra

```
┌────────────────────────────────────────────────────────────┐
│ Header                                                     │
├──────────┬─────────────────────────────────────────────────┤
│ Meters   │ Spettro (live + snapshot A in arancio + B viola)│
│ (LUFS,   │                                                 │
│  TP, GR, │                                                 │
│  PK L/R, │                                                 │
│  RMS L/R)│                                                 │
│          ├─────────────────────────┬───────────────────────┤
│          │ A/B compare table       │ Streaming target +    │
│          │ + Capture A / B / Reset │ Limiter (ceiling/rel) │
└──────────┴─────────────────────────┴───────────────────────┘
```

---

## Build (Windows)

### 1. Installa i prerequisiti

Hai già Git. Mancano CMake e il compilatore C++ MSVC.

**A. Visual Studio Build Tools 2022** (gratuiti, no IDE completo):

- Scarica: <https://visualstudio.microsoft.com/downloads/?q=build+tools>
- Sezione *Tools for Visual Studio* → *Build Tools for Visual Studio 2022*
- Durante l'installazione seleziona il workload **"Desktop development with C++"** (include MSVC, Windows 10/11 SDK e CMake integrato).

**B. CMake** (se non incluso): <https://cmake.org/download/> — scegli l'installer Windows x64 e abilita *"Add CMake to system PATH"*.

Riavvia il terminale dopo l'installazione e verifica:

```powershell
cmake --version
cl /?
```

### 2. Configura e compila

Dalla cartella del progetto (`d:\Analizzatore Audio e RMS`):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

La prima esecuzione scarica JUCE 8.0.4 (~150 MB) tramite `FetchContent` — può richiedere qualche minuto.

### 3. Output

Dopo la compilazione troverai:

- **VST3**: `build\AnalizzatoreAudio_artefacts\Release\VST3\Analizzatore Audio.vst3`
- **Standalone**: `build\AnalizzatoreAudio_artefacts\Release\Standalone\Analizzatore Audio.exe`

Il flag `COPY_PLUGIN_AFTER_BUILD TRUE` nel CMakeLists prova anche a copiare automaticamente il VST3 in `C:\Program Files\Common Files\VST3\` (richiede permessi di amministratore — se fallisce copia il `.vst3` a mano).

### 4. Caricarlo in FL Studio

1. FL Studio → *Options → Manage plugins*
2. Aggiungi il path della cartella VST3 (default `C:\Program Files\Common Files\VST3`)
3. *Find plugins* → trova "Analizzatore Audio"
4. Inseriscilo come **effect** sul mixer track (es. il Master) — è un effetto FX, non uno strumento.

---

## Note tecniche

- **Architettura**: i meter LUFS, TruePeak, RMS e lo spectrum analyzer leggono il segnale **prima** del limiter, così i valori che vedi sono quelli "sorgente". Il limiter applica gain reduction al segnale in uscita (con latency compensation segnalata all'host).
- **A/B**: gli snapshot non bloccano l'analisi — semplicemente "fotografano" lo stato corrente di tutti i meter e dello spettro. Premi **Reset Meas.** per azzerare l'integrato/LRA e iniziare una nuova misurazione di un brano.
- **Apply to Input Gain**: applica un gain delta = `target − integrated`. Misura prima il pezzo intero, poi premi Apply per posizionarlo al target. Per evitare clipping il limiter (ON di default) interviene al ceiling impostato.
- **Latency**: il limiter dichiara la latency = `oversampling.getLatencyInSamples() + lookahead/4` campioni a sample-rate host. FL Studio compensa automaticamente.

## Layout file sorgente

```
Source/
├── PluginProcessor.{h,cpp}      # parameters, audio pipeline, A/B capture
├── PluginEditor.{h,cpp}         # window layout
├── StreamingTargets.h           # platform presets
├── Analyzer/
│   ├── LufsMeter.{h,cpp}        # BS.1770-4 K-weighting + gating
│   ├── TruePeakDetector.{h,cpp} # 4x oversampled peak
│   ├── SpectrumAnalyzer.{h,cpp} # FFT 4096 + log smoothing
│   └── Snapshot.h               # A/B snapshot struct
├── DSP/
│   └── TruePeakLimiter.{h,cpp}  # lookahead + 4x oversampled limiter
└── GUI/
    ├── Theme.h
    ├── MetersPanel.{h,cpp}      # LUFS / TP / GR / Peak / RMS bars
    ├── SpectrumPanel.{h,cpp}    # log Hz spectrum + A/B overlays
    ├── ABPanel.{h,cpp}          # capture buttons + diff table
    └── TargetPanel.{h,cpp}      # streaming preset + limiter controls
```
