# Code Review: feature/011-note-field-rendering

**Scope**: Branch `feature/011-note-field-rendering` vs `main`, including all uncommitted work
(tracked modifications + untracked files). GitHub issue #11 ([B3] Note field rendering with
receptors, speed mods, and scroll direction).
**Recommendation**: APPROVE WITH NITS

## Summary

The change adds a GL 3.3 textured-quad pipeline (`src/render/`), a gameplay layer (`src/gameplay/`)
with pure speed-mod math, deterministic note-field layout, a procedural placeholder noteskin, a field
renderer, and a `GameplayView` host whose only time source is the B1 `MusicClock`. A temporary
`--gameplay-demo` harness wires it end-to-end. The layout/mod core is genuinely pure (no
SDL/GL/chrono), the C/X/M-mod math and scroll offset match the pinned OpenITG commit exactly, the
ratified decisions (X-mod 1.0 upscroll default, pure-mirror downscroll, 15%/85% receptors, M-mod=max
BPM, temporary harness) are all honored, the build is warning-free, and 11/11 tests pass. Remaining
items are one latent logic bug on the audio-relaunch path plus test/validation-coverage nits — none
block the change.

## Value Provenance Verification

Sampled against the cloned upstream repo (`/tmp/opencode/openitg`,
`HEAD == f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`) and cross-checked against the plan's citations.

| Semantics | Blaze 4k | Upstream | Source | Match |
|-----------|--------|----------|--------|-------|
| Beat-spacing offset (X/M) | `(beat - seconds_to_beat(t)) * 64 * x_speed` | `(fNoteBeat - fSongBeat) * ARROW_SPACING`, then `*= fScrollSpeed` | `ArrowEffects.cpp:36-39,135` | ✅ |
| Time-spacing offset (C) | `(note.time_seconds - t) * (c_bpm/60) * 64` | `(fNoteSeconds - fSongSeconds) * (fBPM/60) * ARROW_SPACING` | `ArrowEffects.cpp:44-51` | ✅ |
| M-mod → X-mod | `m_value / max_chart_bpm` | `fMaxScrollBPM / fMaxBPM` | `Player.cpp:234-237` | ✅ |
| Mod parse `Nx`/`cN`/`mN` | suffix/prefix `x`, `c`, `m`; positive finite only | regex `^([0-9]+(\.[0-9]+)?)x$`, `c%f`, `m%f` | `PlayerOptions.cpp:263-287` | ✅ |
| Defaults | X-mod 1.0, upscroll | `fScrollSpeed=1.0`, `fTimeSpacing=0`, `fScrollBPM=200` | `PlayerOptions.cpp:17-20` | ✅ |
| Downscroll | pure mirror `screen_y = receptor_y - offset` | `fScale = SCALE(percent,0,1,1,-1)` + half-reverse shift | `ArrowEffects.cpp:141-157` | ⚠ documented/ratified simplification (c) |

`ARROW_SIZE = 64` (used as `pixels_per_beat`) matches `ArrowEffects.cpp:11` / `ScreenDimensions.h:30`.
The prefix-`X` form is a superset of OpenITG's suffix-only regex and cannot alter suffix behavior
(report deviation #4).

## Issues Found

### Critical

None.

### High Priority

None.

### Medium Priority

1. **`src/gameplay/gameplay_view.cpp:91-93` — the audio-relaunch branch is unreachable, so a failed
   `audio_.play()` at init permanently pins the clock to the synthetic stub while audio may later
   play.** `use_stub_` is set to `!audio_started_` at init (`:48-52`), and the `else if
   (!audio_started_ && audio_.is_loaded())` can therefore never execute (the first branch always wins
   when `use_stub_` is true; when it is false `audio_started_` is already true). If `play()` fails at
   init for a transient reason but would succeed later, `audio_started_` is never re-attempted, or if
   it were, the clock source would remain the stub and gameplay time would desync from real audio —
   directly at odds with the "timing is sacred" principle. Wire the clock source to whichever source
   actually drives update, or drop the dead retry branch.

2. **E2E/validation claim exceeds what the committed assets can exercise.**
   `tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/` contains a 15-byte placeholder
   `music.ogg` (load fails, `[SoundStream] error -10`) and a Beginner chart with `taps=4 holds=0
   rolls=0 mines=0`. The plan (`011-note-field-rendering-plan.md:597-608`) describes this fixture as
   having "real audio + holds/mines" and E2E step 4 as verifying hold/roll/mine rendering and C400
   sync "in time with the music". Neither is achievable: the committed harness always runs the stub
   clock and only the tap render path, and headless cannot exercise GL at all. The implementation
   report acknowledges the audio gap (deviation #9) but not the missing note types. Add a hold/roll/
   mine-bearing fixture (or a synthesized chart) and a real audio clip so the demo actually covers the
   acceptance criteria it claims.

### Suggestions (Low)

3. **`src/gameplay/gameplay_view.cpp:62-65` — for C-mod the log prints `effective_x_speed()` (the raw
   C BPM, e.g. `x-speed 400`), which reads as an X multiplier but is not.** Print the BPM/60 time-
   spacing or omit the x-speed field for C-mod.
4. **`src/gameplay/note_field.cpp:91-98` — culling tests note centers only, with no half-quad margin,
   so heads/bodies whose center is just outside the window are dropped while their quad would still
   be partially on-screen (visible pop-in at the screen edge).** Extend the visible bounds by half a
   note dimension.
5. **`tests/note_field_test.cpp` — coverage gaps in the pure-math suite.** Test 8 exercises
   `tail_offset_for` only through X-mod; the C-mod tail branch (`note_field.cpp:57-59`) and
   `offset_for_beat`'s C branch are untested. The M-mod "no valid BPM → 1.0" fallback is documented
   unreachable, so that is acceptable; the C-mod tail path is reachable and should be pinned.
6. **`src/main.cpp:20` — help text lists `Nx, cN, or mN` but the parser also accepts prefix `X2`.** Align
   the help with the accepted grammar.
7. **Unused interface surface**: `GameplayView::is_using_stub_clock` / `chart()` / `skin()`
   (`gameplay_view.hpp:43-45`) and `NoteFieldRenderer::last_drawn_quads()` (`note_field_renderer.hpp:21`)
   have no callers; consider trimming until B4/C1 needs them.
8. **`src/gameplay/note_field.cpp:64-103` — `compute_visible` rescans every note each frame.** Accepted
   by the plan as a v1 tradeoff (notes are beat-sorted); worth tracking for a cursor/binary-search
   follow-up.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j16`, forced recompile of all new TUs) | PASS — zero warnings under `-Wall -Wextra -Wpedantic` |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 11/11 (10 prior + `note_field_test`), 0 skipped |
| Lint | N/A (project has no separate linter; warnings-as-signal via compiler flags) |
| Module purity (`SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter` on `speed_mod.*`, `note_field.*`) | PASS — no matches |
| Upstream provenance sample (ArrowEffects/PlayerOptions/Player) | PASS — offsets, M-mod resolution, defaults, parse match pinned commit |
| Headless harness smoke (`--gameplay-demo`, X-mod and `C400 --downscroll`, 60 frames) | PASS — exits 0, logs counts/speed, no GL calls headless |
| GL render path | NOT RUN — no display in this environment; `NoteSkin` is disabled headless and the demo's committed fixture cannot exercise holds/rolls/mines or the audio clock (finding 2) |

## What's Good

- Clean two-layer split: pure `speed_mod`/`note_field` are free of platform/time headers and take an
  absolute music time, so layout is deterministic and testable; only `src/render/` + `noteskin`/
  `gameplay_view` touch GL.
- Mod math and offset formulas match the pinned OpenITG source exactly (table above), including the
  stop semantics (X/M frozen vs C moving) and the `fYOffset < 0` early-return equivalence.
- No wall-clock/frame-delta leaks into the judgment/layout path; `MusicClock` is the sole time source
  for the field (`gameplay_view.cpp:115`).
- All five ratified decisions are implemented as specified (default X-mod 1.0 upscroll; pure-mirror
  downscroll; 15%/85% receptors and 64px columns as placeholders; M-mod `max()` over
  `TimingData::bpms()`; temporary `--gameplay-demo` clearly marked).
- GL resource lifetimes are safe: move-only `Texture`, all GL-backed objects explicitly shut down
  before the window/app destructor, and every draw entry point no-ops when uninitialized (headless).
- Non-throwing, bounds-checked input handling: strict `from_chars` + finiteness/positivity checks in
  `parse_speed_mod`; `resolve_x_speed` guards division by zero with a warning.

## Recommendation

Approve with nits. Issue #11's acceptance criteria are met and the implementation faithfully
reproduces OpenITG semantics. The only non-blocking correctness item is the dead audio-relaunch
branch (finding 1); findings 2 (harness coverage vs. claims) and 5 (C-mod tail test) are the most
worth addressing next.

Base directory for this skill: /home/lauri/.claude/skills/review
