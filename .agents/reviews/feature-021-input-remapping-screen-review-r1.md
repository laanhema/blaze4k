# Code Review (Re-review r1): feature/021-input-remapping-screen (C6 Input Remapping Screen)

**Scope**: Branch `feature/021-input-remapping-screen` vs `main`, including uncommitted working-tree changes (GitHub issue #21)
**Recommendation**: APPROVE WITH NITS

## Summary

The fix pass resolves all six findings from the prior review. `InputManager::apply_bindings()` now iterates the compiled default actions and merges each persisted per-action override over them, so legacy/partial configs keep arrows, DFJK, Tab, and gamepad panels instead of being silently unbound (verified by code inspection and by the new `input_test` cases 8/8b). The remap screen now renders through a sliding window that keeps the selected binding and the trailing RESET row on-screen at every resolution tested (the row count is resolution-independent at 15 visible), and the remaining low findings are addressed. No new regressions were found in the rewritten merge logic or the scrolling render window.

## Prior Findings Verification

| # | Severity | Finding | Status | Evidence |
|---|----------|---------|--------|----------|
| 1 | High | `apply_bindings` must map actions absent from persisted lists to compiled defaults | **RESOLVED** | `src/input/input_manager.cpp:48-104` iterates `defaults`, picks the matching persisted override (or the default when absent), and falls back to the compiled names when the override yields no parseable name. `tests/input_test.cpp:234-285` (case 8b) asserts a legacy `{Confirm, Back}`-only config still resolves arrows/DFJK/Tab/panels. |
| 2 | Medium | All 24 binding rows + RESET visible/reachable | **RESOLVED** | `src/screens/input_remap_screen.cpp:170-204` adds a scroll window (`visible_rows` = 15 at all tested resolutions, `first_row`/`last_row` clamped so the selection and RESET row are always drawn). |
| 3 | Low | Move ctor/assignment carry `capture_mode_` | **RESOLVED** | `src/input/input_manager.cpp:123` (ctor) and `:137` (assignment). |
| 4 | Low | Select→InputRemap launch-seam test exists | **RESOLVED** | `tests/select_screen_test.cpp:459-503` (`test_remap_launch_from_options`) asserts Confirm and Right both transition to `ScreenId::InputRemap` and that Back/Options behave. |
| 5 | Low | Empty name-list override falls back to defaults in the UI model | **RESOLVED** | `src/screens/input_remap.cpp:16` only applies an override when `!override_binding.second.empty()`; covered by `tests/input_remap_test.cpp:90-95`. |
| 6 | Low | Stale conflict message cleared on row navigation | **RESOLVED** | `src/screens/input_remap_screen.cpp:70` (Up) and `:78` (Down) call `model_.message.clear()`. |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions

1. **`src/screens/input_remap.cpp:15-20` vs `src/input/input_manager.cpp:74-102`** — The UI model and the runtime disagree when a persisted override is non-empty but no name parses (e.g. a hand-edited `"Confirm": ["Bogus"]`): the screen shows `Bogus`, while `apply_bindings` silently falls back to the compiled default (`Return`/`Keypad Enter`). Not reachable through the screen (capture always emits canonical SDL names) and not a regression from the fix, but the fix's comment claims the two "agree" — they agree only for empty/absent overrides. Consider making `append_rows` skip unparseable names for exact parity, or softening the comment.

2. **`src/screens/select_screen.cpp:255-313`** — The `CalibrateOffset` and `RemapInput` action-row blocks are near-verbatim duplicates (~28 lines). Pre-existing pattern (the calibration block already existed), so not a regression; a small helper taking the target `ScreenId` would remove the drift risk when a third action row is added.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build | PASS (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`, 0 warnings) |
| Lint | N/A (no linter configured; `-Wall -Wextra -Wpedantic` clean) |
| Tests | PASS (25/25, `ctest --test-dir build --output-on-failure`) |
| High finding regression guard (partial/legacy config) | PASS (`input_test` case 8b) |
| Merge-over-defaults + reserved Escape/pad-Back | PASS (`input_test` 8/8b, `input_remap_screen_test` 7/8) |
| Scroll window / render smoke | PASS (`input_remap_screen_test` 9 headless render at 1280x720 and 0x0) |
| Move ctor/assignment capture state | PASS (code inspection; latent path not exercised by a test) |
| Capture timestamps preserved verbatim | PASS (`input_test` case 9, `event.key.timestamp` / `event.gbutton.timestamp`) |

## What's Good

- The High finding's fix is correct and, importantly, tested at the boundary that mattered: a config that omits actions entirely now backfills each absent action from the compiled default while still honoring present overrides — including the `any_valid` fallback for an action whose only persisted name fails to parse.
- The render window is resolution-independent (the viewport/row-height ratio is constant), so the fix does not just work at one window size; `first_row` clamping provably keeps the selected row (and RESET) visible at both scroll extremes.
- Persisted defaults were brought in line with the runtime map (drift fix), and the config-persistence test was updated to assert the 7/7 authority including `Options`/`Back`.
- Timestamps remain sacred on the capture path; repeat keys are still filtered before the capture branch (`input_manager.cpp:263-279`), and pad-Back hold synthesis is bypassed while capturing.

## Recommendation

Approve with nits. All prior findings are resolved and no new functional regressions were found. The two suggestions are optional code-quality items, not blockers; the latent move-constructor capture-state path remains untested but is now correct by inspection.
