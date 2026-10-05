# Implementation Report

**Plan**: `.agents/plans/completed/054-calibration-wide-delay-window-plan.md`
**Branch**: `feature/054-calibration-wide-delay-window`
**Status**: COMPLETE (owner hardware checks pending)

## Summary

Fixes #74: the calibration wizard no longer saves a wrong positive offset when the total delay is above 250 ms.

- Each tap is now paired with the **most recent beat** (it may be up to 50 ms early). Before, it was paired with the nearest beat (`CalibrationConfig::matching_beat_index` replaces `nearest_beat_index`).
- `OffsetCalibration::result()` first finds the tap cluster's circular centre. It then moves each delta by whole beat periods to sit next to that centre, and only then runs the unchanged median/MAD/mean/stddev step. Deltas that already sit next to the centre are not changed.
- The supported measured delay is **−50 ms to +420 ms** (`max_early_seconds = 0.05`, `max_late_seconds = 0.42`, replacing `max_abs_delta_seconds = 0.25`). A result outside that range sets `CalibrationResult::out_of_range` and is never `ready`.
- The screen has a new `CalibrationPhase::OutOfRange`. When it is reached, the screen:
  - shows `OUT OF RANGE` and the red notice `DELAY OUT OF RANGE - OFFSET WILL NOT BE SAVED`;
  - shows `---` for the offset and the no-save hint bar;
  - logs one readable `[Calibration] measured delay +NNN ms is outside the supported range (-50..+420 ms); offset not saved` line;
  - refuses Confirm.
- The phase is derived on every update, not sticky: more taps can bring it back to `Ready`.
- `docs/AUDIO_LATENCY.md`: rewrote the summary bullet and the "Wizard headroom" section, documented the >450 ms alias limit and the possible slower extended-range follow-up (owner decision on OQ1), and refreshed the code citations.

The music clock and input aging are unchanged.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure model: matching index, slot acceptance, circular-centre unwrap, range flag | `src/timing/offset_calibration.{hpp,cpp}` | ✅ |
| 2 | Model tests (rewrote `test_schedule` index lines + 7 new cases) | `tests/offset_calibration_test.cpp` | ✅ |
| 3 | Screen wiring: matching call, Confirm gate on result, derived phase, log line, render | `src/screens/calibration_screen.{hpp,cpp}` | ✅ |
| 4 | Screen tests (large delay saves, out-of-range refuses, recovers) | `tests/calibration_screen_test.cpp` | ✅ |
| 5 | `kOutOfRangeNotice`, `CalibrationView::out_of_range`, notice draw + art tests | `src/screens/setup_art.{hpp,cpp}`, `tests/setup_art_test.cpp` | ✅ |
| 6 | Docs (summary, citations, Wizard headroom, alias limit) | `docs/AUDIO_LATENCY.md` | ✅ |
| 7 | Full validation | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (Release, `cmake --build build -j$(nproc)`) | ✅ 0 warnings in the full build log after touching all changed TUs |
| Build (Debug, `build-debug`) | ✅ (only CMake's pre-existing developer notice) |
| Lint (no linter; zero-new-warnings gate) | ✅ "no new warnings" |
| Tests, Release (sandboxed ctest) | ✅ 51/51 passed |
| Tests, Debug (sandboxed ctest) | ✅ 51/51 passed |
| Static: `nearest_beat_index` / `max_abs_delta_seconds` in src/tests/docs | ✅ none |
| Static: header includes only `<cstddef>/<vector>` | ✅ |
| Static: `SDL\|chrono` in `offset_calibration.*` | ✅ only the pre-existing purity comment "no SDL/GL/audio/clock" (no include or call) |
| Static: `todo-stories.md` not staged, still ` M` | ✅ |
| E2E 1: automated AC1/AC2/AC3 through model + real screen path | ✅ |
| E2E 2: headless smoke (`--headless --smoke-test 5 --start-screen select`, scratch data dir, sandboxed) | ✅ "Blaze 4k shut down cleanly." |
| E2E 3a–d: owner hardware checks (wired, WH-1000XM4, deliberate out-of-range demo, optional HFP) | ⏳ owner-only, pending |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/offset_calibration.hpp` | UPDATE | +19/-7 |
| `src/timing/offset_calibration.cpp` | UPDATE | +39/-8 |
| `src/screens/calibration_screen.hpp` | UPDATE | +4/-1 |
| `src/screens/calibration_screen.cpp` | UPDATE | +39/-8 |
| `src/screens/setup_art.hpp` | UPDATE | +5/-1 |
| `src/screens/setup_art.cpp` | UPDATE | +5/-2 |
| `tests/offset_calibration_test.cpp` | UPDATE | +147/-6 |
| `tests/calibration_screen_test.cpp` | UPDATE | +69/-0 |
| `tests/setup_art_test.cpp` | UPDATE | +29/-0 |
| `docs/AUDIO_LATENCY.md` | UPDATE | +31/-12 |

## Deviations from Plan

1. **`test_unwrap_is_identity_in_range` delta set.** I first used an evenly spread set over `[−0.04, 0.40]` (`−0.04, 0.00, 0.05, …, 0.40`). That set covers 88% of the 0.5 s circle, so 0.40 and −0.04 are only 60 ms apart around it. Its circular centre is about 0.098, and 0.40 then correctly unwraps to −0.10, so the 1e-12 check failed. The test now uses `{−0.04, 0.04, 0.08, 0.13, 0.18, 0.23, 0.28, 0.32, 0.40}`. It still spans `[−0.04, 0.40]`, and every delta is within half a period of the centre (0.18), which is what the test is meant to show. The model code did not change.
2. **Static `SDL|chrono` grep** matches the existing purity comment in `offset_calibration.hpp` ("no SDL/GL/audio/clock/wall-clock"). There is no include or call, so the purity intent holds.
3. **Confirm when out of range** logs `[Calibration] delay out of range; offset not saved` once per Confirm press, as the plan says. This is in addition to the one-time phase-entry log line.
4. **Branch creation with a dirty tree.** `main` had an unrelated modification to `.agents/stories/todo-stories.md`. As the invoking request said, the branch was created with that change carried over, untouched and unstaged.
5. **Recovery screen test** reuses a small `check_out_of_range_refuses` helper, so case 3 starts from exactly case 2's state.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/offset_calibration_test.cpp` | `test_schedule` (matching index rewritten: early/late slot edges, clamps, NaN), `test_config_invariants`, `test_large_late_delays` (AC1 0.28/0.40), `test_low_latency_unchanged` (AC2 +0.02/−0.03), `test_cluster_straddles_slot_edge` (0.38/0.41; −0.02/−0.06), `test_out_of_range` (0.44; 0.43/0.46; −0.07), `test_mistap_far_from_cluster`, `test_unwrap_is_identity_in_range` |
| `tests/calibration_screen_test.cpp` | `test_large_delay_saves_negative_offset` (0.40, 0.28 → saved −0.40/−0.28 + Select), `test_out_of_range_refuses_to_save` (phase, flags, Confirm no-save, config 0.123 kept, headless render), `test_out_of_range_recovers` (24 × +0.03 → Ready, −0.03, 8 outliers) |
| `tests/setup_art_test.cpp` | Width budget for `kOutOfRangeNotice` and "OUT OF RANGE", out-of-range render scenario with real/null services at every size |
