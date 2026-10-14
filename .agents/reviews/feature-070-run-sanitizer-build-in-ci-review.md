# Code Review: feature-070-run-sanitizer-build-in-ci

**Scope**: branch `feature/070-run-sanitizer-build-in-ci` vs merge base `9739413` (including uncommitted changes)
**Recommendation**: APPROVE

## Summary

Reviewed branch `feature/070-run-sanitizer-build-in-ci` implementing GitHub issue #147. The change adds a dedicated second CI job (`sanitizer`) to `.github/workflows/ci.yml` that configures, builds, and tests the `asan-ubsan` preset on pull requests and pushes to `main`, sharing dependency caching with the `check` job and asserting canary tests with `--no-tests=error`. The rule enforcement table in `AGENTS.md` and the building documentation in `docs/BUILDING.md` are updated to reflect the automated CI gate. All acceptance criteria are satisfied:

- **Criterion 1 (Second CI job configures, builds, and tests asan-ubsan on PR and push to main)**: Met. `.github/workflows/ci.yml` defines the `sanitizer` job inheriting workflow triggers `push: branches: [main]` and `pull_request`, running `cmake -B build -DCMAKE_BUILD_TYPE=Release` (to populate `build/_deps`), `cmake --preset asan-ubsan`, `cmake --build --preset asan-ubsan`, and `ctest --preset asan-ubsan`.
- **Criterion 2 (Job fails on test failure and when canary tests are missing)**: Met. The job executes `ctest --preset asan-ubsan` and explicitly verifies the canary tests with `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error`. Verified locally that CTest exits with code 8 if no matching canary tests are found.
- **Criterion 3 (scripts/check.sh does not run preset)**: Met. `scripts/check.sh` is unchanged and continues to execute only lint, format, and the standard Release build and test suite.
- **Criterion 4 (Installs Debian/Ubuntu packages from docs/BUILDING.md)**: Met. The workflow installs `build-essential` and SDL3 build dependencies matching `docs/BUILDING.md`, which notes that `build-essential` installs GCC ASan and UBSan runtimes on Debian/Ubuntu with no extra packages needed.
- **Criterion 5 (Rule table in AGENTS.md has a row for sanitizer run)**: Met. `AGENTS.md` adds a table row attributing enforcement of "Project sources and tests run without memory errors or undefined behaviour" to CI job `sanitizer` (`.github/workflows/ci.yml`, `asan-ubsan` preset).

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (Release) & `cmake --build --preset asan-ubsan` (Debug ASan+UBSan) on Linux host |
| Warnings | NONE | Zero warnings emitted; configured with `-DBLAZE_WERROR=ON`; no C++ code was modified in this branch |
| Lint | PASS | `scripts/lint.sh`, `scripts/format.sh --check`, and CI YAML parse check via Python `yaml.safe_load` |
| Tests | PASS | `scripts/check.sh`: 53/53 tests passed; `ctest --preset asan-ubsan`: 55/55 tests passed; canary test regex matched 2/2 tests; missing canary test simulation exited with code 8 |
| UI verification | N/A | CI workflow and documentation updates only; no UI, markup, or rendering touched |

## What's Good

- Consistent workflow architecture: The `sanitizer` job reuses the same caching key and SDL3 dependency installation list as the existing `check` job, running independently without slowing down `check`.
- Robust failure guarantees: Pre-populating `build/_deps` satisfies `FETCHCONTENT_SOURCE_DIR_*` requirements of the preset, and the canary check ensures compiler instrumentation cannot be dropped unnoticed.
- Documentation hygiene: Updates to `AGENTS.md` and `docs/BUILDING.md` strictly comply with repository lint rules (no hardcoded test counts or line number citations).

## Recommendation

APPROVE. All acceptance criteria for issue #147 are satisfied with full test and lint verification. The branch is ready to be committed and merged.
