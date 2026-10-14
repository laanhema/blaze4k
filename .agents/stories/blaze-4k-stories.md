# Blaze 4k — User Stories

**Source:** `.agents/PRDs/PRD.md` (v1.0, 2026-09-26)
**Generated:** 2026-09-26
**Last Updated:** 2026-10-14
**Story count:** 26 (Phase A: 8, Phase B: 7, Phase C: 7, Phase D: 4)

Ordering: by PRD implementation phase (§12), then dependencies, then priority. Each story references its PRD section(s) for traceability.

---

## Status

All 26 stories are done: every issue is closed as completed and every pull request is merged into `main`. No story issue or pull request is open.

| Story | Issue | Pull request | Phase | Status | Acceptance criteria ticked |
| --- | --- | --- | --- | --- | --- |
| A1 CMake project scaffold with dependency fetching | #1 | #27 | A | done | 3 of 4 |
| A2 Application window, GL context, and main loop | #2 | #28 | A | done | 4 of 4 |
| A3 Audio engine: miniaudio playback with stream-position query | #3 | #29 | A | done | 3 of 4 |
| A4 Input layer: keyboard + pad with nanosecond event timestamps | #4 | #30 | A | done | 2 of 4 |
| A5 SM/SSC header and timing parsing | #5 | #31 | A | done | 4 of 5 |
| A6 4-panel note data parsing (taps, holds, rolls, mines) | #6 | #32 | A | done | 4 of 4 |
| A7 Song library scanner with pack grouping and art resolution | #7 | #33 | A | done | 4 of 4 |
| A8 Parser hardening: unit tests and fuzz against reference pack | #8 | #34 | A | done | 3 of 4 |
| B1 Music-driven gameplay clock with global offset | #9 | #35 | B | done | 4 of 4 |
| B2 Data-driven judgment and scoring constants from OpenITG | #10 | #36 | B | done | 3 of 4 |
| B3 Note field rendering with receptors, speed mods, and scroll direction | #11 | #37 | B | done | 4 of 5 |
| B4 Judgment engine with event-sourced judgment log | #12 | #38 | B | done | 4 of 5 |
| B5 Scoring: dance points, percentage, combo, and ★ grades | #13 | #39 | B | done | 2 of 5 |
| B6 Life bar with drain/refill and fail handling | #14 | #40 | B | done | 4 of 5 |
| B7 Metronome sync-test chart (permanent regression tool) | #15 | #41 | B | done | 3 of 3 |
| C1 Screen state machine with Title and Attract screens | #16 | #42 | C | done | 4 of 4 |
| C2 JSON persistence: config and per-chart high scores | #17 | #43 | C | done | 4 of 4 |
| C3 Song Select wheel with banners, difficulties, and audio preview | #18 | #44 | C | done | 5 of 5 |
| C4 Options menu: speed mod, scroll direction, and fail toggle | #19 | #45 | C | done | 4 of 4 |
| C5 Global offset calibration wizard | #20 | #46 | C | done | 4 of 4 |
| C6 Input remapping screen | #21 | #47 | C | done | 4 of 4 |
| C7 Results screen with grade, breakdown, and NEW RECORD flag | #22 | #48 | C | done | 4 of 4 |
| D1 Dimmed simfile background during gameplay with fallback | #23 | #49 | D | done | 3 of 4 |
| D2 Noteskin visuals, judgment/combo pop animations, and UI sounds | #24 | #50 | D | done | 4 of 4 |
| D3 Results polish: grade animations and NEW RECORD flow | #25 | #51 | D | done | 3 of 3 |
| D4 Cross-platform build verification and performance pass | #26 | #52 | D | done | 0 of 4 |

The criteria were checked on 2026-10-12 against the code at `d11de69` and one run of the test suite (51 of 51 passed, with the sound device hidden). The game window was not opened, and nothing was run on a dance pad, a GPU, Windows or macOS. A ticked box means that the code or that test run shows the criterion. An unticked box has a note that says what is not met, only partly verified, not checked, or dropped by an owner decision. An As Built section says where the result differs from the story's wording and which file holds it. The entries carry no GitHub line, so each one was matched to its issue by the ID in the issue title. The delivery records are the plans under `.agents/plans/completed/`, the reports under `.agents/reports/`, and the reviews under `.agents/reviews/`.

### Open Items Without a Story

**Specified in the source but not built**

- Gameplay autoplay as the Attract demo (PRD §7.3)

**Open GitHub issues that no story names**

- #60 [TODO-7] Add a pause menu during gameplay
- #69 Expose the active audio output device (backend, ID, name, buffer) and handle reroutes
- #70 Per-output-device global offset (remember calibration per headphones/speakers)
- #98 [TODO-26] Polish the Cabinet theme and verify it cross-platform

---

## Phase A — Foundation (skeleton + data)

## [A1] CMake project scaffold with dependency fetching

**Type**: Technical
**GitHub Label**: technical
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `build`, `infrastructure`
**Status**: done — issue closed, PR #27 merged. Of 4 criteria, 1 only partly verified.

### Description

As the developer, I want a CMake build that fetches and links all dependencies, so that the project compiles from a fresh clone on Windows, macOS, and Linux.

### Acceptance Criteria

- [x] Given a fresh clone, when `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` runs, then it produces a runnable `blaze-4k` binary on at least one OS
- [x] Given the CMakeLists.txt, when configuring, then SDL3 (≥3.2), glad (GL 3.3), miniaudio, stb_truetype, and nlohmann/json are all fetched via FetchContent with pinned versions
- [ ] Given MSVC, Clang, or GCC, when building with C++20, then there are no compiler-specific errors
  - *Partly verified:* the GCC build and the test suite ran without errors on Linux on 2026-10-12. No MSVC or Clang build is recorded (`docs/CROSS_PLATFORM_VERIFICATION.md`).
- [x] Given the planned layout, when the scaffold is created, then `src/`, `assets/`, `tests/` directories and `main.cpp` stub exist

### Technical Notes

- Create root `CMakeLists.txt` per PRD §6 directory structure
- CMake ≥ 3.24; use FetchContent for all deps (PRD §8)
- Single-header deps (miniaudio, stb_truetype) can be vendored under a `third_party/` or fetched
- PRD reference: §8 Technology Stack, §12 Phase A

### As Built

- Dependencies are pinned in `CMakeLists.txt`: SDL3 `release-3.2.8`, glad `v0.1.36`, nlohmann/json `v3.11.3`, miniaudio `0.11.21`, and stb at one commit.
- Per-platform configure, build and test presets are in `CMakePresets.json`.

### Dependencies

- Blocked by: —
- Blocks: A2, A3, A4, A5, B2, C2

---

## [A2] Application window, GL context, and main loop

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `app`, `render`
**Status**: done — issue closed, PR #28 merged

### Description

As the developer, I want a bootable app with an SDL3 window, OpenGL 3.3 context, and a fixed-timestep main loop with vsync, so that all gameplay and screens have a stable frame foundation.

### Acceptance Criteria

- [x] Given the built binary, when launched, then an SDL3 window opens with a GL 3.3 core context (via glad)
- [x] Given the main loop, when running, then updates use fixed-timestep accumulation and rendering is vsync'd
- [x] Given the app is running, when the user closes the window or presses Escape, then it shuts down cleanly (no crash, no leaked GL context)
- [x] Given a resize event, when the window changes size, then the GL viewport updates correctly

### Technical Notes

- Files: `src/main.cpp`, `src/app/` (main loop, frame pacing)
- Keep SDL3 behind a thin platform wrapper (PRD §6 pattern 5) — core stays testable
- No frame-timing logic may leak into future judgment path (PRD core principle 1)
- PRD reference: §6 architecture, §12 Phase A

### As Built

- The window asks for an OpenGL 3.3 core context and loads it through glad (`src/app/window.cpp`).
- Escape is the Back action: it quits from Title and navigates back on every other screen (`default_back_navigates` in `src/screens/screen_manager.cpp`).
- The viewport follows `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (`src/app/app.cpp`).

### Dependencies

- Blocked by: A1
- Blocks: A4, B3, C1

---

## [A3] Audio engine: miniaudio playback with stream-position query

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `audio`
**Status**: done — issue closed, PR #29 merged. Of 4 criteria, 1 only partly verified.

### Description

As the developer, I want an audio wrapper that plays song files and exposes the exact stream position, so that the gameplay clock can be driven by the audio hardware position instead of wall time.

### Acceptance Criteria

- [x] Given a supported audio file (wav/ogg/mp3 per miniaudio), when playback starts, then sound plays and `get_position_seconds()` returns sample position ÷ sample rate
- [ ] Given a playing stream, when queried repeatedly, then reported position is monotonic and tracks real time within a few ms
  - *Partly verified:* the audio tests passed on the null audio device on 2026-10-12. No run on a real device was made for this update; `docs/AUDIO_LATENCY.md` holds the earlier device measurements.
- [x] Given a missing or corrupt file, when load is attempted, then the call fails gracefully with a readable log line (no crash)
- [x] Given the wrapper API, when the game needs it later, then start/stop/pause and volume control are available

### Technical Notes

- Files: `src/audio/` (miniaudio wrapper)
- Stream position = PCM frame cursor ÷ sample rate; this is the future basis of the music clock (PRD §6 pattern 1)
- Keep miniaudio isolated behind an interface, same philosophy as the SDL3 wrapper
- PRD reference: §7 audio engine, §12 Phase A

### As Built

- The stream interface is `load`, `play`, `stop`, `seek_seconds`, `get_position_seconds`, `is_playing` and `set_volume` (`src/audio/sound_stream.hpp`); the miniaudio stream adds `pause`.
- Ogg files are decoded through stb_vorbis (`src/audio/miniaudio_impl.cpp`).

### Dependencies

- Blocked by: A1
- Blocks: B1, C3

---

## [A4] Input layer: keyboard + pad with nanosecond event timestamps

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `input`
**Status**: done — issue closed, PR #30 merged. Of 4 criteria, 1 not met, 1 only partly verified.

### Description

As a pad player, I want keyboard and USB dance pad input captured with SDL3 nanosecond event timestamps, so that hit deltas are accurate to the hardware event rather than the frame boundary.

### Acceptance Criteria

- [ ] Given a keyboard, when arrow keys/enter/esc are pressed, then events are logged with their SDL3 nanosecond timestamps
  - *Not met:* each event carries its SDL3 timestamp (`InputEvent::timestamp_ns`), but no log line is printed per event.
- [ ] Given a USB dance pad, when connected, then it is recognized as an SDL3 gamepad and its panel presses are logged with timestamps
  - *Partly verified:* `InputManager` opens every SDL gamepad on `SDL_EVENT_GAMEPAD_ADDED` and logs the connection. Presses carry timestamps but are not logged, and no run on pad hardware is recorded.
- [x] Given a pad is unplugged mid-session, when hotplug occurs, then the game handles it gracefully and re-detects on reconnect
- [x] Given the input layer API, when a consumer asks, then it receives polled input events with device, button, state, and timestamp

### Technical Notes

- Files: `src/input/` (SDL3 devices, mapping, event timestamps)
- Capture the SDL event timestamp at poll time — never `now()` after the fact (PRD §7.4)
- Default mapping: arrows + enter/esc; remapping UI is a separate story (C6)
- Poll input before rendering each frame (PRD §14 latency mitigation)
- PRD reference: §7.4 Input, §12 Phase A

### As Built

- `InputEvent` holds `action`, `pressed`, `timestamp_ns`, `device`, `device_id` and `raw_code` (`src/input/input_event.hpp`).
- Raw keys and buttons are mapped to game actions through the bindings of `config.json` before a consumer sees them.

### Dependencies

- Blocked by: A2
- Blocks: B4, C6

---

## [A5] SM/SSC header and timing parsing

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Large
**Phase**: A — Foundation
**Labels**: `chart`, `parser`
**Status**: done — issue closed, PR #31 merged. Of 5 criteria, 1 not checked.

### Description

As a pad player, I want the game to parse SM and SSC file headers (title, artist, banner, background, music, offset, BPMs, stops), so that my existing simfile library loads without conversion.

### Acceptance Criteria

- [x] Given a valid `.sm` file, when parsed, then title, artist, banner path, background path, music path, offset, BPM changes, and stops are extracted into the chart model
- [x] Given a valid `.ssc` file, when parsed, then the same metadata is extracted including per-chart SSC fields
- [x] Given a file with BPM changes and stops, when parsed, then the timing data preserves beat→time conversion fidelity
- [x] Given a malformed header, when parsed, then the file is rejected with a readable log line — never a crash
- [ ] Given ambiguous semantics, when implementing, then behavior matches StepMania 5 source rather than guesswork
  - *Not checked:* the parser cites StepMania 5 source lines in its comments (`src/chart/simfile_parser.cpp`), but the StepMania source was not read for this update.

### Technical Notes

- Files: `src/chart/` (parser, note model)
- Simfiles are untrusted input: bounds-checked, allocation-capped (PRD §9)
- Reference authority: StepMania 5 source for parsing semantics (PRD §15)
- Console verification is sufficient at this phase (PRD §12A)
- PRD reference: §7.1 Simfile engine, §12 Phase A

### As Built

- The header parser also reads `SUBTITLE`, and an SSC chart's own `BPMS`, `STOPS` and `OFFSET` (`src/chart/simfile_parser.cpp`).

### Dependencies

- Blocked by: A1
- Blocks: A6, A7

---

## [A6] 4-panel note data parsing (taps, holds, rolls, mines)

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Large
**Phase**: A — Foundation
**Labels**: `chart`, `parser`
**Status**: done — issue closed, PR #32 merged

### Description

As a pad player, I want 4-panel note charts parsed into the internal note model — taps, holds, rolls, and mines — so that every playable chart in my packs becomes game data.

### Acceptance Criteria

- [x] Given a 4-panel (dance-single) chart, when parsed, then every tap, hold (head+tail), roll, and mine is represented in the note model with correct beat/time and column
- [x] Given multiple difficulties in one file, when parsed, then each chart with its passthrough difficulty label and foot rating is available separately
- [x] Given a non-4-panel chart (e.g. dance-double, pump), when parsed, then it is rejected gracefully with a log entry and skipped
- [x] Given exotic timing (warps, negative BPMs, split timing), when encountered, then the chart is rejected with a readable log line rather than misparsed

### Technical Notes

- Files: `src/chart/` (note model)
- Note model should be "editor-shaped" for the future chart editor (PRD §13) — immutable notes with column, time, type, length
- Difficulty display is passthrough from the simfile (locked decision, PRD §15)
- Check StepMania source for edge cases; never guess (AGENTS.md core principle 2)
- PRD reference: §7.1, §12 Phase A

### As Built

- A steps type that is not 4-panel is skipped with a log line (`src/chart/note_parser.cpp`).
- A warp (negative stop) or a non-positive BPM marks the timing as exotic, and the chart is rejected with a log line (`TimingData::has_exotic_timing`).
- SSC split timing is not rejected: a chart's own `BPMS`, `STOPS` and `OFFSET` are parsed and used.
- A chart of more than 10,000 measures, 1,024 rows in a measure, or 100,000 notes is rejected.

### Dependencies

- Blocked by: A5
- Blocks: A7, B3, B4

---

## [A7] Song library scanner with pack grouping and art resolution

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `chart`, `library`
**Status**: done — issue closed, PR #33 merged

### Description

As a pad player, I want to point the game at my `Songs/` folder and have it recursively scan packs into a grouped song library with resolved banner/background art, so that my collection appears without any manual setup.

### Acceptance Criteria

- [x] Given a `Songs/<Pack>/<Song>/` folder structure, when scanned, then every song with at least one 4-panel chart appears in the library grouped by pack
- [x] Given a song folder, when scanned, then banner and background image paths from the simfile are resolved (with missing-art fallback)
- [x] Given a corrupt or unsupported simfile in the tree, when scanned, then it is skipped with a log line and scanning continues
- [x] Given the reference pack, when scanned, then 100% of its 4-panel songs load error-free (PRD §11)

### Technical Notes

- Files: `src/chart/` (library scanner)
- Console verification of scan results is acceptable for Phase A; the wheel UI is C3
- Reads community packs unmodified (PRD §4 Integration)
- PRD reference: §7.1, §5 story 1, §11 success criteria, §12 Phase A

### As Built

- Without `--songs <dir>`, the game scans `./songs` and then `./data/songs` (`src/main.cpp`).
- A song without its own art gets the fallback banner and background that `SongLibrary::set_fallback_banner` and `set_fallback_background` hold.

### Dependencies

- Blocked by: A5, A6
- Blocks: C3, D1

---

## [A8] Parser hardening: unit tests and fuzz against reference pack

**Type**: Technical
**GitHub Label**: technical
**Priority**: High
**Complexity**: Medium
**Phase**: A — Foundation
**Labels**: `tests`, `parser`
**Status**: done — issue closed, PR #34 merged. Of 4 criteria, 1 not checked.

### Description

As the developer, I want parser unit tests plus fuzzing against malformed files and the reference pack, so that a corrupt simfile can never crash the game.

### Acceptance Criteria

- [x] Given the reference simfile pack, when the test suite runs, then 100% of its 4-panel songs parse with zero crashes
- [x] Given deliberately malformed `.sm`/`.ssc` files (truncated, huge allocations, bad encodings, binary garbage), when fuzzed/parsed, then every file is rejected gracefully — no crash, no unbounded allocation
- [ ] Given known SM/SSC edge cases from StepMania source, when tested, then parsed output matches reference behavior
  - *Not checked:* the edge-case tests passed on 2026-10-12, but their expected values were not compared with the StepMania source for this update.
- [x] Given `ctest --test-dir build --output-on-failure`, when run, then all parser tests pass

### Technical Notes

- Files: `tests/` (parser tests + fixtures)
- Reference pack provided by product owner — add as test fixture location (PRD §15)
- Allocation caps and bounds checks are acceptance-testable, not just best-effort (PRD §9)
- PRD reference: §9 Security, §11 success criteria, §12 Phase A

### As Built

- The caps are in `src/chart/msd_file.hpp`: 16 MB per file, 10,000 tags, 1,000 parameters per tag, 1 MB per parameter.
- `tests/parser_hardening_test.cpp` holds the fuzz mutations, driven by a deterministic generator.

### Dependencies

- Blocked by: A5, A6, A7
- Blocks: —

---

## Phase B — Gameplay core

## [B1] Music-driven gameplay clock with global offset

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: B — Gameplay core
**Labels**: `timing`, `audio`
**Status**: done — issue closed, PR #35 merged

### Description

As a competitive player, I want gameplay time derived from the audio stream position plus a calibrated global offset, so that notes scroll and judge in perfect sync with the music — never drifting with frame timing.

### Acceptance Criteria

- [x] Given a playing song, when gameplay queries the time, then it returns audio stream sample position ÷ sample rate + global offset
- [x] Given frame hitches or vsync jitter, when they occur, then the music clock is unaffected (no wall-clock or frame-delta input anywhere in the judgment path)
- [x] Given a global offset value in config, when applied, then positive/negative offsets shift judged time in the correct direction
- [x] Given the offset is uncalibrated (0), when a song plays, then the clock still functions with sane defaults

### Technical Notes

- Files: `src/timing/` (music clock, offset)
- This is the correctness backbone — PRD §6 pattern 1; violating it breaks the core promise (§14 risks)
- Unit-test the clock math independently of audio hardware where possible
- PRD reference: §6 pattern 1, §12 Phase B

### As Built

- `MusicClock` (`src/timing/music_clock.cpp`) adds the global offset to the stream position.
- Between audio updates the position is interpolated with a monotonic clock by `ClockInterpolator`: re-anchored at every audio update, never decreasing, and bounded to one device period (#71, #81; `docs/AUDIO_LATENCY.md`). Frame time is not an input.

### Dependencies

- Blocked by: A3
- Blocks: B3, B4, B7, C5

---

## [B2] Data-driven judgment and scoring constants from OpenITG

**Type**: Technical
**GitHub Label**: technical
**Priority**: High
**Complexity**: Small
**Phase**: B — Gameplay core
**Labels**: `timing`, `data`
**Status**: done — issue closed, PR #36 merged. Of 4 criteria, 1 not checked.

### Description

As a competitive player, I want timing windows, DP weights, grade boundaries, and life deltas loaded from a JSON config table seeded with OpenITG-verified values, so that scoring matches real ITG behavior and stays tunable.

### Acceptance Criteria

- [x] Given the constants table, when loaded from JSON, then Judge-4-tight windows, DP weights, grade boundaries, and life deltas are all configurable without recompiling
- [ ] Given the seeded defaults, when compared against OpenITG source, then windows, weights, life behavior, and grade thresholds match the reference
  - *Not checked:* OpenITG's `metrics.ini` was not fetched for this update. `assets/data/judgment_constants.json` names the OpenITG commit that its values come from.
- [x] Given a missing or malformed constants file, when the game starts, then it falls back to compiled-in OpenITG defaults with a log warning
- [x] Given the constants, when a Fantastic/Great/Decent/Way Off/Miss is evaluated, then the correct window boundaries apply (Fantastic +2 DP, Decent breaks combo, per OpenITG)

### Technical Notes

- Files: `src/timing/` (windows), `src/data/` (JSON loading)
- Lift the final grade-boundary/DP table from OpenITG source — resolves the PRD §15 open item
- PRD §6 pattern 3: data-driven constants
- PRD reference: §6 pattern 3, §15 open items, §12 Phase B

### As Built

- The table is `assets/data/judgment_constants.json`. A missing or malformed file gives the compiled defaults with a warning (`src/data/judgment_constants_loader.cpp`).
- A Fantastic is worth 5 DP (`dp_weights.fantastic`), not the 2 that the criterion's example gives.
- The windows are 21.5, 43, 102, 135 and 180 ms, each widened by `judge_window_add` (1.5 ms).

### Dependencies

- Blocked by: A1
- Blocks: B4, B5, B6

---

## [B3] Note field rendering with receptors, speed mods, and scroll direction

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Large
**Phase**: B — Gameplay core
**Labels**: `render`, `gameplay`
**Status**: done — issue closed, PR #37 merged. Of 5 criteria, 1 not checked.

### Description

As a player, I want a scrolling 4-panel note field with receptors and C-mod, X-mod, and M-mod speed options plus up/down scroll, so that I can read charts at my preferred speed regardless of BPM changes.

### Acceptance Criteria

- [x] Given a parsed chart and playing song, when gameplay runs, then notes scroll toward the receptor row driven by the music clock
- [x] Given C400 selected, when the song has BPM changes, then scroll speed stays constant through tempo shifts
- [ ] Given X-mod or M-mod selected, when gameplay runs, then scroll speed follows the modifier semantics matching ITG behavior
  - *Not checked:* `SpeedModType` holds the semantics (an X-mod spaces notes by beats, an M-mod is resolved to an X-mod), but they were not compared with ITG for this update.
- [x] Given the scroll direction option, when toggled, then the field renders upscroll (default) or downscroll correctly
- [x] Given holds, rolls, and mines, when rendered, then each note type is visually distinct (body/tail for holds & rolls, mine graphic)

### Technical Notes

- Files: `src/gameplay/` (note field), `src/render/` (quads, sprites)
- 2D textured quads only, GL 3.3 (PRD §8); placeholder noteskin acceptable until D2
- Speed mod math must use chart timing (BPMs/stops) from A5/A6
- PRD reference: §7.2 Gameplay core, §5 story 3, §12 Phase B

### As Built

- The mods are parsed by `parse_speed_mod` (`src/gameplay/speed_mod.hpp`); the default is `1x`, upscroll (`GameplaySettings` in `src/data/config.hpp`).

### Dependencies

- Blocked by: A2, A6, B1
- Blocks: B4, D1, D2

---

## [B4] Judgment engine with event-sourced judgment log

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Large
**Phase**: B — Gameplay core
**Labels**: `gameplay`, `timing`
**Status**: done — issue closed, PR #38 merged. Of 5 criteria, 1 not checked.

### Description

As the developer, I want every hit evaluated against the timing windows and emitted as an immutable timestamped judgment event, so that scoring, combo, life, results — and future replays/stats — all derive from one event log.

### Acceptance Criteria

- [x] Given a note and an input timestamp, when evaluated, then the delta in ms is computed against the music clock and classified into Fantastic/Great/Decent/Way Off/Miss per the constants table
- [x] Given any hit, when judged, then an event `{column, note_time, hit_time, delta_ms, window}` is appended to the in-memory session log
- [ ] Given a hold, when the head is hit and tail released, then hold head/tail handling matches OpenITG behavior; rolls support re-hit logic; mines apply their penalty when triggered
  - *Not checked:* hold, roll and mine handling is in `src/gameplay/judgment_engine.cpp`, but it was not compared with the OpenITG source for this update.
- [x] Given visual judgment feedback, when shown, then it matches the event log exactly (PRD §11 quality indicator)
- [x] Given an untouched note passing its window, when it expires, then a Miss event is emitted

### Technical Notes

- Files: `src/gameplay/` (judgments), `src/timing/` (windows), `src/input/` (timestamps)
- PRD §6 pattern 2: event-sourced judgments — scoring/life/results must consume the log, not judge independently
- No frame-timing logic in the judgment path (AGENTS.md core principle 1)
- PRD reference: §6 pattern 2, §5 stories 2 & 8, §7.2, §12 Phase B

### As Built

- `JudgmentEvent` (`src/gameplay/judgment.hpp`) holds `kind`, `column`, `note_time_seconds`, `hit_time_seconds`, `delta_ms`, `window`, `hold`, `note_type` and `note_index`.
- The judgment and combo animations consume the same events (`JudgmentAnimator::consume`).

### Dependencies

- Blocked by: B1, B2, A4, A6
- Blocks: B5, B6, C7, D2

---

## [B5] Scoring: dance points, percentage, combo, and ★ grades

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: B — Gameplay core
**Labels**: `gameplay`, `scoring`
**Status**: done — issue closed, PR #39 merged. Of 5 criteria, 1 dropped by owner decision, 2 not checked.

### Description

As a competitive player, I want OpenITG-equivalent scoring — dance points, percentage, combo, and ★ grades — so that my scores mean the same thing they do on real hardware.

### Acceptance Criteria

- [x] Given a judgment event stream, when scoring runs, then DP totals accumulate per OpenITG weights (Fantastic +2 DP etc.)
- [ ] Given a completed chart, when the percentage is computed, then it matches the OpenITG DP% formula for a known reference chart
  - *Not checked:* no score was compared with OpenITG on a reference chart for this update.
- [ ] Given the DP%, when mapped to a grade, then ★ grade thresholds match OpenITG (99%+ earns ★★★★ per PRD §5 example)
  - *Not checked:* the thresholds were not compared with OpenITG for this update. In `grade_tiers`, 99% gives the three-star grade and 100% the four-star grade.
- [x] Given a combo-affecting judgment, when it occurs, then combo increments on Great-or-better and breaks on Decent-or-worse per OpenITG rules
- [ ] Given live gameplay, when the HUD renders, then score %, combo, and judgment counts update from the event log
  - *Dropped:* the owner decided on 2026-10-12 that the live HUD shows no score % and no judgment counts (PRD §7.2). The HUD draws the judgment text, the combo, the life bar and a difficulty badge; the score % and the counts are on Results.

### Technical Notes

- Files: `src/gameplay/` (scoring, combo)
- All scoring derives from the B4 event log — no independent judgment logic
- Verify against OpenITG source values on a known chart (PRD §12B validation)
- PRD reference: §7.2, §5 story 2, §12 Phase B

### As Built

- `ScoreKeeper` (`src/gameplay/score_keeper.hpp`) consumes the event log.
- A Fantastic is worth 5 DP, not the 2 that the criterion's example gives; the grade tiers are in `assets/data/judgment_constants.json`.

### Dependencies

- Blocked by: B4, B2
- Blocks: B6, C7

---

## [B6] Life bar with drain/refill and fail handling

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: B — Gameplay core
**Labels**: `gameplay`, `scoring`
**Status**: done — issue closed, PR #40 merged. Of 5 criteria, 1 not met.

### Description

As a player, I want an ITG-style life bar that starts full, drains on bad judgments and mines, refills on good ones, and fails the song at empty, so that the stakes match arcade play.

### Acceptance Criteria

- [ ] Given a song start, when gameplay begins, then the life bar starts full
  - *Not met:* life starts at 50% (`LifeState::life = 0.5` in `src/gameplay/life_keeper.hpp`). A comment there says that this follows OpenITG in place of the criterion's wording.
- [x] Given judgments, when they occur, then life drains on Decent/Way Off/Miss/mine and refills on Great-or-better per the constants table
- [x] Given an empty life bar with fail enabled, when it empties, then the song fails and transitions out of gameplay
- [x] Given the Fail-Off option, when enabled, then the song continues to the end even at zero life
- [x] Given life changes, when they derive from state, then they consume only the B4 event log

### Technical Notes

- Files: `src/gameplay/` (life)
- Life deltas come from the B2 constants table, verified against OpenITG source
- PRD reference: §7.2, §12 Phase B

### As Built

- `LifeKeeper` consumes the event log, and the deltas come from `life_deltas` in `assets/data/judgment_constants.json`.
- A Decent leaves life unchanged (delta 0.0); a dropped hold drains it by 0.08.

### Dependencies

- Blocked by: B5, B2
- Blocks: —

---

## [B7] Metronome sync-test chart (permanent regression tool)

**Type**: Technical
**GitHub Label**: technical
**Priority**: High
**Complexity**: Small
**Phase**: B — Gameplay core
**Labels**: `tests`, `timing`
**Status**: done — issue closed, PR #41 merged

### Description

As the developer, I want a permanent metronome sync-test chart, so that gameplay sync drift is caught as a regression within one judgment window over a full song.

### Acceptance Criteria

- [x] Given the sync-test chart, when played (autoplay or manual), then every note falls exactly on a metronome beat for the full song length
- [x] Given an introduced audio/offset drift, when the test chart runs, then the judgment deltas reveal the drift within one timing window
- [x] Given the test chart, when the project evolves, then it remains in `tests/` as a runnable regression tool

### Technical Notes

- Files: `tests/` (sync chart fixture + harness)
- This is the primary mitigation for the audio-sync-drift risk (PRD §14) — keep it permanent, not throwaway
- PRD reference: §11 success criteria, §14 risks, §12 Phase B

### As Built

- The chart is `tests/fixtures/sync_test/metronome.sm`, and `tests/metronome_sync_test.cpp` plays it with zero drift and with a constant offset drift. It passed on 2026-10-12.

### Dependencies

- Blocked by: B1
- Blocks: —

---

## Phase C — Arcade shell (screens + persistence)

## [C1] Screen state machine with Title and Attract screens

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `screens`, `frontend`
**Status**: done — issue closed, PR #42 merged

### Description

As a player, I want the game to boot to a title screen and idle into an attract/demo loop, so that it feels like an arcade cabinet in my living room.

### Acceptance Criteria

- [x] Given the game boots, when loading completes, then a Title screen shows the logo and "Press Start"
- [x] Given the Title screen, when Start is pressed, then it transitions toward song select
- [x] Given no input for an idle timeout, when on Title/Select, then the Attract screen (demo gameplay autoplay or title loop) activates, and any Start press exits back
- [x] Given screens, when implemented, then each is an object with `enter/update/render/exit` and explicit transitions (PRD §6 pattern 4)

### Technical Notes

- Files: `src/screens/` (screen manager, title, attract)
- ScreenManager state machine: Title → Attract → Select → Game → Results (PRD §6)
- Attract is just another screen in the machine — no special casing
- PRD reference: §7.3 Screen flow, §5 story 5, §12 Phase C

### As Built

- Attract is a title loop; gameplay autoplay is not built (`src/screens/attract_screen.hpp`).
- The idle timer runs on Title and Select only, and its default is 120 s (`--attract-timeout`).

### Dependencies

- Blocked by: A2
- Blocks: C3, C4, C7

---

## [C2] JSON persistence: config and per-chart high scores

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `data`, `persistence`
**Status**: done — issue closed, PR #43 merged

### Description

As a player, I want my settings, offset, and per-chart best scores saved to local JSON files, so that my records and configuration survive restarts.

### Acceptance Criteria

- [x] Given changed settings, when the game exits, then `config.json` (video/audio/input/offset/gameplay options) is written and reloaded correctly on next boot
- [x] Given a completed song, when the score beats the stored best for that chart, then `scores.json` updates `{grade, %, DP, timestamp}` for the chart key
- [x] Given corrupt JSON on disk, when the game boots, then it falls back to defaults with a log warning — never a crash
- [x] Given the portable layout, when installed, then data files live in a portable `data/` folder next to the binary (with XDG path option on Linux)

### Technical Notes

- Files: `src/data/` (config, high scores via nlohmann/json)
- Chart key must be stable (e.g. path + chart hash) so scores survive pack reorganization reasonably
- Fully offline: all state is local JSON (AGENTS.md core principle 4)
- PRD reference: §7.6 Persistence, §9 config, §5 story 6, §12 Phase C

### As Built

- A score record holds `grade`, `percent`, `dance_points` and `timestamp_unix`, under a content-based chart key (`make_chart_key` in `src/data/high_scores.hpp`).
- The data folder is `data/` next to the binary. `--xdg` or `BLAZE4K_XDG=1` selects the XDG data directory, and `--data-dir <path>` overrides both.

### Dependencies

- Blocked by: A1
- Blocks: C3, C4, C5, C7

---

## [C3] Song Select wheel with banners, difficulties, and audio preview

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Large
**Phase**: C — Arcade shell
**Labels**: `screens`, `frontend`, `audio`
**Status**: done — issue closed, PR #44 merged

### Description

As a pad player, I want a song select wheel showing banners, artist/BPM, difficulty list with passthrough labels and foot ratings, my best grade per chart, and audio previews, so that browsing my library feels like the arcade.

### Acceptance Criteria

- [x] Given the scanned library, when entering Select, then the wheel lists songs grouped/scrollable with banner art, artist, and BPM
- [x] Given a highlighted song, when it changes, then its audio preview plays (with sensible delay) and stops on navigation
- [x] Given a song, when its difficulties are shown, then labels and foot ratings pass through from the simfile, with my best grade displayed per chart
- [x] Given keyboard or pad input, when navigating, then all wheel operations are fully navigable on both (PRD §12C validation)
- [x] Given a selection, when confirmed, then gameplay launches with the chosen chart and current options

### Technical Notes

- Files: `src/screens/` (select), `src/audio/` (preview playback), `src/chart/` (library)
- Preview uses the simfile's preview point if present, else a sane default
- Best-grade display reads from C2 scores.json
- PRD reference: §7.3, §5 story 1, §12 Phase C

### As Built

- Each difficulty row shows the best percentage, or `---` without a record, in place of a grade.
- The preview starts 0.5 s after the highlight changes (`kPreviewDelaySeconds`).
- The wheel shows a song's subtitle, and an Edit chart is listed under its own name.

### Dependencies

- Blocked by: C1, A7, A3, C2
- Blocks: —

---

## [C4] Options menu: speed mod, scroll direction, and fail toggle

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `screens`, `frontend`
**Status**: done — issue closed, PR #45 merged

### Description

As a player, I want an options menu on the select screen for speed mod (C/X/M), scroll direction, and fail on/off, so that I can configure gameplay without editing files.

### Acceptance Criteria

- [x] Given the select screen, when opening options, then speed mod (C/X/M with value), scroll direction, and fail toggle are all adjustable
- [x] Given changed options, when a song starts, then gameplay applies them (mod math from B3, fail behavior from B6)
- [x] Given changed options, when the game exits, then they persist via config.json (C2)
- [x] Given pad-only input, when using the menu, then every option is reachable and changeable

### Technical Notes

- Files: `src/screens/` (options menu), `src/data/` (config)
- Mods in scope: C/X/M + scroll direction only — turn/appearance mods are out of scope (PRD §4, locked decisions)
- Entry point for the offset wizard (C5) and remapping (C6)
- PRD reference: §7.3 Select, §4 scope, §12 Phase C

### As Built

- The rows are speed type, speed value, scroll, fail, assist tick, calibrate offset and remap input (`OptionsRow` in `src/screens/options_menu.hpp`).
- The menu opens with the Options action: Tab, or a shoulder button on a pad.

### Dependencies

- Blocked by: C1, C2
- Blocks: C5, C6

---

## [C5] Global offset calibration wizard

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `screens`, `timing`, `input`
**Status**: done — issue closed, PR #46 merged

### Description

As a pad player, I want a guided tap-to-the-beat calibration wizard that computes and saves my global audio offset, so that my hits register on-time despite pad/OS/audio latency.

### Acceptance Criteria

- [x] Given the wizard, when started from the options menu, then it plays a steady beat and prompts the user to tap along
- [x] Given a series of taps, when enough samples are collected, then the average hit delta is computed using music-clock time and input event timestamps
- [x] Given the computed offset, when confirmed, then it is written to config.json and applied to the gameplay clock (B1)
- [x] Given a cancelled wizard, when aborted, then the previous offset is retained unchanged

### Technical Notes

- Files: `src/screens/` (wizard), `src/timing/` (offset), `src/data/` (config)
- Must use the same input-timestamp + music-clock path as real gameplay, or the calibration is meaningless
- Mitigates the per-OS/hardware latency risk (PRD §14)
- PRD reference: §7.5 Calibration wizard, §5 story 4, §12 Phase C

### As Built

- The offset is the negated mean of hit time minus beat time (`src/timing/offset_calibration.hpp`).
- A delay outside the accepted range is flagged and is never saved; the offset is written only on Confirm (`src/screens/calibration_screen.cpp`).

### Dependencies

- Blocked by: C4, B1
- Blocks: —

---

## [C6] Input remapping screen

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `screens`, `input`
**Status**: done — issue closed, PR #47 merged

### Description

As a player, I want to remap keyboard keys and pad panels to game actions from a menu, so that any controller layout works without editing config files.

### Acceptance Criteria

- [x] Given the remapping screen, when an action is selected, then the next key/pad input binds to it (with conflict detection)
- [x] Given new bindings, when applied, then menu navigation and gameplay both honor them immediately
- [x] Given saved bindings, when the game restarts, then they persist via config.json
- [x] Given a "reset to defaults" action, when used, then arrows + enter/esc and default pad mapping are restored

### Technical Notes

- Files: `src/screens/` (remap UI), `src/input/` (mapping layer)
- Mapping indirection lives in the input layer (A4); this story is the UI + persistence
- Keep at least one working navigation path so users can't soft-lock themselves
- PRD reference: §7.4 Input, §12 Phase C

### As Built

- The defaults are the arrow keys and D/F/J/K, Enter and keypad Enter, Escape, and Tab; on a pad, the d-pad and face buttons, Start, Back, and the shoulder buttons (`src/data/config_loader.cpp`).
- A key that is already bound is refused with an `IN USE` message; `input_remap_reset` restores the defaults (`src/screens/input_remap.hpp`).

### Dependencies

- Blocked by: C4, A4
- Blocks: —

---

## [C7] Results screen with grade, breakdown, and NEW RECORD flag

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: High
**Complexity**: Medium
**Phase**: C — Arcade shell
**Labels**: `screens`, `frontend`, `scoring`
**Status**: done — issue closed, PR #48 merged

### Description

As a player, I want a results screen after each song showing my grade, percentage, DP, judgment breakdown, max combo, and a NEW RECORD flag on personal bests, so that I get an honest arcade-style evaluation.

### Acceptance Criteria

- [x] Given a completed (or failed) song, when gameplay ends, then Results shows grade, %, DP, per-window judgment counts, and max combo — all derived from the event log
- [x] Given a new personal best for the chart, when results display, then a "NEW RECORD" flag shows and scores.json is updated
- [x] Given the results screen, when confirmed, then flow returns to the song wheel with no dead ends (PRD §11 complete-loop criterion)
- [x] Given a failed song, when results display, then the failure is clearly indicated alongside the stats

### Technical Notes

- Files: `src/screens/` (results), `src/gameplay/` (event log), `src/data/` (scores)
- Everything on this screen derives from the B4 event log + B5 scoring (PRD §6 pattern 2)
- Grade animation polish is D3; this story needs correct, readable data first
- PRD reference: §7.3 Results, §5 story 6, §11, §12 Phase C

### As Built

- The summary is built by `results_summary_from` (`src/screens/results.hpp`) from the score state and the fail flag.
- Results also shows holds OK, holds NG and mines hit, and a FAILED ribbon after a fail.

### Dependencies

- Blocked by: C1, B5, C2
- Blocks: D3

---

## Phase D — Polish (visual + feel)

## [D1] Dimmed simfile background during gameplay with fallback

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: D — Polish
**Labels**: `render`, `gameplay`
**Status**: done — issue closed, PR #49 merged. Of 4 criteria, 1 only partly verified.

### Description

As a player, I want the simfile's background image shown dimmed behind the note field during gameplay, so that songs have visual identity without distracting from the arrows.

### Acceptance Criteria

- [x] Given a song with a background image, when gameplay runs, then it renders behind the receptors at reduced brightness
- [x] Given a song without background art, when gameplay runs, then the bundled fallback background renders instead
- [ ] Given any background, when rendered, then note field readability is unaffected (contrast preserved)
  - *Partly verified:* the background is drawn at 35% brightness under a 30% black scrim (`kBackgroundDim`, `kBackgroundOverlay`). Readability was not looked at on screen for this update.
- [x] Given the implementation, when reviewed, then it is 2D only — no video, no 3D (locked decision)

### Technical Notes

- Files: `src/render/` (sprites), `src/gameplay/` (note field), `assets/` (fallback bg)
- Art resolution paths come from A7's scanner
- PRD reference: §5 story 7, §12 Phase D, §15 locked decisions

### As Built

- `BackgroundRenderer` (`src/render/background_renderer.cpp`) draws a still image only; the fallback is under `assets/backgrounds`.

### Dependencies

- Blocked by: B3, A7
- Blocks: —

---

## [D2] Noteskin visuals, judgment/combo pop animations, and UI sounds

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: D — Polish
**Labels**: `render`, `audio`, `gameplay`
**Status**: done — issue closed, PR #50 merged

### Description

As a player, I want a real noteskin, judgment and combo pop animations, and UI sounds, so that the game looks and sounds like a shipped arcade product.

### Acceptance Criteria

- [x] Given gameplay, when notes render, then the final noteskin graphics replace Phase B placeholders (distinct tap/hold/roll/mine art)
- [x] Given a judgment, when it occurs, then the corresponding judgment sprite pops with animation matching the event log
- [x] Given combo milestones, when reached, then combo pop animations trigger
- [x] Given menu navigation and confirmations, when they occur, then UI sounds play

### Technical Notes

- Files: `assets/` (noteskin, fonts, UI sounds), `src/render/`, `src/gameplay/`
- Classic arcade look (locked decision); stb_truetype texture-atlased text for UI
- Animation triggers consume the B4 event log — visuals must match it exactly
- PRD reference: §12 Phase D, §15 locked decisions

### As Built

- The noteskin is Cel (`assets/noteskins/cel`).
- The combo flashes at every multiple of 50 (`JudgmentAnimator::kComboMilestone`).
- Menu sounds are played by `UiSoundPlayer` (`src/audio/ui_sounds.hpp`).

### Dependencies

- Blocked by: B3, B4
- Blocks: —

---

## [D3] Results polish: grade animations and NEW RECORD flow

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Low
**Complexity**: Small
**Phase**: D — Polish
**Labels**: `screens`, `frontend`
**Status**: done — issue closed, PR #51 merged

### Description

As a player, I want animated grade reveals and a celebratory NEW RECORD flow on the results screen, so that personal bests feel rewarding.

### Acceptance Criteria

- [x] Given the results screen, when it appears, then the grade animates in (arcade-style reveal) rather than appearing statically
- [x] Given a new record, when results show, then the NEW RECORD sequence is visually distinct from a normal clear
- [x] Given the animations, when input is pressed, then they can be skipped to the final state

### Technical Notes

- Files: `src/screens/` (results)
- Builds on C7's data-correct results screen; purely presentational
- PRD reference: §12 Phase D

### As Built

- `ResultsAnimator` (`src/screens/results_anim.hpp`) runs the reveal, and `skip()` jumps to its last frame on a press.

### Dependencies

- Blocked by: C7
- Blocks: —

---

## [D4] Cross-platform build verification and performance pass

**Type**: Technical
**GitHub Label**: technical
**Priority**: High
**Complexity**: Medium
**Phase**: D — Polish
**Labels**: `build`, `performance`, `tests`
**Status**: done — issue closed, PR #52 merged. Of 4 criteria, 3 only partly verified, 1 not checked.

### Description

As the developer, I want verified builds on Windows, macOS, and Linux plus a performance pass, so that v1 ships stable at vsync rate on a mid-range machine from a fresh clone.

### Acceptance Criteria

- [ ] Given a fresh clone, when built per AGENTS.md commands, then native binaries result on Windows (MSVC), macOS (Clang), and Linux (GCC/Clang)
  - *Partly verified:* the Linux/GCC build ran on 2026-10-12. No Windows or macOS build is recorded (`docs/CROSS_PLATFORM_VERIFICATION.md`).
- [ ] Given gameplay on a mid-range machine, when a full song plays, then frame rate holds stable 60 fps (or vsync rate) with no hitches
  - *Partly verified:* `perf_loop_test`, a headless CPU-budget test, passed on 2026-10-12. No frame-rate run on a GPU is recorded.
- [ ] Given the full arcade loop, when exercised on pad hardware, then boot → attract → select → play → results → wheel completes with no dead ends
  - *Partly verified:* the arcade-loop contract in `screen_manager_test` passed on 2026-10-12. No traversal on pad hardware is recorded.
- [ ] Given the side-by-side feel test, when the developer plays against OpenITG on pad hardware, then timing feel is indistinguishable (PRD §12D validation)
  - *Not checked:* the feel test against OpenITG on pad hardware is an owner step with no recorded result.

### Technical Notes

- Portable folder layout; no installer (PRD §4 Deployment)
- Profile the render loop and audio callback; check per-OS audio backend latency guidance (PRD §14)
- This is the final PRD §11 success-criteria gate for v1
- PRD reference: §11 Success criteria, §12 Phase D

### As Built

- The presets are in `CMakePresets.json`, and `scripts/fresh-clone-check.sh` is the Linux fresh-clone check.
- `--perf-report` prints a frame-time report for a run on real hardware (`src/main.cpp`).

### Dependencies

- Blocked by: C3, C5, C7, D1, D2, D3
- Blocks: —

---

## Validation Summary

- **PRD coverage:** every §4 in-scope item, §7 feature, and §11 success criterion maps to at least one story; all 8 PRD user stories (§5) are covered (story 1→A7/C3, 2→B2/B4/B5, 3→B3, 4→C5, 5→C1, 6→C2/C7, 7→D1, 8→B4)
- **DAG check:** dependencies form a valid acyclic graph (A→B→C→D layering; no back-references)
- **Out-of-scope honored:** no stories for editor, courses, online, 5/6-panel, turn/appearance mods, video backgrounds, practice mode, or bundled soundtrack (PRD §4 contract)
- **Traceability:** each story cites its PRD section; open item "grade-boundary/DP table" is resolved by B2
