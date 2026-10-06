# Code Review: feature/055-mine-damage-openitg-parity

**Scope**: branch `feature/055-mine-damage-openitg-parity` vs merge base `246add8` (main), all changes uncommitted (4 modified files plus the untracked plan and implementation report), checked against issue #118
**Recommendation**: APPROVE WITH NITS

## Summary

Spike #118 confirms that Blaze 4k's mine values match OpenITG and fixes the two small mismatches it found. M1: `LifeKeeper` now sends `HitMine` through a guarded `consume_hit_mine`, so each mine takes life at most once. M2: the `hot_downgrade` provenance comment is corrected. The code change is small, follows the existing `consume_hold_outcome` guard pattern, and is pinned by six new test blocks (20a–20f), two of which drive the real `JudgmentEngine`. I fetched the upstream citations again at OpenITG `f2c129fe`, and they are accurate: `LifeMeterBar.cpp:119` and `:175` hold the `-0.10f` literals, `Player.cpp:1096` holds `tn.result.tns = score`, and `Player.cpp:1130-1134` judges rows from taps and hold heads only. Every file was reviewed in full.

### Acceptance criteria

| AC | Status | Evidence |
|----|--------|----------|
| 1. Mine delta (−0.05) and hot override (−0.10) checked against OpenITG with file:line on the issue | Met | Verification Report comment on #118, AC 1 table |
| 2. Side effects (regain debt, DP −6) compared | Met | AC 2 table on #118. Tests 20d (debt: 4 suppressed, the 5th pays, debt shared with misses) and 20e (hot penalty then debt) |
| 3. SM5 defaults recorded for comparison | Met | AC 3 table on #118 |
| 4. A mine takes life at most once, stepped on or held through, proven by a unit test | Met | `tests/life_keeper_test.cpp` 20a (duplicate, foreign and out-of-range events), 20b (stepped on, then held; engine → keeper), 20c (held through at 10 ms, 100 ms and 1 ms update grids) |
| 5. Any mismatch fixed with a unit test or filed as a follow-up bug | Met for M1/M2 (see L1) | M1 is fixed with tests 20a–20c. M2 is comment-only. The related `ScoreKeeper` gap is described only as a "possible follow-up" |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L1** `src/gameplay/score_keeper.cpp:81` — The `HitMine` case in `ScoreKeeper` still adds the −6 DP and the mine tally with no per-mine guard, and the issue comment calls this only a "possible low-priority follow-up". No follow-up issue exists (searched the repo issues). **Why:** AC 5 asks for each gap to be fixed or *filed*. Without a filed issue, the defensive asymmetry between `LifeKeeper` (now guarded) and `ScoreKeeper` (unguarded) will be lost once #118 closes. A duplicate `HitMine` event would cost DP twice while costing life once. The engine emits one `HitMine` per mine today, so players are not affected. **Fix:** file a low-priority follow-up issue (for example with `/create-story`): add a `mine_scored_` guard in `ScoreKeeper`'s `HitMine` case plus one `score_keeper_test` case. Link it from #118 before closing.

**Noted, not a finding**: adding the `ScoreKeeper` DP guard in this branch was explicitly scoped out by the plan (Open Question 1). Owner play-testing is still pending and is owner-only.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build` on the host, exit 0 |
| Warnings | NONE | Real gate: recompiled `src/gameplay/life_keeper.cpp` (`blaze4k_core` flags) and `tests/life_keeper_test.cpp` (`life_keeper_test` flags) with `-std=c++20 -Wall -Wextra -Wpedantic -O3`, `-c -o /dev/null`. Zero diagnostics |
| Lint | N/A | The project has no linter configuration (`.clang-format`/`.clang-tidy` absent); the warnings gate stands in |
| Tests | PASS | 51/51 passed, 0 skipped, sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`. The audio tests fell back to `NULL Playback Device` (a sandbox fallback that still runs every assertion, not a skip). `life_keeper_test` printed blocks 20a–20f |

## What's Good

- `consume_hit_mine` mirrors `consume_hold_outcome`: bounds check, note-type check, then the guard. It reuses `note_scored_` safely, because mines have `note_row_ == -1` and so never interact with tap row aggregation.
- The type check (`NoteType::Mine` only) also stops a stray `HitMine` on a tap index from consuming that tap's guard slot. Test 20a pins this explicitly: the real tap still pays +0.008 afterward.
- Tests 10 and 11 were corrected to target real mine notes rather than tap indices, so they still exercise the mine path under the new type check instead of silently becoming no-ops.
- 20b and 20c test the full engine → keeper path at three update rates, so AC 4 rests on integration evidence rather than only on keeper-level events.
- The upstream citations in the code comments and the issue report are accurate at the pinned commit.

## Recommendation

Ready to merge. Before closing #118, file the `ScoreKeeper` duplicate-mine DP guard as a low-priority follow-up issue (L1), so that AC 5's "or filed as a follow-up bug" holds for that gap too.
