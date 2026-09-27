# Code Review: feature/021-input-remapping-screen (C6 Input Remapping Screen)

**Scope**: Branch `feature/021-input-remapping-screen` vs `main`, including uncommitted working-tree changes (GitHub issue #21)
**Recommendation**: NEEDS WORK

## Summary

The C6 remap feature is well-structured: a pure, SDL-free `InputRemapModel`, a thin `InputManager` binding bridge, reserved Escape / pad-Back safety, and immediate in-memory apply + clean-exit persistence mirroring C4/C5. Timestamps are preserved verbatim in capture mode and the soft-lock guard works as designed. However, `InputManager::apply_bindings()` does not backfill actions that are entirely absent from the persisted binding lists, so legacy/partial `config.json` files silently lose panel, arrow, and Options bindings — verified by probe — which is an upgrade regression and diverges from the model's merge behavior.

## Issues Found

### Critical
None

### High Priority

1. **`src/input/input_manager.cpp:46-82` (`apply_bindings_impl`) / `:120-133` (`apply_bindings`)** — Only the actions present in `settings.*_bindings` are processed; actions missing from the persisted list get no runtime mapping at all, so a legacy/partial config silently unbinds them. The loader (`read_bindings`, `config_loader.cpp:153`) fully replaces the default lists when a bindings object is present, and pre-C6 configs persist only 6 key / 2 pad entries (`Options` absent; gamepad has only `South`/`East`, now invalid). Result on upgrade: `Tab`/Options is unmapped (a keyboard user cannot open the options overlay to reach this very screen) and all gamepad panel bindings are gone. Verified with a probe: after `apply_bindings({{"Confirm",{"Space"}},{"Back",{"Escape"}}})`, `SDLK_LEFT`, `SDLK_DOWN`, and `SDLK_TAB` all resolve to `GameAction::None`. This contradicts the plan's stated "missing actions fall back to defaults" expectation and `input_remap_from_config`, which merges config over defaults. Recommendation: iterate the compiled default actions and merge per-action overrides (falling back when the action is absent or has no valid names).

### Medium Priority

2. **`src/screens/input_remap_screen.cpp:162-189`** — With the default set there are 24 binding rows (12 keyboard + 12 gamepad) plus the RESET row, but the renderer uses a fixed `row_h = height * 0.045` starting at `height * 0.15` with no scrolling or clipping; at 720p the last ~5 rows and the trailing "RESET TO DEFAULTS" row are drawn off-screen (last row y ≈ 0.15h + 24×0.045h ≈ 1.23h). The reset action and lower gamepad bindings are therefore invisible/unreachable by eye. Recommendation: add paging/scrolling or size rows to the available viewport so the RESET row and all bindings are visible.

### Suggestions

3. **`src/input/input_manager.cpp:94-118`** — The move constructor and move assignment copy/move all state except the new `capture_mode_`, so a moved `InputManager` silently drops capture state (the screen would believe it is capturing while the manager emits mapped events). Not currently exercised (App owns the manager in place), but it is a latent inconsistency; set `capture_mode_(other.capture_mode_)` in both.

4. **`src/screens/select_screen.cpp:285-313` / `tests/select_screen_test.cpp`** — The `CalibrateOffset` row has an explicit Select→screen launch test, but the new `RemapInput` row has no equivalent test asserting Confirm/Right on that row transitions to `ScreenId::InputRemap` (only row name/`adjust` no-op are covered). Add a launch-seam test mirroring `test_options_calibration_launch`.

5. **`src/screens/input_remap.cpp:11-44`** — An action present in config with an empty name list (permitted by `read_bindings`, `config_loader.cpp:166-175`) yields zero UI rows, while `apply_bindings` would fall back to that action's defaults — so the screen and the runtime disagree about that action. Low impact, but consider surfacing a fallback row or documenting the divergence.

6. **`src/screens/input_remap_screen.cpp:60-100`** — `model_.message` is only cleared by `start_capture`; navigating rows after a conflict leaves the stale "IN USE: …" text on screen. Clear it on row movement.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build | PASS (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`, 0 warnings) |
| Lint | N/A (no linter configured; `-Wall -Wextra -Wpedantic` clean) |
| Tests | PASS (25/25, `ctest --test-dir build --output-on-failure`) |
| Timestamp preservation (capture path) | PASS (`event.key.timestamp` / `event.gbutton.timestamp` copied verbatim) |
| Soft-lock guard (Escape / pad-Back) | PASS (reserved in `apply_bindings`; capture treats them as cancel; `back_consumed()` blocks App-quit) |
| Persistence (C4/C5 precedent) | PASS (immediate in-memory write + clean-exit `save_config`; boot `apply_bindings`) |

## What's Good

- **Timestamps are sacred here**: capture mode copies `event.key.timestamp` / `event.gbutton.timestamp` directly and bypasses both the mapped path and the pad-Back hold synthesis — no re-timing. Covered by `input_test` case 9.
- **No soft-lock**: Escape / pad-Back are force-mapped after every `apply_bindings`, reserved during capture (cancel), and `back_consumed()` keeps `App` from quitting on Escape while capturing. The InputRemap Back → Select default is wired and tested.
- Clean separation: all conflict/reset/apply logic is in a pure, headless-testable model; SDL stays confined to `InputManager` and one raw-code→name call in the screen.
- The default-binding drift fix (persisted lists now the single runtime authority, 6/2 → 7/7) is a genuine improvement, and persistence matches the C4/C5 pattern exactly.

## Recommendation

Fix finding #1 (merge missing actions from the compiled defaults instead of leaving them unmapped) — it is a real upgrade regression that can strand existing users without panel/Options bindings and block access to this screen. Address #2 for usability, and the suggestions are optional. Re-review after the fix.
