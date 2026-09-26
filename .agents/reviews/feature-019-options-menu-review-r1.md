# Code Review: feature/019-options-menu (re-review r1)

**Scope**: branch `feature/019-options-menu` vs `main`, HEAD `1697cbf`, committed + uncommitted (working tree clean: `git status --porcelain` empty). Issue #19 — Options menu.
**Prior report**: `.agents/reviews/feature-019-options-menu-review.md` (APPROVE with nits, 4 Low findings)
**Recommendation**: APPROVE (with nits) — all 4 prior findings fixed; no new Critical/High/Medium regression, 4 new Low notes.

## Summary

The single commit `1697cbf` ("Implement #19 … Fix review nits: …") contains the whole feature plus the four prior fixes. All four prior findings are verifiably fixed with test coverage. I hunted the changed `ScreenManager` Back control flow and `InputManager` Back-release classification for new regressions (double-handling, spurious Options on a tap, quit-at-Title, event ordering) and found none above Low. Build and all 21 tests pass; purity holds.

## Fix Verification

| # | Prior finding | Status | Evidence |
|---|---------------|--------|----------|
| 1 | Same-tick Options+Back navigated on stale modal state | **FIXED** | `src/screens/screen_manager.cpp:189-191, 202-204` defers `handle_back()` to after `active->update()` whenever `options_pressed` is in the batch, so it sees the modal state update() created; lone Back still handled pre-update. Test: `tests/select_screen_test.cpp:407-421` asserts Select stays Select on `[Options, Back]`, then lone Back → Title. |
| 2 | C/M step/clamp unsourced, off-grid snap, range too narrow | **FIXED** | Clamp now `[1, 9999]` (`src/screens/options_menu.cpp:42-43`), endpoints returned by `options_speed_values()` (`:99`) and applied by `set_speed_value()` (`:114-116`). C/M adjustment steps relative to current value, preserving off-grid (`:197-202`). Step provenance + "UI affordance" documented (`:31-37`, hpp `:58-61`). Tests: `tests/options_menu_test.cpp:122-152` (460/1/9999 + off-grid 403 preserved). |
| 3 | `back_navigates()` desynced from `handle_back()` | **FIXED** | Both now share `default_back_navigates()` (`screen_manager.cpp:13-15, 138, 158`) and `back_navigates()` ORs `active->back_consumed()` (`:150-159`); `Screen::back_consumed()` added (`screen.hpp:72`) with `SelectScreen::back_consumed() == options_open_` (`select_screen.hpp:47`). The two-sides consistency is documented on the hook. |
| 4 | Shoulder-only opening; bare pads cannot open overlay | **FIXED** | `InputManager::handle_gamepad_back` classifies on release: tap → Back, hold ≥500 ms → Options (`input_manager.cpp:133-165`, threshold `input_manager.hpp:27-30`), dispatched for gamepad Back (`:212-217`). Test: `tests/input_test.cpp:131-160` verifies 100 ms tap → Back and 500 ms+ hold → Options. |

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority
None.

New-regression sweep on the changed paths came back clean:
- **Double-handling**: only one of the two `handle_back()` call sites runs per tick (mutually exclusive on `options_pressed`, `screen_manager.cpp:189/202`); a consumed Back returns early (`:135-137`). No path calls `handle_back()` twice.
- **Options on a normal Back tap**: keyboard Back (`SDLK_ESCAPE`) never enters `handle_gamepad_back`; gamepad tap <500 ms emits `Back` only (test `input_test.cpp:143-150`). No spurious Options.
- **Quit-at-Title**: App's quit path is keyboard-`Back` only (`src/app/app.cpp:102-109`); gamepad Back never reached it (before or after). A pad Back hold at Title yields `Options`, which Title ignores — it does not trigger a quit and does not suppress the keyboard Escape quit. Unaffected.
- **Event ordering**: the deferral only moves Back relative to the same tick's `update()`; `apply_pending()` still runs after both (`screen_manager.cpp:206-207`), so no transition is lost or applied on the wrong screen. Attract-Confirm handling is unchanged.

### Suggestions / Low
- **Low — `src/input/input_manager.cpp:133-165`**: Hold-classification applies to **every** gamepad Back, not just shoulder-less pads. In Gameplay the Options action is unreachable/ignored, so holding Back ≥500 ms no longer aborts the run on press; a normal <500 ms tap still aborts (so impact is small). Consider scoping the hold gesture to screens with an overlay, or noting that gameplay abort is tap-only. (Inherent tradeoff of the requested fallback; not blocking.)
- **Low — `src/input/input_manager.cpp:142-146`**: If focus is lost while Back is held, `clear_action_states()` erases the hold entry; the later release then takes the `it == end` branch, computes `held=false`, and emits a spurious Back tap → unintended navigation after refocus.
- **Low — `src/input/input_manager.cpp:115-124`**: `on_gamepad_removed()` does not erase `gamepad_back_hold_ns_` for the removed device; a pending hold is orphaned until overwritten by a reused device id (single-entry, self-correcting).
- **Low — `src/input/input_manager.cpp:155`**: The synthesized press carries `timestamp_ns = down_ns` (original press) but is enqueued at release; correct for menu semantics, but a future event-log/aging consumer could be misled.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS — no warnings/errors from project sources (only vendored `_deps/glad`) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 21/21, 0 skipped/env-guarded |
| Purity (`SDL_/glad/miniaudio/chrono/GetTicks/std::this_thread` in options_menu.* / select_screen.*) | PASS — no matches |

Note: the 4 new Low items rely on untested edge conditions; the focus-loss orphan (#2) and device-removal orphan (#3) have no test. Everything else is covered by the two new test files (`input_test.cpp`, `options_menu_test.cpp`) plus the `test_same_tick_options_back` case.

## What's Good

- The same-tick fix is minimal and correct: it changes only *when* Back is evaluated, keeps lone-Back pre-update behavior, and has a direct regression test.
- `back_navigates()`/`handle_back()` now derive from one shared predicate plus the side-effect-free `back_consumed()` hook, closing the desync class rather than patching one screen.
- The C/M clamp is sourced to the parser's `[1,9999]` range and the step is honestly documented as a UI affordance, with the off-grid preservation property tested explicitly.
- Hold-Back fallback is implemented at the input layer (the right place) with a deterministic, test-driven threshold.

## Recommendation

Approve. All four prior findings are fixed and verified; no new Critical/High/Medium regressions. The Low items are optional polish, with the hold-vs-tap semantics in Gameplay the only one worth a deliberate decision.
