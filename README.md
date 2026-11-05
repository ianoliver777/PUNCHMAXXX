# PUNCHMAXXX — Transient Drum Processor

by **TRIPLE SEVEN MAFIA** · [triplesevenaudio.com](https://triplesevenaudio.com) · v1.1.0 · JUCE 8 · VST3 / AU / Standalone (macOS universal, Windows)

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
  - macOS: `~/Library/Application Support/TRIPLE SEVEN MAFIA/PUNCHMAXXX/Presets`
  - Windows: `%APPDATA%\TRIPLE SEVEN MAFIA\PUNCHMAXXX\Presets`

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

## Install
Each zip ships with `INSTALL.txt` and a one-click installer:
- **macOS** — right-click `Install PUNCHMAXXX.command` → Open. Copies the VST3 + AU to `~/Library/Audio/Plug-Ins`, the app to `/Applications`, and clears the quarantine flag (the CI build is ad-hoc signed, not notarized).
- **Windows** — extract the zip, double-click `Install PUNCHMAXXX.bat`, accept the admin prompt. Copies the VST3 to `C:\Program Files\Common Files\VST3`.

Manual install and troubleshooting are in `install/macOS/INSTALL.txt` and `install/Windows/INSTALL.txt`.

## Changelog
- **1.1.0** — new TRIPLE SEVEN skin, author is now **TRIPLE SEVEN MAFIA**, logo links to triplesevenaudio.com, one-click installers in the release zips. Plugin IDs are unchanged, so old projects still load it; presets from the old `TRIPLE 7 AUDIO` folder are copied over automatically.
- **1.0.0** — first release.

## Tests
`PunchmaxxxTests` checks: project save/reopen restores every parameter + preset name + modified flag, preset save/overwrite/delete/navigation, neutral settings are transparent (-56 dB null), each knob does what it says, MIX 0 is perfectly dry, all 20 presets are clean and level-matched, mono works, and it renders the UI to PNG.

## Art
`assets/background.png` is generated from `assets/ui_concept.png` by `python3 tools/prep_background.py assets/ui_concept.png assets/background.png` (it erases the baked-in knob pointers/arcs, the FOCUS readout and the preset name so the plugin can draw them live).
