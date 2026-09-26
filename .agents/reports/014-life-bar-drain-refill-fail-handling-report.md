# Implementation Report

**Plan**: `.agents/plans/014-life-bar-drain-refill-fail-handling-plan.md`
**Branch**: `feature/014-life-bar-drain-refill-fail-handling`
**Status**: COMPLETE

## Summary

Built Tundra's life layer as a pure, event-sourced module. `LifeKeeper` consumes the frozen B4
`JudgmentEvent` log, groups per-note tap events into OpenITG rows (row identity = exact `Note.beat`,
the B5 rule), applies exactly one life delta per resolved row plus individual deltas for hold/roll
outcomes and hit mines, clamps life to `[0,1]`, and derives `is_failing()` / `has_failed()`. A
quads-only life-bar HUD element was added, `GameplayView` now owns the keeper, drains events into it,
honors `fail_enabled` (Fail-Off), exposes `outcome()`, and stops gameplay on failure; a `--fail-off`
demo flag was wired. Per the user's resolved decisions the B2 `LifeDeltas` table gained OpenITG's
`hot_downgrade` (-0.10) and `regen_combo_after_miss` (5) constants (JSON seed + loader + compiled
fallback), and the hot downgrade and combo-to-regain behaviors are implemented.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Life state + keeper interface | `src/gameplay/life_keeper.hpp` | ✅ |
| 2 | Reset: row tables + full life (0.5) | `src/gameplay/life_keeper.cpp` | ✅ |
| 3 | Consume: per-row taps, holds, mines | `src/gameplay/life_keeper.cpp` | ✅ |
| 4 | Apply pipeline: clamp, freeze, fail / Fail-Off, merciful, hot, regen | `src/gameplay/life_keeper.cpp` | ✅ |
| 5 | HUD life bar (quads only) | `src/gameplay/hud_renderer.{hpp,cpp}` | ✅ |
| 6 | GameplayView integration + fail transition | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| 7 | Demo `--fail-off` flag | `src/main.cpp` | ✅ |
| 8 | Register source + test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 9 | Test suite | `tests/life_keeper_test.cpp` | ✅ |
| + | Resolved decision 2: `hot_downgrade` / `regen_combo_after_miss` constants + JSON/loader | `src/timing/judgment_constants.{hpp,cpp}`, `src/data/judgment_constants_loader.cpp`, `assets/data/judgment_constants.json`, `tests/judgment_constants_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=...`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ 0 warnings / 0 errors (`-Wall -Wextra -Wpedantic`) |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 14/14 |
| Explicit `./build/tests/life_keeper_test` | ✅ all sub-checks pass |
| Purity `rg "SDL|glad|gl[A-Z]|ma_|chrono|thread|...|fixed_dt" life_keeper.{hpp,cpp}` | ✅ no matches |
| Headless smoke (`--gameplay-demo ... --smoke-test 120`) | ✅ clean exit, `life 0.500 alive`, `fail enabled` |
| Fail-Off smoke (`--smoke-test 120 --fail-off`) | ✅ clean exit, `fail off` |
| `git` state | ✅ uncommitted changes on feature branch (per policy) |

An initial `-Wmaybe-uninitialized` warning on `delta` in `delta_for_tap` was fixed (initialized to
`0.0`); the subsequent full rebuild produced zero warnings.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/life_keeper.hpp` | CREATE | new |
| `src/gameplay/life_keeper.cpp` | CREATE | new |
| `src/gameplay/hud_renderer.hpp` | UPDATE | +5 |
| `src/gameplay/hud_renderer.cpp` | UPDATE | +41 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +15 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +40/-6 |
| `src/main.cpp` | UPDATE | +5 |
| `CMakeLists.txt` | UPDATE | +1 |
| `tests/CMakeLists.txt` | UPDATE | +11 |
| `tests/life_keeper_test.cpp` | CREATE | new |
| `src/timing/judgment_constants.hpp` | UPDATE | +9 |
| `src/timing/judgment_constants.cpp` | UPDATE | +6/-3 |
| `src/data/judgment_constants_loader.cpp` | UPDATE | +2 |
| `assets/data/judgment_constants.json` | UPDATE | +3/-1 |
| `tests/judgment_constants_test.cpp` | UPDATE | +12/-2 |

`src/gameplay/judgment.hpp`, `judgment_engine.*`, and `score_keeper.*` are unmodified.

## Deviations from Plan

1. **Start life = 0.5** (user resolved decision 1), not the AC's "full" 1.0, for OpenITG
   `LifeMeterBar.InitialValue` parity. `LifeState::life` defaults to `0.5`; test 1 pins it.
2. **Hot downgrade + combo-to-regain implemented** (user resolved decision 2), which the plan had
   deferred to OQ2. Added `hot_downgrade`/`regen_combo_after_miss` to `LifeDeltas` (JSON, loader,
   compiled fallback, validation, seed-parity test).
3. **Regen formula scope**: the user pinned only `regen_combo_after_miss = 5`. Implemented OpenITG's
   decrement-then-check loop with the debt re-armed to exactly 5 on every loss. OpenITG additionally
   accumulates successive losses up to `MaxRegenComboAfterMiss = 10` and has a fail-time
   `RegenComboAfterFail = 10` bump (`LifeMeterBar.cpp:221-227,244-253`); these two constants were not
   requested, so they are omitted. With the default 5, one loss suppresses the next four positive
   judgments (the debt reaches 0 before the fifth, which applies).
4. **Plan typo corrected**: the plan's Task 3 said `delta_for_tap(Miss/HitMine) -> 0`; implemented
   `Miss -> life.miss` and `HitMine -> life.hit_mine` (otherwise misses would deal no life damage).
5. **Added a hold/roll outcome duplicate guard** (`hold_scored_`, mirroring B5) so repeated
   `HoldOk`/`HoldNg` events are idempotent (plan's test 9 requires this; the plan's consume sketch
   had no guard).
6. **Extended `tests/judgment_constants_test.cpp`** (not in the plan's file list) to assert and
   JSON-override the two new constants and to include them in seed deep-equality, so B2 coverage is
   strengthened rather than regressed.
7. **Fail needs a 6th miss to trigger**: `0.5 - 5*0.1` is `~2.8e-17` (not `<= 0`) in double, so
   tests that must reach failure feed one extra miss; the implementation remains faithful to
   `life <= 0`, and any real miss past the empty display underflows the bar to exact `0.0`.
8. **Headless smoke does not exercise drain**: `App`'s smoke loop paces `on_update` off wall-clock,
   so the demo harness produced 0 judgment events in the run. GameplayView drain/fail/recovery is
   instead proven by `life_keeper_test` tests 15a–15c, which drive `update(fixed_dt)` directly.
9. **B6 duplicates B5's row grouping** (resolved decision 3); noted in `life_keeper.hpp` as a future
   shared-helper refactor.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/life_keeper_test.cpp` | 1 start 0.5; 2a tap table; 2b mine/OK/NG; 3 jump row grouping; 4 clamp; 5 fail-enabled freeze; 6 fail-off recovery; 7 merciful drain; 8 holds/mines individual + neutral events; 9 idempotence; 10 incremental==batch; 11 hot downgrade; 12 combo-to-regain; 13 engine-produced log arithmetic; 14 fail-on vs fail-off through engine; 15a–c GameplayView boundary/outcome; 16 null/empty chart |
| `tests/judgment_constants_test.cpp` | extended defaults parity, seed deep-equality, and JSON override for `hot_downgrade` / `regen_combo_after_miss` |
