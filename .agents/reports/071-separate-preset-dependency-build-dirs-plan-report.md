# Implementation Report

**Plan**: `.agents/plans/completed/071-separate-preset-dependency-build-dirs-plan.md`
**Branch**: `feature/071-separate-preset-dependency-build-dirs`
**Status**: COMPLETE

## Summary

Separated preset dependency build directories across all CMake presets while keeping downloaded sources shared in `build/_deps`. Added `BLAZE_DEPS_SOURCE_DIR` cache variable defaulting to `${CMAKE_SOURCE_DIR}/build/_deps` in `CMakeLists.txt` and passed `SOURCE_DIR` to each `FetchContent_Declare` call. Removed `FETCHCONTENT_BASE_DIR` from the `base` preset in `CMakePresets.json` so each preset's dependency build tree defaults to `${CMAKE_BINARY_DIR}/_deps` (`build-<preset>/_deps`), and simplified `asan-ubsan` by removing manual source overrides. Updated `scripts/fresh-clone-check.sh` to pass `-DBLAZE_DEPS_SOURCE_DIR` in offline mode, added `Language: Json; DisableFormat: true` to `.clang-format` to support JSON format checks, and updated `docs/BUILDING.md` and `TODO.md`.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Update `.clang-format` to support JSON files without error | `.clang-format` | PASS |
| 2 | Declare shared dependency source directory and pass to `FetchContent_Declare` | `CMakeLists.txt` | PASS |
| 3 | Isolate preset dependency builds in `CMakePresets.json` | `CMakePresets.json` | PASS |
| 4 | Update `scripts/fresh-clone-check.sh` to pass `BLAZE_DEPS_SOURCE_DIR` | `scripts/fresh-clone-check.sh` | PASS |
| 5 | Update documentation in `docs/BUILDING.md` | `docs/BUILDING.md` | PASS |
| 6 | Check off TODO-45 in `TODO.md` | `TODO.md` | PASS |

## Validation Results

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Type check / Build | PASS | `scripts/check.sh` (`cmake --build build`) — host (Linux x86_64 GCC 16.2.1) |
| Lint | PASS | `scripts/lint.sh` — host |
| Format | PASS | `scripts/format.sh --check` — host |
| Tests | PASS (53 passed) | `scripts/check.sh` (`ctest --test-dir build`) — host |
| Presets Build & Test (Release) | PASS (53 passed) | `cmake --preset linux-gcc-release && cmake --build --preset linux-gcc-release -j"$(nproc)" && ctest --preset linux-gcc-release` — host |
| Presets Build & Test (Debug) | PASS (53 passed) | `cmake --preset linux-gcc-debug && cmake --build --preset linux-gcc-debug -j"$(nproc)" && ctest --preset linux-gcc-debug` — host |
| Presets Isolation (No rebuild) | PASS | `cmake --build --preset linux-gcc-release` (built instantly, SDL3 target up-to-date) — host |
| Sanitizer Preset | PASS (55 passed) | `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan` — host |
| Sanitizer Canaries | PASS (2 passed) | `ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error` — host |
| Offline Fresh Clone Check | PASS (53 passed) | `scripts/fresh-clone-check.sh` — host |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `.clang-format` | UPDATE | +4/-0 |
| `CMakeLists.txt` | UPDATE | +10/-0 |
| `CMakePresets.json` | UPDATE | +3/-12 |
| `docs/BUILDING.md` | UPDATE | +8/-12 |
| `scripts/fresh-clone-check.sh` | UPDATE | +2/-2 |
| `TODO.md` | UPDATE | +1/-1 (gitignored, modified in place) |

## Deviations from Plan

`TODO.md` is gitignored per repo rules (`AGENTS.md`) and was modified in place; it does not appear in git tracked status or diffs.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| None | N/A (Build system and tooling enhancement; verified through automated preset builds, sanitizer canary suite, and fresh-clone check) |
