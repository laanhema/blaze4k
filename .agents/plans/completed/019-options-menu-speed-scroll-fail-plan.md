# Plan: Options Menu — Speed Mod, Scroll Direction, and Fail Toggle (C4)

## Summary

Add an in-screen **Options** overlay to the C3 song-select screen. Pressing a new
`GameAction::Options` (keyboard `Tab`, gamepad shoulder) opens a modal panel with four
adjustable rows — **Speed Type** (`XMOD`/`CMOD`/`MMOD`), **Speed Value**, **Scroll** (`UP`/`DOWN`),
and **Fail** (`ON`/`OFF`) — navigated with the four directional actions and Confirm, and closed
with Back. Every change is written straight into the shared `GameConfig` (C2) that `main` already
saves to `config.json` on clean exit, so `SelectScreen`'s existing `Confirm` →
`gameplay_options_from_config(*ctx.config)` path (which feeds B3 mod math and B6 fail behavior)
picks the new values up automatically with no gameplay changes. The menu is a small, pure,
headless-testable state model in its own translation unit; the only architectural touch is a
defaulted `Screen::handle_back` hook so an in-screen modal can consume Back before the manager's
default navigation (otherwise Back on Select would jump to Title with the panel still open). The
row enum + adjust dispatch are the clean seam where C5 (offset wizard) and C6 (remapping) will
later add entries — neither is implemented here.

## User Story

As a player
I want an options menu on the select screen for speed mod (C/X/M), scroll direction, and fail on/off
So that I can configure gameplay without editing files.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/` (new pure options model + Select integration + a `Screen` back hook), `src/input/` (new Options action/mapping), `src/screens/screen_manager.cpp` (consult the screen's back hook), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #19 ([C4]) |
| PRD refs | §7.3 Select ("options menu (speed mod, scroll, offset wizard)"), §4 scope/locked decisions ("C/X/M speed mods only"), §12 Phase C, §5 story 3 |
| Depends on | C1 (#16 screens), C2 (#17 config), C3 (#18 select) — all merged |
| Blocks | C5 (#20), C6 (#21) — seam only, not implemented |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` from root CMake (no `-Werror`) |
| Cores | 16 | `-j16` safe |
| Baseline tests | **20/20 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 20" (0.40 s), run this session |
| Select screen (host) | `src/screens/select_screen.{hpp,cpp}` | Wheel/chart nav in `update`; `Confirm` already builds options via `gameplay_options_from_config(*ctx.config)` (`select_screen.cpp:253-265`); pure helpers + test accessors pattern |
| Screen interface / manager | `src/screens/screen.hpp:51-61`, `screen_manager.cpp:140-192` | Manager calls `handle_back()` **before** `active->update` (`:162-164`) and applies the queued transition at `:176`; `back_navigates()` gates App-quit (`main.cpp:334-336`) |
| Config model | `src/data/config.hpp:41-45` | `GameplaySettings{speed_mod:"1x", scroll:"up", fail_enabled:true}` — the exact fields C4 mutates; pure header (no platform/JSON) |
| Config persistence | `src/data/config_loader.{hpp,cpp}`, `main.cpp:344-347` | `main` owns `game_config` and calls `save_config(paths.config_file, game_config)` on clean exit; `ScreenContext::config` points at it, so in-memory edits persist for free |
| Options mapping (existing) | `src/gameplay/gameplay_options.cpp:9-23`, `gameplay_options.hpp:11-20` | `gameplay_options_from_config` parses `speed_mod` via `parse_speed_mod`, maps `scroll`, copies `fail_enabled` + offset. **No change needed** — C4 only mutates the config it reads |
| Speed parse/format | `src/gameplay/speed_mod.hpp:22-31` | `parse_speed_mod` accepts `"1x"`/`"X1.5"`, `"C400"`, `"M600"`; `SpeedModType{CMod,XMod,MMod}` |
| Input actions | `src/input/input_event.hpp:8-24`, `input_manager.cpp:36-72` | Directions (arrows/DFJK + pad D-pad/face), `Confirm` (Return/KP_Enter/Start), `Back` (Escape/Back). `MenuUp/Down/Left/Right` are **unmapped**. No spare button exists today |
| Config bindings | `config_loader.cpp:246-262`, `config_persistence_test.cpp:85-86` | Bindings are opaque and **not applied** by `InputManager` (hardcoded map); test asserts defaults are 6 key / 2 pad → do **not** change defaults |
| Test idiom / registration | `tests/select_screen_test.cpp:24-31`, `tests/CMakeLists.txt:176-194` | `TEST_CHECK` + temp-dir synth packs; one `add_executable`/`target_link_libraries(... blaze4k_core)`/`add_test` block per target |

**Start green, stay green:** 20 tests pass; this plan adds **1** target (`options_menu_test`) and
extends `select_screen_test` in place → **21 expected**. No timing/judgment/scoring/note-field
changes; `main.cpp`, `config.hpp`, and `config_loader.*` are untouched.

---

## Pinned Semantics

Authority: **PRD §7.3 / §4 locked decisions** for scope; **C2 `GameplaySettings`** for the mutated
fields; **existing `gameplay_options_from_config`** for gameplay application. No gameplay constants
are introduced or changed.

### Rows and adjustment contract (keyboard + pad)

The overlay is modal: while open, the wheel does not move.

| Row | Values | Left/Right (and Confirm) |
|-----|--------|--------------------------|
| `SpeedType` | `XMOD` → `CMOD` → `MMOD` (cycle) | change type ±1 (wraps) |
| `SpeedValue` | numeric, type-specific | step by `options_speed_step(type)` (clamped) |
| `Scroll` | `UP` / `DOWN` | toggle |
| `Fail` | `ON` / `OFF` | toggle |

- **Up/Down** move the highlighted row (clamped `[0, kOptionsRowCount-1]`).
- **Confirm** advances the current row by `+1` (so a pad with only Confirm can change every value).
- Only the four directions + `Confirm` + `Back` (+ the new `Options` open/close action) are used;
  all are mapped on both keyboard and pad.
- On any change the menu is applied to `*ctx.config` immediately (no separate OK/cancel).

### Speed value memory and formatting (ITG-style per-type memory)

- The menu remembers a value per type (`x_value`, `c_value`, `m_value`) so switching type does not
  produce nonsense like `C1.5`.
- `options_menu_from_config` parses the stored `speed_mod`; if valid it seeds the matching type's
  slot, leaving the other two at defaults (X `1.0`, C `400`, M `400`). An invalid/legacy string
  leaves all three at defaults (mirrors `gameplay_options_from_config` fallback).
- Formatting: X → trimmed decimal + `x` (`"1x"`, `"1.5x"`); C → `"C400"`; M → `"M400"`.
- Steps / clamps (chosen for feel; **not** sourced from OpenITG — OQ2): X step `0.25`,
  clamp `[0.25, 10.0]`; C and M step `10`, clamp `[100, 1000]`.

### Apply / persist

- `options_menu_apply(menu, config)` sets `config.gameplay.speed_mod`, `.scroll` (`"up"`/`"down"`),
  `.fail_enabled`. `SelectScreen` calls it after every adjust and again on Back.
- Persistence is `main.cpp:344-347`'s clean-exit `save_config` of the same `GameConfig`
  (`ScreenContext::config` is owned by `main`). No new save path or context field.

### Back handling (the one C1 seam)

- Add `virtual bool Screen::handle_back(ScreenContext&) { return false; }`.
- `ScreenManager::handle_back()` first asks the active screen; if it returns true, the manager
  returns without its default navigation.
- `SelectScreen::handle_back` returns true (closing the panel) when the overlay is open, else false
  (unchanged `Select → Title`).
- Default `false` keeps C1 behavior for every existing screen (incl. `SelectPlaceholderScreen` and
  test spies), so `screen_manager_test`/`test_real_screens` stay green unchanged.

### C5/C6 seam (no implementation)

- `OptionsRow`, `options_row_name`, and `OptionsMenu::adjust`'s dispatch are the extension points.
  A later ticket adds rows (e.g. `CalibrateOffset`, `RemapInput`) plus a transition to a new screen;
  this plan deliberately adds **no** placeholder rows and **no** new `ScreenId`.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| `speed_mod` / `scroll` / `fail_enabled` field names + default `"1x"`/`"up"`/`true` | `src/data/config.hpp:41-45` (C2) | Sourced |
| `"up"`/`"down"` scroll string contract | `config_loader.cpp:228-231`, `gameplay_options.cpp:18-19` | Sourced |
| `"1x"`/`"C400"`/`"M600"` accepted forms | `src/gameplay/speed_mod.cpp:59-95` | Sourced |
| X step `0.25` clamp `[0.25,10]`; C/M step `10` clamp `[100,1000]` | **Unsourced** (UI feel; OpenITG not consulted for menu steps) | **Flagged OQ2** |
| Options open binding `Tab` + shoulder buttons | **Unsourced** (no reference parity requirement) | **Flagged OQ1** |
| Per-type value memory seeded from config | ITG PlayerOptions behavior, recalled but unverified in source | Flagged OQ2 |

No judgment windows, DP weights, grade boundaries, or life deltas are introduced or changed.

---

## Patterns to Follow

### Screen lifecycle + headless event consumption (C1/C3)
```cpp
// SOURCE: src/screens/select_screen.cpp:232-272
void SelectScreen::update(ScreenContext&, double fixed_dt, const std::vector<InputEvent>& events) {
    preview_.update(fixed_dt);
    for (const InputEvent& event : events) {
        if (!event.pressed) continue;
        switch (event.action) { /* Up/Down/Left/Right/Confirm */ }
    }
}
```

### Config → gameplay options (already wired; C4 only mutates config)
```cpp
// SOURCE: src/screens/select_screen.cpp:262-264, src/gameplay/gameplay_options.cpp:9-23
ctx.play_request->options = gameplay_options_from_config(*ctx.config);
```

### Pure, SDL/GL-free helper module + test accessors
```cpp
// SOURCE: src/screens/select_screen.hpp:17-30, src/gameplay/speed_mod.hpp:1-6
[[nodiscard]] std::string format_bpm_range(const TimingData&);   // header-only pure helper style
```

### Screen back hook (new, defaulted additive)
```cpp
// existing default navigation to keep: src/screens/screen_manager.cpp:122-131
} else if (active_id_ == ScreenId::Select) {
    transition_to(ScreenId::Title);
}
```

### Input mapping table (hardcoded, not config-driven)
```cpp
// SOURCE: src/input/input_manager.cpp:52-71
key_map_[SDLK_RETURN] = GameAction::Confirm;
gamepad_button_map_[SDL_GAMEPAD_BUTTON_START] = GameAction::Confirm;
```

### Test idiom + registration
```cpp
// SOURCE: tests/screen_manager_test.cpp:24-31, tests/CMakeLists.txt:176-184
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/options_menu.hpp` | CREATE | Pure `OptionsMenu` state + `OptionsRow` enum + adjust/format/apply helpers (header-only-ish; no SDL/GL) |
| `src/screens/options_menu.cpp` | CREATE | Row dispatch, per-type value memory/steps/clamps, config<->menu mapping, display strings |
| `src/screens/select_screen.hpp` | UPDATE | Add overlay members (`OptionsMenu options_`, `bool options_open_`), test accessors, `handle_back` override decl |
| `src/screens/select_screen.cpp` | UPDATE | Open/close on `Options`, route modal input, apply to config, render panel, override `handle_back` |
| `src/input/input_event.hpp` | UPDATE | Add `GameAction::Options` + `action_to_string` case |
| `src/input/input_manager.cpp` | UPDATE | Map `Tab` + `SDL_GAMEPAD_BUTTON_LEFT_SHOULDER`/`RIGHT_SHOULDER` → `Options` |
| `src/screens/screen.hpp` | UPDATE | Add defaulted `virtual bool handle_back(ScreenContext&) { return false; }` |
| `src/screens/screen_manager.cpp` | UPDATE | `handle_back()` consults the active screen's hook first |
| `src/screens/screen_manager.hpp` | UPDATE | (comment only) document the hook in the back-navigation contract |
| `CMakeLists.txt` | UPDATE | Add `src/screens/options_menu.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `options_menu_test` |
| `tests/options_menu_test.cpp` | CREATE | Pure model: seeding, row/value adjust, clamps, formatting, apply/round-trip, invalid input |
| `tests/select_screen_test.cpp` | UPDATE | Open/adjust/close via real `ScreenManager`; wheel suspended; Back closes then second Back → Title; Confirm publishes changed options; save/load persistence; empty-library safety |
| `tests/input_test.cpp` | UPDATE | Assert `Tab` and shoulder map to `GameAction::Options` |

Not modified: `src/main.cpp` (exit save already persists `game_config`), `src/data/config.hpp`,
`src/data/config_loader.*` (bindings untouched), `src/gameplay/*` (options already consumed),
`src/timing/*`, `src/chart/*`, `src/render/*`, existing `select_placeholder_screen.*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Add the `Options` action and bindings

- **Files**: `src/input/input_event.hpp`, `src/input/input_manager.cpp`
- **Action**: UPDATE
- **Implement**: add `Options,` to `enum class GameAction` before `None`, and
  `case GameAction::Options: return "Options";` in `action_to_string`. In
  `setup_default_mappings()` add `key_map_[SDLK_TAB] = GameAction::Options;`,
  `gamepad_button_map_[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = GameAction::Options;`, and the same for
  `SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER`. Do **not** touch `default_key_bindings()` /
  `default_gamepad_bindings()` (keep `config_persistence_test`'s 6/2 assertion; bindings are unused
  by `InputManager`).
- **Mirror**: `src/input/input_manager.cpp:52-71`, `src/input/input_event.hpp:8-24`.
- **Validate**: `cmake --build build -j16` (no new `-Wswitch` warning).

### Task 2: Pure `OptionsMenu` model

- **Files**: `src/screens/options_menu.hpp`, `src/screens/options_menu.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  // options_menu.hpp  (pure: <string> + gameplay/speed_mod.hpp only)
  namespace blaze4k {
  struct GameConfig;
  enum class OptionsRow : int { SpeedType = 0, SpeedValue, Scroll, Fail, Count };
  inline constexpr int kOptionsRowCount = static_cast<int>(OptionsRow::Count);

  struct OptionsMenu {
      SpeedModType speed_type = SpeedModType::XMod;
      double x_value = 1.0, c_value = 400.0, m_value = 400.0;
      bool scroll_down = false; bool fail_enabled = true; int row = 0;
      [[nodiscard]] double speed_value() const;   // active type's slot
      void set_speed_value(double v);             // clamp + store active slot
  };
  [[nodiscard]] OptionsMenu options_menu_from_config(const GameConfig&);
  void options_menu_apply(const OptionsMenu&, GameConfig&);
  void options_menu_move_row(OptionsMenu&, int delta);   // clamp [0, Count)
  void options_menu_adjust(OptionsMenu&, int delta);     // row-specific
  [[nodiscard]] double options_speed_step(SpeedModType);
  [[nodiscard]] std::string options_row_name(int row);
  [[nodiscard]] std::string options_row_value_text(const OptionsMenu&, int row);
  } // namespace blaze4k
  ```
  `options_menu_from_config` uses `parse_speed_mod(config.gameplay.speed_mod, mod)`; on success sets
  `speed_type` + the matching slot to `mod.value`. `scroll_down = (config.gameplay.scroll=="down")`,
  `fail_enabled = config.gameplay.fail_enabled`. `options_menu_apply` writes
  `config.gameplay.speed_mod = format(...)` (`"1.5x"`/`"C400"`/`"M400"` via a local
  `format_speed_mod`), `.scroll = scroll_down ? "down" : "up"`, `.fail_enabled`. Steps/clamps are
  `constexpr` tables (X `0.25`/`[0.25,10]`; C,M `10`/`[100,1000]`). No exceptions; never throws.
- **Mirror**: `src/gameplay/speed_mod.{hpp,cpp}` (pure model + parse), `src/data/config.hpp:41-45`.
- **Validate**: `cmake --build build -j16` (after Task 5 registers the `.cpp`).

### Task 3: `Screen::handle_back` seam

- **Files**: `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`, `src/screens/screen_manager.hpp`
- **Action**: UPDATE
- **Implement**: add to `Screen`:
  ```cpp
  // Consume Back for an in-screen modal/sub-state; return true to suppress the
  // manager's default back navigation. Default false preserves C1 behavior.
  virtual bool handle_back(ScreenContext& /*ctx*/) { return false; }
  ```
  In `ScreenManager::handle_back()` add at the top:
  ```cpp
  if (Screen* active = active_screen(); active != nullptr && active->handle_back(ctx_)) {
      return;
  }
  ```
  Update `screen_manager.hpp`'s back-navigation comment to mention the hook. Keep
  `back_navigates()` unchanged (Select still returns true, so App won't quit; the hook decides
  close-vs-navigate inside the manager).
- **Mirror**: `src/screens/screen_manager.cpp:122-138`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` still passes.

### Task 4: Wire the overlay into `SelectScreen`

- **Files**: `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Members `OptionsMenu options_; bool options_open_ = false;`; test accessors
    `bool options_open() const`, `const OptionsMenu& options_menu() const`.
  - `update`: still `preview_.update(fixed_dt)` first. If `options_open_`: for each press —
    `Options`/`Back` → close (`options_open_=false`); `Up/Down` → `options_menu_move_row(±1)`;
    `Left/Right` → `options_menu_adjust(∓1)`; `Confirm` → `options_menu_adjust(+1)`; after any
    change call `options_menu_apply(options_, *ctx.config)` when `ctx.config != nullptr`; then
    `continue` (no wheel nav). Else (closed): add `case GameAction::Options:` →
    `options_ = options_menu_from_config(*ctx.config ? *ctx.config : GameConfig{}); options_open_=true;`
    (guard null config) before the existing cases.
  - `handle_back(ctx) override`: if `options_open_` → apply to `ctx.config`, close, `return true`;
    else `return false`.
  - `enter`: `options_open_ = false;` (also reset on re-entry after attract/gameplay). `exit`:
    `options_open_ = false;`.
  - `render`: if `options_open_`, draw the panel (title "OPTIONS", each row as
    `name + "   " + value` with the active row highlighted, footer
    `"[UP/DOWN] ROW  [LEFT/RIGHT] CHANGE  [ENTER] NEXT  [BACK] CLOSE"`) and return; otherwise the
    existing wheel drawing. Headless renderer stays a no-op.
- **Mirror**: `src/screens/select_screen.cpp:201-272` (lifecycle/event loop), `title_screen.cpp:37-74`
  (headless-safe centred draw); `src/render/bitmap_font.hpp:19-24`.
- **Validate**: `cmake --build build -j16`.

### Task 5: Register the new source and test

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/screens/options_menu.cpp` to the `blaze4k_core` list
  (`CMakeLists.txt:114-119` area); append an `options_menu_test` block mirroring
  `tests/CMakeLists.txt:176-184`.
- **Mirror**: `CMakeLists.txt:80-120`, `tests/CMakeLists.txt:176-184`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 6: `options_menu_test` (pure)

- **File**: `tests/options_menu_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no SDL/GL/audio):
  1. **Seeding** — config `speed_mod="C400"`, `scroll="down"`, `fail_enabled=false`, offset `0.02` →
     `speed_type==CMod`, `c_value==400`, `scroll_down`, `!fail_enabled`, `x_value==1.0`, `m_value==400`.
  2. **Invalid speed** — `speed_mod="zzz"` → all slots default, `speed_type==XMod` (no crash).
  3. **Row nav** — `move_row(+1)` walks `0→1→2→3` and clamps at 3; `move_row(-1)` clamps at 0.
  4. **Type cycle** — `adjust` on `SpeedType`: X→C→M→X; per-type slots preserved across switches.
  5. **Value step/clamp** — X from `1.0`: `+1` → `1.25`; floor at `0.25`; ceiling at `10.0`. C from
     `400`: `+1` → `410`; clamp `[100,1000]`.
  6. **Toggles** — `Scroll` and `Fail` flip on ±1.
  7. **Formatting** — `options_row_value_text` returns `"XMOD"/"CMOD"/"MMOD"`, `"1x"/"1.5x"/"C400"/"M400"`,
     `"UP"/"DOWN"`, `"ON"/"OFF"`.
  8. **Apply + round-trip** — `options_menu_apply` writes expected strings; `parse_speed_mod` on the
     result round-trips; `options_menu_apply` then `options_menu_from_config` returns an equal menu.
- **Mirror**: `tests/select_screen_test.cpp:24-31,214-234`.
- **Validate**: `./build/tests/options_menu_test` → 0.

### Task 7: Extend `select_screen_test` (integration, real manager + C2 path)

- **File**: `tests/select_screen_test.cpp`
- **Action**: UPDATE
- **Implement** (reuse the existing temp-pack fixture and `manager`):
  1. **Open** — press `GameAction::Options` → `select->options_open()`; press `Down` → song index
     unchanged (wheel suspended).
  2. **Adjust** — while open, set the `SpeedType` row to CMod and step to `400`, toggle scroll to
     `DOWN`, fail to `OFF`; assert the shared `config.gameplay.speed_mod == "C400"`, `scroll == "down"`,
     `!fail_enabled` after each.
  3. **Back closes** — press `Back` → `options_open()==false` and `manager.active_id()==ScreenId::Select`
     (the `handle_back` hook consumed it). A second `Back` → `ScreenId::Title` (existing behavior).
  4. **Gameplay applies** — return to Select, `Confirm` → the published `PlayRequest.options` has
     `SpeedModType::CMod`, `value==400`, `ScrollDirection::Down`, `fail_enabled==false` (AC2).
  5. **Persist** — `save_config(tmp/config.json, config)` then `load_config(...)` and assert the three
     gameplay fields survive (AC3, real C2 path).
  6. **Empty library + options** — the existing empty-library case additionally opens the overlay,
     adjusts, closes, renders (uninitialized renderer), no crash.
- **Mirror**: `tests/select_screen_test.cpp:236-330`; `tests/config_persistence_test.cpp:118-145`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect **21/21**).

### Task 8: Input mapping assertion

- **File**: `tests/input_test.cpp`
- **Action**: UPDATE
- **Implement**: after the DFJK block, synthesize `SDLK_TAB` key-down and
  `SDL_GAMEPAD_BUTTON_LEFT_SHOULDER` button-down; assert `action_for_key(SDLK_TAB)==GameAction::Options`
  and the polled event action is `Options`. Keep all existing assertions.
- **Mirror**: `tests/input_test.cpp:64-105`.
- **Validate**: `./build/tests/input_test` → 0.

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 21/21: 20 existing + options_menu_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/options_menu_test
./build/tests/select_screen_test
./build/tests/input_test
./build/tests/screen_manager_test

# Purity: options/select logic must stay free of SDL/GL/audio/clock
rg -n "SDL_|glad|miniaudio|chrono|GetTicks" src/screens/options_menu.* src/screens/select_screen.*
# expected: no matches

# Warning budget (no -Wswitch on GameAction)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless, non-blocking (no window/GL/audio device), and isolated via `--data-dir` so
the developer's real `data/` and library are untouched.

1. **Real binary boots into Select with the new code** (no regression):
   ```bash
   rm -rf /tmp/blaze4k-e2e-opt
   ./build/blaze-4k --headless --smoke-test 30 \
     --start-screen select --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-opt
   # exit 0; logs "[SongLibrary] ...", "[SelectScreen] library: 4 songs, 6 charts",
   # "[ScreenManager] enter Select"; window/GL/audio device not required
   ```
2. **Config round-trip through the real exit-save path** (AC3 wiring):
   ```bash
   # seed gameplay options, boot + clean-exit, confirm they survive
   printf '{"version":1,"gameplay":{"speed_mod":"C400","scroll":"down","fail_enabled":false}}\n' \
     > /tmp/blaze4k-e2e-opt/config.json
   ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir /tmp/blaze4k-e2e-opt
   # exit 0; /tmp/blaze4k-e2e-opt/config.json still contains "C400", "down", false
   ```
3. **Menu adjust + Back + persistence via the real screen/machine and C2 path** (AC1/AC3/AC4):
   ```bash
   ./build/tests/select_screen_test
   # opens the overlay with GameAction::Options, suspends the wheel, adjusts
   # SpeedType=CMOD / value=400 / scroll=DOWN / fail=OFF into the shared GameConfig,
   # asserts Back closes without leaving Select (second Back -> Title),
   # saves + reloads config.json and confirms the three fields persist
   ```
4. **Gameplay applies the changed options** (AC2):
   ```bash
   ./build/tests/select_screen_test   # Confirm publishes PlayRequest.options {CMod,400,Down,fail=false}
   ./build/tests/options_menu_test    # apply -> parse_speed_mod round-trip
   # gameplay mod math (B3) and fail behavior (B6) are unchanged and already consume these values
   ```
5. **Pure model unit coverage**:
   ```bash
   ./build/tests/options_menu_test
   # seeding, invalid speed fallback, row/value clamping, type cycle + per-type memory,
   # toggles, display text, apply/round-trip
   ```
6. **Input mapping** (pad/keyboard reachability):
   ```bash
   ./build/tests/input_test   # Tab -> Options, shoulder -> Options, existing mappings intact
   ```
7. **Regression**:
   ```bash
   ctest --test-dir build --output-on-failure   # 21/21; metronome_sync_test/music_clock_test/
   # judgment_engine_test/life_keeper_test/config_persistence_test unchanged
   ```
8. `git status` shows only additions under `src/screens/`, edits to
   `input_event.hpp`/`input_manager.cpp`/`screen*.{hpp,cpp}`/`select_screen.*`, the CMake files, and
   the test files — no changes to `src/main.cpp`, `src/timing/`, `src/chart/`, or gameplay/scoring.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The `Screen::handle_back` hook changes the C1 manager contract | Default `false` reproduces every existing behavior; `screen_manager_test` (spies + `test_real_screens`) re-run in Task 3/7 | **In scope** |
| Back on Select with the panel open would navigate to Title instead of closing | The hook is exactly why it exists; explicitly asserted in Task 7 case 3 | **In scope** |
| A bare USB dance pad may lack shoulder buttons, so the overlay cannot be opened pad-only | All *in-menu* changes use only directions/Confirm/Back (already pad-mapped). Opening uses Tab + shoulders; flagged **OQ1** with a fallback (hold-Back gesture) if the reference pad lacks shoulders | **In scope** — flagged |
| Speed steps/clamps are not OpenITG-sourced | Constant tables in one place; flagged **OQ2**; changing them touches only `options_menu.cpp` + its test | **Flagged OQ2** |
| A stored speed value not on the step grid (e.g. `"C375"`) is displayed verbatim | Seeding stores the parsed value; display shows the slot until adjusted, then it snaps to the grid. Acceptable; flagged **OQ2** | **In scope** — flagged |
| Old `config.json` lacks an `"Options"` binding → menu unopenable? | `InputManager` uses a hardcoded map, not config bindings (verified), so the action always works; config defaults are left untouched | **In scope** |
| Adding a `GameAction` triggers a `-Wswitch` warning | `action_to_string` gets the new case; build checked in Task 1/5 | **In scope** |
| Idle-attract fires while the panel is open (Select is idled) | `options_open_` is reset on `enter`; changes are already applied, so Attract is harmless. Suppressing attract while modal is out of scope | **Out of scope** — flagged |
| Panel opened and Back pressed in the same tick (manager sees Back before `update` opens it) | Single-frame, two-key edge case; documented; the open key is not Back in practice | **Out of scope** — flagged |
| Offset wizard / remapping creep in | Rows enum + dispatch are the seam; no placeholder rows, no new `ScreenId`, C5/C6 not implemented | **Out of scope** — by contract |

---

## Decisions

- **Overlay inside `SelectScreen`, not a new `ScreenId`.** The issue says "options menu on the select
  screen"; an overlay (with a defaulted `Screen::handle_back` hook) keeps Select the owner and avoids
  expanding the state machine. Revisit only if C5/C6 prefer a dedicated screen (OQ6).
- **One new `GameAction::Options`, hardcoded in `InputManager`.** Config bindings are opaque/unused
  today, and changing the defaults would break `config_persistence_test`; C6 owns real remapping.
- **Immediate apply to `ctx.config`, persist on clean exit.** Reuses `main.cpp`'s existing
  `save_config`; no new context field or save call, matching the AC ("when the game exits").
- **Per-type value memory** (X/C/M slots) to avoid nonsense cross-type values; seeded from the parsed
  config.
- **Pure, data-driven option tables** in `options_menu.cpp`, unit-tested headless.

---

## Open Questions

1. **Non-blocking — open-options binding on a bare dance pad.** No free pad button exists (D-pad +
   face = directions, Start = Confirm, Back = Back). Proposed: `Tab` (keyboard) +
   `LEFT_SHOULDER`/`RIGHT_SHOULDER` (gamepad), with all in-menu navigation on the shared
   directions/Confirm/Back. If the reference pad has no shoulder, specify an alternative (e.g.
   short-hold Back detected via `action_down`) — that adds gesture timing, so it is not planned here.
2. **Non-blocking — speed step/clamp values.** No OpenITG source consulted for the menu step grid.
   Proposed X `0.25`/`[0.25,10]`, C & M `10`/`[100,1000]`. Confirm or supply the reference values
   (+ commit) if strict parity is wanted.
3. **Non-blocking — apply-on-change vs confirm/cancel.** Proposed immediate apply (no cancel).
   Confirm no "discard" semantics are required.
4. **Non-blocking — row navigation clamp vs wrap.** Proposed clamp at the ends (consistent with the
   difficulty list). Confirm wrap if desired.
5. **Non-blocking — save timing.** The AC only needs persistence on exit; changes are lost on a
   hard kill. Proposed relying on the existing clean-exit save. Say if an immediate save-on-close is
   wanted (would need the config path in `ScreenContext`).
6. **Non-blocking — overlay vs separate `ScreenId::Options`.** Proposed overlay for the literal
   "on the select screen" wording. If C5/C6 would rather own a first-class options screen, switch to
   a new `ScreenId` and drop the `handle_back` hook.

---

## Acceptance Criteria

- [ ] Opening options on Select exposes Speed Type (X/C/M), Speed Value, Scroll, and Fail, all
      adjustable (Tasks 2/4; `options_menu_test`; `select_screen_test` cases 1–2)
- [ ] Confirm on Select publishes `PlayRequest.options` reflecting the changed config, so gameplay
      applies them via B3 mod math and B6 fail (Task 7 case 4; no gameplay code changed)
- [ ] Changed options survive a clean exit via `config.json` (Task 7 case 5; E2E 2–3)
- [ ] The menu is fully reachable/changeable with pad-mapped actions only (directions/Confirm/Back)
      (Tasks 1/4; `input_test`; flagged OQ1 for the open binding)
- [ ] `ctest --test-dir build --output-on-failure` → **21/21**; `--gameplay-demo`, `screen_manager_test`,
      and all prior tests stay green (Task 7; E2E 7)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic`; `options_menu`/`select_screen` stay
      SDL/GL/audio/clock-free; C5/C6 not implemented (row enum + `handle_back` are the seam)
