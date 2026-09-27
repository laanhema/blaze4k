# Plan: Input Remapping Screen (C6)

## Summary

Add a remapping screen that lets the player rebind keyboard keys and pad panels to game actions
from the options menu, with conflict detection, immediate effect, JSON persistence, and
reset-to-defaults. Today the C2 config already stores `input.key_bindings` / `input.gamepad_bindings`
as `action -> [names]`, but the values are **opaque and never applied**: `InputManager`
(`src/input/input_manager.cpp`) hard-codes its own `SDL_Keycode`/`SDL_GamepadButton` tables, and a
rebuilt config (e.g. only 6 key / 2 pad entries) would be ignored. C6 closes that gap and adds the UI.

The design adds a thin **runtime binding bridge** to `InputManager` (`apply_bindings`, capture mode,
reserved safety keys), makes the C2 default binding lists the **single source of truth** (they are
expanded to match the compiled runtime map), and adds a **pure, headless-testable remap model**
(`src/screens/input_remap.hpp`) plus a `ScreenId::InputRemap` screen entered from a new
`OptionsRow::RemapInput`. The screen captures the next raw key/button through a new opt-in
`InputManager` capture mode, converts the raw code to SDL's own canonical name
(`SDL_GetKeyName` / `SDL_GetGamepadStringForButton`), runs conflict detection in the pure model, and
writes straight into the shared `GameConfig` that `main` already `save_config`s on clean exit — the
same C4/C5 pattern. A hardwired Escape / pad-Back fallback guarantees no remap can soft-lock the shell.

## User Story

As a player
I want to remap keyboard keys and pad panels to game actions from a menu
So that any controller layout works without editing config files.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/input/` (binding apply/capture + name serialization), `src/data/config_loader.cpp` (default binding authority), `src/screens/` (new pure remap model + screen + options row + Select intercept), `src/screens/screen.hpp` + `screen_manager.cpp` (new `ScreenId::InputRemap`, `ScreenContext::input` seam), `src/main.cpp` (register screen, wire input, apply saved bindings), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #21 ([C6]) |
| PRD refs | §7.4 Input ("full remapping"), §7.6 Persistence, §12 Phase C, §6 pattern 5 (thin platform wrapper) |
| Depends on | A4 (#4 input layer), C4 (#19 options menu + `OptionsRow` seam) — both merged; C2 (#17 config), C1 (#16 screens) |
| Blocks | — |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` from root CMake (no `-Werror`) |
| Cores | 16 | `-j16` safe |
| Baseline tests | **23/23 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 23" (0.66 s), run this session |
| SDL version | 3.2.8 | fetched at `build/_deps/sdl3-src`; name helpers in `src/events/SDL_keymap.c`, `src/joystick/SDL_gamepad.c` |
| Input manager | `src/input/input_manager.{hpp,cpp}` | hard-coded maps; public `bind_key`/`bind_gamepad_button`/`reset_to_defaults`/`action_for_key`; `is_action_down`; no enumeration, no removal, no config bridge, no capture |
| Config bindings | `src/data/config.hpp:47-59`, `config_loader.cpp:141-177,246-262` | `InputBinding = pair<string, vector<string>>`; `default_key_bindings()` = **6** entries, `default_gamepad_bindings()` = **2**; loader treats names as opaque |
| Config persistence test | `tests/config_persistence_test.cpp:85-86,122-123,143-144` | asserts default counts `== 6` / `== 2`; round-trip only (names not applied) |
| App → Back gate | `src/app/app.cpp:102-109` | on KEY_DOWN checks `action_for_key(...) == Back` then `event_cb_` → `shell->back_navigates()`; `handle_sdl_event` runs after |
| Screen seam / manager | `src/screens/screen.hpp:31-47`, `screen_manager.cpp:13-16,20-30,134-162` | `ScreenContext` holds concrete pointers (`config`, `scores`, `library`, `constants`, `play_request`, `action_down`); `handle_back`/`back_consumed` modal hook; `default_back_navigates` + `handle_back` branch per screen |
| Options seam | `src/screens/options_menu.hpp:12-24`, `select_screen.cpp:255-283` | `OptionsRow` enum + `options_menu_adjust` documented C6 extension point; Select intercepts the action row |
| Test idiom / registration | `tests/input_test.cpp:8-14`, `tests/CMakeLists.txt:32-40` | `TEST_CHECK` (abort) + one `add_executable`/`target_link_libraries(... tundra_core)`/`add_test` block per target |
| SDL name tables | `sdl3-src/src/events/SDL_keymap.c:680-802`, `sdl3-src/src/joystick/SDL_gamepad.c:1064-1090` | key/scancode names ("D","Left","Return","Escape","Tab","Keypad Enter"); gamepad names lowercase ("a","b","x","y","back","start","leftshoulder","rightshoulder","dpup","dpdown","dpleft","dpright") |

**Start green, stay green:** 23 tests pass; this plan adds **2** targets (`input_remap_test`,
`input_remap_screen_test`) and extends `input_test`, `options_menu_test`, `screen_manager_test`,
`config_persistence_test` in place → **25 expected**. No `src/timing/*`, `src/gameplay/*` judgment/
scoring, `src/chart/*`, or `src/render/*` changes.

---

## Pinned Semantics

Authority: **SDL 3.2.8 source** for the key/gamepad name grammar; **`input_manager.cpp:40-94`** for
the current runtime defaults; **C2 `InputSettings`** for persistence; **PRD §7.4/§7.6** for scope.
No judgment, scoring, or timing constants are involved.

### Canonical binding-name grammar (persisted)

Bindings are persisted as the **names SDL itself round-trips**, so no private grammar is invented:

| Device | Encode (code → name) | Decode (name → code) |
|--------|----------------------|----------------------|
| Keyboard | `SDL_GetKeyName(SDL_Keycode)` | `SDL_GetKeyFromName(const char*)` |
| Gamepad | `SDL_GetGamepadStringForButton(SDL_GamepadButton)` | `SDL_GetGamepadButtonFromString(const char*)` |

Verified examples (pinned SDL 3.2.8): keyboard `"D"`, `"F"`, `"J"`, `"K"`, `"Left"`, `"Down"`,
`"Up"`, `"Right"`, `"Return"`, `"Keypad Enter"`, `"Escape"`, `"Tab"`; gamepad `"dpleft"`, `"dpdown"`,
`"dpup"`, `"dpright"`, `"x"`, `"a"`, `"y"`, `"b"`, `"start"`, `"back"`, `"leftshoulder"`,
`"rightshoulder"` (see Value Provenance). Unknown/malformed names are skipped with one
`[InputManager]` warning; an action with **zero** valid names falls back to its compiled default
(safety, and graceful migration of the old placeholder names `"South"/"North"/"East"`).

### Default binding authority (single source)

The C2 default lists become the full runtime default set, and `InputManager` derives its map from
them (name → code). This removes the current drift where the compiled map has DFJK/Tab/KP-Enter/
shoulders but the persisted defaults had only 6 key / 2 pad entries.

```
default_key_bindings()      // src/data/config_loader.cpp
  Left    = {"Left", "D"}
  Down    = {"Down", "F"}
  Up      = {"Up", "J"}
  Right   = {"Right", "K"}
  Confirm = {"Return", "Keypad Enter"}
  Back    = {"Escape"}
  Options = {"Tab"}                      // NEW: Tab was in the compiled map but not persisted

default_gamepad_bindings()
  Left    = {"dpleft", "x"}
  Down    = {"dpdown", "a"}
  Up      = {"dpup", "y"}
  Right   = {"dpright", "b"}
  Confirm = {"start"}
  Back    = {"back"}                     // FIX: old default "East" collided with panel Right (East=b)
  Options = {"leftshoulder", "rightshoulder"}
```

These expand the counts from 6/2 to **7/7**; `config_persistence_test.cpp:85-86` is updated
accordingly (the C4 note "do not change defaults" is superseded by C6, which owns remapping).

### Reserved safety bindings (soft-lock guard)

`InputManager::apply_bindings()` always ends by forcing:

```
key_map_[SDLK_ESCAPE]                 = Back
gamepad_button_map_[SDL_GAMEPAD_BUTTON_BACK] = Back
```

so a bad remap can never remove the escape hatch. While capture mode is active, Escape / pad-Back are
interpreted by the screen as **cancel capture**, never as an assignable code. This is the issue's
"keep at least one working navigation path" requirement.

### Apply / capture contract (InputManager additions)

```
void  apply_bindings(const InputSettings&);   // clear + rebuild both maps from names, install reserved keys
void  set_capture_mode(bool);                  // opt-in raw mode
bool  capture_mode() const;
```

- `apply_bindings`: for each `(action, names)` parse names to codes; per action, if no name parses,
  use that action's compiled default names; then install the reserved keys. Never throws.
- `set_capture_mode(true)`: `handle_sdl_event` emits **one** `InputEvent` per physical KEY_DOWN/UP and
  GAMEPAD_BUTTON_DOWN/UP with `action = None`, `raw_code = event.key.key` / `event.gbutton.button`,
  `device`, `pressed`, and the **unchanged SDL nanosecond timestamp**; the mapped path and the
  pad-Back hold→Options synthesis are bypassed. This preserves AGENTS principle 1 (timestamp captured
  at poll time, never re-timed).
- `set_capture_mode(false)` restores normal mapping; reserved keys remain.

### Persistence / immediate-effect semantics

- Every accepted binding is written into the shared `ctx.config->input` **immediately** and pushed to
  `ctx.input->apply_bindings(*ctx.config)` in the same tick, so menu navigation and later gameplay
  honor it at once (AC2). `main`'s existing clean-exit `save_config` (`main.cpp:344-347`) persists it
  (AC3), exactly like C4/C5. No new save call and no config-path in `ScreenContext`.
- At startup, `main` calls `app.input_manager().apply_bindings(game_config.input)` after the config
  load and before the shell starts, so a saved remap is honored from boot (AC2/AC3).
- **Reset to defaults** rebuilds the model from `default_key_bindings()` /
  `default_gamepad_bindings()`, writes them to `ctx.config->input`, and calls
  `apply_bindings` → arrows + DFJK + enter/esc + Tab + default pad mapping restored (AC4).

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Keyboard binding names (`"D"`, `"Left"`, `"Return"`, `"Escape"`, `"Tab"`, `"Keypad Enter"`, …) | `sdl3-src/src/events/SDL_keymap.c:680-802` (SDL 3.2.8) | Sourced |
| Keyboard round-trip helpers | `sdl3-src/src/events/SDL_keyboard.c` `SDL_GetKeyName` / `SDL_GetKeyFromName` | Sourced |
| Gamepad names (`"a"`,`"b"`,`"x"`,`"y"`,`"back"`,`"start"`,`"leftshoulder"`,`"rightshoulder"`,`"dp*"`) | `sdl3-src/src/joystick/SDL_gamepad.c:1064-1090` `map_StringForGamepadButton` | Sourced |
| Gamepad round-trip helpers | `SDL_GetGamepadStringForButton` / `SDL_GetGamepadButtonFromString` | Sourced |
| Runtime default map semantics (arrows + DFJK → panels, Return/KP-Enter → Confirm, Esc → Back, Tab → Options; dpad + face → panels, Start → Confirm, Back → Back, shoulders → Options) | `src/input/input_manager.cpp:40-82` | Sourced (current code) |
| Reserved Escape → Back / pad-Back → Back fallback | Issue technical note ("keep at least one working navigation path"); C4/C5 precedent | **Design decision** |
| Conflict policy (reject + message) | PRD/issue silent | **Flagged OQ1** |
| One row per binding slot | PRD/issue silent | **Flagged OQ3** |
| Clean-exit (not immediate file) save | C4/C5 precedent | **Flagged OQ4** |
| Expanded default binding counts (7/7) | Derived from `input_manager.cpp` runtime map | Sourced + test update |
| Keyboard keyed by `SDL_Keycode` (not scancode) | Existing code (`input_manager.cpp:190`) | **Flagged OQ2** |

No judgment windows, DP weights, grade boundaries, or life deltas are introduced or changed.

---

## Patterns to Follow

### Pure, SDL-free state model (the C4 model seam)
```cpp
// SOURCE: src/screens/options_menu.hpp:29-67
struct OptionsMenu { SpeedModType speed_type; double x_value; ... int row; };
[[nodiscard]] OptionsMenu options_menu_from_config(const GameConfig&);
void options_menu_apply(const OptionsMenu&, GameConfig&);
void options_menu_move_row(OptionsMenu&, int delta);
void options_menu_adjust(OptionsMenu&, int delta);
```

### Screen lifecycle + event loop + modal consume
```cpp
// SOURCE: src/screens/calibration_screen.hpp:27-50; select_screen.cpp:233-367
void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events);
bool handle_back(ScreenContext& ctx);      // true suppresses manager navigation
[[nodiscard]] bool back_consumed() const;  // kept in sync with handle_back
```

### Options-menu action-row extension (C5 precedent, mirrored by C6)
```cpp
// SOURCE: src/screens/options_menu.cpp:212-218,221-255; select_screen.cpp:255-283
case OptionsRow::CalibrateOffset: break;          // action row: no value adjustment
if (options_.row == (int)OptionsRow::CalibrateOffset) { /* Up/Down move; Confirm/Right -> Calibration */ }
```

### Config-driven defaults (name authority)
```cpp
// SOURCE: src/data/config_loader.cpp:246-262
std::vector<InputBinding> default_key_bindings() {
    return { {"Left", {"Left"}}, ... };
}
```

### SDL event → timestamped InputEvent (capture must not re-time)
```cpp
// SOURCE: src/input/input_manager.cpp:184-205
ie.timestamp_ns = event.key.timestamp;   // direct SDL3 nanosecond timestamp
ie.raw_code = static_cast<uint32_t>(event.key.key);
```

### Config persistence (atomic, tolerant)
```cpp
// SOURCE: src/data/config_loader.hpp:26-35; main.cpp:344-350
[[nodiscard]] bool save_config(const std::filesystem::path&, const GameConfig&, std::string*);
```

### Test idiom + registration
```cpp
// SOURCE: tests/input_test.cpp:8-14; tests/CMakeLists.txt:32-40
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
add_executable(input_test input_test.cpp)
target_link_libraries(input_test PRIVATE tundra_core)
add_test(NAME input_test COMMAND input_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/input_remap.hpp` | CREATE | Pure `RemapRow`/`InputRemapModel` + `input_remap_*` (from_config, move, assign w/ conflict, reset, apply); no SDL/GL |
| `src/screens/input_remap.cpp` | CREATE | Row enumeration from config names, conflict detection, grouping back to `InputSettings` |
| `src/screens/input_remap_screen.hpp` | CREATE | `InputRemapScreen : Screen` + test accessors (`capturing`, model) |
| `src/screens/input_remap_screen.cpp` | CREATE | Lifecycle, capture mode toggling, raw code → name via SDL, apply/reset, render |
| `src/input/input_manager.hpp` | UPDATE | Add `apply_bindings(const InputSettings&)`, `set_capture_mode`, `capture_mode()`; include `data/config.hpp` (InputSettings) or forward-declare |
| `src/input/input_manager.cpp` | UPDATE | Name↔code helpers, rebuild-from-config, reserved keys, capture emission; `setup_default_mappings` delegates to `apply_bindings(InputSettings{})` |
| `src/data/config_loader.cpp` | UPDATE | Expand `default_key_bindings()`/`default_gamepad_bindings()` to the full 7/7 runtime set |
| `src/screens/screen.hpp` | UPDATE | Add `ScreenId::InputRemap`; add `InputManager* input = nullptr;` to `ScreenContext` (forward-declared, keeps SDL out) |
| `src/screens/screen_manager.cpp` | UPDATE | `screen_id_name` case; InputRemap in `default_back_navigates`; `handle_back` branch → Select |
| `src/screens/options_menu.hpp` | UPDATE | Add `OptionsRow::RemapInput` (after `CalibrateOffset`) |
| `src/screens/options_menu.cpp` | UPDATE | Row name/value text; no-op adjust for the new action row |
| `src/screens/select_screen.cpp` | UPDATE | Intercept the Remap row (Confirm/Right) → `transition_to(ScreenId::InputRemap)`; close overlay |
| `src/main.cpp` | UPDATE | `apply_bindings(game_config.input)` at boot; register `InputRemapScreen`; wire `context().input = &app.input_manager()` |
| `CMakeLists.txt` | UPDATE | Add `src/screens/input_remap.cpp` + `src/screens/input_remap_screen.cpp` to `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `input_remap_test` and `input_remap_screen_test` |
| `tests/input_remap_test.cpp` | CREATE | Pure model: enumeration, move clamp, assign/replace, conflict, reset, config grouping |
| `tests/input_remap_screen_test.cpp` | CREATE | Headless screen integration with real `InputManager` + `ScreenManager`: capture, apply, conflict, reset, cancel, back |
| `tests/input_test.cpp` | UPDATE | `apply_bindings` rebuild + fallback; reserved Escape; capture mode raw emission/timestamps |
| `tests/options_menu_test.cpp` | UPDATE | New row name/value, count 6, adjust no-op, apply does not touch `config.input` |
| `tests/screen_manager_test.cpp` | UPDATE | InputRemap back → Select; `back_navigates()` true |
| `tests/config_persistence_test.cpp` | UPDATE | Default binding counts 6/2 → 7/7 |

Not modified: `src/timing/*`, `src/gameplay/*`, `src/chart/*`, `src/render/*`,
`src/data/config.hpp` (field layout unchanged), `src/data/config_loader.hpp`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Expand the default binding authority (C2)

- **File**: `src/data/config_loader.cpp`
- **Action**: UPDATE
- **Implement**: replace the bodies of `default_key_bindings()` (`:246-255`) and
  `default_gamepad_bindings()` (`:257-262`) with the 7-entry full sets in **Pinned Semantics**
  (keyboard uses SDL key names; gamepad uses SDL gamepad-button strings, replacing the invalid
  placeholder `"South"/"North"/"East"`). Keep the return type `std::vector<InputBinding>` and the
  declaration in `config.hpp` unchanged.
- **Mirror**: `src/data/config_loader.cpp:246-262`.
- **Validate**: `cmake --build build -j16`; `./build/tests/config_persistence_test` after Task 15.

### Task 2: Runtime binding bridge + capture mode in `InputManager`

- **File**: `src/input/input_manager.hpp`, `src/input/input_manager.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `#include "data/config.hpp"` (or forward-declare `struct InputSettings;` — prefer the
    include in the `.cpp` and a forward declaration in the `.hpp` to keep the header light).
  - Public API: `void apply_bindings(const InputSettings& settings);`, `void set_capture_mode(bool on);`,
    `[[nodiscard]] bool capture_mode() const { return capture_mode_; }`.
  - `.cpp` anonymous-namespace helpers:
    `SDL_Keycode key_from_name(const std::string&, bool& ok)` (via `SDL_GetKeyFromName`),
    `SDL_GamepadButton button_from_name(const std::string&, bool& ok)` (via
    `SDL_GetGamepadButtonFromString`).
  - `apply_bindings`: clear `key_map_`/`gamepad_button_map_`; for each `(action, names)`, parse each
    name, map code→action, and if the action ends with **no** valid code, retry with that action's
    default names from `default_key_bindings()`/`default_gamepad_bindings()` (loaded from
    `data/config_loader.hpp`); warn once per invalid name; finally force
    `key_map_[SDLK_ESCAPE] = GameAction::Back` and
    `gamepad_button_map_[SDL_GAMEPAD_BUTTON_BACK] = GameAction::Back` (reserved).
  - `setup_default_mappings()` → `apply_bindings(InputSettings{})`; `reset_to_defaults()` likewise.
  - `handle_sdl_event`: at the top of the KEY_DOWN/UP branch and GAMEPAD_BUTTON_DOWN/UP branch, when
    `capture_mode_` is true, `push_back` a raw `InputEvent{action = GameAction::None, pressed,
    timestamp_ns = event.key.timestamp` / `event.gbutton.timestamp, device, device_id, raw_code}`
    and `return` (skip repeat keys, skip the Back hold synthesis, skip map lookup). Add
    `bool capture_mode_ = false;`.
- **Mirror**: `src/input/input_manager.cpp:40-94,184-231`.
- **Validate**: `cmake --build build -j16`; existing `./build/tests/input_test` still passes.

### Task 3: Pure remap model

- **Files**: `src/screens/input_remap.hpp`, `src/screens/input_remap.cpp`
- **Action**: CREATE
- **Implement** (pure: include only `<string>`, `<vector>`, `data/config.hpp`; no SDL/GL):
  ```cpp
  namespace td {
  struct RemapRow { GameAction action; DeviceType device; std::string name; };
  enum class RemapStatus { Bound, Replaced, Conflict, Unchanged };
  struct InputRemapModel {
      std::vector<RemapRow> rows;
      int row = 0;
      bool capturing = false;
      std::string message;                 // transient feedback ("BOUND TO LEFT", "IN USE: CONFIRM")
  };
  [[nodiscard]] InputRemapModel input_remap_from_config(const GameConfig&);
  void input_remap_move_row(InputRemapModel&, int delta);              // clamp [0, rows.size()-1]
  void input_remap_set_row(InputRemapModel&, int row);
  // Assign `name` to the selected row's device. Returns Conflict (message set, no change) if
  // another row with a different action already has that (device, name); Unchanged if the same
  // device+name already exists on this action; Bound/Replaced otherwise.
  RemapStatus input_remap_assign(InputRemapModel&, const std::string& name);
  void input_remap_reset(InputRemapModel&);                            // rebuild default rows
  void input_remap_apply(const InputRemapModel&, InputSettings&);      // rows -> action->[names]
  [[nodiscard]] std::string remap_action_name(GameAction);             // "LEFT", "CONFIRM", ...
  [[nodiscard]] std::string remap_device_name(DeviceType);             // "KEYBOARD", "PAD"
  [[nodiscard]] std::string remap_row_value_text(const InputRemapModel&, int row); // row name or "<PRESS>"
  } // namespace td
  ```
  `input_remap_from_config`: start from `default_key_bindings()`/`default_gamepad_bindings()`, then
  for each action present in `config.input.*` replace that action's names; one `RemapRow` per
  binding, grouped keyboard-first then gamepad. Skip empty name lists. `input_remap_assign` replaces
  only the selected row's `name`. `input_remap_apply`: group rows by `(action, device)`, emit
  `key_bindings` / `gamepad_bindings` preserving name order.
- **Mirror**: `src/screens/options_menu.hpp:29-67`, `options_menu.cpp:130-165`.
- **Validate**: `cmake --build build -j16` (after Task 14 registers the `.cpp`).

### Task 4: `ScreenId::InputRemap` + context input seam + back navigation

- **Files**: `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`
- **Action**: UPDATE
- **Implement**: add `InputRemap` to `enum class ScreenId` (after `Calibration`); add
  `class InputManager;` forward declaration near `struct GameConfig;` and
  `InputManager* input = nullptr;` to `ScreenContext` (comment: wired by `main`, used by the C6
  remap screen; null in headless tests). In `screen_manager.cpp`: `screen_id_name` →
  `case ScreenId::InputRemap: return "InputRemap";`; `default_back_navigates` include
  `id == ScreenId::InputRemap`; `handle_back` add
  `else if (active_id_ == ScreenId::InputRemap) { transition_to(ScreenId::Select); }`. Update the
  header comment to list InputRemap → Select.
- **Mirror**: `src/screens/screen_manager.cpp:13-16,20-30,134-152`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` (Task 16 adds cases).

### Task 5: `InputRemapScreen`

- **Files**: `src/screens/input_remap_screen.hpp`, `src/screens/input_remap_screen.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  class InputRemapScreen : public Screen {
  public:
      [[nodiscard]] ScreenId id() const override { return ScreenId::InputRemap; }
      void enter(ScreenContext&) override;   // rebuild model from ctx.config; capturing=false
      void update(ScreenContext&, double, const std::vector<InputEvent>&) override;
      void render(ScreenContext&, GlQuadRenderer&, int w, int h) override;
      void exit(ScreenContext&) override;    // clear capture; never writes on abort
      bool handle_back(ScreenContext&) override;      // capturing -> cancel capture, consume
      [[nodiscard]] bool back_consumed() const override { return model_.capturing; }
      // test accessors
      [[nodiscard]] const InputRemapModel& model() const { return model_; }
      [[nodiscard]] bool capturing() const { return model_.capturing; }
  private:
      void commit(ScreenContext&);           // apply model -> ctx.config->input + ctx.input->apply_bindings
      InputRemapModel model_;
      GlQuadRenderer* unused_ = nullptr;     // (none; renderer passed per-call)
  };
  ```
  - `enter`: `model_ = input_remap_from_config(ctx.config ? *ctx.config : GameConfig{});` and if
    `ctx.input != nullptr` `ctx.input->set_capture_mode(false);`.
  - `update` (list mode, `!model_.capturing`): iterate pressed events; `Up`/`Down` →
    `input_remap_move_row(-1/+1)`; `Confirm`/`Right` → if selected row is the trailing
    **"RESET TO DEFAULTS"** row call `input_remap_reset` + `commit`, else start capture
    (`model_.capturing = true; ctx.input->set_capture_mode(true)`); `Back` is owned by
    `handle_back` (not handled here, mirroring C4). Ignore other actions.
  - `update` (capture mode): for each **pressed** event, inspect `event.raw_code`:
    - keyboard Escape (`SDLK_ESCAPE`) → cancel capture (`capturing=false; set_capture_mode(false)`).
    - device gamepad BACK (`SDL_GAMEPAD_BUTTON_BACK`) → cancel capture.
    - otherwise → `name = (device==Keyboard) ? SDL_GetKeyName(raw_code) : SDL_GetGamepadStringForButton(raw_code);`
      if non-empty call `input_remap_assign(model_, name)`; on `Conflict` keep capture on with the
      message (or return to list — OQ1); on success `capturing=false; set_capture_mode(false); commit(ctx);`.
  - `handle_back`: if `model_.capturing` → `model_.capturing=false; if(ctx.input) ctx.input->set_capture_mode(false); return true;` else `return false` (manager default → Select).
  - `commit`: `input_remap_apply(model_, ctx.config->input); if (ctx.input) ctx.input->apply_bindings(ctx.config->input);` (null-guarded).
  - `render`: dim backdrop; title "REMAP INPUT"; list rows `"<ACTION>  [<DEVICE>]  <NAME>"`, highlight
    `model_.row`; trailing "RESET TO DEFAULTS" row; footer
    `"[UP/DOWN] SELECT  [ENTER] REBIND  [BACK] EXIT"` or, capturing,
    `"PRESS A KEY OR PAD BUTTON (ESC/CANCEL TO ABORT)"`; show `model_.message` when set. Headless
    renderer is a no-op (`draw_text` is already guarded).
  - `exit`: `if (ctx.input) ctx.input->set_capture_mode(false);`
- **Mirror**: `src/screens/calibration_screen.cpp:56-134,183-186`, `select_screen.cpp:233-367,369-508`.
- **Validate**: `cmake --build build -j16`.

### Task 6: Options-menu entry row

- **Files**: `src/screens/options_menu.hpp`, `src/screens/options_menu.cpp`
- **Action**: UPDATE
- **Implement**: append `RemapInput` to `enum class OptionsRow` **after `CalibrateOffset`** (keeps
  existing indices 0–4 stable; `kOptionsRowCount == 6`). `options_row_name` → `"REMAP INPUT"`;
  `options_row_value_text` → `">"` (an action row); `options_menu_adjust` → documented no-op for
  `RemapInput` (activation lives in `SelectScreen`). Leave seeding/apply untouched.
- **Mirror**: `src/screens/options_menu.cpp:212-255`.
- **Validate**: `cmake --build build -j16`.

### Task 7: Wire the entry from `SelectScreen`

- **File**: `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**: inside the `if (options_open_)` modal block, add a second action-row interception
  mirroring the Calibrate one (`:255-283`):
  ```cpp
  if (options_.row == static_cast<int>(OptionsRow::RemapInput)) {
      if (event.action == GameAction::Up)   { options_menu_move_row(options_, -1); continue; }
      if (event.action == GameAction::Down) { options_menu_move_row(options_, +1); continue; }
      if (event.action == GameAction::Options) { /* close overlay as on the other rows */ ... }
      if (event.action == GameAction::Confirm || event.action == GameAction::Right) {
          options_open_ = false; options_consumed = true;
          if (ctx.manager != nullptr) { ctx.manager->transition_to(ScreenId::InputRemap); }
          continue;
      }
      continue;
  }
  ```
  Keep the existing `switch` and Calibrate branch unchanged.
- **Mirror**: `src/screens/select_screen.cpp:255-283`.
- **Validate**: `cmake --build build -j16`; `./build/tests/select_screen_test` still passes.

### Task 8: Wire `main` — apply saved bindings, register, context seam

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Include `screens/input_remap_screen.hpp`.
  - After `app.init()` succeeds (`:214-217`) and before the shell is started, call
    `app.input_manager().apply_bindings(game_config.input);` so a saved remap is honored at boot
    (AC2). (Safe if the file was missing: defaults are applied.)
  - Register `shell->add_screen(std::make_unique<td::InputRemapScreen>());` alongside the other
    `add_screen` calls (`:289-294`).
  - Add `shell->context().input = &app.input_manager();` next to the other context wiring
    (`:295-302`).
- **Mirror**: `src/main.cpp:213-217,286-302`.
- **Validate**: `cmake --build build -j16`.

### Task 9: Register sources and test targets

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/screens/input_remap.cpp` and `src/screens/input_remap_screen.cpp` to the
  `tundra_core` list (`CMakeLists.txt:80-124`); append `input_remap_test` and
  `input_remap_screen_test` blocks mirroring (`tests/CMakeLists.txt:32-40`).
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 10: `input_remap_test` (pure model)

- **File**: `tests/input_remap_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no SDL/GL):
  1. **Enumeration** — default config → one row per default binding (7 keyboard + 7 gamepad), action
     and device correct; a config with `{"Confirm": {"Space"}}` replaces only Confirm's rows.
  2. **Move clamp** — `input_remap_move_row` clamps to `[0, rows.size()-1]`.
  3. **Assign replace** — assign `"Space"` to Confirm → that row's name is `"Space"`, others unchanged.
  4. **Conflict** — assign a name already on a different action's row → `RemapStatus::Conflict`,
     rows unchanged, `message` non-empty.
  5. **Duplicate on same action** — assigning an existing same-action name → `Unchanged`.
  6. **Reset** — after mutating rows, `input_remap_reset` restores default rows exactly.
  7. **Apply grouping** — `input_remap_apply` yields `key_bindings`/`gamepad_bindings` grouping the
     rows by action preserving order; a round-trip through `save_config`/`load_config` preserves the
     remapped names (AC3).
  8. **Empty config** — `GameConfig{}` and a config with empty binding vectors do not crash.
- **Mirror**: `tests/options_menu_test.cpp:10-17,34-56`.
- **Validate**: `./build/tests/input_remap_test` → 0.

### Task 11: `input_remap_screen_test` (headless integration)

- **File**: `tests/input_remap_screen_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; construct a real `td::InputManager` and a `td::ScreenManager` with
  `InputRemap` + `Select` registered; a `GameConfig` and `ScreenContext`):
  1. **Rows from config** — `enter` builds the default rows; `exit` clears capture.
  2. **Capture start** — a Confirm press on a binding row sets `capturing()` and
     `input.capture_mode()`.
  3. **Capture assign** — feed a raw `InputEvent{action=None, raw_code=SDLK_SPACE, device=Keyboard,
     pressed=true, timestamp_ns=...}` → model row updated, `config.input` updated, and
     `input.action_for_key(SDLK_SPACE) == <action>` (AC2 immediate apply).
  4. **Conflict** — capture a code already bound to another action → no change + message (AC1).
  5. **Escape cancels** — capture then raw `SDLK_ESCAPE` → `capturing()==false`, config unchanged.
  6. **Back exits** — not capturing → `handle_back` returns false; through `manager.update` with a
     Back action the active screen becomes `Select` (AC4 shell).
  7. **Reset** — select the RESET row, Confirm → config bindings equal the defaults and
     `input.action_for_key(SDLK_TAB) == Options`, `input.action_for_key(SDLK_ESCAPE) == Back` (AC4).
  8. **Reserved safety** — after applying a config that omits Escape, `input.action_for_key(SDLK_ESCAPE)
     == Back` still holds.
  9. **Render/exit** — uninitialized `GlQuadRenderer` render is a no-op; re-enter resets state.
- **Mirror**: `tests/calibration_screen_test.cpp` (manager + context wiring), `tests/input_test.cpp`.
- **Validate**: `./build/tests/input_remap_screen_test` → 0.

### Task 12: Extend `input_test`

- **File**: `tests/input_test.cpp`
- **Action**: UPDATE
- **Implement**: add cases — (a) `apply_bindings` with a custom `InputSettings` maps the new key and
  removes the old one (`action_for_key`), and an action whose names are all invalid falls back to its
  default; (b) reserved `action_for_key(SDLK_ESCAPE) == Back` even when the settings omit Escape;
  (c) `set_capture_mode(true)` makes an unmapped key (`SDLK_Q`) emit exactly one raw
  `InputEvent` with `action == None`, `raw_code == SDLK_Q`, and the **exact** SDL nanosecond
  timestamp; capture bypasses the gamepad Back hold synthesis; `set_capture_mode(false)` restores
  normal mapping. Keep all existing cases green.
- **Mirror**: `tests/input_test.cpp:189-201`.
- **Validate**: `./build/tests/input_test` → 0.

### Task 13: Extend `options_menu_test` and `screen_manager_test`

- **Files**: `tests/options_menu_test.cpp`, `tests/screen_manager_test.cpp`
- **Action**: UPDATE
- **Implement**: `options_menu_test`: row-nav clamp now `0..5`; `options_row_name(RemapInput) ==
  "REMAP INPUT"`; `options_menu_adjust` on the row leaves the menu unchanged; `options_menu_apply`
  does not modify `config.input`. `screen_manager_test`: register a spy for `ScreenId::InputRemap`,
  `start` it, press Back → active becomes `ScreenId::Select`; `back_navigates()` is true.
- **Mirror**: `tests/options_menu_test.cpp:69-84`; `tests/screen_manager_test.cpp:112-160`.
- **Validate**: `./build/tests/options_menu_test` && `./build/tests/screen_manager_test`.

### Task 14: Update `config_persistence_test` default counts

- **File**: `tests/config_persistence_test.cpp`
- **Action**: UPDATE
- **Implement**: change `:85-86` from `== 6` / `== 2` to `== 7` / `== 7`; add assertions that
  `default_key_bindings()` contains `Options -> {"Tab"}` and `default_gamepad_bindings()` contains
  `Back -> {"back"}`. Leave the opaque round-trip test (`:122-144`) unchanged.
- **Mirror**: `tests/config_persistence_test.cpp:69-104`.
- **Validate**: `./build/tests/config_persistence_test` → 0.

### Task 15: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build and run everything; check for new warnings.
- **Validate**: see **Validation** below (`ctest` → **25/25**, no warnings).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 25/25: 23 existing + input_remap_test + input_remap_screen_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/input_remap_test
./build/tests/input_remap_screen_test
./build/tests/input_test
./build/tests/options_menu_test
./build/tests/screen_manager_test
./build/tests/config_persistence_test

# Purity: the remap model must stay free of SDL/GL/audio/clock
rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time" src/screens/input_remap.hpp src/screens/input_remap.cpp
# expected: no matches

# Warning budget (no -Wswitch on ScreenId / OptionsRow)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless, non-blocking (no window/GL/audio device required), and use `--data-dir` so the
developer's real `data/` is untouched.

1. **Binary boots with a saved remap and keeps it** (AC2/AC3 at the binary level):
   ```bash
   rm -rf /tmp/td-e2e-remap
   mkdir -p /tmp/td-e2e-remap
   printf '{"version":1,"input":{"key_bindings":{"Confirm":["Space"],"Back":["Escape"]},'\
   '"gamepad_bindings":{"Confirm":["start"],"Back":["back"]}}}\n' > /tmp/td-e2e-remap/config.json
   ./build/tundra-dance --headless --smoke-test 30 --start-screen select \
     --songs tests/fixtures/reference_pack --data-dir /tmp/td-e2e-remap
   # exit 0; logs "[InputManager] applied ..." or no warning; config.json still has Confirm:["Space"];
   # no crash with a partial binding file (missing actions fall back to defaults)
   ```
2. **Defaults are the single authority** (AC4):
   ```bash
   printf '{}\n' > /tmp/td-e2e-remap/config.json
   ./build/tundra-dance --headless --smoke-test 30 --start-screen select \
     --songs tests/fixtures/reference_pack --data-dir /tmp/td-e2e-remap
   # exit 0; config.json re-saved with the full default 7/7 binding maps
   ```
3. **Screen end-to-end with a real InputManager (no device)** — capture, apply, conflict, reset
   (AC1/AC2/AC4):
   ```bash
   ./build/tests/input_remap_screen_test
   # Confirm starts capture; a raw SDLK_SPACE press binds the selected row, writes config.input,
   # and InputManager::action_for_key(SDLK_SPACE) reflects it immediately; a duplicate code is
   # rejected with a message; Escape cancels; the RESET row restores arrows+DFJK+enter/esc+Tab+pads
   ```
4. **Pure model + persistence** (AC3):
   ```bash
   ./build/tests/input_remap_test          # enumeration, move, assign, conflict, reset, apply
   ./build/tests/config_persistence_test   # remapped names survive save/load; defaults now 7/7
   ```
5. **Options-menu reachability + shell back-nav** (AC1/AC4):
   ```bash
   ./build/tests/options_menu_test         # "REMAP INPUT" row present; adjust no-op
   ./build/tests/screen_manager_test       # InputRemap Back -> Select; back_navigates() true
   ./build/tests/select_screen_test        # existing modal tests still green
   ```
6. **Regression**:
   ```bash
   ctest --test-dir build --output-on-failure   # 25/25; music_clock_test/metronome_sync_test/
   # judgment_engine_test/life_keeper_test/parser_* unchanged; --gameplay-demo path untouched
   ```
7. `git status` shows new files under `src/screens/` and `tests/`, edits limited to
   `src/input/input_manager.*`, `src/data/config_loader.cpp`, `src/screens/screen.hpp`,
   `screen_manager.cpp`, `options_menu.*`, `select_screen.cpp`, `src/main.cpp`, and the CMake files.
   No `src/gameplay/`, `src/timing/`, or `src/chart/` changes.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A bad remap removes Escape/Back and soft-locks the shell | `apply_bindings` always force-maps `SDLK_ESCAPE` and `SDL_GAMEPAD_BUTTON_BACK` to Back; capture treats them as cancel; Task 11 case 8, Task 12 (b) | **In scope** |
| `InputManager` compiled defaults drift from the persisted C2 defaults | Make `default_*_bindings()` the single name authority and derive `setup_default_mappings` from it; Task 1 + Task 2; Task 14 pins counts | **In scope** |
| An old config with placeholder names (`"South"/"North"`) silently unbinds actions | Per-action fallback to compiled defaults when no name parses; skip-and-warn per invalid name; Task 2 + Task 12 (a) | **In scope** |
| Capture mode re-times or drops input, violating the nanosecond-timestamp principle | Reuse `event.key.timestamp` / `event.gbutton.timestamp` verbatim, emit `action=None` + `raw_code`; Task 11 case 3, Task 12 (c) | **In scope** |
| While capturing, App still quits on Escape | `back_consumed()` returns `model_.capturing`, so `shell->back_navigates()` is true and `App` does not quit; the screen then cancels capture; Tasks 4/5/11 | **In scope** |
| Multiple default bindings per action (arrows + DFJK) collapse to one | One `RemapRow` per binding slot preserves all defaults; `input_remap_apply` re-groups by action; Tasks 3/5 | **In scope** |
| Keyboard bound by `SDL_Keycode` (layout-dependent) is not portable across OS/keyboard layouts | Documented; names are SDL's canonical key names and SDL handles lookup; panel/nav keys used here are layout-stable; flagged **OQ2** | **Flagged** |
| SDL name helpers misbehave without `SDL_Init` | They read static tables (`SDL_keymap.c`, `SDL_gamepad.c`); Task 11 constructs `InputManager` headless and round-trips names; if a probe fails, initialize SDL in the test | **In scope** |
| Conflict policy surprises the user (steal vs reject) | Reject + visible message by default; one-point change in `input_remap_assign`; flagged **OQ1** | **Flagged** |
| Immediate config write lost on a hard kill before clean exit | Same clean-exit persistence model as C4/C5; AC requires config.json persistence, which the clean exit provides; flagged **OQ4** | **Out of scope** — flagged |
| `-Wswitch` warnings from the two new enum members | Add both `screen_id_name` and `options_row_name` cases in their tasks; warning-budget check in Validation | **In scope** |

---

## Decisions

- **One default-binding authority.** The C2 default lists are expanded to the full runtime map and
  `InputManager` derives its tables from them; this fixes the current silent drift (DFJK/Tab/
  KP-Enter/shoulders missing from persisted defaults) and the invalid `"East" → Back` collision. The
  C4 note "do not change defaults" is superseded by C6.
- **New `ScreenId::InputRemap`, entered from an options action row.** Mirrors C5's Calibration
  screen/row exactly, keeping Select the owner of the options overlay and Back the manager's default
  (InputRemap → Select).
- **Pure, name-based remap model in `src/screens/`.** SDL is confined to `InputManager` (code↔name)
  and the screen's one raw-code→name call, so conflict/reset/apply logic is unit-tested headless
  (AGENTS principles: data-driven, thin platform wrapper).
- **Reserved Escape / pad-Back.** Deliberate, documented safety over full remappability, satisfying
  the issue's soft-lock requirement.
- **Capture mode on `InputManager`, toggled by the screen.** Reuses the existing event queue and
  nanosecond timestamps rather than introducing a second raw-input path or SDL into `ScreenContext`.
- **Immediate in-memory apply + clean-exit persistence.** Reuses `ctx.config`, `ScreenContext.action_down`,
  and `main`'s `save_config`, matching C4/C5; no new save path and no config path in the context.

---

## Open Questions

1. **Non-blocking — conflict policy.** The AC says "with conflict detection" but not the resolution.
   Proposed default: **reject and show `IN USE: <ACTION>`**, leaving the existing binding intact.
   Alternatives: steal (unbind the other action) or swap. Confirm, or specify swap semantics.
2. **Non-blocking — keyboard identity.** The current code keys bindings by `SDL_Keycode`
   (`event.key.key`). Proposed: keep `SDL_Keycode` + `SDL_GetKeyName`/`SDL_GetKeyFromName` names
   (simplest, matches existing tests, and the game's keys are layout-stable). A scancode-based scheme
   would be more layout-portable but changes `InputManager` matching and every existing input test.
   Confirm the keycode approach is acceptable.
3. **Non-blocking — UI granularity.** Proposed: one row per binding slot (e.g. Left has "Left" and
   "D" rows), which preserves arrows + DFJK and needs no add/remove UX. Alternative: one row per
   action with an appended binding list plus clear. Confirm the row-per-binding UI is acceptable.
4. **Non-blocking — save timing.** The AC says bindings "persist via config.json". Proposed:
   write `ctx.config` immediately and rely on `main`'s clean-exit `save_config` (C4/C5 model). Say if
   an immediate save-on-apply (needing the config path in `ScreenContext`) is required.
5. **Non-blocking — default-count expansion.** Expanding C2 defaults from 6/2 to 7/7 changes an
   existing test assertion and the persisted default shape. Proposed: yes, for a single authority.
   Confirm, or keep the on-disk defaults minimal and merge with the compiled runtime map instead.
6. **Non-blocking — reserved keys.** Proposed: Escape and pad-Back are reserved and cannot be
   permanently rebound (they always also map to Back). Confirm this trade-off against full remapping.
7. **Non-blocking — config version.** Binding names remain opaque to the loader, and invalid names
   fall back gracefully, so `kConfigVersion` is left at 1. Confirm no version bump is wanted.

---

## Acceptance Criteria

- [ ] Given the remapping screen, when an action is selected and a key/pad input follows, then that
      input binds to the action, with conflict detection (Tasks 3/5/11; OQ1)
- [ ] Given new bindings, when applied, then menu navigation and gameplay honor them immediately
      (Tasks 2/5/8; Task 11 case 3; E2E 1)
- [ ] Given saved bindings, when the game restarts, then they persist via `config.json` (Tasks 1/5/10;
      Task 10 case 7; E2E 1/4)
- [ ] Given "reset to defaults", when used, then arrows + enter/esc (and DFJK/Tab) and the default pad
      mapping are restored (Tasks 1/3/5/11; Task 11 case 7; E2E 3)
- [ ] A bad remap can never soft-lock the shell: Escape / pad-Back always navigate Back
      (reserved safety; Task 11 case 8; Task 12 (b))
- [ ] `ctest --test-dir build --output-on-failure` → **25/25**; `input_test`/`options_menu_test`/
      `screen_manager_test`/`config_persistence_test` and the `--gameplay-demo` path stay green
      (Tasks 12–15; E2E 5/6)
- [ ] `src/screens/input_remap.*` stays SDL/GL/audio/clock-free; zero new warnings under
      `-Wall -Wextra -Wpedantic` (Validation)
- [ ] Open Questions OQ1–OQ7 confirmed or defaults accepted (conflict policy, key identity,
      UI granularity, save timing, default counts, reserved keys, config version)
