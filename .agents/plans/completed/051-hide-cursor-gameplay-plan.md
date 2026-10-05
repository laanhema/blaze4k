# Plan: Hide the mouse cursor during gameplay

## Summary

The OS mouse cursor stays drawn over the note field while a song plays. This plan adds one thin platform call, `Window::set_cursor_visible(bool)` in `src/app/window.{hpp,cpp}`, which wraps SDL3's `SDL_HideCursor()` / `SDL_ShowCursor()`. It is a safe no-op when there is no SDL window (headless, or before `init()`). It is exposed to screens through a new nullable `std::function<void(bool)> set_cursor_visible` on `ScreenContext`, wired by `main` exactly like `action_down`, so `src/screens/` stays free of SDL. `GameplayScreen::enter` hides the cursor and `GameplayScreen::exit` shows it again. `ScreenManager` always calls `exit()` on the outgoing screen before `enter()` on the next one, so every way out of gameplay brings the cursor back: run cleared or failed → Results, Back-abort → Select, and any future pause-menu "quit" that goes through a transition. App shutdown does **not** call `exit()` on the active screen, so `Window::shutdown()` also restores the cursor before it destroys the window. That covers closing the window mid-song and the `--gameplay-demo` path, which hides the cursor after `gameplay.init` succeeds. `SDL_HideCursor` is global to the SDL mouse, not per window, so it applies the same way in windowed and fullscreen modes. This is presentation/platform only: nothing touches the music clock, input timestamps or the judgment path.

## User Story

As a player
I want the mouse cursor to disappear while I'm playing a song
So that it doesn't sit on top of the arrows, and comes back when I'm in the menus

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/app/window.{hpp,cpp}`, `src/screens/screen.hpp`, `src/screens/gameplay_screen.{hpp,cpp}`, `src/main.cpp`, `tests/gameplay_screen_test.cpp`, `tests/app_test.cpp` |
| GitHub Issue | #112 ([TODO-29] Hide the mouse cursor during gameplay) |
| Branch (suggested) | `feature/051-hide-cursor-gameplay` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and builds cleanly with `cmake --build build -j` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **49/49 pass** | Run on `main` @ `33d5dd2` with the sandboxed ctest command in Validation. The count stays **49**, because no test executable is added |
| Sandbox requirement | — | Some tests open the real sound device. **Always** run ctest, and any test or app binary, inside the `bwrap` prefix in Validation |
| SDL3 | `release-3.2.8` (`CMakeLists.txt:25`) | `build/_deps/sdl3-src/include/SDL3/SDL_mouse.h:651,666,681`: `bool SDL_ShowCursor(void)`, `bool SDL_HideCursor(void)`, `bool SDL_CursorVisible(void)`. All three return `bool` (SDL3 style, not the SDL2 `int toggle`). They need the video subsystem, which only `Window::init()` starts (`src/app/window.cpp:69`) |
| Video subsystem in headless | `src/app/window.cpp:50-66` | Headless (flag or no `DISPLAY`/`WAYLAND_DISPLAY`) returns before `SDL_InitSubSystem(SDL_INIT_VIDEO)`, and `window_` stays `nullptr`. The new method must guard on `window_ != nullptr` so it never calls SDL mouse APIs without video |
| Screen lifecycle | `src/screens/screen_manager.cpp:79-142` | `start()` and `apply_pending()` always call the outgoing `exit()` before the incoming `enter()`. A same-id transition is a no-op (`:113-115`), so Gameplay is never entered twice without an exit |
| Shutdown | `src/main.cpp:419-440`, `src/app/app.cpp:12-21` | After `app.run()` nothing calls the active screen's `exit()`. `~App` calls `window_.shutdown()`, so that is where the cursor must be restored |
| Gameplay exits | `src/screens/gameplay_screen.cpp:110-112`, `src/screens/screen_manager.cpp:157-158` | Run end (clear **or** fail) → Results (Select if Results is not registered). Back → Select (abort). No pause exists yet (#60). `grep -rni pause src` hits only the audio stream |
| Runtime fullscreen toggle | none | `fullscreen` is only read at window creation (`src/main.cpp:196`, `src/app/window.cpp:82`). Nothing toggles it at runtime |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #112

None in `src/`, `tests/`, `docs/` or earlier plans (`grep -rn "#112\|cursor" src tests docs .agents/plans` hits only the unrelated mine-crossing cursor, audio stream cursor and list cursors). The story text in `.agents/stories/todo-stories.md:1014+` is off-limits and is already reflected here.

---

## Patterns to Follow

### Nullable `std::function` service on `ScreenContext`, wired by main
```cpp
// SOURCE: src/screens/screen.hpp:51-56
// Authoritative per-action down-state, wired by main to
// InputManager::is_action_down. GameplayScreen samples this for held notes
// instead of replaying press/release events. Null in headless/unit tests,
// where callers fall back to event-derived state.
std::function<bool(GameAction)> action_down;
```
```cpp
// SOURCE: src/main.cpp:374-376
shell->context().action_down = [&app](blaze4k::GameAction action) {
    return app.input_manager().is_action_down(action);
};
```

### Null-guarded use in a screen
```cpp
// SOURCE: src/screens/gameplay_screen.cpp:68-73
if (ctx.action_down) {
    held_[0] = ctx.action_down(GameAction::Left);
    ...
```

### Window wrapper: guard on headless / null handle, never throw
```cpp
// SOURCE: src/app/window.cpp:148-152
void Window::swap_buffers() {
    if (window_ && gl_context_ && !config_.headless) {
        SDL_GL_SwapWindow(window_);
    }
}
```
Errors are logged with the `[Window]` prefix and `SDL_GetError()` (`src/app/window.cpp:70, 97`). A failed cursor call is cosmetic: log once to `std::cerr`, do not fail.

### Move semantics must carry every owned field
```cpp
// SOURCE: src/app/window.cpp:14-41
Window::Window(Window&& other) noexcept
    : config_(other.config_), window_(other.window_), ... is_initialized_(other.is_initialized_) {
    other.window_ = nullptr; ... other.is_initialized_ = false;
}
```
The new `cursor_hidden_` field joins both the move constructor and the move assignment, and is reset on `other`.

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`)
```cpp
// SOURCE: tests/gameplay_screen_test.cpp:23-29, 36-44, 59-115, 225-233
#define TEST_CHECK(expr) ... std::abort();
class StubScreen : public blaze4k::Screen { ... };   // destination screens
blaze4k::ScreenManager manager(0.0);                  // idle -> Attract disabled
manager.context().play_request = &request;
manager.start(ScreenId::Gameplay);
```
```cpp
// SOURCE: tests/screen_manager_test.cpp:37-42
InputEvent press(GameAction action) { InputEvent event; event.action = action; event.pressed = true; return event; }
```
Headless `Window` / `App` checks live in `tests/app_test.cpp:83-105` (`smoke_cfg.window.headless = true`).

---

## Pinned Semantics (the contract the tests pin)

- **`Window::set_cursor_visible(bool visible)`** (public, `void`):
  - If `window_ == nullptr` (headless, not yet initialised, or after `shutdown()`), it returns without calling SDL, and `cursor_hidden()` stays `false`.
  - Otherwise, if `visible == !cursor_hidden_` (already in that state), it returns (idempotent, no SDL call).
  - Otherwise it calls `SDL_ShowCursor()` or `SDL_HideCursor()`. On `true` it sets `cursor_hidden_ = !visible`. On `false` it logs `[Window] Failed to show/hide cursor: <SDL_GetError()>` to `std::cerr` and leaves `cursor_hidden_` unchanged.
- **`[[nodiscard]] bool Window::cursor_hidden() const`** returns `cursor_hidden_`. It is the wrapper's own record of what it asked SDL for (test/diagnostic accessor).
- **`Window::shutdown()`**: before `SDL_GL_DestroyContext` / `SDL_DestroyWindow`, if `window_ && cursor_hidden_`, it calls `set_cursor_visible(true)`. It then sets `cursor_hidden_ = false` unconditionally, so a moved-from or re-initialised window starts visible.
- **`ScreenContext::set_cursor_visible`**: `std::function<void(bool)>`, default empty, documented as "#112: show/hide the OS mouse cursor, wired by main to `Window::set_cursor_visible`. Null in headless/unit tests and the `--gameplay-demo` path (every call is null-guarded)." The header stays free of SDL (`<functional>` is already included).
- **`GameplayScreen::enter`**: the **first** statement (before the play-request early return) is `if (ctx.set_cursor_visible) { ctx.set_cursor_visible(false); }`. Hiding even when there is no play request keeps `enter`/`exit` symmetric. The screen is blank in that case, and the cursor comes back on Back.
- **`GameplayScreen::exit`**: `if (ctx.set_cursor_visible) { ctx.set_cursor_visible(true); }`, then the existing `view_.shutdown(); active_ = false;`. The parameter is no longer commented out.
- **No other screen** touches the cursor. Menus keep the OS default (visible), which is what the issue's AC 2 asks for.
- **`--gameplay-demo`** (`src/main.cpp`, after `gameplay.init(...)` succeeds): `app.window().set_cursor_visible(false);`. `Window::shutdown()` from `~App` restores it, and headless is a no-op by the first rule.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/app/window.hpp` | UPDATE | Declare `set_cursor_visible(bool)`, `cursor_hidden()`; add `bool cursor_hidden_ = false;` |
| `src/app/window.cpp` | UPDATE | Implement the method; restore in `shutdown()`; carry `cursor_hidden_` through both move operations |
| `src/screens/screen.hpp` | UPDATE | Add `std::function<void(bool)> set_cursor_visible;` to `ScreenContext` with a comment |
| `src/screens/gameplay_screen.cpp` | UPDATE | Hide in `enter`, show in `exit` (both null-guarded) |
| `src/screens/gameplay_screen.hpp` | UPDATE (comment only) | Class comment: gameplay hides the OS cursor for its lifetime (enter → exit) |
| `src/main.cpp` | UPDATE | Wire `shell->context().set_cursor_visible` to `app.window()`; hide on the `--gameplay-demo` path |
| `tests/gameplay_screen_test.cpp` | UPDATE | New `test_cursor_hidden_during_gameplay` (enter/exit via end-of-run → Results, Back-abort → Select, re-entry, empty play request, null callback) |
| `tests/app_test.cpp` | UPDATE | Headless `Window::set_cursor_visible` is a no-op that never crashes and keeps `cursor_hidden() == false`, before and after `init()`/`shutdown()` |

No files are created. `CMakeLists.txt` / `tests/CMakeLists.txt` are unchanged.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Window cursor wrapper

- **File**: `src/app/window.hpp`, `src/app/window.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: under `on_resize`, add a short comment ("#112: show/hide the OS mouse cursor (SDL3, global to the SDL mouse, so it holds in windowed and fullscreen). A no-op without an SDL window (headless / before init / after shutdown). Idempotent.") and `void set_cursor_visible(bool visible);`. Next to `is_headless()` add `[[nodiscard]] bool cursor_hidden() const { return cursor_hidden_; }`. Add the private `bool cursor_hidden_ = false;`.
  - `.cpp`: implement exactly as in Pinned Semantics. Use `SDL_HideCursor()` / `SDL_ShowCursor()`, which `<SDL3/SDL.h>` (already included by the header) declares.
  - `shutdown()`: at the top, `if (window_ != nullptr && cursor_hidden_) { set_cursor_visible(true); }`, then the existing destroy calls, then `cursor_hidden_ = false;` next to `is_initialized_ = false;`.
  - Move constructor and move assignment: copy `cursor_hidden_` from `other`, then set `other.cursor_hidden_ = false`. Move assignment already calls `shutdown()` first, which restores this object's cursor before it is overwritten.
- **Mirror**: `src/app/window.cpp:148-152` (guard style), `:14-41` (move fields), `:70` (error log style)
- **Validate**: `cmake --build build -j`

### Task 2: `ScreenContext` service

- **File**: `src/screens/screen.hpp`
- **Action**: UPDATE
- **Implement**: after `action_down` (line 56), add the documented `std::function<void(bool)> set_cursor_visible;` (see Pinned Semantics for the comment). Keep aggregate-init compatibility: the member is default-initialised, like `action_down`.
- **Mirror**: `src/screens/screen.hpp:51-56`
- **Validate**: `cmake --build build -j`

### Task 3: Gameplay hides on enter and shows on exit

- **File**: `src/screens/gameplay_screen.cpp`, `src/screens/gameplay_screen.hpp`
- **Action**: UPDATE
- **Implement**:
  - `enter`: the null-guarded hide as the first statement, with a one-line comment ("#112: no OS cursor over the note field. `exit()` always restores it, since ScreenManager exits before every transition").
  - `exit(ScreenContext& ctx)`: the null-guarded show before `view_.shutdown()`.
  - Header class comment: append one sentence: "While active it hides the OS mouse cursor (via `ScreenContext::set_cursor_visible`) and restores it on `exit()`."
- **Mirror**: `src/screens/gameplay_screen.cpp:68-73` (null-guarded `ctx` service)
- **Validate**: `cmake --build build -j`

### Task 4: Wire it in `main`

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Shell path, right after the `action_down` wiring (line ~376): `shell->context().set_cursor_visible = [&app](bool visible) { app.window().set_cursor_visible(visible); };` with a `// #112` comment. Capturing `&app` matches `action_down`. `app` outlives the shell's last `update()`, and nothing calls screen `exit()` after `app.run()` returns.
  - `--gameplay-demo` path, after the `gameplay.init(...)` success check (line ~323): `app.window().set_cursor_visible(false); // #112: the demo is gameplay; Window::shutdown() restores it`.
- **Mirror**: `src/main.cpp:374-376`
- **Validate**: `cmake --build build -j`

### Task 5: Tests

- **File**: `tests/gameplay_screen_test.cpp`, `tests/app_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `gameplay_screen_test.cpp`: add a `press(GameAction)` helper (copy of `tests/screen_manager_test.cpp:37-42`) and `void test_cursor_hidden_during_gameplay()`, registered in `main`. It records every call in a `std::vector<bool> calls` through `manager.context().set_cursor_visible = [&](bool v) { calls.push_back(v); };` and covers:
    1. **Run end → Results**: register Gameplay plus `StubScreen(Results)` and `StubScreen(Select)`. Use the one-tap chart with `fail_enabled = false` from `test_end_delay_before_results`. After `start(Gameplay)`, `calls == {false}`. Update `kEndDelaySeconds / 0.25` times at `dt = 0.25`. Then `active_id() == Results` and `calls == {false, true}`.
    2. **Fail → Results**: no separate test. A failed run and a cleared run leave through the exact same `transition_to` line (`gameplay_screen.cpp:110-112`, gated only on `outcome() != InProgress`), so case 1 covers the cursor lifecycle for both. Add a comment saying so. Do **not** add test-only hooks to production code. The owner's windowed check covers a real fail.
    3. **Back-abort → Select**: `start(Gameplay)`, then `manager.update(0.25, {press(GameAction::Back)})`, then one more `manager.update(0.0, {})` so the deferred transition applies. Then `active_id() == Select` and `calls == {false, true}`.
    4. **Re-entry**: from Select, `transition_to(Gameplay)` + `update(0.0, {})` gives a trailing `false`. Leaving again gives a trailing `true`. The calls strictly alternate, so the state never sticks.
    5. **No play request**: `play_request = nullptr`, `start(Gameplay)` gives `calls == {false}`. Back gives `{false, true}`. AC: the cursor never stays stuck even on the empty screen.
    6. **Null callback**: the existing tests already run with the callback empty. Add one explicit `start` → Back → update with `set_cursor_visible` left empty to pin "doesn't crash".
  - `app_test.cpp`: add a section "6. Cursor wrapper is a no-op without a window (#112)". Use a `blaze4k::Window` with `WindowConfig{.headless = true}` (or set the field). Call `set_cursor_visible(false)` **before** `init()`, check `!cursor_hidden()`; `init()`; `set_cursor_visible(false)` and `set_cursor_visible(true)` again, both `!cursor_hidden()`; `shutdown()`; call once more. No crash. Also move-construct a second `Window` from it and check `!moved.cursor_hidden()`.
- **Mirror**: `tests/gameplay_screen_test.cpp:59-115`, `tests/screen_manager_test.cpp:37-42, 292`, `tests/app_test.cpp:83-105`
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R "gameplay_screen_test|app_test"`

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j 2>&1 | grep -iE "warning" | grep -E "window|screen\.hpp|gameplay_screen|main\.cpp|app_test" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **49/49 pass** (baseline 49/49 on `33d5dd2`; no executable is added).

Static checks:

```bash
# SDL stays out of screens: the only new SDL calls are in the window wrapper
grep -rn "SDL_HideCursor\|SDL_ShowCursor" src   # expect hits only in src/app/window.cpp
grep -n "SDL" src/screens/screen.hpp src/screens/gameplay_screen.*   # expect no hits
# Every use of the ctx service is null-guarded
grep -n "set_cursor_visible" src/screens/gameplay_screen.cpp   # each call sits behind `if (ctx.set_cursor_visible)`
# Timing / judgment untouched
git diff --stat -- src/timing src/audio src/input src/gameplay assets   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-stories   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable):** `gameplay_screen_test` pins the hide/show call sequence across run end → Results, Back-abort → Select, re-entry, an empty play request and a null callback. `app_test` pins the headless no-op (before init, after init, after shutdown, after a move). Both must pass under the sandboxed ctest.
2. **Headless app smoke (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and adds no `[Window] Failed to ... cursor` line. Gameplay itself is not reachable headless from the CLI (`--start-screen` takes only `title` / `select`). Step 1 covers the gameplay lifecycle. Do **not** add a CLI flag.
3. **Windowed check (owner; the agent must not launch the GUI, since it opens a window and plays audio):**
   - Windowed 1280x720: move the mouse over the window on Select, and the cursor is visible. Start a song, and the cursor disappears over the window (it still shows over other windows/desktop when the pointer leaves). Let the song finish, and the cursor is back on Results and on Select after it.
   - Start a song and press Back mid-song: the cursor is visible on Select.
   - Start a song with fail on and fail it: the cursor is visible on Results.
   - Start a song and close the window (or Alt+F4) mid-song: the OS cursor is normal afterwards.
   - Repeat the first two with `"fullscreen": true` in the config.
   - `--gameplay-demo <file.sm>`: the cursor is hidden while it plays and normal after quitting.
   - Repeat on Windows and macOS when convenient (AC 5: "Builds on Linux, Windows and macOS"). The code is portable SDL3 with no platform `#if`.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The cursor stays hidden after leaving gameplay through some path | `exit()` restores it, and `ScreenManager` exits before **every** transition (`screen_manager.cpp:79-142`). The test pins the strict hide/show alternation over clear, Back and re-entry | In scope |
| App closes mid-song: no screen `exit()` runs | `Window::shutdown()` restores the cursor before it destroys the window (`~App` → `window_.shutdown()`). The OS also restores the cursor when the process exits | In scope |
| SDL mouse calls without the video subsystem (headless / no display) | The method is guarded on `window_ != nullptr`. Headless never creates a window. Pinned by `app_test` | In scope |
| `SDL_HideCursor` fails on some backend | The failure is logged once per call and `cursor_hidden_` is left unchanged. Purely cosmetic, never fatal | In scope |
| The future pause window (#60) | If pause is an in-screen modal (like the options overlay), Gameplay stays active and the cursor stays hidden, which is correct. "Quit" from it must leave through a `ScreenManager` transition, which runs `exit()`. #60 should keep that rule. Note it in the `gameplay_screen.hpp` comment | In scope (comment); #60 is out of scope |
| The window loses focus (Alt+Tab) during a song | SDL hides the cursor only over its own windows, so the desktop cursor is unaffected. Nothing to do | Out of scope (flag) |
| A future runtime fullscreen toggle | `SDL_HideCursor` is global to the SDL mouse, not tied to the window mode, so a toggle keeps the state. No toggle exists today | Out of scope |
| Timing path touched (principle 1) | Only window/screen lifecycle code changes. The `git diff --stat` check on timing/audio/input/gameplay is in Validation | In scope |

---

## Open Questions

None of these blocks the work. The plan already uses each recommended default.

1. **Hide only during gameplay, or on every screen?** The menus don't use the mouse either.
   **Recommendation: gameplay only**, as the issue's title and AC 2 say ("visible again on the results screen and every other screen"). Hiding everywhere would be a one-line change in `main` (hide once after init) plus dropping the `exit()` show, if the owner prefers it later.
2. **Should the `--gameplay-demo` harness hide it too?** The issue note says only that the cursor must be "shown again" on that path.
   **Recommendation: yes, hide it.** The demo plays a song, and `Window::shutdown()` already restores the cursor, so it is consistent and costs one line.
3. **Hide when Gameplay is entered without a valid play request?** That screen is empty, and only Back leaves it.
   **Recommendation: hide anyway**, so `enter`/`exit` stay symmetric and can never leave the cursor stuck. The case is a wiring error that only shows in logs.

---

## Acceptance Criteria

- [ ] The mouse cursor is hidden while the gameplay screen is active, in both windowed and fullscreen modes (`SDL_HideCursor` is global; owner check)
- [ ] The cursor is visible again on Results and every other screen after gameplay ends (clear or fail), is aborted with Back, or the app quits (tested lifecycle + `Window::shutdown()` restore)
- [ ] Leaving gameplay through any `ScreenManager` transition (including a future #60 pause "quit") restores the cursor, because `exit()` always shows it
- [ ] Headless / unit-test paths don't crash: `ScreenContext::set_cursor_visible` is null-guarded, and `Window::set_cursor_visible` is a no-op without an SDL window (tested)
- [ ] SDL stays out of `src/screens/` (static grep)
- [ ] The build has no new warnings, and the sandboxed ctest passes 49/49
- [ ] No platform-specific code. It builds on Linux (verified), and Windows/macOS use the same portable SDL3 calls
- [ ] Nothing in the timing / judgment / input path changed. `.agents/stories/todo-stories.md` is untouched
