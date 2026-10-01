# Code Review: feature/016-screen-state-machine-title-attract

**Scope**: Branch `feature/016-screen-state-machine-title-attract` vs `main` (incl. uncommitted/untracked files)
**Issue**: #16 — [C1] Screen state machine with Title and Attract screens
**Recommendation**: APPROVE (with nits)

## Summary

This change introduces the C1 arcade shell: a `ScreenManager` with explicit
`enter/update/render/exit` lifecycle and deferred frame-boundary transitions, a
`TitleScreen`, an `AttractScreen` title loop, and a `SelectPlaceholderScreen`, plus a
shared `bitmap_font` extracted from `hud_renderer`. `App` gains an optional event callback
so a screen can consume Escape as Back instead of quitting. The design follows the PRD's
screen-state-machine and music-driven patterns, screens are headless-constructible, and the
new unit tests are thorough and deterministic.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

1. **`src/screens/screen_manager.cpp:125-127` — `back_navigates()` and `handle_back()` disagree for `ScreenId::Results`.**
   `back_navigates()` returns `true` for anything except Title/Gameplay (so Results returns
   true), but `handle_back()` (lines 116-123) only acts on Attract/Select. A Results screen
   would therefore swallow Escape (App does not quit) while no navigation occurs — the app
   becomes unquittable via keyboard. Latent today (C1 registers neither Gameplay nor
   Results) but the manager advertises all five `ScreenId`s. Recommend making
   `back_navigates()` enumerate exactly the screens `handle_back()` handles, or explicitly
   document Results/Gameplay as App-quit states.

2. **`src/app/app.cpp:102-109` + `src/main.cpp:194-199` — quit/consume decision hardcodes `SDLK_ESCAPE`, bypassing `InputManager`'s Back mapping.**
   Escape-to-Back already lives in `InputManager::setup_default_mappings()`
   (`src/input/input_manager.cpp:55`). The new App callback re-encodes the same key in the
   platform layer, so if a user rebinds Back the quit path diverges (Escape still quits,
   the rebound key does not). It also puts input knowledge in `main`, against the "thin
   platform wrapper" intent. Consider deriving consumption from the manager after the
   `Back` `InputEvent` is seen, or routing the decision through `InputManager`.

3. **`src/main.cpp:66` — malformed `--attract-timeout` aborts the process.**
   `std::stod(argv[++i])` throws `std::invalid_argument` uncaught; confirmed
   `./build/blaze-4k --attract-timeout abc` exits 134 with an abort. Same pre-existing
   pattern as `--smoke-test`'s `std::stoi`, but new code should validate and fall back to
   the default with a readable log line.

### Suggestions

4. **`src/screens/screen_manager.cpp:170-179` — repeated transition-retry log spam if Attract is unregistered.**
   The idle block calls `transition_to(Attract)` every subsequent frame once the timeout is
   reached; `apply_pending()` logs `no screen registered ... transition ignored` each time.
   Harmless in C1 (Attract is always registered) but consider resetting/clamping
   `idle_seconds_` on a failed transition to avoid per-frame logging.

5. **`src/screens/screen_manager.cpp:57-72` — `start()` does not `exit()` a previously entered screen.**
   Calling `start()` again switches `active_id_` and re-enters without exiting the old
   screen, violating the enter/exit pairing the manager otherwise maintains (tests call
   `start()` repeatedly for setup, masking this). Either assert single-start or exit the
   outgoing screen.

6. **Test coverage gap for the idle-disable path.** `set_idle_timeout_seconds(<=0)` and the
   CLI `--attract-timeout <=0` disable behavior are never exercised; the `<=0` guard at
   `screen_manager.cpp:171` is untested.

7. **`src/main.cpp:64-69` — `--attract-timeout` is silently ignored when `--gameplay-demo` is also given**
   (the value is only consumed by the shell branch). A one-line warning would avoid the
   silent no-op.

8. **Title "logo" is text-only (`src/screens/title_screen.cpp:52`), not a logo asset.**
   AC #1 says the Title screen "shows the logo"; the PRD lists the logo as an open item and
   the screen comments acknowledge this. Acceptable for C1, but flag for C2/presentation.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check (build, `-Wall -Wextra -Wpedantic`) | PASS — clean build; manual `-fsyntax-only` on new/changed TUs produced zero warnings |
| Lint | N/A — no lint target configured in the project |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 16/16, including new `screen_manager_test`; no tests skipped or env-guarded |
| Headless smoke (`--headless --smoke-test 120`) | PASS — boots Title, exits cleanly |
| Purity check (`chrono|GetTicks|GetPerformanceCounter|SDL_GetTicks|this_thread` in `src/screens`) | PASS — no wall-clock usage |

### Notes on gates
- No test was skipped or conditionally compiled out; all 16 CTest entries executed.
- The headless smoke test does **not** validate the idle→Attract transition: headless
  frames run far faster than `fixed_dt`, so `on_update` (and thus `ScreenManager::update`)
  may be called zero times. Idle behavior is only covered by `test_idle_attract_policy` /
  `test_real_screens`, which inject `fixed_dt` directly. This is a coverage observation,
  not a failure.

## What's Good

- `ScreenManager` uses deferred transitions applied at frame boundaries (exit old → enter
  new) with tests proving a screen is not exited during its own `update()` — solid
  self-destruction safety.
- Screens are SDL/GL-free in `update` and render through a headless-safe renderer seam,
  keeping them unit-testable; the purity check confirms no wall-clock reads in `src/screens`.
- The idle-attract policy is deterministic, measured from injected `fixed_dt`, and correctly
  limited to Title/Select with an origin remembered for Confirm-return.
- The bitmap font extraction is a clean refactor (no duplicated symbols, new glyphs
  `space`, `[`, `]`, full `A-Z`) and `hud_renderer` now shares it without behavior change.
- Tests are meaningful: ordering, deferred application, unregistered targets, idle reset,
  attract return from both origins, back navigation, headless render dispatch, real screens,
  font arithmetic, and an App+shell headless smoke.

## Recommendation

APPROVE with nits. The C1 acceptance criteria (Title with "Press Start", Confirm→Select,
idle→Attract with Start returning to origin, explicit enter/update/render/exit object model)
are met and validated. Address findings 1–3 (especially 1 and 2) before Gameplay/Results are
registered so the central Back policy stays consistent, and consider 4–7 as follow-ups.
