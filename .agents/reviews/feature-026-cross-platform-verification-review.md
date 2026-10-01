# Code Review: feature/026-cross-platform-verification (issue #26, D4)

**Scope**: Branch `feature/026-cross-platform-verification` vs `main`, including uncommitted working-tree changes (7 tracked modifications + 7 new paths: `src/app/frame_stats.hpp`, `tests/frame_stats_test.cpp`, `tests/perf_loop_test.cpp`, `CMakePresets.json`, `scripts/fresh-clone-check.sh`, `docs/BUILDING.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`; plus plan/report artifacts).
**Recommendation**: APPROVE WITH NITS (needs none of the findings to ship; one Medium worth fixing)

## Summary

D4 is a verification/measurement task, and the change set keeps that contract: the only production code touched is presentation-only `FrameStats` instrumentation plus opt-in `--perf-report`/`--perf-budget-ms`, and the judgment path stays on `MusicClock`. The honesty requirement is met — Windows/macOS builds, real FPS, pad traversal, and OpenITG feel are all marked `❌ owner`/`⚠️ proxy` in `docs/CROSS_PLATFORM_VERIFICATION.md`, and the headless benchmark is explicitly labelled a CPU-budget proxy. Automated validation is green (34/34, fresh-clone PASS, presets PASS, no project warnings); findings are documentation/portability nits, led by a CMake-presets schema/minimum inconsistency.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

1. **`CMakePresets.json:2` — preset schema version contradicts the project's declared CMake minimum.** The file uses `"version": 6` (schema introduced in CMake **3.25**) while `cmakeMinimumRequired.minor` is `24`, and the root `CMakeLists.txt`, `AGENTS.md`, and `docs/BUILDING.md` all state CMake ≥ 3.24. A host with exactly CMake 3.24 cannot load the preset file at all, so the headline deliverable (the preset matrix the docs tell users to run) is unusable at the documented minimum. None of the version-6 features (package/workflow presets, `outputJUnitFile`) are used — recommend `"version": 5` (all used fields exist in 5), or raise the project/document minimum to 3.25.

### Suggestions (Low)

2. **`scripts/fresh-clone-check.sh:107` — hardcoded test count `"100% tests passed out of 34"` is brittle.** Any added/removed test, or a skipped test (which changes CTest's summary format), makes the gate fail with an unhelpful `got: unknown` message even when nothing is wrong. Prefer deriving the expected count (or trusting `ctest`'s exit code plus an explicit zero-failures check). Conservative rather than fake-green, but noisy.
3. **`scripts/fresh-clone-check.sh:47-51` — no cleanup trap; the `mktemp -d` work tree and logs are always left behind.** The script prints the paths, so keeping them is arguably intentional, but there is no `trap` and no opt-out (`--keep`/`--rm`). Either document that the temp tree is deliberately retained or clean it on success.
4. **`scripts/fresh-clone-check.sh:58` — `tar --ignore-failed-read` is a GNU tar extension.** The script header scopes it to the "Linux leg", but `docs/BUILDING.md` presents the fresh-clone check generically; on macOS/BSD tar this option errors. Note the Linux/GNU-tar requirement (or drop the flag, since `set -e`/`pipefail` already surface real read failures).
5. **`docs/CROSS_PLATFORM_VERIFICATION.md:39` and implementation report:49 — quoted perf numbers are non-reproducible snapshots and disagree with each other** (`p99 = 0.006 ms` in the doc vs `0.004 ms` in the report; this reviewer's runs gave 0.006–0.011 ms). Report a range/order-of-magnitude ("< 0.02 ms") instead of a precise value that changes every run.
6. **`docs/BUILDING.md:16` — compiler minimums `GCC 13+ / Clang 16+` are unsourced**, and the Ubuntu package list (`:30-33`) may be incomplete for a genuinely clean SDL3 build (SDL3 commonly also pulls `libxkbcommon-dev`, `wayland-protocols`, `libudev-dev`). Either cite the source for the compiler floor or phrase it as "a recent C++20 compiler".
7. **`README.md:5` — "v1 feature-complete (Phases A–D)"** reads as more finished than D4's state, where every owner-manual AC is still unchecked. Consider "v1 feature-complete; D4 cross-platform verification owner steps pending" to match the linked checklist.
8. **`src/main.cpp:147` — `std::stod` accepts trailing garbage** (`--perf-budget-ms 16.67abc` silently becomes 16.67). Minor CLI-validation gap; validate the full string (`std::stod(value, &pos)` and require `pos == value.size()`).
9. **`src/app/app.cpp:111` — `print_perf_report()` leaves `std::cout` in `std::fixed`/`setprecision(3)` state.** Harmless because it runs at exit, but restoring the stream flags (or formatting into a local stream) avoids surprising any future post-`run()` output.

### Noted, not a finding (explicitly scoped/accepted by the plan)

- `src/app/app.cpp:74` samples `frame_dt` **after** the `max_frame_dt` clamp, so the reported `max` can never exceed 250 ms. The plan pinned "after the accumulator clamp"; the hitch count (`over_budget`) still flags such stalls, but the docs do not mention the cap — worth one sentence if precise maxima matter.
- The short headless `--perf-report` run reports `FAIL` because the first frame absorbs startup (observed: `p99 = 50.520 ms`). This is documented in the output line and the report, and is irrelevant for the owner's full-song run.
- `perf_loop_test`'s 50 ms `max` constant and 16.67 ms p99 gate are documented design decisions (plan OQ2), with the `BLAZE4K_PERF_STRICT=0` escape hatch.
- Headless benchmark is not a GPU/vsync FPS proof; correctly stated in the test banner and docs.

## Validation Results

| Check | Status |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | PASS |
| Build (`cmake --build build -j16`) | PASS — 0 warnings in `src/`/`tests/` (1940 `warning` lines are all fetched deps) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 100% out of 34 (0.53 s) |
| `frame_stats_test` / `app_test` / `perf_loop_test` / `screen_manager_test` | PASS (perf: song Cleared 94 steps, p99 0.006–0.011 ms, hitches 0) |
| `BLAZE4K_PERF_STRICT=0` escape hatch | PASS — prints "advisory only" banner and skips budget assertions (code-verified: `strict = !(env == "0")`); `BLAZE4K_PERF_STRICT=1`/unset → strict. No flake under 32 concurrent CPU hogs |
| `--perf-report` E2E + `--perf-budget-ms` (valid/invalid/negative/missing) | PASS — invalid values warn and keep default; `--perf-budget-ms 100` → PASS |
| Purity (`SDL_/glad/miniaudio/chrono/GetTicks/std::time/<ctime>` in `frame_stats.hpp`) | PASS — no matches |
| Scope (no `gameplay/timing/chart/audio/render/data/input/screens/window.` edits; `FrameStats` only under `src/app/`) | PASS |
| Fresh clone (`./scripts/fresh-clone-check.sh`) | PASS — "100% tests passed out of 34" |
| Presets (`cmake --preset linux-gcc-release` → build → `ctest --preset linux-gcc-release`) | PASS — 34/34; sibling `binaryDir` resolves fixtures correctly |
| Preset listing (`--list-presets` =build =test) | PASS; Windows MSVC configure presets filtered out on Linux (documented, generator-availability filter) |
| README status + doc links | PASS — no `pre-implementation`; all four links resolve |
| Shell lint (`shellcheck`) | NOT AVAILABLE on host — `bash -n` syntax check PASS |
| Native Windows/macOS, real GPU FPS, pad traversal, OpenITG feel | NOT RUN & NOT CLAIMED — owner checklist only, honestly marked |

## What's Good

- **Scope discipline is real, not just asserted.** `git diff --name-only` touches no gameplay/scoring/timing/render/screens/persistence file, and `FrameStats` appears only under `src/app/`; the judgment path remains `MusicClock`.
- **`frame_stats.hpp` is genuinely pure** — `<algorithm>/<cmath>/<cstddef>/<vector>` only, no SDL/GL/audio/wall-clock, with negative/non-finite samples rejected and an order-independent nearest-rank percentile.
- **Honest labelling throughout** — the benchmark banner, `docs/BUILDING.md`, and the ✅/⚠️/❌ AC map in `docs/CROSS_PLATFORM_VERIFICATION.md` never present Windows/macOS/pad/feel as verified; the sign-off table leaves owner rows blank.
- **The escape hatch is real and the gate is not flaky** — `BLAZE4K_PERF_STRICT=0` disables budget assertions, and the generous gate held even under heavy CPU contention.
- Deterministic math is properly unit-tested, and the CLI validation (`isfinite`, `> 0`) rejects NaN/Inf/negative budgets with a readable warning.

## Recommendation

Approve with nits. The Medium preset-schema/minimum mismatch should be fixed before merge (one-line `"version": 5` or a docs/minimum bump); the remaining items are documentation/robustness polish. No gameplay-affecting change was found, and the verification record is honest about what was and was not machine-checked.
