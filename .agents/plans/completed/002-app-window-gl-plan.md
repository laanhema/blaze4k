# Plan: Application Window, GL Context, and Main Loop

## Summary

Build the application runtime foundation for Blaze 4k:
1. `src/app/window.hpp` & `src/app/window.cpp`: Encapsulate SDL3 window creation, OpenGL 3.3 Core context initialization with glad loader, swap interval (vsync), and viewport resize management.
2. `src/app/app.hpp` & `src/app/app.cpp`: Fixed-timestep accumulator main loop (`kFixedTimestep = 1.0 / 60.0` s), frame pacing, event pump, clean shutdown on quit/Escape, and extensible update/render hooks. Provide `--headless` or `--smoke-test` (run N frames and exit) flags for headless testing.
3. Update `src/main.cpp` to launch the application.
4. Unit tests in `tests/` verifying the fixed-timestep accumulator logic and window configuration options.

## User Story

As a developer,
I want a bootable app with an SDL3 window, OpenGL 3.3 context, and a fixed-timestep main loop with vsync,
So that all gameplay and screens have a stable frame foundation.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/app/`, `src/main.cpp`, `tests/` |
| GitHub Issue | #2 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/app/window.hpp` | CREATE | Window and GL context wrapper header |
| `src/app/window.cpp` | CREATE | SDL3 window, GL 3.3 context, viewport management implementation |
| `src/app/app.hpp` | CREATE | Application class and fixed-timestep accumulator loop header |
| `src/app/app.cpp` | CREATE | App event handling, fixed update tick, and render loop implementation |
| `src/main.cpp` | UPDATE | Initialize and run App instance |
| `CMakeLists.txt` | UPDATE | Add `src/app/window.cpp` and `src/app/app.cpp` to blaze-4k target |
| `tests/app_test.cpp` | CREATE | Unit tests for timestep accumulator and app configuration |
| `tests/CMakeLists.txt` | UPDATE | Add `app_test` executable |

---

## Tasks

### Task 1: Implement `Window` abstraction
- **Files**: `src/app/window.hpp`, `src/app/window.cpp`
- **Action**: CREATE
- **Implement**:
  - Class `Window` wrapping `SDL_Window*` and `SDL_GLContext`.
  - Config: title, width (default 1280), height (default 720), vsync (true/false).
  - GL 3.3 Core profile attributes (`SDL_GL_CONTEXT_MAJOR_VERSION = 3`, `SDL_GL_CONTEXT_MINOR_VERSION = 3`, `SDL_GL_CONTEXT_PROFILE_CORE`).
  - Initialize `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)` when context is created.
  - Viewport update function `on_resize(int width, int height)` calling `glViewport(0, 0, width, height)`.
  - Clean destructor releasing GL context and SDL window.

### Task 2: Implement `App` with fixed-timestep accumulator
- **Files**: `src/app/app.hpp`, `src/app/app.cpp`
- **Action**: CREATE
- **Implement**:
  - `AppConfig`: `width`, `height`, `title`, `vsync`, `fixed_dt` (default 1/60s), `headless` mode, `smoke_test_frames` (optional auto-exit after N frames).
  - Main loop using SDL3 `SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()` for precise time deltas.
  - Timestep accumulator pattern (`accumulator += frame_dt; while (accumulator >= fixed_dt) { update(fixed_dt); accumulator -= fixed_dt; }`). Clamp max frame delta to prevent spiral of death.
  - Event loop handling `SDL_EVENT_QUIT`, `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, `SDL_EVENT_KEY_DOWN` (Escape to quit), and `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`.
  - Rendering callback: clear color (`glClearColor(0.05f, 0.05f, 0.08f, 1.0f)`), clear buffers (`glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`), and `window.swap_buffers()`.

### Task 3: Update `src/main.cpp` and `CMakeLists.txt`
- **Files**: `src/main.cpp`, `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Parse command-line flags (e.g. `--smoke-test <frames>`, `--headless`), instantiate `App`, and run it.

### Task 4: Unit tests for timestep accumulator in `tests/app_test.cpp`
- **Files**: `tests/app_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**: Test accumulator tick counts over various frame deltas (exact, sub-step, multi-step, spiral-of-death clamp).

### Task 5: Validate and End-to-End Verification
- Build project: `cmake --build build`
- Run CTest: `ctest --test-dir build --output-on-failure`
- Run smoke test: `./build/blaze-4k --smoke-test 10`

---

## Validation

```bash
cmake --build build
ctest --test-dir build --output-on-failure
./build/blaze-4k --smoke-test 10
```

## Acceptance Criteria

- [ ] SDL3 window opens with GL 3.3 Core context (via glad)
- [ ] Fixed-timestep accumulator main loop with vsync
- [ ] Clean shutdown on window close or Escape
- [ ] GL viewport updates on resize
- [ ] All unit tests pass and smoke test completes cleanly
