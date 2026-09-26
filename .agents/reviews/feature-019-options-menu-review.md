# Code Review: feature/019-options-menu

**Scope**: branch `feature/019-options-menu` vs `main`, including uncommitted changes (issue #19 — Options menu: speed mod, scroll direction, fail toggle)
**Recommendation**: APPROVE (with nits)

## Summary

The change adds a pure `OptionsMenu` model (`src/screens/options_menu.{hpp,cpp}`), wires a modal overlay into `SelectScreen`, adds a `Screen::handle_back` hook, and a new `GameAction::Options` (Tab + both shoulders). The three ACs are met: the overlay opens from Select and edits speed type/value, scroll, and fail; Confirm republishes `GameplayOptions` from the shared `GameConfig` so gameplay applies the B3 mod math and B6 fail behavior; edits mutate the shared `GameConfig` that `main` saves on clean exit (C2). Build and all 21 tests pass, and the purity check is clean. Only minor issues found.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority
None.

### Suggestions / Low
- **Low — `src/screens/screen_manager.cpp:125` + `src/screens/select_screen.cpp:318-327`**: `handle_back()` runs before the active screen's `update()` for the same event batch, so a same-tick `[Options, Back]` sequence (Back consumed before the overlay is opened inside `update`) can navigate Select → Title in the frame the overlay opens. Benign (no crash, no visible modal), but the ordering is subtle; consider documenting or handling Back inside the screen's own event loop exclusively.
- **Low — `src/screens/options_menu.cpp:40-42, 96-100`**: The C/M step (10) and clamp `[100,1000]` are unsourced. Assessment: this is a **UI affordance, not a correctness/parity bug** — the stored string still round-trips through `parse_speed_mod` and gameplay resolves it correctly, and OpenITG's PlayerOptions only offers a single `C450`/`M600` with no increment. Residual gaps: the menu cannot express the parser's documented `C/M 1..9999` range, and a config manually seeded outside `[100,1000]` (or an x-mod outside `{1..6}`) displays fine but snaps to the nearest grid entry on the first Left/Right press (e.g. `C20` → `C100`, `0.5x` → `1x`). Worth a provenance note or widening the clamp, but not a blocker.
- **Low — `src/screens/screen_manager.cpp:138-143`**: `back_navigates()` (the App's Escape-quit policy) is state-independent and cannot see the new per-screen modal state, weakening the "keep exactly in sync with `handle_back()`" invariant. Currently safe because Select always returns `true`, so the overlay's Back is consumed without quitting; flag is informational for future screens.
- **Low — `src/input/input_manager.cpp:76-77`**: Opening the overlay is bound to shoulder buttons. Standard gamepads have these, but many USB dance pads expose only D-pad + Start/Back, so pad-only opening may not hold on all pads. In-menu navigation itself is fully D-pad/Confirm/Back reachable, satisfying AC4 on the tested device set.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 21/21, 0 skipped/env-guarded |
| Purity (`SDL_/glad/miniaudio/chrono/GetTicks` in options_menu.* / select_screen.*) | PASS — no matches |

## What's Good

- Clean separation: `OptionsMenu` is pure (no SDL/GL/audio/clock), so it is unit-testable and the overlay state is SDL-free.
- AC2 wiring verified end-to-end: `SelectScreen::Confirm` builds `play_request->options` via `gameplay_options_from_config(*ctx.config)` (select_screen.cpp:306-308), `GameplayView::init` forwards speed/scroll to `NoteField` and fail to `LifeKeeper`.
- AC3 verified: the overlay mutates the `GameConfig` owned by `main` (`main.cpp:292`) which is saved via `save_config` on clean exit (`main.cpp:344`); the test exercises the real `save_config`/`load_config` round-trip.
- Modal correctly suspends the wheel (verified in test) and route all presses to the overlay; `handle_back` default preserves prior screen behavior.
- No `-Wswitch` regressions: every switch over `GameAction`/`OptionsRow` either enumerates the new member or has a `default`.
- Per-type value memory avoids nonsense like "C1.5x", and X-mod grid is traceable to OpenITG's PlayerOptions metrics.

## Recommendation

Approve. The low items are optional polish (document the `handle_back` ordering, add a provenance note or widen the C/M clamp, consider a dance-pad-openable binding). No changes required for merge.
