# Code Review: feature/063-sanitizer-build-preset

**Scope**: branch `feature/063-sanitizer-build-preset` against its merge base `d50744b` (`main`), issue #136. The branch has no commits; the whole change is uncommitted (6 modified files, 4 untracked).
**Recommendation**: APPROVE WITH NITS

## Summary

The change adds the CMake preset `asan-ubsan`, the option `BLAZE_SANITIZE`, two canary tests, an expected-report rule for three tests, and the documentation. All four acceptance criteria of #136 are met, and each was checked here independently of the implementation report. There is no Critical or High finding. One Medium finding: the preset ignores the dependency version pins, which matters for the follow-up issues #144 and #145. Three Low findings are about wording and bookkeeping. `.agents/issues/todo-issues.md` and the plan files were read lightly.

Acceptance criteria of #136:

| # | Criterion | Result | Evidence |
|---|-----------|--------|----------|
| 1 | A preset builds and runs the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer | Met | `cmake --preset asan-ubsan`, build, `ctest --preset asan-ubsan`: 54 of 54, three runs. 122 of 122 project compile commands carry the flags, 0 of 291 dependency commands do |
| 2 | All tests pass, or each report is fixed or filed | Met | Two reports, both in miniaudio 0.11.21, filed as #144 and #145 (both open). Three tests pass by the expected-report rule (see "Noted") |
| 3 | `docs/BUILDING.md` documents the preset and its packages | Met | `docs/BUILDING.md`, table row and section "Sanitizer build" |
| 4 | The normal release and debug builds are unchanged | Met | Compile commands (412) and link commands (55) of `main` and of the branch are identical for Release and for Debug; no canary target; `build/` not written by the preset |

## Issues Found

### Critical

None

### High Priority

None

### Medium Priority

- **M1** `CMakePresets.json:112` — The five `FETCHCONTENT_SOURCE_DIR_*` lines (`:112`-`:116`) make the preset ignore the `GIT_TAG` pins in `CMakeLists.txt`: it builds whatever source tree `build/_deps` holds at that moment. **Why:** After a pin changes, the preset tests the old dependency until `build/` is configured again, and prints nothing about it. Shown in a scratch copy: with the miniaudio pin set to a tag that does not exist, `cmake --preset asan-ubsan` still configured and used 0.11.21. Issue #145 proposes exactly such a change (move the miniaudio pin to 0.11.23), so whoever fixes it and runs the preset first sees the old leak and the old use-after-free. `docs/BUILDING.md:145` says `build/` is needed "once", which hides this. The other presets do not have the problem: they run FetchContent's own update step. **Fix:** State the rule where the command is given: in `docs/BUILDING.md:145` replace "once" with "first, and again after a dependency version in `CMakeLists.txt` changes", and add the same note to the technical notes of #145. Alternative (the plan's Open Question 3): delete the five lines, so the preset fetches its own sources and honours the pins.

### Suggestions (Low)

- **L1** `tests/CMakeLists.txt:637` — The canary tests and the expected-report rule are registered only when `BLAZE_SANITIZE` is on, so they cannot catch the preset losing that option. **Why:** If the `"BLAZE_SANITIZE": "ON"` line in `CMakePresets.json:110` is removed or misspelt, or the option is renamed in `CMakeLists.txt` only, `ctest --preset asan-ubsan` passes on a plain Debug build with no canary in the list (checked: a Debug configure of the branch without the option has no `sanitizer_canary` target). The comment at `tests/CMakeLists.txt:635` and `docs/BUILDING.md:169` promise more than this ("a build that lost its sanitizer flags fails"), and the plan's decision D5 names the misspelt option as the case the canaries are for. What they do prove was checked and holds: flags lost inside the `BLAZE_SANITIZE` block, or a report that is only printed, turn them red. **Fix:** Say in both places what the canaries prove (the flags of the `BLAZE_SANITIZE` block reach the tests and a report ends the program) and that the two `sanitizer_canary_*` lines must appear in the `ctest` output. Optional stronger form: end the documented command with `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error`.
- **L2** `docs/BUILDING.md:96` — The paragraph under the preset table says every preset downloads the dependencies on a brand-new machine, which is not true for the new row. **Why:** Without a configured `build/`, `cmake --preset asan-ubsan` stops with `Manually specified source directory is missing: FETCHCONTENT_SOURCE_DIR_SDL3` (run here in a scratch copy). The cure is 50 lines further down, in "Sanitizer build". **Fix:** Add one sentence to that paragraph, or to the table row at `:84`: "`asan-ubsan` needs `build/` configured first (see Sanitizer build)".
- **L3** `.agents/issues/todo-issues.md:1690` — The entry of TODO-39 still says `Blocks: None`, while the new entries say `Blocked by: #136` (`:1818`, `:1860`), and they write `#136` and `TODO-43` where the file uses `[TODO-N]` (`:1606`, `:1689`). **Why:** The dependency is recorded on one side only and in a second format, and `/do` reads these lines. **Fix:** Set `:1690` to `Blocks: [TODO-42], [TODO-43]` and write `[TODO-39]`, `[TODO-42]` and `[TODO-43]` in the three new dependency lines.

**Noted, not a finding**:

- The three tests under the expected-report rule stop early, so their code gets almost no sanitizer coverage until #144 is fixed. Measured here: `audio_test` stops at `tests/audio_test.cpp:39` (before the corrupt-file check, any successful load, playback, seek and unload), `preview_player_test` stops in the first of its 8 test functions, and `select_screen_test` stops in the third of its 19. The plan accepted this (decision D6, Risks), `docs/BUILDING.md` says it, and #144 removes it. The rule itself is sound: with the summary line suppressed (`ASAN_OPTIONS=print_summary=0`), `audio_test` fails with "Required regular expression not found". The three bug classes that the issue names stay covered, because `config_persistence_test`, `ttf_font_test` and `clock_tap_test` run in full.
- The preset is run by hand; it is not in `scripts/check.sh` or CI, and no follow-up issue for that exists (plan, Open Question 2). The acceptance criteria ask only for a preset.
- The existing presets share `build/_deps/*-build` with `build/` (plan, Risks). Not changed here.
- `BLAZE_SANITIZE` says "GCC or Clang", and Clang was not run (no Clang on the host).
- Not reproduced here: the 744-byte leak of #145 (it shows only once #144 is out of the way), and the Debian/Ubuntu package statement (no Ubuntu host). The upstream statements were checked: miniaudio 0.11.25 is the newest tag and still frees the node before reading it, and the changelog of 0.11.23 has "Fixed a memory leak in the resource manager when opening a file fails".

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` on the host (Fedora 44, GCC 16.2.1), run twice: `CHECK: PASS`. Sanitizer build: `cmake --preset asan-ubsan` and `cmake --build --preset asan-ubsan -j16`, exit 0 |
| Warnings | NONE | Both `scripts/check.sh` builds were incremental and compiled nothing, so they prove nothing about warnings. Real gate: all 122 project compile commands of `build-asan-ubsan/compile_commands.json` were re-run with `-o /dev/null`: no diagnostic. `tests/sanitizer_canary.cpp` also compiles clean with `-Werror` at `-O0` and `-O2`. The normal build compiles no changed source |
| Lint | PASS | `scripts/lint.sh` (step 1 of `scripts/check.sh`, and alone): `LINT: PASS` — host |
| Tests | PASS | Normal build: `scripts/check.sh`, 52 passed, 0 failed, 0 skipped. Sanitizer preset: `ctest --preset asan-ubsan`, 54 passed, 0 failed, 0 skipped, three runs. Not a full run under the preset: `audio_test`, `select_screen_test` and `preview_player_test` pass on the expected report of #144 and stop there. All tests open miniaudio's null device (a fallback that runs every assertion, not a skip) |
| UI verification | N/A | No game code changed (`git diff --stat -- src scripts .github` is empty) |

Further checks, all run in the scratchpad with the repository untouched:

- Normal builds unchanged: `main` (exported with `git archive`) and the branch were each configured for Release with `-DBLAZE_WERROR=ON` and for Debug. The 412 compile commands and the 55 `link.txt` files are identical after the paths are normalised. No `-fsanitize`, `BLAZE_SANITIZE:BOOL=OFF`, no `sanitizer_canary` target.
- The preset leaves `build/` alone: no file in `build/` (outside `check-logs/`) is newer than a marker made before the preset's configure, build and three test runs. `build-asan-ubsan/_deps` holds build directories only, so nothing was downloaded.
- The canary: without sanitizer flags, and with the flags but without `-fno-sanitize-recover`, it prints `canary survived` (the test would fail). With the preset's flags at `-O2` both modes still stop with their report.
- The preset catches the old `read_integer` defect: a scratch copy of `src/data/high_scores.cpp` without the range check, compiled with the preset's flags and linked into `config_persistence_test`, stops with `runtime error: 1e+300 is outside the range of representable values of type 'long int'` in `read_integer`. The unmodified test passes.

## What's Good

- `BLAZE_SANITIZE` follows `BLAZE_WERROR` exactly (an option after the dependencies), so the normal builds are provably unchanged and a report is always in code that this repository compiles.
- The plan found that `-fsanitize=undefined` alone would not have caught the issue's own example, and added `float-cast-overflow` and `-fno-sanitize-recover`. Both additions are needed, and both are explained in the code comment and the docs.
- The preset has its own dependency build directory, so it does not force an SDL rebuild in `build/`.
- The three affected tests are not excluded. The rule fails as soon as the report disappears, so it cannot be forgotten.
- The numbers in the implementation report (122 and 291 commands, 54 tests, the report's file and line) all matched what was measured here.

## Recommendation

Ready to merge. M1 and L1 to L3 are small text changes (M1 can also be solved by deleting five preset lines); fix them with `/fix-findings .agents/reviews/feature-063-sanitizer-build-preset-review.md` before the pull request, or accept them. #144 is worth doing soon, because until then the preset checks very little of the sound loading, preview and song select code.
