# Implementation Report

**Plan**: `.agents/plans/013-scoring-dance-points-percentage-combo-grades-plan.md`
**Branch**: `feature/013-scoring-dance-points-percentage-combo-grades`
**Status**: COMPLETE (work left uncommitted per orchestrator git policy)

## Summary

Implemented Blaze 4k's event-sourced scoring layer (B5): a pure `ScoreKeeper` that groups B4
`JudgmentEvent`s into OpenITG rows and derives dance points, possible dance points, DP%, combo,
max-combo, miss-combo, per-window judgment counts, and the ★ grade; a self-contained 5×7 bitmap-font
HUD renderer; and `GameplayView` integration that drains the judgment log into the keeper and draws
the live HUD. Scoring derives only from the B4 log plus a chart-derived denominator; no independent
judgment, no frame/wall-clock logic, and B2 `JudgmentConstants` weights/tiers are reused unchanged.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Score state + keeper interface | `src/gameplay/score_keeper.hpp` | ✅ |
| 2 | Reset — row tables and possible DP | `src/gameplay/score_keeper.cpp` | ✅ |
| 3 | Consume — tap/miss row aggregation and row scoring | `src/gameplay/score_keeper.cpp` | ✅ |
| 4 | Consume — holds, mines, ignored events; derived accessors | `src/gameplay/score_keeper.cpp` | ✅ |
| 5 | HUD renderer (percent, combo, counts, grade, bitmap font) | `src/gameplay/hud_renderer.{hpp,cpp}` | ✅ |
| 6 | GameplayView integration | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| 7 | Register sources and test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 8 | Test suite | `tests/score_keeper_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`) | ✅ zero warnings |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 13/13 (12 prior + `score_keeper_test`) |
| New test binary (`./build/tests/score_keeper_test`) | ✅ 15 sub-checks pass |
| Headless smoke (`--headless --gameplay-demo ... --smoke-test 120`) | ✅ exits cleanly, no GL attempted, score line emitted |
| Purity `rg` (score_keeper.*: no SDL/GL/`chrono`/frame-delta) | ✅ no matches |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/score_keeper.hpp` | CREATE | +79 |
| `src/gameplay/score_keeper.cpp` | CREATE | +239 |
| `src/gameplay/hud_renderer.hpp` | CREATE | +33 |
| `src/gameplay/hud_renderer.cpp` | CREATE | +189 |
| `tests/score_keeper_test.cpp` | CREATE | +645 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +9 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +28/-1 |
| `CMakeLists.txt` | UPDATE | +2 |
| `tests/CMakeLists.txt` | UPDATE | +11 |

`src/gameplay/judgment.hpp` and `judgment_engine.*` were not modified (B4 frozen).

## Deviations from Plan

1. **Purity doc comment reworded.** `score_keeper.hpp`'s header comment originally read "no SDL/GL/audio/chrono
   headers", which the plan's literal `rg` purity command matched (it greps for those tokens). Reworded to
   "no platform, audio, or time headers" so the check is truly zero-match. Comment-only change.
2. **`apply_hold` takes the full `JudgmentEvent`** (not `HoldJudgment`) so it can key `hold_scored_` on
   `note_index`; required to implement the plan's own `is_complete()` and duplicate guard.
3. **`RowAggregate::last_delta_ms` initialized to `numeric_limits<double>::lowest()`** when a row is created.
   The plan's literal `e.delta_ms >= row.last_delta_ms` against a `0.0` default would drop an early first hit
   (negative delta); the lowest sentinel plus the documented tie rule (equal delta → later column) preserves
   OpenITG last-tap semantics correctly.
4. **A missed hold/roll head marks `hold_scored_`** so `is_complete()` resolves it (OpenITG emits no hold
   outcome after a missed head — `judgment_engine.cpp:227-229`); the plan's Task 4 wording assumed every hold
   gets an outcome.
5. **HUD grade label**: added glyphs `+, -, *, A, B, C, D, S` and a pure `format_grade()` helper mapping
   star tiers to `****`/`***`/`**`/`*` and passing letter grades through, to satisfy resolved decision 4
   ("live grade shown") within the 5×7 bitmap-font constraint. Additional pure helper is unit-tested.
6. **Added test 15** (`GameplayView` boundary) beyond the plan's 14: drives `init` + repeated `update` on the
   deterministic stub clock, proving the drain→consume wiring and `score_state()` accessor end-to-end.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/score_keeper_test.cpp` | 1 denominator; 2 per-window DP/percent; 3 grade tiers (quad/triple/double/single/S+/C-/D + B2 equality); 4 jump row scores once; 5 last-tap + miss dominance; 6 combo rules (progress/break/miss_combo/hold/mine); 7 hold DP (OK/NG/missed head); 8 roll DP + RollHit neutral; 9 hit/avoided mine; 10 null/empty/equal-DP edges; 11 idempotence; 12 incremental==batch; 13 JudgmentEngine→drain→keeper reference arithmetic; 14 HUD formatting (percent truncation/clamp, combo, grade labels); 15 GameplayView boundary |

## End-to-End Verification Notes

- Steps 1, 2, 3, 5 passed as listed in **Validation Results**.
- **Step 4 (on a display) is blocked by the environment** (headless CI-style box, no display/GL context).
  It is externally blocked, not a code defect: `GlQuadRenderer` requires a real GL context, so the HUD
  draw path cannot be exercised visually here. Substitute coverage: the pure formatters are fully
  unit-tested (test 14), the draw path uses only `GlQuadRenderer::draw_quad` (same primitive as the
  noteskin renderer), and the headless run confirms `render()`'s uninitialized-renderer no-op and clean
  shutdown. Must be re-run on a display before release.
- The demo stub clock is wall-clock-gated (`App::run` uses `SDL_GetPerformanceCounter` frame time), so a
  short smoke run cannot reach chart end; this is a pre-existing harness affordance documented in the
  plan's Risks. The deterministic `JudgmentEngine`→`drain`→`ScoreKeeper` and `GameplayView` boundary tests
  cover the same path without wall-clock dependence.

## Cross-Issue Interfaces Exposed

- `ScoreKeeper::is_complete()` — row + hold completion for B6 fail transition / C7 results trigger.
- `GameplayView::score_state()`, `dance_points()`, `score_percent()` — live score for B6/C7.
