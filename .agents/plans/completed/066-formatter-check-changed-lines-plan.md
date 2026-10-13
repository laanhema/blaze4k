# Plan: Add a formatter check for changed lines (#142)

## Summary

Four code reviews flagged comment or code lines that were wrapped unevenly or ran past the width of their neighbours, and nothing currently checks it. Adding a global formatter pass across the entire codebase would churn git blame across hundreds of files (297 lines in `src/` and `tests/` currently exceed 100 columns). 

This change introduces a root `.clang-format` configured to faithfully match Blaze 4k's existing C++20 conventions (100 column limit, 4-space indentation, attached braces, left pointer alignment, no namespace indentation, preserved include order, and comment reflowing). It adds a dedicated helper script `scripts/format.sh` and integrates it into `scripts/check.sh` as step `[2/5] Format...`, ensuring that `scripts/check.sh` fails with actionable fix instructions whenever a line touched by a change violates formatting. Untouched existing lines and files remain unformatted. CI workflow `.github/workflows/ci.yml` is updated to pin the exact host formatter version (`clang-format 23.1.1` via PyPI) with full commit history checkout, and `AGENTS.md` gains the governing rule. A verified prototype patch is saved beside this plan.

## User Story

As a developer contributing C++ code to Blaze 4k
I want an automated check that validates formatting on lines touched by my changes
So that line width and comment wrapping issues are caught before code review without reformatting untouched legacy code

## Metadata

| Field | Value |
|---|---|
| Type | ENHANCEMENT |
| Complexity | MEDIUM |
| Systems Affected | Tooling and CI (`.clang-format`, `scripts/format.sh`, `scripts/check.sh`, `.github/workflows/ci.yml`, `AGENTS.md`, `TODO.md`) |
| GitHub Issue | #142 ([TODO-41] Add a formatter check for changed lines) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/066-formatter-check-changed-lines-prototype.patch` |
| Branch (suggested) | `feature/066-formatter-check-changed-lines` |
| Plan sequence | `066`. `065` was #141 (PR #150). |

---

## Environment Findings

Planned on `main` @ `6bb256d`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| Host clang-format | `23.1.1` (`clang-format version 23.1.1`) | Installed in `~/.local/bin/clang-format` via pip (`pip show clang-format`) |
| Host git-clang-format | `/home/lauri/.local/bin/git-clang-format` | Python-based git extension bundled with `clang-format 23.1.1` |
| Baseline tests | `scripts/check.sh` prints `CHECK: PASS`, 100% tests passed out of 53 | Verified at planning time |
| Prototype run | Lint passes (`LINT: PASS`), format check passes (`FORMAT: PASS`), zero warnings under `-DBLAZE_WERROR=ON`, 53 of 53 tests pass | Verified in detached scratch worktree |
| Audio in tests | Silent null audio device | No real sound hardware opened |
| Forward references to #142 | `TODO.md:57`, `.agents/issues/todo-issues.md:1740-1778` | Specifies 100 cols, `.clang-format`, `check.sh` failure, CI version pin |
| Existing legacy code lines > 100 cols | 297 lines across `src/` and `tests/` | Explains why format check is strictly scoped to changed lines |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Create standalone `scripts/format.sh` and invoke `scripts/format.sh --check` from `scripts/check.sh` | Decouples format checking and in-place fixing. Developers can run `scripts/format.sh` directly to fix formatting, while `scripts/check.sh` runs `--check` non-destructively. |
| D2 | Auto-detect base commit for diffing: CLI arg -> `$GITHUB_BASE_REF` -> `HEAD~1` (on CI push) -> merge base with `origin/main` or `main` -> `HEAD` | Handles all developer and CI workflows seamlessly: local uncommitted changes, feature branches with commits, CI pull requests, and CI pushes to `main`. |
| D3 | Format output mirrors `scripts/lint.sh` style | Prints `FORMAT: clang-format`, lists violating files indented by 2 spaces, and prints `Fix: scripts/format.sh`, matching the repo's established lint failure UX. |
| D4 | Pin `clang-format==23.1.1` in CI via `pip` | Ubuntu apt does not package LLVM 23. The host has `clang-format 23.1.1` installed via PyPI wheel. Using `pip install --break-system-packages clang-format==23.1.1` in CI guarantees identical formatting decisions between host and CI. |
| D5 | Set `fetch-depth: 0` in `.github/workflows/ci.yml` `actions/checkout` | Default depth of 1 prevents `git-clang-format` from finding the base commit of pull requests or feature branches. Fetching full history ensures merge-base detection succeeds without network shallow-clone errors. |
| D6 | Graceful exit on non-git trees | If `scripts/format.sh` is invoked outside a git repository (e.g. from an exported source tarball), it exits 0 without error since there are no git-tracked changes. |

---

## Patterns to Follow

### Lint Failure Output Format
```bash
// SOURCE: scripts/lint.sh:41-45
echo "LINT: $name"
echo "$hits" | sed 's/^/  /'
echo "  Fix: $fix"
echo ""
failures=$((failures + 1))
```

### Check Script Step Pipeline
```bash
// SOURCE: scripts/check.sh:70-75
echo "[1/4] Lint..."
run_step lint "$repo_root/scripts/lint.sh"

echo "[2/4] Configure..."
run_step configure cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DBLAZE_WERROR=ON
```

### Rule Table in AGENTS.md
```markdown
// SOURCE: AGENTS.md:126-128
| Docs and scripts leave the test count to `ctest`. | lint `doc-test-count` |
| Docs cite this repo's code by file and symbol name. | lint `doc-line-citation` |
| A lint exception sits on the offending line with a reason, an expiry date and the approving human. | lint `lint-exception` |
```

---

## Files to Change

| File | Action | Purpose | Covered by Patch |
|---|---|---|---|
| `.clang-format` | CREATE | Project clang-format style configuration matching existing codebase conventions | Yes |
| `scripts/format.sh` | CREATE | Standalone check and fix script using `git-clang-format` | Yes |
| `scripts/check.sh` | UPDATE | Add step `[2/5] Format...` calling `scripts/format.sh --check` | Yes |
| `.github/workflows/ci.yml` | UPDATE | Fetch full git history (`fetch-depth: 0`) and install `clang-format==23.1.1` | Yes |
| `AGENTS.md` | UPDATE | Add rule for changed-line formatting to the "Rules and what enforces them" table | Yes |
| `TODO.md` | UPDATE | Mark item 57 ([TODO-41]) as completed (local gitignored file) | No (gitignored) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Create `.clang-format` configuration
- **File**: `.clang-format`
- **Action**: CREATE
- **Implement**: Create root `.clang-format` with `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 100`, `AccessModifierOffset: -4`, `PointerAlignment: Left`, `BreakBeforeBraces: Attach`, `NamespaceIndentation: None`, `SortIncludes: Never`, `ReflowComments: true`, `AlignTrailingComments: {Kind: Leave}`, `AllowShortFunctionsOnASingleLine: Empty`, `AllowShortBlocksOnASingleLine: Empty`, `AllowShortLambdasOnASingleLine: Inline`, and `BreakConstructorInitializers: BeforeColon`.
- **Mirror**: `src/timing/music_clock.hpp:44-60`, `src/util/parse_number.hpp:10-32`
- **Validate**: `clang-format --style=file:.clang-format --dry-run src/util/parse_number.hpp` (exits 0 with zero diffs)

### Task 2: Create `scripts/format.sh` wrapper script
- **File**: `scripts/format.sh`
- **Action**: CREATE
- **Implement**: Create executable bash script that accepts `--check` and an optional base commit. Detect base revision (`GITHUB_BASE_REF`, `HEAD~1` on push, or `git merge-base HEAD origin/main`). Run `git clang-format --diff` on `--check` and output violating file names with `FORMAT: clang-format` and `Fix: scripts/format.sh` on failure. Run `git clang-format -f` without `--check` to format files in place. Handle missing tool gracefully with installation advice (`pip install clang-format==23.1.1`).
- **Mirror**: `scripts/lint.sh:35-47`, `scripts/check.sh:20-43`
- **Validate**: `scripts/format.sh --check` (exits 0 with `FORMAT: PASS` on clean branch)

### Task 3: Update `scripts/check.sh` with Format step
- **File**: `scripts/check.sh`
- **Action**: UPDATE
- **Implement**: Insert format check step `[2/5] Format...` immediately after Lint step `[1/5] Lint...`. Run `run_step format "$repo_root/scripts/format.sh" --check`. Renumber subsequent steps to `[3/5] Configure...`, `[4/5] Build...`, `[5/5] Test...`.
- **Mirror**: `scripts/check.sh:70-82`
- **Validate**: `scripts/check.sh`

### Task 4: Update `.github/workflows/ci.yml`
- **File**: `.github/workflows/ci.yml`
- **Action**: UPDATE
- **Implement**: Add `fetch-depth: 0` to `actions/checkout@v7` step. Add step `Install clang-format` running `pip install --break-system-packages clang-format==23.1.1` before building.
- **Mirror**: `.github/workflows/ci.yml:20-41`
- **Validate**: `scripts/lint.sh`

### Task 5: Update `AGENTS.md` rule table
- **File**: `AGENTS.md`
- **Action**: UPDATE
- **Implement**: Add row `| Lines touched by a change are formatted with .clang-format. | scripts/check.sh (scripts/format.sh --check) |` to the table in section "Rules and what enforces them".
- **Mirror**: `AGENTS.md:126-128`
- **Validate**: `scripts/lint.sh`

### Task 6: Verify format impact on three existing files
- **File**: N/A (Verification)
- **Action**: VALIDATE
- **Implement**: Format three existing files (`src/util/parse_number.hpp`, `src/timing/music_clock.hpp`, `src/gameplay/life_keeper.hpp`) with the new config. Verify that diffs are minimal (0 lines for `parse_number.hpp` and `music_clock.hpp`, 1 comment space for `life_keeper.hpp`). Record these results in the implementation report and PR description.
- **Mirror**: Acceptance Criteria #5
- **Validate**: `clang-format -i src/util/parse_number.hpp src/timing/music_clock.hpp src/gameplay/life_keeper.hpp && git diff src/`

### Task 7: Update `TODO.md`
- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Mark line 57 ([TODO-41] #142) as checked `[x]` in local `TODO.md`.
- **Mirror**: `TODO.md:56`
- **Validate**: `git status -s` (confirms `TODO.md` is ignored and untracked)

---

## Validation

```bash
# Type check / Build with warnings as errors
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLAZE_WERROR=ON
cmake --build build -j"$(nproc)"

# Lint
scripts/lint.sh

# Run format check directly
scripts/format.sh --check

# Run full repository check suite (lint, format, configure, build, test)
scripts/check.sh
```

## End-to-End Verification

1. **Clean state test**: On a clean working tree on `main` or a feature branch, execute `scripts/format.sh --check` and verify output prints `FORMAT: PASS` and exits with code 0.
2. **Failure and error message test**: In a C++ file (e.g. `src/util/parse_number.cpp`), introduce a deliberately unformatted comment or mis-indented code block. Run `scripts/check.sh` and verify:
   - Step `[2/5] Format...` fails with exit code 1.
   - The output displays `CHECK: FAIL`, names `src/util/parse_number.cpp`, and displays `Fix: scripts/format.sh`.
3. **Fix command test**: Run `scripts/format.sh` and verify it reformats only the modified lines. Re-run `scripts/check.sh` and verify all 5 steps pass. Revert test edits (`git checkout src/util/parse_number.cpp`).
4. **Three files proof**: Run `clang-format -i src/util/parse_number.hpp src/timing/music_clock.hpp src/gameplay/life_keeper.hpp`. Run `git diff` and verify that changes are minimal (no syntactic refactoring, only whitespace adjustments on existing lines). Revert before commit.
5. **Full suite run**: Run `scripts/check.sh` to confirm zero lint failures, zero formatting errors, zero compiler warnings under `-DBLAZE_WERROR=ON`, and 100% tests passing across all 53 ctest targets.

---

## Risks

| Risk | Mitigation | In / Out of Scope |
|---|---|---|
| Reformatting existing code produces massive churn on git blame | Format check is strictly scoped to changed lines via `git-clang-format`. Existing code is untouched. | In scope (enforced by design) |
| Clang-format version skew between developer machine and CI causes spurious failures | Host version is `23.1.1`; CI explicitly installs and pins `clang-format==23.1.1` via PyPI. | In scope |
| Shallow clone in CI lacks base commit for diffing | CI workflow checks out with `fetch-depth: 0` so merge-base commit is always available. | In scope |
| Developer runs `scripts/format.sh` on an environment without `git-clang-format` | Script checks for binary availability and outputs a clear failure message with pip install command. | In scope |
| Tarball source exports without `.git` fail in `format.sh` | Script detects absence of git work tree via `git rev-parse --is-inside-work-tree` and exits 0 cleanly. | In scope |

---

## Open Questions

- **Question 1: CI package installation method for clang-format 23.1.1**
  - *Context*: Issue 142 notes mention "CI installs its tools with apt-get. Pin the formatter there to the host's version". However, Debian/Ubuntu apt repositories only package releases up to clang-format-18/19; version 23.1.1 is provided via PyPI wheel.
  - *Options*:
    1. (Proposed default) Install `clang-format==23.1.1` via `pip install --break-system-packages clang-format==23.1.1` in CI.
    2. Add LLVM nightly apt repo to Ubuntu in CI.
  - *Rationale*: Option 1 is instantaneous (< 3 seconds in CI), exactly mirrors the host's Python wheel in `~/.local/lib/python3.14/site-packages/clang_format`, and avoids fragile apt repository signing keys.

---

## Acceptance Criteria

- [ ] A `.clang-format` at the repo root, 100 columns, matching the current style
- [ ] `scripts/check.sh` fails when a line the change touches is not formatted, and the error prints the command that fixes it
- [ ] The existing code is not reformatted
- [ ] CI and the host use the same clang-format version (`23.1.1`)
- [ ] Formatting three existing files with the new config changes little, with the result shown in the pull request
- [ ] The rule table in `AGENTS.md` gains the row
- [ ] `scripts/check.sh` passes 100% with zero warnings and zero lint/format failures
