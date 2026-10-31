# PUNCHMAXXX — Drum Dynamics Engine
**TRIPLE 7 AUDIO** · JUCE 8 · VST3 / AU / Standalone

## Signal flow
`IN → [oversample up] → COMPRESSOR → TRANSIENT → MIX (parallel blend) → [oversample down] → MAXIMIZER → OUTPUT`

- **Compressor** — feed-forward, stereo-linked, 6 dB soft knee, gentle auto make-up (~40% of the gain reduction).
- **Transient** — level-independent differential-envelope shaper. ATTACK ±100% = more/less snap, SUSTAIN ±100% = more/less body/tail. Gated so it never pumps up the noise floor.
- **Mix** — parallel (NY-style) blend of the processed vs. dry signal. Dry is sample-aligned, so no phasing.
- **Maximizer** — DRIVE (0–100% = 0 to +18 dB) into a 1.5 ms lookahead, true-peak-aware brickwall limiter. CEILING is never exceeded.
- **Oversample** — OFF / 2X / 4X, linear-phase FIR.

### Clean by design
- Every stage is gain-only (no EQ / filters in the path) → zero phase shift.
- Latency is constant in every oversample mode and reported to the host → no phasing against other tracks, no re-sync when you switch modes.
- All knobs and power buttons are smoothed (30 ms) → no zipper noise or clicks.

## Controls
- Drag knobs up/down · **Shift-drag** = fine · **Double-click** = reset to default
- Power icons bypass each section (smooth crossfade)
- Preset bar: **< >** step through presets · click the **name** for the full list
- 💾 **Save** (overwrites current user preset, or asks for a name on factory presets)
- ⧉ **Save As** (new user preset)
- ⚙ **Settings**: New (INIT), Save As, Open preset file, Export preset file, Delete, Show preset folder
- A `*` after the preset name = edited since load
- The editor is resizable (keeps aspect ratio)

## Presets
INIT + 21 factory presets (Kick Thump, Snare Crack, NY Parallel Smash, Dubstep Slam, Riddim Knock, Into The Void, …).

User presets are `.pmx` files in:
- **Windows:** `%APPDATA%\TRIPLE 7 AUDIO\PUNCHMAXXX\Presets`
- **macOS:** `~/Library/Application Support/TRIPLE 7 AUDIO/PUNCHMAXXX/Presets`

Every knob, power state, oversample setting **and the preset name** are saved with your DAW project.

## Build
```bash
# JUCE is picked up from ./JUCE or ../JUCE, otherwise downloaded automatically (8.0.9)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Output: `build/PUNCHMAXXX_artefacts/Release/VST3/PUNCHMAXXX.vst3` (+ AU on macOS).

Windows: install Visual Studio 2022 (Desktop C++) + CMake. macOS: Xcode command line tools + CMake (`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal build).

**GitHub Actions:** push this folder to a repo and `.github/workflows/build.yml` builds the Windows VST3 and the macOS VST3/AU automatically (download them from the run's Artifacts).

Install: copy `PUNCHMAXXX.vst3` to `C:\Program Files\Common Files\VST3` (Win) or `~/Library/Audio/Plug-Ins/VST3` (Mac); `.component` → `~/Library/Audio/Plug-Ins/Components`.

## Tests
```bash
cmake -B build -DPMX_BUILD_TESTS=ON && cmake --build build --target PunchmaxxxTests
./build/PunchmaxxxTests_artefacts/Release/PunchmaxxxTests ui.png
```
Checks: bypass null test in all oversample modes, ceiling on every preset, transient punch, project recall, user preset save/load/delete, and renders the UI.
