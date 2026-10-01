# Code Review (Re-review r2): feature/017-json-persistence-config-and-high-scores

**Scope**: Branch `feature/017-json-persistence-config-and-high-scores` vs `main` at HEAD `181da2b` ("Harden #17 integer parsing boundary and clarify --data-dir warning"). `git status --porcelain` clean except this untracked report. Issue #17 (C2: JSON persistence, config + high scores).
**Recommendation**: APPROVE

## Summary

Re-review of the one commit (`181da2b`) made after r1. The medium residual N1 (`read_integer` out-of-range cast at the `2^63` boundary) is now fully fixed by reading integer JSON natively (`uint64`/`int64`) instead of round-tripping through `double`, with a corrected open upper bound on the float path. The Low #8 `--data-dir` message is clarified. All previously-fixed High/Medium findings (#1/#2/#3/#4) still hold; no Critical/High regression was introduced. Build is clean and all 17 tests pass, with an independent probe reproducing the N1 edge cases as correct.

## Fix-Verification Table

| Finding | Prior severity | Status | Evidence |
|---------|----------------|--------|----------|
| **N1** — `read_integer` accepts `2^63` and casts out-of-range double (UB), corrupts valid `INT64_MAX` | Medium | **Fixed — VERIFIED** | `high_scores.cpp:103-114` reads integer JSON natively: unsigned path rejects `value > (uint64)INT64_MAX` (`:105`), signed path returns `get<int64_t>()` directly (`:112`). Float path uses the corrected open bound `value >= 9223372036854775808.0` (`:117`, `:120`) so `2^63` is rejected instead of cast. Independent probe against `libblaze4k_core.a`: `2^63` literal → REJECTED; `2^63` double → REJECTED; `UINT64_MAX` → REJECTED; `INT64_MAX` literal → `dp=INT_MAX`, `timestamp` intact (no `INT64_MIN`); `1e300`/`-1e300`/`1.5` → REJECTED; `INT64_MIN` literal → clamp to `INT_MIN` (correct, native parse). Test `config_persistence_test.cpp:343-356` locks the `boundary_hi` reject and `int64_max` survival. |
| **Low #8** — `--data-dir` missing argument warns and continues instead of failing | Low | **Addressed (behavior intentionally unchanged)** | `main.cpp:110-113` now emits "ignoring the flag and using the default data directory", making the fallback explicit rather than surprising. The r1 report already accepted warn-and-continue as consistent with sibling flags (`--speed`, `--gameplay-demo`); no defect. |
| High #1 — unchecked out-of-range double → UB on cast | High | **Still fixed** | Original exploit (`1e300`) rejected at `high_scores.cpp:118-121`; probe confirms REJECTED. |
| High #2 — `dance_points` narrowed int64→int without bound check | High | **Still fixed** | Clamp to `[int::lowest(), int::max()]` before the cast at `high_scores.cpp:247-253`; probe `INT64_MAX`/`INT64_MIN` clamp to `INT_MAX`/`INT_MIN` without wrapping. |
| Medium #3 — invalid string enums loaded and re-persisted | Medium | **Still fixed** | Defaults captured and invalid `speed_mod` (empty) / `scroll` (∉{up,down}) coerced at `config_loader.cpp:217-231`; test 5b asserts non-re-persistence (17/17 pass). |
| Medium #4 — `video.fullscreen` persisted but never applied | Medium | **Still fixed** | `WindowConfig::fullscreen` at `window.hpp:14`; applied as `SDL_WINDOW_FULLSCREEN` at `window.cpp:82-84`; copied from config at `main.cpp:148`. |

## New Findings

**None.** The post-r1 commit is confined to `read_integer`, the `--data-dir` message, and a test addition; it introduces no new correctness, safety, or regression issues. Other r1 non-actionable observations (invalid-but-nonempty speed mods; version field written but not validated on load) remain as accepted Low/awareness notes.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS (all 20 targets; no warnings) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (17/17, including `config_persistence_test`) |
| Lint | N/A (no separate linter configured) |
| Independent boundary probe (`/tmp/opencode/probe_n1`, not added to repo) | PASS (all 8 hostile/boundary cases behave correctly) |

Skipped/env-guarded tests: none — `ctest` executed 17/17, 0 skipped.

## What's Good

- The N1 fix is the robust version of the suggestion (native integer parsing) rather than a brittle epsilon tweak; the `uint64`-range check is exact and the float open bound is documented.
- The boundary is now covered by a real test including the `INT64_MAX`-must-survive case and a re-save assertion that no `-9223372036854775808` is emitted (`config_persistence_test.cpp:350-370`).
- The `--data-dir` message change removes the ambiguity the prior Low finding called out at negligible cost.

## Recommendation

Approve. N1 and Low #8 are resolved, prior High/Medium fixes hold, no new findings, and validation is green. Ready to merge.
