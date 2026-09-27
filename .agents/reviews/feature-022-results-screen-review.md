# Code Review: feature/022-results-screen (#22 [C7] Results screen)

**Scope**: branch `feature/022-results-screen` vs `main`, including uncommitted working-tree changes (new `src/screens/results.{hpp,cpp}`, `src/screens/results_screen.{hpp,cpp}`, `tests/results_test.cpp`, `tests/results_screen_test.cpp`; edits to `screen.hpp`, `screen_manager.{hpp,cpp}`, `gameplay_screen.{hpp,cpp}`, `main.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/screen_manager_test.cpp`)
**Recommendation**: APPROVE (with nits)

## Summary

The C7 Results screen closes the arcade loop cleanly: `GameplayScreen` snapshots the event-sourced
`ScoreState` + life-fail flag into a pure `ResultsSummary` and publishes it once through
`ScreenContext::results`, then transitions to `Results` (falling back to `Select` when no Results
screen is registered). `ResultsScreen` reads the snapshot, submits via the existing
`submit_high_score` best-score rule, renders grade/%/DP/window counts/max combo plus `NEW RECORD` or
`FAILED`, and returns to the wheel on Confirm and Back. Every displayed field traces back to the
judgment log; no scoring/timing/parser/render code was touched. The implementation matches the plan,
builds warning-free, and all 27 tests pass. Findings are minor/design-level.

## Criteria Verification

| Criterion | Result |
|-----------|--------|
| All displayed data derives from event-sourced `ScoreState`/judgment log; no scoring/timing semantics changed | PASS — `results_summary_from` copies only `ScoreState` fields + life fail flag; diff touches no `judgment*`/`timing`/`score_keeper`/`life_keeper` |
| Handoff reports exactly once (no double submit) | PASS — `end_reported_` set before publish/transition (`gameplay_screen.cpp:75-88`); `apply_pending` no-ops on same-id and `GameplayScreen::enter` resets it |
| Cannot strand on Gameplay when Results unregistered | PASS — `has_screen(Results) ? Results : Select` (`gameplay_screen.cpp:84-86`) |
| Confirm AND Back both return to Select (no dead end); Results in back-nav contract | PASS — Confirm/Options/Right → Select (`results_screen.cpp:81-86`); `default_back_navigates` + `handle_back` Results→Select (`screen_manager.cpp:13-15,150-151`); comments updated |
| Failed runs do not submit and show FAILED | PASS — guard rejects `summary.failed`; render draws red `FAILED` banner (`results_screen.cpp:157-161`); test case 3 |
| `src/screens/results.*` SDL/GL/audio/`<ctime>`-free | PASS — `rg "SDL_|glad|miniaudio|chrono|GetTicks|std::time" results.hpp results.cpp` → no matches; timestamp injected |
| `percent < 0` guard correctness | DEFENSIBLE, minor concern — see M1 |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **`src/screens/results.cpp:26-30` — the `percent < 0` guard silently drops a legitimate
   completed-but-poor run.** On a Fail-Off run (config `fail_enabled=false`, a supported mod) an
   all-miss clear computes a negative `ScoreState::percent` (the implementation report measured
   `-2.4`), so `results_submit_score` returns false, no record is stored, and — because this is the
   chart's first play — no `NEW RECORD` flag, contradicting plan OQ1 ("first-ever play = NEW
   RECORD"). It is *correct per the plan's explicit Task 1 wording* and arguably justified because
   the on-disk schema (`high_scores.cpp:231`) rejects `percent < 0` on load, so storing it would
   produce a record that is dropped on the next startup. Recommendation: make the choice explicit
   rather than accidental — either `std::clamp` to `[0,1]` before submit (matching `format_percent`'s
   display clamp) or document that only non-negative clears can ever become records, and add a test
   for the Fail-Off all-miss path.

2. **`src/screens/results_screen.cpp:57-63` — `submitted_` can be true when no submission actually
   happened.** `submitted_ = !summary_.failed` is set whenever `ctx.scores != nullptr`, independent
   of `results_submit_score`'s result; for a valid, non-failed run with an empty `grade_label` or
   `percent < 0` the helper returns false and inserts nothing, yet `submitted()` reports true and the
   log prints `CLEARED` with no record. The header calls this "permitted", but the name/accessor
   misleads. Recommendation: set `submitted_` from the helper's success, or rename to
   `submit_permitted_` / split "attempted" vs "stored" and note it in the log.

### Low Priority / Suggestions

3. **`src/screens/results.cpp:26-28` — the range guard is asymmetric.** Negative percent is
   rejected but `percent > 1.0` is not, while the persistence loader rejects both
   (`high_scores.cpp:231`). In practice unreachable (recompute clamps `actual == possible` to 1.0),
   but symmetric enforcement would be clearer and future-proof.

4. **`src/screens/results_screen.cpp:29-43` — `grade_color` introduces unsourced presentation
   thresholds** (`0.99/0.94/0.80/0.64`) that do not line up with the pinned `grade_tiers`
   boundaries. The comment correctly labels this pure presentation, so it is acceptable, but coloring
   from the grade tier index would keep grade semantics in one place (AGENTS "data-driven constants").

5. **`tests/results_screen_test.cpp:251-253` — no assertion of "no double submit".** The loop stops
   as soon as `Results` is active; `end_reported_` is checked once. Adding a few post-transition
   frames asserting `scores` has exactly one entry and `end_reported_` stays true would directly
   guard the plan's "submitted exactly once" risk. Also no test for non-finite `percent`.

6. **`src/screens/gameplay_screen.cpp:75-88` (edge observation) — a Back press on the exact frame
   the run ends is overridden.** `handle_back()` queues `Select` before `active->update`, then the
   completed run re-queues `Results`, so the abort becomes a results report. Harmless (the run is
   already over and Results→Select reaches the wheel) and arguably correct; noted for awareness.

## Validation Results

| Check | Status |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | PASS |
| Build (`cmake --build build -j16`) | PASS |
| Warnings (`-Wall -Wextra -Wpedantic`, forced recompile of changed sources) | PASS (none) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 27/27 |
| `results_test` / `results_screen_test` / `screen_manager_test` | PASS |
| Purity regex on `src/screens/results.{hpp,cpp}` | PASS (no SDL/GL/audio/clock matches) |

## What's Good

- `ResultsSummary` is a flat, owned-copy value; no live `GameplayView` state leaks into the screen,
  keeping it headless-testable and preserving the event-sourcing boundary.
- The once-only handoff (`end_reported_` + `apply_pending` same-id no-op) and the unregistered-Results
  fallback are both correct and directly tested.
- Failed-run policy (report stats, never submit) and Back-navigation changes are consistent across
  implementation, comments, and tests.
- `results_test` covers first/lower/tie/higher/failed/invalid/null plus a real `scores.json`
  round-trip — good coverage of the model contract.
- Scope discipline: zero changes to scoring/timing/judgment/parser/audio/render modules.

## Recommendation

Approve. The behavior matches the plan and all gates pass; M1/M2 are worth a follow-up decision or
comment so the negative-percent edge and the `submitted_` naming do not surprise a future reader.
