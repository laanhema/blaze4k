# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Project Overview

**Blaze 4k** is a 4-panel arrow rhythm game for desktop (Windows, macOS, Linux) — a faithful homage to *In The Groove*, *Mungyodance*, and *StepMania*. Players hit scrolling arrows in time with music via keyboard or USB dance pad, judged with ITG-tight timing windows and scored with the OpenITG dance-point/grade system. Fully offline; reads SM/SSC simfiles; ships engine-only (users import their own song packs).

**Status: pre-implementation.** The repo currently contains only the PRD (`.agents/PRDs/PRD.md`). All structure/commands below describe the planned layout from the PRD — create files accordingly.

**Core principles (non-negotiable):**

1. **Timing is sacred** — gameplay is driven by the music clock (audio stream position), never wall-clock or frame delta. Input events are timestamped at nanosecond precision.
2. **Faithful, not novel** — mechanics mirror ITG/OpenITG reference behavior. Check OpenITG/StepMania source before guessing semantics.
3. **Lean scope** — 4-panel single-song play done perfectly. The PRD's out-of-scope list is a contract.
4. **Offline forever** — no network, no accounts. All state is local JSON files.
5. **Respect the ecosystem** — SM/SSC compatibility; existing packs and tools must just work.

---

## Tech Stack

| Technology     | Purpose                                                          |
| -------------- | ---------------------------------------------------------------- |
| C++20          | Language (MSVC / Clang / GCC)                                    |
| SDL3 (≥ 3.2)   | Window, GL context, input events (nanosecond timestamps), hotplug |
| OpenGL 3.3     | Rendering via glad; 2D textured quads only                       |
| miniaudio      | Audio playback + stream position (single-header)                 |
| stb_truetype   | Texture-atlased UI text                                          |
| nlohmann/json  | Config + high-score persistence                                  |
| CMake (≥ 3.24) | Build; FetchContent for dependencies                             |

Reference authority: **OpenITG source** (judgment windows, DP weights, grades, life behavior) and **StepMania 5 source** (SM/SSC parsing semantics).

---

## Commands

Planned CMake workflow (no build files exist yet):

```bash
# Configure & build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Debug build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Test (once tests/ exists)
ctest --test-dir build --output-on-failure
```

---

## Architecture

Planned layout (from PRD §6):

```
blaze-4k/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/           # main loop, frame pacing (fixed-timestep + vsync)
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

Screen flow: **Title → Attract → Select → Gameplay → Results** (state machine; attract is just another screen).

---

## Code Patterns

### Key design patterns (from PRD §6 — follow these)

1. **Music-driven clock.** Gameplay time = audio stream sample position ÷ sample rate + calibrated global offset. Never wall-clock, never frame delta. No frame-timing logic in the judgment path.
2. **Event-sourced judgments.** Every hit produces an immutable judgment event `{column, note_time, hit_time, delta_ms, window}`. Scoring, combo, life, and results all derive from the event log.
3. **Data-driven constants.** Timing windows, DP weights, grade boundaries, and life deltas live in a config table (JSON-loaded), seeded with values verified against OpenITG source.
4. **Screen state machine.** Screens are objects with `enter/update/render/exit`; transitions are explicit.
5. **Thin platform wrapper.** SDL3 is isolated behind input/window interfaces, keeping the core portable and testable.

### Error handling

- Simfiles are **untrusted input**: the parser must be bounds-checked, allocation-capped, and fuzz-tested. A corrupt `.sm` must never crash the game.
- Reject unsupported constructs (non-4-panel charts, exotic timing) gracefully with a readable log line — never guess semantics; check StepMania source.

---

## Testing

- **Test location**: `tests/` (parser tests, timing unit tests, metronome sync-test chart)
- **Fixtures**: reference simfile pack (provided by product owner) — parser must handle 100% of its 4-panel songs with zero crashes
- **Regression tool**: permanent metronome sync-test chart to verify gameplay sync holds within one judgment window over a full song

---

## Key Files

| File                   | Purpose                                                      |
| ---------------------- | ------------------------------------------------------------ |
| `.agents/PRDs/PRD.md`  | Product Requirements Document — full spec, scope, locked decisions |

---

## Notes

- **Scope contract**: out of scope for v1 — chart editor, courses/marathon, anything online, 5/6-panel or doubles, turn/appearance mods, video/3D backgrounds, dancers, practice mode, bundled soundtrack.
- Mods in scope: C-mod, X-mod, M-mod speed mods + scroll direction only.
- Note elements: taps, holds, rolls, mines.
- Difficulty display is passthrough from the simfile (labels + foot rating).
- Solo hobby project — no CI for v1; local cross-platform CMake builds must work from a fresh clone.
