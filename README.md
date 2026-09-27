# Tundra Dance

A modern 4-panel arrow rhythm game for desktop (Windows, macOS, Linux) — a faithful homage to _In The Groove_, _Mungyodance_, and _StepMania_. Hit scrolling arrows in time with music using a keyboard or USB dance pad, judged with ITG-tight timing windows and scored with the OpenITG dance-point/grade system.

**Status: v1 feature-complete; D4 cross-platform verification owner steps pending.** The full specification lives in the [PRD](.agents/PRDs/PRD.md). See [Building](docs/BUILDING.md) for per-OS prerequisites and presets, and [Cross-platform verification](docs/CROSS_PLATFORM_VERIFICATION.md) for the D4 verification record and owner checklist.

## Highlights

- **Timing is sacred** — gameplay is driven by the music clock (audio stream position), never wall-clock or frame delta. Input events are timestamped at nanosecond precision.
- **ITG-faithful mechanics** — Judge-4-tight windows, OpenITG Dance Points, percentage, ★ grades, combo, and life bar with fail.
- **SM/SSC compatible** — reads the de-facto standard simfile formats; drop in your existing song packs and play. No conversion, no lock-in.
- **Full arcade loop** — Title → Attract → Song Select → Gameplay → Results, with per-chart local high scores.
- **Speed mods** — C-mod, X-mod, M-mod, plus scroll direction.
- **Offline forever** — no accounts, no servers, no network. All state is local JSON files.
- **Keyboard + USB dance pad** — full remapping and a guided global offset calibration wizard.

## Tech Stack

| Component | Choice                              |
| --------- | ----------------------------------- |
| Language  | C++20                               |
| Platform  | SDL3 (≥ 3.2)                        |
| Rendering | OpenGL 3.3 core (2D textured quads) |
| Audio     | miniaudio                           |
| Fonts     | stb_truetype                        |
| Data      | nlohmann/json                       |
| Build     | CMake (≥ 3.24)                      |

Reference authority: **OpenITG source** (judgment windows, DP weights, grades, life behavior) and **StepMania 5 source** (SM/SSC parsing semantics).

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build produces a native, portable binary — no installer required. Dependencies are fetched via CMake FetchContent. Per-OS prerequisites, the `CMakePresets.json` preset matrix, and the fresh-clone check are documented in [docs/BUILDING.md](docs/BUILDING.md).

## Running

Tundra Dance ships engine-only: you import your own song packs. Point the game at a `Songs/` folder containing SM/SSC packs (folder-per-song structure); every song with a 4-panel chart appears on the song wheel with its banner, artist, and difficulty rating.

Settings, high scores, and the calibrated global offset persist as JSON files next to the binary.

## Note Elements & Gameplay

- Taps, holds, rolls, and mines
- Upscroll default (downscroll option)
- Live HUD: score %, combo, judgment counts, life bar
- Results screen: grade, %, DP, judgment breakdown, max combo, "NEW RECORD" flag

## Out of Scope (v1)

Chart editor, courses/marathon modes, anything online, 5/6-panel or doubles, turn/appearance mods, video/3D backgrounds, dancers, practice mode, and a bundled soundtrack. The out-of-scope list is a contract — 4-panel single-song play done perfectly.

## Project Layout

```
tundra-dance/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/           # main loop, frame pacing
│   ├── screens/       # title, attract, select, game, results
│   ├── timing/        # music clock, offset, judgment windows
│   ├── input/         # SDL3 devices, mapping, event timestamps
│   ├── chart/         # SM/SSC parser, note model, library scanner
│   ├── audio/         # miniaudio wrapper, preview playback
│   ├── render/        # GL context, quads, sprites, fonts
│   ├── gameplay/      # note field, judgments, scoring, life
│   └── data/          # config, high scores (JSON)
├── assets/            # noteskin, fonts, UI sounds, fallback bg
├── tests/             # parser tests, timing unit tests, sync chart
└── docs/              # PLAN.md, PRD
```

## Documentation

- [Product Requirements Document](.agents/PRDs/PRD.md) — full spec, scope, and locked decisions
- [Building](docs/BUILDING.md) — per-OS prerequisites, presets, testing, portable data layout
- [Cross-platform verification](docs/CROSS_PLATFORM_VERIFICATION.md) — D4 verification record + owner checklist
- [AGENTS.md](AGENTS.md) — contributor/agent guidance
