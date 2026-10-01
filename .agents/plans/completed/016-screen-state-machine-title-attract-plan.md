# Plan: Screen State Machine with Title and Attract Screens (C1)

## Summary

Build the arcade shell foundation: a **`Screen` interface + `ScreenManager` state machine** in the
currently-empty `src/screens/`, plus fully-implemented **Title** and **Attract** screens and a minimal
**placeholder Song Select** screen so the boot flow is explicit end-to-end. Each screen is an object
with `enter/update/render/exit` and transitions are requested explicitly (PRD §6 pattern 4; PRD §7.3).

The manager owns screens by `ScreenId`, applies transitions only at safe frame boundaries (exit → enter
ordering), and implements the **idle-attract policy** centrally: when the active screen is `Title` or
`Select` and no input arrives for a configurable timeout, it transitions to `Attract`, remembering where
it came from so a Start/Confirm press returns there. Attract for C1 is a **title loop** (animated
logo/receptor quads), which PRD §7.3 explicitly permits ("gameplay autoplay **or** title loop"); real
autoplay is deferred (flagged).

Screens render through the existing `GlQuadRenderer` (a documented headless no-op), so all behavior is
testable with no window, no GL context, and no audio device. UI text needs letters the current 5×7 HUD
font lacks (`PRESS START`, `BLAZE 4K`), so the bitmap font is extracted into a reusable
`src/render/bitmap_font.{hpp,cpp}` with a full A–Z set and `hud_renderer.cpp` is switched to it
(behavior-preserving for the HUD).

C1 is the first Phase C ticket and **blocks C3/C4/C7**, so it defines the seam those tickets build on:
`ScreenId` (Title/Attract/Select/Gameplay/Results), the `Screen` lifecycle contract, and the
`ScreenManager` transition API. Song select, gameplay, results, persistence, options, and remapping are
**out of scope** and owned by their own tickets.

## User Story

As a player
I want the game to boot to a title screen and idle into an attract/title loop
So that it feels like an arcade cabinet in my living room.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/` (NEW: interface, manager, title, attract, select-placeholder), `src/render/` (NEW shared bitmap font), `src/app/` (event-callback hook), `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #16 ([C1]) |
| PRD refs | §6 (architecture, pattern 4), §7.3 Screen flow, §5 story 5, §12 Phase C |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3 3.2.8, glad (GL 3.3 core), miniaudio 0.11.21, nlohmann_json 3.11.3, stb fetched |
| Baseline tests | **15/15 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 15" (0.26 s), recorded this run |
| `src/screens/` | **exists, empty** | Add new sources here; register in `blaze4k_core` |
| Test registration | `tests/CMakeLists.txt:1-154` | Add one block (`add_executable` / `target_link_libraries(... blaze4k_core)` / `add_test`) |
| App callbacks | `src/app/app.hpp:34-38`, `src/app/app.cpp:119-138` | `set_update_callback` / `set_render_callback`; `on_event` is a protected no-op virtual |
| Headless renderer | `src/render/gl_quad_renderer.hpp:12-13,22-36` | Uninitialized renderer draws are safe no-ops; `begin/end` must be gated on `is_initialized()` (see `src/main.cpp:125-129`) |
| HUD font | `src/gameplay/hud_renderer.cpp:32-94` | Private 5×7 glyph table + `draw_text`; lacks most letters needed for screen text |
| Input actions | `src/input/input_event.hpp:8-24`, `src/input/input_manager.cpp:52-71` | `Confirm` = Enter/KP-Enter/Start; `Back` = Escape/Back |
| Escape handling | `src/app/app.cpp:102-106` | App currently quits on Escape key-down; `input_manager` also emits `Back` — see Risk/OQ2 |
| Reference chart (if ever needed) | `tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm` | Present; **not** required by C1's title-loop attract |

**Start green, stay green:** 15 tests pass; this plan adds 1 test target (`screen_manager_test`) →
**16 expected**. Changes to `app.cpp`, `main.cpp`, `hud_renderer.cpp`, and CMake are additive except the
Escape-delegation hook, whose default path preserves A2's existing behavior.

---

## Pinned Semantics

Authority: **PRD** (this project's contract) for the screen flow/lifecycle; existing Blaze 4k code for the
interfaces C1 must mirror. **No new gameplay constants** are introduced.

### Screen lifecycle (PRD §6 pattern 4)

- Screens are objects with `enter/update/render/exit` (`PRD.md:179`).
- Attract is **just another screen** in the machine — no special casing (`PRD.md:179`;
  issue #16 comment). Idle policy lives in the `ScreenManager`, not inside `TitleScreen`.
- Screen flow: `Title → Attract → Select → Gameplay → Results` (`PRD.md:136,352`).
- Title: logo + "Press Start"; Attract: idle demo "gameplay autoplay **or** title loop" (`PRD.md:198-199`).

### Transition & lifecycle ordering contract (C1 defines this for C3/C4/C7)

- A screen requests a transition via `ctx.manager->transition_to(id)`; the manager **defers** it so a
  screen is never destroyed inside its own `update`.
- Transitions are applied at frame boundaries: the outgoing screen's `exit()` is called, then the
  incoming screen's `enter()`. No screen is entered twice; `start()` counts as an enter.
- Transitioning to an unregistered `ScreenId` is a logged no-op (stay put) — the machine never crashes
  on a forward reference to a screen a later ticket will add.

### Input contract

- `Confirm` (`GameAction::Confirm`) confirms; `Back` (`GameAction::Back`) navigates back (`input_event.hpp:8-24`).
- Menu screens consume `Confirm` as pressed events with **no** wall-clock/render-frame dependency
  (`InputEvent` carries the SDL nanosecond timestamp; screens ignore it — navigation is edge-triggered).
- Idle is measured only from the **fixed-timestep `fixed_dt`** accumulator passed to `update` — never
  wall-clock, never `SDL_GetTicks`, never frame delta (AGENTS.md core principle 1's spirit; screens are
  not the judgment path but the same discipline applies).

### Headless contract

- `render()` always receives a `GlQuadRenderer`; when it is uninitialized every draw is a no-op
  (`gl_quad_renderer.hpp:12-13`). Screens must therefore render unconditionally when given screen dims
  and never require a live GL context to construct or update.

---

## Value Provenance

C1 introduces **no upstream numeric game constants**. The only tunable is the UX idle timeout, which is
not pinned by any reference source (flagged in Open Questions).

| Value / Semantic | Value | Source |
|------------------|-------|--------|
| Screen lifecycle | `enter/update/render/exit` | `PRD.md:179` (PRD §6 pattern 4) |
| Screen flow | Title → Attract → Select → Gameplay → Results | `PRD.md:136,352` (PRD §6/§7.3) |
| Attract content | "gameplay autoplay **or** title loop" | `PRD.md:199` (PRD §7.3) |
| Button for Start | `GameAction::Confirm` | `src/input/input_event.hpp:16`; defaults `input_manager.cpp:53-54,70` |
| Button for Back | `GameAction::Back` | `src/input/input_event.hpp:17`; defaults `input_manager.cpp:55,71` |
| Headless render no-op | uninitialized renderer safe | `src/render/gl_quad_renderer.hpp:12-13` |
| Idle timeout (screens) | **proposed `30.0 s`**, configurable | Blaze 4k UX choice — **unsourced**, OQ1 |

---

## Patterns to Follow

### App callback hooks (the seam main.cpp already uses)
```cpp
// SOURCE: src/app/app.hpp:34-38
using UpdateCallback = std::function<void(double fixed_dt)>;
using RenderCallback = std::function<void(double alpha)>;
void set_update_callback(UpdateCallback cb);
void set_render_callback(RenderCallback cb);
```

### Headless renderer gating (must be preserved around `begin/end`)
```cpp
// SOURCE: src/main.cpp:125-149
if (!app.window().is_headless()) { if (!quad_renderer.init()) { ... } }
...
app.set_render_callback([&](double) {
    if (quad_renderer.is_initialized()) quad_renderer.begin(w, h);
    ... render ...
    if (quad_renderer.is_initialized()) quad_renderer.end();
});
```

### Text/quad drawing primitive (to be generalized)
```cpp
// SOURCE: src/gameplay/hud_renderer.cpp:75-94
void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y, float pixel,
               Color color);
// SOURCE: src/gameplay/hud_renderer.cpp:213-227
renderer.draw_quad(Rect{...}, color);
```

### Headless App smoke pattern (tests)
```cpp
// SOURCE: tests/app_test.cpp:82-104
blaze4k::AppConfig cfg; cfg.window.headless = true; cfg.smoke_test_frames = 10;
blaze4k::App app(cfg);
app.set_update_callback([&](double){ update_count++; });
app.set_render_callback([&](double){ render_count++; });
TEST_CHECK(app.init()); app.run();
TEST_CHECK(render_count == 10); TEST_CHECK(!app.is_running());
```

### Existing test assertion idiom
```cpp
// SOURCE: tests/app_test.cpp:30-36 (identical in input_test.cpp:8-14)
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" \
    << __LINE__ << ": " #expr << "\n"; std::abort(); } } while (0)
```

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:80-106 / tests/CMakeLists.txt:136-144
add_library(blaze4k_core STATIC ... src/gameplay/life_keeper.cpp ...)
add_executable(life_keeper_test life_keeper_test.cpp)
target_link_libraries(life_keeper_test PRIVATE blaze4k_core)
add_test(NAME life_keeper_test COMMAND life_keeper_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/bitmap_font.hpp` | CREATE | Reusable `draw_text`/`text_width` + full A–Z 5×7 glyph set |
| `src/render/bitmap_font.cpp` | CREATE | Glyph table + text drawing (moved/generalized from `hud_renderer.cpp`) |
| `src/screens/screen.hpp` | CREATE | `ScreenId`, `ScreenContext`, `Screen` interface (`enter/update/render/exit`) |
| `src/screens/screen_manager.hpp` | CREATE | `ScreenManager` API: registry, transitions, idle-attract policy |
| `src/screens/screen_manager.cpp` | CREATE | Deferred transition application, lifecycle ordering, idle accumulation, logging |
| `src/screens/title_screen.hpp` | CREATE | Title screen class |
| `src/screens/title_screen.cpp` | CREATE | Render logo + blinking "PRESS START"; Confirm → Select |
| `src/screens/attract_screen.hpp` | CREATE | Attract (title-loop) screen class |
| `src/screens/attract_screen.cpp` | CREATE | Animated logo/receptor loop; Confirm → return screen |
| `src/screens/select_placeholder_screen.hpp` | CREATE | Minimal placeholder for the Song Select slot (C3 replaces it) |
| `src/screens/select_placeholder_screen.cpp` | CREATE | Renders "SONG SELECT"; Back → Title; Confirm logs "C3 pending" |
| `src/gameplay/hud_renderer.cpp` | UPDATE | Use shared `bitmap_font` instead of its private glyph copy (behavior-preserving) |
| `src/app/app.hpp` | UPDATE | Add optional event callback returning "consumed" (for Escape delegation) |
| `src/app/app.cpp` | UPDATE | Quit on Escape **only if** no event callback consumes it (A2 default preserved) |
| `src/main.cpp` | UPDATE | Build/register the ScreenManager as the default path; add `--attract-timeout`; keep `--gameplay-demo` override |
| `CMakeLists.txt` | UPDATE | Add the new `src/screens/*.cpp` and `src/render/bitmap_font.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `screen_manager_test` |
| `tests/screen_manager_test.cpp` | CREATE | Headless lifecycle/transition/idle/real-screen/App-smoke tests |

`src/gameplay/life_keeper.*`, `score_keeper.*`, `judgment_engine.*`, `src/timing/*`, `src/input/*`, and
`src/render/gl_quad_renderer.*` are **not modified**.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Shared bitmap font

- **Files**: `src/render/bitmap_font.hpp`, `src/render/bitmap_font.cpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: CREATE (first two), UPDATE (third)
- **Implement**:
  - Header `namespace blaze4k`: declare
    ```cpp
    // 5x7 glyph bitmap font. Coordinates are pixels, top-left origin (y down).
    [[nodiscard]] float text_width(const std::string& text, float pixel);
    void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y,
                   float pixel, Color color);
    void draw_text_centered(GlQuadRenderer& renderer, const std::string& text, float center_x,
                            float y, float pixel, Color color);
    ```
    and forward-declare `class GlQuadRenderer;` (`#include "render/geometry.hpp"` for `Color`).
  - cpp: move the `Glyph` struct + `glyph_for` from `hud_renderer.cpp:32-69`, **extend** to the full
    uppercase alphabet `A–Z`, digits `0–9`, space, and the punctuation already present (`. % x - + *`).
    Unknown chars draw nothing but still advance the cursor.
  - `hud_renderer.cpp`: delete its private `kGlyphs`, `glyph_for`, `text_width`, `draw_text`
    (`hud_renderer.cpp:32-94`); `#include "render/bitmap_font.hpp"`; keep `format_*` and both `render`
    methods unchanged (same call sites: `:153,157,172,192`).
- **Mirror**: `src/gameplay/hud_renderer.cpp:32-94` (exact glyph mechanics), `src/render/gl_quad_renderer.hpp:30`.
- **Validate**: `cmake --build build -j16` then `ctest --test-dir build --output-on-failure` (15/15 still).

### Task 2: Screen interface and context

- **File**: `src/screens/screen.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  #pragma once
  #include <optional>
  #include <string_view>
  #include <vector>
  #include "input/input_event.hpp"

  namespace blaze4k {
  class ScreenManager;
  class GlQuadRenderer;

  // Stable identity of every arcade screen (PRD section 6/7.3). C1 implements
  // Title/Attract and a Select placeholder; C3/C4/C7 replace/extend the rest.
  enum class ScreenId { Title, Attract, Select, Gameplay, Results };

  [[nodiscard]] std::string_view screen_id_name(ScreenId id);

  // Services a screen may use. Kept small and free of SDL/GL so screens are
  // constructible and updatable headless. C2/C3 will extend it (library, config).
  struct ScreenContext {
      ScreenManager* manager = nullptr;
  };

  // A screen is an object with explicit enter/update/render/exit (PRD section 6
  // pattern 4). update() receives input events already polled by the App for this
  // tick; render() receives a renderer that is a safe no-op when headless.
  class Screen {
  public:
      virtual ~Screen() = default;
      [[nodiscard]] virtual ScreenId id() const = 0;
      virtual void enter(ScreenContext& /*ctx*/) {}
      virtual void update(ScreenContext& /*ctx*/, double /*fixed_dt*/,
                          const std::vector<InputEvent>& /*events*/) {}
      virtual void render(ScreenContext& /*ctx*/, GlQuadRenderer& /*renderer*/, int /*w*/,
                          int /*h*/) {}
      virtual void exit(ScreenContext& /*ctx*/) {}
  };
  } // namespace blaze4k
  ```
  - Includes only `<optional>`, `<string_view>`, `<vector>`, `input/input_event.hpp`. **No** SDL/GL.
- **Mirror**: header style of `src/gameplay/hud_renderer.hpp:1-36`.
- **Validate**: `cmake --build build -j16` (once included by Tasks 3-6).

### Task 3: ScreenManager state machine

- **Files**: `src/screens/screen_manager.hpp`, `src/screens/screen_manager.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  class ScreenManager {
  public:
      explicit ScreenManager(double idle_timeout_seconds = 30.0);

      void add_screen(std::unique_ptr<Screen> screen);          // keyed by screen->id()
      void start(ScreenId initial);                             // calls enter() once
      void transition_to(ScreenId target);                      // deferred request

      void update(double fixed_dt, const std::vector<InputEvent>& events);
      void render(GlQuadRenderer& renderer, int screen_w, int screen_h);

      [[nodiscard]] ScreenId active_id() const { return active_id_; }
      [[nodiscard]] Screen* active_screen();
      [[nodiscard]] bool has_screen(ScreenId id) const;
      [[nodiscard]] bool back_navigates() const;                // false on Title (App quits handle it)
      [[nodiscard]] ScreenId attract_return() const { return attract_return_; }

      void set_idle_timeout_seconds(double seconds);            // <=0 disables idle->Attract
      [[nodiscard]] double idle_timeout_seconds() const;
      [[nodiscard]] double idle_seconds() const { return idle_seconds_; }

  private:
      void apply_pending();          // exit() old then enter() new; logs each transition
      void handle_back();

      ScreenContext ctx_;
      std::vector<std::unique_ptr<Screen>> screens_;
      ScreenId active_id_ = ScreenId::Title;
      ScreenId pending_id_ = ScreenId::Title;
      bool has_pending_ = false;
      bool started_ = false;
      ScreenId attract_return_ = ScreenId::Title;
      double idle_timeout_seconds_ = 30.0;
      double idle_seconds_ = 0.0;
  };
  } // namespace blaze4k
  ```
  - `start(id)`: store `active_id_`, set `started_`, reset idle, call that screen's `enter(ctx_)`,
    log `[ScreenManager] enter <name>`.
  - `transition_to(id)`: `pending_id_=id; has_pending_=true;` (deferred; safe to call from inside
    `update`).
  - `apply_pending()`: if `!has_pending_` return; look up target; if missing → log
    `[ScreenManager] no screen registered for <name>; transition ignored` and clear pending; else
    call outgoing `exit(ctx_)`, update `active_id_`, reset idle, call new `enter(ctx_)`, log
    `[ScreenManager] <old> -> <new>`, clear pending.
  - `update(dt, events)`:
    1. `apply_pending()`.
    2. `const bool had_press = any event with pressed==true;`
    3. if `had_press` → `idle_seconds_ = 0.0`.
    4. scan events for `GameAction::Back && pressed` → `handle_back()` (deferred transition);
       if active is `Attract` and any `GameAction::Confirm && pressed` → `transition_to(attract_return_)`
       (manager-central; the Attract screen stays dumb).
    5. call `active_screen()->update(ctx_, dt, events)`.
    6. `apply_pending()`.
    7. idle policy: if active is `Title` or `Select` and `idle_timeout_seconds_ > 0`:
       if `!had_press && active unchanged by step 6` accumulate `idle_seconds_ += dt`; if
       `idle_seconds_ >= idle_timeout_seconds_` → `attract_return_ = active_id_; transition_to(Attract);
       apply_pending();` (idle reset inside apply/enter branch).
  - `handle_back()`: if active is `Attract` → `transition_to(attract_return_)`; else if active is
    `Select` → `transition_to(Title)`; else (Title/Gameplay/Results) → no-op.
  - `back_navigates()`: `active_id_ != ScreenId::Title && active_id_ != ScreenId::Gameplay`.
  - `render`: `active_screen()->render(ctx_, renderer, w, h)` (renderer no-ops when headless).
  - `add_screen`: `push_back`; at most one per id (last wins), no dynamic allocation on the hot path
    during transitions.
- **Mirror**: `src/app/app.cpp:51-91` (timestep/loop discipline), logging style throughout the repo.
- **Validate**: `cmake --build build -j16` (once Task 9 registers it).

### Task 4: Title screen

- **Files**: `src/screens/title_screen.hpp`, `src/screens/title_screen.cpp`
- **Action**: CREATE
- **Implement**:
  - `class TitleScreen : public Screen` with `ScreenId id() const override { return ScreenId::Title; }`,
    a `double blink_seconds_ = 0.0;` member.
  - `enter`: reset `blink_seconds_=0`; log `[TitleScreen] logo + "Press Start"`.
  - `update`: advance `blink_seconds_ += fixed_dt`; on any `Confirm && pressed` event →
    `ctx.manager->transition_to(ScreenId::Select)`.
  - `render`: guard `screen_w<=0||screen_h<=0`; draw a text "logo" — `draw_text_centered("BLAZE 4K", ...)`
    at large pixel size plus a row of four solid receptor quads below it (no asset; PRD logo is an
    open item). Blink "PRESS START" with `(static_cast<int>(blink_seconds_*2) % 2 == 0)` so it is
    deterministic in `fixed_dt`.
- **Mirror**: `src/gameplay/hud_renderer.cpp:142-196` (centered text + quad drawing).
- **Validate**: `cmake --build build -j16`.

### Task 5: Attract screen (title loop)

- **Files**: `src/screens/attract_screen.hpp`, `src/screens/attract_screen.cpp`
- **Action**: CREATE
- **Implement**:
  - `class AttractScreen : public Screen` with `id() == ScreenId::Attract`, `double phase_seconds_ = 0.0;`.
  - `enter`: reset `phase_seconds_=0`; log `[AttractScreen] title loop active (any Confirm exits)`.
  - `update`: `phase_seconds_ += fixed_dt`; Confirm is handled centrally by the manager
    (`ScreenManager::update` transitions an active Attract back to `attract_return()`), so the screen
    stays dumb with no special-casing (PRD §6 pattern 4).
  - `render`: dim full-screen backdrop quad, cycling "BLAZE 4K" text scaled/brightness-pulsed from
    `phase_seconds_`, and four receptors blinking in sequence — a pure quad/text loop (no chart, no
    audio).
- **Mirror**: `src/gameplay/hud_renderer.cpp:198-228` (quad fill + palette).
- **Validate**: `cmake --build build -j16`.

### Task 6: Placeholder Song Select screen

- **Files**: `src/screens/select_placeholder_screen.hpp`, `src/screens/select_placeholder_screen.cpp`
- **Action**: CREATE
- **Implement**:
  - `class SelectPlaceholderScreen : public Screen` with `id() == ScreenId::Select`.
  - `enter`: log `[SelectPlaceholder] Song Select not implemented yet (C3)`.
  - `update`: Back is handled centrally by the manager; on `Confirm && pressed` log
    `[SelectPlaceholder] Confirm ignored (C3 pending)` and stay.
  - `render`: "SONG SELECT" + "COMING SOON" text and a `[Back] to title` hint.
  - Purpose: makes `Title -> Select` an explicit, testable transition today. C3 replaces this file's
    registration (not the manager).
- **Mirror**: `src/gameplay/hud_renderer.cpp:142-196`.
- **Validate**: `cmake --build build -j16`.

### Task 7: App event-callback hook (Escape delegation)

- **Files**: `src/app/app.hpp`, `src/app/app.cpp`
- **Action**: UPDATE
- **Implement**:
  - `app.hpp`: add
    ```cpp
    // Returns true if the callback consumed the event (e.g. a screen handled Back).
    using EventCallback = std::function<bool(const SDL_Event&)>;
    void set_event_callback(EventCallback cb) { event_cb_ = std::move(cb); }
    ...
    EventCallback event_cb_;
    ```
  - `app.cpp` `process_events()`: in the `SDL_EVENT_KEY_DOWN`/Escape branch, call
    `event_cb_` first; `stop()` **only if** the callback is absent or returns false. All other branches
    (`QUIT`, `WINDOW_CLOSE_REQUESTED`, resize) unchanged.
  - Default behavior (no callback) is byte-identical to today, preserving A2 (`tests/app_test.cpp`).
- **Mirror**: `src/app/app.cpp:93-117`.
- **Validate**: `cmake --build build -j16`; `./build/tests/app_test` passes.

### Task 8: main.cpp wiring + CLI flags

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Default (no `--gameplay-demo`): construct `ScreenManager`, `add_screen` Title, Attract, Select
    placeholder (C1 ids); `start(ScreenId::Title)`.
  - `app.set_update_callback`: `auto ev = app.input_manager().poll_events(); shell.update(dt, ev);`
  - `app.set_render_callback`: gate `quad_renderer.begin/end` on `is_initialized()` (as
    `main.cpp:125-149`), then `shell.render(quad_renderer, w, h)`.
  - `app.set_event_callback`: if Escape key-down and `shell.back_navigates()` → return true (claim;
    screens/manager navigate via the polled `Back` event); otherwise false (App quits from Title).
  - Init `quad_renderer` when not headless (same as demo path).
  - New CLI: `--attract-timeout <seconds>` (default 30) → `shell.set_idle_timeout_seconds(...)`. Add to
    `print_help()` (`main.cpp:15-25`).
  - Keep `--gameplay-demo` as a **temporary override** that bypasses the shell (unchanged), so Phase B
    harness smoke tests continue to work.
- **Mirror**: `src/main.cpp:29-159`.
- **Validate**: `cmake --build build -j16`; headless smoke below.

### Task 9: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root `blaze4k_core` source list (after `src/data/judgment_constants_loader.cpp`, `CMakeLists.txt:105`):
    `src/render/bitmap_font.cpp`, `src/screens/screen_manager.cpp`, `src/screens/title_screen.cpp`,
    `src/screens/attract_screen.cpp`, `src/screens/select_placeholder_screen.cpp`.
  - Tests: append a `screen_manager_test` block mirroring `tests/CMakeLists.txt:136-144`.
- **Mirror**: `CMakeLists.txt:80-106`, `tests/CMakeLists.txt:136-144`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 10: Headless screen-machine test suite

- **File**: `tests/screen_manager_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK` from `tests/app_test.cpp:30-36`; no SDL/GL/audio; deterministic):
  1. **Spy screens** — file-local `SpyScreen : Screen` recording `enter/update/render/exit` counts and
     an optional forced transition; register for Title/Attract/Select/Gameplay.
  2. **Boot lifecycle** — `start(Title)`: `active_id()==Title`; Title `enter_count==1`; no `exit`.
  3. **Explicit transition + ordering** — `transition_to(Select)`; call `update`; assert
     `exit(Title)==1` then `enter(Select)==1` (record a global ordered log and assert the sequence).
  4. **Deferred application** — a spy that calls `ctx.manager->transition_to(...)` from inside its own
     `update` is not destroyed until that `update` returns (assert `update` finishes, then exit/enter).
  5. **Unregistered target** — `transition_to(Gameplay)` with no screen registered stays on the active
     id and does not crash.
  6. **Idle-attract policy** — timeout 1.0 s: step 0.1 s × 9 → still Title; ×1 more → Attract; assert
     `attract_return()` origin. `update(0.1, {Confirm pressed})` before timeout resets the accumulator
     (assert idle_seconds()==0).
  7. **Idle only from Title/Select** — register spy for Gameplay, `start(Gameplay)`, advance past
     timeout with no input → stays Gameplay.
  8. **Attract return** — from Title, idle→Attract, then `Confirm` → Title; from Select, idle→Attract,
     then `Confirm` → Select.
  9. **Back navigation** — `back_navigates()` false on Title, true on Select/Attract; `Back` event on
     Select → Title; on Attract → origin.
  10. **Render dispatch headless** — `render(uninitialized_renderer, 1280, 720)` increments the active
      screen's `render_count` and does not crash (draws are no-ops).
  11. **Real screens** — construct the actual `TitleScreen`/`AttractScreen`/`SelectPlaceholderScreen`,
      register them, and assert: Title + `Confirm` → `Select`; Attract + `Confirm` → origin; idle from
      Title → `Attract`; render headless does not crash.
  12. **Font sanity** — `text_width("PRESS START", 3.0) > 0`; `draw_text` on an uninitialized renderer
      is a no-op (no crash); `draw_text_centered` centers within 1 px of `(w - text_width)/2`.
  13. **App + shell smoke (headless)** — `AppConfig{headless=true, smoke_test_frames=30}`; App with
      update/render callbacks driving a `ScreenManager` started at Title; `init()`+`run()`; assert
      `!app.is_running()` and the manager is still on `Title` (mirrors `tests/app_test.cpp:82-104`).
- **Mirror**: `tests/app_test.cpp:1-108`, `tests/input_test.cpp:1-132`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 16/16).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 16/16: 15 existing + screen_manager_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/screen_manager_test

# Headless boot smoke (Title screen, no window, exits cleanly after 120 frames)
./build/blaze-4k --headless --smoke-test 120

# Headless attract smoke: short timeout must log the Title -> Attract transition
./build/blaze-4k --headless --smoke-test 600 --attract-timeout 1.0 2>&1 \
  | rg "ScreenManager.*(Title|Attract)"

# Purity check: screens must not read wall-clock / frame delta (fixed_dt accumulation is allowed)
rg -n "chrono|GetTicks|GetPerformanceCounter|SDL_GetTicks|std::this_thread" src/screens
# (no matches)

# Existing Phase B harness still works (shell bypassed)
./build/blaze-4k --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --smoke-test 60
```

## End-to-End Verification

1. `./build/tests/screen_manager_test` prints each sub-check and exits 0. Cases 2–5 prove the
   lifecycle/transition contract (exit→enter ordering, deferred application, unregistered-target
   safety); cases 6–9 prove the idle-attract policy and Back navigation; case 10/12 prove headless
   rendering and the shared font are safe with no GL context; case 13 proves the real `App` main loop
   boots the shell and exits cleanly.
2. `ctest --test-dir build --output-on-failure` → **16/16**; all 15 prior tests stay green (additive
   sources; the only behavioral toggle is the Escape hook, whose default keeps A2 intact).
3. `./build/blaze-4k --headless --smoke-test 120` exits 0 without opening a window and without
   blocking; stdout shows `[ScreenManager] enter Title` (boot → Title) and `Blaze 4k shut down
   cleanly.` No GL calls are attempted headless (renderer `begin/end` and all draws are gated/no-op).
4. `./build/blaze-4k --headless --smoke-test 600 --attract-timeout 1.0 2>&1 | rg "ScreenManager"`
   shows the idle `Title -> Attract` transition (≈60 simulated frames at 1/60 s), proving the idle
   timer is driven by `fixed_dt` and is observable headless.
5. The purity `rg` finds no wall-clock use in `src/screens` (idle/animation use only the injected
   `fixed_dt`), satisfying AGENTS.md core principle 1.
6. `git status` shows only additions under `src/screens/`, `src/render/bitmap_font.*`, the test, the App
   event hook, `main.cpp`, and the CMake files — no changes to `src/timing/`, `src/input/`,
   `src/gameplay/life_keeper.*`, or `gl_quad_renderer.*`.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Escape has two meanings: A2 quits the app, menus need Back | Add an `App` event callback; App quits on Escape only when it is not consumed. On `Title` it is not consumed (boot Escape still quits, preserving A2); on other screens the manager handles the polled `Back` event | **In scope** — flagged (OQ2) |
| Attract content unspecified (autoplay vs loop) and autoplay needs a chart + input generator | Implement the PRD-permitted **title loop** for C1; real gameplay autoplay deferred and flagged | **In scope** — flagged (OQ3) |
| Placeholder Select could conflict with C3's real screen | Placeholder is a separate file with a distinct id-registration in `main.cpp`; C3 replaces the registration, not the manager; documented in Tasks 6/8 | **In scope** — flagged (OQ4) |
| A screen destroyed inside its own `update` (use-after-free) if transitions apply immediately | `transition_to` is deferred and applied only at frame boundaries; test case 4 | **In scope** |
| Idle timeout value is not pinned by any reference source | Configurable via `ScreenManager`/`--attract-timeout`; proposed 30 s; OQ1 | **In scope** — flagged |
| Shared-font refactor changes HUD rendering | Glyph mechanics moved verbatim and extended; HUD draws are headless no-ops with no pixel assertions; `ctest` guards; Task 1 validation re-runs all 15 tests | **In scope** |
| Headless App smoke blocks if `smoke_test_frames` is not set | Uses existing `--smoke-test` mechanism (`app.cpp:86-89`); E2E steps 3–4 always pass an explicit frame count | **In scope** |
| Screens reading wall-clock/frame delta would violate the timing principle | Idle/animation use only `fixed_dt`; purity `rg` in Validation/E2E step 5 | **In scope** |
| `ScreenManager::render` called with an uninitialized renderer | Renderer is a documented no-op (`gl_quad_renderer.hpp:12-13`); tests 10/12 and E2E step 3 | **In scope** |
| Real gameplay autoplay, song wheel, results, persistence, options, remapping | Explicitly owned by C3/C4/C7/C2 | **Out of scope** — flagged |
| A real logo asset | Text-quad logo only; asset deferred per PRD open item | **Out of scope** — flagged |

---

## Decisions

- **ScreenManager + Screen interface define the Phase C seam.** `ScreenId` enumerates all five flow
  states now so later tickets add screens without changing the machine; C1 implements Title, Attract,
  and a Select placeholder.
- **Attract is a title loop, not gameplay autoplay.** PRD §7.3 permits either; a loop needs no chart,
  clock, or autoplay input generator, keeping C1 lean. Autoplay is flagged for C3/C7/follow-up.
- **Idle policy lives in the manager, not the screens** — Attract is "just another screen" (PRD §6
  pattern 4). The manager measures idle from `fixed_dt` and only from `Title`/`Select`.
- **Transitions are deferred and explicit**, applied at frame boundaries with `exit()` before
  `enter()`; transitioning to an unregistered screen is a logged no-op.
- **Attract return is origin-aware** (Title→Title, Select→Select), so "exits back" returns the player
  where they left off.
- **Screens are SDL/GL-free and headless-testable.** Input arrives as `std::vector<InputEvent>`;
  rendering goes through `GlQuadRenderer`'s no-op path.
- **Text is centralized** in `src/render/bitmap_font.*` with a full A–Z glyph set; `hud_renderer.cpp`
  adopts it (single source of truth for C3/C4/C7).
- **App gains an optional event callback** so Escape can be delegated to screens while preserving A2's
  default quit-on-Escape at the title screen.
- **`--gameplay-demo` stays as a temporary override** so Phase B harness smoke tests are unaffected.

---

## Open Questions

1. **Non-blocking — idle timeout duration.** Proposed `30.0 s`, configurable via
   `--attract-timeout`. Not pinned by any reference source. Confirm an arcade-like value.
2. **BLOCKING — Escape/Back semantics vs A2 AC.** A2 requires Escape to shut the app down; menus need
   `Back`. Proposed: App quits on Escape **only** when no screen consumes it; at boot (Title) it quits
   (A2 behavior preserved), while Select/Attract/Gampelay navigate back. Confirm this reading of A2.
3. **Non-blocking — Attract content.** Proposed title loop for C1; real gameplay autoplay deferred.
   Confirm a loop satisfies the issue's "demo gameplay autoplay **or** title loop".
4. **Non-blocking — placeholder Select screen.** Proposed a minimal placeholder so `Title -> Select` is
   explicit and testable now; C3 replaces its registration. Confirm this is preferable to leaving
   `Select` unregistered (which would make the transition a logged no-op).
5. **Non-blocking — attract return target.** Proposed return to the origin screen (Title or Select).
   Confirm vs always returning to Title.
6. **Non-blocking — logo presentation.** Proposed text-quad "BLAZE 4K" + receptor row; a real logo
   asset remains a PRD open item.

---

## Acceptance Criteria

- [ ] Given the game boots, a Title screen shows a logo and "Press Start" (Tasks 4/8; test 2/11)
- [ ] Given the Title screen, Start/Confirm transitions toward Song Select (Tasks 4/6; test 11)
- [ ] Given no input for the idle timeout on Title/Select, the Attract screen activates; any Start
      press exits back to the origin screen (Tasks 3/5; tests 6–8/11)
- [ ] Each screen is an object with `enter/update/render/exit` and transitions are explicit (Tasks
      2/3; tests 2–5)
- [ ] Attract is just another registered screen — no special-casing in the screens (Tasks 3/5)
- [ ] All new screens construct, update, and render headless with no GL context and no audio device
      (tests 10/12; E2E steps 3–4)
- [ ] Screens use only injected `fixed_dt`/input events — no wall-clock or frame-delta reads
      (purity `rg`; E2E step 5)
- [ ] `ctest --test-dir build --output-on-failure` → **16/16** (15 prior + `screen_manager_test`)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic`; existing Phase B `--gameplay-demo` still works
- [ ] Follows existing module/naming/test/CMake patterns; no C2/C3/C4/C7 features added
