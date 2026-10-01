# Plan: Judgment Engine with Event-Sourced Judgment Log

## Summary

Build Blaze 4k's judgment core as a **pure, event-sourced engine** (PRD §6 pattern 2 / AGENTS.md core
principles 1 & 2). Every hit is evaluated against the B2 `JudgmentConstants` timing windows and emitted
as an immutable `JudgmentEvent {column, note_time, hit_time, delta_ms, window}`; scoring, combo, life,
results, and future replays all derive from this one append-only log. The work splits into three pure
modules plus thin integration:

1. `src/gameplay/judgment.hpp` (NEW) — the event model: `JudgmentKind`, `JudgmentEvent`, and the
   hidden-note predicate. Pure value types; no clocks, no platform headers.
2. `src/gameplay/judgment_input.hpp` (NEW) — a pure, inline conversion from an SDL nanosecond event
   timestamp to a music-clock time, so input events can be aged against the `MusicClock` without any
   frame-delta or wall-clock math in the judgment path.
3. `src/gameplay/judgment_engine.{hpp,cpp}` (NEW) — the deterministic engine: closest-ungraded-note
   selection, tap/mine classification, miss expiry, hold/roll life, roll re-hits, and held-over-mine
   crossing. It is **time-parameterized** (callers pass absolute music time) exactly like `NoteField`,
   so it never reads a clock and is fully unit-testable.
4. `src/gameplay/gameplay_view.{hpp,cpp}` (UPDATE) — host the engine, feed it input events + held
   columns + `MusicClock` time, filter hidden notes from the render list, and expose the log/latest
   event for the HUD (D2 will draw judgment sprites from this).
5. `src/app/app.{hpp,cpp}`, `src/main.cpp` (UPDATE) — forward `InputManager` events and a monotonic
   nanosecond reference into gameplay; expose held columns.

**Scope boundary:** B4 produces the event log and hold/roll/mine judgment only. It does **not** compute
combo, life, DP, percent, or grades — those are B5 (`[B5]`) and B6, and they must consume this log
rather than re-judging (PRD §6 pattern 2). No judgment sprites are drawn here (D2); "visual feedback
matches the log" is enforced as (a) the exact hidden-note set and (b) an exposed `latest_event()` /
event stream for the future HUD.

All tap/step/hold/roll/mine semantics are pinned to **OpenITG** commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (see **Pinned Semantics** and **Value Provenance**).

## User Story

As the developer
I want every hit evaluated against the timing windows and emitted as an immutable timestamped judgment
event
So that scoring, combo, life, results — and future replays/stats — all derive from one event log.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | LARGE |
| Systems Affected | `src/gameplay/` (NEW `judgment*` files), `src/app/`, `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #12 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3 3.2.8, glad (GL 3.3 core), miniaudio 0.11.21, nlohmann_json 3.11.3, stb already fetched |
| Baseline tests | 11/11 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 11" (0.29s) |
| `src/gameplay/` | exists | B3 (`note_field`, `speed_mod`, `noteskin`, `note_field_renderer`, `gameplay_view`) present; add new `judgment*` sources |
| `JudgmentConstants` | `src/timing/judgment_constants.*` | Reuse **as-is**: `TapJudgment`, `HoldJudgment`, `TimingWindows`, `classify_tap`, `continues_combo`; never re-define windows |
| Input timestamps | `src/input/input_event.hpp:51` | `InputEvent.timestamp_ns` is SDL3's nanosecond event timestamp, same timebase as `SDL_GetTicksNS()` |
| Input delivery | `src/app/app.cpp:93-114` | `App::process_events()` drains SDL into `InputManager`'s queue once per rendered frame; gameplay has no access yet — B4 must forward events |
| `MusicClock` | `src/timing/music_clock.hpp` | `time_seconds()` = PCM frames/rate + offset; the **only** gameplay time source |
| Upstream source | `/tmp/opencode/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Cloned for provenance |

**Start green, stay green:** 11 tests pass; this plan adds 1 test target (`judgment_engine_test`) → 12 expected.

---

## Pinned Semantics

**Authority for B4 is OpenITG** (PRD §15). Blaze 4k re-expresses OpenITG's step/update logic against its
own `Note`/`MusicClock` domains: OpenITG stores notes by integer row and works in beats; Blaze 4k has
absolute `note.time_seconds`, so the same behavior is expressed in seconds (documented where the
re-expression is not literally identical).

### Time and delta sign

- OpenITG ages an input with a monotonic timer: `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep`
  (`src/Player.cpp:908-919`). Blaze 4k mirrors this in `music_time_for_event` (input→music time), then the
  engine only ever sees a music time.
- `fNoteOffset = note_time - hit_time`; stored `tn.result.fTapNoteOffset = -fNoteOffset = hit_time -
  note_time` — **negative means early, positive means late** (`src/Player.cpp:919,1105-1106`;
  `src/NoteTypes.h:14-16`). Blaze 4k's event uses this stored convention: `delta_ms = (hit_time -
  note_time) * 1000`.

### Note selection on a step (`GetClosestNote`)

- Search radius `StepSearchDistance = 1.0 s` (`src/Player.cpp:27`); the closest **ungraded** note in the
  column within ±1.0 s is selected, searching forward and backward and preferring the nearer row
  (`src/Player.cpp:791-823`, called `src/Player.cpp:894-895`). Mines, taps, hold heads and roll heads
  all participate.
- If no note is within range → `TNS_NONE`, **no event**.

### Tap classification (`HandleStep`)

- `|delta| <= marvelous` → Fantastic; `<= perfect` → Excellent; `<= great` → Great; `<= good` →
  Decent; `<= boo` → Way Off; otherwise `TNS_NONE` — **a step too far produces no judgment at all, not
  a Miss** (`src/Player.cpp:938-947`). Miss events come only from expiry (below).
- Mine: if the selected note is a mine and `|delta| <= mine window (0.070)` → `TNS_HIT_MINE`;
  otherwise `TNS_NONE` (`src/Player.cpp:930-934`). Mines use their own window, not `classify_tap`.
- Hold/roll heads are graded exactly like taps (`src/Player.cpp:938-947,1117-1122`).

### Miss / avoided-mine expiry

- `UpdateTapNotesMissedOlderThan(GetMaxStepDistanceSeconds())`; `GetMaxStepDistanceSeconds() =
  music_rate * TW_Boo = 0.180 s` (`src/Player.cpp:440,1666-1669`).
- Untouched **tap / hold-head / roll-head** → `TNS_MISS`; untouched **mine** → `TNS_AVOIDED_MINE`
  (no penalty) (`src/Player.cpp:1403-1435`). A missed hold head produces **no** hold OK/NG.
- OpenITG detects expiry per row (one row miss for a jump) and works in beats to honor freezes. Blaze 4k
  emits **one event per note** and compares seconds directly (`music_time > note.time_seconds +
  way_off`), which is equivalent for the absolute-time note model. Row aggregation is a B5 concern
  (Open Question 3).

### Holds (`src/Player.cpp:506-610`)

- Head result `tns`; `bSteppedOnTapNote = tns != TNS_NONE && tns != TNS_MISS` (Decent/Way Off still
  count as stepped-on) (`src/Player.cpp:519`).
- `fLife` starts at 1.0. While the song row is within `[headRow, endRow]`:
  - hold: if stepped-on **and** button held → `fLife = 1`; else `fLife -= dt / HW_OK(0.320)`, clamped
    at 0 (`src/Player.cpp:556-566`).
  - roll: `fLife -= dt / HW_Roll(0.350)`, clamped at 0 (`src/Player.cpp:573-580`).
- `HNS_NG` when `bSteppedOnTapNote && fLife == 0`; `HNS_OK` when `musicRow >= endRow &&
  bSteppedOnTapNote && fLife > 0` (`src/Player.cpp:585-599`). A hold whose head was missed is **never**
  OK or NG.
- On judgment, `HandleHoldScore(hns, tns)` is called once (`src/Player.cpp:599-610`).
- **Blaze 4k re-expression:** OpenITG decrements `fLife` by the per-frame `dt`. Blaze 4k computes life
  **analytically** from music time — `life = clamp(1 - (music_time - last_satisfied_time) / window,
  0, 1)`, where `last_satisfied_time` is the last music time the hold was held (hold) or re-hit
  (roll). This is frame-rate independent and uses only the music clock (AGENTS.md core principle 1).

### Rolls (`src/Player.cpp:1190-1225`)

- A roll is kept alive by **re-hits**, not by holding. Each button-down in the roll's column while the
  roll is active and unjudged sets `fLife = 1` and fires a visual hold flash (`DidHoldNote(HNS_OK)`)
  (`src/Player.cpp:1211-1218`). The final OK/NG uses the same rule as holds with `HW_Roll = 0.350`.

### Mines held over (`CrossedMineRow`)

- Holding a panel while a mine row is crossed triggers the mine (`Step(col, now, bHeld=true)`)
  (`src/Player.cpp:643,1461-1483`). With `PadStickSeconds = 0` the check is `IsButtonDown`.
- A triggered mine applies its penalty via `HandleTapScore(TNS_HIT_MINE)` and **never touches combo**
  (`src/ScoreKeeperMAX2.cpp:333-341`).

### Visual state (hidden notes)

- `DisplayJudgedRow` hides a note only when `score >= TNS_GREAT` (or blind); mines are hidden when hit
  (`src/Player.cpp:1284-1302,1075-1078`; `src/NoteField.cpp:721-724`). Decent/Way Off/Miss notes are
  **not** hidden — they scroll off. Blaze 4k mirrors this exact hidden set; B4 never invents hide rules.

---

## Value Provenance

All behavioral values/semantics below are transcribed from the cloned OpenITG repository at commit
**`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`**.

| Value / Semantic | Value | Upstream source (file:line) |
|------------------|-------|-----------------------------|
| Step search radius | `1.0 s` | `src/Player.cpp:27` |
| Closest ungraded note selection | nearest row in ±1.0 s | `src/Player.cpp:791-823,894-895` |
| Tap window ladder | Marvelous→Perfect→Great→Good→Boo→NONE | `src/Player.cpp:938-947` |
| Mine hit window | `JudgeWindowSecondsMine = 0.070` | `src/Player.cpp:930-934`; `metrics.ini:93` |
| Delta sign | stored offset = hit−note (negative=early) | `src/Player.cpp:1105-1106`; `src/NoteTypes.h:14-16` |
| Miss threshold | `TW_Boo = 0.180 s` | `src/Player.cpp:440,1666-1669`; `metrics.ini:94` |
| Untouched tap → Miss | `TNS_MISS` | `src/Player.cpp:1420-1426` |
| Untouched mine → avoided | `TNS_AVOIDED_MINE`, weight 0 | `src/Player.cpp:1417-1419`; `src/ScoreKeeperMAX2.cpp:560` |
| Hold OK window | `JudgeWindowSecondsOK = 0.320` | `src/Player.cpp:56-58`; `metrics.ini:99` |
| Roll OK window | `JudgeWindowSecondsRoll = 0.350` | `src/Player.cpp:59-61`; `src/PrefsManager.cpp:94` |
| Hold life rule | held→1, else −dt/HW_OK | `src/Player.cpp:556-566` |
| Roll life rule | −dt/HW_Roll; re-hit→1 | `src/Player.cpp:573-580,1211-1218` |
| Hold OK/NG conditions | end & head-hit & life>0 / head-hit & life==0 | `src/Player.cpp:585-599` |
| Missed hold head → no OK/NG | `bSteppedOnTapNote` gate | `src/Player.cpp:519,585,588` |
| Hold outcome scoring hook | `HandleHoldScore(HNS_OK/NG)` | `src/Player.cpp:599-610`; `src/ScoreKeeperMAX2.cpp:414-436` |
| Held-over-mine trigger | `CrossedMineRow` → `Step(bHeld=true)` | `src/Player.cpp:643,1461-1483` |
| Mine does not affect combo | `HandleTapScore` only | `src/ScoreKeeperMAX2.cpp:333-341` |
| Hidden-note rule | hide when `score >= TNS_GREAT` | `src/Player.cpp:1284-1302`; `src/NoteField.cpp:721-724` |
| Combo counts notes, score per row | `ComboIsPerRow=false` | `metrics.ini:18-20` |

No value in this plan is invented; the analytic hold-life re-expression and per-note miss events are
explicit Blaze 4k decisions (see **Decisions** / **Open Questions**).

---

## Patterns to Follow

### Time-parameterized pure module (no clock reads)
```cpp
// SOURCE: src/gameplay/note_field.hpp:35-59
// "This module is time-parameterized: callers pass an absolute music time (from
//  `MusicClock`) rather than letting the field read a clock..."
[[nodiscard]] double offset_for_note(const Note& note, double music_time_seconds) const;
```
`JudgmentEngine::handle_step(col, music_time_seconds)` and `update(music_time_seconds, held)` follow
this: the engine never includes `<chrono>`, SDL, or GL.

### Reusing B2 constants (never re-define windows)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:82-84
[[nodiscard]] TapJudgment classify_tap(double delta_seconds) const;
[[nodiscard]] bool continues_combo(TapJudgment j) const;
```
B4 calls `classify_tap` for taps and reads `windows.hit_mine`/`hold_ok`/`hold_roll` directly.

### Note model
```cpp
// SOURCE: src/chart/note.hpp:25-36
struct Note {
    int column = 0;            // 0=Left, 1=Down, 2=Up, 3=Right
    double beat = 0.0;
    double time_seconds = 0.0;
    NoteType type = NoteType::Tap;   // Tap, HoldHead, RollHead, Mine
    double hold_length_beats = 0.0;
    double hold_end_time_seconds = 0.0;
};
```

### Logging / error handling
```cpp
// SOURCE: src/audio/sound_stream.cpp:63-67
std::cerr << "[SoundStream] Failed to load audio file '" << filepath << "' ...\n";
```
Tagged `std::cerr` lines prefixed `[JudgmentEngine]`; never throw.

### Tests
```cpp
// SOURCE: tests/note_field_test.cpp:12-18
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
```
Plain `int main()` binaries, `TEST_CHECK`, synthetic `Chart` built with the local `make_note` helper
(`tests/note_field_test.cpp:26-36`), deterministic and hardware-independent.

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:96-101 / tests/CMakeLists.txt:103-111
add_library(blaze4k_core STATIC ... src/gameplay/note_field.cpp ...)
add_executable(note_field_test note_field_test.cpp)
target_link_libraries(note_field_test PRIVATE blaze4k_core)
add_test(NAME note_field_test COMMAND note_field_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/judgment.hpp` | CREATE | `JudgmentKind`, `JudgmentEvent`, `is_tap_window_hidden` (pure event model) |
| `src/gameplay/judgment_input.hpp` | CREATE | Inline `music_time_for_event(...)` (pure SDL-ns → music-time conversion) |
| `src/gameplay/judgment_engine.hpp` | CREATE | `JudgmentEngine` interface + per-note state |
| `src/gameplay/judgment_engine.cpp` | CREATE | Step path (tap/mine/closest-note) + update path (expiry, holds, rolls, mine crossing) |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Own engine; add `constants` to `init`; input/held hooks; expose log + hidden state |
| `src/gameplay/gameplay_view.cpp` | UPDATE | Feed engine, filter hidden notes before render, sample reference music time |
| `src/app/app.hpp` | UPDATE | Expose `input_reference_ns()` (SDL `GetTicksNS` captured at poll) |
| `src/app/app.cpp` | UPDATE | Capture the reference ns at the end of `process_events()` |
| `src/main.cpp` | UPDATE | Poll `InputManager`, forward events + reference ns, pass held columns to gameplay |
| `CMakeLists.txt` | UPDATE | Add `src/gameplay/judgment_engine.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `judgment_engine_test` |
| `tests/judgment_engine_test.cpp` | CREATE | Tap/mine/hold/roll/miss/visual/input-conversion tests |

---

## Tasks

Execute in order. Each task is atomic and verifiable. No scoring/combo/life/DP code.

### Task 1: Event model

- **File**: `src/gameplay/judgment.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {

  enum class JudgmentKind {
      Tap,          // tap / hold-head / roll-head graded against a tap window
      Miss,         // untouched tap / hold-head / roll-head expired
      HitMine,      // mine triggered (stepped on, or held over)
      AvoidedMine,  // mine passed untouched (no penalty; stats only)
      HoldOk, HoldNg, RollOk, RollNg,
      RollHit,      // roll re-hit refresh (visual only; no score/combo)
  };

  struct JudgmentEvent {
      JudgmentKind kind = JudgmentKind::Tap;
      int column = 0;
      double note_time_seconds = 0.0;
      double hit_time_seconds = 0.0;          // music time of input / expiry
      double delta_ms = 0.0;                  // (hit - note)*1000; negative = early
      TapJudgment window = TapJudgment::Num;  // tap classification; Num for hold outcomes
      HoldJudgment hold = HoldJudgment::Num;  // hold/roll outcome; Num otherwise
      NoteType note_type = NoteType::Tap;
      int note_index = -1;                    // index into Chart::notes
  };

  // OpenITG hides a judged note only when score >= TNS_GREAT (Player.cpp:1284-1302).
  [[nodiscard]] constexpr bool is_tap_window_hidden(TapJudgment j) {
      return j == TapJudgment::Fantastic || j == TapJudgment::Excellent || j == TapJudgment::Great;
  }

  } // namespace blaze4k
  ```
  - Includes `<cstdint>`, `"chart/note.hpp"`, `"timing/judgment_constants.hpp"` only.
- **Mirror**: `src/chart/note.hpp:1-36` (small pure value types), `src/timing/judgment_constants.hpp:21-23`.
- **Validate**: `cmake --build build -j16` once included by Task 3.

### Task 2: Input timestamp → music time conversion

- **File**: `src/gameplay/judgment_input.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  // Reconstructs the music-clock time at which an SDL-timestamped input occurred,
  // given the music time and SDL nanosecond reference sampled together. Mirrors
  // OpenITG's `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep`
  // (src/Player.cpp:908-919) but is a pure function: it reads no clock.
  [[nodiscard]] inline double music_time_for_event(
      uint64_t event_timestamp_ns, uint64_t reference_ns, double reference_music_seconds) {
      if (event_timestamp_ns == 0 || event_timestamp_ns >= reference_ns) {
          return reference_music_seconds;   // unset / future event: no age
      }
      const double age = static_cast<double>(reference_ns - event_timestamp_ns) / 1e9;
      return reference_music_seconds - age;
  }
  } // namespace blaze4k
  ```
  - Header-only, includes `<cstdint>` only. Pure; no SDL/`<chrono>`.
- **Mirror**: `src/gameplay/speed_mod.hpp` (pure header helpers).
- **Validate**: `cmake --build build -j16` once included by Task 3.

### Task 3: Engine interface

- **File**: `src/gameplay/judgment_engine.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {

  class JudgmentEngine {
  public:
      // `chart` and `constants` must outlive the engine. Builds per-column note
      // index lists and resets all note/judgment state.
      void reset(const Chart* chart, const JudgmentConstants* constants);

      // Button-down in `column` at `music_time_seconds`. Mirrors Player::HandleStep:
      // closest-note tap/mine grading + roll re-hit refresh.
      void handle_step(int column, double music_time_seconds);

      // Per-frame: miss/avoided-mine expiry, hold/roll life, held-over-mine crossing.
      // Mirrors Player::Update. Uses only `music_time_seconds` (never frame delta).
      void update(double music_time_seconds, const std::array<bool, 4>& held_columns);

      [[nodiscard]] const std::vector<JudgmentEvent>& events() const { return events_; }
      void drain_new_events(std::vector<JudgmentEvent>& out); // appends events since last drain

      [[nodiscard]] bool is_note_judged(int note_index) const;
      [[nodiscard]] bool is_note_hidden(int note_index) const;
      [[nodiscard]] const JudgmentEvent* latest_event() const;

  private:
      struct NoteState {
          TapJudgment tap = TapJudgment::Num;     // Num = ungraded
          HoldJudgment hold = HoldJudgment::Num;
          double hold_satisfied_time = 0.0;       // last music time life was full
          bool hold_head_hit = false;             // bSteppedOnTapNote
          bool complete = false;                  // no further judgment possible
      };

      void emit(const JudgmentEvent& event);
      void handle_step_tap(int note_index, double delta_seconds, double hit_time);
      void handle_step_mine(int note_index, double delta_seconds, double hit_time);
      void refresh_active_rolls(int column, double music_time);
      void expire_notes(double music_time);
      void update_holds(double music_time, const std::array<bool, 4>& held_columns);
      void cross_mines(double music_time, const std::array<bool, 4>& held_columns);

      const Chart* chart_ = nullptr;
      const JudgmentConstants* constants_ = nullptr;
      std::vector<NoteState> states_;
      std::vector<std::vector<int>> column_notes_;   // note indices per column (time order)
      std::vector<std::vector<int>> column_mines_;   // mine indices per column (time order)
      std::vector<int> active_holds_;                // hold/roll indices not yet complete
      std::vector<JudgmentEvent> events_;
      std::size_t new_event_begin_ = 0;
      bool has_last_update_ = false;
      double last_update_time_ = 0.0;
  };

  } // namespace blaze4k
  ```
  - Includes `<array>`, `<vector>`, `<cstddef>`, `"chart/chart.hpp"`, `"gameplay/judgment.hpp"`,
    `"timing/judgment_constants.hpp"`. **No** SDL/GL/`<chrono>`.
- **Mirror**: `src/gameplay/note_field.hpp:41-72` (time-parameterized pure class).
- **Validate**: `cmake --build build -j16`.

### Task 4: Engine — reset + step path

- **File**: `src/gameplay/judgment_engine.cpp`
- **Action**: CREATE
- **Implement**:
  - `reset`: store pointers, size `states_` to `chart_->notes.size()`, build `column_notes_`/
    `column_mines_` (chart notes are beat-sorted → already time-sorted), clear `active_holds_` (populate
    with every `HoldHead`/`RollHead` index), clear `events_`, `new_event_begin_=0`,
    `has_last_update_=false`. Guard null chart (no-op).
  - `handle_step(col, t)`:
    1. Guard column 0-3, null chart.
    2. `refresh_active_rolls(col, t)` (Task 4b) — refreshes an in-progress roll regardless of tap
       search, mirroring `Player.cpp:1190-1225`.
    3. Closest-ungraded-note selection over `column_notes_[col]` within ±`StepSearchDistance = 1.0 s`,
       preferring the nearer note (tie → later, matching `Player.cpp:818-821`). Skip notes whose
       `states_[i].tap != Num` (graded).
    4. If none → return (no event; OpenITG `TNS_NONE`).
    5. `delta = note.time_seconds - hit_time`; `delta_seconds = |delta|`.
    6. Mine → `handle_step_mine`; tap/hold-head/roll-head → `handle_step_tap`.
  - `handle_step_tap(i, |delta|, hit_time)`:
    - `TapJudgment j = constants_->classify_tap(delta_seconds);`
    - If `j == TapJudgment::Miss` → return (**no event**; OpenITG step-too-far is `TNS_NONE`).
    - `states_[i].tap = j; states_[i].complete = true;` for a plain tap; for a hold/roll head set
      `hold_head_hit = true`, `hold_satisfied_time = hit_time`, and leave the hold outcome pending.
    - Emit `{kind=Tap, column, note_time, hit_time, delta_ms=(hit_time-note_time)*1000, window=j,
      note_type, note_index=i}`.
  - `handle_step_mine(i, |delta|, hit_time)`:
    - If `|delta| <= constants_->windows.hit_mine`: mark `tap = HitMine`, `complete = true`, emit
      `{kind=HitMine, window=HitMine, ...}`.
    - Else: return (no event).
  - `refresh_active_rolls(col, t)`: for each active roll in `col` with `hold_head_hit` and
    `t` within `[note.time_seconds, hold_end_time_seconds]`: set `hold_satisfied_time = t` and emit
    `{kind=RollHit, window=Num, hold=Num, ...}` (visual refresh; no score/combo).
- **Mirror**: `src/Player.cpp:832-1160` (HandleStep), `src/chart/timing_data.cpp` (time math).
- **Validate**: `cmake --build build -j16`.

### Task 5: Engine — update path (expiry, holds, rolls, mines)

- **File**: `src/gameplay/judgment_engine.cpp`
- **Action**: CREATE (same file as Task 4)
- **Implement**:
  - `update(t, held)`:
    1. Guard null chart; if `has_last_update_ && t < last_update_time_` (backward jump/seek) → set
       `last_update_time_ = t` and return without emitting (documented; no seeking in v1).
    2. `expire_notes(t)`.
    3. `update_holds(t, held)`.
    4. `cross_mines(t, held)`.
    5. `last_update_time_ = t; has_last_update_ = true;`
  - `expire_notes(t)`: threshold `t - constants_->windows.way_off`. For each column, scan
    `column_notes_` from a monotonic position while `note.time_seconds < threshold`:
    - Skip if `states_[i].complete` or `tap != Num`.
    - Mine → `kind=AvoidedMine`, `complete=true`, emit (no penalty).
    - Tap/HoldHead/RollHead → `tap = Miss`, `complete = true`, `hold = Num` (missed head → no hold
      outcome), emit `{kind=Miss, window=Miss, hit_time = note.time_seconds + way_off,
      delta_ms = way_off*1000}` (deterministic expiry time; see Decisions).
  - `update_holds(t, held)`: for each active hold/roll `i` with `hold_head_hit`:
    - `end = note.hold_end_time_seconds`; if `t < note.time_seconds` continue.
    - **Hold** (`NoteType::HoldHead`): if `t <= end` and `held[note.column]` →
      `hold_satisfied_time = t`. `life = (held[column] && t <= end) ? 1.0 :
      clamp(1 - (t - hold_satisfied_time)/windows.hold_ok, 0, 1)`.
    - **Roll** (`NoteType::RollHead`): `life = clamp(1 - (t - hold_satisfied_time)/windows.hold_roll,
      0, 1)` (re-hits in `handle_step` reset `hold_satisfied_time`).
    - If `life <= 0` → `hold = Ng`, `complete = true`, remove from active, emit
      `{kind = HoldNg/RollNg, hold=Ng, hit_time=t, delta_ms=0, note_time, note_index=i}`.
    - Else if `t >= end` → `hold = Ok`, `complete = true`, remove from active, emit
      `{kind = HoldOk/RollOk, hold=Ok, ...}`.
    - Else keep active.
  - `cross_mines(t, held)`: for each column, scan `column_mines_` from a monotonic position while
    `mine.time_seconds <= t`: if ungraded and `held[column]` → `tap=HitMine`, `complete=true`, emit
    `{kind=HitMine, window=HitMine, hit_time=t, delta_ms=(t-mine.time)*1000}`. (Untouched mines are
    left for `expire_notes` → AvoidedMine.)
  - `is_note_judged(i)`: `states_[i].complete`.
  - `is_note_hidden(i)`: mine with `tap == HitMine`, or `is_tap_window_hidden(states_[i].tap)`.
  - `latest_event()`: last element or `nullptr`.
  - `drain_new_events(out)`: append `events_[new_event_begin_..]`, then set
    `new_event_begin_ = events_.size()`.
- **Mirror**: `src/Player.cpp:369-660,1190-1225,1368-1483` (Update/roll/mine/miss logic).
- **Validate**: `cmake --build build -j16` with zero warnings under `-Wall -Wextra -Wpedantic`.

### Task 6: GameplayView integration

- **Files**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: `#include "gameplay/judgment_engine.hpp"` and `#include "input/input_event.hpp"`; add
    `bool init(const Chart& chart, const JudgmentConstants& constants, const std::string& audio_path,
    const GameplayOptions& options);` (new `constants` param — update the one call site in `main.cpp`);
    add
    ```cpp
    void handle_input_events(const std::vector<InputEvent>& events, uint64_t reference_ns);
    void update(double fixed_dt, const std::array<bool, 4>& held_columns); // replaces update(fixed_dt)
    [[nodiscard]] const std::vector<JudgmentEvent>& judgment_events() const;
    [[nodiscard]] const JudgmentEvent* latest_judgment() const;
    [[nodiscard]] bool is_note_hidden(int note_index) const;
    ```
    and members `JudgmentEngine judge_; std::vector<NoteRenderItem> visible_items_;`.
  - `init`: call `judge_.reset(&chart_, &constants);`.
  - `handle_input_events`: sample `const double ref_music = clock_.time_seconds();` once, then for each
    `pressed` event whose action is `Left/Down/Up/Right`, map the action to column 0-3 and call
    `judge_.handle_step(column, music_time_for_event(ev.timestamp_ns, reference_ns, ref_music));`.
    Ignore menu actions and releases.
  - `update(fixed_dt, held)`: keep the existing stub-clock advance; then
    `judge_.update(clock_.time_seconds(), held);`. (When using the audio source, `update` still calls
    the judge; no frame time enters the judge.)
  - `render`: after `field_.compute_visible(...)`, copy non-hidden items into `visible_items_`
    (`judge_.is_note_hidden(static_cast<int>(item.note - chart_.notes.data()))`), and render
    `visible_items_`. This is the "visual feedback matches the log" enforcement (OpenITG hide rule).
  - Keep all existing logs; add one `[GameplayView]` line reporting the event count on shutdown.
- **Mirror**: `src/gameplay/gameplay_view.cpp:89-133` (update/render), `src/gameplay/note_field.cpp:64-104`.
- **Validate**: `cmake --build build -j16`.

### Task 7: App + main wiring for input and time reference

- **Files**: `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `app.hpp`: add `[[nodiscard]] uint64_t input_reference_ns() const { return input_reference_ns_; }`
    and member `uint64_t input_reference_ns_ = 0;`.
  - `app.cpp`: at the end of `process_events()`, set `input_reference_ns_ = SDL_GetTicksNS();` (same
    timebase as `InputEvent::timestamp_ns`).
  - `main.cpp`: in the update callback, poll once and forward:
    ```cpp
    app.set_update_callback([&](double fixed_dt) {
        auto events = app.input_manager().poll_events();
        gameplay.handle_input_events(events, app.input_reference_ns());
        const std::array<bool,4> held = {
            app.input_manager().is_action_down(blaze4k::GameAction::Left),
            app.input_manager().is_action_down(blaze4k::GameAction::Down),
            app.input_manager().is_action_down(blaze4k::GameAction::Up),
            app.input_manager().is_action_down(blaze4k::GameAction::Right),
        };
        gameplay.update(fixed_dt, held);
    });
    ```
    and pass `app.judgment_constants()` as the new `init` argument. Add `#include <array>` and
    `#include "timing/judgment_constants.hpp"` as needed.
  - `App::process_events` still early-exits on `SDL_EVENT_KEY_DOWN` Escape; unchanged.
- **Mirror**: `src/app/app.cpp:93-114` (event drain), `src/main.cpp:125-136` (callback wiring).
- **Validate**: `cmake --build build -j16`.

### Task 8: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root: add `src/gameplay/judgment_engine.cpp` to `blaze4k_core` after `src/gameplay/note_field.cpp`.
  - Tests: append a `judgment_engine_test` block mirroring `note_field_test`
    (`tests/CMakeLists.txt:103-111`).
- **Mirror**: `CMakeLists.txt:96-101`, `tests/CMakeLists.txt:103-111`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 9: Test suite

- **File**: `tests/judgment_engine_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; synthetic `Chart` via the local `make_note` helper copied from
  `tests/note_field_test.cpp:26-36`; `JudgmentConstants::compiled_defaults()`; deterministic):
  1. **Tap classification + event fields**: a tap at `note_time=2.0`; steps at `note_time`,
     `+fantastic+ε`, `+excellent+ε`, `+great+ε`, `+decent+ε`, `+way_off+ε` yield
     Fantastic/Excellent/Great/Decent/WayOff; `+way_off+2ε` yields **no event** (OpenITG `TNS_NONE`).
     Assert `column`, `note_time_seconds`, `hit_time_seconds`, `delta_ms`, `window`, `note_type`,
     `note_index` exactly.
  2. **Delta sign**: early hit (`hit < note`) → `delta_ms < 0`; late → `delta_ms > 0`.
  3. **Miss expiry**: untouched tap expires once `t > note + way_off`; exactly at threshold → not yet;
     a hit before expiry produces no Miss. Miss event's `window == Miss` and `kind == Miss`.
  4. **Mine**: press within `hit_mine` → `HitMine`; press just outside → no event; untouched mine
     expires → `AvoidedMine` (never `Miss`).
  5. **Hold OK**: head hit (Great) + held through `hold_end_time` → events = `[Tap(Great), HoldOk]`;
     assert hold event `hold == Ok`.
  6. **Hold NG**: head hit, then released so cumulative not-held time exceeds `hold_ok` → `HoldNg`.
  7. **Hold with missed head**: head expires (Miss) → only a `Miss` event; **no** `HoldOk`/`HoldNg`.
  8. **Roll re-hit**: head hit; `handle_step` twice mid-roll → `RollHit` events; end → `RollOk`;
     a roll left for > `hold_roll` → `RollNg`.
  9. **Closest-note selection**: two notes in one column 0.5 s apart; hit the nearer; a second press
     grades the other; the graded note is never re-judged.
  10. **Held-over-mine**: `update` crossing a mine time with `held[col]=true` → `HitMine` (even though
      the button was pressed earlier); with `held=false` → `AvoidedMine` at expiry.
  11. **Frame-rate independence**: hold life with one `update(dt=0.1)` equals ten `update(dt=0.01)`
      calls (analytic life; proves no frame-delta dependence).
  12. **Log append-only + drain**: `events()` grows monotonically; `drain_new_events` returns only the
      new suffix and a second drain is empty.
  13. **Visual/log consistency**: `is_note_hidden` true after Great/Excellent/Fantastic and after a mine
      hit; false after Decent/WayOff/Miss; every hidden note has a corresponding event.
  14. **`music_time_for_event`**: reference/event equal → reference music time; event 10 ms older →
      −0.010 s; event in the future or `timestamp_ns==0` → reference music time.
  15. **Empty/null chart**: `reset(nullptr, ...)` and updates are safe no-ops.
- **Mirror**: `tests/note_field_test.cpp:1-120`, `tests/judgment_constants_test.cpp` (constants use).
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 12/12).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 12/12: 11 existing + judgment_engine_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/judgment_engine_test

# Headless harness smoke (stub clock; fixture has taps/holds/rolls/mines)
./build/blaze-4k --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --smoke-test 120

# Purity check: judgment core must not touch platform/time/GL headers
rg -n "SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter|GetTicksNS" \
  src/gameplay/judgment.hpp src/gameplay/judgment_input.hpp \
  src/gameplay/judgment_engine.hpp src/gameplay/judgment_engine.cpp
# (only the pure `uint64_t` parameters in judgment_input.hpp; no includes match)

# Confirm no frame-delta/wall-clock enters the judgment path
rg -n "fixed_dt|delta_time|frame" src/gameplay/judgment_engine.cpp
# (no matches)
```

## End-to-End Verification

1. `./build/tests/judgment_engine_test` prints each sub-check; tests 1/2/3 prove tap classification,
   sign convention, and Miss expiry against the B2 windows; tests 5-8 prove hold/roll/mine behavior
   matches the pinned OpenITG semantics; test 11 proves frame-rate independence.
2. `ctest --test-dir build --output-on-failure` → **12/12**; all 11 prior tests stay green (changes are
   additive registrations plus a `GameplayView` signature update).
3. `--headless --gameplay-demo ... --smoke-test 120` exits cleanly with the stub clock; confirm the
   `[GameplayView]` line reports the event count and that no GL calls are attempted headless.
4. On a display: run `--gameplay-demo` on the fixture, press the arrows on the beat, and confirm notes
   judged Great-or-better (and hit mines) disappear exactly as events are emitted, while Decent/Way
   Off/Miss notes scroll past (matching OpenITG's hidden set). Holding a panel across a mine triggers
   `HitMine`; releasing a hold too long yields `HoldNg`.
5. Enforce module purity with the `rg` commands in **Validation** → the judgment core contains no
   SDL/GL/`<chrono>` and no frame-delta math; the only wall-clock read (`SDL_GetTicksNS`) lives in
   `App` and is used solely to age input events, never as a gameplay clock.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Row-vs-note semantics: OpenITG scores/misses per **row** (`ComboIsPerRow=false`, `HandleTapRowScore` once per row) but B4 emits per-note events | Emit per-note events with `note_time`/`column`; B5 aggregates same-row events into one row score/miss. Documented and flagged (Open Question 3) | **In scope** — B4 per-note; B5 aggregation |
| Hold life uses per-frame `dt` upstream; frame delta is banned | Analytic life from music time (`1 - (t - last_satisfied)/window`); test 11 pins frame-rate independence | **In scope** |
| OpenITG ages inputs with a monotonic timer; SDL event timestamps are not the audio clock | Convert at the input boundary via `music_time_for_event` using a reference ns captured at poll; engine stays pure | **In scope** |
| `SDL_GetTicksNS()` reference sampled a few µs after `SDL_PollEvent` | Capture once at the end of `App::process_events()`; error is far below a judgment window; documented | **In scope** |
| Held-over-mine uses row crossing upstream; Blaze 4k uses a time window | Cross when `mine.time <= t` with the column held; equivalent for absolute-time notes; test 10 covers it | **In scope** — documented |
| A step beyond Way Off yields `TNS_NONE`, not Miss — easy to mis-implement | Explicit guard in `handle_step_tap`; test 1 asserts **no event**; Miss only from expiry | **In scope** |
| Missed hold head must never produce OK/NG | `hold_head_hit` gate; test 7 | **In scope** |
| Backward music-time jump (seek) corrupts expiry/hold state | Guard: if `t < last_update_time_`, resync without emitting; no seeking in v1 | **In scope** — documented |
| Full per-frame scans on dense charts | Per-column note/mine lists + monotonic cursors for expiry/crossing; active-hold list is small. Further optimization deferred | **Out of scope** — documented |
| B4 scope creep into B5/B6 (combo/life/DP) | Event log only; `latest_event()`/`events()` are the handoff; no score/life state in the engine | **In scope** — boundary |
| `GameplayView::update` signature change ripples | Single call site (`main.cpp`); full suite re-run after the change | **In scope** |

---

## Decisions

- **Event model in its own pure header** (`judgment.hpp`) so B5/B6/replays depend only on the log type,
  not on the engine.
- **`delta_ms = (hit_time - note_time) * 1000`** with **negative = early**, matching OpenITG's stored
  `fTapNoteOffset` (`Player.cpp:1105-1106`, `NoteTypes.h:14-16`).
- **Miss events are deterministic**: emitted with `hit_time = note_time + way_off` and
  `delta_ms = way_off*1000`, rather than the frame-dependent detection time OpenITG records — the
  event log must be reproducible.
- **Per-note events; row aggregation deferred to B5** (see Open Question 3).
- **Hold/roll life is analytic** from music time, not accumulated frame deltas (AGENTS.md principle 1).
- **`AvoidedMine` and `RollHit` are logged** so the log is the single source for stats and visual
  feedback; both carry no score/combo effect.
- **Hidden-note rule is OpenITG-exact** (`score >= Great`, hit mines); B4 owns render filtering, D2 owns
  judgment sprites/popups.
- **No scoring/combo/life/DP** in B4; the engine emits events and nothing else.

---

## Open Questions

1. **Visual-feedback scope (needs confirmation, non-blocking).** D2 owns judgment sprites, so B4
   enforces "feedback matches the log" as the hidden-note set plus an exposed `latest_event()` /
   `events()` stream. Confirm that is the intended B4/AC interpretation, or whether B4 must also render
   placeholder judgment text now.
2. **`RollHit` / `AvoidedMine` events (non-blocking).** OpenITG flashes the hold judgment on a roll
   re-hit and records avoided mines in stats, but emits no score event. Proposed default: log both
   (`RollHit`, `AvoidedMine`) with no score/combo effect so the log is complete. Confirm.
3. **Row aggregation for jumps (non-blocking for B4, blocking for B5 tests).** OpenITG emits one miss
   per row and scores a jump row by its last tap (`ComboIsPerRow=false`, `metrics.ini:18-20`;
   `Player.cpp:1399-1437,1504-1540`). Proposed: B4 logs per-note; B5 groups same-`note_time` events into
   one row judgment. Confirm the grouping key (exact `note_time` equality vs beat/row id) before B5
   writes parity tests.
4. **Miss `hit_time` convention (non-blocking).** Proposed `note_time + way_off` for determinism
   (Decision above). Confirm no consumer needs the actual detection time.
5. **Input reference accuracy (non-blocking).** Proposed `SDL_GetTicksNS()` captured once after the SDL
   poll loop, using SDL's shared nanosecond timebase. Confirm this is acceptable versus a dedicated
   audio-synced timestamp source.
6. **`GameplayView::update` signature change (non-blocking).** Proposed adding `held_columns` and a
   `constants` parameter to `init`. Confirm, or prefer setter-style injection to avoid changing the
   existing signature.

---

## Acceptance Criteria

- [ ] Given a note and an input timestamp, when evaluated, then the delta in ms is computed against the
      music clock and classified Fantastic/Great/Decent/Way Off/Miss per the B2 constants (tests 1-3)
- [ ] Given any hit, when judged, then a `JudgmentEvent {column, note_time_seconds, hit_time_seconds,
      delta_ms, window}` is appended to the in-memory session log (tests 1, 12)
- [ ] Given a hold, head/tail handling matches OpenITG (OK/NG windows, missed-head rule); rolls support
      re-hit logic; mines apply their penalty when triggered (tests 5-10; Value Provenance)
- [ ] Given visual judgment feedback, when shown, then the hidden-note set matches the event log
      exactly (test 13 + E2E step 4)
- [ ] Given an untouched tap/hold-head/roll-head passing its window, when it expires, then a Miss event
      is emitted; untouched mines emit `AvoidedMine`, not Miss (tests 3-4)
- [ ] No frame-timing or wall-clock logic in the judgment path: `judgment_engine.*` is time-parameterized
      and platform-free; the only `SDL_GetTicksNS` read is in `App` for input aging (E2E step 5)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 12/12 pass (11 prior + `judgment_engine_test`)
- [ ] Follows existing module/naming/test/CMake patterns; no scoring/combo/life/DP logic added (B5/B6)
