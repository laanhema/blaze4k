# Implementation Report

**Plan**: `.agents/plans/012-judgment-engine-event-log-plan.md`
**Branch**: `feature/012-judgment-engine-event-log`
**Status**: COMPLETE

## Summary

Implemented Tundra's judgment core as a pure, event-sourced engine. Every hit is
evaluated against the B2 `JudgmentConstants` windows and appended as an immutable
`JudgmentEvent {column, note_time, hit_time, delta_ms, window}`. The work comprises
three pure modules (`judgment.hpp`, `judgment_input.hpp`, `judgment_engine.{hpp,cpp}`)
plus thin integration into `GameplayView`, `App`, and `main.cpp`. The engine is
time-parameterized (callers pass absolute music time), reads no clock, and touches no
SDL/GL/audio/`<chrono>`. It emits no scoring/combo/life/DP state — that is B5/B6.

Resolved decisions applied: hidden-note set + exposed `latest_event()`/`events()` (no
placeholder judgment text); `RollHit`/`AvoidedMine` logged with no score/combo effect;
per-note events (no row aggregation); deterministic Miss `hit_time = note_time + way_off`;
`SDL_GetTicksNS()` captured once after the poll loop; `GameplayView::init` takes a
`constants` parameter and `update` takes `held_columns`.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Event model | `src/gameplay/judgment.hpp` | ✅ |
| 2 | Input timestamp → music time | `src/gameplay/judgment_input.hpp` | ✅ |
| 3 | Engine interface | `src/gameplay/judgment_engine.hpp` | ✅ |
| 4 | Engine reset + step path | `src/gameplay/judgment_engine.cpp` | ✅ |
| 5 | Engine update path (expiry/holds/rolls/mines) | `src/gameplay/judgment_engine.cpp` | ✅ |
| 6 | GameplayView integration | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| 7 | App + main input/time wiring | `src/app/app.{hpp,cpp}`, `src/main.cpp` | ✅ |
| 8 | Register sources + test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 9 | Test suite | `tests/judgment_engine_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Release build (`cmake --build build -j16`) | ✅ |
| Zero warnings under `-Wall -Wextra -Wpedantic` | ✅ (touch + rebuild, no warning/error lines) |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 12/12 (0.21s) |
| Explicit `./build/tests/judgment_engine_test` | ✅ all 15 sub-checks print pass |
| Purity: no SDL/GL/`<chrono>` in judgment core | ✅ (only comments match; no includes) |
| No frame-delta/wall-clock in `judgment_engine.cpp` | ✅ (`rg` for `fixed_dt|delta_time|frame` → no matches) |
| Headless smoke (`--headless --gameplay-demo … --smoke-test 120`) | ✅ exit 0, `[GameplayView] Session judgment events: 0`, no GL |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/judgment.hpp` | CREATE | +44 |
| `src/gameplay/judgment_input.hpp` | CREATE | +20 |
| `src/gameplay/judgment_engine.hpp` | CREATE | +71 |
| `src/gameplay/judgment_engine.cpp` | CREATE | +394 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +17/-3 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +79/-19 |
| `src/app/app.hpp` | UPDATE | +2 |
| `src/app/app.cpp` | UPDATE | +3 |
| `src/main.cpp` | UPDATE | +15/-3 |
| `CMakeLists.txt` | UPDATE | +1 |
| `tests/CMakeLists.txt` | UPDATE | +11 |
| `tests/judgment_engine_test.cpp` | CREATE | +457 |

## Deviations from Plan

1. **Expiry/crossing use full per-column scans, not monotonic cursors.** The
   approved engine interface has no cursor state; scanning each column's note list
   and skipping graded/complete notes is semantically identical. O(notes) per frame
   is negligible for v1 and was flagged as deferred optimization in the plan's Risks.
2. **`AvoidedMine` event fields.** The plan did not pin `hit_time`/`delta_ms` for
   avoided mines; used `note_time + way_off` / `way_off*1000` for the same
   determinism convention as Miss.
3. **Test 1 uses window midpoints instead of exact boundaries.** Hitting exactly at a
   window boundary is fragile in double precision (`note_time + window - note_time`
   can drift above the stored window). Midpoints still assert each classification;
   the no-event case uses `way_off + 0.01`.
4. **Test 11 clamps each step to the exact hold end time.** Accumulating `0.01` 50
   times can land just short of the end and miss the OK edge; clamping keeps the
   coarse (0.1) and fine (0.01) grids directly comparable while preserving the
   frame-rate-independence intent.
5. **E2E step 4 (on-display manual press) not executable headless.** The on-screen
   path is covered by unit tests 5-8/10/13 plus the headless smoke run; the manual
   display check remains to be done on a workstation.
6. **Added `#include <array>`** to `gameplay_view.cpp`/`main.cpp` and `<algorithm>`
   to the test (implied by the new signatures/`std::min`).

## Tests Written

| Test File | Test Cases |
|-----------|-----------|
| `tests/judgment_engine_test.cpp` | 15: tap classification + exact event fields; delta sign; Miss expiry; mine hit/avoid; hold OK; hold NG; missed hold head; roll re-hit + RollOk/RollNg; closest-note selection + tie-break; held-over-mine; frame-rate independence; append-only log + drain + latest_event; visual/log consistency; `music_time_for_event`; null/empty chart |

## End-to-End Verification

1. ✅ `./build/tests/judgment_engine_test` — all sub-checks pass (tap/sign/Miss vs B2
   windows; hold/roll/mine vs pinned OpenITG semantics; frame-rate independence).
2. ✅ `ctest` → 12/12; all 11 prior tests stay green.
3. ✅ `--headless --gameplay-demo … --smoke-test 120` exits cleanly; reports 0 events
   (no input headless); no GL attempted.
4. ⚠️ Manual on-display input check deferred to a workstation (see Deviation 5).
5. ✅ Purity and frame-delta `rg` checks pass; the only wall-clock read
   (`SDL_GetTicksNS`) is in `App::process_events`, used solely to age input events.
