# Code Review: branch feature/066-formatter-check-changed-lines

**Scope**: branch `feature/066-formatter-check-changed-lines` (vs `main` merge base, including uncommitted changes; Issue #142)
**Recommendation**: NEEDS CHANGES

## Summary

Reviewed branch `feature/066-formatter-check-changed-lines` implementing issue #142 ([TODO-41] Add a formatter check for changed lines). The change adds a root `.clang-format`, adds `scripts/format.sh` wrapping `git-clang-format`, integrates a format check step into `scripts/check.sh`, pins `clang-format==23.1.1` in CI, and adds the governing rule to `AGENTS.md`. All local validations (lint, format, build with warnings as errors, and 53 ctest tests) pass. Two issues require attention: a merge-base omission for CI pull-request runs that causes false format failures when target branches advance, and an overly restrictive short-function rule in `.clang-format` that expands single-line accessors.

## Issues Found

### Critical
None

### High Priority
- **H1** `scripts/format.sh:67` — In CI pull requests (`GITHUB_BASE_REF` is set), `base` is assigned `origin/$GITHUB_BASE_REF` directly without resolving to the merge base with `HEAD`. **Why:** `git-clang-format` invokes `git diff-index -p -U0 <commit> --`; if commits have landed on the target branch after the PR branch diverged, `git diff-index` treats upstream target commits as reverse diffs, causing `scripts/format.sh --check` to falsely fail on lines only touched upstream. **Fix:** Compute the merge base matching the local fallback on line 71: `base="$(git merge-base HEAD "origin/$GITHUB_BASE_REF" 2>/dev/null || echo "origin/$GITHUB_BASE_REF")"`.

### Medium Priority
- **M1** `.clang-format:20` — `AllowShortFunctionsOnASingleLine` is set to `Empty` instead of `Inline`. **Why:** The codebase standard style commonly uses one-line inline functions/accessors inside class definitions (over 200 occurrences in `src/`); setting `Empty` causes `clang-format` to split every touched single-line method into a multi-line block (e.g. testing `src/gameplay/life_keeper.hpp` yields 28 changed lines instead of the expected 1 comment space). **Fix:** Change `AllowShortFunctionsOnASingleLine: Empty` to `AllowShortFunctionsOnASingleLine: Inline` in `.clang-format`.

### Suggestions (Low)
- **L1** `scripts/format.sh:83` — `scripts/format.sh --check` swallows the exit code of `git clang-format` via `|| true` and assumes lack of diff output indicates success. **Why:** If `git-clang-format` crashes or fails due to an unexpected environment or toolchain error (e.g. missing Python modules or invalid arguments), the failure is hidden and the script outputs `FORMAT: PASS`. **Fix:** Inspect the exit code to differentiate diff detection (exit code 1 with diff output) from genuine command failures.

**Noted, not a finding**: Untracked files are not checked by `git-clang-format` until staged or committed; this matches standard `git-clang-format` behavior and is covered once files are staged or committed.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` steps `[3/5] Configure...` & `[4/5] Build (-j16)...` (`-DCMAKE_BUILD_TYPE=Release -DBLAZE_WERROR=ON`) — host |
| Warnings | NONE | Zero compiler warnings under `-DBLAZE_WERROR=ON` (no C++ files modified in scope) |
| Lint | PASS | `scripts/lint.sh` — host (0 failures) |
| Tests | PASS | `scripts/check.sh` step `[5/5] Test...` (`ctest --test-dir build --output-on-failure`); 53 of 53 passed, 0 skipped, 0 failed |
| UI verification | N/A | No UI, rendering, or layout changes |

## What's Good

- Scoped specifically to changed lines via `git-clang-format`, preventing massive git blame churn across untouched legacy code.
- Seamlessly integrated as step `[2/5] Format...` in `scripts/check.sh` with clear, actionable fix guidance (`Fix: scripts/format.sh`).
- Proper toolchain version parity: pins `clang-format==23.1.1` via PyPI with `--break-system-packages` and configures full history checkout (`fetch-depth: 0`) in `.github/workflows/ci.yml`.
- Documentation updated in lockstep in `AGENTS.md` and local `TODO.md`.

## Recommendation

Fix **H1** by resolving the merge base for `GITHUB_BASE_REF` in `scripts/format.sh`, and update **M1** in `.clang-format` to `AllowShortFunctionsOnASingleLine: Inline` to preserve codebase accessor formatting conventions.
