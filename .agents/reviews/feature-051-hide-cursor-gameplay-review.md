# Code Review: feature/051-hide-cursor-gameplay

**Scope**: Branch `feature/051-hide-cursor-gameplay` vs `main` (no commits yet; all changes uncommitted), for issue #112. Files: `src/app/window.{hpp,cpp}`, `src/screens/screen.hpp`, `src/screens/gameplay_screen.{hpp,cpp}`, `src/main.cpp`, `tests/app_test.cpp`, `tests/gameplay_screen_test.cpp`, plus the untracked plan and implementation report. `.agents/stories/todo-stories.md` is an unrelated owner edit and was excluded.
**Recommendation**: APPROVE WITH NITS

## Summary

The change hides the OS cursor while `GameplayScreen` is active. It goes through a nullable `ScreenContext::set_cursor_visible` callback that `main` wires to a new `Window::set_cursor_visible` wrapper around SDL3's `SDL_HideCursor` / `SDL_ShowCursor`. `Window::shutdown()` restores the cursor because no screen `exit()` runs when the app quits. The implementation is small and correct, follows the existing `action_down` service pattern, keeps SDL out of `src/screens/`, and doesn't touch the timing or judgment path. The only findings are two Low test nits.

## Verification of key claims

| Claim | Verified against | Result |
|-------|------------------|--------|
| `SDL_ShowCursor` / `SDL_HideCursor` return `bool` in SDL3 | `build/_deps/sdl3-src/include/SDL3/SDL_mouse.h:651,666` | Correct. `const bool ok = ...` matches |
| `ScreenManager` always calls `exit()` before `enter()`; a same-id transition is a no-op | `src/screens/screen_manager.cpp:79-142` (`start` and `apply_pending`) | Correct. Hide/show strictly alternate |
| No screen `exit()` runs at shutdown, so `Window::shutdown()` must restore the cursor | `ScreenManager` has no destructor that calls `exit()`; `~App` (`src/app/app.cpp:12-21`) calls `window_.shutdown()` before `SDL_Quit()` | Correct |
| The lambda captures `&app` safely | `shell` is declared after `app` (`src/main.cpp:257,270`), so it is destroyed first, and nothing calls `exit()` after `app.run()` | Safe |
| Move assignment restores this object's cursor before it is overwritten | `src/app/window.cpp:30` calls `shutdown()` first | Correct |
| Headless never calls SDL mouse APIs | `window_` stays `nullptr` on both headless paths (`window.cpp:52-67`), and `set_cursor_visible` guards on it (`:165`) | Correct |
| Gameplay is entered only through the ScreenManager | Only `select_screen.cpp:500` calls `transition_to(ScreenId::Gameplay)`. Calibration and Attract don't host `GameplayScreen` | Correct |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`tests/gameplay_screen_test.cpp:125,148,175,192`: the case numbering jumps from 1 to 3.** The block comments use the plan's case numbers (1, 3+4, 5, 6). Case 2 (fail → Results) only exists as a note inside case 1. Someone reading the test alone will think a case is missing.
   *Recommendation:* renumber to 1–5, or label the note in case 1 as "(covers 2: fail → Results)".

2. **`tests/app_test.cpp:137-139`: the "after a move" check can't fail.** On a headless `Window`, `cursor_hidden_` can never become `true`, so `TEST_CHECK(!moved.cursor_hidden())` passes whether or not the move constructor carries `cursor_hidden_` (`src/app/window.cpp:21,25,37,42`). This only shows that a move doesn't crash. The implementation report lists it as coverage "after a move", which is more than it proves. A unit test can't exercise this without SDL video, so the move logic is checked by reading the code only.
   *Recommendation:* add a one-line comment saying the move check only shows that a headless move doesn't crash. Don't add test-only hooks to production code.

**Noted, not a finding:**
- Windowed and fullscreen behaviour on Linux, Windows and macOS (AC 1 and AC 5) is the owner's GUI check. Agents must not launch the GUI.
- The fail → Results path has no test of its own. It uses the same `transition_to` line as a cleared run (`gameplay_screen.cpp`, gated on `outcome() != InProgress`). The plan accepted this.
- The pause window (#60) doesn't exist yet. The constraint is documented in `gameplay_screen.hpp:26-28`, and the plan scoped it out.
- Focus loss (Alt+Tab) and a runtime fullscreen toggle were scoped out. `SDL_HideCursor` is global to the SDL mouse and hides the cursor only over SDL windows.
- The `screen.hpp:58-60` comment says the callback is "Null in headless". In a headless app run `main` still wires it, and the `Window` wrapper does nothing. The `action_down` comment just above uses the same wording, so this follows the existing style.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | PASS |
| Warnings gate: changed TUs (`window.cpp`, `gameplay_screen.cpp`, `main.cpp`, `app_test.cpp`, `gameplay_screen_test.cpp`) recompiled to `/dev/null` with each owning target's `flags.make` (`-O3 -std=c++20 -Wall -Wextra -Wpedantic`) | PASS (0 warnings, rc=0) |
| Lint | No linter is configured. The warnings gate above stands in for it |
| Tests: sandboxed `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`. Inside the sandbox, `/dev/snd` and `/run/user/$UID` were confirmed empty first | PASS 49/49. Both new sections ran: "Headless cursor wrapper is a no-op." and "cursor hidden during gameplay, restored on every exit ok." |
| Skipped / env-guarded tests | None. No `GTEST_SKIP` or `[SKIP]` markers. The "Skipping" lines are parser log output from tests that passed |
| Headless smoke run (sandboxed, scratch `--data-dir`, `--headless --smoke-test 5 --start-screen select`) | PASS. rc=0, printed "Blaze 4k shut down cleanly.", no `[Window] Failed to ... cursor` line. ALSA "cannot find card" noise is expected because the sandbox hides the device |
| Static: `SDL_HideCursor` / `SDL_ShowCursor` appear only in `src/app/window.cpp:171` | PASS |
| Static: no SDL includes or calls in `screen.hpp` / `gameplay_screen.*` | PASS |
| Static: every `ctx.set_cursor_visible` call is null-guarded (`gameplay_screen.cpp:31,131`) | PASS |
| Static: `git diff main --stat -- src/timing src/audio src/input src/gameplay assets` | PASS (empty) |

## What's Good

- **Fits the existing design.** The cursor service copies the `action_down` pattern: a nullable `std::function` on `ScreenContext`, wired in `main`, so screens stay free of SDL (design pattern 5, "thin platform wrapper").
- **The wrapper is defensive.** It does nothing without a window, makes no SDL call when the cursor is already in the requested state, and logs a failed SDL call with the `[Window]` prefix without ever being fatal. `cursor_hidden_` only changes when SDL reports success.
- **Shutdown is covered.** `Window::shutdown()` restores the cursor before it destroys the GL context and the window. That covers closing the window mid-song and the `--gameplay-demo` path, which no screen `exit()` would reach.
- **`enter`/`exit` are symmetric.** The cursor is hidden before the play-request early return, so even the empty Gameplay screen can't leave it stuck.
- **Tests check the exact call sequence.** They record every hide/show call across run end, Back-abort, re-entry, an empty play request and a null callback, so a stuck-hidden regression would fail a test.
- **The timing path is untouched**, as principle 1 requires.

## Recommendation

Ready to merge after the owner's windowed and fullscreen check (implementation report, E2E 3). The two Low nits are optional; fix them with `/fix-findings` if wanted.
