# Code Review: feature/067-input-remap-device-filter

**Scope**: branch `feature/067-input-remap-device-filter` (vs merge base `399ed27`, including uncommitted changes)
**Recommendation**: APPROVE

## Summary

Reviewed branch `feature/067-input-remap-device-filter` addressing GitHub issue #139, where the Input Remap screen accepted keyboard inputs during a `PAD` row capture and gamepad buttons during a `KEYBOARD` row capture. In `src/screens/input_remap_screen.cpp`, input events during capture are now checked against the active row's device (`model_.rows[model_.row].device`), ignoring mismatched device events so capture remains open until a matching event arrives, while keeping universal cancel shortcuts (`Escape` and pad `Back`) intact.

All acceptance criteria of issue #139 are met:
- **A keyboard key pressed during a PAD row capture does not change the binding, and the capture stays open**: Met. Verified in `test_mismatched_device_filtered` (`tests/input_remap_screen_test.cpp`) and verified live in the real app with `b4k.sh` (`04-after-keyboard-q.png` shows `<PRESS>` remains active, and `config.json` preserves `["dpleft", "x"]`).
- **A pad button pressed during a KEYBOARD row capture does not change the binding, and the capture stays open**: Met. Verified in `test_mismatched_device_filtered` (`tests/input_remap_screen_test.cpp`).
- **Escape and pad Back still cancel a capture on any row**: Met. Verified in `test_mismatched_device_filtered` for both row types and live in the real app (`05-after-escape.png` confirms `dpleft` restored).
- **A unit test covers both mismatched-device cases**: Met. `test_mismatched_device_filtered` covers both keyboard-on-pad and pad-on-keyboard captures, matching device binding completion, and both cancellation paths.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build` (GCC C++20 Linux host) |
| Warnings | NONE | Direct recompile of touched sources (`src/screens/input_remap_screen.cpp`, `tests/input_remap_screen_test.cpp`) with `-Wall -Wextra -Wpedantic -Werror -c -o /dev/null` |
| Lint | PASS | `scripts/lint.sh` (0 rule hits) |
| Format | PASS | `scripts/format.sh --check` |
| Tests | PASS | `scripts/check.sh` (53/53 tests passed on silent null audio device); `input_remap_screen_test` passed under `asan-ubsan` preset |
| UI verification | PASS | `.claude/skills/verify/scripts/b4k.sh` at 1280x720 window; captured `/tmp/blaze4k-verify/review-139/evidence/01-remap-open.png` through `05-after-escape.png` |

## What's Good

- Minimal, clean 7-line filter in `src/screens/input_remap_screen.cpp` with defensive row bounds checking.
- Universal cancel keys (`Escape` and pad `Back`) are checked before the device filter, maintaining intuitive cancellation behavior across both row sections.
- Comprehensive headless unit tests covering both mismatched device directions, valid bindings, and cross-device cancels.

## Recommendation

No changes needed. Ready to commit and merge.
