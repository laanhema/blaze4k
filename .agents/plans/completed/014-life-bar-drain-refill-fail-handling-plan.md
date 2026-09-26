# Plan: Life Bar — Drain / Refill / Fail Handling

## Summary

Build Tundra's life layer as a **pure, event-sourced module** (PRD §6 pattern 2; AGENTS.md core
principles 1 & 2). Life is a projection of the frozen B4 `JudgmentEvent` log — never an independent
re-judgment, never wall-clock/frame driven. The work splits into a pure life core, a HUD bar, and
gameplay integration:

1. `src/gameplay/life_keeper.{hpp,cpp}` (NEW) — `LifeState` + `LifeKeeper`: consumes the B4 event log,
   groups per-note **tap events into OpenITG rows** (same row identity/semantics B5 uses), maps each
   resolved row / hold outcome / mine hit to a **delta read from the B2 `JudgmentConstants::life`
   table** (never redefined), clamps life to `[0,1]`, and derives `is_failing()` / `has_failed()`.
   Pure value types; no clocks, no platform headers.
2. `src/gameplay/hud_renderer.{hpp,cpp}` (UPDATE) — add a **life-bar element** drawn with the existing
   `GlQuadRenderer` + in-code bitmap/quad approach. No stb_truetype, no font asset.
3. `src/gameplay/gameplay_view.{hpp,cpp}` (UPDATE) — own the keeper, drain new events into it each
   `update`, honor a new `GameplayOptions::fail_enabled` (the Fail-Off option), expose a
   fail/clear outcome (B6's "transition out of gameplay" signal, consumed later by C1/C7), and render
   the life bar.
4. `src/main.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/life_keeper_test.cpp`
   (UPDATE/CREATE) — add a demo `--fail-off` flag, register the new source/test.

**Scope boundary:** B6 owns life/drain/refill/fail only. It does **not** build the Title/Attract/
Select/Results screens or the screen state machine (C1/C7), does not persist high scores (C2), does
not draw judgment/combo pop sprites (D2), and does not add the metronome sync-test chart. B4
(`judgment.*`, `judgment_engine.*`) and B5 (`score_keeper.*`) are **frozen**: `LifeKeeper` re-derives
its own row table from the B4 log + chart (exactly as B5 does), so no B4/B5 change is needed.

**Critical cross-issue requirement:** OpenITG changes life **once per completed note row**
(`Player::HandleTapRowScore`, called from `OnRowCompletelyJudged`) while B4 logs events **per note**.
B6 groups same-row tap events (row identity = exact `Note.beat`, the B5-pinned rule) so jumps and
mixed hit/miss rows cannot double-count life. Mines and hold/roll outcomes change life individually.

All behavioral semantics are pinned to **OpenITG** commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (see **Pinned Semantics** / **Value Provenance**).

## User Story

As a player
I want an ITG-style life bar that starts full, drains on bad judgments/mines, refills on good ones,
and fails the song at empty (unless Fail-Off is set)
So that the stakes match arcade play.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/gameplay/` (NEW `life_keeper*`; UPDATE `hud_renderer*`, `gameplay_view*`), `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #14 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3 3.2.8, glad (GL 3.3 core), miniaudio 0.11.21, nlohmann_json 3.11.3, stb fetched |
| Baseline tests | 13/13 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 13" (0.31s) |
| `src/gameplay/` | exists | B3/B4/B5 (`note_field`, `noteskin`, `note_field_renderer`, `gameplay_view`, `judgment*`, `score_keeper*`, `hud_renderer*`) present; add new `life_keeper*` sources |
| B2 constants | `src/timing/judgment_constants.hpp:52-63` | `LifeDeltas {fantastic 0.008, excellent 0.008, great 0.004, decent 0.0, way_off -0.050, miss -0.100, hit_mine -0.050, hold_ok 0.008, hold_ng -0.080, merciful_drain false}` — **consume as-is; never re-define** |
| B4 event log | `src/gameplay/judgment.hpp:26-36`, `judgment_engine.*` | `JudgmentEvent {kind, column, note_time_seconds, hit_time_seconds, delta_ms, window, hold, note_type, note_index}`; `drain_new_events(out)` is the incremental handoff |
| B5 scoring | `src/gameplay/score_keeper.hpp:44-55` | `reset/consume/state/is_complete`; row identity = exact `Note.beat`; closed for B6 (read-only reference) |
| Chart row identity | `src/chart/note.hpp:25-36`, `note_parser.cpp:150-158,244-248` | One `beat` per row shared across columns; `chart.notes` sorted by beat then column |
| HUD text | in-code 5×7 bitmap font | `src/gameplay/hud_renderer.cpp:29-51`; solid tinted quad is the primitive (`gl_quad_renderer.hpp`) — life bar uses quads only |
| Renderer headless guard | `src/render/gl_quad_renderer.hpp` | Uninitialized renderer is a safe no-op — HUD draws are headless-safe |
| Upstream source | `/tmp/opencode/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Cloned for provenance; arcade runtime overrides in `assets/patch-data/Themes/default/metrics.ini` |

**Start green, stay green:** 13 tests pass; this plan adds 1 test target (`life_keeper_test`) → 14 expected.

---

## Pinned Semantics

**Authority for B6 is OpenITG** (PRD §15). OpenITG's life meter is `LifeMeterBar`
(`ScreenGameplay.cpp:323-324`), driven by `SongOptions` defaults `LIFE_BAR` / `DRAIN_NORMAL` /
`FAIL_IMMEDIATE` (`SongOptions.cpp:7-10`).

### When life changes (per event vs per row)

- **Tap / hold-head / roll-head:** life changes **once per completed note row**, in
  `Player::HandleTapRowScore` → `ChangeLife(scoreOfLastTap)` (`Player.cpp:1653`), reached via
  `OnRowCompletelyJudged` (`Player.cpp:1364,1433`). Dance uses `m_bCountNotesSeparately=false`
  (`GameManager.cpp:105-108`), so a jump is one row and one life delta. The row score is the B5-pinned
  "last tap, miss dominates" value (`NoteDataWithScoring.cpp:133-190`).
- **Hold/roll outcome:** one delta per hold when the tail resolves, `ChangeLife(holdScore, tapScore)`
  (`Player.cpp:1700`). The head was already graded as its row's tap (so both apply).
- **Hit mine:** one delta immediately on trigger, `ChangeLife(TNS_HIT_MINE)` (`Player.cpp:1065`).
- **Avoided mine / roll re-hit:** no life change (`RollHit` is visual only; avoided mines only appear in
  stats — matches B5's ignored-event list).

### Delta values

- Tap windows (`LifeMeterBar.cpp:108-114`) and hold outcomes (`LifeMeterBar.cpp:169-170`) read the
  `LifeDeltaPercentChange*` preferences. **Effective arcade values** come from the runtime override
  layer (`assets/patch-data/Themes/default/metrics.ini:126-134`), which is exactly what B2 seeded:
  Marvelous/Perfect `+0.008`, Great `+0.004`, Good/Decent `0.0`, Boo/WayOff `-0.050`, Miss `-0.100`,
  HitMine `-0.050`, OK `+0.008`, NG `-0.080`. **B6 reads these from B2 only.**
- Merciful drain (`LifeMeterBar.cpp:204-205`): when enabled, a negative delta is scaled by
  `SCALE(life, 0, 1, 0.5, 1)` (`RageUtil.h:38`) — at life `0.5` the penalty is ×0.75. Arcade sets
  `MercifulDrain=0` (`metrics.ini:156`), matching B2's `merciful_drain=false`.

### Clamp, fail and Fail-Off

- After each delta: `life += delta; CLAMP(life, 0, 1)` (`LifeMeterBar.cpp:255-256`).
- `FAIL_THRESHOLD = 0` (`LifeMeterBar.cpp:18`); `IsFailing() == life <= 0` (`:290-293`).
- **Fail-enabled (FAIL_IMMEDIATE):** when life reaches 0, `bFailed` is set and the song leaves gameplay
  (`ScreenGameplay.cpp:1471-1493,1521-1531`; `SongOptions.h:24-26`). Once failed, **all further life
  deltas are zeroed** — life is frozen (`LifeMeterBar.cpp:229-231`).
- **Fail-Off:** `GetPlayerFailType == FAIL_OFF` short-circuits before any fail check
  (`ScreenGameplay.cpp:1476`); `IsPlayerDead`/`IsDying` return false (`GameState.cpp:1985-1996`). The
  song continues to the end and the player **can recover** life above 0.
- Danger threshold (HUD tint only): `0.3` (`metrics.ini:2565`).

### Bar start value

- `DRAIN_NORMAL` starts at the theme metric `LifeMeterBar.InitialValue` (`LifeMeterBar.cpp:23-27`).
  The arcade section does **not** define it (`metrics.ini:2564-2571`); the fallback theme defines
  `InitialValue=0.5` (`assets/d4/Themes/fallback/metrics.ini:2385`). The issue AC / PRD §7.2 instead
  require **"starts full"**. See **Open Questions OQ1** — B6 defaults to `1.0` per the AC.

### Deliberately not ported in v1

`LifeMeterBar::ChangeLife(float)` also contains a **hot downgrade** (`:118-119,174-175`),
**progressive lifebar** (`:217-220`), **life difficulty scale** (`:237-240`) and
**combo-to-regain-life** (`:208-227`). These are **not** in B2 and the issue AC specifies plain
"per the constants table" behavior, so B6 omits them and flags them in **Open Questions OQ2** with
full provenance. (The arcade defaults make progressive `0` and life-difficulty `1.0` inert anyway;
hot and regen-combo are the only behaviorally significant omissions.)

---

## Value Provenance

All semantics below are from the cloned OpenITG repo at commit
**`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`**. B6 introduces **no new numeric life constants**; the
nine deltas are consumed from B2. Only the bar start value and danger tint are presentation/project
choices (flagged).

| Value / Semantic | Value | Upstream source (file:line) |
|------------------|-------|-----------------------------|
| Life meter type | `LIFE_BAR` → `LifeMeterBar` | `ScreenGameplay.cpp:323-324`; `SongOptions.cpp:7` |
| Default drain / fail | `DRAIN_NORMAL` / `FAIL_IMMEDIATE` | `SongOptions.cpp:8,10` |
| Tap life once per **row** | `ChangeLife(scoreOfLastTap)` | `Player.cpp:1653` (from `OnRowCompletelyJudged`, `:1364,1433`) |
| Row score = B5 last-tap rule | miss dominates; else greatest offset | `NoteDataWithScoring.cpp:133-190` |
| Dance scores rows together | `m_bCountNotesSeparately=false` | `GameManager.cpp:105-108` |
| Hold outcome life | `ChangeLife(holdScore, tapScore)` | `Player.cpp:1700` |
| Hit-mine life immediate | `ChangeLife(TNS_HIT_MINE)` | `Player.cpp:1065` |
| Tap deltas | M/P +0.008, G +0.004, D 0.0, W −0.050, Miss −0.100, Mine −0.050 | `LifeMeterBar.cpp:108-114`; `metrics.ini:126-134` |
| Hold deltas | OK +0.008, NG −0.080 | `LifeMeterBar.cpp:169-170`; `metrics.ini:126-134` |
| Merciful drain | negative × `SCALE(life,0,1,0.5,1)`; arcade `0` | `LifeMeterBar.cpp:204-205`; `RageUtil.h:38`; `metrics.ini:156`; B2 `merciful_drain=false` |
| Clamp | `CLAMP(life, 0, 1)` | `LifeMeterBar.cpp:255-256` |
| Fail threshold | `FAIL_THRESHOLD=0`; `IsFailing = life<=0` | `LifeMeterBar.cpp:18,290-293` |
| Fail on empty (fail on) | `bFailed` + leave gameplay | `ScreenGameplay.cpp:1471-1493,1521-1531` |
| Fail-Off never fails, can recover | skip fail; `IsPlayerDead=false` | `ScreenGameplay.cpp:1476`; `GameState.cpp:1985-1996` |
| Freeze after fail | `if bFailed → fDeltaLife = 0` | `LifeMeterBar.cpp:229-231` |
| Danger threshold (HUD) | `0.3` | `metrics.ini:2565` |
| Omitted: hot downgrade | `score < TNS_GOOD` (or NG) at full → `−0.10` | `LifeMeterBar.cpp:118-119,174-175` |
| Omitted: progressive lifebar | `delta *= 1 + ProgressiveLifebar/8 * missCombo` | `LifeMeterBar.cpp:217-220`; `PrefsManager.cpp:221` |
| Omitted: life difficulty | gain × scale, loss ÷ scale; arcade `1.0` | `LifeMeterBar.cpp:237-240`; `PrefsManager.cpp:98`; `Other.lua:105` |
| Omitted: combo-to-regain | after a loss, next N wins give 0; defaults 5/10 | `LifeMeterBar.cpp:208-227`; `PrefsManager.cpp:120,122` |
| Bar start (`DRAIN_NORMAL`) | `InitialValue`; fallback `0.5` — **AC says full** | `LifeMeterBar.cpp:23-27`; `metrics.ini:2564-2571`; `fallback/metrics.ini:2385` |

**Unsourced presentation choices** (flagged, non-blocking): life-bar geometry, colors, and danger tint
are Tundra's own and carry no OpenITG parity requirement.

---

## Patterns to Follow

### Pure, time-free/value-type module (no clock reads)
```cpp
// SOURCE: src/gameplay/score_keeper.hpp:30-37
// "Pure, event-sourced scorer. ... includes no platform, audio, or time headers, so the scoring path
//  can never consult wall-clock or frame timing (AGENTS.md core principle 1)."
```

### Reusing B2 life constants (never re-define)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:52-63
struct LifeDeltas {
    double fantastic = 0.008; double excellent = 0.008; double great = 0.004; double decent = 0.0;
    double way_off = -0.050; double miss = -0.100; double hit_mine = -0.050;
    double hold_ok = 0.008; double hold_ng = -0.080; bool merciful_drain = false;
};
```

### Event handoff from B4
```cpp
// SOURCE: src/gameplay/judgment_engine.hpp:35-36
[[nodiscard]] const std::vector<JudgmentEvent>& events() const { return events_; }
void drain_new_events(std::vector<JudgmentEvent>& out); // appends events since last drain
```

### Row grouping built from chart + `note_index` (B5's proven approach)
```cpp
// SOURCE: src/gameplay/score_keeper.cpp:29-46
// Row identity is the exact `Note.beat` ... mines never define or join a scoring row.
```

### Solid-quad HUD primitive
```cpp
// SOURCE: src/gameplay/hud_renderer.cpp:75-78
renderer.draw_quad(Rect{x, y, w, h}, color);
```

### Tests
```cpp
// SOURCE: tests/score_keeper_test.cpp:19-25,31-38
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
td::Note make_note(int column, double time_seconds, td::NoteType type, double hold_end_time = 0.0);
```

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:99-100 / tests/CMakeLists.txt:125-133
add_library(tundra_core STATIC ... src/gameplay/score_keeper.cpp src/gameplay/hud_renderer.cpp ...)
add_executable(life_keeper_test life_keeper_test.cpp)
target_link_libraries(life_keeper_test PRIVATE tundra_core)
add_test(NAME life_keeper_test COMMAND life_keeper_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/life_keeper.hpp` | CREATE | `LifeState` + `LifeKeeper` API (pure event-derived life/fail) |
| `src/gameplay/life_keeper.cpp` | CREATE | Row grouping, delta mapping, clamp, fail/freeze, Fail-Off |
| `src/gameplay/hud_renderer.hpp` | UPDATE | Declare life-bar render method |
| `src/gameplay/hud_renderer.cpp` | UPDATE | Draw the life bar as solid quads (danger tint) |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Own `LifeKeeper`; add `fail_enabled` option + outcome accessors |
| `src/gameplay/gameplay_view.cpp` | UPDATE | Consume events into life; stop on fail; render bar; log |
| `src/main.cpp` | UPDATE | Add `--fail-off` demo flag |
| `CMakeLists.txt` | UPDATE | Add `src/gameplay/life_keeper.cpp` to `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `life_keeper_test` |
| `tests/life_keeper_test.cpp` | CREATE | Life deltas, row grouping, clamp, fail/Fail-Off, engine integration |

`src/gameplay/judgment.hpp`, `judgment_engine.*`, and `score_keeper.*` are **not modified**.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Life state + keeper interface

- **File**: `src/gameplay/life_keeper.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace td {

  // Live life derived solely from the B4 judgment log (PRD section 6 pattern 2).
  // Pure value type: no clocks, no platform headers.
  struct LifeState {
      double life = 1.0;   // clamped [0,1]; starts full (issue AC)
      bool failed = false; // fail-enabled and life reached 0
  };

  class LifeKeeper {
  public:
      // `chart` and `constants` must outlive the keeper. Builds the per-row tap
      // tables (row identity = exact Note.beat, as in ScoreKeeper) and resets life.
      void reset(const Chart* chart, const JudgmentConstants* constants);

      // FAIL_IMMEDIATE (true, default) / FAIL_OFF (false). Set before gameplay.
      void set_fail_enabled(bool enabled) { fail_enabled_ = enabled; }

      // Consume the append-only B4 log; idempotent per note.
      void consume(const JudgmentEvent& event);
      void consume(const std::vector<JudgmentEvent>& events);

      [[nodiscard]] const LifeState& state() const { return state_; }
      [[nodiscard]] double life() const { return state_.life; }
      [[nodiscard]] bool is_failing() const { return state_.life <= 0.0; }
      [[nodiscard]] bool has_failed() const { return state_.failed; }
      [[nodiscard]] bool fail_enabled() const { return fail_enabled_; }

  private:
      struct RowAggregate {
          int expected = 0;            // tap/hold-head notes in this row
          int judged = 0;
          bool has_miss = false;
          double last_delta_ms = 0.0;  // greatest offset wins; ties -> later column
          int last_column = -1;
          TapJudgment last_window = TapJudgment::Num;
      };

      void consume_tap_like(const JudgmentEvent& event, bool miss);
      void resolve_row(int row);
      [[nodiscard]] double delta_for_tap(TapJudgment window) const;
      [[nodiscard]] double delta_for(const JudgmentEvent& event) const;
      void apply(double delta);

      const Chart* chart_ = nullptr;
      const JudgmentConstants* constants_ = nullptr;
      bool fail_enabled_ = true;
      std::vector<int> note_row_;     // note_index -> row id (-1 = mine/unscored)
      std::vector<RowAggregate> rows_;
      std::vector<bool> note_scored_; // per-note idempotence guard for tap-like events
      LifeState state_;
  };

  } // namespace td
  ```
  - Includes `<vector>`, `<cstddef>`, `"chart/chart.hpp"`, `"gameplay/judgment.hpp"`,
    `"timing/judgment_constants.hpp"`. **No** SDL/GL/`<chrono>`.
- **Mirror**: `src/gameplay/score_keeper.hpp:30-80`.
- **Validate**: `cmake --build build -j16` (once included by Task 8).

### Task 2: Reset — row tables and full life

- **File**: `src/gameplay/life_keeper.cpp`
- **Action**: CREATE
- **Implement**:
  - `reset(chart, constants)`: store pointers; `state_ = LifeState{}` (life `1.0`, `failed=false`);
    clear vectors.
  - Null/empty chart → empty tables, life stays `1.0`; return.
  - Build rows exactly like `ScoreKeeper::reset` (`score_keeper.cpp:21-46`): skip `NoteType::Mine`;
    new row when `beat` differs from the previous non-mine note's beat; set `note_row_[i]` and
    `rows_[row].expected++`; init `last_delta_ms` to `std::numeric_limits<double>::lowest()`.
  - `note_scored_.assign(chart->notes.size(), false)`.
- **Mirror**: `src/gameplay/score_keeper.cpp:8-58`.
- **Validate**: `cmake --build build -j16` (once included by Task 8).

### Task 3: Consume — per-row taps, holds, mines

- **File**: `src/gameplay/life_keeper.cpp`
- **Action**: CREATE (same file as Task 2)
- **Implement**:
  - `delta_for_tap(w)`: switch over B2 `constants_->life`: `Fantastic→fantastic`,
    `Excellent→excellent`, `Great→great`, `Decent→decent`, `WayOff→way_off`; `Miss/HitMine/Num→0`.
  - `delta_for(event)`:
    - `Tap` → `delta_for_tap(event.window)` (applied only when its row resolves, see below).
    - `Miss` → `life.miss`; `HitMine` → `life.hit_mine`.
    - `HoldOk`/`RollOk` → `life.hold_ok`; `HoldNg`/`RollNg` → `life.hold_ng`.
    - `AvoidedMine`/`RollHit` → `0.0`.
  - `consume(event)` dispatch:
    - `Tap` → `consume_tap_like(event, false)`; `Miss` → `consume_tap_like(event, true)`.
    - `HoldOk/HoldNg/RollOk/RollNg` → `apply(delta_for(event))` immediately (hold outcome).
    - `HitMine` → `apply(delta_for(event))` immediately.
    - `AvoidedMine`/`RollHit` → nothing.
    - Early-return if `chart_ == nullptr || constants_ == nullptr`; return immediately if
      `state_.failed` (frozen — Task 4 `apply` also guards).
  - `consume_tap_like(event, miss)`: range-guard `event.note_index`; skip if `note_row_[i] < 0` (mine);
    duplicate-guard `note_scored_[i]`; mark scored; `aggregate.judged++`; if `miss || window==Miss` set
    `has_miss=true`; else track max `delta_ms` (ties → later column) and `last_window`. When
    `judged == expected` → `resolve_row(row)`.
  - `resolve_row(row)`: `score = has_miss ? Miss : last_window`; `apply(delta_for_tap(score))` once.
  - `consume(vector)`: loop `consume`.
- **Mirror**: `src/gameplay/score_keeper.cpp:60-156`; `Player.cpp:1653`.
- **Validate**: `cmake --build build -j16` with zero warnings.

### Task 4: Apply pipeline — clamp, freeze, fail / Fail-Off

- **File**: `src/gameplay/life_keeper.cpp`
- **Action**: CREATE (same file as Task 2)
- **Implement** `apply(double delta)` mirroring `LifeMeterBar::ChangeLife(float)`
  (`LifeMeterBar.cpp:202-261`), restricted to B2-provided behavior:
  1. If `state_.failed` → `delta = 0.0` (freeze; `:229-231`).
  2. If `constants_->life.merciful_drain && delta < 0` → `delta *= SCALE(life, 0, 1, 0.5, 1)`
     (`:204-205`; local helper, do not include RageUtil).
  3. `state_.life = std::clamp(state_.life + delta, 0.0, 1.0)` (`:255-256`).
  4. If `fail_enabled_ && !state_.failed && state_.life <= 0.0` → `state_.failed = true`
     (`FAIL_THRESHOLD=0`, `IsFailing=life<=0`; `LifeMeterBar.cpp:18,290-293` as enforced by
     `ScreenGameplay.cpp:1471-1493`). Fail-Off (`fail_enabled_==false`) never sets `failed`.
     No frame/wall-clock input anywhere in this path.
- **Mirror**: `src/gameplay/score_keeper.cpp:186-201` (pure derived recompute style).
- **Validate**: `cmake --build build -j16`.

### Task 5: HUD life bar

- **Files**: `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: forward-declare `class GlQuadRenderer;` (already) and add:
    ```cpp
    // ITG-style horizontal life bar. 0..1; danger tint below the arcade threshold
    // (0.3, metrics.ini:2565). Solid quads only: no font, no stb_truetype, no asset.
    void render_life(double life, int screen_w, int screen_h, GlQuadRenderer& renderer) const;
    ```
  - cpp: early-return if `screen_w<=0 || screen_h<=0`; clamp `life` to `[0,1]`; draw a background
    frame quad and a filled portion quad (e.g. bottom-centre, fixed height) with a
    `kLifeFillColor`; use a `kLifeDangerColor` when `life < 0.3`. File-local constexpr colors
    (Tundra presentation, consistent with the existing palette at `hud_renderer.cpp:12-21`).
  - Do **not** add glyphs for a "LIFE" label (the 5×7 font lacks the letters; bar-only avoids a font
    asset). Existing `render(ScoreState,...)` stays unchanged.
- **Mirror**: `src/gameplay/hud_renderer.cpp:158-165` (quad draw), `:133-140` (guards).
- **Validate**: `cmake --build build -j16`.

### Task 6: GameplayView integration + fail transition

- **Files**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: `#include "gameplay/life_keeper.hpp"`; add to `GameplayOptions`:
    ```cpp
    bool fail_enabled = true; // false = Fail-Off (song continues to the end)
    ```
    Add an outcome enum + members/accessors:
    ```cpp
    enum class GameplayOutcome { InProgress, Cleared, Failed };
    [[nodiscard]] const LifeState& life_state() const { return life_.state(); }
    [[nodiscard]] bool has_failed() const { return life_.has_failed(); }
    [[nodiscard]] bool is_cleared() const { return score_.is_complete(); }
    [[nodiscard]] GameplayOutcome outcome() const;
    ```
    Add members `LifeKeeper life_; bool exited_ = false;` and reset `exited_=false` in `init`.
  - `init`: after `score_.reset(&chart_, &constants);` add
    `life_.set_fail_enabled(options.fail_enabled); life_.reset(&chart_, &constants);`. Log fail mode.
  - `update`: after `score_.consume(new_events_);` add `life_.consume(new_events_);`. If
    `life_.has_failed() && !exited_` → `exited_ = true; audio_.stop();` and log
    `[GameplayView] Failed: life empty at <music_time>s`. Once `exited_`, skip `judge_.update` and
    event draining (gameplay has ended). `fixed_dt` still only advances the demo stub clock.
  - `render`: after `hud_.render(score_.state(), ...)` call
    `hud_.render_life(life_.life(), screen_w, screen_h, renderer);`.
  - `shutdown`: extend the session line with `life <x.xxx>` and fail state.
  - `outcome()`: `Failed` if `has_failed()`; else `Cleared` if `is_cleared()` and music has passed the
    chart end (or simply `is_cleared()`); else `InProgress`. (Document the exact choice; C1/C7 consume
    this later.)
- **Mirror**: `src/gameplay/gameplay_view.cpp:36-37,127-155,184-195,196-221`.
- **Validate**: `cmake --build build -j16`.

### Task 7: Demo `--fail-off` flag

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**: parse `--fail-off` → `options.fail_enabled = false;`; add a `--fail-off` line to
  `print_help()`. This is the temporary-harness way to exercise Fail-Off end-to-end before C1's
  options screen.
- **Mirror**: `src/main.cpp:43-72,105-113`.
- **Validate**: `cmake --build build -j16`.

### Task 8: Register source and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root: add `src/gameplay/life_keeper.cpp` to `tundra_core` after `src/gameplay/hud_renderer.cpp`
    (line ~100).
  - Tests: append a `life_keeper_test` block mirroring `score_keeper_test`
    (`tests/CMakeLists.txt:125-133`).
- **Mirror**: `CMakeLists.txt:96-104`, `tests/CMakeLists.txt:125-133`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 9: Test suite

- **File**: `tests/life_keeper_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; `make_note`/`make_tap`/`make_miss`/`make_hold_outcome` harness adapted
  from `tests/score_keeper_test.cpp:31-90`; `JudgmentConstants::compiled_defaults()`; deterministic;
  events fed both synthetically and via `JudgmentEngine` + `drain_new_events`):
  1. **Starts full**: fresh `reset` → `life()==1.0`, `!is_failing()`, `!has_failed()`.
  2. **Table deltas** (fresh chart per case; assert exact B2 values): Fantastic `+0.008`,
     Excellent `+0.008`, Great `+0.004`, Decent `0.0`; below-full cases pin WayOff `−0.050`,
     Miss `−0.100`, HitMine `−0.050`, HoldOk `+0.008`, HoldNg `−0.080` (dip life below 1 first so no
     clamp masks the value).
  3. **Row grouping — jump counts once**: 2-note jump both Great → life changes by exactly one
     `+0.004` (a Great on each column), not `+0.008`; jump Great+Miss → miss dominates → one `−0.100`.
  4. **Clamp**: repeated Fantastics keep `life()==1.0`; repeated Misses bottom out at `0.0`.
  5. **Fail enabled**: enough Misses to reach 0 → `has_failed()`, `is_failing()`; a following Great
     does **not** raise life (frozen).
  6. **Fail-Off**: `set_fail_enabled(false)`; Misses to 0 → `life()==0.0` but `!has_failed()`; a
     following Great raises life above 0 (recovery).
  7. **Merciful drain**: with `constants.life.merciful_drain=true`, a negative delta at life≈0.5 is
     scaled by `0.75`; with `false` (B2 default) it is not.
  8. **Holds/mines are individual**: hold head Great (row) `+0.004` then HoldOk `+0.008` both apply;
     `AvoidedMine`/`RollHit` change nothing.
  9. **Idempotence**: consuming the same event twice does not double-apply (per-note guard).
  10. **Incremental == batch**: one-by-one `consume` equals `consume(vector)`.
  11. **Engine integration / engine-produced log**: build a chart, run `JudgmentEngine` through a
      scripted hit/miss/hold sequence, `drain_new_events`, consume, and assert life/fail against
      hand-computed OpenITG arithmetic (document the arithmetic in a comment).
  12. **Fail-Off end-to-end through the engine**: same sequence with `fail_enabled=false`; assert no
      failure and monotonic recovery; contrast with `fail_enabled=true` failing at the same point.
- **Mirror**: `tests/score_keeper_test.cpp:1-90`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 14/14).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 14/14: 13 existing + life_keeper_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/life_keeper_test

# Headless harness smoke (stub clock; fixture has taps/holds/rolls/mines)
./build/tundra-dance --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Tundra Pack/Tundra Anthem/Tundra Anthem.sm" --smoke-test 120

# Fail-Off harness smoke (same run; must not fail/leave early)
./build/tundra-dance --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Tundra Pack/Tundra Anthem/Tundra Anthem.sm" \
  --smoke-test 120 --fail-off

# Purity check: life core must not touch platform/time/GL headers
rg -n "SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter|GetTicksNS|fixed_dt" \
  src/gameplay/life_keeper.hpp src/gameplay/life_keeper.cpp
# (no matches)
```

## End-to-End Verification

1. `./build/tests/life_keeper_test` prints each sub-check; tests 2-3 prove the B2 delta table and
   per-row aggregation; tests 5-6 prove Fail-Enabled vs Fail-Off; test 11-12 prove life/fail derive
   from an engine-produced B4 log.
2. `ctest --test-dir build --output-on-failure` → **14/14**; all 13 prior tests stay green (changes are
   additive plus a `GameplayView` internal member and a HUD method).
3. `--headless --gameplay-demo ... --smoke-test 120` exits cleanly; the `[GameplayView]` shutdown line
   reports `life <x.xxx>` and fail state; no GL calls are attempted headless (life-bar draw is a
   no-op).
4. The `--fail-off` variant runs identically and never sets the failed state (Fail-Off continues to
   song end).
5. On a display: run `--gameplay-demo` on the fixture and confirm the life bar refills on Great+ and
   drains on Decent/WayOff/Miss/mines, matching the console session summary; when life hits 0 with the
   default fail mode the run reports `Failed` and gameplay stops.
6. Enforce module purity with the `rg` command in **Validation** → the life core contains no
   SDL/GL/`<chrono>` and no frame-delta math; `fixed_dt` reaches only the demo stub clock.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| OpenITG changes life **once per row** but B4 logs per note; naive per-note application double-counts jumps | `LifeKeeper` groups tap events by exact `Note.beat` (same rule as B5) and applies one delta per completed row; tests 3, 9 | **In scope** |
| Row grouping logic duplicated from `ScoreKeeper` | B5 is frozen and exposes no resolved-row stream; keep B6 self-contained and document the shared rule; a future refactor could extract a shared helper | **In scope** — flagged |
| Issue AC says "starts full" but OpenITG `LifeMeterBar.InitialValue` resolves to `0.5` via fallback | Default `life = 1.0` per AC/PRD; OQ1 asks for ratification | **In scope** — flagged |
| AC says "refills on Great-or-better" but OpenITG has hot-downgrade + combo-to-regain that modify this | v1 omits non-B2 knobs and flags them (OQ2) with provenance; if ratified, add as a follow-up or set the relevant defaults | **In scope** — flagged |
| Life could be applied after the fail freeze, refilling a dead player | `apply()` zeroes deltas once `failed` (`LifeMeterBar.cpp:229-231`); test 5 | **In scope** |
| Fail detected in a frame loop in OpenITG, but Tundra is event-sourced | Detect at `apply()` time (`life<=0` with fail enabled), which is equivalent for FAIL_IMMEDIATE; documented in Pinned Semantics | **In scope** |
| "Transition out of gameplay" needs a screen manager that does not exist yet | `GameplayView` exposes `outcome()`/`has_failed()` and stops audio; C1/C7 consume it later | **In scope** — boundary |
| HUD life bar needs a font/glyphs | Draw with solid quads only (no label text); no stb_truetype/font asset | **In scope** |
| Headless GL crash from life-bar draw | `GlQuadRenderer` is a documented no-op when uninitialized; E2E step 3 | **In scope** |
| `merciful_drain` in B2 but arcade-false; easy to misread as active | Read B2 flag; test 7 exercises both values | **In scope** |
| Values not in B2 (hot/regen/progressive/difficulty) | Omitted and flagged in OQ2 with `file:line`; no invented numbers | **Out of scope** — flagged |
| Results screen / persistence / sync chart | Explicitly out of B6 (C2/C7/Phase B chart ticket) | **Out of scope** |

---

## Decisions

- **Life consumes only B2 values** (the nine deltas + `merciful_drain`). No value is re-defined; no
  new life delta constants are introduced.
- **Tap life is per completed row**, using B5's pinned "miss dominates, else greatest offset (ties →
  later column)" row score. Mines and hold/roll outcomes apply individually.
- **Start life = `1.0` (full)** per issue AC1 / PRD §7.2, despite OpenITG's fallback
  `InitialValue=0.5` (OQ1).
- **Fail = FAIL_IMMEDIATE semantics**: with fail enabled, life reaching 0 sets `failed`, freezes life,
  and ends gameplay; with Fail-Off (`fail_enabled=false`) the song continues and life can recover.
- **Non-B2 OpenITG life refinements are omitted in v1** (hot downgrade, progressive lifebar, life
  difficulty, combo-to-regain) and flagged in OQ2; progressive/life-difficulty are inert at arcade
  defaults anyway.
- **HUD life bar is quads-only** (no font glyphs, no stb_truetype, no asset), consistent with B5.
- **No B4/B5 changes**: row aggregation uses `note_index` + the chart; `JudgmentEvent` and
  `ScoreKeeper` stay frozen.
- **The Fail-Off option is a `GameplayOptions` flag** (`fail_enabled`), wired to a temporary
  `--fail-off` demo flag until C1's options screen exists.

---

## Open Questions

1. **BLOCKING — bar start value: "starts full" (AC) vs OpenITG `InitialValue=0.5`.** The issue AC1 and
   PRD §7.2 require a full bar at start; OpenITG `DRAIN_NORMAL` starts at the `LifeMeterBar`
   `InitialValue` theme metric, which the arcade theme does not define and the fallback theme sets to
   `0.5` (`fallback/metrics.ini:2385`), i.e. half full. **Proposed default: start at `1.0` (full)**,
   treating the project docs as authoritative and the OpenITG fallback as a theme artifact. Confirm
   before life values are locked by tests.
2. **BLOCKING — AC "refills on Great-or-better" vs OpenITG hot-downgrade + combo-to-regain.** The AC
   specifies drain/refill straight from the constants table, but OpenITG's `ChangeLife(float)` also
   (a) forces a `−0.10` penalty for WayOff/Miss/mine and hold-NG while life is full (hot,
   `LifeMeterBar.cpp:118-119,174-175`) and (b) suppresses positive gains for the next
   `RegenComboAfterMiss=5` judgments after a loss (`:208-227`). Both are sourced but not in B2.
   **Proposed default for v1: omit both** (AC-literal, lean). If arcade parity is required, ratify and
   add them as a follow-up (or a small extension to B2's life table). Confirm.
3. **Non-blocking — Fail-Off recovery display.** With Fail-Off, life can sit at 0 and later recover;
   confirm the HUD should keep drawing the bar (proposed: yes, at 0 with the danger tint) rather than
   hiding it once empty.
4. **Non-blocking — `outcome()` "cleared" definition.** Proposed: `Cleared` when `score_.is_complete()`
   (all rows/holds resolved), `Failed` when `has_failed()`, else `InProgress`. Confirm C7 wants
   `is_complete()` as the results trigger rather than a music-past-chart-end check.
5. **Non-blocking — life-bar placement/style.** Proposed: a horizontal bar along the bottom-centre with
   a `0.3` danger tint; exact geometry/colors are Tundra presentation (unsourced).

---

## Acceptance Criteria

- [ ] Given a song start, life begins full (`1.0`) (Task 1/2; test 1)
- [ ] Given Decent/Way Off/Miss/mine, life drains and given Great-or-better it refills, using only B2
      deltas (Tasks 3-4; tests 2, 8)
- [ ] Given an empty bar with fail enabled, the game fails and gameplay ends (Task 4/6; tests 5, 12)
- [ ] Given Fail-Off, the song continues to the end at zero life (Tasks 4, 6, 7; tests 6, 12)
- [ ] Life changes derive **only** from the B4 event log — no independent judgment, no frame/wall-clock
      logic (purity `rg` check; tests 9, 11)
- [ ] Tap life is applied once per OpenITG row (jumps do not double-count) (Task 3; test 3)
- [ ] A life bar renders via the existing quad/bitmap HUD path with no new font asset (Task 5)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 14/14 pass (13 prior + `life_keeper_test`)
- [ ] Follows existing module/naming/test/CMake patterns; no results-screen/persistence logic added
