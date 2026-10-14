# Plan: Run the sanitizer build in CI (#147)

## Summary

Add a dedicated second CI job, `sanitizer`, to `.github/workflows/ci.yml` that configures, builds, and tests the `asan-ubsan` preset on every pull request and on push to `main`. The job runs concurrently alongside the existing `check` job so that memory errors and undefined behaviour are caught automatically without slowing down the fast local check (`scripts/check.sh`). Because the `asan-ubsan` preset consumes third-party dependency sources from `build/_deps`, the job restores cached dependencies and configures `build/` first before invoking the preset. It runs `ctest --preset asan-ubsan` followed by `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error` to guarantee that both test failures and missing canary tests (from a lost `BLAZE_SANITIZE` flag) fail the CI run. `AGENTS.md` is updated with a rule table entry for the sanitizer run, and `docs/BUILDING.md` is updated to note the CI automation.

## User Story

As a developer and maintainer of Blaze 4k
I want AddressSanitizer and UndefinedBehaviorSanitizer checks to run automatically in CI on every PR and commit to `main`
So that memory safety and undefined behavior regressions are prevented automatically without manual intervention or local check slowdowns

## Metadata

| Field | Value |
|---|---|
| Type | ENHANCEMENT |
| Complexity | MEDIUM |
| Systems Affected | `.github/workflows/ci.yml`, `AGENTS.md`, `docs/BUILDING.md`, `TODO.md` |
| GitHub Issue | #147 ([TODO-44] Run the sanitizer build in CI) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/070-run-sanitizer-build-in-ci-prototype.patch` |
| Branch (suggested) | `feature/070-run-sanitizer-build-in-ci` |
| Plan sequence | `070`. `069` was #145. No branch or merge since then took a number |

---

## Environment Findings

Planned on `main` @ `9739413`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| Build | `build/` (Release, `-DBLAZE_WERROR=ON`, GCC 16.2.1) | Configured on this host |
| Baseline `check.sh` | `scripts/check.sh` prints `CHECK: PASS`, all tests pass | Green baseline on HEAD |
| Sanitizers | `build-asan-ubsan/` (Debug, `-DBLAZE_SANITIZE=ON`) | Configured via `cmake --preset asan-ubsan` |
| Baseline Sanitizers | `ctest --preset asan-ubsan` passes all 55 tests (including 2 canaries) | Green baseline on HEAD (issues #144 and #145 resolved) |
| Canary test check | `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error` | Passes 2/2 tests; exits with code 8 if no matching tests found |
| Prototype patch | `.agents/plans/070-run-sanitizer-build-in-ci-prototype.patch` | Verified via `git apply --check` with zero errors |
| Workflow YAML validation | PyYAML `safe_load` on `.github/workflows/ci.yml` | Syntax valid and parses cleanly |
| Lint | `scripts/lint.sh` passes (`LINT: PASS`) | `doc-line-citation` and `doc-test-count` rules respected |
| Audio in tests | The null device (`AudioBackend::Null`) | Silent, safe without sandbox |
| Forward references to #147 | `TODO.md:60`, `.agents/issues/todo-issues.md:1867-1910` | Tracked as TODO-44 / #147 |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Add `sanitizer` as a second top-level job in `.github/workflows/ci.yml` running alongside `check` without `needs: check` | Ensures `check` and `sanitizer` execute concurrently on GitHub Actions runners. `check` finishes fast and provides immediate lint/build/test feedback without waiting for the heavier sanitizer build. |
| D2 | Share the dependency cache between `check` and `sanitizer` using key `deps-${{ runner.os }}-${{ hashFiles('CMakeLists.txt') }}` | Both jobs require the same 5 dependency source trees (`build/_deps/*-src`). Caching prevents redundant git clones in both jobs. |
| D3 | Run `cmake -B build -DCMAKE_BUILD_TYPE=Release` before configuring the preset | As established in #136, the `asan-ubsan` preset reads dependency sources from `build/_deps` (`FETCHCONTENT_SOURCE_DIR_*`). If `build/` has not been configured, CMake aborts with "Manually specified source directory is missing". Configuring `build/` downloads or verifies dependencies in `build/_deps` without performing a compilation of `build/`. |
| D4 | Run `ctest --preset asan-ubsan` followed by `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error` as distinct workflow steps | If any test triggers an ASan or UBSan violation, the first step fails immediately with actionable stack traces. The second step ensures that if `BLAZE_SANITIZE` was ever omitted or misspelled (which would make CTest pass on normal uninstrumented binaries without canaries), the job fails because `--no-tests=error` fails when 0 canary tests run. |
| D5 | Keep `scripts/check.sh` untouched | Respects Acceptance Criterion 3: `scripts/check.sh` remains the lightweight local pre-commit check. |
| D6 | Update rule table and preamble in `AGENTS.md` | Preamble is adjusted so it accurately reflects that `scripts/check.sh` runs compiler and lint rules, while the sanitizer run is enforced by the CI job (`sanitizer`). Rule table receives a new row. |
| D7 | Document CI job in `docs/BUILDING.md` | Testing and Sanitizer build sections note the automated CI check. Confirms Debian/Ubuntu requires no extra packages beyond `build-essential`. |

---

## Patterns to Follow

### Workflow Job Structure
```yaml
// SOURCE: .github/workflows/ci.yml:16-46
jobs:
  check:
    name: Build & Test
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v7
        with:
          fetch-depth: 0

      # The SDL3 build dependencies that docs/BUILDING.md lists for Debian / Ubuntu.
      - name: Install SDL3 build dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \
            build-essential cmake git libasound2-dev libpulse-dev \
            libwayland-dev wayland-protocols libxkbcommon-dev libudev-dev \
            libx11-dev libxext-dev libxrandr-dev libxi-dev \
            libxcursor-dev libxfixes-dev libgl1-mesa-dev
```

### AGENTS.md Rules Table
```markdown
// SOURCE: AGENTS.md:119-122
| Rule | Enforced by |
| ---- | ----------- |
| Project sources compile with zero warnings. | `-DBLAZE_WERROR=ON` in `scripts/check.sh` |
```

---

## Files to Change

| File | Action | Purpose | In Patch? |
|---|---|---|---|
| `.github/workflows/ci.yml` | UPDATE | Add `sanitizer` job with dependency caching, configure, build, and test steps | Yes |
| `AGENTS.md` | UPDATE | Add sanitizer rule to table and adjust preamble | Yes |
| `docs/BUILDING.md` | UPDATE | Document sanitizer CI job under Testing and Sanitizer build sections | Yes |
| `TODO.md` | UPDATE | Check off TODO-44 item (gitignored, uncommitted) | No |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Add `sanitizer` CI Job to `.github/workflows/ci.yml`

- **File**: `.github/workflows/ci.yml`
- **Action**: UPDATE
- **Implement**: Add job `sanitizer` with:
  - Checkout step with `fetch-depth: 0`
  - Install SDL3 build dependencies via apt
  - Cache fetched dependencies (`build/_deps`)
  - Step `cmake -B build -DCMAKE_BUILD_TYPE=Release` to populate `build/_deps`
  - Step `cmake --preset asan-ubsan`
  - Step `cmake --build --preset asan-ubsan -j"$(nproc)"`
  - Step `ctest --preset asan-ubsan`
  - Step `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error`
- **Mirror**: `.github/workflows/ci.yml:16-46` (job structure and apt dependencies)
- **Validate**: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"`

### Task 2: Update Rule Table in `AGENTS.md`

- **File**: `AGENTS.md`
- **Action**: UPDATE
- **Implement**:
  - Update the sentence preceding the table: "`scripts/check.sh` runs every check below locally and in CI, except the sanitizer run which is enforced by a dedicated CI job (`sanitizer`)."
  - Add row: `| Project sources and tests run without memory errors or undefined behaviour. | CI job `sanitizer` (`.github/workflows/ci.yml`, `asan-ubsan` preset) |`
- **Mirror**: `AGENTS.md:117-123`
- **Validate**: `scripts/lint.sh`

### Task 3: Document CI Job in `docs/BUILDING.md`

- **File**: `docs/BUILDING.md`
- **Action**: UPDATE
- **Implement**:
  - In section "Testing", add note that a dedicated CI job (`sanitizer`) builds and tests the `asan-ubsan` preset on pull requests and push to `main`.
  - In section "Sanitizer build", add note that CI runs this preset in a dedicated `sanitizer` job on pull requests and push to `main`.
  - Verify Debian/Ubuntu package line confirms no extra packages needed beyond `build-essential`.
- **Mirror**: `docs/BUILDING.md:109-113`, `142-146`
- **Validate**: `scripts/lint.sh`

### Task 4: Update `TODO.md`

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Change line 60 from `- [ ] Run the sanitizer build in CI...` to `- [x] Run the sanitizer build in CI...`. (Note: `TODO.md` is gitignored and will not be committed).
- **Mirror**: `TODO.md:58-61`
- **Validate**: `git status -sb` (ensure `TODO.md` remains untracked/ignored)

---

## Validation

```bash
# Validate workflow YAML
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"

# Validate repo lint rules (doc-line-citation, doc-test-count, formatting)
scripts/lint.sh
scripts/check.sh

# Validate local sanitizer preset and canary check
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j"$(nproc)"
ctest --preset asan-ubsan
ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error
```

## End-to-End Verification

1. **Verify Prototype / Implementation Patch**:
   - `git apply --check .agents/plans/070-run-sanitizer-build-in-ci-prototype.patch` passes cleanly with exit code 0.
2. **Verify YAML Syntax**:
   - Run Python YAML parser on `.github/workflows/ci.yml`; verify zero syntax errors.
3. **Verify Canary Test Behavior**:
   - Execute `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error`; verify both canary tests pass.
   - Execute `ctest --preset asan-ubsan -R non_existent_canary --no-tests=error`; verify CTest returns non-zero exit code (code 8) when canaries are not matched.
4. **Verify Fast Check Script Unchanged**:
   - Verify `git diff scripts/check.sh` is empty.
   - Run `scripts/check.sh` and verify full suite passes without invoking sanitizer builds.
5. **Verify Repository Lints**:
   - Run `scripts/lint.sh` and verify no doc line citation or test count violations were introduced into `AGENTS.md` or `docs/BUILDING.md`.

---

## Risks

| Risk | Mitigation | Scope |
|---|---|---|
| GitHub Actions runner (`ubuntu-latest`) runs a different GCC release than host (e.g. GCC 13/14 vs GCC 16) | `asan-ubsan` does not enable `-DBLAZE_WERROR=ON`. Prior fixes (#144, #145) cleaned up miniaudio sanitizer defects so the entire test suite passes cleanly under ASan and UBSan | In scope |
| Sanitizer compilation and testing on 2-core runner takes several minutes | Job runs in parallel with `check` and does not delay it. Dependency caching via `actions/cache@v6` avoids redundant git fetches | In scope |
| First CI run on a branch or PR lacks dependency cache | Step `cmake -B build -DCMAKE_BUILD_TYPE=Release` automatically clones missing dependencies into `build/_deps` before preset configuration | In scope |
| `doc-line-citation` or `doc-test-count` lint failure in doc updates | Docs cite `.github/workflows/ci.yml` and `CMakePresets.json` by name and avoid line numbers or test counts; verified by `scripts/lint.sh` | In scope |

---

## Open Questions

None. All technical choices and requirements are established by Issue #147 and prior decisions in #136.

---

## Acceptance Criteria

- [ ] A second CI job configures, builds and tests the `asan-ubsan` preset on every pull request and on every push to `main`
- [ ] The job fails when a test fails under the preset, and when the two canary tests are missing (`ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error`)
- [ ] `scripts/check.sh` does not run the preset
- [ ] The job installs the sanitizer packages that `docs/BUILDING.md` lists for Debian/Ubuntu, and that line is corrected if the job shows it is wrong
- [ ] The rule table in `AGENTS.md` has a row for the sanitizer run, with the CI job as what enforces it
