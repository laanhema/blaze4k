# Plan: Ignore mismatched-device inputs during input remap capture (#139)

## Summary

On the Input Remap screen, a capture started on a `PAD` row currently accepts keyboard key presses and assigns their names to gamepad bindings, while a capture on a `KEYBOARD` row would accept pad buttons. This change updates the capture loop in `src/screens/input_remap_screen.cpp` so that inputs from a mismatched device are ignored and the capture stays open until an input from the matching device arrives, while preserving Escape and pad Back as universal cancellation shortcuts across both row types. Comprehensive unit tests are added to `tests/input_remap_screen_test.cpp` to verify that mismatched inputs do not alter bindings or close capture, and that cancellation works universally.

## User Story

As a player remapping inputs with a keyboard or USB dance pad
I want key presses to be ignored when capturing a dance pad row and pad buttons to be ignored when capturing a keyboard row
So that my bindings are not corrupted by inadvertent presses from the wrong device

## Metadata

| Field | Value |
|---|---|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | Input remapping UI (`src/screens/input_remap_screen.cpp`, `tests/input_remap_screen_test.cpp`) |
| GitHub Issue | #139 |
| PRD Phase | N/A |
| Prototype Patch | N/A |
| Branch (suggested) | `feature/067-input-remap-device-filter` |
| Plan sequence | 067 |

---

## Files to Change

| File | Action | Purpose |
|---|---|---|
| `src/screens/input_remap_screen.cpp` | UPDATE | Ignore inputs from mismatched devices during capture after checking universal cancel keys |
| `tests/input_remap_screen_test.cpp` | UPDATE | Add tests for mismatched-device input filtering and cross-device cancel behavior |

---

## Tasks

### Task 1: Filter mismatched device events during capture

- **File**: `src/screens/input_remap_screen.cpp`
- **Action**: UPDATE
- **Implement**: In `InputRemapScreen::update`, after checking `event.raw_code == SDLK_ESCAPE` (for Keyboard) and `event.raw_code == SDL_GAMEPAD_BUTTON_BACK` (for Gamepad) which cancel capture, verify that the current row index is valid and that `event.device == model_.rows[model_.row].device`. If `event.device` does not match the active row's device, `continue` to ignore the input so capture remains open.
- **Mirror**: `src/screens/input_remap_screen.cpp:98-140`
- **Validate**: `cmake --build build`

### Task 2: Add unit tests for device filtering and cancellation

- **File**: `tests/input_remap_screen_test.cpp`
- **Action**: UPDATE
- **Implement**: Add tests verifying:
  1. Keyboard key press on a PAD row does not alter the binding and leaves capture open; subsequent pad button press binds successfully and closes capture.
  2. Gamepad button press on a KEYBOARD row does not alter the binding and leaves capture open; subsequent keyboard key press binds successfully and closes capture.
  3. Keyboard Escape cancels capture on a PAD row.
  4. Pad Back cancels capture on a KEYBOARD row.
- **Mirror**: `tests/input_remap_screen_test.cpp:121-158`
- **Validate**: `ctest --test-dir build --output-on-failure -R input_remap_screen_test`

---

## Validation

```bash
# Type check / Build
cmake --build build

# Lint & Format
scripts/lint.sh
scripts/format.sh --check

# Tests
ctest --test-dir build --output-on-failure

# Reproduction (BUG_FIX only): red on unfixed code, green after fix
ctest --test-dir build --output-on-failure -R input_remap_screen_test
```

## End-to-End Verification

1. Run `ctest --test-dir build --output-on-failure -R input_remap_screen_test` to confirm the new unit tests for device mismatch pass.
2. Run `scripts/check.sh` to ensure linting, line formatting, warnings-as-errors build, and the full test suite pass.

## Acceptance Criteria

- [ ] A keyboard key pressed during a `PAD` row capture does not change the binding, and the capture stays open
- [ ] A pad button pressed during a `KEYBOARD` row capture does not change the binding, and the capture stays open
- [ ] Escape and pad Back still cancel a capture on any row
- [ ] A unit test covers both mismatched-device cases
- [ ] All tests in `scripts/check.sh` pass
