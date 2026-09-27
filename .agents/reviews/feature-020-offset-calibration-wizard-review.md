# Code Review: feature/020-offset-calibration-wizard

**Scope**: branch `feature/020-offset-calibration-wizard` vs `main`, including uncommitted changes (issue #20 — Global offset calibration wizard)
**Recommendation**: APPROVE (with nits)

## Summary

The change adds a pure offset model (`src/timing/offset_calibration.{hpp,cpp}`), a click-track/metronome wrapper (`src/audio/metronome.{hpp,cpp}`), a new `CalibrationScreen` entered from Select's options menu, a `ScreenId::Calibration` member with Back → Select abort wiring, and two test binaries. AC1–AC4 are all met: the wizard is reachable from the options menu and prompts taps to a steady beat; the average delta is computed through the exact gameplay path (`clock_.time_seconds()` + `music_time_for_event(...)`, no wall-clock/frame delta in the estimator); Confirm writes `ctx.config->offset.global_offset_seconds` (persisted by `main` on clean exit) which flows to the gameplay clock via `gameplay_options_from_config`; and Back aborts without touching the config. Build is clean (no warnings) and all 23 tests pass. Only minor issues found.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority
- **Medium — `src/screens/select_screen.cpp:251-268`**: The calibration-row special case swallows `GameAction::Options` (Tab / shoulder buttons) because that action falls into the final `continue; // Left and unrelated actions: no-op`. On every other row `Options` closes the overlay (`case GameAction::Options: close = true;`), and `InputManager` documents Tab as open/close, so while the CALIBRATE OFFSET row is highlighted the toggle key is dead (Back still closes via `handle_back`). Route `Options` to `close` before the catch-all `continue`.
- **Medium — `tests/select_screen_test.cpp` (new seam untested)**: The only production path that starts the wizard (Select options overlay → `OptionsRow::CalibrateOffset` → `transition_to(ScreenId::Calibration)`) has no test. `test_options_overlay` only navigates rows 0–3 and never reaches row 4. Add a case that Downs to the row, presses Confirm/Right, and asserts `active_id() == ScreenId::Calibration` (and that Back returns to Select with the offset untouched).

### Suggestions / Low
- **Low — `src/screens/options_menu.cpp:257-262` + `src/screens/calibration_screen.cpp:39-44`**: `format_offset` / `format_seconds` are byte-identical copies of the same signed-seconds formatter. Consider sharing one helper.
- **Low — `src/timing/offset_calibration.cpp:32`**: `std::lround` returns `long`; on Windows `long` is 32-bit, so an extreme `music_seconds` (`~1e10`) can overflow before the `std::clamp`. Inputs are clock-derived today, but clamping in `double` first would be safer.
- **Low — `src/audio/metronome.cpp:119-124`**: The click WAV is regenerated only when `exists()` is false, with no check that an existing file matches the requested `MetronomeConfig`. If the defaults (BPM/lead-in/beats) ever change, a stale file is reused and the audible click would no longer line up with the analytic schedule.
- **Low — `src/audio/metronome.cpp:92`**: `write_click_track` returns `out.good()` while the stream is still open; buffered writes are not yet flushed, so a late flush error is not detected. `out.flush()` before the check (or checking the close) would be more robust.
- **Low — `src/screens/calibration_screen.cpp:124-132`**: Confirm with enough samples but `ctx.config == nullptr` silently does nothing (no log, no transition). A one-line warning would match the synthetic-path logging at line 131.
- **Low — `src/screens/calibration_screen.cpp:170-172`**: Progress text keeps rendering `SAMPLES n / min_samples` after readiness, so a fully-collected set (e.g. 32) reads `32 / 8`. Cosmetic.
- **Low — `src/screens/calibration_screen.cpp:92-97,174-177`**: The no-audio synthetic fallback measures deltas against a `fixed_dt`-advanced stub clock (though event aging still uses timestamps) and displays the computed offset. It correctly refuses to save, and the render notice says so, but the provisional number is still shown; consider hiding the offset readout in synthetic mode to avoid implying a usable result.
- **Low — `src/screens/select_screen.cpp:260-267`**: Two events in the same batch that both satisfy the Confirm/Right branch set `options_open_ = false` on the first, so a second event in the same tick falls through to the wheel switch (e.g. could start gameplay). Real input normally yields one press per action per tick; harmless but worth a guard.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS — forced recompile of the new sources, zero warnings/errors |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 23/23, 0 skipped/env-guarded (includes new `offset_calibration_test` and `calibration_screen_test`) |
| Purity (`rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time" src/timing/offset_calibration.*`) | PASS — no matches |

## What's Good

- AC2 wiring is exactly right: `CalibrationScreen::update` samples `clock_.time_seconds()` once per frame and ages every press with `music_time_for_event(event.timestamp_ns, ctx.input_reference_ns, reference_music)` (calibration_screen.cpp:106-121) — the identical call GameplayView uses (gameplay_view.cpp:110-129). No wall-clock or frame delta reaches the estimator.
- Sign convention is pinned and tested both directions: `offset_seconds = -mean(hit - beat)`, matching the documented MusicClock convention (positive = clock later; late taps → negative offset). `offset_calibration_test` covers zero/late/early bias and `test_b1_application` proves config → `gameplay_options_from_config` → `MusicClock` application.
- `OffsetCalibration` is genuinely pure (only `<cstddef>`/`<vector>`/`<algorithm>`/`<cmath>`), never throws, and returns finite zeroed results for empty/degenerate input; wild-tap rejection and MAD outlier pruning are unit-tested.
- Abort semantics are correct and enforced by design: `CalibrationScreen` never overrides `handle_back`, so the manager's `Calibration → Select` path (screen_manager.cpp:149-151) runs `exit()` (metronome stop, clock source cleared) with no config write; `test_abort_retains_previous_offset` verifies the prior value survives.
- New `ScreenId::Calibration` is handled everywhere it must be (`screen_id_name`, `default_back_navigates`, `handle_back`) with no `-Wswitch` gaps.
- Headless safety is real: an unavailable/failed stream collapses to a synthetic clock that refuses to save, and `test_synthetic_refuses_to_save` exercises it; rendering is a no-op-capable path and re-entry resets all state.

## Recommendation

Approve. Two medium items are worth addressing before merge if convenient: the `Options`-key swallow on the calibration row (one-line fix) and a test for the Select → Calibration navigation seam. Everything else is optional polish. No correctness or AC blockers.