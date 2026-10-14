# Code Review: feature/071-separate-preset-dependency-build-dirs

**Scope**: branch `feature/071-separate-preset-dependency-build-dirs` (vs `main` merge base, including uncommitted changes)
**Recommendation**: APPROVE

## Summary

Reviewed build system and configuration changes across 5 files (`CMakeLists.txt`, `CMakePresets.json`, `docs/BUILDING.md`, `scripts/fresh-clone-check.sh`, `.clang-format`) that isolate dependency compilation directories per CMake preset while retaining shared source trees in `build/_deps`. The implementation is clean, robust, and completely eliminates cross-preset dependency recompilation and `build/` cache invalidation.

Acceptance criteria verification against GitHub issue #148:
- **Configuring any preset changes no file under `build/`, and a `scripts/check.sh` run afterwards rebuilds no dependency**: Met. Verified by comparing `build/` file timestamps before and after configuring `linux-gcc-release` (0 files modified under `build/`) and confirming `scripts/check.sh` / `cmake --build build` rebuilt no dependency targets (`[  %] Built target ...`).
- **A Debug preset and a Release preset can be configured and built one after the other without rebuilding each other's dependency libraries**: Met. Verified by building `linux-gcc-release`, then configuring and building `linux-gcc-debug`, and immediately re-invoking `cmake --build --preset linux-gcc-release`, which completed instantly with no re-compilation of SDL3 or other dependency libraries.
- **The dependency sources are still downloaded only once and shared; a preset that downloads them on a new machine today still does**: Met. Verified that `FetchContent_Declare` with `SOURCE_DIR "${BLAZE_DEPS_SOURCE_DIR}/<dep>-src"` downloads missing sources into the shared directory upon initial configure and tracks `GIT_TAG` revisions without duplicating source trees.
- **`scripts/fresh-clone-check.sh` still passes in its offline mode**: Met. Executed `./scripts/fresh-clone-check.sh` and confirmed 100% tests passed (53 of 53 tests) in offline mode using `-DBLAZE_DEPS_SOURCE_DIR=$repo_root/build/_deps`.
- **`docs/BUILDING.md` says where each preset keeps its dependency sources and its dependency builds**: Met. Verified documentation in `docs/BUILDING.md` explicitly documents the layout for `BLAZE_DEPS_SOURCE_DIR` (`build/_deps`) and preset build directories (`build-<preset>/_deps`).

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
| Build / Type Check | PASS | `scripts/check.sh` (`cmake --build build`), `cmake --build --preset linux-gcc-release`, `cmake --build --preset linux-gcc-debug`, and `cmake --build --preset asan-ubsan` — host (Linux x86_64 GCC 16.2.1) |
| Warnings | NONE | Built with `-DBLAZE_WERROR=ON` across all targets with zero compiler warnings |
| Lint | PASS | `scripts/lint.sh` and `scripts/format.sh --check` — host |
| Tests | PASS | 53/53 tests passed in default build, 53/53 in release preset, 53/53 in debug preset, 55/55 in `asan-ubsan` (including 2 sanitizer canary tests), and 53/53 in `fresh-clone-check.sh` |
| UI verification | N/A | No user interface, rendering, or gameplay logic modified |

## What's Good

- Elegant and minimal solution: introducing `BLAZE_DEPS_SOURCE_DIR` and setting `SOURCE_DIR` in `FetchContent_Declare` solves the shared source vs separate build directory conflict natively through standard CMake mechanisms without complex wrapper scripts.
- Cleans up `asan-ubsan` preset configuration by removing five individual `FETCHCONTENT_SOURCE_DIR_<NAME>` overrides and removing previous limitations where `build/` had to be configured first.
- Prevents `git-clang-format` failures on JSON files by adding `Language: Json; DisableFormat: true` to `.clang-format`.
- All acceptance criteria are thoroughly satisfied and validated across multiple configurations.

## Recommendation

Ready to merge into `main`.
