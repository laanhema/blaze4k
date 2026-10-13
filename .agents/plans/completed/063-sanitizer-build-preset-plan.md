# Plan: Add a sanitizer build preset (AddressSanitizer + UndefinedBehaviorSanitizer)

## Summary

Add a CMake preset, `asan-ubsan`, that builds the project sources and the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer and runs the suite. A new CMake option, `BLAZE_SANITIZE` (off by default, placed after the dependencies like `BLAZE_WERROR`), adds the flags to the project's own targets only. The preset is a Debug build in `build-asan-ubsan/` with its own dependency build directory, so it never rewrites `build/`. Two canary tests exist only in this build and pass only when the sanitizer reports a deliberate error, so a build that lost its flags fails.

The change was built and run in a scratch worktree, and the result is saved as a prototype patch. Three things the issue did not say came out of that run:

1. **`-fsanitize=undefined` does not catch the out-of-range cast the issue names.** GCC leaves `float-cast-overflow` out of it, and by default a report is only printed and the test still passes. The option adds `float-cast-overflow` and `-fno-sanitize-recover`. With them the preset catches the old `read_integer` bug when it is put back (file and line in the report).
2. **The first run found two real defects, both in miniaudio 0.11.21**, on the path where a sound file fails to load: a read of freed memory (still in miniaudio 0.11.25) and a 744-byte leak (fixed in 0.11.23). No report came from project code. Acceptance criterion 2 allows "fixed or filed"; this plan files them (Open Question 1 gives the alternative). Three tests reach the defect. They are not excluded: in this build each passes only when that exact report appears, so fixing miniaudio turns them red until the rule is removed.
3. **The timing tests do not need loosening.** `perf_loop_test` passed 20 of 20 strict runs under the sanitizers (worst frame 0.19 ms against a 50 ms limit), and `clock_tap_test` 10 of 10.

No game code changes. The normal Release and Debug builds compile with byte-identical commands (checked on all 121 project compile commands).

## User Story

As the owner (and any agent) changing Blaze 4k
I want one preset that runs the tests under AddressSanitizer and UndefinedBehaviorSanitizer
So that memory errors and undefined behaviour stop a test with a file and line, instead of waiting for a reviewer to spot them

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY (build and test tooling) |
| Complexity | MEDIUM |
| Systems Affected | `CMakeLists.txt` (one option), `CMakePresets.json` (one configure, build and test preset), `tests/CMakeLists.txt` (one `BLAZE_SANITIZE` block), `tests/sanitizer_canary.cpp` (new), `docs/BUILDING.md`, `AGENTS.md` (Commands), `TODO.md` |
| GitHub Issue | #136 ([TODO-39] Add a sanitizer build preset (AddressSanitizer + UndefinedBehaviorSanitizer)) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/063-sanitizer-build-preset-prototype.patch` |
| Branch (suggested) | `feature/063-sanitizer-build-preset` |
| Plan sequence | `063`. `062` was #131. #134, #135 and the CI work had no plan file and used no number |

---

## Environment Findings

Planned on `main` @ `d50744b`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| CMake | 4.4.3 | Presets file is `"version": 5`; test presets support `environment` and `filter` |
| Compiler | GCC 16.2.1 (Fedora 44), 16 cores | No Clang on the host |
| Sanitizer runtimes | `libasan-16.2.1-2.fc44`, `libubsan-16.2.1-2.fc44` installed | The issue's owner step is done. #134 (blocker) is closed |
| Baseline | `scripts/check.sh` prints `CHECK: PASS`, 52 of 52 tests | Re-run after the prototype: still green, 8 s incremental |
| `scripts/check.sh` and this change | It never reads the `asan-ubsan` preset | The check for this change is the three preset commands in Validation. On `main` the first one fails: `CMake Error: No such configure preset ... "asan-ubsan"` (run at planning time) |
| Prototype timings (`-j16`) | configure 33 s, first build 1 min 24 s, no-op build 0.6 s, `ctest` 10.5 s | 10 full `ctest` runs with the final flags, all green (4 of them with the final expected-report rule). `build-asan-ubsan/` is about 1.5 GB (gitignored by `build-*/`) |
| What is instrumented | 68 `src/` and 54 `tests/` compile commands carry `-fsanitize`; 291 dependency commands carry none | Read from `build-asan-ubsan/compile_commands.json`. miniaudio and stb compile inside project sources (`src/audio/miniaudio_impl.cpp`, `src/render/stb_*_impl.cpp`), so they are instrumented |
| Normal build | 121 project compile commands in a Release configure of the patched tree equal those of `main`'s `build/compile_commands.json`; no `-fsanitize`; `BLAZE_SANITIZE:BOOL=OFF`; no `sanitizer_canary` target | Compared after path normalisation |
| Warnings in the sanitizer build | 0 in project sources | The build log has about 2300 warnings, all in SDL3 and glad sources. Those get the project's warning flags in every build today; not part of this work |
| Lint | `scripts/lint.sh` passes with the patch and with the doc text of Tasks 4 and 5 | Dry-run in the scratch worktree |
| Debian / Ubuntu packages | `libgcc-13-dev` depends on `libasan8` and `libubsan1` (packages.ubuntu.com, noble) | So `build-essential` already installs the runtimes. Not run on an Ubuntu host |
| Network | Available, not needed | The preset reads the dependency sources that `build/_deps` already holds |

### GCC facts (probe program, this host)

| Flags | `static_cast<int64_t>(1e300)` |
|---|---|
| `-fsanitize=address,undefined` | No report, exit 0 |
| `... ,float-cast-overflow` | Report printed, exit 0 (the test would pass) |
| `... ,float-cast-overflow -fno-sanitize-recover=undefined,float-cast-overflow` | Report printed, exit 1 |

`detect_stack_use_after_return` is already on by default in this runtime (a probe that reads a returned local is reported without `ASAN_OPTIONS`), so the preset sets no `ASAN_OPTIONS`. LeakSanitizer runs by default.

### Sanitizer reports on `main` (the whole list)

Both come from one call: `SoundStream::load` (`src/audio/sound_stream.cpp:63-91`) calls `ma_sound_init_from_file` with a file that is missing or corrupt.

| # | Report | Where | Reached by | Upstream |
|---|---|---|---|---|
| R1 | `AddressSanitizer: heap-use-after-free`, read of 4 bytes | `ma_resource_manager_data_buffer_node_acquire`, `miniaudio.h:68596`: the node is freed at `:68588` and its `isDataOwnedByResourceManager` field is read right after | `audio_test` (`tests/audio_test.cpp:37-41`, the missing-file check), `select_screen_test`, `preview_player_test` | Same code in 0.11.22, 0.11.23, 0.11.24 and 0.11.25 (the newest tag) |
| R2 | `LeakSanitizer`: 744 bytes per failed load (2, 10 and 3 times in the three tests) | `ma_sound_init_from_file_internal`, `miniaudio.h:75891`: the data source is not freed when the load fails | The same three tests, visible only once R1 is out of the way | Fixed in 0.11.23 ("Fixed a memory leak in the resource manager when opening a file fails") |

Scratch evidence, none of it in the patch:

- With both defects patched in a scratch copy of `miniaudio.h`, all 54 tests pass under the preset. So R1 and R2 are the complete list.
- With unmodified miniaudio and a pre-check in `SoundStream::load` (open the file with `ma_decoder_init_file`, return false when that fails, `ma_decoder_uninit`, then load as today), all 54 tests also pass. That is the fix proposed in the issues Task 2 files.
- In the game: R1 reads memory in the same thread right after the free, and the value only gates a branch that is not taken. R2 leaks 744 bytes per failed load. Neither is a crash today.

### The preset catches a real past bug

`read_integer` (`src/data/high_scores.cpp:93-125`) was fixed after a review found the out-of-range cast. With its range check removed in the scratch worktree, `config_persistence_test` (the `"dp":1e300` case, `tests/config_persistence_test.cpp:421-458`) fails under the preset:

```
src/data/high_scores.cpp:119:40: runtime error: 1e+300 is outside the range of representable values of type 'long int'
    #0 ... in read_integer src/data/high_scores.cpp:119
```

### Issue-stated numbers and notes, re-checked

| Issue note | Measured |
|---|---|
| "`perf_loop_test` checks a CPU budget ... may need to be left out of this preset or given a looser limit" | Not needed. 20 strict runs of the sanitizer binary: 0 failures; worst `max` 0.19 ms against 50 ms, `p99` 0.053 ms against 16.67 ms. The preset leaves the test as it is. `BLAZE4K_PERF_STRICT=0` (`tests/perf_loop_test.cpp:158-178`) stays the switch if a loaded machine ever needs it |
| "other tests check timing" | `clock_tap_test` 10 of 10, `clock_interpolation_test` and `music_clock_test` 20 of 20 |
| "the sanitizer runtimes are not installed" | Installed now |
| "ThreadSanitizer is out of scope" | Kept out |

### Forward references to #136

`grep -rn "#136\|TODO-39"` finds `TODO.md:55` (the source line, which Task 6 ticks) and `.agents/issues/todo-issues.md` (`:49`, `:70`, `:1655-1690`; the issue's own entry). Nothing in `src/`, `tests/`, `docs/`, `scripts/`, `.claude/` or `.github/`. Two older documents mention sanitizers as "not installed": `.agents/reviews/feature-053-audio-period-clock-interpolation-review-r1.md:75` and `.agents/plans/completed/053-audio-period-clock-interpolation-plan.md:44`. They are dated records; leave them.

### Presets share dependency build directories (found, not fixed here)

The hidden `base` preset (`CMakePresets.json:9-16`) points every preset's `FETCHCONTENT_BASE_DIR` at `build/_deps`. That shares the sources and also the `*-build` directories. Evidence: `build-linux-gcc-release/` was configured at 16:15 today and never built (it has no library), and `build/_deps/sdl3-build/libSDL3.a` was rebuilt at 16:20 by the next `scripts/check.sh`, though SDL's sources date from 2 October. A Debug sanitizer build in that shared directory would force an SDL rebuild in `build/` after every run. This is why the new preset has its own `FETCHCONTENT_BASE_DIR`. The existing presets keep the problem; see Risks.

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | The flags come from a CMake option, `BLAZE_SANITIZE`, after `FetchContent_MakeAvailable`; not from `CMAKE_CXX_FLAGS` in the preset | Mirrors `BLAZE_WERROR` (`CMakeLists.txt:83-94`). SDL3 and glad stay uninstrumented, so a report is in code this repo compiles itself. The tests are headless and run little SDL code |
| D2 | The preset sets its own `FETCHCONTENT_BASE_DIR` (`build-asan-ubsan/_deps`) and points `FETCHCONTENT_SOURCE_DIR_<NAME>` at the five source trees in `build/_deps` | `build/` is never rewritten, nothing is downloaded, and no second copy of the sources (340 MB) is made. Cost: `build/` must be configured once first. Without it CMake stops with "Manually specified source directory is missing: FETCHCONTENT_SOURCE_DIR_SDL3" (run in scratch). Open Question 3 |
| D3 | `CMAKE_BUILD_TYPE` is `Debug` | No optimisation removes an error before the sanitizer sees it, the line numbers are exact, and asserts are on. The suite still runs in 10.5 s |
| D4 | `-fsanitize=address,undefined,float-cast-overflow` with `-fno-sanitize-recover=undefined,float-cast-overflow` and `-fno-omit-frame-pointer` | See "GCC facts". Without the first addition the issue's own example bug is not caught; without the second a report does not fail the test |
| D5 | Two canary tests, registered only under `BLAZE_SANITIZE` | The repo rule: a test "fails if the code under test does nothing". A preset whose option name is mistyped builds a normal Debug tree and passes everything. Each canary needs the sanitizer's own report in its output (`PASS_REGULAR_EXPRESSION`) and fails if the program runs on (`FAIL_REGULAR_EXPRESSION "canary survived"`). Checked by hand: built without sanitizers, and built without `-fno-sanitize-recover`, the canary prints `canary survived` |
| D6 | The three tests that reach R1 are not excluded. Under `BLAZE_SANITIZE` each passes only when R1's exact summary line is in its output | An exclusion stays forever unless someone remembers it. This rule ends itself: with miniaudio patched in scratch, the three tests fail with "Required regular expression not found" until the rule is removed. Cost: in this build those three tests stop at their first failed load (they run in full in the normal build) |
| D7 | The test preset sets only `UBSAN_OPTIONS=print_stacktrace=1` | The other defaults already do what is wanted (see "GCC facts" and the perf row above) |
| D8 | The preset does not set `BLAZE_WERROR`, does not run tests in parallel, and is not added to `scripts/check.sh` or CI | Warnings stay `scripts/check.sh`'s job. Parallel runs would disturb the real-time tests. Open Question 2 covers automation |

---

## Patterns to Follow

### A project-only build option, placed after the dependencies

```cmake
# SOURCE: CMakeLists.txt:83-94
# Warnings as errors for the project's own targets. Placed after the
# dependencies so it never reaches them. scripts/check.sh (and so CI) turns it
# on; it is off by default so the owner-run Windows and macOS presets are
# unchanged.
option(BLAZE_WERROR "Treat compiler warnings in project sources as errors" OFF)
if(BLAZE_WERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()
```

### A special-purpose preset: one configure, one build and one test entry with the same name

```json
// SOURCE: CMakePresets.json:90-99 (configure), :143-146 (build), :204-215 (test)
{
  "name": "benchmark",
  "displayName": "Benchmark (RelWithDebInfo)",
  "description": "Builds the perf_loop_test CPU-budget benchmark with debug info.",
  "inherits": "base",
  "generator": "Unix Makefiles",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo"
  }
}
```

### Test registration, with a comment that says what the test is for

```cmake
# SOURCE: tests/CMakeLists.txt:609-619
# util/parse_number.hpp: the one text -> number path (scripts/lint.sh bans the
# raw std::sto* / strto* / ato* / from_chars calls elsewhere under src/).
add_executable(parse_number_test
    parse_number_test.cpp
)

target_link_libraries(parse_number_test PRIVATE
    blaze4k_core
)

add_test(NAME parse_number_test COMMAND parse_number_test)
```

### Error handling

A configuration that cannot work stops at configure time with one readable line (`message(FATAL_ERROR ...)`), as the patch does for `BLAZE_SANITIZE` with MSVC. A test never skips (`scripts/lint.sh`, rule `test-skip`): the three R1 tests still run and are judged by an exact expected report.

### Docs

`docs/BUILDING.md:72-97` (preset table and commands) and `:99-135` (Testing, with one `###` subsection per special check). Docs name code by file and symbol, never by line number (lint `doc-line-citation`), and never state a test count (lint `doc-test-count`).

---

## Files to Change

| File | Action | Covered by the patch | Purpose |
|------|--------|----------------------|---------|
| `CMakeLists.txt` | UPDATE | Yes | Option `BLAZE_SANITIZE` after the `BLAZE_WERROR` block |
| `CMakePresets.json` | UPDATE | Yes | Configure, build and test preset `asan-ubsan` |
| `tests/CMakeLists.txt` | UPDATE | Yes, except the issue number in one comment (Task 3) | The `if(BLAZE_SANITIZE)` block: the two canary tests and the expected-report rule for the three R1 tests |
| `tests/sanitizer_canary.cpp` | CREATE | Yes | The canary program (modes `use-after-free` and `float-cast`) |
| `docs/BUILDING.md` | UPDATE | No (Task 4) | Preset table row and a "Sanitizer build" section with the packages |
| `AGENTS.md` | UPDATE | No (Task 5) | The preset's command line in the Commands block |
| `TODO.md` | UPDATE | No (Tasks 2 and 6) | Two new lines for R1 and R2; tick line 55. Gitignored, so local only |
| `.agents/issues/todo-issues.md` | UPDATE | No (Task 2) | Only what `/create-issues` writes for the two new issues |

Not changed: everything under `src/`, `scripts/check.sh`, `scripts/lint.sh`, `.github/workflows/ci.yml`, `README.md` (it points to `docs/BUILDING.md`), the pinned dependency versions.

---

## Tasks

Execute in order. `/implement` applies the prototype patch before Task 1; no task repeats it.

### Task 1: Build and run the patched preset

- **File**: none (verification of the patch)
- **Action**: VERIFY
- **Implement**:
  1. Before any `cmake` or `scripts/check.sh` run, keep the compile commands of `main`: `cp build/compile_commands.json <scratch>/cc-main.json`. Planning left `build/` configured at `d50744b`. If the file is missing or `build/` was reconfigured on another commit, skip this copy and rely on the `grep` checks in Validation.
  2. Run `scripts/check.sh`. Expect `CHECK: PASS`.
  3. Run the three preset commands from Validation. Expect every test to pass, with `sanitizer_canary_use_after_free` and `sanitizer_canary_float_cast` in the list.
  4. If a test fails with a sanitizer report that is not R1 (a different toolchain can find more): it falls under acceptance criterion 2. Fix it in this change when it is in project code and the fix is local and obvious; otherwise add it to Task 2's list and give it the same kind of expected-report rule. Report every such case. Do not loosen the flags and do not exclude a test.
- **Mirror**: `CMakeLists.txt:83-94` for the option; `CMakePresets.json:90-99` for the preset
- **Validate**: `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan`

### Task 2: File the two miniaudio reports as their own issues

- **File**: `TODO.md`, `.agents/issues/todo-issues.md`, GitHub
- **Action**: UPDATE
- **Implement**: Acceptance criterion 2 needs each report fixed or filed. Append these two lines at the end of the task list in `TODO.md`:

  ```markdown
  - [ ] miniaudio reads freed memory when a sound file fails to load. With a missing or corrupt file, `ma_sound_init_from_file` (called by `SoundStream::load`) reads a data buffer node that miniaudio has just freed; AddressSanitizer reports heap-use-after-free in `ma_resource_manager_data_buffer_node_acquire`. miniaudio 0.11.25 still has it. Fix it or route around it, then remove the expected-report rule for `audio_test`, `select_screen_test` and `preview_player_test` from the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt`, so `ctest --preset asan-ubsan` runs those tests in full. #technical #audio (from the #136 sanitizer run)
  - [ ] miniaudio leaks 744 bytes each time a sound file fails to load. `ma_sound_init_from_file_internal` in miniaudio 0.11.21 does not free its data source when the load fails; LeakSanitizer reports it once the use-after-free is out of the way. miniaudio 0.11.23 fixed it. #technical #audio (from the #136 sanitizer run)
  ```

  Then run the `create-issues` skill on `TODO.md` for these two lines only (`--no-review`). It creates the GitHub issues, writes `(#N)` onto the two lines and adds their entries to `.agents/issues/todo-issues.md`, which is committed with this change (commit `43a72e6` did the same for #141 and #142). If the skill cannot run in this session, create the two issues with `gh issue create --label technical --label audio`, write `(#N)` onto the lines by hand, leave `.agents/issues/todo-issues.md` alone and say so in the report.

  Put these technical notes into the two issues (as the skill's technical notes, or as an issue comment):

  - Both: found by `ctest --preset asan-ubsan` (#136). The failing call is `ma_sound_init_from_file` in `SoundStream::load` (`src/audio/sound_stream.cpp`); `src/audio/assist_tick_player.cpp` has the only other call. Reproduce: `UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test`.
  - Use-after-free: in `miniaudio.h` 0.11.21 the node is freed in the `done:` block of `ma_resource_manager_data_buffer_node_acquire` and its `isDataOwnedByResourceManager` field is read in the next statement. Tags 0.11.22 to 0.11.25 have the same code. Options: (a) a pre-check in `SoundStream::load`: open the file with `ma_decoder_init_file`, return false when that fails, `ma_decoder_uninit`, then load as today. Planning tried this in scratch on unmodified miniaudio: every test passed under the preset, and the leak was gone too because the failing path was no longer reached. (b) A local patch to miniaudio that moves the init-notification block above the free. (c) Report it upstream and wait.
  - Leak: options: the same pre-check (a), or move the miniaudio pin to 0.11.23 or later. The audio library drives the music clock, so a version change needs the measurements in `docs/AUDIO_LATENCY.md` repeated.

  Record the two issue numbers; Tasks 3 and 4 use them as `#<R1>` and `#<R2>`.
- **Mirror**: `TODO.md:53-57` (the style of the `#technical` lines); commit `43a72e6`
- **Validate**: `gh issue view <R1> --json title,state` and `gh issue view <R2> --json title,state` both print an open issue; `grep -c "from the #136 sanitizer run" TODO.md` prints 2

### Task 3: Name the issue in the expected-report rule

- **File**: `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: In the `if(BLAZE_SANITIZE)` block, the comment above the `set_tests_properties(audio_test select_screen_test preview_player_test ...)` call starts with `# Known report:`. Change that to `# Known report (#<R1>):` with the real number. Change nothing else in the block.
- **Mirror**: `tests/CMakeLists.txt:195-196` (comments that start with the issue number)
- **Validate**: `cmake --preset asan-ubsan && ctest --preset asan-ubsan -R "audio_test|select_screen_test|preview_player_test"` passes all three

### Task 4: Document the preset in `docs/BUILDING.md`

- **File**: `docs/BUILDING.md`
- **Action**: UPDATE
- **Implement**:
  1. In the preset table (`:76-83`), add this row after the `benchmark` row:

     ```markdown
     | `asan-ubsan` | Linux / GCC | **Verified here**: the tests under AddressSanitizer + UndefinedBehaviorSanitizer |
     ```

  2. Add this section after "### Fresh-clone check" and before "## Running" (`:137`). Replace `#<R1>` and `#<R2>` with the numbers from Task 2. This text passed `scripts/lint.sh` in the scratch worktree.

     ````markdown
     ### Sanitizer build

     The `asan-ubsan` preset builds the project sources and the tests with GCC's
     AddressSanitizer and UndefinedBehaviorSanitizer. A memory error or undefined
     behaviour then stops the test that reaches it and prints where it happened.

     ```bash
     cmake -B build -DCMAKE_BUILD_TYPE=Release   # once: the preset reads the dependency sources in build/_deps
     cmake --preset asan-ubsan
     cmake --build --preset asan-ubsan -j"$(nproc)"
     ctest --preset asan-ubsan
     ```

     Packages it needs on top of the Linux prerequisites:

     - Fedora / RHEL: `sudo dnf install libasan libubsan`
     - Debian / Ubuntu: none. `build-essential` installs both runtimes with GCC.

     What the preset does:

     - It is a Debug build in `build-asan-ubsan/` (about 1.5 GB). It builds its own
       copy of the dependencies there from the sources in `build/_deps` and never
       writes to `build/`, so the normal builds are not changed or rebuilt.
     - Only the project's sources and tests are instrumented (`BLAZE_SANITIZE` in
       `CMakeLists.txt`). miniaudio and stb compile inside project sources, so they
       are instrumented too. SDL3 and glad are not.
     - The undefined-behaviour checks include `float-cast-overflow` (a number
       converted to a type that cannot hold it), which GCC leaves out of
       `-fsanitize=undefined`. A report ends the test; it is not only printed.
     - `sanitizer_canary_use_after_free` and `sanitizer_canary_float_cast` exist
       only in this build. Each makes one deliberate error and passes only when the
       sanitizer reports it, so a build that lost its sanitizer flags fails.
     - ThreadSanitizer is not part of it: it cannot be combined with
       AddressSanitizer.

     Known reports. Both are in miniaudio 0.11.21, where a sound file fails to load:

     - #<R1>: miniaudio reads a data buffer node that it has just freed
       (`ma_resource_manager_data_buffer_node_acquire`). miniaudio 0.11.25 still
       does.
     - #<R2>: miniaudio leaks 744 bytes (`ma_sound_init_from_file_internal`). Fixed
       in miniaudio 0.11.23.

     `audio_test`, `select_screen_test` and `preview_player_test` load a missing or
     corrupt file, so AddressSanitizer stops them at the first report. Until #<R1> is
     fixed, this preset passes each of the three only when that exact report is in
     its output (the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt`), and the rest
     of each test does not run here. The normal build runs them in full.

     To run one test by hand:
     `UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/<name>`.
     ````

- **Mirror**: `docs/BUILDING.md:124-135` ("### Fresh-clone check")
- **Validate**: `scripts/lint.sh` prints `LINT: PASS`; `grep -c "asan-ubsan" docs/BUILDING.md` is at least 5; `grep -c "#<R" docs/BUILDING.md` prints 0

### Task 5: Add the command to `AGENTS.md`

- **File**: `AGENTS.md`
- **Action**: UPDATE
- **Implement**: In the Commands code block (`:37-58`), add these two lines and a blank line directly above `# One test` (`:53`):

  ```bash
  # Sanitizers: every test under AddressSanitizer + UndefinedBehaviorSanitizer (needs build/ configured; docs/BUILDING.md)
  cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan
  ```

  Do not add a row to "Rules and what enforces them": that table lists what `scripts/check.sh` runs, and this preset is not in it (Open Question 2).
- **Mirror**: `AGENTS.md:50-54` (comment line, then the command)
- **Validate**: `scripts/lint.sh` prints `LINT: PASS`; `grep -c "asan-ubsan" AGENTS.md` prints 1

### Task 6: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:55`: `- [ ]` → `- [x]`. Change only that mark on that line. The file is gitignored, so this is a local edit. The acceptance-criteria ticks in `.agents/issues/todo-issues.md` are `/update-issues`' work; do not tick them by hand.
- **Validate**: `sed -n 55p TODO.md` starts with `- [x]` and contains `(#136)`

---

## Validation

```bash
# Lint, Release build with warnings as errors, every test (the normal check; this work must not change its result)
scripts/check.sh

# The sanitizer preset. First configure about 35 s, first build about 1.5 min at -j16, tests about 10 s
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan -j"$(nproc)"
ctest --preset asan-ubsan

# One test under the preset
ctest --preset asan-ubsan -R config_persistence_test

# The canaries show the two reports
ctest --preset asan-ubsan -R sanitizer_canary -V | grep -E "heap-use-after-free|outside the range"

# Normal builds unchanged (acceptance criterion 4)
grep -c -- "-fsanitize" build/compile_commands.json       # expect 0
grep -c "BLAZE_SANITIZE:BOOL=OFF" build/CMakeCache.txt    # expect 1
ctest --test-dir build -N | grep -c sanitizer_canary      # expect 0
cmp <scratch>/cc-main.json build/compile_commands.json    # expect no output (when Task 1 took the copy)

# The preset left build/ alone: a second check compiles nothing
scripts/check.sh && grep -c "Building C" build/check-logs/build.log   # expect 0

# Only build files, one test file and docs changed
git diff --stat -- src scripts .github                    # expect no output
```

There is no separate type check: the build is the type check. `scripts/lint.sh` runs inside `scripts/check.sh`.

## End-to-End Verification

No game window is needed: no game code changes, so `/verify` does not apply. Everything below is headless and silent (the tests open miniaudio's null device).

1. **Red before** (already measured on `main`): `cmake --preset asan-ubsan` fails with `No such configure preset`.
2. **Green after**: the three preset commands in Validation. Expected: every test passes; the list has the two `sanitizer_canary_*` tests; `audio_test`, `select_screen_test` and `preview_player_test` show `Passed` (by the expected-report rule).
3. **The canaries see the sanitizers**: the `-R sanitizer_canary -V` line in Validation prints one `AddressSanitizer: heap-use-after-free` line and one `runtime error: 1e+300 is outside the range of representable values` line.
4. **The preset catches a real past bug.** Put the old defect back for one run, then remove it:
   1. In `read_integer` (`src/data/high_scores.cpp`), delete the `constexpr double kTwoPow63` line and the whole `if (!std::isfinite(value) || ... ) { return false; }` statement that follows it, so the function casts `value` unchecked.
   2. `cmake --build --preset asan-ubsan --target config_persistence_test && ctest --preset asan-ubsan -R config_persistence_test`
   3. Expected: the test fails, and its output has `runtime error: 1e+300 is outside the range of representable values of type 'long int'` with `read_integer` in the first stack frame.
   4. Restore: `git checkout -- src/data/high_scores.cpp`, then `git status --short src` prints nothing, then rebuild and re-run step 2's command: the test passes.
   Quote the report line in the implementation comment. Never commit the defect.
5. **Normal builds unchanged**: the four "Normal builds unchanged" lines and the "left build/ alone" line in Validation give the expected values.
6. **Final state**: `scripts/check.sh` prints `CHECK: PASS` and `ctest --preset asan-ubsan` passes, both run after step 4's restore.

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The three R1 tests stop at their first failed load in this build, so the rest of each does not run under the sanitizers | Known and documented in `docs/BUILDING.md`. The rule ends itself when R1 is fixed (shown in scratch). The filed issue names the block to remove. They run in full in the normal build | In scope (documented); the fix is the filed issue |
| The expected-report rule would also hide a new, different error in those three tests that happens after R1 | It cannot happen after R1: AddressSanitizer stops the program at R1. An error before R1 changes the output order but R1's summary line still has to appear, so a crash before it fails the test | In scope |
| A different GCC or an Ubuntu host finds more reports | Task 1, step 4 says what to do. CI does not run the preset, so a pull request cannot fail on it | In scope |
| The preset fails on a machine where `build/` was never configured | CMake stops with "Manually specified source directory is missing". `docs/BUILDING.md` and the preset's description give the one command to run first | In scope |
| A dependency is added to `CMakeLists.txt` later and not to the preset's five `FETCHCONTENT_SOURCE_DIR_*` lines | The preset then downloads that one dependency into `build-asan-ubsan/_deps`. It still builds | Out of scope (flag only) |
| The real-time tests flake under instrumentation on a loaded machine | 10 full runs and 50 single runs of the real-time tests were green at planning time. If `perf_loop_test` ever fails here, add `"BLAZE4K_PERF_STRICT": "0"` to the test preset's `environment` and say so | In scope if seen |
| The existing presets (`linux-gcc-release`, `benchmark`, ...) share `build/_deps/*-build` with `build/`, so configuring one makes the next `scripts/check.sh` rebuild SDL | Not changed here: it would alter the other presets. The new preset avoids it (D2). Candidate fix for its own issue: give each `FetchContent_Declare` a `BINARY_DIR` under `${CMAKE_BINARY_DIR}` | Out of scope (flag to owner) |
| CI's dependency cache key is the hash of `CMakeLists.txt`, which this change edits | One uncached dependency fetch on the first CI run. No action | Out of scope (note) |
| `BLAZE_SANITIZE` is written for GCC; Clang accepts the same flags but was not run (no Clang on the host) | The preset names `gcc` and `g++`. The option's help text says GCC or Clang; MSVC stops at configure time | Out of scope |
| `build-asan-ubsan/` takes about 1.5 GB | Stated in `docs/BUILDING.md`. Delete the directory to get the space back | In scope (documented) |
| Timing path touched (principle 1) | Nothing under `src/` changes. The `git diff --stat -- src scripts .github` line in Validation proves it | In scope |
| Sound | Every test opens the null device (`AudioBackend` default). The canary opens no device. Never open the real one | In scope |

## Open Questions

None blocks the work. The plan uses the stated default for each.

1. **Owner decision: file the two miniaudio defects, or fix them in this change?**
   In plain words: the first sanitizer run found two faults in the audio library, not in our code. Both happen only when a sound file cannot be loaded. Neither crashes the game today.
   - **Default (planned, recommended): file each as its own issue** (Task 2) and land the preset with the self-ending expected-report rule (D6). The fix changes how every build loads a sound file, which is game code and deserves its own review; acceptance criterion 4 says the normal builds stay unchanged.
   - Alternative: fix now with the pre-check in `SoundStream::load` (about 10 lines; tried in scratch, all tests then pass under the preset with no rule). Then drop Task 2's issues and Task 3, remove the expected-report rule from `tests/CMakeLists.txt`, and add a line to the audio docs. The game opens each sound file twice on load.

2. **Owner decision: should the preset run automatically?** The issue's goal is that these bugs are "caught automatically", but its acceptance criteria ask only for a preset.
   - **Default (planned): the preset is run by hand**, and `AGENTS.md` lists its command. Nothing in CI or `scripts/check.sh` changes.
   - Recommended follow-up, as its own issue: a second CI job (or a `--sanitize` flag on `scripts/check.sh`) that runs the three preset commands after the normal check. Measured cost on the host: 10 s of tests on top of an incremental build; from nothing, about 2 min at 16 cores, so several minutes on a CI runner. A row in the `AGENTS.md` rule table belongs to that change, not this one.

3. **Dependency sources: reuse `build/_deps`, or download a second copy?**
   - **Default (planned): reuse** (D2). Offline and no extra 340 MB, but `build/` must have been configured once.
   - Alternative: delete the five `FETCHCONTENT_SOURCE_DIR_*` lines from the preset. It then works on a machine with no `build/`, and downloads the dependencies again on its first configure.

4. **One issue per report, or one for both?** Acceptance criterion 2 says "each sanitizer report ... as its own issue".
   - **Default (planned): two issues.** They have different upstream states (one fixed in 0.11.23, one not fixed).
   - Alternative: one issue for both, since one pre-check fixes both. Then merge Task 2's two lines into one.
   - If the owner prefers to file the issues himself, he should say so before `/implement` runs; Tasks 3 and 4 then wait for the numbers.

## Acceptance Criteria

- [ ] `cmake --preset asan-ubsan`, `cmake --build --preset asan-ubsan` and `ctest --preset asan-ubsan` build and run the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer (issue criterion 1)
- [ ] Every test passes under the preset. The two sanitizer reports found (both in miniaudio 0.11.21) are each filed as their own issue, and the three tests that reach the first one pass only on that exact report (issue criterion 2)
- [ ] With the old `read_integer` range check removed for one run, `config_persistence_test` fails under the preset with the `float-cast-overflow` report; the defect is restored afterwards
- [ ] The two `sanitizer_canary_*` tests pass in the preset and do not exist in the normal build
- [ ] `docs/BUILDING.md` has the preset in its table and a "Sanitizer build" section with the commands, the Fedora and Debian/Ubuntu packages, and the known reports (issue criterion 3)
- [ ] The normal Release and Debug builds are unchanged: no `-fsanitize` in `build/compile_commands.json`, `BLAZE_SANITIZE` off, `build/` not rebuilt by the preset, nothing under `src/` changed (issue criterion 4)
- [ ] `AGENTS.md` lists the preset's command
- [ ] `scripts/check.sh` prints `CHECK: PASS`; no new compiler warning in project sources
- [ ] `TODO.md:55` ticked (local), and the two new lines carry their issue numbers
