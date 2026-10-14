# Product Requirements Document: Blaze 4k

**Version:** 1.0
**Date:** 2026-09-26
**Last Updated:** 2026-10-14
**Status:** v1 implemented — Phases A–D are delivered on Linux; the Windows/macOS builds, the GPU frame-rate check, and the pad-hardware feel test are owner steps that are not recorded yet
**Product type:** Desktop rhythm game (solo hobby project)

---

## 1. Executive Summary

**Blaze 4k** is a modern 4-panel arrow rhythm game for desktop (Windows, macOS, Linux), built as a faithful homage to *In The Groove*, *Mungyodance*, and *StepMania*. Players hit scrolling arrows in time with music using a keyboard or USB dance pad, judged against tight, ITG-style timing windows, and scored with the OpenITG dance-point/grade system.

The game is built on a custom C++20 engine (SDL3 + OpenGL + miniaudio) designed around one non-negotiable principle: **timing precision**. Gameplay is driven by a music-synchronized clock rather than frame timing, and input events are timestamped at the nanosecond level. The game is fully offline, reads the de-facto standard SM/SSC simfile formats (instantly compatible with decades of community charts), and ships engine-only — users import their own song packs.

**MVP goal:** A complete arcade loop — Title → Attract → Song Select → Gameplay → Results — with accurate ITG-style judgment, scoring, life bar, and persistent local high scores, verified playable on both keyboard and USB dance pad using a real simfile pack. The loop is built and machine-checked on Linux against the reference pack; the pad-hardware check is an owner step with no recorded result ([cross-platform verification](../../docs/CROSS_PLATFORM_VERIFICATION.md)).

**Core value proposition:** The authentic ITG arcade feel — tight windows, honest scoring, speed mods, classic aesthetics — in a clean, modern, dependency-free desktop build with zero online requirements.

---

## 2. Mission

Deliver the definitive offline ITG-style experience: mechanically faithful, timing-accurate, and free of modern distractions (no accounts, no servers, no video backgrounds, no dancers).

**Core principles:**

1. **Timing is sacred.** Every architectural decision favors input/audio precision over convenience.
2. **Faithful, not novel.** Mechanics mirror ITG/OpenITG reference behavior; no gimmicks or reinvention.
3. **Lean scope.** 4-panel single-song play done perfectly beats ten half-finished modes.
4. **Offline forever.** All state is local files; the game works identically with no network.
5. **Respect the ecosystem.** SM/SSC compatibility means existing packs and tools (ArrowVortex, StepMania) just work.

---

## 3. Target Users

### Primary persona: The ITG veteran
- Plays StepMania/Etterna/OpenITG at home on a USB dance pad or keyboard
- Expects C-mod/X-mod support, Judge-4-tight windows, and honest DP scoring
- Owns large simfile pack libraries; wants to drop them in and play
- **Pain point:** existing engines are aging (StepMania 5), carry baggage (theme hacks), or focus on keyboard-only online play (Etterna)

### Secondary persona: The arcade nostalgist
- Grew up on DDR/ITG cabinets; wants the classic arcade flow and look at home
- **Pain point:** modern rhythm games bury the core loop under accounts, unlocks, and online systems

### Developer context
- Solo developer, experienced in C/C++, hobby pace, no deadline
- The developer is the first target user: the game must feel right on real pad hardware

---

## 4. MVP Scope

### In Scope

**Core Functionality**
- [x] 4-panel gameplay with taps, holds, rolls, and mines
- [x] ITG-style judgment (Judge-4-tight windows, configurable constants)
- [x] OpenITG-equivalent scoring: Dance Points, percentage, ★ grades, combo, life bar with fail
- [x] Speed modifiers: C-mod, X-mod, M-mod; scroll direction option
- [x] Full arcade screen flow: Title → Attract/demo → Song Select → Gameplay → Results
- [x] Song Select wheel with banner art, difficulty (passthrough labels + foot rating), and audio preview
- [x] Local per-chart high score persistence

**Technical**
- [x] SM and SSC parser (4-panel charts; BPM changes, stops)
- [x] Song library scanner for user-imported packs (folder-per-song structure)
- [x] Music-driven gameplay clock (audio stream position, not wall time)
- [x] Timestamped low-latency input (SDL3 events) for keyboard + USB dance pad, with remapping
- [x] Global offset calibration wizard
- [x] 2D rendering: note field, receptors, judgment sprites, simfile background images (dimmed), UI
- [x] Settings/profile persistence as local JSON

**Integration**
- [x] Reads community SM/SSC packs unmodified
- [x] Displays simfile banner/background artwork
- [x] Reference validation against a real simfile pack (provided by product owner)

**Deployment**
- [ ] CMake build producing native binaries on Windows, macOS, Linux — Linux is verified (`scripts/fresh-clone-check.sh`); the Windows and macOS presets exist and are not verified yet
- [x] No installer required; portable folder layout

### Out of Scope (deferred)

- [ ] Chart editor (later phase; ArrowVortex/StepMania serve this need meanwhile)
- [ ] Courses / marathon / nonstop modes
- [ ] Online anything: leaderboards, profiles, multiplayer
- [ ] 5-panel / 6-panel / doubles layouts
- [ ] Turn mods (mirror/shuffle) and appearance mods (sudden/hidden)
- [ ] Video backgrounds, 3D backgrounds, character dancers
- [ ] Practice mode (slowdown, section looping)
- [ ] Bundled/original soundtrack
- [ ] Distribution decision (free/open-source/paid)

---

## 5. User Stories

1. **As a pad player, I want to import my existing simfile pack folders, so that I can play my library without conversion.**
   - *Example:* Point the game at `songs/` containing an ITG-style pack; every song with a 4-panel chart appears on the wheel with banner, artist, and foot rating.

2. **As a competitive player, I want ITG-tight judgment windows and OpenITG-faithful DP scoring, so that my scores mean the same thing they do on real hardware.**
   - *Example:* A Fantastic counts +5 DP, a Decent breaks combo, and 99%+ earns ★★★ (100% earns ★★★★) on the results screen.

3. **As a player, I want C-mod, X-mod, and M-mod speed options, so that I can read charts at my preferred scroll speed regardless of BPM changes.**
   - *Example:* Selecting C400 keeps scroll speed constant through a song's tempo shifts.

4. **As a pad player, I want a global offset calibration wizard, so that my hits register on-time despite my pad/OS/audio latency.**
   - *Example:* A guided tap-to-the-beat tool computes and saves the global audio offset to config.

5. **As a player, I want the full arcade flow with attract mode, so that the game feels like a cabinet in my living room.**
   - *Example:* Booting shows a title screen; idling plays a demo loop; pressing Start enters the song wheel.

6. **As a player, I want my best score per chart saved locally, so that I can chase my own records over time.**
   - *Example:* After a new personal best, the results screen flags "NEW RECORD" and the wheel shows the best percentage next to that difficulty.

7. **As a player, I want the simfile's background image shown (dimmed) during gameplay, so that songs have visual identity without distracting from the note field.**
   - *Example:* The SM file's `background` image renders behind the receptors at reduced brightness; no video, no 3D.

8. **As the developer, I want every judgment emitted as a timestamped event, so that replays, stats, and a future editor can be built on the same data.**
   - *Example:* Each hit records `{column, note_time, hit_time, delta_ms, window}` to an in-memory session log.

---

## 6. Core Architecture & Patterns

### High-level architecture

```
┌─────────────────────────────────────────────┐
│ App (main loop: fixed-timestep + vsync)      │
├─────────────────────────────────────────────┤
│ ScreenManager (state machine)                │
│  Title → Attract → Select → Game → Results   │
├──────────┬──────────┬──────────┬────────────┤
│ Timing   │ Input    │ Chart    │ Audio      │
│ Engine   │ Layer    │ Engine   │ Engine     │
│ music    │ SDL3 kb/ │ SM/SSC   │ miniaudio  │
│ clock +  │ pad,     │ parser → │ stream,    │
│ offset   │ remapped │ note     │ select-    │
│          │          │ data     │ screen     │
│          │          │ model    │ preview    │
├──────────┴──────────┴──────────┴────────────┤
│ Renderer (OpenGL 3.3, 2D textured quads:     │
│ notes, receptors, judgments, bg, UI)         │
├─────────────────────────────────────────────┤
│ Persistence (JSON: config, high scores)      │
└─────────────────────────────────────────────┘
```

### Directory structure

```
blaze4k/
├── CMakeLists.txt
├── CMakePresets.json  # per-platform configure/build/test presets
├── src/
│   ├── main.cpp       # command-line options, startup wiring
│   ├── app/           # main loop, frame pacing, window + GL context
│   ├── screens/       # title, attract, select, game, results, calibration, input remap, options overlay
│   ├── timing/        # music clock, clock interpolation, judgment constants, offset calibration
│   ├── input/         # SDL3 devices, mapping, event timestamps
│   ├── chart/         # SM/SSC parser, note model, timing data, library scanner
│   ├── audio/         # miniaudio wrapper, music stream, preview, UI sounds, assist tick, metronome
│   ├── render/        # quads, textures, TrueType + bitmap fonts, theme, background
│   ├── gameplay/      # note field, noteskin, judgments, scoring, life, HUD, speed mods
│   ├── data/          # config, high scores, judgment-constant loader, data paths (JSON)
│   └── util/          # checked text -> number parsing
├── assets/            # noteskins, fonts, Cabinet theme textures, fallback bg, judgment constants
├── scripts/           # check (lint + build + tests), lint rules, fresh-clone check, symbol font build
├── tests/             # one executable per test, fixtures (reference pack, sync-test chart)
└── docs/              # BUILDING, AUDIO_LATENCY, CROSS_PLATFORM_VERIFICATION, cabinet-theme
```

### Key design patterns

1. **Music-driven clock.** Gameplay time = audio stream sample position ÷ sample rate, plus calibrated global offset. Never wall-clock, never frame delta. This is the correctness backbone. Between audio updates the position is interpolated with a monotonic clock: re-anchored at every audio update, never decreasing, and bounded to one device period ahead of the latest anchor (#71, #81; [audio latency notes](../../docs/AUDIO_LATENCY.md)).
2. **Event-sourced judgments.** Every hit produces an immutable judgment event (column, note time, hit time, delta, window). Scoring, combo, life, and results all derive from the event log — enabling replays/stats later at zero extra cost.
3. **Data-driven constants.** Timing windows, DP weights, grade boundaries, and life deltas live in a config table (loaded from JSON), seeded with values verified against OpenITG source.
4. **Screen state machine.** Screens are objects with `enter/update/render/exit`; transitions are explicit; attract mode is just another screen.
5. **Thin platform wrapper.** SDL3 is isolated behind input/window interfaces, keeping the core portable and testable. Outside them, SDL is called only for the executable's base path (`src/data/data_paths.cpp`), key and button names (`src/screens/input_remap_screen.cpp`), and the monotonic clock (`src/main.cpp`).

---

## 7. Tools/Features

### 7.1 Simfile engine (SM/SSC parser + scanner)
- Parses `.sm` and `.ssc` headers (title, artist, banner, background, music, offset, BPMs, stops) and 4-panel note data (taps, holds, rolls, mines)
- Library scanner: recursive folder scan → song/pack grouping → metadata + art resolution
- Graceful rejection of unsupported constructs (non-4-panel charts, exotic timing) with a log entry, never a crash: a non-4-panel steps type is skipped, a song with no valid 4-panel chart is left out of the library, and a negative stop (warp) is logged as a warning
- Limits: a simfile of at most 16 MB, 10,000 tags, 1,000 parameters per tag and 1 MB per parameter; a chart of at most 10,000 measures, 1,024 rows per measure and 100,000 notes. A chart over a limit is rejected with a log line

### 7.2 Gameplay core
- Receptor row + scrolling note field, upscroll default (downscroll option)
- Judgment evaluation per §constants; hold head + tail handling; roll re-hit logic; mine penalty
- Combo, life bar (starts at 50%; drains on Way Off/Miss/mine and on a dropped hold, a Decent leaves it unchanged; refills on Great+ and on a held hold; fail at empty, Fail-Off option)
- Live HUD: judgment text, combo, life bar, and a difficulty badge. The score % and the judgment counts are shown on Results only (owner decision, 2026-10-12)
- Assist tick option: a tick on every note row, for timing practice

### 7.3 Screen flow
- **Title:** logo, "Press Start"
- **Attract:** title loop after 120 s without input (`--attract-timeout`). Not implemented: gameplay autoplay
- **Select:** song wheel (banner, artist, BPM), difficulty list with passthrough labels + foot rating, per-chart best % display, audio preview, options menu (speed mod type and value, scroll, fail, assist tick, offset wizard, input remapping). The wheel shows a song's subtitle, and an Edit chart is listed under its own name
- **Gameplay:** §7.2
- **Results:** grade, %, DP, judgment breakdown, max combo, holds OK/NG and mines hit, "NEW RECORD" flag
- **Calibration** and **Input Remap:** screens opened from the options menu on Select

### 7.4 Input
- Keyboard (default arrows or D/F/J/K, Enter to confirm, Esc for back, Tab for options) and SDL3 gamepad/pad support (default d-pad or face buttons, Start, Back, shoulder buttons for options) with full remapping
- SDL3 nanosecond event timestamps captured at poll time for hit-delta accuracy

### 7.5 Calibration wizard
- Guided tap-along computes average hit delta → writes global offset to config. A measured delay outside the accepted range is flagged and cannot be saved

### 7.6 Persistence
- `config.json`: video/audio/input/offset/gameplay options
- `scores.json`: per-chart best {grade, %, DP, timestamp}, keyed by a content-based chart key (simfile name, title, artist, chart and its notes), so a record survives moving the pack folder

---

## 8. Technology Stack

| Component | Choice | Version/Notes |
|---|---|---|
| Language | C++ | C++20 |
| Platform layer | **SDL3** | ≥ 3.2 (pinned `release-3.2.8`) — window, GL context, input events (nanosecond timestamps), pad hotplug |
| Rendering | OpenGL | 3.3 core via glad (pinned `v0.1.36`); 2D textured quads only |
| Audio | miniaudio | Single-header, pinned `0.11.21`; playback, stream position, preview |
| Fonts | stb_truetype | Texture-atlased UI text; stb is pinned to one commit and also supplies stb_image for textures |
| Data | nlohmann/json | Pinned `v3.11.3`; config + scores |
| Build | CMake | ≥ 3.24; FetchContent for deps |
| Compilers | MSVC / Clang / GCC | Linux CI (scripts/check.sh + sanitizer); local cross-platform builds |
| Reference | OpenITG source | Authority for windows, DP weights, grades, life behavior |

**Rationale:** C++20 gives deterministic real-time performance and direct access to the StepMania-lineage reference code. SDL3 was chosen over SDL2 for timestamped input events and longevity; the thin wrapper keeps switching cost low. miniaudio handles audio independently so SDL's audio model is irrelevant.

---

## 9. Security & Configuration

- **No authentication, no network access, no user accounts** — the entire security surface is local file parsing.
- **Configuration:** JSON files in a portable `data/` folder next to the binary on every OS. On Linux, `--xdg` or `BLAZE4K_XDG=1` selects `${XDG_DATA_HOME:-~/.local/share}/blaze-4k` instead, and `--data-dir <path>` overrides both.
- **Input hardening (parser):** simfiles are untrusted input — the parser must be bounds-checked, allocation-capped, and fuzz-tested against malformed files (a corrupt `.sm` must never crash the game). The caps are listed in §7.1; `tests/parser_hardening_test.cpp` holds the fuzz mutations.
- **Out of security scope:** anti-cheat (no online play), sandboxing, code signing.

---

## 10. API Specification

Not applicable — fully offline desktop application with no network API. Internal module interfaces (chart model ↔ gameplay ↔ scoring) are documented in code.

---

## 11. Success Criteria

**MVP is successful when the developer can play a full song on a USB dance pad and the experience is indistinguishable in feel from OpenITG.**

Ticks as of 2026-10-14 rest on the code and its test suite alone: the product was not run for this update.

- [x] Reference simfile pack scans with 100% of its 4-panel songs playable; zero parser crashes
- [x] Gameplay sync holds within one judgment window over a full-length song (verified with a metronome sync-test chart)
- [ ] Hit deltas on calibrated pad hardware register within expected spread (no systematic early/late bias) — not measured: owner step on pad hardware
- [ ] Stable 60 fps (or vsync rate) gameplay on a mid-range machine — not measured on a GPU: only the headless CPU-budget proxy (`perf_loop_test`) exists
- [x] Complete arcade loop: boot → attract → select → play → results → back to wheel, with no dead ends
- [x] High scores, settings, and offset survive restart
- [x] Clean CMake build from a fresh clone on at least one OS (Linux)

**Quality indicators:** judgment event log matches visual feedback exactly; every unsupported simfile is rejected with a readable log line; no frame-timing logic in the judgment path.

---

## 12. Implementation Phases

Phases A–D were delivered by issues #1–#26 (stories A1–D4) through pull requests #27–#52; the plans are under [`../plans/completed/`](../plans/completed/).

### Phase A — Foundation (skeleton + data)
**Goal:** Bootable app that understands a simfile library.
- [x] CMake + SDL3 window + GL context + main loop
- [x] miniaudio playback with stream-position query
- [x] Keyboard + pad input logging with event timestamps
- [x] SM/SSC parser + library scanner (console verification)
- [x] Parser unit tests against the reference pack
**Validation:** parses the entire reference pack error-free; input/audio latency measurable via log output.

### Phase B — Gameplay core
**Goal:** Play a chart start-to-finish with correct sync and scoring.
- [x] Music-driven clock + offset application
- [x] Note field rendering, receptors, scrolling (X/C/M mods)
- [x] Judgment engine, event log, combo, life bar, DP/% scoring
- [x] Fail handling + metronome sync-test chart
**Validation:** a full song completes with stable sync; scoring matches OpenITG reference values on a known chart.

### Phase C — Arcade shell (screens + persistence)
**Goal:** Complete, navigable arcade loop with saved state.
- [x] Title/attract/select/results screens
- [x] Song wheel: banners, difficulty passthrough, audio preview
- [x] Options menu + offset calibration wizard + input remapping
- [x] JSON persistence: config + per-chart high scores
**Validation:** full loop keyboard- and pad-navigable; records and settings survive restart.

### Phase D — Polish (visual + feel)
**Goal:** Ships-feeling v1.
- [x] Background image rendering (dimmed), fallback background
- [x] Noteskin visuals, judgment/combo pop animations, UI sounds
- [x] Grade animations on results; "NEW RECORD" flow
- [ ] Cross-platform builds verified; performance pass — the Linux build is verified and a headless CPU-budget test (`perf_loop_test`) is in the suite; the Windows/macOS builds and the GPU frame-rate run are not verified yet
**Validation:** side-by-side feel test against OpenITG by the developer on pad hardware.

*(Timeline: no deadline, hobby pace. Rough order-of-magnitude at casual weekly sessions: A ~2–3 weeks, B ~4–6 weeks, C ~3–4 weeks, D open-ended.)*

---

## 13. Future Considerations

- **Chart editor** (the explicitly deferred big one; internal note model is already editor-shaped)
- Courses / marathon mode, practice mode (slowdown, section loop)
- Turn mods (mirror/shuffle), appearance mods
- 5/6-panel and doubles support (note model permitting)
- Local stats depth: timing histograms, session graphs
- Optional online leaderboard (would reverse the "offline forever" principle only by explicit decision)
- Distribution decision: itch.io / Steam / open-source release; possible bundled starter pack for first-run UX

**Specified and not built (deferred):**

- Gameplay autoplay as the Attract demo (§7.3)

---

## 14. Risks & Mitigations

| Risk | Impact | Mitigation | Status |
|---|---|---|---|
| **Audio sync drift** | Game feels wrong; core promise broken | Music-clock architecture from day one; permanent metronome sync-test chart as a regression tool | In place: `metronome_sync_test` runs in the test suite |
| **Pad input latency varies by OS/hardware** | Systematic early/late hits | SDL3 event timestamps; per-frame pre-render polling; offset calibration wizard; documented per-OS audio backend guidance | In place; the backend guidance is in `docs/BUILDING.md` and `docs/AUDIO_LATENCY.md`. The pad-hardware measurement is not recorded |
| **SM/SSC edge cases** (split timing, warps, negative BPMs, encoding quirks) | Misparsed charts, broken sync | Test against real reference pack; reject unsupported constructs gracefully; never guess semantics — check StepMania source | In place: reference-pack and parser-hardening tests |
| **Scope creep** (solo dev, no deadline) | v1 never ships | Out-of-scope list is a contract; editor and modes only after v1 ships | Held: nothing on the out-of-scope list is in the code |
| **Solo burnout / long tail polish** | Project stalls in Phase D | Phases A–C each produce a playable artifact; Phase D polish items are independently shippable | Phases A–D are delivered |

---

## 15. Appendix

### Reference materials
- **OpenITG source** — authority for judgment windows, DP weights, life behavior, grade thresholds
- **StepMania 5 source** — SM/SSC parsing semantics and edge cases
- **Reference simfile pack** — provided by product owner; fixture for parser tests and Phase B/D validation (`tests/fixtures/reference_pack`)
- **Delivery records** — [stories](../stories/blaze-4k-stories.md), [issues from the task list](../issues/todo-issues.md), [plans](../plans/completed/), [implementation reports](../reports/), [reviews](../reviews/)
- **Project docs** — [building](../../docs/BUILDING.md), [audio latency](../../docs/AUDIO_LATENCY.md), [cross-platform verification](../../docs/CROSS_PLATFORM_VERIFICATION.md)

### Locked decisions log
| Decision | Value |
|---|---|
| Name | Blaze 4k |
| Platform | Windows / macOS / Linux |
| Stack | C++20, SDL3, OpenGL 3.3, miniaudio, stb_truetype, nlohmann/json, CMake |
| Panels | 4 only (v1) |
| Elements | Taps, holds, rolls, mines |
| Judgment | ITG-tight (configurable constants) |
| Scoring | OpenITG-equivalent DP + % + ★ grades |
| Mods | C/X/M speed mods only |
| Input | Keyboard + USB dance pad, remappable |
| Charts | SM/SSC read; editor deferred |
| Music | Engine-only; user imports packs (reference pack provided) |
| Online | None |
| Visuals | Classic arcade look; 2D simfile backgrounds; no video/dancers |
| Flow | Title → Attract → Select → Gameplay → Results |
| Live HUD | Judgment text, combo, life bar, difficulty badge; no score % or judgment counts (decided 2026-10-12) |
| Modes | Single song |
| Difficulty display | Passthrough from simfile |
| Profiles | Simple local high scores |
| Distribution | Undecided (deferred) |

### Open items (non-blocking)
- Final grade-boundary/DP table — lift from OpenITG source during Phase B. Resolved: the table is `assets/data/judgment_constants.json`, taken from OpenITG's `metrics.ini`
- Optional free starter pack for first-run UX
- Game logo / visual identity. Resolved in part: the Cabinet theme supplies the logo and the UI art (`assets/theme/cabinet`); its polish and cross-platform check are open in #98
