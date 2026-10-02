# Plan: Implement OpenITG MercifulBeginner (#67)

## Summary

Blaze 4k judges and scores Beginner charts the same way as every other difficulty. OpenITG does not. The ITG theme turns on
`MercifulBeginner=1` (`metrics.ini:157`, `[Preferences]`), and on a `DIFFICULTY_BEGINNER` chart that does three things:

1. It widens **only the Way Off (Boo) window, by +0.5 s**. The bonus is added after `JudgeWindowScale`/`JudgeWindowAdd`. It
   applies to both tap classification and miss expiry.
2. An **early** step that classifies as Way Off is only shown. Nothing is recorded and the note stays live.
3. **Negative DP and grade weights are clamped to 0**, which covers Way Off, Miss, hit mine and a negative hold NG.

The issue's line references were re-checked against OpenITG `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` while writing this plan
(see **Value Provenance**). Life is confirmed unaffected, so `LifeKeeper` is not changed.

**Approach.** Beginner status is derived from the `Chart*` that `JudgmentEngine::reset` and `ScoreKeeper::reset` already receive.
A new pure helper, `Chart::is_beginner()`, mirrors OpenITG `StringToDifficulty`: a case-insensitive `"beginner"`. That means no
call-site signature changes. A data-driven `JudgmentConstants::merciful_beginner` flag (default `true`, JSON key
`"merciful_beginner"`) gates all three behaviors. The +0.5 s bonus is layered on top of the existing `effective_windows()` through a
new `effective_windows(bool is_beginner)` overload, and `classify_tap` gains an `is_beginner` parameter (default `false`). The
engine keeps a separate, display-only queue for the suppressed early Way Off. It is never part of the judgment log, so score, life,
combo and results never see it. `GameplayView` feeds that queue to the judgment popup only. `ScoreKeeper` clamps every DP weight it
adds to at least 0 when the merciful rule applies. The possible-DP total is unchanged, as in OpenITG.

One preparatory change is needed. `Chart::difficulty` currently defaults to `"Beginner"`, so every hand-built test chart would
silently become merciful and break existing expectations (for example `score_keeper_test` §13/§15, where a Miss costs −12). The
default becomes `""`, which mirrors OpenITG `Steps` defaulting to `DIFFICULTY_INVALID` (see Decision D1). The parser always sets
the label, so production behavior is unaffected.

## User Story

As a new player learning on Beginner charts
I want Blaze 4k to be as forgiving on Beginner as a real ITG cabinet is (wider Way Off, no early Way Off penalties, no negative score)
So that my first songs feel the way they do on real hardware and a rough run does not end in a negative percent

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX (OpenITG parity) |
| Complexity | MEDIUM |
| Systems Affected | `chart/chart.hpp` (default label + `is_beginner`), `timing/judgment_constants` (flag, Beginner windows, `classify_tap`), `data/judgment_constants_loader` + `assets/data/judgment_constants.json` (flag), `gameplay/judgment_engine` (windows, early Way Off suppression, display-only queue), `gameplay/score_keeper` (weight clamp), `gameplay/gameplay_view` (popup wiring), tests |
| GitHub Issue | #67 (found as M3 in #57; plan `completed/031-verify-judgment-timing-windows-plan.md:143-151`) |
| Branch | `feature/033-merciful-beginner` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Release, host g++) |
| C++ compiler | GCC 16.2.1 | C++20 with `-Wall -Wextra -Wpedantic` (there is no separate linter, so new warnings in touched files act as the lint gate) |
| Baseline | **38/38 pass** on `main` @ `d72f23f` | `cmake --build build -j$(nproc)`, then `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` → "100% tests passed out of 38" (0.40 s) |
| Test sandbox | `bwrap … --tmpfs /dev/snd --unshare-net` | **Required**: `audio_test` touches real hardware. Never run ctest or the app unsandboxed, and never play sound |
| Test harness | One `main()` per area with `TEST_CHECK` + `std::abort`, numbered `// N.` sections | `tests/judgment_engine_test.cpp:13-19,61`. New cases go into existing executables, so **no new ctest target is added** and the count stays at 38 |
| Upstream source | `<scratchpad>/oitg/*.cpp` fetched from `raw.githubusercontent.com/openitg/openitg/f2c129fe…/` | Re-fetch with `curl -sfL https://raw.githubusercontent.com/openitg/openitg/f2c129fe65c65e4a9b3a691ff35e7717b4e8de51/src/Player.cpp` (likewise `ScoreKeeperMAX2.cpp`, `PlayerStageStats.cpp`, `LifeMeterBar.cpp`, `Difficulty.cpp`, `NotesLoaderSM.cpp`, `PrefsManager.cpp`, `GameState.cpp`, `Judgment.cpp`, `assets/patch-data/Themes/default/metrics.ini`) |
| App smoke | `./build/blaze-4k --headless --smoke-test 10` | `src/main.cpp:38,83-88`. Run it inside the same bwrap sandbox |
| Off-limits | `.agents/stories/todo-stories.md` (untracked) | Do not modify, stage or delete it |

---

## Value Provenance (OpenITG `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`)

| Value / behavior | Blaze target | Upstream source | Runtime layer that wins |
|---|---|---|---|
| `MercifulBeginner` on | `merciful_beginner = true` | `assets/patch-data/Themes/default/metrics.ini:157` (section `[Preferences]` from line 22) | Theme `[Preferences]` overrides the compiled default `false` (`src/PrefsManager.cpp:128`). `[Preferences-cabinet]` does not touch it |
| Bonus amount, Boo only | `+0.5 s` to `way_off` | `src/Player.cpp:55-56`: `if( bIsPlayingBeginner && PREFSMAN->m_bMercifulBeginner && tw==TW_Boo ) fSecs += 0.5f;` | — |
| Bonus order | After `base*scale+add` (and after the per-player `fTimingScale`, which Blaze does not have, effectively 1.0) | `src/Player.cpp:49-56` | — |
| Mine window not widened | `hit_mine` unchanged | `Player.cpp:55` (`tw==TW_Boo` only), mine check at `Player.cpp:948` uses `TW_Mine` | — |
| Hold/roll windows not widened | `hold_ok`/`hold_roll` unchanged | `AdjustedWindowHold` ignores `bIsPlayingBeginner` (`Player.cpp:60-74`) | — |
| Miss expiry uses the widened Boo | expiry threshold `music − way_off(beginner)` | `Player.cpp:440` (`UpdateTapNotesMissedOlderThan( GetMaxStepDistanceSeconds() )`), `:1710-1713` | Expiry also covers mines, so avoided-mine marking is delayed the same way (`Player.cpp:1397-1413`) |
| Step search radius | `kStepSearchDistanceSeconds = 1.0` (unchanged; 0.6815 < 1.0) | `Player.cpp:27` | — |
| Early Way Off is display-only | No log event, note stays live | `Player.cpp:1089-1093`: `bSteppedEarly = -fNoteOffset < 0`, and in that case only `m_Judgment.SetJudgment(score, bSteppedEarly)`. `tns` is not set, so there is no score, life or row handling | — |
| Meaning of "early" | `hit_time < note_time` (strict) | `fNoteOffset = (fStepSeconds - fMusicSeconds)` (`Player.cpp:929`), and early ⇔ `fNoteOffset > 0` | — |
| Beginner detection | case-insensitive `"beginner"` only | `IsPlayingBeginner` (`Player.cpp:1724-1737`) → `GetDifficulty()==DIFFICULTY_BEGINNER`; `StringToDifficulty` (`src/Difficulty.cpp:22-26`, `MakeLower`, `== "beginner"`); labels trimmed at `src/NotesLoaderSM.cpp:24-26` (Blaze's MSD reader already trims, `src/chart/msd_file.cpp:121-161`) | `"novice"` is **not** Beginner in OpenITG (it maps to `DIFFICULTY_INVALID`) |
| DP clamp | `max(0, weight)` on tap, hit-mine and hold DP | `src/ScoreKeeperMAX2.cpp:529-530` (tap/mine), `:544-545` (hold). Hit mine goes through the member overload at `:338` → `:490-493` (`m_bIsBeginner`) | — |
| Grade clamp | Same (Blaze grades from DP percent) | `ScoreKeeperMAX2.cpp:570-571,585-586`; `PlayerStageStats::GetGrade` `src/PlayerStageStats.cpp:164-179` | Grade weights equal DP weights in Blaze's data, and grade is computed from `ScoreState::percent` |
| Possible DP not clamped | `possible_dp` unchanged | `ScoreKeeperMAX2.cpp:449-451` passes `false` for `bBeginner` | — |
| Course mode is excluded | N/A (courses are out of scope in the PRD) | `ScoreKeeperMAX2.cpp:187-189` | — |
| Life unaffected | `LifeKeeper` untouched | `src/LifeMeterBar.cpp` has no Beginner/Merciful-beginner branch. Its only `*Difficulty` hits are `LifeDifficultyScale` (`:42-43,238-240`) | — |
| Beginner judgment graphic | Not adopted (presentation only) | `src/Judgment.cpp:32-34` loads `"BeginnerLabel"` | Out of scope |

Arithmetic with the shipped data: `way_off_eff = 0.18 × 1.0 + 0.0015 = 0.1815 s`, so the Beginner Way Off is `0.6815 s`.

---

## Pinned Semantics

- **S1 — Early Way Off.** On a merciful Beginner chart, if `classify_tap(|Δ|, true) == WayOff` and `hit_time < note.time_seconds`,
  the engine changes no `NoteState`, appends nothing to `events_`, and pushes one event to a separate display-only queue
  (`kind=Tap`, `window=WayOff`, `delta_ms=(hit−note)*1000 < 0`, the real `note_index`, `column` and `note_type`). This also
  applies to an early press inside the **base** Way Off band (for example −0.15 s), because OpenITG keys on `score==TNS_BOO`
  regardless of which part of the window it came from. Early Decent or better and every late Way Off are recorded normally.
  A step exactly on time is never "early".
- **S2 — Note stays live** after S1. It can be stepped again (a later Fantastic is recorded), and if untouched it expires as a
  Miss at `note + 0.6815 s`. Hold/roll heads behave the same: `hold_head_hit` stays false, so no hold life starts.
- **S3 — Held-over-mine crossing** reuses `step()`. If the crossing's closest note is a tap and it lands as an early Way Off,
  that is display-only too, exactly as in OpenITG (`Step(…, bHeld=true)` goes through the same `HandleStep`). There is no
  special case.
- **S4 — Mines linger longer** as closest-note candidates on Beginner, because expiry waits for the widened Boo. A press that
  picks such a mine outside the mine window yields no event (OpenITG `TNS_NONE`). This is faithful and needs no special case.
- **S5 — Combo, counts and life are unchanged.** A recorded Way Off or Miss still breaks combo, still increments `tap_counts`,
  and still drains life with the normal deltas. Only the DP added is clamped.
- **S6 — Results.** A Beginner run can no longer reach a negative percent, so a completed Fail-Off all-miss Beginner run now
  scores 0.00% / D. That run passes `results_submit_permitted` (`src/screens/results.cpp:25-39`) and becomes a high score, as it
  would in OpenITG. No code change is needed.

---

## Patterns to Follow

### Pure constants module + OpenITG provenance comments
```cpp
// SOURCE: src/timing/judgment_constants.hpp:116-127
    // The windows the judgment path must use. Mirrors OpenITG
    // `AdjustedWindowTap` / `AdjustedWindowHold` (src/Player.cpp:34-74): each of
    // fantastic..way_off, hit_mine, hold_ok and hold_roll becomes
    // `base * judge_window_scale + judge_window_add` (scale first, then add).
    ...
    [[nodiscard]] TimingWindows effective_windows() const;

    // Pure lookups (units: seconds, percent as fraction 0.0-1.0)
    [[nodiscard]] TapJudgment classify_tap(double delta_seconds) const;
```
The header must keep including only `<array>` and `<string>` (`judgment_constants.hpp:21-24`).

### Boolean JSON override (loader error style)
```cpp
// SOURCE: src/data/judgment_constants_loader.cpp:117-123
    auto it = node.find("merciful_drain");
    if (it != node.end() && !it->is_null()) {
        if (!it->is_boolean()) {
            throw std::runtime_error("field 'merciful_drain' must be a boolean");
        }
        target.merciful_drain = it->get<bool>();
    }
```
Any throw becomes a fallback to the compiled defaults with a warning (`judgment_constants_loader.cpp:215-218`).

### Engine event construction / window reads
```cpp
// SOURCE: src/gameplay/judgment_engine.cpp:109-137 (handle_step_tap) and :211-214 (expire_notes)
    const TapJudgment judgment = constants_->classify_tap(delta_seconds);
    if (judgment == TapJudgment::Miss) {
        return; // A step beyond Way Off yields TNS_NONE, not a Miss (Player.cpp:938-947).
    }
    ...
    const TimingWindows w = constants_->effective_windows();
    const double threshold = music_time - w.way_off;
```

### Drain API (append-only log; the new display queue mirrors the signature)
```cpp
// SOURCE: src/gameplay/judgment_engine.cpp:450-456
void JudgmentEngine::drain_new_events(std::vector<JudgmentEvent>& out) {
    if (new_event_begin_ < events_.size()) {
        out.insert(out.end(), events_.begin() + static_cast<std::ptrdiff_t>(new_event_begin_),
                   events_.end());
    }
    new_event_begin_ = events_.size();
}
```

### Scoring weights
```cpp
// SOURCE: src/gameplay/score_keeper.cpp:78-81, 170-179, 203-219
        case JudgmentKind::HitMine:
            state_.actual_dp += constants_->dp_weights.hit_mine;
...
int ScoreKeeper::tap_weight(TapJudgment j) const { ... const Weights& w = constants_->dp_weights; switch (j) {...} }
```

### Tests
```cpp
// SOURCE: tests/judgment_engine_test.cpp:785-805 (§17: one fresh chart + engine per sub-case)
    {
        blaze4k::JudgmentConstants wide = blaze4k::JudgmentConstants::compiled_defaults();
        wide.windows.judge_window_add = 0.02;
        TEST_CHECK(wide.validate());
        const blaze4k::TimingWindows ww = wide.effective_windows();
        ...
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.handle_step(1, note_time + k.windows.way_off + 0.01);
            TEST_CHECK(engine.events().size() == 1);
```
`score_keeper_test.cpp:44-80` provides `make_tap` / `make_hold_outcome` for synthetic events, and `:638-661` (§15) shows the
`GameplayView` stub-clock boundary pattern.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/chart/chart.hpp` | UPDATE | Default `difficulty` changes to `""` (D1). Add `is_beginner_difficulty(std::string_view)` and `Chart::is_beginner()` |
| `src/timing/judgment_constants.hpp` | UPDATE | `merciful_beginner` flag, `kMercifulBeginnerWayOffBonusSeconds`, `merciful_beginner_applies()`, `effective_windows(bool)`, `classify_tap(double, bool = false)` |
| `src/timing/judgment_constants.cpp` | UPDATE | Implement the above and seed `merciful_beginner = true` in `compiled_defaults()` |
| `src/data/judgment_constants_loader.cpp` | UPDATE | Top-level boolean `"merciful_beginner"` override |
| `assets/data/judgment_constants.json` | UPDATE | `"merciful_beginner": true` plus a provenance note in `"source"` |
| `src/gameplay/judgment_engine.hpp` / `.cpp` | UPDATE | Beginner flag from the chart, Beginner windows everywhere, early Way Off suppression (S1/S2), display-only queue and drain |
| `src/gameplay/score_keeper.hpp` / `.cpp` | UPDATE | Merciful flag from the chart, clamp DP weights ≥ 0 |
| `src/gameplay/gameplay_view.hpp` / `.cpp` | UPDATE | Drain the display-only queue into `judge_anim_` only |
| `tests/judgment_constants_test.cpp` | UPDATE | §1 parity line, `same_constants` field, new §13 (windows/classify) and §14 (loader flag) |
| `tests/judgment_engine_test.cpp` | UPDATE | New §19 (all engine behaviors plus controls) |
| `tests/score_keeper_test.cpp` | UPDATE | New §17 (clamp plus controls) and §18 (`GameplayView` Beginner boundary) |
| `tests/life_keeper_test.cpp` | UPDATE | New §19 control: life identical on Beginner |
| `tests/note_parser_test.cpp` | UPDATE | `is_beginner` on parsed charts and on the label helper |

`LifeKeeper`, `JudgmentAnimator`, the parsers, results and high scores are **not** changed.

---

## Tasks

Execute in order. After each task: `cmake --build build -j$(nproc)`. After tasks 1, 6 and 11, also run the full sandboxed ctest.

### Task 1: `Chart` Beginner helper and neutral default label

- **File**: `src/chart/chart.hpp`
- **Action**: UPDATE
- **Implement**:
  - Change `std::string difficulty = "Beginner";` to `std::string difficulty;`, with a comment that the parser always sets it,
    and that an empty label mirrors OpenITG `Steps` defaulting to `DIFFICULTY_INVALID` (it is not Beginner).
  - Add `#include <string_view>` and `<cctype>`, plus a free inline function
    `[[nodiscard]] inline bool is_beginner_difficulty(std::string_view label)`. It returns true only for a case-insensitive
    `"beginner"`, comparing with `std::tolower(static_cast<unsigned char>(c))`. Cite `Difficulty.cpp:22-26` and note that
    `"novice"` is not Beginner in OpenITG.
  - Add the member `[[nodiscard]] bool is_beginner() const { return is_beginner_difficulty(difficulty); }`.
- **Do not** touch the SSC parser's own `cur_diff = "Beginner"` default (`src/chart/simfile_parser.cpp:123,131`). See Risks.
- **Validate**: build, then the full sandboxed ctest stays at 38/38. No test depends on the old default: a grep found only
  parser-produced or explicitly set labels.

### Task 2: Merciful flag and Beginner windows in `JudgmentConstants`

- **Files**: `src/timing/judgment_constants.hpp`, `src/timing/judgment_constants.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `JudgmentConstants`, add `bool merciful_beginner = true;`. Its comment cites `metrics.ini:157` (`[Preferences]`) and the
    compiled default `false` at `PrefsManager.cpp:128`.
  - Add `static constexpr double kMercifulBeginnerWayOffBonusSeconds = 0.5;` (cite `Player.cpp:55-56`).
  - Add `[[nodiscard]] bool merciful_beginner_applies(bool is_beginner) const { return is_beginner && merciful_beginner; }`.
  - Add the overload `[[nodiscard]] TimingWindows effective_windows(bool is_beginner) const;`. It returns
    `effective_windows()`, and when `merciful_beginner_applies(is_beginner)` it also adds the bonus to `way_off` only. Its
    comment says the bonus is applied after scale/add (`Player.cpp:49-56`) and that the result must not be fed back in (unlike
    the scale/add normalization, the bonus would apply twice). The no-arg `effective_windows()` keeps its exact behavior.
  - Change `classify_tap(double delta_seconds)` to `classify_tap(double delta_seconds, bool is_beginner = false)` and use
    `effective_windows(is_beginner)`. Classification stays symmetric (`fabs`). Early suppression belongs to the engine, not
    here; say so in the comment.
  - In `compiled_defaults()`, add `c.merciful_beginner = true;`. `validate()` needs no change, because the bonus only widens
    an already-positive window.
- **Mirror**: `judgment_constants.cpp:145-182`
- **Validate**: build. Existing tests still pass, because the default arguments keep every current call identical.

### Task 3: Loader and seed JSON

- **Files**: `src/data/judgment_constants_loader.cpp`, `assets/data/judgment_constants.json`
- **Action**: UPDATE
- **Implement**:
  - In `load_judgment_constants`, inside the `try` after `tiers_it`, read the top-level key `"merciful_beginner"`. Absent or
    `null` keeps the default. A non-boolean value throws `"field 'merciful_beginner' must be a boolean"`, which becomes a
    fallback. Mirror lines 117-123.
  - JSON: add `"merciful_beginner": true` at the top level, after `"label_note"` and before `"windows_seconds"`. Append to
    `"source"`: `"; merciful_beginner is MercifulBeginner=1 from metrics.ini:157 [Preferences] (compiled default false at src/PrefsManager.cpp:128)"`.
- **Validate**: build, then `judgment_constants_test`. Seed parity (§6) still passes because both sides are `true`; Task 7 adds
  the field to `same_constants`.

### Task 4: Engine — Beginner windows, early Way Off suppression, display-only queue

- **Files**: `src/gameplay/judgment_engine.hpp`, `src/gameplay/judgment_engine.cpp`
- **Action**: UPDATE
- **Implement**:
  - New members: `bool is_beginner_ = false;` and `std::vector<JudgmentEvent> display_only_events_;`.
  - `reset()`: clear `display_only_events_`, then set `is_beginner_ = chart_ != nullptr && chart_->is_beginner();`. With a
    null chart it is false.
  - Replace every `constants_->effective_windows()` (`handle_step_mine`, `expire_notes`, `update_holds`) with
    `constants_->effective_windows(is_beginner_)`. Only `way_off` differs, so mine and hold timing are unchanged by
    construction. Keep a single source of window truth.
  - `handle_step_tap`: classify with `constants_->classify_tap(delta_seconds, is_beginner_)`. After the `Miss` early return,
    look up `note`. If `judgment == TapJudgment::WayOff && constants_->merciful_beginner_applies(is_beginner_) &&
    hit_time < note.time_seconds`, build the event exactly like the recorded one (`kind=Tap`, `window=WayOff`, signed
    `delta_ms`, `note_index`, `column`, `note_type`), push it to `display_only_events_`, and `return` **before** touching
    `state`. Comment it with `Player.cpp:1089-1093` (only `m_Judgment.SetJudgment`, `tns` unset) and `:929` (early ⇔
    `fNoteOffset > 0` ⇔ `hit < note`).
  - Public API (header, documented next to `drain_new_events`):
    `void drain_display_only_events(std::vector<JudgmentEvent>& out);` appends and then clears the queue. Document that these
    events are presentation-only, never in `events()` or `latest_event()`, and must never reach score, life or results.
    Optionally add `[[nodiscard]] bool is_beginner() const { return is_beginner_; }` for tests and logging.
  - In the class comment ("owns the append-only judgment log and nothing else"), add a sentence about the display-only side
    channel.
- **Mirror**: `judgment_engine.cpp:109-137,450-456`
- **Validate**: build, then the existing `judgment_engine_test` still passes. Its charts are no longer Beginner after Task 1.

### Task 5: `ScoreKeeper` — clamp negative weights on Beginner

- **Files**: `src/gameplay/score_keeper.hpp`, `src/gameplay/score_keeper.cpp`
- **Action**: UPDATE
- **Implement**:
  - New member `bool merciful_ = false;`. In `reset()`, after storing the pointers, set
    `merciful_ = chart_ != nullptr && constants_ != nullptr && constants_->merciful_beginner_applies(chart_->is_beginner());`.
    Reset it to false at the top as well.
  - Private helper `[[nodiscard]] int dp_weight(int weight) const { return merciful_ ? std::max(0, weight) : weight; }`. Cite
    `ScoreKeeperMAX2.cpp:529-530,544-545` and note that course mode is excluded upstream (`:187-189`), which does not apply
    because Blaze has no courses.
  - Apply it to the `tap_weight()` result (keep `tap_weight` returning the raw table value and wrap at the use site in
    `resolve_row`, or wrap inside `tap_weight`; either works, pick one), to the `HitMine` add (line 80), and to the
    `hold_ok`/`hold_ng` adds (lines 173, 177).
  - **Do not** clamp `possible_dp` (it is positive anyway, and upstream passes `false`, `ScoreKeeperMAX2.cpp:449-451`). Counts,
    combo and miss_combo stay untouched (S5). Add a comment that Blaze grades from DP percent, so this also covers the
    grade-weight clamp (`:570,585`; `PlayerStageStats.cpp:164-179`).
- **Validate**: build, then `score_keeper_test` (existing §13/§15 still give −12, because the charts are not Beginner).

### Task 6: `GameplayView` — show the suppressed early Way Off

- **Files**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - New member `std::vector<JudgmentEvent> display_events_;` next to `new_events_`.
  - In `update()`, right after `judge_.drain_new_events(new_events_);`, add `display_events_.clear();
    judge_.drain_display_only_events(display_events_);`. Then call `judge_anim_.consume(display_events_);` **before** the
    existing `judge_anim_.consume(new_events_);`, so a recorded judgment in the same tick wins the popup. Do **not** pass
    `display_events_` to `score_`, `life_`, or `arm_explosion` (OpenITG does not call `DidTapNote` here).
  - In the `exited_` early-return branch, nothing changes (input is closed after fail).
  - Optionally add `", merciful beginner " << (on ? "on" : "off")` to the init log line (`gameplay_view.cpp:93-97`).
- **Validate**: build, then the full sandboxed ctest stays at 38/38.

### Task 7: Constants tests

- **File**: `tests/judgment_constants_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `same_constants`: also compare `a.merciful_beginner != b.merciful_beginner`.
  - §1 parity: `TEST_CHECK(defaults.merciful_beginner);` and
    `TEST_CHECK(nearly(blaze4k::JudgmentConstants::kMercifulBeginnerWayOffBonusSeconds, 0.5));`.
  - **§13 MercifulBeginner windows** (`Player.cpp:49-56`):
    - `effective_windows(false)` equals `effective_windows()` field by field.
    - `effective_windows(true).way_off` is about `0.6815`, and every other field equals `effective_windows()`, including
      `hit_mine`, `hold_ok`, `hold_roll` and `pad_stick`.
    - Bonus after scale/add: with `judge_window_scale = 2.0` and `judge_window_add = 0.01`, `effective_windows(true).way_off`
      is about `0.18*2 + 0.01 + 0.5 = 0.87`.
    - Flag off: with `merciful_beginner = false`, `effective_windows(true).way_off == effective_windows().way_off`.
    - `classify_tap(0.6815, true) == WayOff` (inclusive edge, computed from `effective_windows(true).way_off`).
      `classify_tap(that + 1e-6, true) == Miss`. `classify_tap(0.5) == Miss` (non-Beginner control).
      `classify_tap(0.01, true) == Fantastic`. `classify_tap(-0.5, true) == WayOff` (symmetric).
  - **§14 Loader flag**: `{"merciful_beginner": false}` loads with status `LoadedFromFile`, gives `!merciful_beginner`, and
    leaves the other fields at their defaults. `{"merciful_beginner": null}` keeps `true`. `{"merciful_beginner": 1}` and
    `{"merciful_beginner": "yes"}` fall back to the defaults with a non-empty warning message.
- **Mirror**: `tests/judgment_constants_test.cpp:191-213` (§2) and `:237-270` (§5)
- **Validate**: sandboxed `ctest --test-dir build -R judgment_constants_test --output-on-failure`

### Task 8: Engine tests (§19)

- **File**: `tests/judgment_engine_test.cpp`
- **Action**: UPDATE (add `// 19. MercifulBeginner (#67; OpenITG Player.cpp:55-56,1089-1093,1710-1713).` before the final
  success print)
- **Implement**: Add a local helper `make_chart(const char* difficulty)`, or set `chart.difficulty = "Beginner"` / `"Medium"`
  inline. Use `note_time = 2.0` and `const double bw = k.effective_windows(true).way_off;` (0.6815). Each sub-case uses a fresh
  chart and engine:
  - 19.1 **Late Way Off widened**: Beginner, step at `2.4` gives 1 `Tap`/`WayOff` event with `delta_ms` about 400. Control
    `Medium`: same step gives no events.
  - 19.2 **Miss expiry delayed**: Beginner, `update(2.0 + w.way_off + 0.01)` gives no events, then `update(2.0 + bw + 1e-6)`
    gives exactly 1 `Miss` with `hit_time ≈ 2.0 + bw` and `delta_ms ≈ bw*1000`. Control `Medium`: a Miss already after
    `2.0 + w.way_off + 0.01`.
  - 19.3 **Early Way Off is display-only, note stays live**: Beginner, step at `1.6` → `events().empty()`,
    `!is_note_judged(0)`, `!is_note_hidden(0)`, `latest_event() == nullptr`. `drain_display_only_events` gives exactly 1 event
    (`kind Tap`, `window WayOff`, `delta_ms ≈ -400`, `note_index 0`, `column` correct). A second drain gives nothing, and
    `drain_new_events` gives nothing. Then step at `2.0` → 1 recorded `Fantastic`.
  - 19.4 **Early Way Off then untouched** gives exactly one `Miss` at `2.0 + bw + 1e-6`.
  - 19.5 **Only early Way Off is suppressed**: Beginner, early inside the base Boo band (step `1.85`) is display-only. Early
    Decent (step at `2.0 - (w.great + w.decent)/2`) is recorded as `Decent`. Late base-band Way Off (step `2.15`) is recorded
    as `WayOff`.
  - 19.6 **Hold head early Way Off**: Beginner `HoldHead` at 2.0 (end 4.0), step `1.7` → no events,
    `!is_hold_head_hit(0)`, `!is_hold_in_progress(0)`. Step `2.0` gives a recorded `Fantastic`, and
    `update(4.0, held_col(0))` gives `HoldOk`.
  - 19.7 **Mine window not widened**: Beginner mine at 2.0, step at `2.0 + w.hit_mine + 0.01` → no `HitMine`.
  - 19.8 **Flag off**: `JudgmentConstants off = compiled_defaults(); off.merciful_beginner = false;` on a Beginner chart. Step
    `1.85` gives a recorded `WayOff`, step `2.4` on a fresh note gives no event, and a Miss comes at the base window. In other
    words it behaves exactly like the control.
  - 19.9 **Label matching**: `"beginner"` and `"BEGINNER"` behave as Beginner (repeat the 19.1 late-step check). `"Novice"`,
    `"Easy"` and `""` do not.
  - 19.10 **Reset clears the queue**: produce a display-only event, call `reset()` again with the same chart, and the drain
    then gives nothing.
- **Mirror**: `tests/judgment_engine_test.cpp:785-868` (§17)
- **Validate**: sandboxed `ctest --test-dir build -R judgment_engine_test --output-on-failure`

### Task 9: Score keeper tests (§17, §18)

- **File**: `tests/score_keeper_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - **§17 Clamp**. The chart has taps at beats 1, 2, 3 and 4 (cols 0-3), a hold head at 5 (end 6), and a mine at 7.
    `tap_count = 4`, `hold_count = 1`. Events via `make_tap`/`make_hold_outcome`: row1 `WayOff`, row2 `Miss`
    (`JudgmentKind::Miss`), row3 `Fantastic`, row4 `Decent`, hold head `Fantastic`, `HoldNg`, and a `HitMine` event on the mine.
    - `"Beginner"`: `actual_dp == 10` (0+0+5+0+5+0+0), `possible_dp == 30`, percent ≈ 1/3. Counts are still WayOff 1, Miss 1,
      HitMine 1, NG 1. `max_combo == 1` and combo matches the control.
    - Control `"Hard"`: `actual_dp == -14` (−6−12+5+0+5+0−6), same `possible_dp`, counts and combo.
    - `"Beginner"` with `merciful_beginner = false`: `actual_dp == -14`.
    - Hold clamp proof: constants with `dp_weights.hold_ng = -3` give `"Beginner"` +0 and `"Hard"` −3 for one `HoldNg`.
    - Positive weights are never clamped down: a Beginner all-Fantastic run still gives `actual == possible`.
  - **§18 GameplayView boundary on Beginner**: copy §15 (`score_keeper_test.cpp:638-661`) with `chart.difficulty = "Beginner"`
    and **150** frames (2.5 s; expiry is now at 1.6815 s). Expect `dance_points() == 0`, `score_percent() == 0.0` (§15 had
    −2.4), and Miss count 1.
- **Validate**: sandboxed `ctest --test-dir build -R score_keeper_test --output-on-failure`

### Task 10: Life control and parser/label tests

- **Files**: `tests/life_keeper_test.cpp`, `tests/note_parser_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `life_keeper_test` **§19**: two one-tap charts, identical except `difficulty` (`"Beginner"` vs `"Medium"`). Consuming the
    same `Miss` event, and separately the same `WayOff` tap event, gives identical `life()` in both. This pins that life has
    no Beginner branch (`LifeMeterBar.cpp`).
  - `note_parser_test` §2 (after line 107): `TEST_CHECK(multi_parser.charts()[0].is_beginner());` and
    `TEST_CHECK(!multi_parser.charts()[1].is_beginner());`. Add a small section for the helper:
    `is_beginner_difficulty("beginner"/"Beginner"/"BEGINNER")` is true, `("Novice")`, `("Easy")` and `("")` are false, and
    `blaze4k::Chart{}.difficulty.empty()` with `!blaze4k::Chart{}.is_beginner()`.
- **Validate**: sandboxed `ctest --test-dir build -R "life_keeper_test|note_parser_test" --output-on-failure`

### Task 11: Full validation and smoke

- Run every command in **Validation** and **End-to-End Verification** (automated parts). Expect 38/38 and no new warnings.

### Task 12: File the Beginner fail-type follow-up (only if the owner agrees, Open Question 2)

- `gh issue list --state all --search "FailOffInBeginner"` must return nothing first. If GraphQL is rate-limited, use
  `gh api "search/issues?q=repo:laanhema/blaze4k+FailOffInBeginner"`.
- Then `gh issue create --title "Beginner/Easy fail-type rules (OpenITG FailOffInBeginner / FailOffForFirstStageEasy)" --label gameplay --label bug --body-file <scratch>`.
  The body cites `metrics.ini:40-41` and `GameState.cpp:1405-1443`. Do **not** implement it here.

---

## Validation

```bash
# Build (warnings in touched files = lint gate; no linter configured)
cmake --build build -j$(nproc) 2>&1 | grep -E "warning|error"   # expect no output for touched files
cmake --build build -j$(nproc)

# Tests: expect 38/38. MUST be sandboxed (audio_test touches real hardware; no sound, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Targeted
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build -R "judgment_constants_test|judgment_engine_test|score_keeper_test|life_keeper_test|note_parser_test|gameplay_screen_test|results_screen_test|metronome_sync_test" --output-on-failure

# Every engine window read goes through the Beginner-aware overload (expect no output)
grep -n "effective_windows()" src/gameplay/judgment_engine.cpp

# Display-only events never reach score/life (expect only the judge_anim_ line and the drain call)
grep -n "display_events_" src/gameplay/gameplay_view.cpp

# Scope guard
git diff --stat
git status --short .agents/stories/todo-stories.md   # still "??", untouched
```

## End-to-End Verification

1. **Automated (headless, required):** `judgment_engine_test` §19 drives the real engine through every behavior: the widened
   late window, delayed expiry, the display-only early Way Off with the note staying live, hold heads, the unchanged mine
   window, the flag being off, and label matching. `score_keeper_test` §18 runs the real `GameplayView`, including
   `init(chart, …)`, so the chart difficulty flows into both engine and keeper through production wiring, and an all-miss
   Beginner run ends at 0 DP / 0% instead of −2.4.
2. **App smoke (headless, required):**
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 10`
   exits 0, and the log shows **no** `[JudgmentConstants] Warning` fallback line (the shipped JSON with `merciful_beginner`
   validates). Never launch a windowed or interactive binary from the agent.
3. **Owner play-test (interactive; the agent does not run it):** play the Beginner chart of *Blaze Anthem*
   (`tests/fixtures/reference_pack/…`, or any pack with a Beginner chart):
   - Step about 0.3 s early on an arrow. "WAY OFF" pops, the arrow keeps scrolling, combo and life are unchanged, and hitting
     it on time afterwards scores normally.
   - Step about 0.4 s late. A recorded Way Off appears, and the percent does not drop.
   - Miss everything with Fail Off. Results show 0.00% (not negative).
   - Play an Easy chart the same way. Normal (harsh) behavior.
   - Setting `"merciful_beginner": false` in the user's `judgment_constants.json` makes Beginner behave like Easy, with no
     rebuild.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| `Chart::difficulty` defaults to `"Beginner"`, so every hand-built test chart (judgment, score, life, gameplay, music-clock, note-field tests) would turn merciful and break existing expectations | Task 1 changes the default to `""` (D1). This is the only place a default label is used, and the parser always sets it | In scope |
| The SSC parser defaults a missing `#DIFFICULTY` to `"Beginner"` (`simfile_parser.cpp:123,131`), so such charts become merciful | Kept as is: it is pre-existing passthrough behavior, and the label already shows "Beginner" in select. StepMania 5 infers difficulty from the meter instead. Flag only | Out of scope (flag) |
| A display-only event leaking into score/life would double-penalize or mis-count | A separate queue that is never in `events()`. `GameplayView` routes it only to `judge_anim_`. A grep check in Validation, and §19.3 asserts `drain_new_events` stays empty | In scope |
| Feeding `effective_windows(true)` back in would add the bonus twice | Documented in the header. Only `classify_tap` and the engine call it, always on `constants_` | In scope |
| Popup ordering when a display-only and a recorded judgment land in the same tick | Display-only is consumed first, so the recorded one wins. Purely cosmetic | In scope |
| A Beginner Fail-Off all-miss run now records a 0% / D high score (it was rejected before as a negative percent) | Faithful to OpenITG (S6). Documented and not guarded | In scope (accepted) |
| OpenITG's SM loader turns a chart into Challenge when its *description* is `smaniac`/`challenge` (`NotesLoaderSM.cpp:35-42`), even with a Beginner label | Not mirrored: Blaze displays labels as passthrough, and this case is vanishingly rare (Open Question 1) | Out of scope (flag) |
| OpenITG `Judgment.cpp:32-34` uses a separate "BeginnerLabel" judgment graphic | Presentation only, not part of the issue | Out of scope |
| Seconds vs note rows: OpenITG expiry is row-quantized | Pre-existing difference (documented in #57). Unchanged by this work | Out of scope (flag) |
| Found while verifying: OpenITG also sets `FailOffInBeginner=1` / `FailOffForFirstStageEasy=1` (`metrics.ini:40-41`), and Easy/Beginner are never harsher than `FAIL_END_OF_SONG` (`GameState.cpp:1426-1439`). Blaze applies the user's fail setting to every difficulty | Not part of #67. Proposed follow-up issue (Task 12, Open Question 2) | Out of scope (follow-up) |

---

## Decisions

- **D1 — `Chart::difficulty` defaults to `""`.** OpenITG `Steps` starts at `DIFFICULTY_INVALID`, which is not Beginner. This
  keeps every existing synthetic test chart on the non-merciful path. The alternative, editing dozens of test charts, is
  noisier and easy to miss.
- **D2 — Beginner status comes from the chart already passed to `reset()`.** There are no signature changes, the engine and
  keeper cannot disagree, and the AC "difficulty reaches the judgment engine and the score keeper" is met by construction.
- **D3 — One flag, `merciful_beginner`, at the top level of `JudgmentConstants` and the JSON.** It gates windows (timing) and
  weights (scoring), so it belongs in neither sub-table. It mirrors the single OpenITG preference.
- **D4 — The early Way Off goes to a side channel, not the log.** The AC requires "no judgment event", and the event-sourced
  contract (AGENTS.md pattern 2) means everything in the log is scored. The side channel preserves OpenITG's visible
  feedback.
- **D5 — Grade clamp is implemented through the DP clamp.** Blaze has no separate grade-weight accumulation, because
  `grade_weights` are loaded but grade derives from DP percent, and the two tables are identical. If grade weights ever get
  their own path, the same clamp must be applied there (note this in the `ScoreKeeper` comment).
- **D6 — No new test target.** The suite stays at 38.

## Open Questions

1. **Mirror the SM description hack** (`smaniac`/`challenge` description turns the chart into Challenge, `NotesLoaderSM.cpp:35-42`)
   in `is_beginner()`? *Proposed default:* no. Labels are displayed as passthrough, so a chart shown as "Beginner" that judges
   as Challenge would confuse players. The case is practically nonexistent.
2. **File the Beginner/Easy fail-type follow-up** (`FailOffInBeginner=1`, `FailOffForFirstStageEasy=1`,
   `GameState.cpp:1405-1443`)? *Proposed default:* yes, as a Low-priority bug (Task 12). It is a separate parity gap that
   interacts with the user-facing fail option, so it needs its own design.
3. **Should a user be able to toggle MercifulBeginner from the Options menu?** *Proposed default:* no. It stays data-driven
   only (JSON), like every other judgment constant. This is consistent with PRD pattern 3 and with the AC wording.

---

## Acceptance Criteria

- [ ] Chart difficulty (Beginner or not) reaches `JudgmentEngine` and `ScoreKeeper` (D2; §18 proves it through `GameplayView::init`)
- [ ] On Beginner, the effective Way Off is `way_off*scale + add + 0.5 s` for classification and miss expiry. Other windows
      and other difficulties are unchanged (§13, §19.1, §19.2, §19.7, controls)
- [ ] On Beginner, an early Way Off produces no judgment event and the note stays steppable and missable (§19.3-§19.6)
- [ ] On Beginner, negative DP/grade weights (Way Off, Miss, hit mine, negative NG) count as 0. `possible_dp`, counts, combo
      and life are unchanged (§17, life §19)
- [ ] `merciful_beginner` flag (default `true`, in the seed JSON and compiled defaults) turns everything off (§14, §19.8, §17)
- [ ] Unit tests cover each behavior plus the non-Beginner control
- [ ] Build has no new warnings in touched files. 38/38 tests pass (sandboxed). Headless smoke exits 0 with no constants fallback warning
- [ ] `.agents/stories/todo-stories.md` untouched
