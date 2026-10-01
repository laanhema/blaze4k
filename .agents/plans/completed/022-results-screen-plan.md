# Plan: Results Screen with Grade, Breakdown, and NEW RECORD Flag (C7)

## Summary

Add the missing **Results** screen to the arcade shell. `ScreenId::Results` already exists
(`src/screens/screen.hpp:25`) and `ctx.scores` is documented as "C3/C7 read; C7 submits"
(`screen.hpp:36`), but no results screen is registered and `GameplayScreen` currently transitions
straight to `Select` when a run ends (`gameplay_screen.cpp:73-75`). C7 closes the loop:
`GameplayScreen`, on `GameplayOutcome::Cleared`/`Failed`, snapshots the event-sourced
`ScoreState` (`gameplay/score_keeper.hpp:18-28`, itself derived from the B4 `JudgmentEvent` log per
PRD §6 pattern 2) into a new **pure** `ResultsSummary` value, publishes it through a
`PlayRequest`-style context handoff, and transitions to `Results`. `ResultsScreen` reads that
snapshot, submits the best record to the in-memory `HighScores` (first entry or strictly greater
percent, via the existing `submit_high_score`, `high_scores.cpp:330-342`), renders grade / % / DP /
per-window counts / max combo plus a `NEW RECORD` flag or `FAILED` banner, and returns to the song
wheel on Confirm (and Back). Score persistence needs no new path: `main` already saves
`scores.json` on clean exit (`main.cpp:358-361`), exactly like C4/C5 options/offset.

## User Story

As a player
I want a results screen after each song showing my grade, percentage, DP, judgment breakdown,
max combo, and a NEW RECORD flag on personal bests
So that I get an honest arcade-style evaluation and can chase my own records.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/` (new pure results model + `ResultsScreen`, context handoff, manager back-nav), `src/gameplay/` (`GameplayScreen` handoff only — no judgment/scoring change), `src/data/` (scores read/submit), `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #22 ([C7]) |
| PRD refs | §7.3 Results, §5 story 6, §11 complete-loop criterion, §12 Phase C |
| Depends on | C1 (#16 screens), B5 (#13 scoring), B4 (#12 event log), C2 (#17 persistence) — all merged |
| Blocks | D3 (grade animations / NEW RECORD polish) |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` from root CMake (no `-Werror`) |
| Cores | 16 | `-j16` safe |
| Baseline tests | **25/25 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 25" (0.38 s), run this session |
| `ScreenId::Results` | `src/screens/screen.hpp:25` | Already declared; `screen_id_name` has a case (`screen_manager.cpp:26`) but **no screen is registered** and it is deliberately excluded from back-nav |
| `ScreenContext` seams | `src/screens/screen.hpp:33-54` | `config`, `scores`, `play_request`, `constants`, `action_down`, `input`; `scores` = "C3/C7 read; C7 submits" |
| End-of-run transition | `src/screens/gameplay_screen.cpp:73-75` | `outcome() != InProgress` → `transition_to(ScreenId::Select)`; comment says "Results/pause/retry belong to C7" |
| Score snapshot API | `src/gameplay/gameplay_view.hpp:58-66` | `score_state()`, `dance_points()`, `score_percent()`, `has_failed()`, `is_cleared()`, `outcome()` |
| Scores API | `src/data/high_scores.hpp:21-62` | `ScoreRecord{grade,percent,dp,timestamp unix}`; `make_chart_key`, `submit_high_score` (returns true on first entry or strictly greater %), `save/load_high_scores` |
| Persistence | `src/main.cpp:358-361` | Clean-exit `save_high_scores`; in-memory `high_scores` round-trips |
| Formatting | `src/gameplay/hud_renderer.cpp:34-76` | `format_percent` (OpenITG truncate/clamp) and `format_grade` (`quad_star`→`****`, letters pass through) |
| Grade tiers | `src/timing/judgment_constants.cpp:36-55` | 17 arcade tiers, `{1.00,"quad_star"} … {-1000,"D"}` |
| Headless completed run | `tests/score_keeper_test.cpp:641-661` | One-tap `Chart`, `GameplayView::init(chart,k,"",options)`, 90×`update(1/60, none)` → Miss → `is_complete()`; the exact pattern for a headless end-to-end test |
| Test idiom / registration | `tests/calibration_screen_test.cpp:24-31,112-142`; `tests/CMakeLists.txt:206-214` | `TEST_CHECK` (abort); one `add_executable`/`target_link_libraries(blaze4k_core)`/`add_test` per target; `Spy` screens + `ScreenManager` |
| Screen test manager setup | `tests/select_screen_test.cpp:540-548` | `ScreenManager(0.0)` (idle disabled), register spies, wire context, `start(...)` |

**Start green, stay green:** 25 tests pass; this plan adds **2** targets (`results_test`,
`results_screen_test`) and extends `screen_manager_test` in place → **27 expected**. No
`src/timing/*`, `src/gameplay/judgment*`, `src/gameplay/score_keeper.cpp`, `src/gameplay/life_keeper.cpp`,
`src/chart/*`, `src/audio/*`, or `src/render/*` changes.

---

## Pinned Semantics

Authority: **PRD §7.3/§5 story 6/§11**, the **B5 `ScoreState`** and **`high_scores`** public
contracts, and the existing **C1/C4/C5 screen pattern**. No new numerical constants are invented:
grade thresholds, grade labels, and percent formatting are already pinned from OpenITG
(`judgment_constants.cpp:36-55`, `hud_renderer.cpp:34-55`).

### Run-result snapshot (pure value, event-sourced)

Everything shown derives from `GameplayView::score_state()` (a `ScoreState`) plus the life fail
flag. `ResultsSummary` is a flat copy so the results screen never holds `GameplayView` state and
stays headless-testable:

```cpp
// src/screens/results.hpp
struct ResultsSummary {
    bool valid = false;
    const Song* song = nullptr;   // SongLibrary-owned; null-safe
    const Chart* chart = nullptr;
    bool failed = false;          // GameplayOutcome::Failed (life empty, fail-enabled)
    std::string grade_label;      // GradeTier::label copied from state.grade ("" if null)
    double percent = 0.0;         // ScoreState::percent, unclamped
    int actual_dp = 0;            // ScoreState::actual_dp
    int possible_dp = 0;          // ScoreState::possible_dp
    int max_combo = 0;            // ScoreState::max_combo
    std::array<int, (std::size_t)TapJudgment::Num>  tap_counts{};  // F/E/G/D/W/M/Mine
    std::array<int, (std::size_t)HoldJudgment::Num> hold_counts{}; // OK/NG
};
[[nodiscard]] ResultsSummary results_summary_from(const ScoreState& state, bool failed,
                                                  const Song* song, const Chart* chart);

// Submits the run's best record and returns true when it is a new personal best.
// No-op (false) for invalid or failed runs. Timestamp injected so the model stays
// free of <ctime>.
[[nodiscard]] bool results_submit_score(HighScores& scores, const ResultsSummary& summary,
                                        std::int64_t timestamp_unix);
```

### NEW RECORD semantics

`results_submit_score` calls the existing `submit_high_score` (`high_scores.cpp:330-342`) with a
`ScoreRecord{summary.grade_label, summary.percent, summary.actual_dp, timestamp_unix}` keyed by
`make_chart_key(*song, *chart)` (`high_scores.cpp:129-159`). It returns `submit_high_score`'s
result: **true** on the first-ever record for the chart, or when `percent` is strictly greater than
the stored best; **false** on a tie or a worse run. The screen shows `NEW RECORD` iff true.
Failed runs never submit and never flag (see Decisions / OQ2).

### Handoff contract (mirrors `PlayRequest`)

Gameplay owns the live `GameplayView`; Results owns presentation. A single context pointer bridges
them, exactly like `ScreenContext::play_request` (`screen.hpp:41`):

```cpp
// ScreenContext addition
struct ResultsSummary;            // forward declaration near GameplayScreen/PlayRequest
ResultsSummary* results = nullptr; // C7: GameplayScreen writes on run end; ResultsScreen reads
```

- Wired by `main` (owned as a local `blaze4k::ResultsSummary`, like `blaze4k::PlayRequest`).
- `GameplayScreen::update`, when `view_.outcome() != GameplayOutcome::InProgress`, fills
  `*ctx.results = results_summary_from(...)` once and transitions to `Results`.
- `ResultsScreen::enter` copies `ctx.results` (or an invalid default) and submits.

### Transition / navigation contract

- Run ends → `GameplayScreen` → `Results` (replaces the current → `Select`).
- `ResultsScreen` Confirm (also Options / Right) → `Select`.
- `Results` is added to the manager's default back-navigation set; Back/`Escape` → `Select`.
  This **changes the documented "Title/Results do not consume Back" comment**
  (`screen_manager.hpp:16-20`, `screen_manager.cpp:13-16`) — see OQ3.
- Back-*abort* of an in-progress run is unchanged: `Gameplay` Back → `Select`, no result, no submit.
- Idle→Attract only fires from Title/Select (`screen_manager.cpp:219-229`), so Results cannot
  become a dead end.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Grade thresholds + labels (`quad_star` … `D`) | `src/timing/judgment_constants.cpp:36-55` (already pinned OpenITG arcade `[Grade] Tier01..17`) | Sourced (reused) |
| Percent display (truncate 2 dp, `+1e-6`, clamp [0,1]) | `src/gameplay/hud_renderer.cpp:34-55` (PercentageDisplay.cpp:110-146) | Sourced (reused) |
| Grade label rendering (`quad_star`→`****`) | `src/gameplay/hud_renderer.cpp:61-76` | Sourced (reused) |
| NEW RECORD rule (first entry, or strictly greater percent; ties keep best) | `src/data/high_scores.cpp:330-342` | Sourced (reused) |
| Record fields persisted (grade, %, DP, timestamp) | `src/data/high_scores.hpp:21-26`, `save_high_scores` `high_scores.cpp:269-328` | Sourced (reused) |
| Failed runs do not submit a record | Issue/PRD silent; OpenITG arcade behavior | **Design decision — OQ2** |
| First-ever play counts as NEW RECORD | `submit_high_score` semantics | **Design decision — OQ1** |
| Back on Results returns to wheel | Issue AC3 says "confirmed"; existing shell comment excludes Results from back-nav | **Design decision — OQ3** |
| Timestamp from wall clock (`std::time`) at results entry | Metadata only; not on the judgment path (AGENTS principle 1 untouched) | **Design decision** |

No judgment windows, DP weights, grade boundaries, or life deltas are introduced or changed.

---

## Patterns to Follow

### Pure, SDL-free value + free functions (options/remap model seam)
```cpp
// SOURCE: src/screens/options_menu.hpp:29-67; src/screens/input_remap.hpp
struct OptionsMenu { ... };
[[nodiscard]] OptionsMenu options_menu_from_config(const GameConfig&);
void options_menu_apply(const OptionsMenu&, GameConfig&);
```
`results.hpp`/`results.cpp` must include only `<array>`, `<cstdint>`, `<string>`,
`gameplay/judgment.hpp`, `gameplay/score_keeper.hpp`, `timing/judgment_constants.hpp`,
`data/high_scores.hpp` (for `HighScores`) — **no SDL/GL/audio/clock**.

### Screen lifecycle + explicit transition
```cpp
// SOURCE: src/screens/calibration_screen.cpp:92-134; src/screens/screen.hpp:59-81
void enter(ScreenContext&) override;
void update(ScreenContext&, double, const std::vector<InputEvent>&) override;
void render(ScreenContext&, GlQuadRenderer&, int, int) override;
```

### Event-sourced end of run + transition (the line C7 changes)
```cpp
// SOURCE: src/screens/gameplay_screen.cpp:73-75
if (view_.outcome() != GameplayOutcome::InProgress && ctx.manager != nullptr) {
    ctx.manager->transition_to(ScreenId::Select);
}
```

### Snapshot from the already-derived score state
```cpp
// SOURCE: src/gameplay/gameplay_view.hpp:58-66
[[nodiscard]] const ScoreState& score_state() const;
[[nodiscard]] bool has_failed() const;
[[nodiscard]] GameplayOutcome outcome() const;
```

### Best-score submit (return value = new record)
```cpp
// SOURCE: src/data/high_scores.cpp:330-342; src/data/high_scores.hpp:55-56
[[nodiscard]] bool submit_high_score(HighScores&, const std::string& chart_key,
                                     const ScoreRecord&);
```

### Manager back-nav set (add Results here)
```cpp
// SOURCE: src/screens/screen_manager.cpp:13-16,135-155
bool default_back_navigates(ScreenId id) {
    return id == ScreenId::Attract || id == ScreenId::Select || id == ScreenId::Gameplay ||
           id == ScreenId::Calibration || id == ScreenId::InputRemap;
}
```

### Test idiom + registration
```cpp
// SOURCE: tests/calibration_screen_test.cpp:24-31; tests/CMakeLists.txt:206-214
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
add_executable(results_test results_test.cpp)
target_link_libraries(results_test PRIVATE blaze4k_core)
add_test(NAME results_test COMMAND results_test)
```

### Headless completed run (for the end-to-end handoff test)
```cpp
// SOURCE: tests/score_keeper_test.cpp:641-661
blaze4k::Chart chart; chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap)); chart.tap_count = 1;
blaze4k::GameplayView view; view.init(chart, k, "", options);
for (int i = 0; i < 90; ++i) view.update(1.0/60.0, held_none()); // tap expires -> Miss -> is_complete()
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/results.hpp` | CREATE | Pure `ResultsSummary` + `results_summary_from` / `results_submit_score`; no SDL/GL/time |
| `src/screens/results.cpp` | CREATE | Snapshot copy from `ScoreState`; chart-key + `submit_high_score` wrapper with failed/invalid guard |
| `src/screens/results_screen.hpp` | CREATE | `ResultsScreen : Screen` + test accessors (`summary`, `new_record`, `submitted`) |
| `src/screens/results_screen.cpp` | CREATE | Read handoff, submit, render grade/%/DP/counts/combo/NEW RECORD/FAILED, Confirm/Back → Select |
| `src/screens/screen.hpp` | UPDATE | Forward-declare `ResultsSummary`; add `ResultsSummary* results = nullptr;` to `ScreenContext` |
| `src/screens/gameplay_screen.hpp` | UPDATE | Add `bool end_reported_ = false;` + `[[nodiscard]] bool end_reported() const`; refresh header comment |
| `src/screens/gameplay_screen.cpp` | UPDATE | On run end: publish `*ctx.results`, transition to `Results` (fallback to `Select` if Results unregistered) |
| `src/screens/screen_manager.cpp` | UPDATE | `Results` in `default_back_navigates`; `handle_back` case `Results → Select`; comment |
| `src/screens/screen_manager.hpp` | UPDATE | Update the Back-navigation contract comment (Results now consumes Back) |
| `src/main.cpp` | UPDATE | Include + declare `blaze4k::ResultsSummary results;`; register `ResultsScreen`; wire `context().results` |
| `CMakeLists.txt` | UPDATE | Add `src/screens/results.cpp` + `src/screens/results_screen.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `results_test` and `results_screen_test` |
| `tests/results_test.cpp` | CREATE | Pure model: snapshot copy, submit first/lower/higher/tie, failed/invalid no-op |
| `tests/results_screen_test.cpp` | CREATE | Headless integration: enter→submit→flag, render, Confirm/Back→Select, Gameplay→Results end-to-end |
| `tests/screen_manager_test.cpp` | UPDATE | Results Back → Select; `back_navigates()` true on Results |

Not modified: `src/gameplay/judgment*`, `src/gameplay/score_keeper.*`, `src/gameplay/life_keeper.*`,
`src/gameplay/gameplay_view.*`, `src/gameplay/hud_renderer.*`, `src/timing/*`, `src/chart/*`,
`src/audio/*`, `src/render/*`, `src/data/high_scores.*`, `src/data/config*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure results model

- **File**: `src/screens/results.hpp`, `src/screens/results.cpp`
- **Action**: CREATE
- **Implement**:
  - `results.hpp`: forward-declare `struct Song; struct Chart;`, include
    `data/high_scores.hpp` for `HighScores` and `gameplay/score_keeper.hpp` for `ScoreState`.
    Declare `ResultsSummary` and the two free functions exactly as in **Pinned Semantics**.
  - `results.cpp`:
    - `results_summary_from`: copy `state.grade ? state.grade->label : ""`, `percent`, `actual_dp`,
      `possible_dp`, `max_combo`, both count arrays; set `failed`, `song`, `chart`, `valid = true`.
    - `results_submit_score`: return `false` if `!summary.valid || summary.failed || song/chart
      null || grade_label empty || percent not finite or < 0`; otherwise build
      `ScoreRecord{grade_label, percent, actual_dp, timestamp_unix}` and call
      `submit_high_score(scores, make_chart_key(*song, *chart), record)`.
  - Must compile with no SDL/GL/audio/`<ctime>` includes.
- **Mirror**: `src/screens/options_menu.hpp:29-67`, `input_remap.hpp` + `input_remap.cpp`.
- **Validate**: `cmake --build build -j16` (after Task 6 registers the `.cpp`).

### Task 2: `ScreenContext` handoff + manager back-nav

- **Files**: `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`, `src/screens/screen_manager.hpp`
- **Action**: UPDATE
- **Implement**:
  - `screen.hpp`: near `struct PlayRequest;` add `struct ResultsSummary;`; in `ScreenContext` after
    `play_request` add
    `ResultsSummary* results = nullptr; // C7: GameplayScreen writes on run end; ResultsScreen reads`.
  - `screen_manager.cpp`: add `|| id == ScreenId::Results` to `default_back_navigates` (`:13-16`);
    in `handle_back` add
    `else if (active_id_ == ScreenId::Results) { transition_to(ScreenId::Select); }` (`:141-154`).
  - `screen_manager.hpp`: update the class comment (`:16-20`) to state
    "Back from Attract → origin, Select → Title, Gameplay → Select (abort), Results → Select,
    Calibration/InputRemap → Select."
- **Mirror**: `src/screens/screen_manager.cpp:13-16,135-155`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` (Task 9 adds cases).

### Task 3: `ResultsScreen`

- **Files**: `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  class ResultsScreen : public Screen {
  public:
      [[nodiscard]] ScreenId id() const override { return ScreenId::Results; }
      void enter(ScreenContext& ctx) override;
      void update(ScreenContext&, double, const std::vector<InputEvent>&) override;
      void render(ScreenContext&, GlQuadRenderer&, int w, int h) override;
      // Test accessors
      [[nodiscard]] const ResultsSummary& summary() const { return summary_; }
      [[nodiscard]] bool new_record() const { return new_record_; }
      [[nodiscard]] bool submitted() const { return submitted_; }
      [[nodiscard]] bool valid() const { return summary_.valid; }
  private:
      ResultsSummary summary_{};
      bool new_record_ = false;
      bool submitted_ = false;
  };
  ```
  - `enter`: `summary_ = ctx.results != nullptr ? *ctx.results : ResultsSummary{};`
    `new_record_ = false; submitted_ = false;` If `summary_.valid && ctx.scores != nullptr`, call
    `results_submit_score(*ctx.scores, summary_, static_cast<std::int64_t>(std::time(nullptr)))`
    and set `submitted_ = true` (only when `!summary_.failed`; the helper already guards, but record
    `submitted_` to mean "a submission was attempted and permitted"). Log one line
    `[ResultsScreen] <title> <difficulty> <meter>: <grade> <percent> DP x/y FAILED|CLEARED NEW RECORD?`.
  - `update`: for each pressed event, `Confirm` / `Options` / `Right` →
    `if (ctx.manager != nullptr) ctx.manager->transition_to(ScreenId::Select);` and return. Back is
    owned by the manager default (Task 2).
  - `render`: guard `w<=0||h<=0`; full-screen backdrop (mirror `calibration_screen.cpp:143-144`);
    if `!summary_.valid` draw `NO RESULT` + `[ENTER] CONTINUE` and return. Otherwise:
    song title/artist (`summary_.song->metadata`), difficulty + meter (`summary_.chart->difficulty`,
    `->meter`); large `format_grade(...)` (build a `GradeTier{0, summary_.grade_label.c_str()}` or
    reuse a small local formatter) colored by tier; `format_percent(summary_.percent)`
    (`gameplay/hud_renderer.hpp`); `DP <actual>/<possible>`; `MAX COMBO <max_combo>`; one row per tap
    window (`F/E/G/D/W/M/MINE`) and hold (`OK/NG`) count; if `summary_.failed` a red `FAILED` banner;
    if `new_record_` an accent `NEW RECORD` banner; footer
    `[ENTER] CONTINUE` / (if failed) `[ENTER] RETURN TO WHEEL`. Headless renderer draws are no-ops.
  - Keep the screen free of SDL/GL complexity; only `render/gl_quad_renderer.hpp`,
    `render/bitmap_font.hpp`, `gameplay/hud_renderer.hpp`, `screens/play_request.hpp`/`song.hpp`/
    `chart.hpp` includes.
- **Mirror**: `src/screens/calibration_screen.cpp:136-181`, `select_screen.cpp:399-538`.
- **Validate**: `cmake --build build -j16`.

### Task 4: `GameplayScreen` end-of-run handoff

- **File**: `src/screens/gameplay_screen.hpp`, `src/screens/gameplay_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - `.hpp`: add `bool end_reported_ = false;` and
    `[[nodiscard]] bool end_reported() const { return end_reported_; }`; update the class comment
    ("returns to Select on Back-abort; reports to Results on run end").
  - `.cpp::enter`: reset `end_reported_ = false;`.
  - `.cpp::update` replace `:73-75` with:
    ```cpp
    if (view_.outcome() != GameplayOutcome::InProgress && ctx.manager != nullptr) {
        if (!end_reported_) {
            end_reported_ = true;
            if (ctx.results != nullptr && ctx.play_request != nullptr) {
                *ctx.results = results_summary_from(view_.score_state(), view_.has_failed(),
                                                    ctx.play_request->song, ctx.play_request->chart);
            }
            // Real app registers Results; tests/legacy fall back to Select so a run
            // can never strand on Gameplay when Results is absent.
            ctx.manager->transition_to(ctx.manager->has_screen(ScreenId::Results)
                                           ? ScreenId::Results
                                           : ScreenId::Select);
        }
    }
    ```
  - Add `#include "screens/results.hpp"`.
- **Mirror**: `src/screens/gameplay_screen.cpp:73-75`; `gameplay_view.hpp:58-66`.
- **Validate**: `cmake --build build -j16`; `./build/tests/select_screen_test` still green (it only
  aborts runs via Back, never completes one).

### Task 5: Wire `main`

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `#include "screens/results_screen.hpp"` and `#include "screens/results.hpp"`.
  - Near `blaze4k::PlayRequest play_request;` (`:212`) add `blaze4k::ResultsSummary results_summary;`.
  - Register `shell->add_screen(std::make_unique<blaze4k::ResultsScreen>());` alongside the other
    `add_screen` calls (`:294-300`).
  - Add `shell->context().results = &results_summary;` next to the other context wiring (`:301-309`).
- **Mirror**: `src/main.cpp:212-222,293-309`.
- **Validate**: `cmake --build build -j16`.

### Task 6: Register sources and test targets

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/screens/results.cpp` and `src/screens/results_screen.cpp` to the
  `blaze4k_core` list (`CMakeLists.txt:80-126`); append `results_test` and `results_screen_test`
  blocks mirroring `tests/CMakeLists.txt:206-214`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 7: `results_test` (pure model)

- **File**: `tests/results_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no SDL/GL; build a `ScoreState` by hand and a `blaze4k::Song`/`blaze4k::Chart`):
  1. **Snapshot copy** — `results_summary_from(state, failed, &song, &chart)` copies grade label,
     percent, `actual_dp`/`possible_dp`, `max_combo`, both count arrays, `failed`, and pointers;
     `valid == true`; null `state.grade` → empty label but valid.
  2. **Submit first** — empty `HighScores`; `results_submit_score` returns true; record stored under
     `make_chart_key(song, chart)` with grade/percent/dp/timestamp.
  3. **Submit lower / tie** — pre-seed a higher percent; a lower run returns false and does not
     change the stored record; equal percent also false.
  4. **Submit higher** — returns true and replaces the record (percent + DP + timestamp).
  5. **Failed / invalid no-op** — `failed=true` returns false and inserts nothing; `valid=false`
     returns false; null `song`/`chart` returns false; no crash.
  6. **Round-trip persistence** — after `results_submit_score`, `save_high_scores` then
     `load_high_scores` preserves the record (AC2 at the data layer).
- **Mirror**: `tests/score_keeper_test.cpp:600-635` (pure assertions), `tests/config_persistence_test.cpp`.
- **Validate**: `./build/tests/results_test` → 0.

### Task 8: `results_screen_test` (headless integration)

- **File**: `tests/results_screen_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; a `ScreenManager(0.0)` registering `ResultsScreen` + a `Select` spy,
  a `GameConfig`, `HighScores`, `ResultsSummary`, and a `ScreenContext`; `TEST_CHECK` idiom from
  `tests/calibration_screen_test.cpp:24-31`):
  1. **Enter + submit + flag** — pre-fill `ctx.results` with a cleared summary; `start(Results)`;
     `submitted()` true, `new_record()` true (first), `ctx.scores` contains the chart key.
  2. **Not a record** — pre-seed a higher stored percent; enter → `new_record()` false, table
     unchanged.
  3. **Failed run** — failed summary → `submitted()`/`new_record()` false, no entry inserted;
     `summary().failed` true.
  4. **Confirm → wheel** — a `Confirm` press transitions active to `Select` (AC3).
  5. **Back → wheel** — a `Back` press goes through `ScreenManager::handle_back` → `Select`;
     `manager.back_navigates()` is true while Results is active (AC3, no dead end).
  6. **Render / re-enter** — `render` with an uninitialized `GlQuadRenderer` is a no-op; re-enter
     after clearing `ctx.results` yields `valid()==false` and no crash (robustness).
  7. **End-to-end Gameplay → Results** — register `GameplayScreen` + `ResultsScreen` + `Select`;
     wire `PlayRequest` with a default `Song` and a one-tap `Chart`; start `Gameplay`; drive
     `manager.update(1/60, {})` until active becomes `Results`; assert the run reported to Results
     (`ctx.results->valid`, `gameplay->end_reported()`), the score was submitted, and a subsequent
     `Confirm` returns to `Select` (mirrors `tests/score_keeper_test.cpp:641-661` for the completed
     run).
- **Mirror**: `tests/calibration_screen_test.cpp:112-142,179-193`; `tests/select_screen_test.cpp:540-548`; `score_keeper_test.cpp:641-661`.
- **Validate**: `./build/tests/results_screen_test` → 0.

### Task 9: Extend `screen_manager_test`

- **File**: `tests/screen_manager_test.cpp`
- **Action**: UPDATE
- **Implement**: register a spy for `ScreenId::Results`, `start` it, press Back → active becomes
  `ScreenId::Select`; `manager.back_navigates()` is true on Results. Keep all existing cases green.
- **Mirror**: `tests/screen_manager_test.cpp:112-158`.
- **Validate**: `./build/tests/screen_manager_test` → 0.

### Task 10: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build and run everything; check for new warnings.
- **Validate**: see **Validation** below (`ctest` → **27/27**, no warnings).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 27/27: 25 existing + results_test + results_screen_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/results_test
./build/tests/results_screen_test
./build/tests/screen_manager_test
./build/tests/select_screen_test

# Purity: the results model/screen must stay free of SDL/GL/audio/<ctime>
rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time" src/screens/results.hpp src/screens/results.cpp
# expected: no matches

# Warning budget (no -Wswitch on ScreenId / new members)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless, non-blocking (no window/GL/audio device required), and use `--data-dir` so
the developer's real `data/` is untouched.

1. **Full arcade loop, no dead ends** (AC1/AC3/PRD §11): the `results_screen_test` case 7 drives a
   real `GameplayScreen` to completion and asserts the `Gameplay → Results` transition, then
   `Results → Select` on Confirm; case 5 asserts Back also returns to the wheel.
2. **NEW RECORD + persistent best** (AC2): `results_test` cases 2-6 + round-trip; at the binary
   level, a fresh data dir and a completed run leaves `scores.json` with a record after clean exit
   (the existing `main` save path):
   ```bash
   rm -rf /tmp/blaze4k-e2e-results && mkdir -p /tmp/blaze4k-e2e-results
   ./build/blaze-4k --headless --smoke-test 30 --start-screen select \
     --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-results
   # exit 0; /tmp/blaze4k-e2e-results/scores.json exists (possibly empty with no run)
   ```
3. **Failed-run clarity** (AC4): `results_screen_test` case 3 asserts the failed summary submits
   nothing and exposes `failed`; the render path draws the red `FAILED` banner (headless no-op).
4. **Regression**: `ctest --test-dir build --output-on-failure` → 27/27; `select_screen_test`,
   `life_keeper_test`, `score_keeper_test`, `judgment_engine_test`, `calibration_screen_test`,
   `input_remap_screen_test`, and the `--gameplay-demo` path stay green.
5. `git status` shows new files under `src/screens/` and `tests/`, edits limited to
   `src/screens/screen.hpp`, `gameplay_screen.*`, `screen_manager.*`, `src/main.cpp`, and the CMake
   files. No `src/gameplay/judgment*`, `src/timing/`, `src/chart/`, `src/audio/`, `src/render/`
   changes.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Results becomes a dead end (no return to wheel) | Confirm (and Back, via the manager set) transition to `Select`; `results_screen_test` cases 4/5/7 | **In scope** |
| A completed run is submitted twice (double NEW RECORD / duplicate work) | `GameplayScreen::end_reported_` publishes once; `enter` submits exactly once per results visit | **In scope** |
| Run strands on Gameplay when `Results` is not registered (tests/older wiring) | `transition_to(has_screen(Results) ? Results : Select)` fallback; existing `select_screen_test` runs stay green | **In scope** |
| Failed runs pollute the high-score table / flag NEW RECORD | `results_submit_score` no-ops on `failed`; `results_screen_test` case 3; **OQ2** | **In scope** (flag OQ2) |
| Changing Results back-nav flips `Escape` on Results from quit to wheel | Intentional; documented in `screen_manager.hpp` + Decisions; **OQ3** | **Flagged** |
| `ScoreState::grade` is null or grade label unknown | `results_summary_from` guards null → empty label; `format_grade` passes letters through; screen tolerates empty | **In scope** |
| Snapshot outlives the `ScoreState`/`Chart` it points into | Snapshot copies all scalar data; `song`/`chart` are `SongLibrary`-owned and outlive every screen (same contract as `PlayRequest`) | **In scope** |
| `std::time` wall clock in a screen violates the "music-driven clock" principle | Timestamp is high-score metadata only; it never enters the judgment/scoring path (B4/B5 untouched); `results.hpp` stays `<ctime>`-free | **In scope** (documented) |
| `-Wswitch` warnings from enum/member changes | No new enum members (Results already exists); warning-budget check in Validation | **In scope** |
| Immediate submit lost if the app is hard-killed before clean exit | Same clean-exit persistence model as C4/C5; **OQ4** | **Out of scope** — flagged |

---

## Decisions

- **Pure `ResultsSummary` snapshot + `PlayRequest`-style context handoff.** Gameplay owns the live
  view; Results owns presentation; the bridge is one `ScreenContext` pointer, keeping the screen
  headless-testable and honoring "everything derives from the event log" (PRD §6 pattern 2).
- **Submit in `ResultsScreen::enter`, flag from `submit_high_score`'s return value.** Reuses the
  existing best-score rule (first entry or strictly greater percent) instead of duplicating a
  comparison; no schema change to `scores.json`.
- **Failed runs report stats but never submit.** Matches arcade/OpenITG behavior and keeps the
  best-score table meaningful; surfaced as OQ2.
- **`Results` joins the manager's default back-navigation set.** Both Confirm and Back return to the
  wheel, satisfying the "no dead ends" criterion; updates the prior "Results does not consume Back"
  comment. Surfaced as OQ3.
- **No new numerical constants.** Grade thresholds/labels and percent formatting are reused from
  `judgment_constants.cpp` / `hud_renderer.cpp`; no Value Provenance additions needed beyond reuse.
- **Clean-exit persistence.** `main` already saves `scores.json` (`main.cpp:358-361`); the screen
  updates only the in-memory `HighScores`, exactly like C4/C5. Surfaced as OQ4.

---

## Open Questions

1. **Non-blocking — first-ever play = NEW RECORD?** `submit_high_score` returns true when no entry
   exists, so a chart's first clear would show `NEW RECORD`. Proposed default: **yes** (it is a
   personal best). Alternative: flag only when a prior record was beaten. Confirm.
2. **Blocking-ish — do failed runs submit a score?** Proposed default: **no** (stats shown, no
   `scores.json` entry, no NEW RECORD). Alternative: submit anyway (a failed run can still be a
   high percent). Confirm, as it changes AC2 behavior for failed songs.
3. **Non-blocking — Back on Results.** Proposed default: **Back returns to the wheel** (Results
   added to the back-nav set), so `Escape` on Results navigates instead of quitting. Alternative:
   keep the old contract (Results does not consume Back → App quits on Escape, only Confirm returns).
   Confirm.
4. **Non-blocking — save timing.** AC2 says "scores.json is updated", but the file write happens on
   clean exit (C4/C5 model). Proposed default: **in-memory submit + clean-exit save**. Say if an
   immediate save-on-results (requiring the scores path in `ScreenContext`) is required.
5. **Non-blocking — persisted fields.** `ScoreRecord` stores only `{grade, %, DP, timestamp}`; max
   combo and the judgment breakdown are shown but not persisted. Proposed default: **keep the
   schema unchanged** (display-only breakdown; D3 can extend later). Confirm.

---

## Acceptance Criteria

- [ ] Given a completed or failed song, when gameplay ends, Results shows grade, %, DP, per-window
      judgment counts, and max combo, all derived from the event log (Tasks 1/3/4; Task 8 cases 1/7)
- [ ] Given a new personal best for the chart, Results shows a NEW RECORD flag and the in-memory
      scores table is updated (Tasks 1/3; Task 7 cases 2/4/6; E2E 2)
- [ ] Given Results, when confirmed (and on Back), flow returns to the song wheel with no dead end
      (Tasks 2/3/5; Task 8 cases 4/5/7; E2E 1)
- [ ] Given a failed song, the failure is clearly indicated alongside the stats and no record is
      submitted (Tasks 1/3; Task 8 case 3; E2E 3)
- [ ] `ctest --test-dir build --output-on-failure` → **27/27**; `select_screen_test`/
      `screen_manager_test` and the `--gameplay-demo` path stay green (Tasks 8-10; E2E 4)
- [ ] `src/screens/results.*` stays SDL/GL/audio/`<ctime>`-free; zero new warnings under
      `-Wall -Wextra -Wpedantic` (Validation)
- [ ] Open Questions OQ1–OQ5 confirmed or defaults accepted (first-ever record, failed-run policy,
      Back behavior, save timing, persisted fields)
