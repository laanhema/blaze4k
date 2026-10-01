# Code Review: feature/025-results-polish (#25 [D3] Results polish)

**Scope**: branch `feature/025-results-polish` vs `main`, including uncommitted working-tree changes (new `src/screens/results_anim.{hpp,cpp}`, `tests/results_anim_test.cpp`; edits to `src/screens/results_screen.{hpp,cpp}`, `tests/results_screen_test.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`)
**Recommendation**: APPROVE WITH NITS

## Summary

The D3 results polish is genuinely presentational: a new pure `ResultsAnimator` (SDL/GL/audio/wall-clock-free,
`fixed_dt`-only) supplies scale/alpha/percent-progress curves and `ResultsScreen` consumes them for a grade
slam, a percent count-up, fading rows, and a gated NEW RECORD finale. No judgment/scoring/timing/persistence,
results schema, or C7 submit path is touched; the screen still submits exactly once in `enter`. At completion
every element collapses to the exact C7 static output (grade, percent formatting, DP, counts), the count-up is
monotonic and never exceeds the true percent, the finale is gated on the real `new_record_` flag, and Back
still exits immediately via the manager. One medium behavioral inconsistency exists on the invalid
`NO RESULT` path; everything else is minor.

## Criteria Verification

| Criterion | Result |
|-----------|--------|
| Purely presentational; no judgment/scoring/timing/persistence/schema/C7-submit changes | PASS — diff limited to `results_screen.*`, new `results_anim.*`, CMake, tests; `enter` submit block unchanged; `fixed_dt` drives only `animator_.update` (`results_screen.cpp:82`) |
| `results_anim.*` SDL/GL/audio/wall-clock-free | PASS — grep for `SDL_|glad|miniaudio|chrono|GetTicks|std::time|<ctime>` → no matches; `<cmath>`/`<algorithm>` only |
| Grade reveal settles to EXACTLY C7 values at completion | PASS — at `elapsed == kRevealSeconds` all alphas = 1, `grade_scale` = 1.0, `percent_progress` = 1.0 (`format_percent(percent * 1.0)`), DP/counts/max-combo are unconditionally the C7 values; finale flash decays to 0 |
| Percent count-up monotonic, never exceeds true percent | PASS for percent ∈ [0,1] — `percent * percent_progress` is non-decreasing; `format_percent` clamps to [0,1] and truncation preserves monotonicity. (Negative/unclamped percents display as 0.00% due to the display clamp.) |
| NEW RECORD finale gated on a real new record, absent on normal clears/failed runs | PASS — screen gates all finale draws on `animator_.new_record()`, seeded from `results_submit_score` (first entry or strictly greater %), which returns false for failed/unstorable runs; test case 4c |
| Confirm/Options/Right skip without navigating while revealing, exit once finished | PASS — single `if (!animator_.finished()) { skip(); return; } else transition` (`results_screen.cpp:88-101`); no double-action (skip path returns); test cases 4/4b/7 |
| Back exits immediately | PASS — manager handles Back before `active->update` (`screen_manager.cpp:225-227,159-160`); test case 5 |
| No dead end | PASS — two Confirm presses (or wait 2.4 s + one) reach Select; Back always works |
| Headless render no-op; no NaN; degenerate inputs guarded | PASS — draws via no-op-when-uninitialized `GlQuadRenderer`; `reveal_alpha` guards `duration <= 0` and `elapsed <= delay`; `update` clamps negative/NaN `fixed_dt`; no division by zero |
| Unsourced presentation constants flagged | PASS (as documented) — header lines 13-14 declare all reveal constants "Blaze 4k presentation, unsourced"; see L1/L2 |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **`src/screens/results_screen.cpp:80-103,117-122` — the invalid `NO RESULT` path is NOT "exactly C7"
   for input: the first Confirm/Options/Right is swallowed by skip gating while the UI already advertises
   `[ENTER] CONTINUE`.** `update()` calls `animator_.update(fixed_dt)` unconditionally and gates the press on
   `animator_.finished()` even when `summary_.valid == false` (the invalid branch in `render` ignores the
   animator and shows CONTINUE immediately). So for the first 2.4 s after entering an invalid results screen a
   press does not navigate, contradicting the plan/report claim that the invalid path "skips animation
   entirely and behaves exactly as C7" (report lines 17-18). Not a dead end (a second press or ~2.4 s of frames
   resolves it; Back exits), but it is an untested input inconsistency. Recommendation: in `update`, navigate
   immediately when `!summary_.valid` (or gate the skip on `animator_.valid()`), and add a regression test for
   invalid-summary input.

### Low Priority / Suggestions

1. **`src/screens/results_screen.cpp:126-217` — the invalid path's static CONTINUE hint and the animated
   valid path were not factored, so the skip/no-skip contract lives only in `update`.** Related to M1; a
   shared decision helper would keep render and input from drifting again.

2. **`src/screens/results_anim.cpp:48,66,74,93,108,116` — several inline magic numbers duplicate the named
   timing constants.** `title_alpha`/`grade_alpha` use literal `0.25`, `stats_alpha` `0.35`, `failed_alpha`
   `0.40`, `record_scale` punch `0.5`, `record_flash` `0.35`. The blanket "unsourced" comment covers them, but
   promoting them to named `constexpr` (like the `k*` constants) would keep tuning in one place and make the
   timeline self-documenting. The exact overshoot/slam numbers (`2.4/1.6/0.7/0.8/0.2`, `0.6/0.55/1.15/0.15`)
   are likewise inline.

3. **`src/screens/results_anim.hpp:36-38` — `valid()`/`failed()` accessors are dead for the screen.** The
   screen reads `summary_.valid`/`summary_.failed` directly; the animator's copies are only exercised by
   `results_anim_test`. Harmless, but the duplicated flags can drift from the summary. Consider dropping
   `valid_`/`failed_` or documenting them as test-only.

4. **`tests/results_screen_test.cpp:225-245` — no assertion that the finale is actually absent at render time
   for a normal clear.** The test pins `animator().new_record() == false`, and `results_anim_test` pins curve
   inertness, but nothing asserts the screen does not draw the banner/overlay on a sub-best or failed run
   (headless draw is a no-op, so this is only verifiable structurally). Low value given the no-op renderer.

## Validation Results

| Check | Status |
|-------|--------|
| Configure | PASS (build dir already host-configured) |
| Build (`cmake --build build -j16`) | PASS |
| Warnings (`-Wall -Wextra -Wpedantic`, forced recompile of changed sources) | PASS (none) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 32/32 |
| `results_anim_test` / `results_screen_test` / `screen_manager_test` / `judgment_animator_test` | PASS |
| Purity grep on `src/screens/results_anim.{hpp,cpp}` | PASS (no SDL/GL/audio/clock matches) |
| Scope grep (`gameplay/|timing/|chart/|audio/|data/|screen_manager|main.cpp|results.*`) | PASS (no matches) |

## What's Good

- The `ResultsAnimator` seam mirrors `JudgmentAnimator` faithfully: pure static curves keyed off an
  accumulated `fixed_dt`, headless-testable, and free of SDL/GL/audio/clock. The purity and scope
  claims are all verifiable.
- "Animate the presentation, never the numbers" is upheld rigorously — percent, DP, counts, and grade
  formatting are all the C7 values; only scale/alpha/progress are animated, and completion is byte-identical
  to C7.
- The skip-vs-exit branch is a clean single `if/else` that `return`s, so a skip press cannot also navigate;
  Back remains entirely manager-owned and is unaffected.
- `results_anim_test` is thorough: endpoints, undershoot, monotonicity, pulse oscillation, punch overshoot,
  flash decay, degenerate `duration <= 0`, negative/oversized `dt`, and skip clamping.
- The once-only submit / no-resurrect assertions in the end-to-end test are preserved and still green.

## Recommendation

Approve with nits. Fixing M1 (invalid-path input semantics vs. the plan's "exactly C7" claim) is worth a
one-line guard in `update` plus a test; L2 (naming the inline timing literals) is optional polish. No
changes are required to correctness, data, or the submit path.
