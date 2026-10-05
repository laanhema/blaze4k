# Code Review: feature/054-calibration-wide-delay-window (re-review r1)

**Scope**: Branch `feature/054-calibration-wide-delay-window` vs `main` for issue #74. Commits `bcd143f` (implementation) and `f01f599` (fixes for the review findings), plus uncommitted changes. The only uncommitted change, `.agents/stories/todo-stories.md`, is unrelated and excluded. Prior report: `.agents/reviews/feature-054-calibration-wide-delay-window-review.md`.
**Recommendation**: APPROVE WITH NITS

## Summary

The fix commit `f01f599` resolves all three prior findings. The early-side aliasing (prior Medium) is now documented accurately in the summary bullet, the Wizard headroom section and the plan's OQ1, and `test_early_alias_documented_limit` pins it. The "20 ms after/before" wording is corrected. The per-frame `result()` recompute is replaced by the equivalent cached `result_`. The fix commit introduces no regressions. Builds, the warnings gate and all 51 sandboxed tests are clean. What remains are two Low documentation nits in `docs/AUDIO_LATENCY.md`.

## Prior Findings: Fix Verification

| # | Prior finding | Status | Evidence |
|---|---|---|---|
| M1 | Early-side aliasing below about −80 ms is saved silently with the wrong sign, and the docs claimed it was refused | **Fixed** (owner chose option (a): document and pin the behaviour) | `docs/AUDIO_LATENCY.md:36-38` (summary) and `:128-131` now say that only about −80 to −50 ms shows `OUT OF RANGE`. The new paragraph "Limit more than about 80 ms early" (`:141-147`) gives the −0.10 s → −0.40 s example. The plan's OQ1 has a "Correction (review of #74)" note at `.agents/plans/completed/054-calibration-wide-delay-window-plan.md:355`. `tests/offset_calibration_test.cpp:280-296` pins −0.10 → `mean_delta` +0.40, offset −0.40, `!out_of_range`, `ready`. I checked the numbers against `offset_calibration.cpp:92-95,147-148`. A centre in `[-0.25,-0.05)` gets one period added, which puts it in `[0.25,0.45)`. `mean > 0.42` therefore holds exactly when the true delay is in `(-0.08,-0.05)`, so the docs' "−80 to −50 ms" band is correct |
| L1 | Docs example said "20 ms **after** the next" | **Fixed** | `docs/AUDIO_LATENCY.md:136` now reads "20 ms before the next" |
| L2 | Redundant per-frame `calib_.result()` in `update` | **Fixed**, no behaviour change | `src/screens/calibration_screen.cpp:112-114` now does `CalibrationResult current = result_;`. I verified the invariant. `calib_` changes only in `enter()` (`:62`, which resets `result_` to `CalibrationResult{}`, the same as `result()` on an empty model) and in `add_sample` (`:124`). A successful add re-reads `current` (`:125`), and `:146` re-syncs `result_` at the end of every update. A rejected add only bumps `rejected_wild`, which `current` never reads (only `ready`, `out_of_range` and `offset_seconds` are read), and the same was true before the fix |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **The citation for the out-of-range log line points to the model, which does not log.** `docs/AUDIO_LATENCY.md:130-131`: "Enter does not save, and one `[Calibration]` line is logged (`src/timing/offset_calibration.cpp:147-149`)". Those lines only compute `out_of_range` and `ready`. The Enter refusal is at `src/screens/calibration_screen.cpp:129-130`, and the one-time log line is at `src/screens/calibration_screen.cpp:157-164`. This was already in `bcd143f`, and the prior review missed it. **Recommendation:** cite `offset_calibration.cpp:147-149` for the flag and `calibration_screen.cpp:129-130, 157-164` for the refusal and the log.
2. **The "extended range" sentence is now in the wrong paragraph.** `docs/AUDIO_LATENCY.md:147-148`: the sentence "A slower 'extended range' calibration (for example 60 BPM, which would cover up to about 900 ms) could be added as a follow-up if a device that slow turns up" belonged to the above-450 ms limit. The fix commit moved it to the end of the new early-side paragraph, where "a device that slow" no longer refers to anything. **Recommendation:** move the sentence back to the end of the "Limit above about 450 ms" paragraph (after `:139`) and rewrap the paragraph to its neighbours' width (line 147 is now about 127 characters).

**Noted, not a finding:**
- With the 120 BPM click, delays above about 450 ms alias to a small value, and net delays more than about 80 ms early alias to a late delay one beat later (−0.10 s saves as a −0.40 s offset). The owner accepted both limits for #74: no 60 BPM or extended mode, and no window rebalance. Both are now documented accurately (`docs/AUDIO_LATENCY.md:34-38, 135-148`, plan OQ1) and pinned by tests (`test_out_of_range` covers 0.43/0.46, and `test_early_alias_documented_limit` covers the early side).
- The implementation report `.agents/reports/054-calibration-wide-delay-window-plan-report.md:13,20` still describes only the >450 ms limit. It is a historical record of the implementation step, and the plan's correction note covers the change, so it does not need an edit.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC) | PASS |
| Warnings gate: changed sources recompiled with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, taken from `flags.make`, `-c -o /dev/null`) for `offset_calibration.cpp`, `calibration_screen.cpp`, `setup_art.cpp` (`blaze4k_core`) and `offset_calibration_test.cpp`, `calibration_screen_test.cpp`, `setup_art_test.cpp` | PASS (0 diagnostics) |
| Lint | N/A (no linter configured) |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` | PASS 51/51. The new `test_early_alias_documented_limit` ran and passed |
| Sandbox check | `/dev/snd` and `/run/user/1000` are empty inside the sandbox. `calibration_screen_test` uses the synthetic-clock fallback (`[Calibration] audio unavailable; using synthetic clock`), and every assertion still runs. That is a fallback, not a skip |
| Skipped / env-guarded tests | None. `ctest -V` shows no skip markers. The "skipping"/"disabled" lines are parser and no-GL log output |
| Doc `file:line` citations after the fix commit shifted lines (`AUDIO_LATENCY.md:53` `calibration_screen.cpp:69`, `:98-165`, `offset_calibration.cpp:65-151`, `.hpp:23-26`; `:121-122` `:28-42`, `.hpp:33-34`; `:126` `:79-101`; `:54` `calibration_screen.cpp:106-107`) | PASS. Every citation points to the cited code, apart from Low #1 above |

## What's Good

- The prior Medium was settled the way the owner decided. The docs, the plan correction and a test now agree on the same numbers (−80/−50 ms band, −0.10 → −0.40), and the test comment links back to the docs section.
- `f01f599` is small and focused: docs, one test, and a one-line cached-result change with a comment that states the invariant.
- The rest of the #74 change, as approved in the prior review, is unchanged: most-recent-beat matching, the circular-mean unwrap, the derived (non-sticky) `OutOfRange` phase, and the refusal to save in that state.

## Recommendation

You can merge as is. Optionally take the two Low docs nits: fix the log-line citation and move the "extended range" sentence back to the >450 ms paragraph. The owner's hardware checks are still pending: re-run the wizard on wired and on the WH-1000XM4 and expect about −0.02 s and about −0.22 s.
