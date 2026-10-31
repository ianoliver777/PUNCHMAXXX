# PUNCHMAXXX — Transient Drum Processor

by barnacle boi · JUCE 8 · VST3 / AU / Standalone (macOS universal, Windows)

## What the controls do

| Control | What it does |
|---|---|
| **TRANSIENT** (-100…+100) | Boosts or softens the attack of every hit (up to ±18 dB on the transient). |
| **PUNCH** (0…100) | Extra attack boost on the low band only (below FOCUS) — the thump of kicks/snares. |
| **BODY** (0…100) | The tail. 50 = untouched, lower = tighter/shorter, higher = fatter/longer. |
| **MODE** | Detector character. TIGHT = fastest, short tails · PUNCH = medium, extra low-end snap · FAT = slower, long tails · CLEAN = smoothest gain movement. |
| **SHAPE** | How the attack gain responds: Soft / Smooth / Hard (snappy, gate-like) / Exp (only big hits). |
| **FOCUS** (50–800 Hz) | Where the PUNCH band ends. |
| **DRIVE** | Soft-clip drive for density/slam (2x oversampled). |
| **SATURATION** | Warm, even-harmonic color (2x oversampled). |
| **TONE LOW / HIGH** | ±12 dB shelves at 120 Hz / 7 kHz. |
| **WIDTH** | 0 = mono, 50 = original, 100 = 2x wider. |
| **MIX** | Parallel blend; the dry path is latency-aligned, so no phasing. |
| **OUTPUT** | -∞ … +6 dB. |

Knobs: drag up/down, **shift-drag** for fine, **double-click** to reset, scroll wheel works.

## Presets
- **INIT** + **20 factory presets** (all level-matched to about the same peak level).
- **SAVE**: name it. Use an existing user preset name to overwrite it (asks first). Saving over a factory preset creates your own copy.
- **< / >** step through everything, click the name for the full list.
- **☰ menu**: save as new, overwrite, delete, load a `.pmxx` file, open the presets folder, window size.
- 1st folder button = load a preset file from anywhere · 2nd folder button = open your presets folder.
- User presets live in:
  - macOS: `~/Library/Application Support/barnacle boi/PUNCHMAXXX/Presets`
  - Windows: `%APPDATA%\barnacle boi\PUNCHMAXXX\Presets`

## Project recall
Everything (all knobs, mode, shape, which preset is loaded, whether you tweaked it, window size) is saved inside the DAW project and comes back exactly when the project reopens.

## Build

Push to GitHub → **Actions** builds macOS (VST3 + AU + app, universal) and Windows (VST3 + exe) and runs the test suite. Grab the zips from the run's artifacts.

Local:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE 8.0.4 is downloaded automatically (or drop a JUCE checkout in `./JUCE`).

Install: copy `PUNCHMAXXX.vst3` → `~/Library/Audio/Plug-Ins/VST3` (mac) or `C:\Program Files\Common Files\VST3` (win); `PUNCHMAXXX.component` → `~/Library/Audio/Plug-Ins/Components`.
The mac CI build is ad-hoc signed, not notarized — if macOS blocks it: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/PUNCHMAXXX.vst3` (same for the .component).

## Tests
`PunchmaxxxTests` checks: project save/reopen restores every parameter + preset name + modified flag, preset save/overwrite/delete/navigation, neutral settings are transparent (-56 dB null), each knob does what it says, MIX 0 is perfectly dry, all 20 presets are clean and level-matched, mono works, and it renders the UI to PNG.

## Art
`assets/background.png` is generated from `assets/ui_concept.png` by `python3 tools/prep_background.py assets/ui_concept.png assets/background.png` (it erases the baked-in knob pointers/arcs, the FOCUS readout and the preset name so the plugin can draw them live).
