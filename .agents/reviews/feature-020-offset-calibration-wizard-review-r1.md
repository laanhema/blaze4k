# Code Review (Re-review): feature/020-offset-calibration-wizard

**Scope**: branch `feature/020-offset-calibration-wizard` (HEAD `9f796c3`) vs `main`, committed + uncommitted (working tree clean; `git status --porcelain` empty). Issue #20 — Global offset calibration wizard.
**Prior report**: `.agents/reviews/feature-020-offset-calibration-wizard-review.md`
**Recommendation**: APPROVE

## Summary

All 10 prior findings (2 Medium + 8 Low) are fixed. The two touched areas — `SelectScreen` event handling (Options on the calibrate row, same-batch double-transition guard) and `offset_calibration.cpp` rounding — are correct and introduce no new Critical/High/Medium regressions. Build is clean (forced recompile of the touched TUs, zero warnings) and all 23 tests pass with no skips.

## Fix Verification

| # | Sev | Prior finding | Status | Evidence |
|---|-----|---------------|--------|----------|
| 1 | Medium | `select_screen.cpp` swallowed `GameAction::Options` on the CalibrateOffset row | FIXED | `src/screens/select_screen.cpp:264-273` routes `Options` to apply+close+`options_consumed`, before the catch-all `continue` at :282 |
| 2 | Medium | No test for Select options → `ScreenId::Calibration` seam | FIXED | `tests/select_screen_test.cpp:413-456` `test_calibration_launch_from_options` (Confirm + Right launch, Back aborts with offset untouched, Options closes on row); invoked at :514 |
| 3 | Low | Duplicated `format_offset` / `format_seconds` | FIXED | Single public helper `src/screens/options_menu.cpp:257-262` + declaration `options_menu.hpp:75`; calibration render uses it at `calibration_screen.cpp:174`; no `format_seconds` refs remain |
| 4 | Low | `std::lround` overflow before clamp in `nearest_beat_index` | FIXED | `src/timing/offset_calibration.cpp:35-39` clamps in `double` to `[0, max_beats-1]` before `lround`; test at `offset_calibration_test.cpp:33` |
| 5 | Low | Stale click WAV reused when config changes | FIXED | `src/audio/metronome.cpp:29-64` `click_track_matches` (sample-rate + data-size check) and gated regeneration at `:165` |
| 6 | Low | `write_click_track` checked `out.good()` before flush | FIXED | `src/audio/metronome.cpp:135-136` `out.flush(); return out.good();` |
| 7 | Low | Confirm with null config silently does nothing | FIXED | `src/screens/calibration_screen.cpp:118-119` logs `no config available; offset not saved` |
| 8 | Low | Progress text kept `n / min_samples` after readiness | FIXED | `src/screens/calibration_screen.cpp:164-167` branches on `result_.ready` |
| 9 | Low | Synthetic mode showed a provisional offset readout | FIXED | `src/screens/calibration_screen.cpp:173` `if (result_.ready && !synthetic_)` |
| 10 | Low | Same-batch Confirm/Right could fall through to wheel switch | FIXED | `src/screens/select_screen.cpp:240-242` `options_consumed` guard, set at :271, :276, :314 |

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority
None.

### Suggestions / Low
- **Low — `src/audio/metronome.cpp:24-28` (NEW, comment accuracy)**: The `click_track_matches` comment claims a "size + sample-rate match pins every audible parameter we write", but `click_frequency_hz` and `click_amplitude` do not affect the data size, so a default change to either would still reuse a stale file. This is tone-only (timing/BPM/lead-in/beats are correctly pinned, which is what the prior finding required); the comment is just stronger than the check.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`; touched TUs force-recompiled via `touch`) | PASS — zero warnings/errors |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 23/23, 0 skipped/env-guarded (incl. `select_screen_test`, `offset_calibration_test`, `calibration_screen_test`) |
| Working tree clean (`git status --porcelain`) | PASS — empty |

## Regression Analysis (touched areas)

- **Calibrate-row Options handling (`select_screen.cpp:255-283`)**: Up/Down still move the row; `Options` applies + closes + sets `options_consumed`; Confirm/Right launch and set `options_consumed`; Left/unrelated fall to a no-op `continue`. No path now reaches the wheel switch while the overlay was open in the same batch.
- **`options_consumed` guard (`select_screen.cpp:240-242,271,276,314`)**: Set on every overlay-closing/launch action before `continue`/loop end, so a second qualifying press in the same tick is dropped. Value-row adjustments deliberately do not set it (a batch of repeats still steps once per event, matching existing behavior).
- **Rounding (`offset_calibration.cpp:27-40`)**: `raw` is finite-checked, then `std::clamp`ed in `double`; `std::lround` result is bounded by `max_beats-1`, so the `int` cast is safe. No behavior change for in-range inputs.
- **ScreenManager**: `ScreenId::Calibration` handled in `screen_id_name`, `default_back_navigates`, and `handle_back`; no `-Wswitch` gaps. Back still aborts without writing config.

## What's Good

- The new seam test `test_calibration_launch_from_options` covers both Confirm and Right entry, the Options-toggle close, and abort-retains-offset — exactly the gap the prior review flagged.
- The `click_track_matches` fix correctly derives the expected data size from the same formula `write_click_track` uses, so it cannot drift.
- The rounding fix is defensive without alterating any existing result.

## Recommendation

Approve. All prior findings are resolved and no new Medium-or-higher issues were introduced. The single remaining Low is a comment-accuracy nit and is optional polish.