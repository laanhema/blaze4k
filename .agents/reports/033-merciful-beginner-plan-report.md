# Implementation Report

**Plan**: `.agents/plans/completed/033-merciful-beginner-plan.md`
**Branch**: `feature/033-merciful-beginner`
**Status**: COMPLETE
**GitHub Issue**: #67

## Summary

Implemented OpenITG `MercifulBeginner` parity (pinned to OpenITG `f2c129fe`). On a chart whose difficulty label is a
case-insensitive `"beginner"`, and while the new data-driven flag `JudgmentConstants::merciful_beginner` is on (default
`true`, JSON key `"merciful_beginner"`):

1. The effective Way Off window gets +0.5 s, added after `base * scale + add` (`Player.cpp:55-56`). It applies to tap
   classification and to miss/avoided-mine expiry. Mine, hold and roll windows are unchanged.
2. An **early** step that grades as Way Off is display-only (`Player.cpp:1089-1093`). The engine pushes it to a separate
   queue (`JudgmentEngine::drain_display_only_events`) instead of the judgment log. The note stays live, so it can still be
   stepped or missed. `GameplayView` feeds that queue to the judgment popup only.
3. `ScoreKeeper` clamps negative DP weights to 0 for tap rows, hit mines and hold outcomes (`ScoreKeeperMAX2.cpp:529-530,544-545`).
   `possible_dp`, counts, combo and life are unchanged. Grade derives from DP percent, so the grade clamp is covered too.

Beginner status comes from the `Chart*` that `reset()` already receives (`Chart::is_beginner()`), so no signatures changed.
`Chart::difficulty` now defaults to `""` (OpenITG `DIFFICULTY_INVALID`), so hand-built charts are not Beginner by accident.
Every parser path still sets the label.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `is_beginner_difficulty`, `Chart::is_beginner()`, empty default label | `src/chart/chart.hpp` | ✅ |
| 2 | `merciful_beginner` flag, bonus constant, `merciful_beginner_applies`, `effective_windows(bool)`, `classify_tap(double, bool)` | `src/timing/judgment_constants.{hpp,cpp}` | ✅ |
| 3 | JSON `merciful_beginner` loader override and seed value | `src/data/judgment_constants_loader.cpp`, `assets/data/judgment_constants.json` | ✅ |
| 4 | Engine: Beginner windows, early Way Off suppression, display-only queue | `src/gameplay/judgment_engine.{hpp,cpp}` | ✅ |
| 5 | ScoreKeeper: clamp DP weights to ≥ 0 on merciful Beginner | `src/gameplay/score_keeper.{hpp,cpp}` | ✅ |
| 6 | GameplayView: display-only events go to the popup only; init log line | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| 7 | Constants tests §1, §13, §14, `same_constants` | `tests/judgment_constants_test.cpp` | ✅ |
| 8 | Engine tests §19.1-§19.10 | `tests/judgment_engine_test.cpp` | ✅ |
| 9 | Score tests §17 (clamp), §18/§18b (GameplayView boundary) | `tests/score_keeper_test.cpp` | ✅ |
| 10 | Life control §19, parser/label tests | `tests/life_keeper_test.cpp`, `tests/note_parser_test.cpp` | ✅ |
| 11 | Full validation and headless smoke | (none) | ✅ |
| 12 | File the fail-type follow-up issue | (none) | Skipped (orchestrator decision, Open Question 2) |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, GCC 16.2.1, `-Wall -Wextra -Wpedantic`) | ✅ no warnings or errors (this is the lint gate; no separate linter) |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 38/38 passed |
| Targeted ctest (8 suites) | ✅ 8/8 passed |
| `grep -n "effective_windows()" src/gameplay/judgment_engine.cpp` | ✅ no output |
| `grep -n "display_events_" src/gameplay/gameplay_view.cpp` | ✅ only clear, drain and `judge_anim_.consume` |
| Headless smoke (`bwrap … ./build/blaze-4k --headless --smoke-test 10`) | ✅ exit 0. Log shows `[JudgmentConstants] Loaded 'assets/data/judgment_constants.json'` and no fallback warning |
| `.agents/stories/todo-stories.md` | ✅ untouched (still `??`) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `assets/data/judgment_constants.json` | UPDATE | +2/-1 |
| `src/chart/chart.hpp` | UPDATE | +27/-1 |
| `src/data/judgment_constants_loader.cpp` | UPDATE | +8/-0 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +7/-1 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +3/-0 |
| `src/gameplay/judgment_engine.cpp` | UPDATE | +43/-16 |
| `src/gameplay/judgment_engine.hpp` | UPDATE | +19/-0 |
| `src/gameplay/score_keeper.cpp` | UPDATE | +6/-4 |
| `src/gameplay/score_keeper.hpp` | UPDATE | +11/-1 |
| `src/timing/judgment_constants.cpp` | UPDATE | +15/-2 |
| `src/timing/judgment_constants.hpp` | UPDATE | +28/-1 |
| `tests/judgment_constants_test.cpp` | UPDATE | +95/-0 |
| `tests/judgment_engine_test.cpp` | UPDATE | +223/-0 |
| `tests/life_keeper_test.cpp` | UPDATE | +31/-0 |
| `tests/note_parser_test.cpp` | UPDATE | +16/-0 |
| `tests/score_keeper_test.cpp` | UPDATE | +177/-0 |

## Deviations from Plan

- **Task 12 skipped.** By orchestrator decision, no fail-type follow-up issue was filed. Open Question 2 goes back to the owner
  as a suggestion: OpenITG `FailOffInBeginner=1` / `FailOffForFirstStageEasy=1` (`metrics.ini:40-41`, `GameState.cpp:1405-1443`).
- **Open Questions 1 and 3** were resolved as the plan proposed: no SM description→Challenge override, and the flag is JSON-only
  with no Options toggle.
- **`ScoreKeeper::reset`**: the plan asked to reset `merciful_` to false at the top and then assign it. Only the single
  assignment was kept, because the assignment already covers every path (null chart or null constants give false). Behavior is
  identical.
- **Added tests beyond the plan** (no behavior change):
  - §19.7 also checks that avoided-mine expiry waits for the widened Way Off (S4, `Player.cpp:1397-1413`).
  - §18b drives an early Way Off through the real `GameplayView::handle_input_events` path and asserts it never reaches the
    log or the keeper.
  - The note_parser label test also rejects `"beginners"`.
- **Init log line**: the optional `merciful beginner on/off` suffix was added to the `[GameplayView]` speed/options line.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/judgment_constants_test.cpp` | §1 flag and bonus parity. §13: `effective_windows(false/true)` field by field, bonus after scale/add, flag off, `classify_tap` Beginner edges (inclusive, +1e-6 → Miss, symmetric, NaN), `merciful_beginner_applies`. §14: JSON flag false, null, and non-boolean (`1`, `"yes"`) → fallback with warning. `same_constants` compares the flag |
| `tests/judgment_engine_test.cpp` | §19.1 late Way Off widened, with Medium control. §19.2 delayed Miss expiry, with control. §19.3 early Way Off display-only, the note stays live, the drain is one-shot, and a later Fantastic is recorded. §19.4 early then untouched gives exactly one Miss. §19.5 early base-band Way Off suppressed, early Decent and late Way Off recorded. §19.6 hold head early Way Off then a normal hit and HoldOk. §19.7 mine window not widened, avoided-mine expiry delayed. §19.8 flag off. §19.9 label matching. §19.10 reset clears the queue |
| `tests/score_keeper_test.cpp` | §17: Beginner DP 10 vs Hard −14 with identical counts and combo; flag off gives −14; negative `hold_ng` clamp; all-Fantastic Beginner = 100%. §18: GameplayView all-miss Beginner gives 0 DP and 0%, with the expiry not yet reached at 1.5 s. §18b: an early Way Off via `handle_input_events` stays display-only |
| `tests/life_keeper_test.cpp` | §19: Miss and Way Off life deltas are identical on Beginner and Medium |
| `tests/note_parser_test.cpp` | §2: `is_beginner()` on parsed charts. §8: `is_beginner_difficulty` case-insensitive and exact; `Chart{}` default is empty and not Beginner |

## End-to-End

- Automated headless E2E: engine §19 and the GameplayView boundary §18/§18b pass. Headless app smoke exits 0 with no constants
  fallback warning.
- The owner play-test (interactive, plan E2E step 3) has not been run. The agent does not launch interactive or windowed binaries.
