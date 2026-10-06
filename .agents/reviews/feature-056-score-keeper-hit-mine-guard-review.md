# Code Review: feature/056-score-keeper-hit-mine-guard

**Scope**: branch `feature/056-score-keeper-hit-mine-guard` vs merge base `f87428f` (no commits on the branch; uncommitted changes to 3 tracked files, plus untracked plan and implementation report), checked against issue #119
**Recommendation**: APPROVE (no findings)

## Summary

The change adds a private `ScoreKeeper::apply_hit_mine` (`src/gameplay/score_keeper.cpp:187-202`) that runs the same guards in the same order as `LifeKeeper::consume_hit_mine` (`src/gameplay/life_keeper.cpp:158-171`): bounds check, then `NoteType::Mine` check, then the `note_scored_` duplicate guard. Only after those does it apply `dp_weight(hit_mine)` and one `HitMine` tally. The `HitMine` case in `consume()` now delegates to it (`score_keeper.cpp:81-83`). A new test block 9b (`tests/score_keeper_test.cpp:490-538`) covers the duplicate, foreign (tap index), out-of-range and negative cases. All five acceptance criteria of #119 are met, and every file was reviewed in full.

### Acceptance criteria

| AC | Status | Evidence |
|----|--------|----------|
| 1. −6 DP and `HitMine` tally at most once per mine | Met | `score_keeper.cpp:195-201`; 9b duplicate step (`score_keeper_test.cpp:530-535`) |
| 2. Out-of-range / non-mine index ignored without using up the guard slot; the real tap still scores | Met | Bounds and type checks run before the mark (`score_keeper.cpp:189-194`); 9b checks a tap Fantastic after a foreign `HitMine` on its index (`score_keeper_test.cpp:510-523`) |
| 3. Unit test: duplicate, out-of-range and foreign `HitMine` (DP, tally, combo unchanged after the first hit) | Met | Block 9b checks DP, tally, combo and max_combo after each step |
| 4. Existing `HitMine` cases target real mines | Met | Block 6 idx 2, block 9 idx 1 and block 17 idx 5 are all `NoteType::Mine` (`score_keeper_test.cpp:345/361`, `:459/471`, `:781/796`) |
| 5. All tests pass (51/51, sandboxed ctest) | Met | See Validation Results |

Reusing `note_scored_` is sound. Mines keep `note_row_ == -1` (`score_keeper.cpp:37-38`), and `apply_tap_like` marks a slot only after its `row < 0` early return (`:106-110`), so the tap path never writes a mine slot. `is_complete()` does not read `note_scored_`, so marking a mine cannot change completeness. `chart_` and `constants_` are non-null here because of the early return in `consume()` (`:64-66`). Combo is untouched, and `AvoidedMine`/`RollHit` stay no-ops.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

**Noted, not a finding**: the issue's technical note suggested the name `consume_hit_mine`. The plan settled on `apply_hit_mine` to match ScoreKeeper's local `apply_tap_like`/`apply_hold` convention, and the implementation report and issue comment both say so. The plan also allowed skipping the optional Beginner sub-case in 9b, and block 17 already covers the MercifulBeginner clamp on the mine path.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build -j8` (Release, GCC, host) |
| Warnings | NONE | Real gate: recompiled `src/gameplay/score_keeper.cpp` (flags from `blaze4k_core`) and `tests/score_keeper_test.cpp` (flags from `score_keeper_test`) with `-O3 -std=c++20 -Wall -Wextra -Wpedantic -c -o /dev/null`. Zero diagnostics. |
| Lint | N/A | No linter configured |
| Tests | PASS | 51/51, 0 skipped. Sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` (`/dev/snd` and the runtime dir were empty inside the sandbox). A direct sandboxed `build/tests/score_keeper_test` prints the 9b line. No `GTEST_SKIP`/`[SKIP]` guards exist. Test 2 logs missing-asset font/theme fallbacks, which predate this change and still run every assertion. |

## What's Good

- The guard order and comment wording match `LifeKeeper::consume_hit_mine` exactly, so the two keepers are now provably consistent per mine.
- The fix is minimal: the MercifulBeginner `dp_weight` clamp and the never-changes-combo citation are kept, and no constants or other scoring paths change.
- Test 9b proves the subtle part of AC 2, that a rejected foreign `HitMine` does not use up the tap's guard slot, by scoring the tap afterward. It also checks `is_complete()` before and after.

## Recommendation

Ready for PR and merge as is.
