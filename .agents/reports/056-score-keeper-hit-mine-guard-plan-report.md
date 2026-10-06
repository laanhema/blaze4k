# Implementation Report

**Plan**: `.agents/plans/completed/056-score-keeper-hit-mine-guard-plan.md`
**Branch**: `feature/056-score-keeper-hit-mine-guard`
**Status**: COMPLETE
**GitHub Issue**: #119

## Summary

`ScoreKeeper` now applies a `HitMine` event at most once per mine note, matching `LifeKeeper::consume_hit_mine` (#118). The `HitMine` case in `consume()` delegates to a new private helper `ScoreKeeper::apply_hit_mine`, which uses the same guards in the same order: bounds check, then `NoteType::Mine` check, then the per-note `note_scored_` duplicate guard, then mark, then apply −6 DP (through the `dp_weight` MercifulBeginner clamp) and one `HitMine` tally. Reusing `note_scored_` is safe because mines have `note_row_ == -1` and `apply_tap_like` never marks them. Combo is still unaffected and `AvoidedMine` stays stats-only. No values changed. Real play is unaffected, because `JudgmentEngine` emits one `HitMine` per mine.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Guarded `apply_hit_mine` helper; `HitMine` case delegates to it; `note_scored_` comment updated | `src/gameplay/score_keeper.hpp`, `src/gameplay/score_keeper.cpp` | ✅ |
| 2 | `make_hit_mine` helper + block 9b (duplicate / foreign / out-of-range / negative `HitMine`) | `tests/score_keeper_test.cpp` | ✅ |
| 3 | Verified existing `HitMine` cases target real mines (block 6 → idx 2, block 9 → idx 1, block 17 → idx 5; all `NoteType::Mine`) | `tests/score_keeper_test.cpp` | ✅ (no edit needed) |
| 4 | Full suite + headless smoke | — | ✅ |

## Validation Results

All test runs used the sandbox prefix
`bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net` (no real audio hardware touched).

| Check | Result |
|-------|--------|
| Build `cmake --build build -j8` (GCC, `-Wall -Wextra -Wpedantic`; touched files rebuilt) | ✅ 0 warnings, 0 errors |
| Lint | N/A (no linter configured; warnings gate above) |
| TDD red step (Task 1 stashed) | ✅ 9b aborted as expected at `score_keeper_test.cpp:513: keeper.actual_dance_points() == 0` |
| Direct `./build/tests/score_keeper_test` (sandboxed) | ✅ exit 0; prints the 9b line |
| Targeted ctest (score_keeper, life_keeper, judgment_engine, results, results_screen, gameplay_screen) | ✅ 6/6 |
| Full ctest (sandboxed) | ✅ 51/51 (2.89 s) |
| Values untouched (`git diff --exit-code` on judgment_constants.json, judgment_constants.cpp, life_keeper.cpp) | ✅ no diff |
| Scope guard (`git diff --stat`) | ✅ only the 3 planned files |
| E2E headless smoke `env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10` (sandboxed) | ✅ exit 0 ("Smoke test finished (10 frames). Exiting cleanly.") |
| Optional `/verify` visual check | Not run (optional per plan; real-play event stream is unchanged) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/score_keeper.hpp` | UPDATE | +2/-1 |
| `src/gameplay/score_keeper.cpp` | UPDATE | +18/-3 |
| `tests/score_keeper_test.cpp` | UPDATE | +60/-0 |

## Deviations from Plan

None. Open Question 1 was resolved to the plan's default helper name, `apply_hit_mine`. The three existing inline `HitMine` constructions were left as they are, which the plan allows for keeping the diff small. The optional Beginner sub-case in 9b was skipped, which the plan also allows, because block 17 already covers the clamp.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/score_keeper_test.cpp` | Block 9b: `HitMine` on a tap's index, idx 99 and idx −1 are ignored (DP 0, tally 0, combo 0, incomplete); the tap's real Fantastic still scores afterward (DP 5, combo 1, complete), which proves the guard slot was not used up; the first real mine hit gives DP −1, tally 1, combo/max 1; a duplicate gives the same values and percent −0.2 |
