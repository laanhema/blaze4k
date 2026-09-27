# Implementation Report

**Plan**: `.agents/plans/completed/021-input-remapping-screen-plan.md`
**Branch**: `feature/021-input-remapping-screen`
**Status**: COMPLETE

## Summary

Added the C6 input-remapping screen. `InputManager` now has a runtime binding
bridge (`apply_bindings`) that rebuilds its key/pad maps from the persisted
binding names, a per-action fallback to compiled defaults for unparseable names,
permanently reserved Escape / pad-Back safety bindings, and an opt-in raw capture
mode that emits `InputEvent`s with `action = None`, the raw SDL code and the
verbatim SDL nanosecond timestamp. The C2 default binding lists are now the single
authority (expanded 6/2 -> 7/7) and `setup_default_mappings` derives from them.
A new pure, SDL-free `InputRemapModel` (`src/screens/input_remap.*`) owns row
enumeration, move/clamp, assign with conflict detection, reset and apply, and a
new `InputRemapScreen` (`src/screens/input_remap_screen.*`) toggles capture,
converts raw codes to SDL canonical names, commits immediately to the shared
`GameConfig` + live `InputManager`, and is entered from a new
`OptionsRow::RemapInput` row in the Select options overlay.

Resolved Open Questions implemented as specified: OQ1 reject + `IN USE: <ACTION>`
(no steal/swap); OQ2 `SDL_Keycode` + `SDL_GetKeyName`/`SDL_GetKeyFromName`; OQ3
one row per binding slot; OQ4 immediate in-memory apply + clean-exit `save_config`
(no config path in `ScreenContext`); OQ5 persisted defaults expanded 6/2 -> 7/7
with `config_persistence_test` updated; OQ6 Escape and pad-Back permanently
reserved to Back; OQ7 `kConfigVersion` stays 1.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Expand default binding authority (7/7) | `src/data/config_loader.cpp` | ✅ |
| 2 | Runtime binding bridge + capture mode | `src/input/input_manager.hpp`, `.cpp` | ✅ |
| 3 | Pure remap model | `src/screens/input_remap.hpp`, `.cpp` | ✅ |
| 4 | `ScreenId::InputRemap` + context seam + back nav | `src/screens/screen.hpp`, `screen_manager.cpp` | ✅ |
| 5 | `InputRemapScreen` | `src/screens/input_remap_screen.hpp`, `.cpp` | ✅ |
| 6 | Options-menu entry row | `src/screens/options_menu.hpp`, `.cpp` | ✅ |
| 7 | Wire entry from `SelectScreen` | `src/screens/select_screen.cpp` | ✅ |
| 8 | Wire `main` (boot apply, register, context) | `src/main.cpp` | ✅ |
| 9 | Register sources + test targets | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 10 | `input_remap_test` | `tests/input_remap_test.cpp` | ✅ |
| 11 | `input_remap_screen_test` | `tests/input_remap_screen_test.cpp` | ✅ |
| 12 | Extend `input_test` | `tests/input_test.cpp` | ✅ |
| 13 | Extend `options_menu_test` + `screen_manager_test` | tests | ✅ |
| 14 | Update `config_persistence_test` counts | `tests/config_persistence_test.cpp` | ✅ |
| 15 | Full suite + warning budget | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 25/25 (100%) |
| Warning budget (forced rebuild, `rg -i warning`) | ✅ none |
| Purity (`input_remap.*` SDL/GL/audio/clock-free) | ✅ no matches |
| E2E 1 (saved remap honored + preserved) | ✅ exit 0, `Confirm:["Space"]` kept |
| E2E 2 (defaults single authority) | ✅ exit 0, re-saved 7/7 |
| E2E 3–5 (screen/model/options/shell tests) | ✅ |
| E2E 6 (regression ctest) | ✅ 25/25 |
| E2E 7 (diff scope) | ✅ edits limited to planned files; no `src/gameplay/`, `src/timing/`, `src/chart/` |

Explicit suite run: `input_remap_test`, `input_remap_screen_test`, `input_test`,
`options_menu_test`, `screen_manager_test`, `config_persistence_test`,
`select_screen_test` all exit 0.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/input_remap.hpp` | CREATE | 71 |
| `src/screens/input_remap.cpp` | CREATE | 169 |
| `src/screens/input_remap_screen.hpp` | CREATE | 43 |
| `src/screens/input_remap_screen.cpp` | CREATE | 217 |
| `tests/input_remap_test.cpp` | CREATE | 248 |
| `tests/input_remap_screen_test.cpp` | CREATE | 224 |
| `src/input/input_manager.cpp` | UPDATE | +117/-36 |
| `src/input/input_manager.hpp` | UPDATE | +17 |
| `src/data/config_loader.cpp` | UPDATE | +24/-8 |
| `src/screens/screen.hpp` | UPDATE | +11/-4 |
| `src/screens/screen_manager.cpp` | UPDATE | +5/-1 |
| `src/screens/options_menu.hpp` | UPDATE | +1 |
| `src/screens/options_menu.cpp` | UPDATE | +8 |
| `src/screens/select_screen.cpp` | UPDATE | +30 |
| `src/main.cpp` | UPDATE | +7 |
| `CMakeLists.txt` | UPDATE | +2 |
| `tests/CMakeLists.txt` | UPDATE | +20 |
| `tests/input_test.cpp` | UPDATE | +60 |
| `tests/options_menu_test.cpp` | UPDATE | +32/-2 |
| `tests/screen_manager_test.cpp` | UPDATE | +14 |
| `tests/config_persistence_test.cpp` | UPDATE | +13/-2 |

## Deviations from Plan

1. **Default row counts (OQ3 vs plan Task 10 text).** The plan's Task 10 case 1
   says "one row per default binding (7 keyboard + 7 gamepad)". With the
   user-confirmed OQ3 ("one row per binding slot") the defaults enumerate to
   **12 keyboard + 12 gamepad rows** (24), because `Left/Down/Up/Right/Confirm/
   Options` each carry two names (e.g. `Left = {"Left","D"}`). The plan's 7/7
   refers to the number of *actions* in the default lists, not rows. The test
   follows OQ3 and computes the expected slot count from
   `default_key_bindings()`/`default_gamepad_bindings()` rather than hardcoding
   7/7, with per-slot spot checks (arrows + DFJK both preserved).
2. **Trailing RESET row selection.** The pure `input_remap_move_row` keeps the
   plan's clamp `[0, rows.size()-1]`. The trailing "RESET TO DEFAULTS" row is
   therefore tracked by a screen-level `reset_selected_` flag (pressing Down on
   the last binding row steps onto it; Up steps off) instead of letting
   `model.row` exceed the binding rows. This keeps the pure model contract
   exactly as specified while making the reset row reachable. An extra
   `reset_selected()` test accessor was added.
3. **`RemapRow` equality.** Added free `operator==`/`operator!=` inline in
   `input_remap.hpp` (not named in the plan) so tests and callers can compare row
   lists directly.
4. **Conflict capture state (OQ1).** On a rejected (`Conflict`) assignment the
   screen keeps capture on with the `IN USE: <ACTION>` message (the plan's first
   option), rather than returning to the list.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/input_remap_test.cpp` | enumeration (one row/slot, keyboard-first, partial-config override), move/set clamp, assign/replace, conflict, duplicate-same-action, reset, apply grouping + save/load round-trip, empty config safety |
| `tests/input_remap_screen_test.cpp` | rows from config + exit clears capture, Confirm starts capture, capture assign writes config + live `action_for_key`, conflict rejected, Escape cancels, Back -> Select, reset restores defaults (+Tab/→Options, Escape/→Back), reserved-Escape safety, headless render + re-enter resets state |
| `tests/input_test.cpp` (extended) | `apply_bindings` rebuild + old-key removal + per-action fallback, reserved Escape, capture raw emission/`action=None`/exact timestamp, repeat skip, pad-Back capture bypass, capture off restores mapping |
| `tests/options_menu_test.cpp` (extended) | row clamp 0..5, `REMAP INPUT` name, adjust no-op, apply does not touch `config.input` |
| `tests/screen_manager_test.cpp` (extended) | `InputRemap` Back -> Select, `back_navigates()` true |
| `tests/config_persistence_test.cpp` (extended) | default counts 7/7, `Options->{"Tab"}` and pad `Back->{"back"}` present |
