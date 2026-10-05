# Implementation Report

**Plan**: `.agents/plans/completed/051-hide-cursor-gameplay-plan.md`
**Branch**: `feature/051-hide-cursor-gameplay`
**Status**: COMPLETE (the owner's windowed GUI check is still pending)

## Summary

The OS mouse cursor is now hidden while the gameplay screen is active. It comes back on every way out of gameplay.

- **Window wrapper.** `Window::set_cursor_visible(bool)` in `src/app/window.{hpp,cpp}` wraps SDL3's `SDL_HideCursor()` / `SDL_ShowCursor()`.
  - It does nothing when there is no SDL window: in headless mode, before `init()` and after `shutdown()`.
  - It is idempotent.
  - It records its own state in `cursor_hidden_`, which the move operations carry over.
  - A failed SDL call is logged with the `[Window]` prefix and is never fatal.
- **Shutdown.** `Window::shutdown()` shows the cursor again before it destroys the window. No screen `exit()` runs when the app quits, so this covers closing mid-song and the `--gameplay-demo` path.
- **Screen hook.** Screens reach the wrapper through a new nullable `ScreenContext::set_cursor_visible` callback, which `main` wires. `src/screens/` still contains no SDL code.
- **Gameplay.** `GameplayScreen::enter` hides the cursor as its first statement, even when there is no play request. `exit` shows it again. Both calls check for a null callback first.
- **Demo harness.** `--gameplay-demo` hides the cursor once `gameplay.init` succeeds.

Nothing in the timing, audio, input or gameplay path changed.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Window cursor wrapper, shutdown restore, move semantics | `src/app/window.hpp`, `src/app/window.cpp` | ✅ |
| 2 | `ScreenContext::set_cursor_visible` service | `src/screens/screen.hpp` | ✅ |
| 3 | Gameplay hides the cursor on enter and shows it on exit, plus the class comment | `src/screens/gameplay_screen.cpp`, `src/screens/gameplay_screen.hpp` | ✅ |
| 4 | Wire the shell context and hide the cursor in `--gameplay-demo` | `src/main.cpp` | ✅ |
| 5 | Tests | `tests/gameplay_screen_test.cpp`, `tests/app_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | ✅ 0 warnings in the whole log, after touching the changed source files to force a rebuild |
| Lint (zero new warnings in the touched files) | ✅ "no new warnings" |
| Tests (sandboxed `ctest`) | ✅ 49/49 passed |
| Static: `SDL_HideCursor` / `SDL_ShowCursor` appear only in `src/app/window.cpp` | ✅ (`window.cpp:171`) |
| Static: no SDL in the screen files | ✅ (narrower variant; see Deviations) |
| Static: every `set_cursor_visible` call in gameplay is null-guarded | ✅ (`gameplay_screen.cpp:31-32`, `:131-132`) |
| Static: no changes under `src/timing`, `src/audio`, `src/input`, `src/gameplay` or `assets` | ✅ empty |
| Static: `todo-stories.md` is not staged | ✅ 0 |
| E2E 1: `gameplay_screen_test` and `app_test` under the sandboxed ctest | ✅ |
| E2E 2: headless smoke run in `bwrap` with a scratch `--data-dir` | ✅ Exit code 0. Printed `Blaze 4k shut down cleanly.` and no cursor log line |
| E2E 3: windowed and fullscreen check on Linux, Windows and macOS (owner only) | ⏳ pending for the owner |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/app/window.hpp` | UPDATE | +7/-0 |
| `src/app/window.cpp` | UPDATE | +27/-1 |
| `src/screens/screen.hpp` | UPDATE | +5/-0 |
| `src/screens/gameplay_screen.hpp` | UPDATE | +3/-0 |
| `src/screens/gameplay_screen.cpp` | UPDATE | +10/-1 |
| `src/main.cpp` | UPDATE | +6/-0 |
| `tests/gameplay_screen_test.cpp` | UPDATE | +110/-0 |
| `tests/app_test.cpp` | UPDATE | +23/-0 |

## Deviations from Plan

1. **SDL static check made narrower.** The plan's `grep -n "SDL" src/screens/screen.hpp src/screens/gameplay_screen.*` matches two comments in `screen.hpp` that were already there ("free of SDL/GL" at line 34 and "stay free of SDL" at line 65). I ran a narrower check that looks only for SDL includes and calls: `grep -nE "#include.*SDL|SDL_[A-Za-z]+\(" ...`. It found nothing. The comments were left as they are.
2. **Extra update in run-end case 1.** That case adds one `manager.update(0.0, {})` after the end-delay updates. It applies any transition that is still waiting, which keeps the test independent of whether `ScreenManager::update` applies the transition in the same tick. The assertions are the ones the plan asks for.
3. **Branch created from a non-clean `main`.** `main` had an unrelated uncommitted change to `.agents/stories/todo-stories.md`. Because the invoking request said to create the branch anyway, I created it with that change carried along, untouched and unstaged.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/gameplay_screen_test.cpp` | `test_cursor_hidden_during_gameplay`, covering: run end → Results (`{false, true}`, which also covers the fail path through the same transition line); Back → Select; leaving, re-entering and leaving again, where the calls strictly alternate `{false, true, false, true}`; no play request (`{false, true}`); a null callback (no crash). Adds a `press()` helper. |
| `tests/app_test.cpp` | Section 6: on a headless `Window`, `set_cursor_visible` does nothing before `init()`, after `init()`, after a move and after `shutdown()`. `cursor_hidden()` stays `false` throughout. |
