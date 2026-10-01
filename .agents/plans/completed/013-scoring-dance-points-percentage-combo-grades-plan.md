# Plan: Scoring — Dance Points, Percentage, Combo, and ★ Grades

## Summary

Build Blaze 4k's scoring layer as a **pure, event-sourced module** (PRD §6 pattern 2 / AGENTS.md core
principles 1 & 2). Every value is derived from the B4 `JudgmentEvent` log — never from an independent
re-judgment — and every OpenITG rule is pinned to the reference commit (see **Value Provenance**).
The work splits into a pure scoring core, a thin HUD, and integration:

1. `src/gameplay/score_keeper.{hpp,cpp}` (NEW) — `ScoreState` + `ScoreKeeper`: consumes the B4 event
   log, **groups per-note events into OpenITG rows**, and derives dance points, possible dance points,
   DP%, combo/max-combo, per-window judgment counts, and the ★ grade. Pure value types; no clocks,
   no platform headers.
2. `src/gameplay/hud_renderer.{hpp,cpp}` (NEW) — a minimal live HUD (score %, combo, judgment counts)
   drawn with the existing `GlQuadRenderer` using a self-contained 5×7 bitmap font; pure formatting
   helpers are unit-testable, drawing is a safe no-op when the renderer is uninitialized (headless).
3. `src/gameplay/gameplay_view.{hpp,cpp}` (UPDATE) — own the keeper, drain new events into it each
   `update`, render the HUD each frame, and expose the live `ScoreState` for B6/C7.
4. `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/score_keeper_test.cpp` (UPDATE/CREATE).

**Scope boundary:** B5 consumes the B4 log and produces score/percent/combo/counts/grade plus a live
HUD. It does **not** touch life, drain/refill, or fail handling (B6), does not draw judgment/combo pop
sprites or noteskins (D2), and does not build the Results screen or persistence (C7/C2). The B4 engine
and `JudgmentEvent` are frozen — row aggregation uses `note_index` + the chart, so no B4 change is
needed.

**Critical cross-issue requirement (from B4):** OpenITG emits one miss/score per **row**
(`ComboIsPerRow=false`, `metrics.ini:18-20`; `Player.cpp:1305-1365,1521-1661`) while B4 logs events
**per note**. B5 groups same-row events (row identity = exact `Note.beat`, see **Pinned Semantics**)
to reproduce OpenITG row semantics.

All tap/hold/mine scoring and combo semantics are pinned to **OpenITG** commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (see **Pinned Semantics** and **Value Provenance**).

## User Story

As a competitive player
I want OpenITG-equivalent dance points, percentage, combo, and ★ grades derived from the judgment log
So that my scores mean the same thing they do on real hardware.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/gameplay/` (NEW `score_keeper*`, `hud_renderer*`), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #13 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3 3.2.8, glad (GL 3.3 core), miniaudio 0.11.21, nlohmann_json 3.11.3, stb already fetched |
| Baseline tests | 12/12 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 12" (0.29s) |
| `src/gameplay/` | exists | B3/B4 (`note_field`, `noteskin`, `note_field_renderer`, `gameplay_view`, `judgment*`) present; add new `score_keeper*`/`hud_renderer*` sources |
| `JudgmentConstants` | `src/timing/judgment_constants.*` | Reuse **as-is**: `dp_weights`, `grade_tiers`, `continues_combo`, `grade_for_percent`; never re-define weights/tiers |
| B4 event log | `src/gameplay/judgment.hpp`, `judgment_engine.*` | `JudgmentEvent {kind, column, note_time_seconds, hit_time_seconds, delta_ms, window, hold, note_type, note_index}`; `drain_new_events(out)` is the incremental handoff |
| Chart row identity | `src/chart/note.hpp:25-36`, `note_parser.cpp:150-158,244-248` | One `beat` computed per row and shared by all columns; `chart.notes` sorted by beat then column |
| Chart counts | `src/chart/chart.hpp:19-22` | `hold_count`/`roll_count` are per-head counts (parser `note_parser.cpp:181,191`); `tap_count` is per-note |
| HUD text | **none** | No font/text module and no font asset exist; `GlQuadRenderer::draw_quad` (solid tinted quad) is the only primitive (`src/render/gl_quad_renderer.hpp:30`) |
| Renderer headless guard | `src/render/gl_quad_renderer.hpp:11-13,27-33` | Uninitialized renderer is a safe no-op — HUD draws are headless-safe |
| Upstream source | `/tmp/opencode/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Cloned for provenance; arcade runtime overrides in `assets/patch-data/Themes/default/metrics.ini` |

**Start green, stay green:** 12 tests pass; this plan adds 1 test target (`score_keeper_test`) → 13 expected.

---

## Pinned Semantics

**Authority for B5 is OpenITG** (PRD §15). OpenITG stores notes by integer row and works in beats;
Blaze 4k stores an absolute `Note.beat` per note. B5 re-expresses OpenITG's row scoring against Blaze 4k's
note/chart domains (documented where the re-expression is not literally identical).

### Row identity and aggregation

- OpenITG scores once per **note row** (`ComboIsPerRow=false`, `metrics.ini:18-20`). A row is scored
  when it is **completely judged** — every non-mine tap/hold-head note in it has a result
  (`IsRowCompletelyJudged = MinTapNoteScore >= TNS_MISS`, `NoteDataWithScoring.cpp:172-190`) — via
  `Player::OnRowCompletelyJudged → HandleTapRowScore` (`Player.cpp:1305-1365,1521-1575`).
- Blaze 4k's row identity is the exact **`Note.beat`**: the parser computes `beat` once per row and
  shares the bit-identical value across that row's columns (`note_parser.cpp:152-158`). B5 maps each
  event's `note_index → chart.notes[note_index].beat` and groups equal beats. This is **stronger than
  grouping by `note_time_seconds`**: a stop/warp can make two distinct beats map to the same seconds,
  and those are distinct OpenITG rows. Mines never define or join a scoring row
  (`IsThereATapOrHoldHeadAtRow`, `NoteData.h:111-113`; mines are skipped by
  `LastTapNoteResult`, `NoteDataWithScoring.cpp:133-159`).
- The number of notes in a row is `GetNumTracksWithTapOrHoldHead(row)` (`NoteData.cpp:260-269`): every
  `Tap`/`HoldHead`/`RollHead` note (roll heads count as hold heads); mines excluded. This count is used
  for the combo increment.

### Row score = "last tap", miss dominates

- `HandleTapRowScore` receives `scoreOfLastTap = LastTapNoteResult(row).tns`
  (`Player.cpp:1523`; `NoteDataWithScoring.cpp:161-169`). `LastTapNoteScoreTrack`
  (`NoteDataWithScoring.cpp:133-159`) returns the **first** track whose result is `TNS_MISS`/`TNS_NONE`
  (a miss anywhere makes the whole row a miss); otherwise the track with the **greatest
  `fTapNoteOffset`** — i.e. the **latest** hit (`fTapNoteOffset = hit − note`, negative = early,
  `NoteTypes.h:14-16`) — with ties going to the **later column**.
- **Blaze 4k:** for a row's buffered per-note events, if any is a `Miss` → row score = Miss; else the
  event with the greatest `delta_ms` (ties → later column) supplies the row score. `delta_ms` carries
  the same sign convention as `fTapNoteOffset` (`judgment.hpp:31`).
- **TNS_NONE cannot appear** in a completed row: B4 emits no event for a step-too-far; the note stays
  unjudged until it expires as a Miss (`judgment_engine.cpp:99-101,197-244`).

### Dance points (numerator) — `ScoreKeeperMAX2.cpp`

- Tap row: `actual_dp += weight(row_score)`; `tap_counts[row_score] += 1` — **once per row**
  (`ScoreKeeperMAX2.cpp:347-352`).
- Hold/roll outcome: `actual_dp += weight(hns)` (OK=+5, NG=0); `hold_counts[hns] += 1`
  (`ScoreKeeperMAX2.cpp:414-420`).
- Hit mine: `actual_dp += weight(HitMine)` (−6); `tap_counts[HitMine] += 1` (`ScoreKeeperMAX2.cpp:333-341`).
- Avoided mine: nothing (`UpdateTapNotesMissedOlderThan` only calls `HandleTapRowScore` when a non-mine
  note was missed, `Player.cpp:1399-1435`). Roll re-hit (`RollHit`): nothing.
- Weights come from B2 `dp_weights` (`metrics.ini:116-124`; `ScoreKeeperMAX2.cpp:509-547`).

### Possible dance points (denominator)

- `GetPossibleDancePoints = NumTaps*5 + NumHolds*5 + NumRolls*5`, where `NumTaps =
  RADAR_NUM_TAPS_AND_HOLDS = GetNumRowsWithTapOrHoldHead()` (`ScoreKeeperMAX2.cpp:440-452`;
  `NoteDataUtil.cpp:641-646`; `NoteData.cpp:477-485`), and holds/rolls are per-head counts.
- **Blaze 4k:** `possible_dp = rows_with_tap_or_hold_head * dp_weights.fantastic
  + chart.hold_count * dp_weights.hold_ok + chart.roll_count * dp_weights.hold_ok`. This is a
  **chart-derived maximum**, not judgment logic; B5 computes it once in `reset()`. (No note-adding
  mods in v1, so OpenITG's `max(pre, post)` radar collapse is a no-op.)

### Percentage — `PlayerStageStats.cpp:218-232`

- `possible == 0 → 0`; `actual == possible → 1.0` (rounding correction); else `actual / possible`.
- Percent is **not clamped** for grade purposes (Way Off/Miss/HitMine weights make it negative).

### Combo — `Player.cpp:1544-1558` + `ScoreKeeperMAX2.cpp:357-368`, `metrics.ini:18-20`

- `ComboIsPerRow=false`; `MinScoreToContinueCombo = MinScoreToMaintainCombo = TNS_GREAT`.
- Row score Fantastic/Excellent/Great → `combo += notes_in_row`; `miss_combo = 0`.
- Row score Decent or Way Off → `combo = 0`.
- Row score Miss → `combo = 0`; `miss_combo += 1`.
- Hit mine → **no combo change** (`HandleTapScore` only, `ScoreKeeperMAX2.cpp:333-341`).
- Hold/roll OK or NG → **no combo change** (`HandleHoldScore` never touches combo,
  `ScoreKeeperMAX2.cpp:414-437`).
- `max_combo = max(max_combo, combo)` (`Player.cpp:1616-1618`).
- The scorekeeper's extra `iCurMissCombo == 0` guard is already satisfied because
  `Player::HandleTapRowScore` resets `iCurMissCombo = 0` on Great+ **before** the scorekeeper call
  (`Player.cpp:1546-1553`); the net behavior is exactly the bullets above.
- **Reuse `JudgmentConstants::continues_combo(score)`** for the Great-or-better test — do **not** write
  `score >= TapJudgment::Great`, because B2's enum order (`Fantastic, Excellent, Great, Decent, WayOff,
  Miss, HitMine, Num`) is not monotonic by quality (`judgment_constants.hpp:21`).

### Grade — `PlayerStageStats.cpp:141-216`

- `grade = constants.grade_for_percent(percent)` (B2 tiers, `metrics.ini:4431-4449`). Arcade sets
  `GradeTier02IsAllPerfects=0` (`metrics.ini:4432`), so the all-perfects special case is disabled.
- Because the arcade `GradeWeight*` and `PercentScoreWeight*` tables are identical
  (`metrics.ini:104-124`) and the possible-point formulas match, **grade% == DP%**; B2's single
  `grade_for_percent` is authoritative. `bFailedEarlier → GRADE_FAILED` is B6.

### Events with no scoring effect

- `AvoidedMine`, `RollHit` — ignored by scoring (stats/visual only).

### HUD display formatting — `PercentageDisplay.cpp:135-146`, `metrics.ini:3116-3117,3137`

- Percent is formatted with `PercentDecimalPlaces=2`, `PercentTotalSize=5`, `PercentUseRemainder=0`,
  **truncated** (not rounded) after a `+0.000001` boost, and **display-clamped to [0,1]** when
  `actual <= possible` (`PercentageDisplay.cpp:110-116`). Blaze 4k: `format_percent` truncates to two
  decimals and appends `%`; grade still uses the unclamped percent.

---

## Value Provenance

All behavioral values/semantics below are transcribed from the cloned OpenITG repository at commit
**`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`** ("ok it's really over now…", 2020-12-17). B5 introduces
**no new numeric scoring constants**; weights/tiers are consumed from B2.

| Value / Semantic | Value | Upstream source (file:line) |
|------------------|-------|-----------------------------|
| Score/miss once per row | `ComboIsPerRow=false` | `metrics.ini:18` |
| Combo continue/maintain threshold | `MinScoreToContinueCombo=MinScoreToMaintainCombo=TNS_GREAT` | `metrics.ini:19-20` |
| Row completion test | `MinTapNoteScore >= TNS_MISS` | `NoteDataWithScoring.cpp:172-190` |
| Row score = last tap, miss dominates | `LastTapNoteResult`/`LastTapNoteScoreTrack` | `NoteDataWithScoring.cpp:133-169` |
| Row scored on completion | `OnRowCompletelyJudged → HandleTapRowScore` | `Player.cpp:1305-1365,1521-1575` |
| Tap-row DP + count | `+= TapNoteScoreToDancePoints(scoreOfLastTap)`; count per row | `ScoreKeeperMAX2.cpp:343-352` |
| Hold DP + count | `+= HoldNoteScoreToDancePoints(hns)`; OK/NG count | `ScoreKeeperMAX2.cpp:414-420` |
| Hit-mine DP + count, no combo | `HandleTapScore(TNS_HIT_MINE)` | `ScoreKeeperMAX2.cpp:333-341` |
| Combo increment | `iCurCombo += iNumTapsInRow` (Great+) | `ScoreKeeperMAX2.cpp:357-368` |
| Combo reset | `< MinScoreToMaintainCombo → 0`; miss sets `iCurMissCombo` | `Player.cpp:1544-1558` |
| Notes in row | `GetNumTracksWithTapOrHoldHead(row)` | `NoteData.cpp:260-269` |
| Possible DP formula | `NumTaps*5 + NumHolds*5 + NumRolls*5` | `ScoreKeeperMAX2.cpp:440-452` |
| Radar tap/hold rows | `RADAR_NUM_TAPS_AND_HOLDS = GetNumRowsWithTapOrHoldHead()` | `NoteDataUtil.cpp:641`; `NoteData.cpp:477-485` |
| Radar holds/rolls | `GetNumHoldNotes()` / `GetNumRolls()` | `NoteDataUtil.cpp:643,646` |
| DP weights | Fantastic 5, Excellent 4, Great 2, Decent 0, WayOff −6, Miss −12, HitMine −6, OK 5, NG 0 | `metrics.ini:116-124`; `ScoreKeeperMAX2.cpp:509-547` |
| Grade weights (== DP weights) | same 9 values | `metrics.ini:104-114`; `ScoreKeeperMAX2.cpp:549-591` |
| DP% formula | `actual/possible`, 0 if possible==0, 1 if equal | `PlayerStageStats.cpp:218-232` |
| Grade lookup | `GetGradeFromPercent` (first tier ≥ threshold) | `PlayerStageStats.cpp:141-154` |
| Grade tier special case off | `GradeTier02IsAllPerfects=0` | `metrics.ini:4432` |
| Grade tiers (17) | 1.00 … 0.55, D | `metrics.ini:4433-4449` (B2 table) |
| Percent display format | 2 decimals, size 5, truncate, display-clamp [0,1] | `PercentageDisplay.cpp:110-146`; `metrics.ini:3116-3117,3137` |
| Delta sign | `fTapNoteOffset = hit − note` (negative = early) | `NoteTypes.h:14-16`; `judgment.hpp:31` |

**Unsourced presentation choices** (flagged, non-blocking — see Open Questions): the HUD bitmap-font
glyph shapes, layout offsets, and per-judgment chip colors are Blaze 4k's own presentation and carry no
OpenITG parity requirement.

---

## Patterns to Follow

### Pure, time-free module (no clock reads)
```cpp
// SOURCE: src/gameplay/judgment_engine.hpp:12-19
// "This module is time-parameterized: callers pass an absolute music time ... It includes
//  no SDL/GL/audio/chrono headers, so the judgment path can never consult wall-clock..."
```
`ScoreKeeper` consumes already-timestamped `JudgmentEvent`s and includes no platform/time headers.

### Reusing B2 constants (never re-define weights/tiers)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:82-84
[[nodiscard]] TapJudgment classify_tap(double delta_seconds) const;
[[nodiscard]] bool continues_combo(TapJudgment j) const;
[[nodiscard]] const GradeTier& grade_for_percent(double percent) const;
```

### Event handoff from B4
```cpp
// SOURCE: src/gameplay/judgment_engine.hpp:35-36
[[nodiscard]] const std::vector<JudgmentEvent>& events() const { return events_; }
void drain_new_events(std::vector<JudgmentEvent>& out); // appends events since last drain
```

### Chart model (row grouping + denominator)
```cpp
// SOURCE: src/chart/note.hpp:25-36
struct Note { int column; double beat; double time_seconds; NoteType type;
              double hold_length_beats; double hold_end_time_seconds; };
// SOURCE: src/chart/chart.hpp:16-26
std::vector<Note> notes; int tap_count; int hold_count; int roll_count; int mine_count;
```

### Solid-quad drawing primitive for the HUD
```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:30
void draw_quad(const Rect& rect, Color color); // solid white quad tinted by `color`
```

### Logging / error handling
```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:31
std::cerr << "[GameplayView] Chart has no notes; nothing to play\n";
```
Tagged `std::cerr` lines prefixed `[ScoreKeeper]` / `[GameplayView]`; never throw.

### Tests
```cpp
// SOURCE: tests/judgment_engine_test.cpp:13-19,27-36
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
blaze4k::Note make_note(int column, double time_seconds, blaze4k::NoteType type, double hold_end_time = 0.0);
```
Plain `int main()` binaries, `TEST_CHECK`, synthetic `Chart`, `JudgmentConstants::compiled_defaults()`,
deterministic and hardware-independent.

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:96-104 / tests/CMakeLists.txt:114-122
add_library(blaze4k_core STATIC ... src/gameplay/judgment_engine.cpp ...)
add_executable(judgment_engine_test judgment_engine_test.cpp)
target_link_libraries(judgment_engine_test PRIVATE blaze4k_core)
add_test(NAME judgment_engine_test COMMAND judgment_engine_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/score_keeper.hpp` | CREATE | `ScoreState` + `ScoreKeeper` API (pure event-derived scoring) |
| `src/gameplay/score_keeper.cpp` | CREATE | Row aggregation, DP/percent/combo/counts/grade |
| `src/gameplay/hud_renderer.hpp` | CREATE | `HudRenderer` + pure `format_percent`/`format_combo` helpers |
| `src/gameplay/hud_renderer.cpp` | CREATE | 5×7 bitmap font + HUD layout via `GlQuadRenderer` |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Own `ScoreKeeper` + `HudRenderer`; expose live `ScoreState` |
| `src/gameplay/gameplay_view.cpp` | UPDATE | Drain new events → keeper in `update`; render HUD; log final score |
| `CMakeLists.txt` | UPDATE | Add `src/gameplay/score_keeper.cpp`, `src/gameplay/hud_renderer.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `score_keeper_test` |
| `tests/score_keeper_test.cpp` | CREATE | DP/percent/grade/combo/counts/row/HUD-format tests |

`src/gameplay/judgment.hpp` and `judgment_engine.*` are **not modified** (B4 frozen).

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Score state + keeper interface

- **File**: `src/gameplay/score_keeper.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {

  // Live score derived solely from the B4 judgment log (PRD section 6 pattern 2).
  // Pure value type; no clocks, no platform headers.
  struct ScoreState {
      int actual_dp = 0;
      int possible_dp = 0;
      int combo = 0;
      int max_combo = 0;
      int miss_combo = 0;                     // OpenITG iCurMissCombo
      std::array<int, static_cast<std::size_t>(TapJudgment::Num)> tap_counts{};
      std::array<int, static_cast<std::size_t>(HoldJudgment::Num)> hold_counts{};
      double percent = 0.0;                   // unclamped; may be negative
      const GradeTier* grade = nullptr;       // points into constants.grade_tiers
  };

  class ScoreKeeper {
  public:
      // `chart` and `constants` must outlive the keeper. Builds the chart-derived
      // row tables and possible-DP denominator; clears all score state.
      void reset(const Chart* chart, const JudgmentConstants* constants);

      // Consume one immutable judgment event (append-only log; idempotent per note).
      void consume(const JudgmentEvent& event);
      void consume(const std::vector<JudgmentEvent>& events);

      [[nodiscard]] const ScoreState& state() const { return state_; }
      [[nodiscard]] int actual_dance_points() const { return state_.actual_dp; }
      [[nodiscard]] int possible_dance_points() const { return state_.possible_dp; }
      [[nodiscard]] double percent() const { return state_.percent; }
      [[nodiscard]] const GradeTier& grade() const;   // constants.grade_for_percent(percent)
      [[nodiscard]] bool is_complete() const;         // every row resolved and every hold scored

  private:
      struct RowAggregate {
          int expected = 0;            // tap/hold-head notes in this row
          int judged = 0;
          bool has_miss = false;
          double last_delta_ms = 0.0;  // greatest offset wins; ties -> later column
          int last_column = -1;
          TapJudgment last_window = TapJudgment::Num;
      };

      void apply_tap_like(const JudgmentEvent& event, bool miss);
      void resolve_row(int row);
      void apply_hold(HoldJudgment hold);
      void recompute_derived();
      [[nodiscard]] int tap_weight(TapJudgment j) const;

      const Chart* chart_ = nullptr;
      const JudgmentConstants* constants_ = nullptr;
      std::vector<int> note_row_;               // note_index -> row id (-1 = mine/unscored)
      std::vector<RowAggregate> rows_;
      std::vector<bool> hold_scored_;           // per-note duplicate guard
      ScoreState state_;
  };

  } // namespace blaze4k
  ```
  - Includes `<array>`, `<vector>`, `<cstddef>`, `"chart/chart.hpp"`, `"gameplay/judgment.hpp"`,
    `"timing/judgment_constants.hpp"`. **No** SDL/GL/`<chrono>`.
- **Mirror**: `src/gameplay/judgment.hpp:1-43` (pure value types), `judgment_engine.hpp:21-69`.
- **Validate**: `cmake --build build -j16` once included by Task 6.

### Task 2: Reset — row tables and possible DP

- **File**: `src/gameplay/score_keeper.cpp`
- **Action**: CREATE
- **Implement**:
  - `reset(chart, constants)`: store pointers; `state_ = ScoreState{}`; clear vectors.
  - Null/empty chart → `note_row_` empty, `rows_` empty, `state_.possible_dp = 0`; return.
  - Build rows by iterating `chart->notes` (already beat-sorted, `note_parser.cpp:244-248`):
    - Skip `NoteType::Mine` (mines never join a scoring row).
    - Start a new row whenever `beat` differs from the previous non-mine note's beat; set
      `note_row_[i] = row`, `rows_[row].expected++`.
  - `hold_scored_.assign(chart->notes.size(), false)`.
  - Denominator:
    ```cpp
    const int row_count = static_cast<int>(rows_.size());
    state_.possible_dp = row_count * constants_->dp_weights.fantastic
                       + chart->hold_count * constants_->dp_weights.hold_ok
                       + chart->roll_count * constants_->dp_weights.hold_ok;
    ```
  - `recompute_derived()`: `state_.percent = (possible==0) ? 0.0 : (actual==possible ? 1.0 :
    double(actual)/possible)`; `state_.grade = &constants_->grade_for_percent(state_.percent)`.
  - `grade()`: return `constants_->grade_for_percent(state_.percent)`; if `constants_ == nullptr`,
    return a function-local static `GradeTier{-1000.0, "D"}` fallback.
- **Mirror**: `src/gameplay/judgment_engine.cpp:19-49` (reset/index building), `PlayerStageStats.cpp:218-232`.
- **Validate**: `cmake --build build -j16` (once included by Task 6).

### Task 3: Consume — tap/miss row aggregation and row scoring

- **File**: `src/gameplay/score_keeper.cpp`
- **Action**: CREATE (same file as Task 2)
- **Implement**:
  - `consume(const JudgmentEvent& e)`: dispatch by `e.kind`:
    - `Tap` (non-mine `note_type`) → `apply_tap_like(e, false)`
    - `Miss` → `apply_tap_like(e, true)`
    - `HoldOk`/`HoldNg` → `apply_hold(e.hold)`
    - `HitMine` → `state_.actual_dp += dp_weights.hit_mine; state_.tap_counts[HitMine]++`
      (no combo change)
    - `AvoidedMine`, `RollHit` → ignore
    - then `recompute_derived()`
  - `consume(const std::vector<JudgmentEvent>&)`: loop `consume`.
  - `apply_tap_like(e, miss)`:
    - Guard `e.note_index` in range and `note_row_[i] >= 0`; return otherwise.
    - Duplicate guard: `if (row.judged >= row.expected) return;`.
    - `row.judged++`.
    - If `miss || e.window == TapJudgment::Miss` → `row.has_miss = true`.
    - Else if `e.delta_ms >= row.last_delta_ms` (ties → later column, mirroring
      `NoteDataWithScoring.cpp:148-155`) → record `last_delta_ms`, `last_column`, `last_window`.
    - When `row.judged == row.expected` → `resolve_row(row)`.
  - `resolve_row(r)`:
    - `const TapJudgment score = row.has_miss ? TapJudgment::Miss : row.last_window;`
    - `if (score == TapJudgment::Num) return;` (defensive; TNS_NONE cannot occur)
    - `state_.actual_dp += tap_weight(score); state_.tap_counts[score]++;`
    - Combo (reuse B2 predicate; **not** enum `>=`):
      ```cpp
      if (constants_->continues_combo(score)) {
          state_.combo += row.expected;      // notes_in_row, ComboIsPerRow=false
          state_.miss_combo = 0;
      } else {
          state_.combo = 0;
          if (score == TapJudgment::Miss) state_.miss_combo++;
      }
      state_.max_combo = std::max(state_.max_combo, state_.combo);
      ```
  - `tap_weight(j)`: switch over `dp_weights.fantastic/excellent/great/decent/way_off/miss/hit_mine`;
    `Num` → 0.
- **Mirror**: `src/ScoreKeeperMAX2.cpp:343-368`; `src/Player.cpp:1544-1558`; `NoteDataWithScoring.cpp:133-190`.
- **Validate**: `cmake --build build -j16` with zero warnings.

### Task 4: Consume — holds, mines, ignored events; derived accessors

- **File**: `src/gameplay/score_keeper.cpp`
- **Action**: CREATE (same file as Task 2)
- **Implement**:
  - `apply_hold(h)`:
    - Duplicate guard using `hold_scored_` is optional (B4 emits one outcome per hold); if added,
      key on the event's `note_index`.
    - `Ok` → `actual_dp += dp_weights.hold_ok; hold_counts[Ok]++`.
    - `Ng` → `actual_dp += dp_weights.hold_ng; hold_counts[Ng]++`.
    - **No combo change** (`ScoreKeeperMAX2.cpp:414-437`).
  - `is_complete()`: all `rows_` have `judged == expected` and every hold/roll note has a hold outcome
    (track via `hold_scored_`); mines do not block completion. (Used by C7/B6; safe to compute
    incrementally.)
  - Keep `recompute_derived()` called once at the end of each `consume`.
- **Mirror**: `src/ScoreKeeperMAX2.cpp:414-437`; `PlayerStageStats.cpp:218-232`.
- **Validate**: `cmake --build build -j16`.

### Task 5: HUD renderer (score %, combo, judgment counts)

- **Files**: `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: CREATE
- **Implement**:
  - Pure, unit-testable helpers (in the header):
    ```cpp
    // OpenITG PercentageDisplay formatting: truncate (not round) to 2 decimals,
    // +0.000001 boost, display-clamped to [0,1] (PercentageDisplay.cpp:135-146).
    [[nodiscard]] std::string format_percent(double percent);
    [[nodiscard]] std::string format_combo(int combo);   // e.g. "123"
    ```
    `format_percent` clamps to `[0,1]` **for display only** (grade uses the unclamped value),
    truncates toward zero to 2 decimals, and appends `'%'`.
  - `class HudRenderer` with
    `void render(const ScoreState& state, int screen_w, int screen_h, GlQuadRenderer& renderer) const;`
    - Draw with `renderer.draw_quad` only; early-return when `screen_w<=0 || screen_h<=0`.
    - A self-contained **5×7 bitmap font** for the glyphs `0-9 . % x -` (constexpr `uint8_t[7]` per
      glyph) and a `draw_text`/`draw_glyph` that emits one `draw_quad` per set pixel. No external font
      file, no stb_truetype, no asset/licensing dependency.
    - Layout (top-left origin, pixel space):
      - top-left: `format_percent(state.percent)` (e.g. `97.45%`);
      - top-centre: `format_combo(state.combo)` with an `x` suffix;
      - top-right: six color-coded chips (Fantastic/Excellent/Great/Decent/WayOff/Miss) each with its
        `tap_counts` value (plus OK/NG hold chips if space allows).
    - Colors are Blaze 4k presentation (see Open Questions); use a fixed palette constant.
  - Header only forward-declares `class GlQuadRenderer;` and includes `"gameplay/score_keeper.hpp"`
    (which is GL-free), so the pure formatters are testable without a GL context.
- **Mirror**: `src/gameplay/note_field_renderer.cpp:20-42` (draw calls), `src/render/geometry.hpp:6-19`.
- **Validate**: `cmake --build build -j16`.

### Task 6: GameplayView integration

- **Files**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: `#include "gameplay/score_keeper.hpp"`, `#include "gameplay/hud_renderer.hpp"`; add
    members `ScoreKeeper score_; HudRenderer hud_; std::vector<JudgmentEvent> new_events_;` and:
    ```cpp
    [[nodiscard]] const ScoreState& score_state() const { return score_.state(); }
    [[nodiscard]] int dance_points() const { return score_.actual_dance_points(); }
    [[nodiscard]] double score_percent() const { return score_.percent(); }
    ```
  - `init`: after `judge_.reset(&chart_, &constants);` add `score_.reset(&chart_, &constants);`.
  - `update`: after `judge_.update(...)`:
    ```cpp
    new_events_.clear();
    judge_.drain_new_events(new_events_);
    score_.consume(new_events_);
    ```
    (`fixed_dt` still only advances the demo stub clock; it never reaches scoring.)
  - `render`: after the note-field render, call `hud_.render(score_.state(), screen_w, screen_h,
    renderer);`. Headless is safe (uninitialized renderer is a no-op; HUD still computes state).
  - `shutdown`: replace/extend the session log line with DP, percent, grade label, combo/max, and the
    per-window counts, e.g. `[GameplayView] Score: DP 123/250 (49.20%) grade C | combo 12 (max 30)`.
- **Mirror**: `src/gameplay/gameplay_view.cpp:125-147,149-182,184-195`.
- **Validate**: `cmake --build build -j16`.

### Task 7: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root: add `src/gameplay/score_keeper.cpp` and `src/gameplay/hud_renderer.cpp` to `blaze4k_core`
    after `src/gameplay/judgment_engine.cpp`.
  - Tests: append a `score_keeper_test` block mirroring `judgment_engine_test`
    (`tests/CMakeLists.txt:114-122`).
- **Mirror**: `CMakeLists.txt:96-104`, `tests/CMakeLists.txt:114-122`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 8: Test suite

- **File**: `tests/score_keeper_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; synthetic `Chart` via `make_note` from
  `tests/judgment_engine_test.cpp:27-36`; `JudgmentConstants::compiled_defaults()`; deterministic;
  events fed both synthetically and via `JudgmentEngine` + `drain_new_events`):
  1. **Possible DP denominator**: chart with 2 separate taps + a 2-note jump + 1 hold + 1 roll + 1 mine
     → `possible_dp == 3*5 + 1*5 + 1*5 == 25`; rows built by beat (jump = 1 row, mine = 0 rows).
  2. **Single-tap DP/percent** (fresh chart per case, one row): Fantastic → `actual 5`, `percent 1.0`;
     Excellent → 4 / 0.8; Great → 2 / 0.4; Decent → 0 / 0.0; Way Off → −6 / −1.2; Miss → −12 / −2.4.
  3. **Grade mapping**: feed events producing percent 1.0/0.99/0.98/0.96/0.94/0.55/below →
     `grade().label` `quad_star/triple_star/double_star/single_star/S+/C-/D`; assert
     `grade() == constants.grade_for_percent(percent())`.
  4. **Row aggregation — jump counts once**: 2-note jump, both Great → `tap_counts[Great]==1`,
     `actual_dp` increased by exactly one Great weight, `combo==2`.
  5. **Last-tap semantics**: jump with col0 early (delta −50 ms, Great) and col1 late (+10 ms,
     Excellent) → row score Excellent; one hit + one Miss → row Miss (`has_miss` dominates).
  6. **Combo rules**: three separate Great taps → combo 1,2,3 / max 3; Decent → combo 0; Miss → combo 0
     and `miss_combo==1`; Great after miss → combo 1 and `miss_combo==0`; jump Great+ → +2; `HoldNg`
     → combo unchanged; `HitMine` → combo unchanged.
  7. **Hold DP**: head Fantastic + HoldOk → `actual 10`, possible 10, 100%; head Great + HoldOk → 7,
     70%; head Fantastic + HoldNg → 5, 50%; head Miss → −12 (no hold outcome), −120%.
  8. **Roll DP**: head Fantastic + RollOk → 10, 100%; `RollHit` events add nothing.
  9. **Mine**: `HitMine` → `actual −6`, `tap_counts[HitMine]==1`, combo unchanged; `AvoidedMine` →
     no change.
  10. **Percent edge cases**: empty chart / `reset(nullptr)` → percent 0, safe no-ops; a chart whose
      only judgment is Fantastic on every row → `actual == possible` → exactly `1.0`.
  11. **Idempotence**: consuming the same event twice does not double-count (row/duplicate guards).
  12. **Incremental == batch**: one-by-one `consume` equals `consume(vector)` on identical event logs.
  13. **Engine integration / reference chart**: build a chart, run `JudgmentEngine` through a scripted
      hit/miss sequence, `drain_new_events`, consume, and assert DP/percent/combo/counts against
      hand-computed OpenITG arithmetic (document the arithmetic in a comment).
  14. **HUD formatting**: `format_percent(1.0)=="100.00%"`, `format_percent(0.9745)=="97.45%"`,
      truncation not rounding (`0.99999 → "99.99%"`), negative clamps to `"0.00%"` for display,
      `format_combo(0)=="0"`.
- **Mirror**: `tests/judgment_engine_test.cpp:1-120`, `tests/judgment_constants_test.cpp`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 13/13).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 13/13: 12 existing + score_keeper_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/score_keeper_test

# Headless harness smoke (stub clock; fixture has taps/holds/rolls/mines)
./build/blaze-4k --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --smoke-test 120

# Purity check: scoring core must not touch platform/time/GL headers
rg -n "SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter|GetTicksNS|fixed_dt" \
  src/gameplay/score_keeper.hpp src/gameplay/score_keeper.cpp
# (no matches)
```

## End-to-End Verification

1. `./build/tests/score_keeper_test` prints each sub-check; tests 1-3 prove the DP/percent/grade
   arithmetic against the pinned OpenITG formula; tests 4-6 prove row grouping, last-tap semantics, and
   the combo rules; tests 7-9 prove hold/roll/mine scoring; test 13 proves the keeper derives everything
   from the B4 log.
2. `ctest --test-dir build --output-on-failure` → **13/13**; all 12 prior tests stay green (changes are
   additive plus a `GameplayView` internal member).
3. `--headless --gameplay-demo ... --smoke-test 120` exits cleanly; the `[GameplayView]` shutdown line
   reports DP/percent/grade/combo/max and per-window counts; no GL calls are attempted headless (HUD
   draw is a no-op).
4. On a display: run `--gameplay-demo` on the fixture and confirm the top-left percent, top-centre
   combo, and the judgment-count chips update live as notes are hit/missed, matching the console
   session summary at exit. Hits Great-or-better advance combo by the row's note count; Decent/Way
   Off/Miss reset it; a hit mine lowers DP without touching combo.
5. Enforce module purity with the `rg` command in **Validation** → the scoring core contains no
   SDL/GL/`<chrono>` and no frame-delta math; `fixed_dt` reaches only the demo stub clock and the HUD
   never reads a clock.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Row-vs-note semantics: OpenITG scores/misses per **row** but B4 logs per-note | B5 groups by exact `Note.beat` (via `note_index`), scores once per completed row, and adds `notes_in_row` to combo; tests 4-6 pin it | **In scope** |
| Grouping by `note_time_seconds` could merge distinct rows across a stop | Group by `beat` (canonical row id), not seconds; documented in Pinned Semantics | **In scope** |
| B2 enum order is not quality-monotonic → `score >= Great` is wrong | Use `constants_->continues_combo(score)` for the Great+ test; explicit Miss check for `miss_combo` | **In scope** |
| "Last tap" = latest hit, not highest column; a miss anywhere wins | Buffer per-row events, pick max `delta_ms` (ties → later column), miss dominates; tests 4-5 | **In scope** |
| Combo/hold/mine interaction is easy to mis-model | Mines and hold outcomes never change combo (OpenITG); test 6 | **In scope** |
| Judgment counts are per row, not per note | `tap_counts` incremented once in `resolve_row`; test 4 | **In scope** |
| Denominator must be chart-derived (full-song), not event-derived | Compute `possible_dp` in `reset()` from rows + `hold_count`/`roll_count`; test 1 | **In scope** |
| Issue AC says "Fantastic +2 DP" but OpenITG arcade is +5 | Consume B2 `dp_weights` (Fantastic 5, Great 2); re-flag as a blocking Open Question for human ratification | **In scope** — flagged |
| PRD says "99%+ = ★★★★" but OpenITG four-star = 1.00 | Consume B2 tiers (99% = triple star); re-flag as an Open Question | **In scope** — flagged |
| HUD needs text but no font/asset exists | Self-contained 5×7 bitmap font drawn as quads (no asset/licensing); pure `format_percent` is unit-tested | **In scope** |
| HUD GL calls crash headless | `GlQuadRenderer` is a documented no-op when uninitialized; E2E step 3 | **In scope** |
| `bFailedEarlier` / life not yet implemented | Grade always uses `grade_for_percent`; `GRADE_FAILED` is B6; documented | **Out of scope** — B6 |
| Last notes still unjudged when the demo stub clock stops | Production audio clock runs past the chart; demo harness only. Ensure `judge_.update` continues to the chart end | **Out of scope** — documented |
| Per-frame full row scans on dense charts | Row tables built once; `consume` is O(1) per event; no per-frame chart scans | **In scope** |
| `ScoreKeeper` scope creep into life/fail/results | Scoring only; `is_complete()`/`grade()` exposed for B6/C7 | **In scope** — boundary |

---

## Decisions

- **Row identity = exact `Note.beat`** (via `note_index`), not `note_time_seconds`, so distinct rows
  separated only by a stop are not merged. Mines never define or join a scoring row.
- **Row score uses OpenITG last-tap semantics**: miss dominates; otherwise the latest-offset hit
  (`max delta_ms`, ties → later column). B4's `delta_ms` carries the OpenITG sign convention.
- **Scoring consumes only the B4 log**; the denominator is a chart-derived maximum computed once in
  `reset()` (mirroring OpenITG's radar-based `iPossibleDancePoints`). No independent judgment.
- **Combo uses `constants.continues_combo`**, not enum ordering, to avoid B2's non-monotonic enum.
- **Mines and hold/roll outcomes never change combo** (OpenITG `HandleTapScore`/`HandleHoldScore`).
- **`possible_dp` uses the full-song denominator**, matching `GetPercentDancePoints` (not the live
  `iCurPossibleDancePoints`, which OpenITG only uses for the subtract display / Oni life meter).
- **Grade = `grade_for_percent(percent)`** because arcade DP and grade weights are identical.
- **HUD is self-contained**: 5×7 bitmap font drawn as quads; no external font asset, no stb_truetype
  yet. `format_percent` mirrors OpenITG's truncate-and-display-clamp formatting.
- **No B4 changes**: row aggregation uses `note_index` + the chart, so `JudgmentEvent` stays frozen.

---

## Open Questions

1. **BLOCKING — "Fantastic +2 DP" vs OpenITG source (needs human decision; carried from B2 OQ1).**
   PRD §5 story 2 and issue #13 AC1 say "Fantastic +2 DP". OpenITG's runtime arcade table gives
   `PercentScoreWeightMarvelous=5` and `GradeWeightMarvelous=5`; the value `2` is the arcade
   `PercentScoreWeightGreat`/`GradeWeightGreat` (and the StepMania-4 compiled grade default). Proposed
   default: **Fantastic = +5 DP, Great = +2 DP** (B2's sourced arcade table), treating the PRD "+2" as
   a stale code-default example. Confirm before parity tests are considered authoritative.
2. **BLOCKING (presentation) — PRD star example.** PRD §5 says "99%+ earns ★★★★"; OpenITG arcade sets
   four-star (quad) = **1.00** and three-star = **0.99**. Proposed default: follow OpenITG (four stars
   requires 100%); 99% maps to triple star. Confirm Blaze 4k's star presentation, or B5's grade tests
   encode the wrong label. (B2 already seeded the OpenITG tiers.)
3. **HUD text approach (non-blocking).** No font/text system or font asset exists. Proposed default:
   a self-contained 5×7 bitmap font drawn as solid quads (no asset/licensing, headless-safe). Confirm,
   or prefer introducing a stb_truetype text renderer + bundled font now (would expand B5 scope and
   need a font-licensing decision).
4. **HUD content/placement (non-blocking).** Proposed: top-left percent, top-centre combo, top-right
   per-window judgment chips (color-coded). Confirm the exact fields (include hold OK/NG chips? show
   grade live?) and whether the live HUD should clamp negative percent for display (proposed yes,
   matching `PercentageDisplay.cpp:110-116`).
5. **Live vs full-song denominator (non-blocking).** Proposed: HUD percent = `actual / possible`
   (full-song), matching OpenITG `GetPercentDancePoints`. Confirm Blaze 4k does not want the "current max"
   denominator (`GetCurMaxPercentDancePoints`) used for a subtract-style display.
6. **`is_complete()` consumer (non-blocking).** Proposed: expose row/hold completion for B6's fail
   transition and C7's results trigger. Confirm B6 wants this rather than its own end-of-chart signal.

---

## Acceptance Criteria

- [ ] Given a judgment event stream, when scoring runs, then DP totals accumulate per OpenITG weights
      from B2 (`dp_weights`), with tap rows counted once, holds by outcome, and hit mines penalized
      (tests 2, 4, 7-9)
- [ ] Given a completed chart, when the percentage is computed, then it matches
      `actual/possible` with the OpenITG denominator (`rows*5 + holds*5 + rolls*5`) for a known
      reference chart (tests 1, 2, 13 + Value Provenance)
- [ ] Given the DP%, when mapped to a grade, then ★ grade thresholds match the OpenITG arcade tiers via
      B2 `grade_for_percent` (test 3 + Value Provenance); PRD's "99%+ = ★★★★" discrepancy flagged
- [ ] Given a combo-affecting judgment, when it occurs, then combo increments on Great-or-better by the
      row's note count and breaks on Decent-or-worse; misses increment `miss_combo`; mines and hold
      outcomes do not affect combo (test 6)
- [ ] Given live gameplay, when the HUD renders, then score %, combo, and judgment counts update from
      the event log (Task 5/6 + E2E steps 3-4); formatting is unit-tested (test 14)
- [ ] Scoring derives **only** from the B4 event log (plus the chart-derived denominator) — no
      independent judgment logic; no frame/wall-clock logic in the scoring path (purity `rg` check)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 13/13 pass (12 prior + `score_keeper_test`)
- [ ] Follows existing module/naming/test/CMake patterns; no life/fail/results logic added (B6/C7)
