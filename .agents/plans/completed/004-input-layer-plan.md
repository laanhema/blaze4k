# Plan: Input Layer with Nanosecond Event Timestamps and Gamepad Hotplug

## Summary

Implement the input subsystem under `src/input/`:
1. `src/input/input_event.hpp`:
   - Enums: `GameAction` (4 rhythm panels: `Left`, `Down`, `Up`, `Right`; menu actions: `Confirm`, `Back`, `MenuUp`, etc.), `DeviceType` (`Keyboard`, `Gamepad`).
   - Struct `InputEvent`: action, pressed state (`bool`), nanosecond timestamp (`uint64_t timestamp_ns` directly from SDL3 event header), device type, device ID, raw input code.
2. `src/input/input_manager.hpp` and `src/input/input_manager.cpp`:
   - Device tracking: SDL3 keyboard and gamepad detection + hotplug (`SDL_EVENT_GAMEPAD_ADDED`, `SDL_EVENT_GAMEPAD_REMOVED`).
   - Default mappings for keyboard (Arrow keys, WASD/DFJK, Enter, Esc) and USB dance pads / gamepads (D-pad and face buttons, Start, Back).
   - Event processing: translates SDL events using native `event.key.timestamp` / `event.gbutton.timestamp` without wall-clock drift.
   - Buffering & querying: frame event queue draining, action pressed state queries (`is_action_down(action)`).
3. Connect `InputManager` to `App` in `src/app/app.cpp` so input is polled before update/render.
4. Unit tests in `tests/input_test.cpp`:
   - Test synthetic SDL3 keyboard and gamepad event translation and nanosecond timestamp retention.
   - Test default mapping resolution for all 4 panels and menu actions.
   - Test action state tracking (press, hold, release).
   - Test gamepad hotplug management.
5. Update `CMakeLists.txt` and validate with CTest.

## User Story

As a pad player,
I want keyboard and USB dance pad input captured with SDL3 nanosecond event timestamps,
So that hit deltas are accurate to the hardware event rather than the frame boundary.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/input/`, `src/app/`, `tests/` |
| GitHub Issue | #4 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/input/input_event.hpp` | CREATE | InputEvent and GameAction data structures |
| `src/input/input_manager.hpp` | CREATE | InputManager interface and mapping tables |
| `src/input/input_manager.cpp` | CREATE | Event processing, timestamp capture, and gamepad hotplug |
| `src/app/app.hpp` | UPDATE | Expose InputManager in App |
| `src/app/app.cpp` | UPDATE | Process input events via InputManager |
| `CMakeLists.txt` | UPDATE | Add input sources to `tundra_core` |
| `tests/input_test.cpp` | CREATE | Unit tests for input mapping, timestamps, and hotplug |
| `tests/CMakeLists.txt` | UPDATE | Add `input_test` executable |

---

## Tasks

### Task 1: Create `src/input/input_event.hpp`
- **File**: `src/input/input_event.hpp`
- **Action**: CREATE
- **Implement**: `GameAction`, `DeviceType`, and `InputEvent` structs.

### Task 2: Create `src/input/input_manager.hpp` and `.cpp`
- **Files**: `src/input/input_manager.hpp`, `src/input/input_manager.cpp`
- **Action**: CREATE
- **Implement**:
  - `InputManager` class with event processor `handle_sdl_event(const SDL_Event& event)`.
  - Capture nanosecond timestamp from `event.key.timestamp` / `event.gbutton.timestamp`.
  - Hotplug handling: `open_gamepad(SDL_JoystickID)`, `close_gamepad(SDL_JoystickID)`.
  - Mapping tables for keyboard keys and gamepad buttons to `GameAction`.
  - Frame queue: `poll_events(std::vector<InputEvent>& out_events)`.
  - State queries: `is_action_down(GameAction action) const`.

### Task 3: Integrate with `App` and `CMakeLists.txt`
- **Files**: `src/app/app.hpp`, `src/app/app.cpp`, `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Forward SDL events to `InputManager` in `App::process_events()`, add input sources to `tundra_core`.

### Task 4: Unit tests in `tests/input_test.cpp`
- **Files**: `tests/input_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**:
  - Test keyboard mapping for all 4 arrow panels and Enter/Escape.
  - Verify exact nanosecond timestamp matches synthetic SDL event timestamp.
  - Test action down / up state tracking.
  - Test custom/remapping ability.
  - Test gamepad connection tracking.

### Task 5: Validate and End-to-End Verification
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `./build/tundra-dance --smoke-test 10`

---

## Validation

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/tundra-dance --smoke-test 10
```

## Acceptance Criteria

- [ ] Keyboard events logged with exact SDL3 nanosecond timestamps
- [ ] Gamepad / USB dance pad button events recognized and timestamped
- [ ] Gamepad hotplug (connect/disconnect) handled cleanly without crashing
- [ ] Polled input event queue exposes action, state, device, and timestamp
- [ ] All unit tests pass
