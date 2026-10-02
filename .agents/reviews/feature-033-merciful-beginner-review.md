# Code Review: feature/033-merciful-beginner

**Scope**: Branch `feature/033-merciful-beginner` vs `main` (all changes uncommitted in the working tree; 16 tracked files, +711/-27). GitHub issue #67.
**Recommendation**: APPROVE (with nits)

## Summary

The diff implements OpenITG `MercifulBeginner` for Beginner charts: the effective Way Off widens by +0.5 s (classification and miss/avoided-mine expiry), an early Way Off is display-only, and negative DP weights are clamped to 0. A data-driven `merciful_beginner` flag (default `true`) controls it. I checked each behavior against the pinned OpenITG source (`f2c129fe`: `Player.cpp:34-77, 1084-1093, 1710-1737`, `ScoreKeeperMAX2.cpp:187-189, 515-590`) and it matches. The engine, keeper and view changes are small and well contained, and every acceptance criterion in #67 is met and tested. One medium parity gap remains in how Beginner is detected, plus one low test gap.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **`src/chart/chart.hpp:11-27` (also the comment at `:14`): Beginner detection skips OpenITG's `Steps::TidyUpData` fallback.**
   `is_beginner_difficulty` only matches the label `"beginner"`. OpenITG resolves difficulty in two steps:
   - `StringToDifficulty(label)` runs first (`Difficulty.cpp:22-44`).
   - Every SM load then calls `Steps::TidyUpData()` (`NotesLoaderSM.cpp:63`, `Steps.cpp:130-141`). When the label is `DIFFICULTY_INVALID` (for example `"Novice"`, an empty label, or any unknown word), it retries `StringToDifficulty(description)`. If that is also invalid, it infers from the meter, and `meter == 1` gives `DIFFICULTY_BEGINNER`.

   So in OpenITG, a `"Novice"` chart with meter 1, or a chart whose description is `"Beginner"`, is a MercifulBeginner chart. SM5 has the same fallback, and it also covers SSC. The comment that `"novice" is NOT Beginner in OpenITG` is wrong for those charts, and two tests lock the wrong claim in: `tests/note_parser_test.cpp:208` and `tests/judgment_engine_test.cpp:1105`.

   There is also a UX inconsistency. The select screen already colors `"Novice"` as Beginner (`src/screens/select_screen.cpp:100`), yet gameplay applies normal rules to that chart.

   The plan only covered the `smaniac`/`challenge` description hack (Open Question 1). It never mentions `TidyUpData`.

   **Recommendation:** Mirror the resolution chain in one pure helper:
   1. Map the label through the full `StringToDifficulty` table.
   2. If the label is invalid, map the description.
   3. If both are invalid, `meter == 1` means Beginner.

   Then update the comment and the two tests, and add cases for `Novice`/meter 1 (Beginner), `Novice`/meter 2 (not Beginner), and an unknown label with description `"Beginner"`. If this is deferred, fix the comment and record it as a known divergence instead.

### Suggestions (Low)

1. **`tests/score_keeper_test.cpp:846-880` (18b): the popup half of the display-only path is untested.**
   Section 18b proves an early Beginner Way Off never reaches the log or the keepers. Nothing asserts that it reaches the judgment popup, which is the only user-visible effect of `Player.cpp:1089-1093` (`m_Judgment.SetJudgment`). If someone deleted `judge_anim_.consume(display_events_)` (`src/gameplay/gameplay_view.cpp:211`), every test would still pass.

   **Recommendation:** Expose a read-only accessor for the popup label, such as `GameplayView::judgment_popup_label()` forwarding `JudgmentAnimator::popup_label()`. Then assert in 18b that the label reads Way Off after the press. Also add a same-tick case where a recorded judgment and a display-only event arrive together, so the recorded one must win the popup.

**Noted, not a finding** (scoped out by the plan or the issue):
- The SM description hack (`smaniac`/`challenge` description forces Challenge, `NotesLoaderSM.cpp:35-42`) is Open Question 1, deliberately not mirrored.
- The course-mode exclusion (`ScoreKeeperMAX2.cpp:187-189`) does not apply because Blaze has no course mode.
- `FailOffInBeginner` was skipped as Task 12 by orchestrator decision.
- Grade derives from DP percent, so the separate grade-weight clamp is covered implicitly. This is documented at `score_keeper.hpp:73-79`.
- The interactive owner play-test has not been run.

**Verified correct against OpenITG source:**
- The +0.5 s is added to Boo only, after scale/add (`Player.cpp:49-56`). Hold windows ignore Beginner (`AdjustedWindowHold`, `:60-74`).
- "Early" means `bSteppedEarly = -fNoteOffset < 0`, i.e. `hit < note` (`:1089`). An early Way Off inside the base band is also suppressed, as upstream does.
- When the head is hit late, hold life restarts from the hit. That is equivalent to upstream's `fLife = 1` on step (`:556-560`) and roll refresh on step (`:1210-1215`).
- DP clamps cover tap, hit-mine and hold OK/NG. `possible_dp` is unclamped (`:449-451`).
- Life is untouched, since `LifeMeterBar.cpp` has no Beginner branch.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | PASS |
| Type check / warnings gate: changed sources re-checked with the build's own flags (`-std=c++20 -Wall -Wextra -Wpedantic -fsyntax-only`). Covers `note_parser.cpp`, `judgment_constants_loader.cpp`, `gameplay_view.cpp`, `judgment_engine.cpp`, `score_keeper.cpp`, `judgment_constants.cpp`, plus the 5 changed test files | PASS (0 warnings) |
| Lint (no separate linter; the warnings gate above is the lint gate) | PASS |
| Tests (`bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; checked inside the sandbox that `/dev/snd` and `/run/user/$UID` are empty) | PASS (38/38) |
| Skipped / env-guarded | No test was skipped. `perf_loop_test` has an existing opt-in strict mode (`BLAZE4K_PERF_STRICT`, unset). It is unrelated to this change. |

## What's Good

- Every behavior cites exact OpenITG line numbers, and the code follows upstream's structure closely: an overloaded `effective_windows(bool)` stands in for `AdjustedWindowTap`'s `bIsPlayingBeginner`, and a `dp_weight` clamp stands in for `iWeight = max(0, iWeight)`.
- The display-only side channel keeps the event-sourced design intact. Suppressed events never enter `events()`, so score, life, combo, results and explosions cannot see them by construction.
- No signatures changed. Beginner status comes from the `Chart*` that `reset()` already receives, and the empty default label stops hand-built charts from becoming Beginner by accident.
- Tests are strong, with controls throughout:
  - Medium/Hard controls and flag-off controls.
  - Inclusive edges, and +1e-6 beyond the edge.
  - Early Way Off inside the base band.
  - Hold head after a suppressed early Way Off.
  - Avoided-mine expiry delay.
  - JSON null and non-boolean values falling back.
  - An end-to-end GameplayView boundary test through the real input path.

## Recommendation

Safe to merge once Medium #1 is resolved: either mirror the `TidyUpData` resolution chain (preferred, since it is a small pure-helper change plus tests) or correct the comment and record the divergence. Low #1 is optional hardening.
