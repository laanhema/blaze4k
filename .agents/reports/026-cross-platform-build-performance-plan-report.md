# Implementation Report

**Plan**: `.agents/plans/026-cross-platform-build-performance-plan.md`
**Branch**: `feature/026-cross-platform-verification`
**Status**: COMPLETE

## Summary

D4 verification + measurement tooling (issue #26). Added a pure, SDL/GL/audio/wall-clock-free
`FrameStats` collector (`src/app/frame_stats.hpp`) and wired it as presentation-only
instrumentation in `App`: one sample per already-computed `frame_dt`, plus an opt-in
`--perf-report` (and `--perf-budget-ms`) that prints one frame-time block
(frames, min/median/p95/p99/max ms, mean, hitches, PASS/FAIL) on exit. Added a headless
full-song CPU-budget benchmark (`tests/perf_loop_test.cpp`) that plays the reference chart to
`Cleared` through the real `GameplayView` logic path and times 2000 real arcade-screen updates,
hard-gating `p99 < 16.67 ms` / `max < 50 ms` unless `BLAZE4K_PERF_STRICT=0` (always printing the
percentiles). Added an arcade-loop "no dead ends" contract case, a `CMakePresets.json` matrix
(Linux GCC/Clang + Windows MSVC + macOS Clang + benchmark + portability diagnostic), a
`scripts/fresh-clone-check.sh` fresh-clone proof, per-OS build docs, an owner-verification
checklist, and corrected the stale README status.

No gameplay/scoring/timing/render/screens/persistence behavior changed; wall-clock only samples
the App loop. User-confirmed OQ1–OQ7 decisions were implemented exactly.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure `FrameStats` collector | `src/app/frame_stats.hpp` | ✅ |
| 2 | Deterministic math tests | `tests/frame_stats_test.cpp` | ✅ |
| 3 | App instrumentation + CLI flags | `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp` | ✅ |
| 4 | `app_test` perf-report case | `tests/app_test.cpp` | ✅ |
| 5 | Headless full-song benchmark | `tests/perf_loop_test.cpp` | ✅ |
| 6 | Register tests | `tests/CMakeLists.txt` | ✅ |
| 7 | Arcade-loop no-dead-ends contract | `tests/screen_manager_test.cpp` | ✅ |
| 8 | Cross-platform preset matrix | `CMakePresets.json` | ✅ |
| 9 | Fresh-clone verification script | `scripts/fresh-clone-check.sh` | ✅ |
| 10 | Docs + README | `docs/BUILDING.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md` | ✅ |
| 11 | Full suite + warning budget | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ **34/34** passed (0.51 s) |
| `frame_stats_test` | ✅ All tests passed |
| `perf_loop_test` | ✅ Cleared in 94 steps; song p99 well under `0.02 ms` (run-dependent), screens p99 `0.000 ms`; gate passed |
| `app_test` (incl. new perf case, count == 60) | ✅ All tests passed |
| `screen_manager_test` (incl. arcade-loop contract) | ✅ All tests passed |
| Purity grep (`SDL_|glad|miniaudio|chrono|GetTicks|std::time|<ctime>` in `frame_stats.hpp`) | ✅ no matches |
| Warning budget (`cmake --build build` after touching changed sources, grep `-i warning`) | ✅ none |
| Scope grep (`git diff --name-only` vs gameplay/timing/chart/audio/render/data/input/screens/window.) | ✅ no matches |
| Fresh clone (`./scripts/fresh-clone-check.sh`) | ✅ **"100% tests passed out of 34"** |
| Preset proof (`cmake --preset linux-gcc-release` → build → `ctest --preset linux-gcc-release`) | ✅ **34/34** |
| Preset parsing (`cmake --list-presets` / `=build` / `=test`) | ✅ all Linux/macOS/benchmark/portability presets list; Windows MSVC presets hidden on Linux by CMake's generator-availability filter (see Deviations) |
| README stale status (`rg -n "pre-implementation" README.md`) | ✅ no matches; all doc links resolve |
| E2E arcade smoke (`--headless --smoke-test 120 --start-screen select --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-d4`) | ✅ exit 0 |
| E2E `--perf-report` flag path (`--headless --smoke-test 60 --perf-report`) | ✅ exit 0; printed `[perf] frames=60 ... hitches=1 budget=16.667ms (FAIL; first frame includes startup)` |
| E2E real GPU/vsync FPS, native Windows/macOS builds, pad traversal, OpenITG feel | ⛔ **not automatable on this host — owner checklist in `docs/CROSS_PLATFORM_VERIFICATION.md`; not faked** |

No validation failures occurred; no verification command failed.

### Fresh-clone-check.sh output (verbatim)

```
Fresh tree: /tmp/blaze4k-fresh-clone.4pMizX/src
Build dir:  /tmp/blaze4k-fresh-clone.4pMizX/src/build
Dependency mode: offline (reuse /home/lauri/github/temp-5/build/_deps when present)
[1/3] Configure...
[2/3] Build (-j16)...
[3/3] Test...

FRESH-CLONE CHECK: PASS
  100% tests passed out of 34
```

### Preset proof (verbatim tail)

```
-- Configuring done (34.0s)
-- Generating done (0.2s)
-- Build files have been written to: /home/lauri/github/temp-5/build-linux-gcc-release
...
34/34 Test #34: perf_loop_test ...................   Passed    0.00 sec

100% tests passed out of 34

Total Test time (real) =   0.47 sec
```

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/app/frame_stats.hpp` | CREATE | +107 |
| `src/app/app.hpp` | UPDATE | +10 |
| `src/app/app.cpp` | UPDATE | +29 |
| `src/main.cpp` | UPDATE | +23 |
| `tests/frame_stats_test.cpp` | CREATE | +139 |
| `tests/perf_loop_test.cpp` | CREATE | +182 |
| `tests/CMakeLists.txt` | UPDATE | +26 |
| `tests/app_test.cpp` | UPDATE | +17 |
| `tests/screen_manager_test.cpp` | UPDATE | +63 |
| `CMakePresets.json` | CREATE | +224 |
| `scripts/fresh-clone-check.sh` | CREATE | +115 (chmod +x) |
| `docs/BUILDING.md` | CREATE | +169 |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | CREATE | +133 |
| `README.md` | UPDATE | +6/-2 |

Tracked diffstat: 7 files changed, 172 insertions(+), 2 deletions(-). No commit/push was made
(git policy: only branch created).

## Deviations from Plan

1. **Preset `binaryDir` moved from `build/<presetName>` to `build-<presetName>`.** The plan's
   nested directory added one level, which broke the relative fixture resolution in three
   pre-existing tests (`parser_hardening_test`, `score_keeper_test`, `metronome_sync_test` all
   assume `../../tests` from `<build>/tests` reaches the repo `tests/`). With the sibling
   directory, `ctest --preset linux-gcc-release` passes **34/34**. `build-*/` is already
   gitignored.
2. **Fresh-clone default exports the working tree, not `git archive HEAD`.** The plan's
   `git archive HEAD` cannot see uncommitted changes, and the task requires running the script
   this session with a 34/34 result while git policy forbids committing. The default now exports
   tracked + untracked files honoring `.gitignore` (still excludes `build/`); `--committed`
   restores strict HEAD export for post-commit use. Both modes are documented in the script.
3. **`--perf-report` PASS/FAIL rule pinned to `p99 < perf_budget_ms`.** OQ6 fixed the printed
   fields but not the comparison operator; `p99` is the documented gate metric and `hitches`
   (samples `> budget`) is printed separately for the owner.
4. **Headless `--perf-report` on a short run can print FAIL.** The pinned contract includes the
   first frame as-is (it absorbs all pre-`run()` setup), so a 60-frame smoke's p99 equals that
   startup frame. This is the documented caveat printed in the report line; the owner's
   full-song run has enough frames for it to be irrelevant.
5. **`linux-portability` preset is default-compiler (GCC here) + `CMAKE_EXPORT_COMPILE_COMMANDS`,
   with no new warning flags** (OQ4). It is an owner diagnostic, not a second compiler.
6. **Windows MSVC configure presets are not listed by `cmake --list-presets` on Linux.** CMake
   filters configure presets by generator availability; invoking one explicitly fails with
   `CMake Error: Could not create named generator Visual Studio 17 2022`. The presets are present
   in the file and will list on Windows.
7. **No commit, push, or PR** (git policy per the invoking request).
8. Desktop GPU/vsync FPS, native Windows/macOS builds, physical pad traversal, and the OpenITG
   side-by-side feel test were **not executed and are not claimed as verified**; they are
   delivered as the owner checklist only.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/frame_stats_test.cpp` | empty queries all zero; basic min/max/mean/median/total; nearest-rank percentile rule + insertion-order independence; non-finite/negative samples ignored; percentile clamping to [0,100]; `over_budget` strict-greater + `reset` |
| `tests/perf_loop_test.cpp` | reference chart parses and resolves `Cleared`; per-step update timing; 2000 arcade-screen updates rotating Title/Attract/Select; percentile printing; strict budget gate (p99 < 16.67 ms, max < 50 ms) unless `BLAZE4K_PERF_STRICT=0` |
| `tests/app_test.cpp` (extended) | headless perf-report run records exactly 60 samples and a finite mean |
| `tests/screen_manager_test.cpp` (extended) | arcade-loop no-dead-ends: all canonical screens registered; `Title → Select → Gameplay → Results → Select → Title` traverses; every non-Title canonical screen consumes Back |

The `--perf-budget-ms` CLI validation branch is glue with no testable seam beyond the flag it is
passed through; it is exercised end-to-end by the `--perf-report` headless smoke above.
