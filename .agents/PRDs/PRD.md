# Product Requirements Document: Blaze 4k

**Version:** 1.0
**Date:** 2026-09-26
**Status:** Draft — ready for implementation planning
**Product type:** Desktop rhythm game (solo hobby project)

---

## 1. Executive Summary

**Blaze 4k** is a modern 4-panel arrow rhythm game for desktop (Windows, macOS, Linux), built as a faithful homage to *In The Groove*, *Mungyodance*, and *StepMania*. Players hit scrolling arrows in time with music using a keyboard or USB dance pad, judged against tight, ITG-style timing windows, and scored with the OpenITG dance-point/grade system.

The game is built on a custom C++20 engine (SDL3 + OpenGL + miniaudio) designed around one non-negotiable principle: **timing precision**. Gameplay is driven by a music-synchronized clock rather than frame timing, and input events are timestamped at the nanosecond level. The game is fully offline, reads the de-facto standard SM/SSC simfile formats (instantly compatible with decades of community charts), and ships engine-only — users import their own song packs.

**MVP goal:** A complete arcade loop — Title → Attract → Song Select → Gameplay → Results — with accurate ITG-style judgment, scoring, life bar, and persistent local high scores, verified playable on both keyboard and USB dance pad using a real simfile pack.

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
- [ ] 4-panel gameplay with taps, holds, rolls, and mines
- [ ] ITG-style judgment (Judge-4-tight windows, configurable constants)
- [ ] OpenITG-equivalent scoring: Dance Points, percentage, ★ grades, combo, life bar with fail
- [ ] Speed modifiers: C-mod, X-mod, M-mod; scroll direction option
- [ ] Full arcade screen flow: Title → Attract/demo → Song Select → Gameplay → Results
- [ ] Song Select wheel with banner art, difficulty (passthrough labels + foot rating), and audio preview
- [ ] Local per-chart high score persistence

**Technical**
- [ ] SM and SSC parser (4-panel charts; BPM changes, stops)
- [ ] Song library scanner for user-imported packs (folder-per-song structure)
- [ ] Music-driven gameplay clock (audio stream position, not wall time)
- [ ] Timestamped low-latency input (SDL3 events) for keyboard + USB dance pad, with remapping
- [ ] Global offset calibration wizard
- [ ] 2D rendering: note field, receptors, judgment sprites, simfile background images (dimmed), UI
- [ ] Settings/profile persistence as local JSON

**Integration**
- [ ] Reads community SM/SSC packs unmodified
- [ ] Displays simfile banner/background artwork
- [ ] Reference validation against a real simfile pack (provided by product owner)

**Deployment**
- [ ] CMake build producing native binaries on Windows, macOS, Linux
- [ ] No installer required; portable folder layout

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
   - *Example:* Point the game at `Songs/` containing an ITG-style pack; every song with a 4-panel chart appears on the wheel with banner, artist, and foot rating.

2. **As a competitive player, I want ITG-tight judgment windows and OpenITG-faithful DP scoring, so that my scores mean the same thing they do on real hardware.**
   - *Example:* A Fantastic counts +2 DP, a Decent breaks combo, and 99%+ earns ★★★★ on the results screen.

3. **As a player, I want C-mod, X-mod, and M-mod speed options, so that I can read charts at my preferred scroll speed regardless of BPM changes.**
   - *Example:* Selecting C400 keeps scroll speed constant through a song's tempo shifts.

4. **As a pad player, I want a global offset calibration wizard, so that my hits register on-time despite my pad/OS/audio latency.**
   - *Example:* A guided tap-to-the-beat tool computes and saves the global audio offset to config.

5. **As a player, I want the full arcade flow with attract mode, so that the game feels like a cabinet in my living room.**
   - *Example:* Booting shows a title screen; idling plays a demo loop; pressing Start enters the song wheel.

6. **As a player, I want my best score per chart saved locally, so that I can chase my own records over time.**
   - *Example:* After a new personal best, the results screen flags "NEW RECORD" and the wheel shows the grade next to that difficulty.

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

### Proposed directory structure

```
blaze-4k/
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
└── docs/              # PLAN.md, this PRD
```

### Key design patterns

1. **Music-driven clock.** Gameplay time = audio stream sample position ÷ sample rate, plus calibrated global offset. Never wall-clock, never frame delta. This is the correctness backbone.
2. **Event-sourced judgments.** Every hit produces an immutable judgment event (column, note time, hit time, delta, window). Scoring, combo, life, and results all derive from the event log — enabling replays/stats later at zero extra cost.
3. **Data-driven constants.** Timing windows, DP weights, grade boundaries, and life deltas live in a config table (loaded from JSON), seeded with values verified against OpenITG source.
4. **Screen state machine.** Screens are objects with `enter/update/render/exit`; transitions are explicit; attract mode is just another screen.
5. **Thin platform wrapper.** SDL3 is isolated behind input/window interfaces, keeping the core portable and testable.

---

## 7. Tools/Features

### 7.1 Simfile engine (SM/SSC parser + scanner)
- Parses `.sm` and `.ssc` headers (title, artist, banner, background, music, offset, BPMs, stops) and 4-panel note data (taps, holds, rolls, mines)
- Library scanner: recursive folder scan → song/pack grouping → metadata + art resolution
- Graceful rejection of unsupported constructs (non-4-panel charts, exotic timing) with a log entry, never a crash

### 7.2 Gameplay core
- Receptor row + scrolling note field, upscroll default (downscroll option)
- Judgment evaluation per §constants; hold head + tail handling; roll re-hit logic; mine penalty
- Combo, life bar (start full; drain on Decent/Way Off/Miss/mine; refill on Great+; fail at empty, Fail-Off option)
- Live HUD: score %, combo, judgment counts, life

### 7.3 Screen flow
- **Title:** logo, "Press Start"
- **Attract:** idle demo (gameplay autoplay or title loop)
- **Select:** song wheel (banner, artist, BPM), difficulty list with passthrough labels + foot rating, per-chart best grade display, audio preview, options menu (speed mod, scroll, offset wizard)
- **Gameplay:** §7.2
- **Results:** grade, %, DP, judgment breakdown, max combo, "NEW RECORD" flag

### 7.4 Input
- Keyboard (default arrows + enter/esc) and SDL3 gamepad/pad support with full remapping
- SDL3 nanosecond event timestamps captured at poll time for hit-delta accuracy

### 7.5 Calibration wizard
- Guided tap-along computes average hit delta → writes global offset to config

### 7.6 Persistence
- `config.json`: video/audio/input/offset/gameplay options
- `scores.json`: per-chart best {grade, %, DP, timestamp}

---

## 8. Technology Stack

| Component | Choice | Version/Notes |
|---|---|---|
| Language | C++ | C++20 |
| Platform layer | **SDL3** | ≥ 3.2 — window, GL context, input events (nanosecond timestamps), pad hotplug |
| Rendering | OpenGL | 3.3 core via glad; 2D textured quads only |
| Audio | miniaudio | Latest single-header; playback, stream position, preview |
| Fonts | stb_truetype | Texture-atlased UI text |
| Data | nlohmann/json | Config + scores |
| Build | CMake | ≥ 3.24; FetchContent for deps |
| Compilers | MSVC / Clang / GCC | CI-free local builds for v1 |
| Reference | OpenITG source | Authority for windows, DP weights, grades, life behavior |

**Rationale:** C++20 gives deterministic real-time performance and direct access to the StepMania-lineage reference code. SDL3 was chosen over SDL2 for timestamped input events and longevity; the thin wrapper keeps switching cost low. miniaudio handles audio independently so SDL's audio model is irrelevant.

---

## 9. Security & Configuration

- **No authentication, no network access, no user accounts** — the entire security surface is local file parsing.
- **Configuration:** JSON files in a portable `data/` folder next to the binary (Windows/macOS) with an XDG-compliant path option on Linux.
- **Input hardening (parser):** simfiles are untrusted input — the parser must be bounds-checked, allocation-capped, and fuzz-tested against malformed files (a corrupt `.sm` must never crash the game).
- **Out of security scope:** anti-cheat (no online play), sandboxing, code signing.

---

## 10. API Specification

Not applicable — fully offline desktop application with no network API. Internal module interfaces (chart model ↔ gameplay ↔ scoring) are documented in code and `docs/PLAN.md`.

---

## 11. Success Criteria

**MVP is successful when the developer can play a full song on a USB dance pad and the experience is indistinguishable in feel from OpenITG.**

- [ ] Reference simfile pack scans with 100% of its 4-panel songs playable; zero parser crashes
- [ ] Gameplay sync holds within one judgment window over a full-length song (verified with a metronome sync-test chart)
- [ ] Hit deltas on calibrated pad hardware register within expected spread (no systematic early/late bias)
- [ ] Stable 60 fps (or vsync rate) gameplay on a mid-range machine
- [ ] Complete arcade loop: boot → attract → select → play → results → back to wheel, with no dead ends
- [ ] High scores, settings, and offset survive restart
- [ ] Clean CMake build from a fresh clone on at least one OS

**Quality indicators:** judgment event log matches visual feedback exactly; every unsupported simfile is rejected with a readable log line; no frame-timing logic in the judgment path.

---

## 12. Implementation Phases

### Phase A — Foundation (skeleton + data)
**Goal:** Bootable app that understands a simfile library.
- [ ] CMake + SDL3 window + GL context + main loop
- [ ] miniaudio playback with stream-position query
- [ ] Keyboard + pad input logging with event timestamps
- [ ] SM/SSC parser + library scanner (console verification)
- [ ] Parser unit tests against the reference pack
**Validation:** parses the entire reference pack error-free; input/audio latency measurable via log output.

### Phase B — Gameplay core
**Goal:** Play a chart start-to-finish with correct sync and scoring.
- [ ] Music-driven clock + offset application
- [ ] Note field rendering, receptors, scrolling (X/C/M mods)
- [ ] Judgment engine, event log, combo, life bar, DP/% scoring
- [ ] Fail handling + metronome sync-test chart
**Validation:** a full song completes with stable sync; scoring matches OpenITG reference values on a known chart.

### Phase C — Arcade shell (screens + persistence)
**Goal:** Complete, navigable arcade loop with saved state.
- [ ] Title/attract/select/results screens
- [ ] Song wheel: banners, difficulty passthrough, audio preview
- [ ] Options menu + offset calibration wizard + input remapping
- [ ] JSON persistence: config + per-chart high scores
**Validation:** full loop keyboard- and pad-navigable; records and settings survive restart.

### Phase D — Polish (visual + feel)
**Goal:** Ships-feeling v1.
- [ ] Background image rendering (dimmed), fallback background
- [ ] Noteskin visuals, judgment/combo pop animations, UI sounds
- [ ] Grade animations on results; "NEW RECORD" flow
- [ ] Cross-platform builds verified; performance pass
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

---

## 14. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **Audio sync drift** | Game feels wrong; core promise broken | Music-clock architecture from day one; permanent metronome sync-test chart as a regression tool |
| **Pad input latency varies by OS/hardware** | Systematic early/late hits | SDL3 event timestamps; per-frame pre-render polling; offset calibration wizard; documented per-OS audio backend guidance |
| **SM/SSC edge cases** (split timing, warps, negative BPMs, encoding quirks) | Misparsed charts, broken sync | Test against real reference pack; reject unsupported constructs gracefully; never guess semantics — check StepMania source |
| **Scope creep** (solo dev, no deadline) | v1 never ships | Out-of-scope list is a contract; editor and modes only after v1 ships |
| **Solo burnout / long tail polish** | Project stalls in Phase D | Phases A–C each produce a playable artifact; Phase D polish items are independently shippable |

---

## 15. Appendix

### Reference materials
- **OpenITG source** — authority for judgment windows, DP weights, life behavior, grade thresholds
- **StepMania 5 source** — SM/SSC parsing semantics and edge cases
- **Reference simfile pack** — provided by product owner; fixture for parser tests and Phase B/D validation

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
| Modes | Single song |
| Difficulty display | Passthrough from simfile |
| Profiles | Simple local high scores |
| Distribution | Undecided (deferred) |

### Open items (non-blocking)
- Final grade-boundary/DP table — lift from OpenITG source during Phase B
- Optional free starter pack for first-run UX
- Game logo / visual identity
