# Plan: Fix Mines Exploding Too Easily (#56)

## Summary

`JudgmentEngine::cross_mines` (`src/gameplay/judgment_engine.cpp:325-356`) runs on every update and
explodes **every** mine in a column that is not yet complete and has `time <= music_time`, as long as
the column is held. A mine only becomes complete when it expires as `AvoidedMine` after the Way Off
window (`expire_notes`, `:197-224`, 180 ms). So for **180 ms after a mine passes the receptor**, any
press in that column explodes it. Most often that press is the player stepping on the *next arrow in
the same column*, which is very common in ITG charts.

OpenITG works differently. `Player::Update` checks only the rows crossed **since the last update**
(`m_iMineRowLastCrossed`), and the check runs `PadStickSeconds` (0.05 s in the arcade layer) behind
the music. `Player::CrossedMineRow` then counts the panel as held only if it has been held for at
least `PadStickSeconds`, and routes the explosion through the normal `Step()` path. Step selection
for a deliberate press (closest note, mines included, Mine window 0.070 s) already matches OpenITG
in Blaze; tests pin it down below.

The fix:

1. Add `pad_stick` as a data-driven constant (0.05 s, from `metrics.ini:103`).
2. Have the engine record each column's last press time in `handle_step`.
3. Replace `cross_mines` with an OpenITG-style crossing cursor: mines in `(last_update − pad_stick, now − pad_stick]` are checked once, and
   if the column has been held since at or before `now − pad_stick`, the engine runs the shared step routine at `now − pad_stick`.
4. Add `judgment_engine_test` cases for stepping on a mine, holding through a mine, a tap next to a mine, frame hitches and pad-stick.

The input layer's stuck-held audit found one gap: a key held across a remap. The plan closes it by
clearing action state in `apply_bindings`.

## User Story

As a player
I want mines to explode only when I actually step on them or keep a panel held as they cross
So that pressing the next arrow in a column right after a mine doesn't cost me life and score the way it wouldn't in In The Groove

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | MEDIUM |
| Systems Affected | `gameplay/judgment_engine` (crossing cursor, press-time tracking, shared step routine), `timing/judgment_constants` (+`pad_stick`), `data/judgment_constants_loader`, `assets/data/judgment_constants.json`, `input/input_manager` (`apply_bindings` clears held state), tests (`judgment_engine_test`, `judgment_constants_test`, `input_test`) |
| GitHub Issue | #56 (related: #57 timing-window verification) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Release) |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic` |
| Cores | 16 | `-j16` is safe |
| Baseline tests | **38/38 pass** | `cmake --build build -j && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build` gives "100% tests passed out of 38" (0.45 s), on `main` @ `0e9e3ff` |
| Upstream source | `<scratchpad>/oitg/` fetched from `raw.githubusercontent.com/openitg/openitg/f2c129fe65c65e4a9b3a691ff35e7717b4e8de51/` | `src/Player.cpp`, `src/Player.h`, `src/PrefsManager.cpp`, `src/ScreenGameplay.cpp`, `assets/patch-data/Themes/default/metrics.ini`. This is the same commit already pinned in `src/timing/judgment_constants.hpp:10-14` |
| Input to engine | `src/gameplay/gameplay_view.cpp:126-156` | Only **press** events reach `handle_step`, with a music time rebuilt from the SDL ns timestamp. Key repeat is dropped (`input_manager.cpp:267-270`), which matches OpenITG's `IET_FIRST_PRESS` (`ScreenGameplay.cpp:2058-2065`) |
| Held state | `src/screens/gameplay_screen.cpp:58-62` | Sampled from `InputManager::is_action_down` once per tick, **before** `handle_input_events`. Presses for this tick reach `handle_step` before `judge_.update(...)` (`gameplay_view.cpp:187`), so a press time recorded in `handle_step` is always visible to the same tick's crossing check |
| Chart-pack evidence | `songs/In The Groove{,2,3}` (1117 dance-single charts, 10,759 mines) | A rough scan (first BPM only; script in scratchpad `minescan.py`) found **514 mines (4.8 %)** with a same-column note within 180 ms after them, and **1758 (16 %)** within 300 ms. Under the current code, stepping early or on time on any of those follow-up notes explodes the mine before it |

**Start green, stay green:** 38 tests pass now. This plan adds **no new test targets**, only cases in
existing ones, so **38/38** are expected afterwards.

---

## Root Cause (to be posted on #56; AC 1)

1. **Main cause: a crossing check that never stops.** `cross_mines` (`judgment_engine.cpp:325-356`)
   tests `note.time_seconds > music_time`, then `held`, then explodes. There is no "already crossed" cursor. A mine
   stays eligible from its own time until `expire_notes` marks it `AvoidedMine` at `time + way_off`
   (180 ms later). A press during that window (for example on the next arrow in the column, or just
   resting a key down) explodes a mine the player had already safely passed. OpenITG checks only
   `FOREACH_NONEMPTY_ROW_ALL_TRACKS_RANGE(m_NoteData, r, m_iMineRowLastCrossed, iRowNow+1)` and then
   moves the cursor forward (`Player.cpp:632-646`). Each mine row is evaluated **once**, when it crosses.
2. **Secondary cause: no pad-stick.** The arcade layer sets `PadStickSeconds=0.05` (`metrics.ini:103`).
   The crossing cursor runs 50 ms behind the music (`Player.cpp:635`), and a held panel counts only if
   `GetSecsHeld() >= PadStickSeconds` (`Player.cpp:1474-1478`). That is, the panel must already have been down at
   the crossing instant and still be down 50 ms later. Blaze treats any instantaneous `held` sample as enough.
3. **Not a cause: step note selection.** `handle_step` (`judgment_engine.cpp:60-94`) already mirrors
   `GetClosestNote` (`Player.cpp:779-829`, mines included, tie goes to the later note) and the
   Mine-window check (`Player.cpp:946-949`). A step aimed at a tap with a nearby mine judges the tap whenever the tap
   is closer, exactly as in OpenITG. Tests are added to pin this down.
4. **Not a cause: the mine window.** It is `0.070` s, which equals the arcade `JudgeWindowSecondsMine=0.070000`
   (`metrics.ini:93`). That overrides the compiled default `0.090` (`PrefsManager.cpp:95`).
5. **Input audit (from the issue's technical notes).** Focus loss already clears held state
   (`input_manager.cpp:248-255`). A **remap** (`apply_bindings`, `input_manager.cpp:147-160`) rebuilds
   `key_map_` but keeps `action_states_`. A key held while bindings change has its release looked up in the
   *new* map, so the release can miss and leave the action stuck "down". That is fixed here (Task 5).

---

## Value Provenance

All OpenITG references are at commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`.

| Value / behaviour | Source | Notes |
|-------------------|--------|-------|
| Mine window = **0.070 s** | `assets/patch-data/Themes/default/metrics.ini:93` (`[Preferences]` section, header at `:22`) | Overrides the compiled default `0.090f` ("same as great") at `src/PrefsManager.cpp:95`. No `[Preferences-cabinet/kit/ps2]` override exists. This already matches `judgment_constants.cpp:20` and `assets/data/judgment_constants.json:11` (confirms AC 5) |
| Mine window gets scale/add | `src/Player.cpp:44, 50-51` (`AdjustedWindowTap`) | Arcade scale/add are 1.0/0.0 (`metrics.ini:90-91`), so this has no effect today. Wiring scale/add into all windows belongs to #57 |
| `PadStickSeconds` = **0.05 s** | `metrics.ini:103` ("Set this on all plaforms because many home pads are sticky too.") | Compiled default is `0` (`PrefsManager.cpp:250`). Only one occurrence in metrics.ini |
| Crossing cursor runs `PadStickSeconds` behind the music; it visits rows in `[lastCrossed, rowNow]` once, then sets `lastCrossed = rowNow + 1` | `Player.cpp:632-646` | Uses `BeatToNoteRowNotRounded` (`NoteTypes.h:204`, truncation). In Blaze's seconds domain this becomes the half-open interval `(prev_cursor, cursor]` |
| Cursor starts at the load position | `Player.cpp:262-263` (`m_iMineRowLastCrossed = row(songBeat) - 1`) | Blaze uses no lower bound on the first update (equivalent here, because nothing exists before the start position) |
| Held-crossing condition | `Player.cpp:1461-1488` (`CrossedMineRow`) | When `PadStickSeconds > 0`, the condition is `GetSecsHeld >= PadStickSeconds`, then `Step(t, now - PadStickSeconds, bHeld=true)`. With `0`, it is `IsButtonDown`, then `Step(t, now, true)`. It runs for every mine in the crossed row, **without** checking whether the mine is already graded |
| Crossing goes through the full step path | `Player.cpp:832-842, 846-857` | `bHeld` only skips the calorie count. Closest-note selection (`:903-905`), mine/tap scoring (`:946-960`) and the roll-life refresh (`:1164-1222`) all run |
| Closest note: mines included, ungraded only, ±`StepSearchDistance` | `Player.cpp:27-28` (1.0 s), `:779-806` (`GetClosestNoteDirectional`: skips only empty, and graded when `!bAllowGraded`), `:811-829` (`GetClosestNote`: on a tie, returns `iNextIndex`, the later note) | Already implemented (`judgment_engine.cpp:10-11, 60-82`) |
| A mine outside the window gives `TNS_NONE` (the step is consumed, nothing is judged) | `Player.cpp:946-949, 1096` | Already implemented (`judgment_engine.cpp:128-131`) |
| An untouched mine becomes `TNS_AVOIDED_MINE` after the Boo window | `Player.cpp:440, 1368-1421, 1710-1713` | Already implemented (`expire_notes`); the order is misses, then holds, then crossing (`:440`, then `:632-646`), and Blaze keeps that order (`judgment_engine.cpp:189-191`) |
| Steps come only from the first press (no repeat) | `ScreenGameplay.cpp:2058-2065` (`IET_FIRST_PRESS`) | Already true (`input_manager.cpp:267-270`, `gameplay_view.cpp:137-139`) |
| `GetSecsHeld` start time = the button-down instant | OpenITG `InputMapper::GetSecsHeld` (input filter press time) | Blaze uses the press's music time, already rebuilt from the SDL ns timestamp (`judgment_input.hpp:10-18`) and recorded in `handle_step` |

---

## Pinned Semantics (what the engine must do after the fix)

Notation: `now` is the music time passed to `update`, `P` is `windows.pad_stick`, `cursor = now − P`, and
`prev_cursor = last_update_time_ − P`, or −∞ on the first update after `reset`.

1. **Deliberate step** (`handle_step(col, t)`): set `last_press_time_[col] = t`, then run
   `step(col, t)`. `step` is today's `handle_step` body: roll refresh, then the closest ungraded note
   (mines included) within 1.0 s, then the mine window or tap classification. Behaviour is unchanged apart from the recorded press time.
2. **Crossing** (in `update`, after expiry and holds): for each column and each mine (graded or not) with
   `prev_cursor < mine.time ≤ cursor`: if `held_columns[col]` **and** `last_press_time_[col] ≤ cursor`,
   call `step(col, cursor)` **once for that mine** (OpenITG calls `Step` once per crossed mine row).
   That shared routine decides the outcome. Normally it picks the just-crossed mine with `|delta| ≤ one frame`, which gives
   `HitMine` with `hit_time = cursor` and a small positive `delta_ms`.
3. A mine crossed while the column was **not** held (or held for less than `P`) is never re-checked.
   It expires to `AvoidedMine` as today.
4. **Frame hitch:** if a stall pushes `cursor` well past the mine, `step(col, cursor)` measures the
   real distance. If that distance is over 0.070 s, nothing happens and the mine later becomes `AvoidedMine`. If `expire_notes` already
   graded it in the same update, the step skips it as graded.
5. `P = 0` is allowed. Every press then satisfies `last_press_time_ ≤ cursor = now`, which is OpenITG's `IsButtonDown` branch.
6. `last_press_time_` starts at −∞ on `reset`, so a column that is held but has no recorded press counts as held
   long enough. This keeps the existing unit tests and the "held from before the song" case working.
7. A backward time jump keeps today's behaviour (resync, emit nothing). The cursor follows from `last_update_time_`.

---

## Patterns to Follow

### Pure, time-parameterized engine (no clocks)
```cpp
// SOURCE: src/gameplay/judgment_engine.hpp:12-20
// This module is time-parameterized: callers pass an absolute music time (from
// `MusicClock`) rather than letting the engine read a clock, exactly like
// `NoteField`. It includes no SDL/GL/audio/chrono headers, so the judgment path
// can never consult wall-clock or frame timing (AGENTS.md core principle 1).
```

### OpenITG provenance comments inline
```cpp
// SOURCE: src/gameplay/judgment_engine.cpp:10-11, 56-61
// OpenITG `StepSearchDistance = 1.0 s` (src/Player.cpp:27).
constexpr double kStepSearchDistanceSeconds = 1.0;
...
    // Closest ungraded note within +/- StepSearchDistance; ties prefer the later
    // note (Player.cpp:791-823, 894-895).
```

### Data-driven constant (struct default + compiled default + loader + JSON + validate)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:25-36, judgment_constants.cpp:14-24,
//         src/data/judgment_constants_loader.cpp:88-99, assets/data/judgment_constants.json:5-16
double hit_mine  = 0.0700;                                 // struct default
c.windows.hit_mine = 0.0700;                               // compiled_defaults()
override_number(node, "hit_mine", target.hit_mine);        // override_windows()
"hit_mine": 0.07,                                          // windows_seconds
```

### Error handling
Constants are validated, not asserted. `JudgmentConstants::validate` returns `fail("...")` with a
readable reason (`judgment_constants.cpp:61-82`). The engine guards null chart/constants and invalid
columns with early returns (`judgment_engine.cpp:51-54, 178-181`). There are no exceptions on the judgment path.

### Tests (idiom)
```cpp
// SOURCE: tests/judgment_engine_test.cpp:355-366
    // 10. Held-over-mine crossing vs avoided at expiry.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(1, 3.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(1, 2.0); // no note in range: nothing
        TEST_CHECK(engine.events().empty());
        engine.update(3.05, held_col(1)); // crossing while held
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::HitMine);
    }
```
Helpers `make_note`, `held_none`, `held_col` and `approx` are at `tests/judgment_engine_test.cpp:22-51`.
Numbered sections print `"  - ..."` on success.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/judgment_constants.hpp` | UPDATE | Add `TimingWindows::pad_stick = 0.05` with a provenance comment |
| `src/timing/judgment_constants.cpp` | UPDATE | Set `pad_stick` in `compiled_defaults()`; add validation (finite, ≥ 0) |
| `src/data/judgment_constants_loader.cpp` | UPDATE | `override_number(node, "pad_stick", ...)` in `override_windows` |
| `assets/data/judgment_constants.json` | UPDATE | `"pad_stick": 0.05` in `windows_seconds`; mention `PadStickSeconds` in `source` |
| `src/gameplay/judgment_engine.hpp` | UPDATE | Private `step(int, double)`, `last_press_time_` array, revised `cross_mines` signature and doc comments |
| `src/gameplay/judgment_engine.cpp` | UPDATE | Extract `step`, record press time, crossing cursor with pad-stick, fix stale line cites |
| `src/input/input_manager.cpp` | UPDATE | `apply_bindings` clears held action state (stuck-held after remap) |
| `tests/judgment_engine_test.cpp` | UPDATE | New section 16 (mine semantics) and make section 10 less brittle |
| `tests/judgment_constants_test.cpp` | UPDATE | `pad_stick` default parity, `same_constants`, JSON override, and validation |
| `tests/input_test.cpp` | UPDATE | Held action is released by `apply_bindings` |

No new files or test targets. `GameplayView` and `GameplayScreen` need no change: press events already
reach `handle_step` before `update` in the same tick.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: `pad_stick` constant (data-driven)

- **Files**: `src/timing/judgment_constants.hpp`, `src/timing/judgment_constants.cpp`,
  `src/data/judgment_constants_loader.cpp`, `assets/data/judgment_constants.json`
- **Action**: UPDATE
- **Implement**:
  1. In `TimingWindows`, after `hold_roll`, add
     `double pad_stick = 0.05;`, with a comment saying it is OpenITG `PadStickSeconds` (`metrics.ini:103`; compiled default 0 at
     `PrefsManager.cpp:250`): the crossing check lags the music by this much, and a held panel counts only once it has been
     held this long (`Player.cpp:632-646, 1461-1488`). Also note that it is not a judge window, so
     `judge_window_scale/add` must never apply to it.
  2. In `compiled_defaults()`, add `c.windows.pad_stick = 0.05;` and extend the provenance comment block
     (`:8-11`) with the `PadStickSeconds` line.
  3. In `validate`, do **not** add it to `positive_windows` (0 is legal). Add a separate
     `if (!std::isfinite(windows.pad_stick) || windows.pad_stick < 0.0) return fail("pad_stick must be finite and >= 0");`
  4. In `override_windows`, add `override_number(node, "pad_stick", target.pad_stick);`
  5. In the JSON, add `"pad_stick": 0.05` after `"hold_roll"`, and append `"; PadStickSeconds from metrics.ini [Preferences]"`
     (or similar) to `"source"`.
- **Mirror**: the `hit_mine` plumbing in all four files (see Patterns).
- **Validate**: `cmake --build build -j16 && ctest --test-dir build -R judgment_constants_test --output-on-failure`

### Task 2: Extract the shared step routine and record press times

- **Files**: `src/gameplay/judgment_engine.hpp`, `src/gameplay/judgment_engine.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Header: add the private `void step(int column, double music_time_seconds);` and
     `std::array<double, 4> last_press_time_{};`. Update the doc on `handle_step`: "Button-down … Mirrors
     Player::Step/HandleStep … records the press instant (for pad-stick, `GetSecsHeld`)". Update the doc on `update`:
     "held-over-mine crossing (Player::CrossedMineRow) since the previous update, `pad_stick` behind".
  2. `reset()`: `last_press_time_.fill(-std::numeric_limits<double>::infinity());` (include `<limits>`).
  3. `handle_step(column, t)`: keep the guard (`chart_`/`constants_`/`valid_column`), set
     `last_press_time_[column] = t`, and call `step(column, t)`.
  4. `step(column, t)`: move the current body of `handle_step` (`:56-94`, roll refresh, closest-note selection, dispatch) here
     **unchanged in behaviour**. Fix the stale cites in its comments to the pinned commit:
     closest-note search `Player.cpp:779-829, 903-905`; roll refresh `Player.cpp:1164-1222`; mine window
     `Player.cpp:946-949`. In `handle_step_tap`, `:938-947` is fine. In `handle_step_mine`, cite `:946-949`.
- **Mirror**: `judgment_engine.cpp:51-95`
- **Validate**: build plus `ctest -R judgment_engine_test` (still green; no behaviour change yet).

### Task 3: Crossing cursor with pad-stick

- **File**: `src/gameplay/judgment_engine.cpp` (+ header signature if changed)
- **Action**: UPDATE
- **Implement**: rewrite `cross_mines(double music_time, const std::array<bool,4>& held_columns)` to follow
  Pinned Semantics 2 to 5:
  ```text
  P      = constants_->windows.pad_stick
  cursor = music_time - P
  lower  = has_last_update_ ? last_update_time_ - P : -infinity
  for column in 0..3:
      if !held[column] or last_press_time_[column] > cursor: continue
      for i in column_mines_[column]:              // time-ordered
          t = note.time_seconds
          if t <= lower: continue
          if t > cursor: break                     // later mines not crossed yet
          step(column, cursor)                     // once per crossed mine (Player.cpp:1466-1484)
  ```
  - Do **not** skip graded mines before calling `step`. OpenITG's `CrossedMineRow` does not check (`:1467`). `step`
    itself skips graded notes, so an already-hit mine produces whatever `step` finds (see Open Question 1).
  - Order inside `update()` stays: `expire_notes`, `update_holds`, `cross_mines` (`Player.cpp:440` then `:632-646`).
    `has_last_update_` and `last_update_time_` are read **before** they are overwritten at `:193-194` (they already are).
  - Replace the comment block with provenance for `Player.cpp:632-646` (cursor, pad-stick lag) and
    `:1461-1488` (held-long-enough check, `Step(..., bHeld=true)`), and explain why a past mine is never
    re-checked (the root cause of #56).
- **Mirror**: the cursor/early-out style of `expire_notes` (`:197-208`).
- **Validate**: build plus `ctest -R judgment_engine_test`. Existing section 10 should still pass (`3.05 − 0.05 == 3.0`
  exactly in double, verified), but Task 4 makes it robust.

### Task 4: Engine tests for mine semantics

- **File**: `tests/judgment_engine_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - **Section 10:** change `engine.update(3.05, held_col(1))` to `engine.update(3.06, held_col(1))` so the crossing is not
    at an exact float boundary, and add `TEST_CHECK(approx(engine.events().front().hit_time_seconds, 3.06 - k.windows.pad_stick))`.
  - Add **section 16, "Mine semantics (#56, OpenITG Player::Step / CrossedMineRow)"**, each case on a fresh chart and engine,
    with `P = k.windows.pad_stick`:
    1. **Step on mine (in window):** mine col 0 @2.0; `handle_step(0, 2.0 - 0.06)` gives a single `HitMine`, `delta_ms ≈ -60`.
       `handle_step(0, 2.0 + 0.071)` on a fresh mine gives nothing (an outside-window step is consumed, no event). This complements section 4.
    2. **Tap next to mine, tap after mine (the regression):** mine col 0 @2.0, tap col 0 @2.125.
       `update(1.9, none)`, `update(2.06, none)` (crossed unheld), `handle_step(0, 2.10)` judges the **tap** (`note_index`
       of the tap, `Excellent`), then `update(2.11, held_col(0))` and `update(2.2, held_col(0))`. Expect **no** `HitMine` at all, and
       the mine ends as `AvoidedMine` after `update(2.0 + way_off + 1e-3, ...)`. (The current code emits `HitMine` here.)
    3. **Tap next to mine, mine after tap:** tap col 0 @2.0, mine col 0 @2.125. `handle_step(0, 2.03)` gives `Tap` on the tap.
       Release, then `update(2.2, none)`: no `HitMine`.
    4. **Mine closer than the tap wins (OpenITG selection):** same chart as 3; `handle_step(0, 2.08)` gives `HitMine` (mine at
       0.045 is closer than the tap at 0.08). The tap stays ungraded and later expires to `Miss`. This documents the rule rather than a bug.
    5. **Pad-stick: pressed after the crossing instant:** mine col 0 @2.0, tap col 0 @2.1. `update(1.9, none)`,
       `handle_step(0, 2.08)` gives a tap `Fantastic`, then `update(2.09, held_col(0))` (cursor 2.04 crosses the mine, but the press at 2.08 > 2.04),
       then `update(2.3, held_col(0))`. Expect no `HitMine`; the mine becomes `AvoidedMine`.
    6. **Pad-stick: released within `P` after the mine:** mine col 0 @2.0. `handle_step(0, 1.8)` (mine 0.2 away gives nothing),
       `update(2.02, held_col(0))` (cursor 1.97, not crossed yet), `update(2.06, held_none())` (crossed, not held). No `HitMine`;
       later `AvoidedMine`.
    7. **Hold through mine (held long enough):** hold head col 0 @1.0 to 1.5 and mine col 0 @2.0. `handle_step(0, 1.0)`, then
       `update` every 0.01 s from 1.0 to 2.2 with `held_col(0)`. Expect exactly one `HitMine` (`note_index` = mine),
       `hit_time` in `[2.0, 2.0 + 0.01]`, plus `HoldOk` for the hold, and no `AvoidedMine`.
    8. **Never re-checked once crossed:** mine col 1 @2.0. `update(2.1, none)` (crossed unheld), then `update(2.12, held_col(1))`
       and `update(2.15, held_col(1))` with no press recorded (−∞, so it counts as held long enough). No `HitMine`.
    9. **Frame hitch past the window:** mine col 2 @2.0, held from `handle_step(2, 1.5)`. `update(1.9, held)`, then a single
       `update(2.15, held)` (cursor 2.10, distance 0.10 > 0.070) gives no `HitMine`, then `AvoidedMine` at expiry.
       Within the window, `update(2.11, held)` on a fresh engine (cursor 2.06) gives `HitMine` with `delta_ms ≈ +60`.
    10. **Frame-rate independence:** run case 7 at a 0.1 s and a 0.001 s update grid. Both produce exactly one `HitMine` for the mine.
    11. **Crossing an already-hit mine runs the full step (OpenITG quirk; see Open Question 1):** mine col 0 @2.0, tap col 0
        @2.125. `handle_step(0, 1.96)` gives `HitMine` (mine closest), `update(1.97, held)`, then `update(2.06, held)` (cursor 2.01,
        the crossing calls step, the mine is graded, the tap is at 0.115). Expect a `Tap` event with `window == Decent` and no
        second `HitMine`. If the owner rejects the quirk (OQ1), flip this case to expect no Tap event.
  - Print `"  - Mine semantics match OpenITG Step/CrossedMineRow (pad-stick, once-only crossing)."`.
- **Mirror**: sections 4, 9, 10 and 11 (`:153-181, 324-377, 380-430`).
- **Validate**: `ctest --test-dir build -R judgment_engine_test --output-on-failure`

### Task 5: Release held state on remap (stuck-held audit)

- **Files**: `src/input/input_manager.cpp`, `tests/input_test.cpp`
- **Action**: UPDATE
- **Implement**: at the top of `InputManager::apply_bindings` (`:147`), call `clear_action_states();` with a comment
  saying a key held across a rebind would have its release looked up in the new map and stay stuck down. Add an `input_test`
  case: key-down Left (as at `input_test.cpp:24-32`), assert down, call `input.apply_bindings(InputSettings{})`, assert
  `!is_action_down(Left)`.
- **Mirror**: the focus-loss test (`tests/input_test.cpp:192-213`).
- **Validate**: `ctest --test-dir build -R input_test --output-on-failure`

### Task 6: Constants tests

- **File**: `tests/judgment_constants_test.cpp`
- **Action**: UPDATE
- **Implement**: add `!nearly(a.windows.pad_stick, b.windows.pad_stick)` to `same_constants` (`:39-49`);
  add `TEST_CHECK(nearly(defaults.windows.pad_stick, 0.05));` to the parity block (`:105-116`); add a JSON override
  case (`{"windows_seconds": {"pad_stick": 0.0}}` loads with 0.0, which is valid) and a validation case (`-0.01` is rejected), using
  the existing override/invalid patterns (`:185`, `:232-240`). The asset-file-equals-defaults check (if present) must stay green
  with the new JSON key.
- **Validate**: `ctest --test-dir build -R judgment_constants_test --output-on-failure`

### Task 7: Document the root cause on the issue (AC 1)

- **Action**: post a comment on #56 (`gh issue comment 56 --body-file <scratch file>`). Use the **Root Cause** section above,
  condensed: the main cause, pad-stick, selection and window confirmed, the input audit, and the chart-pack numbers. Do this after the
  tests are green so the comment can say they cover the cases.
- **Validate**: `gh issue view 56 --json comments` shows the comment.

### Task 8: Full suite

- **Validate**: full build plus ctest (see Validation). Expect **38/38**, with no new warnings in touched files.

---

## Validation

```bash
# Build
cmake --build build -j16 2>&1 | grep -E "warning|error" ; cmake --build build -j16

# Tests: expect 38/38 (headless, no window or sound)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# Targeted
ctest --test-dir build -R "judgment_engine_test|judgment_constants_test|input_test|metronome_sync_test|score_keeper_test|life_keeper_test" --output-on-failure

# Scope guard
git diff --stat
git diff --name-only | grep -E "render|note_field|screens/"     # expect no output
git status --short .agents/stories/todo-stories.md               # still "??", untouched
```

No linter is configured, so the `-Wall -Wextra -Wpedantic` output is the lint gate.

## End-to-End Verification

1. **Automated (headless, required):** `judgment_engine_test` section 16 drives the real `JudgmentEngine` through
   the same `handle_step`/`update(now, held)` sequence that `GameplayView` uses (`gameplay_view.cpp:126-187`), covering
   stepping on a mine, holding through one, a tap next to one, pad-stick, hitches and once-only crossing. `metronome_sync_test` (no mines) must
   stay green, which shows the tap path is unchanged.
2. **App smoke (headless, required):** `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   exits 0. Do **not** launch a windowed or interactive binary from the agent.
3. **Owner play-test (interactive; the agent doesn't run it):** play a mine-heavy ITG chart, for example any chart from the scan above
   with mines right before same-column arrows (`songs/In The Groove*`), or use `./build/blaze-4k --gameplay-demo <song.sm>`. Check:
   - stepping on the arrow that follows a mine in the same column no longer explodes the mine;
   - deliberately stepping onto a mine (within about 70 ms) still explodes it;
   - holding a panel down through a mine still explodes it (once), and tapping briefly just after it passes does not;
   - the results screen's mine count (`HitMine`) drops to roughly what OpenITG gives for the same play.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| **OpenITG quirk:** a held crossing of an *already-graded* mine runs the full `Step` and can judge a nearby same-column tap (for example as Decent) | Faithful by default (AGENTS principle 2), pinned by test 16.11 and raised as Open Question 1. The alternative is a one-line guard (only step when the crossed mine is still ungraded) | In scope (documented) |
| Pad-stick makes a held explosion register 50 ms later than the mine's time | That is OpenITG behaviour. `hit_time = now − P`, so `delta_ms` stays about 0 to one frame, as in OpenITG (the Step time is `now − PadStick`) | In scope |
| Seconds vs rows: OpenITG's closest-note and crossing logic work in note rows, Blaze's in seconds | They are identical at constant BPM. They differ only around stops/BPM changes, where OpenITG truncates rows (`BeatToNoteRowNotRounded`). This is pre-existing, documented in comments, and not changed here | Out of scope (flag only) |
| `last_press_time_` is per column, but two physical keys map to one column (Left + D) | Re-pressing the second key resets the press time, which is conservative (fewer held explosions). OpenITG tracks per `GameInput`. Related pre-existing issue: `action_states_` is per action, so releasing one of two held keys clears the column while the other is still down (under-reports held for holds too) | Out of scope (flag as follow-up) |
| `judge_window_scale/add` are not applied to `hit_mine` (or any window) | Both are identity in the arcade layer, and verifying window wiring is #57 | Out of scope (flag for #57) |
| Clearing action state on `apply_bindings` drops a legitimately held key at rebind | Rebinding only happens on the remap screen or at startup, never mid-song, and focus loss already behaves this way | In scope |
| Setting `pad_stick >= way_off` in JSON would make held crossings impossible (mines expire first) | Validation only requires finite and ≥ 0, matching OpenITG's unconstrained pref. This is a user-edited config, and the default is 0.05 | In scope (accepted) |
| Scoring/life consumers assume one `HitMine` per mine | Unchanged. A mine can be graded only once (`step` skips graded notes) | In scope (test 16.7/16.11 assert no second HitMine) |

---

## Decisions

- **Faithful crossing goes through the shared `step` routine** (OpenITG `CrossedMineRow` → `Step(bHeld=true)`), rather
  than exploding the mine directly. This keeps a single grading path and OpenITG parity.
- **`pad_stick` is a data-driven constant in `TimingWindows`** (JSON key `windows_seconds.pad_stick`), seeded from the same
  arcade `[Preferences]` layer as every other window (AGENTS pattern 3).
- **The press time comes from `handle_step`** (SDL-timestamp-accurate music time), not from edges in `held_columns`
  (tick-quantised). Nothing changes in the screen/view layers.
- **No new test target.** The cases go into the existing `judgment_engine_test`, `judgment_constants_test` and `input_test`, so the suite stays at 38.

## Open Questions

1. **Keep OpenITG's "held crossing of an already-hit mine judges the next tap" quirk?** *Proposed default:* yes, for
   faithfulness (it is what `Player.cpp:1461-1488` does: no graded check before `Step`). Test 16.11 pins it. If the owner
   prefers to avoid it, add `if (states_[i].tap != TapJudgment::Num) continue;` before `step(...)` in `cross_mines` and
   flip test 16.11. That would be a deliberate, documented deviation.
2. **Adopt `PadStickSeconds = 0.05` at all?** It comes from the arcade `metrics.ini`, which the project already treats as
   authoritative, and its comment targets home pads too. *Proposed default:* yes. Setting `"pad_stick": 0` in the JSON gives the
   compiled-StepMania `IsButtonDown` behaviour with no code change.

---

## Acceptance Criteria

- [ ] Root cause identified and posted on #56 (Task 7; Root Cause section)
- [ ] A step aimed at a tap with a nearby same-column mine judges the tap when the tap is the closest note (OpenITG `GetClosestNote`); tests 16.2–16.4
- [ ] A held column explodes a mine only when it crosses (once, `pad_stick` behind, held ≥ `pad_stick`), never a mine already in the past or after a hitch beyond the window; tests 16.5–16.10
- [ ] `tests/judgment_engine_test.cpp` covers stepping on a mine (16.1), holding through a mine (16.7), and a tap next to a mine (16.2–16.4)
- [ ] Mine window 0.070 s confirmed against OpenITG `metrics.ini:93` (overrides `PrefsManager.cpp:95` 0.090), pinned in `judgment_constants_test`
- [ ] Held state cannot stay stuck after a remap (Task 5); focus loss is already covered
- [ ] All tasks completed; the build has no new warnings in touched files
- [ ] 38/38 tests pass (start green at 38, no new targets)
- [ ] Follows existing patterns (pure time-parameterized engine, data-driven constants with provenance, `TEST_CHECK` idiom)
