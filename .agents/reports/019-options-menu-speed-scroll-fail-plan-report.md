# Implementation Report

**Plan**: `.agents/plans/019-options-menu-speed-scroll-fail-plan.md`
**Branch**: `feature/019-options-menu`
**GitHub Issue**: #19 ([C4] Options menu: speed mod, scroll direction, fail toggle)
**Status**: COMPLETE

## Summary

Added an in-screen **Options** overlay to the C3 song-select screen. A new
`GameAction::Options` (keyboard `Tab`, gamepad `LEFT_SHOULDER`/`RIGHT_SHOULDER`)
opens a modal panel with four rows — Speed Type (`XMOD`/`CMOD`/`MMOD`), Speed
Value, Scroll (`UP`/`DOWN`), and Fail (`ON`/`OFF`) — navigated with the existing
directional actions + Confirm and closed with Back. Changes are applied straight
into the shared `GameConfig` that `main` already saves on clean exit, so
`SelectScreen`'s existing `Confirm -> gameplay_options_from_config(*ctx.config)`
path picks them up with zero gameplay changes.

The menu is a pure, SDL/GL/audio/clock-free state model in its own translation
unit (`options_menu.{hpp,cpp}`). The only architectural touch is a defaulted
`Screen::handle_back` hook so the modal consumes Back before the manager's
default Select -> Title navigation.

Per the resolved decision on OQ2, the Speed value grid was sourced from the
OpenITG tree rather than feel-based numbers (see **Value Provenance** below).

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Add `GameAction::Options` + Tab/shoulder bindings | `src/input/input_event.hpp`, `src/input/input_manager.cpp` | DONE |
| 2 | Pure `OptionsMenu` model | `src/screens/options_menu.hpp`, `src/screens/options_menu.cpp` | DONE |
| 3 | `Screen::handle_back` seam | `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`, `src/screens/screen_manager.hpp` | DONE |
| 4 | Wire the overlay into `SelectScreen` | `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp` | DONE |
| 5 | Register source + test | `CMakeLists.txt`, `tests/CMakeLists.txt` | DONE |
| 6 | `options_menu_test` (pure) | `tests/options_menu_test.cpp` | DONE |
| 7 | Extend `select_screen_test` (integration) | `tests/select_screen_test.cpp` | DONE |
| 8 | Input mapping assertion | `tests/input_test.cpp` | DONE |

## Value Provenance

OpenITG was cloned at commit **`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`**
(`git clone https://github.com/openitg/openitg.git`). Every sourced value:

| Value | OpenITG source (file:line) | Status |
|-------|----------------------------|--------|
| X-mod option-menu value grid `{1, 1.5, 2, 2.5, 3, 4, 5, 6}` (clamp `[1, 6]`) | `assets/patch-data/Themes/default/metrics.ini:3694-3701` (`Speed,1..8=mod,...`) — identical in `assets/game-data/Themes/home/metrics.ini:7-14` | **Sourced** |
| X-mod in-game scroll-speed progression `{0.5,0.75,1,1.5,2,3,4,5,8}` (cross-reference only) | `src/CodeDetector.cpp:220-221` (`INCREMENT_SCROLL_SPEED`/`DECREMENT_SCROLL_SPEED`) | Sourced (not used; this menu is the PlayerOptions menu, not the code gesture) |
| C-mod default value `450` | `assets/patch-data/Themes/default/metrics.ini:3702` (`Speed,9=mod,C450`) | **Sourced** |
| M-mod default value `600` | `assets/patch-data/Themes/default/metrics.ini:3703` (`Speed,10=mod,M600`) | **Sourced** |
| C/M custom-mod *valid range* `1..9999` | `src/OptionRowHandler.cpp:171-172` (regex `^C[0-9]{1,4}$`, `^M[0-9]{1,4}$`) | Sourced (range only; no increment defined) |
| C/M step `10` (relative to current value), clamp `[1, 9999]` | **No OpenITG increment.** OpenITG defines a single C option (C450) and a single M option (M600) and no increment; the parser validates the *range* C/M 1..9999 (`src/OptionRowHandler.cpp:171-172`). The step is a documented UI affordance; the clamp uses the sourced range. | Sourced range + UI step |
| `"1x"`/`"C450"`/`"M600"` accepted string forms | In-project `src/gameplay/speed_mod.cpp:59-95` (B3, mirrors OpenITG `PlayerOptions`) | Sourced (in-project) |
| `scroll` `"up"`/`"down"` contract | In-project `src/gameplay/gameplay_options.cpp:18-19` | Sourced (in-project) |

**C/M values (post-review):** OpenITG's PlayerOptions menu exposes only one C
value (C450) and one M value (M600) and defines no C/M increment anywhere in
`src/` (verified across `PlayerOptions.cpp`, `OptionRowHandler.cpp`,
`CodeDetector.cpp`, `ScreenOptionsMasterPrefs.cpp`, and the theme `metrics.ini`
files). To keep the C/M rows adjustable per the issue's "configurable"
requirement, the review fix sets the clamp to the sourced parser range
`[1, 9999]` and steps C/M **relative to the current value** by a documented UI
step `10` (no grid, so a seeded off-grid value like `C400` is preserved until
adjusted). X-mod is fully sourced and uses the PlayerOptions metrics grid.

Note: the plan's originally-proposed X step/clamp (0.25 / `[0.25,10]`) was
**replaced** by the sourced OpenITG grid per the resolved OQ2 decision.

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | PASS |
| Build (`cmake --build build -j16`), no errors | PASS |
| Warning budget (`-Wall -Wextra -Wpedantic`, `rg -i warning`) | PASS (none) |
| Full test suite (`ctest --test-dir build --output-on-failure`) | PASS (21/21) |
| `options_menu_test` | PASS |
| `select_screen_test` | PASS |
| `input_test` | PASS |
| `screen_manager_test` | PASS |
| Purity (`rg "SDL_\|glad\|miniaudio\|chrono\|GetTicks"` on options/select) | PASS (no matches) |
| E2E 1: real binary boots into Select headless | PASS (exit 0) |
| E2E 2: config round-trip via real exit-save path | PASS |

## E2E Evidence

1. Real binary boot (headless, isolated data dir), exit 0:
   ```
   [SongLibrary] scanned 'tests/fixtures/reference_pack': 4 songs, 9 charts
   [SelectScreen] library: 4 songs, 9 charts
   [ScreenManager] enter Select
   ```
   (Chart count is 9 for the current fixture, not the plan's stale "6"; no
   behavioral impact.)
2. Exit-save round trip: seeded `{"speed_mod":"C400","scroll":"down","fail_enabled":false}`,
   ran `--headless --smoke-test 5 --start-screen select`, cleaned exit; saved
   `config.json` still contains `"speed_mod": "C400"`, `"scroll": "down"`,
   `"fail_enabled": false`.
3. Overlay open/adjust/close, wheel suspension, Confirm->Gameplay options
   (`CMod`, `400`, `Down`, `fail=false`), Back-closes-then-second-Back->Title,
   and the C2 `save_config`/`load_config` persistence path are all asserted in
   `select_screen_test` (`test_options_overlay`).
4. `cd build && ctest --output-on-failure` -> `100% tests passed out of 21`.

No failures occurred; no verbatim error output to report.

## Files Changed

| File | Action | Notes |
|------|--------|-------|
| `src/screens/options_menu.hpp` | CREATE | Pure `OptionsMenu` / `OptionsRow` model + helpers |
| `src/screens/options_menu.cpp` | CREATE | Dispatch, sourced grids, config<->menu mapping, display strings |
| `src/screens/select_screen.hpp` | UPDATE | Overlay members, accessors, `handle_back` override |
| `src/screens/select_screen.cpp` | UPDATE | Open/close/modal routing, apply, panel render, `handle_back` |
| `src/input/input_event.hpp` | UPDATE | `GameAction::Options` + `action_to_string` case |
| `src/input/input_manager.cpp` | UPDATE | `Tab` + shoulder buttons -> `Options` |
| `src/screens/screen.hpp` | UPDATE | Defaulted `virtual bool handle_back(ScreenContext&)` |
| `src/screens/screen_manager.cpp` | UPDATE | `handle_back()` consults the active screen hook first |
| `src/screens/screen_manager.hpp` | UPDATE | Back-navigation contract comment |
| `CMakeLists.txt` | UPDATE | `src/screens/options_menu.cpp` in `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | `options_menu_test` target/test |
| `tests/options_menu_test.cpp` | CREATE | Pure model coverage |
| `tests/select_screen_test.cpp` | UPDATE | Overlay integration + persistence + empty-library safety |
| `tests/input_test.cpp` | UPDATE | Tab + shoulder mapping assertions |

Untouched as required: `src/main.cpp`, `src/data/config.*`,
`src/data/config_loader.*`, `src/gameplay/*`, `src/timing/*`, `src/chart/*`.

## Deviations from Plan

1. **OQ2 resolved by the user overrides the plan's feel-based speed grid.** The
   plan proposed X step `0.25`/clamp `[0.25,10]` and C/M step `10`/clamp
   `[100,1000]`. X-mod now uses the OpenITG PlayerOptions metrics grid
   `{1,1.5,2,2.5,3,4,5,6}` (clamp `[1,6]`). C/M use the sourced parser clamp
   `[1, 9999]` with a documented UI step `10` applied **relative to the current
   value** (no grid; off-grid seeded values are preserved) since OpenITG defines
   no C/M increment. The `options_speed_step()` helper from the plan was replaced
   by `options_speed_values()` (the X grid; C/M range endpoints for clamping),
   because the sourced X progression is non-uniform and cannot be expressed as a
   single step. Correspondingly `options_menu_test` expectations were updated.
2. **C/M defaults are `450`/`600`** (OpenITG metrics) rather than the plan's
   `400`/`400`; X default remains `1.0`.
3. **`select_screen_test` main setup now registers a `TitleScreen`** so the
   "second Back -> Title" assertion exercises a real transition (the manager
   previously had no Title registered). No behavior change to the feature.
4. **Panel renderer is inlined** in `SelectScreen::render` (the plan mentioned a
   "draw the panel" step; no separate helper method was required).

## Open Questions / Follow-ups

- **OQ2 (C/M increment):** no OpenITG increment exists. Post-review, C/M clamp
  to the sourced parser range `[1, 9999]` and step relative to the current value
  by a documented UI step `10` (preserving off-grid seeded values). See Value
  Provenance.
- **OQ1 (hold-Back fallback):** implemented post-review. A pad Back is
  classified on release: a short tap stays Back, a hold `>= 500 ms` synthesizes
  `Options`, so the overlay is openable on a shoulder-less pad. `Tab` + shoulders
  remain the primary bindings.
- Idle-attract while the panel is open is explicitly out of scope (reset on
  `enter` keeps it harmless). The same-tick `[Options, Back]` edge case is now
  handled: the manager defers Back until after `update()` on any tick that also
  carries an `Options` press.

## Tests Written / Updated

| Test file | Coverage |
|-----------|----------|
| `tests/options_menu_test.cpp` (NEW) | seeding, invalid-speed fallback, row nav clamps, type cycle + per-type memory, value step/clamp (X sourced grid, C/M relative step + [1,9999] clamp, off-grid preservation), toggles, display text, apply + parser round-trip |
| `tests/select_screen_test.cpp` | open, wheel suspension, adjust into shared config, Back closes then second Back -> Title, Confirm publishes changed options, save/load persistence, empty-library + overlay render safety, same-tick Options+Back does not navigate on stale state |
| `tests/input_test.cpp` | `Tab`, `LEFT_SHOULDER`, `RIGHT_SHOULDER` -> `GameAction::Options`, hold-Back fallback (tap -> Back, hold -> Options), existing mappings intact |

## Artifacts

- Report: `.agents/reports/019-options-menu-speed-scroll-fail-plan-report.md`
- Plan archived: `.agents/plans/completed/019-options-menu-speed-scroll-fail-plan.md`
