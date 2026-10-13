# Implementation Report: Add a sanitizer build preset (AddressSanitizer + UndefinedBehaviorSanitizer)

**Plan**: `.agents/plans/completed/063-sanitizer-build-preset-plan.md`
**Branch**: `feature/063-sanitizer-build-preset`
**Status**: COMPLETE

## Summary

Added the CMake preset `asan-ubsan` (configure, build and test). It builds the project sources and the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer and runs the suite. The flags come from a new option, `BLAZE_SANITIZE`, placed after the dependencies like `BLAZE_WERROR`, so SDL3 and glad are not instrumented. The preset is a Debug build in `build-asan-ubsan/` with its own dependency build directory and never writes into `build/`.

Two canary tests (`sanitizer_canary_use_after_free`, `sanitizer_canary_float_cast`) exist only in this build and pass only when the sanitizer reports a deliberate error. The two sanitizer reports found in miniaudio 0.11.21 are filed as #144 (use-after-free) and #145 (leak). The three tests that reach the first one (`audio_test`, `select_screen_test`, `preview_player_test`) pass under the preset only when that exact report is in their output; the rule ends itself when #144 is fixed.

Nothing under `src/`, `scripts/` or `.github/` changed. The work is uncommitted on the branch.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 0 | Apply the prototype patch (`git apply --check` passed) | `CMakeLists.txt`, `CMakePresets.json`, `tests/CMakeLists.txt`, `tests/sanitizer_canary.cpp` | PASS |
| 1 | Build and run the patched preset | none (verification) | PASS |
| 2 | File the two miniaudio reports as their own issues (#144, #145) | `TODO.md`, `.agents/issues/todo-issues.md`, GitHub | PASS |
| 3 | Name the issue in the expected-report rule (`# Known report (#144):`) | `tests/CMakeLists.txt` | PASS |
| 4 | Document the preset (table row and "Sanitizer build" section) | `docs/BUILDING.md` | PASS |
| 5 | Add the command to the Commands block | `AGENTS.md` | PASS |
| 6 | Tick the TODO line for #136 | `TODO.md` | PASS |

## Validation Results

All commands ran on the host (Fedora 44, GCC 16.2.1, CMake 4.4.3, 16 cores). No sandbox: the tests open miniaudio's null device.

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Type check / build | PASS | `scripts/check.sh` (configures `build/` with `-DBLAZE_WERROR=ON`, builds) — host |
| Lint | PASS (`LINT: PASS`) | `scripts/lint.sh` (step 1 of `scripts/check.sh`, and alone after Tasks 4 and 5) — host |
| Tests, normal build | PASS (52 passed) | `scripts/check.sh` → `CHECK: PASS`, run 3 times (after the patch, after the E2E restore, after the final preset run) — host |
| Sanitizer preset: configure | PASS | `cmake --preset asan-ubsan` (33.5 s on the first run) — host |
| Sanitizer preset: build | PASS (0 warnings in project sources) | `cmake --build --preset asan-ubsan -j16` — host |
| Sanitizer preset: tests | PASS (54 passed) | `ctest --preset asan-ubsan` (9.7 s to 12.3 s), run twice in full — host |
| One test under the preset | PASS | `ctest --preset asan-ubsan -R config_persistence_test` — host |
| Canaries show both reports | PASS | `ctest --preset asan-ubsan -R sanitizer_canary -V \| grep -E "heap-use-after-free\|outside the range"` — host |
| Instrumentation scope | PASS | `build-asan-ubsan/compile_commands.json`: 122 of 122 project commands carry `-fsanitize`, 0 of 291 dependency commands do — host |
| Normal build unchanged | PASS | `grep -c -- "-fsanitize" build/compile_commands.json` → 0; `grep -c "BLAZE_SANITIZE:BOOL=OFF" build/CMakeCache.txt` → 1; `ctest --test-dir build -N \| grep -c sanitizer_canary` → 0; `cmp <scratch>/cc-main.json build/compile_commands.json` → no output — host |
| The preset left `build/` alone | PASS | `scripts/check.sh && grep -c "Building C" build/check-logs/build.log` → 0, run directly after the full preset configure, build and test — host |
| Only build files, one test file and docs changed | PASS | `git diff --stat -- src scripts .github` → no output — host |

### End-to-end verification

| # | Step | Result |
|---|------|--------|
| 1 | Red before: `cmake --preset asan-ubsan` on the unpatched branch | PASS: `CMake Error: No such configure preset in /home/lauri/github/blaze4k: "asan-ubsan"` |
| 2 | Green after: the three preset commands | PASS: 54 of 54; both `sanitizer_canary_*` tests listed; `audio_test`, `select_screen_test`, `preview_player_test` show `Passed` |
| 3 | The canaries see the sanitizers | PASS: one `AddressSanitizer: heap-use-after-free` line (`sanitizer_canary.cpp:22 in use_after_free`) and one `runtime error: 1e+300 is outside the range of representable values of type 'long int'` line |
| 4 | The preset catches a real past bug | PASS: with the range check of `read_integer` removed, `config_persistence_test` failed (see below); after `git checkout -- src/data/high_scores.cpp`, `git status --short src` printed nothing and the test passed again |
| 5 | Normal builds unchanged | PASS: the five lines above |
| 6 | Final state | PASS: `scripts/check.sh` → `CHECK: PASS`; `ctest --preset asan-ubsan` → 54 of 54, both after step 4's restore |

Step 4's report, from the run with the defect put back:

```
/home/lauri/github/blaze4k/src/data/high_scores.cpp:117:40: runtime error: 1e+300 is outside the range of representable values of type 'long int'
    #0 0x00000056d700 in read_integer /home/lauri/github/blaze4k/src/data/high_scores.cpp:117
    #1 0x00000057184d in blaze4k::load_high_scores(...) /home/lauri/github/blaze4k/src/data/high_scores.cpp:232
    #2 0x000000417cdf in main /home/lauri/github/blaze4k/tests/config_persistence_test.cpp:433
```

The known report, as the preset sees it in `audio_test`:

```
SUMMARY: AddressSanitizer: heap-use-after-free /home/lauri/github/blaze4k/build/_deps/miniaudio-src/miniaudio.h:68596 in ma_resource_manager_data_buffer_node_acquire
```

No sanitizer report other than R1 appeared (Task 1, step 4 did not apply).

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `CMakeLists.txt` | UPDATE | +19/-0 |
| `CMakePresets.json` | UPDATE | +33/-0 |
| `tests/CMakeLists.txt` | UPDATE | +32/-0 |
| `tests/sanitizer_canary.cpp` | CREATE | +47 |
| `docs/BUILDING.md` | UPDATE | +53/-0 |
| `AGENTS.md` | UPDATE | +3/-0 |
| `.agents/issues/todo-issues.md` | UPDATE | +87/-1 |
| `TODO.md` | UPDATE | +2/-0 and one tick (edited locally, gitignored) |

GitHub: issues #144 and #145 created (labels `technical`, `audio`; project board "blaze4k", status Backlog), each with a technical-notes comment.

## Deviations from Plan

1. **The report line of E2E step 4 is at `high_scores.cpp:117`, not `:119`.** The plan's sample came from a scratch copy where the function sat two lines lower. The report text and the first stack frame (`read_integer`) match the plan.
2. **The "second check compiles nothing" line needed one `scripts/check.sh` run in between.** E2E step 4 edits and restores `src/data/high_scores.cpp`, which gives the file a new timestamp, so the next `scripts/check.sh` recompiled that one file (1 "Building C" line). The check after the following full preset run compiled nothing (0 lines), which is the proof the plan asks for.
3. **`/create-issues` asks the user before it creates issues; that question was not asked.** The run was a subagent with no user dialog, and the orchestrator had passed on the decision "two issues, one per report, filed as Task 2 says". The plan's Task 2 names the action. Both issues went to the board with status Backlog.
4. **The new issues carry `Blocked by: #136`, and #145 also `Blocked by: #144`.** The plan did not state dependencies. Both issues' acceptance criteria use `ctest --preset asan-ubsan`, which exists only once #136 is merged, and the leak report shows under the preset only once the use-after-free no longer stops the test. `/do` reads these lines.
5. **`TODO.md` does not appear in the diff.** It is gitignored. Lines 58 and 59 are new and carry `(#144)` and `(#145)`; line 55 is ticked.
6. **One extra check, not in the plan:** `tests/sanitizer_canary.cpp` compiled without sanitizer flags prints `canary survived` in both modes, so `FAIL_REGULAR_EXPRESSION "canary survived"` does fail a build that lost its flags. The scratch binary was deleted.

Not done, as decided: the preset is not in `scripts/check.sh` or CI, and no follow-up issue for automation was filed.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/sanitizer_canary.cpp` (registered in `tests/CMakeLists.txt` under `BLAZE_SANITIZE` only) | `sanitizer_canary_use_after_free` (passes only on `AddressSanitizer: heap-use-after-free`, fails on `canary survived`); `sanitizer_canary_float_cast` (passes only on `runtime error: ... is outside the range of representable values`, fails on `canary survived`) |
| `tests/CMakeLists.txt` (expected-report rule, no new source) | `audio_test`, `select_screen_test`, `preview_player_test` pass under the preset only when the R1 summary line of #144 is in their output |

## Notes for the owner

- `build-asan-ubsan/` now exists on this machine (about 1.5 GB, gitignored). Delete it to get the space back.
- The existing presets (`linux-gcc-release`, `benchmark`, ...) still share `build/_deps/*-build` with `build/` (plan, Risks). Not changed here.
- CI's dependency cache key is the hash of `CMakeLists.txt`, which this change edits, so the first CI run fetches the dependencies once.
