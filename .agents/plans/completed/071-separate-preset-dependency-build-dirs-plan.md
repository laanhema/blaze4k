# Plan: Give each build preset its own dependency build directory (#148)

## Summary

The hidden `base` preset in `CMakePresets.json` currently sets `FETCHCONTENT_BASE_DIR` to `${sourceDir}/build/_deps`. Because CMake's FetchContent derives both the downloaded source directory (`<dep>-src`) and the dependency compilation output directory (`<dep>-build`) from `FETCHCONTENT_BASE_DIR`, configuring any preset (such as `linux-gcc-release` or `linux-gcc-debug`) writes its dependency builds directly into `build/_deps/`, overwriting the builds used by `build/` and by other presets. As a result, subsequent `scripts/check.sh` runs rebuild SDL3 and other dependencies, and Debug and Release presets overwrite each other's compiled dependency libraries.

This change separates the dependency build directories across all presets while keeping downloaded sources shared in `build/_deps`. A CMake cache variable, `BLAZE_DEPS_SOURCE_DIR` (defaulting to `${CMAKE_SOURCE_DIR}/build/_deps`), is introduced in `CMakeLists.txt` and passed as the explicit `SOURCE_DIR` to each of the five `FetchContent_Declare` calls (SDL3, glad, nlohmann_json, miniaudio, stb). In `CMakePresets.json`, `FETCHCONTENT_BASE_DIR` is removed from `base`, allowing CMake to default each preset's dependency build tree to its own `binaryDir` (`${sourceDir}/build-${presetName}/_deps`). This also allows simplifying `asan-ubsan` by removing its five manual `FETCHCONTENT_SOURCE_DIR_<NAME>` overrides, removing its previous prerequisites and allowing it to download sources and track `GIT_TAG` pins like every other preset. `scripts/fresh-clone-check.sh` is updated to pass `-DBLAZE_DEPS_SOURCE_DIR` in offline mode, `docs/BUILDING.md` is updated to document the layout, and `.clang-format` is given a `Language: Json; DisableFormat: true` section so `git-clang-format` does not fail on JSON files. A verified prototype patch is saved beside this plan.

## User Story

As a developer configuring and building presets (Release, Debug, Sanitizer) in Blaze 4k  
I want each preset to maintain its own independent dependency build directory while sharing downloaded sources in `build/_deps`  
So that configuring or building one preset never invalidates `build/` or another preset's compiled dependency libraries.

## Metadata

| Field | Value |
|---|---|
| Type | ENHANCEMENT (build system and tooling) |
| Complexity | MEDIUM |
| Systems Affected | `CMakeLists.txt`, `CMakePresets.json`, `docs/BUILDING.md`, `scripts/fresh-clone-check.sh`, `.clang-format`, `TODO.md` |
| GitHub Issue | #148 ([TODO-45] Give each build preset its own dependency build directory) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/071-separate-preset-dependency-build-dirs-prototype.patch` |
| Branch (suggested) | `feature/071-separate-preset-dependency-build-dirs` |
| Plan sequence | `071`. `070` was #147 (`.agents/plans/completed/070-run-sanitizer-build-in-ci-plan.md`). |

---

## Environment Findings

Planned on `main` @ `f4793cd`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| CMake | 4.4.3 | Supports `FetchContent` `SOURCE_DIR` in `FetchContent_Declare` |
| Compiler | GCC 16.2.1 (Fedora 44), 16 cores | Linux host with GCC only |
| Baseline `scripts/check.sh` | Prints `CHECK: PASS`, 53 of 53 tests | Verified at planning time |
| Baseline `fresh-clone-check.sh` | Prints `FRESH-CLONE CHECK: PASS`, 53 of 53 tests | Verified at planning time |
| Issue symptom reproduced | `cmake --preset linux-gcc-release` touched `build/_deps/glad-build`, `nlohmann_json-build`, `sdl3-build`, causing rebuild in `build/` | Observed live during Phase 2 exploration |
| Prototype verification | Prototype tested in isolated worktree: `linux-gcc-release` (53/53 tests pass), `linux-gcc-debug` (53/53 tests pass), `asan-ubsan` (55/55 tests pass including both canaries), no rebuild when switching presets, `fresh-clone-check.sh` passes, `scripts/check.sh` passes | Verified at planning time |
| Formatter discovery | `git-clang-format` inspects `.json` by default; without a `Language: Json` section in `.clang-format`, editing `CMakePresets.json` caused clang-format to exit code 2 | Fixed in prototype by adding `Language: Json; DisableFormat: true` to `.clang-format` |
| Audio in tests | Silent null audio device | Null backend used by default |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Set `SOURCE_DIR "${BLAZE_DEPS_SOURCE_DIR}/<dep>-src"` on each `FetchContent_Declare` in `CMakeLists.txt` | Unlike `FETCHCONTENT_SOURCE_DIR_<NAME>`, passing `SOURCE_DIR` to `FetchContent_Declare` instructs FetchContent to manage git clones at that location. If the directory is missing, FetchContent clones it there; if `GIT_TAG` changes, FetchContent updates it. |
| D2 | Introduce `BLAZE_DEPS_SOURCE_DIR` defaulting to `${CMAKE_SOURCE_DIR}/build/_deps` | Keeps downloaded dependency sources in the canonical `build/_deps` location that `.github/workflows/ci.yml` caches and developers expect, while allowing scripts like `fresh-clone-check.sh` to point to another source tree when testing offline fresh clones. |
| D3 | Remove `FETCHCONTENT_BASE_DIR` from the `base` preset in `CMakePresets.json` | With `FETCHCONTENT_BASE_DIR` omitted from `base`, CMake defaults each preset's dependency build tree to `${CMAKE_BINARY_DIR}/_deps` (i.e. `${sourceDir}/build-${presetName}/_deps`). Each preset's object files and libraries are completely isolated. |
| D4 | Remove `FETCHCONTENT_SOURCE_DIR_<NAME>` and `FETCHCONTENT_BASE_DIR` from `asan-ubsan` | `asan-ubsan` now automatically inherits `build-asan-ubsan/_deps` from `base` and reads shared sources via `BLAZE_DEPS_SOURCE_DIR`. It no longer requires `build/` configured first, and it tracks `GIT_TAG` changes automatically, removing both limitations documented in #136. |
| D5 | Update `scripts/fresh-clone-check.sh` to pass `-DBLAZE_DEPS_SOURCE_DIR=$repo_root/build/_deps` in offline mode | In offline mode, the exported fresh tree reads the parent repo's downloaded sources without polluting the parent repo's `build/_deps/*-build` directories. |
| D6 | Add `Language: Json; DisableFormat: true` to `.clang-format` | `git-clang-format` inspects `.json` files by default. Without a `Json` language section, clang-format errors out with `Configuration file(s) do(es) not support Json`. Disabling JSON formatting in `.clang-format` keeps `scripts/format.sh --check` green when `.json` files are edited. |

---

## Patterns to Follow

### Naming
```cmake
// SOURCE: CMakeLists.txt:87
option(BLAZE_WERROR "Treat compiler warnings in project sources as errors" OFF)

// Cache variable pattern for project-specific CMake settings:
set(BLAZE_DEPS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/_deps" CACHE PATH
    "Directory for shared dependency sources")
```

### Dependency Declaration
```cmake
// SOURCE: CMakeLists.txt:25-30
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8
    GIT_SHALLOW TRUE
    SOURCE_DIR "${BLAZE_DEPS_SOURCE_DIR}/sdl3-src"
)
```

### Presets
```json
// SOURCE: CMakePresets.json:9-12
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build-${presetName}"
    },
```

---

## Files to Change

| File | Action | Purpose | Covered by Prototype Patch? |
|---|---|---|---|
| `.clang-format` | UPDATE | Add `Language: Json; DisableFormat: true` so `git-clang-format` handles `.json` files without error | Yes |
| `CMakeLists.txt` | UPDATE | Add `BLAZE_DEPS_SOURCE_DIR` cache variable and pass `SOURCE_DIR` to all 5 `FetchContent_Declare` calls | Yes |
| `CMakePresets.json` | UPDATE | Remove `FETCHCONTENT_BASE_DIR` from `base` preset; simplify `asan-ubsan` | Yes |
| `scripts/fresh-clone-check.sh` | UPDATE | Pass `-DBLAZE_DEPS_SOURCE_DIR` in offline mode and update comments | Yes |
| `docs/BUILDING.md` | UPDATE | Document shared sources in `build/_deps` and isolated preset build dirs in `build-<preset>/_deps`; update Sanitizer build instructions | Yes |
| `TODO.md` | UPDATE | Mark TODO-45 / #148 completed | No (gitignored file) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Update `.clang-format` to support JSON files without error

- **File**: `.clang-format`
- **Action**: UPDATE
- **Implement**: Append a document section for `Language: Json` with `DisableFormat: true` so `git-clang-format` does not fail when JSON files (such as `CMakePresets.json`) are modified.
- **Mirror**: `.clang-format:1-24`
- **Validate**: `scripts/format.sh --check`

### Task 2: Declare shared dependency source directory and pass to `FetchContent_Declare`

- **File**: `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Define `BLAZE_DEPS_SOURCE_DIR` defaulting to `${CMAKE_SOURCE_DIR}/build/_deps` and pass `SOURCE_DIR "${BLAZE_DEPS_SOURCE_DIR}/<dep>-src"` to each of the 5 `FetchContent_Declare` calls (`SDL3`, `glad`, `nlohmann_json`, `miniaudio`, `stb`).
- **Mirror**: `CMakeLists.txt:19-68`
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release`

### Task 3: Isolate preset dependency builds in `CMakePresets.json`

- **File**: `CMakePresets.json`
- **Action**: UPDATE
- **Implement**: Remove `FETCHCONTENT_BASE_DIR` from the `base` preset. Remove `FETCHCONTENT_BASE_DIR` and the five `FETCHCONTENT_SOURCE_DIR_<NAME>` cache variables from `asan-ubsan`, updating its description to reflect that it shares sources in `build/_deps` and builds in its own directory without requiring `build/` configured first.
- **Mirror**: `CMakePresets.json:9-16`, `CMakePresets.json:101-118`
- **Validate**: `cmake --preset linux-gcc-release && cmake --preset linux-gcc-debug`

### Task 4: Update `scripts/fresh-clone-check.sh` to pass `BLAZE_DEPS_SOURCE_DIR`

- **File**: `scripts/fresh-clone-check.sh`
- **Action**: UPDATE
- **Implement**: Update offline mode in `scripts/fresh-clone-check.sh` from `-DFETCHCONTENT_BASE_DIR=$repo_root/build/_deps` to `-DBLAZE_DEPS_SOURCE_DIR=$repo_root/build/_deps`. Update the header comments accordingly.
- **Mirror**: `scripts/fresh-clone-check.sh:4-8`, `scripts/fresh-clone-check.sh:105-110`
- **Validate**: `./scripts/fresh-clone-check.sh`

### Task 5: Update documentation in `docs/BUILDING.md`

- **File**: `docs/BUILDING.md`
- **Action**: UPDATE
- **Implement**: Update the preset dependency caching paragraph (around line 96) to state that presets share downloaded sources in `build/_deps` (`BLAZE_DEPS_SOURCE_DIR`) while keeping their dependency build directories separate under each preset's own build tree (`build-<preset>/_deps`). Update the Sanitizer build section to remove the prerequisite to configure `build/` first and note that it tracks `GIT_TAG` pins like every other preset.
- **Mirror**: `docs/BUILDING.md:95-101`, `docs/BUILDING.md:149-175`
- **Validate**: `scripts/lint.sh`

### Task 6: Check off TODO-45 in `TODO.md`

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Change `- [ ] Give each build preset its own dependency build directory...` to `- [x]` on line 61.
- **Mirror**: `TODO.md:60-61`
- **Validate**: `grep -n "TODO-45" TODO.md`

---

## Validation

```bash
# Lint checks
scripts/lint.sh

# Formatter checks (including on changed CMakePresets.json)
scripts/format.sh --check

# Standard check: build with -DBLAZE_WERROR=ON and run full test suite
scripts/check.sh

# Presets verification: configure and build Release and Debug sequentially
cmake --preset linux-gcc-release && cmake --build --preset linux-gcc-release -j"$(nproc)"
cmake --preset linux-gcc-debug && cmake --build --preset linux-gcc-debug -j"$(nproc)"

# Verify no dependency is rebuilt when re-invoking Release after Debug
cmake --build --preset linux-gcc-release

# Sanitizer preset verification
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan
ctest --preset asan-ubsan -R sanitizer_canary --no-tests=error

# Fresh-clone verification (offline mode)
scripts/fresh-clone-check.sh
```

## End-to-End Verification

1. Run `scripts/check.sh` to ensure `build/` is up to date and tests pass.
2. Record the modification timestamp of `build/_deps/sdl3-build/libSDL3.a`.
3. Configure and build `linux-gcc-release`:
   ```bash
   cmake --preset linux-gcc-release
   cmake --build --preset linux-gcc-release -j"$(nproc)"
   ctest --preset linux-gcc-release
   ```
4. Verify that `build/_deps/sdl3-build/libSDL3.a` was not modified and no files under `build/` changed.
5. Configure and build `linux-gcc-debug`:
   ```bash
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug -j"$(nproc)"
   ctest --preset linux-gcc-debug
   ```
6. Verify that re-running `cmake --build --preset linux-gcc-release` builds instantly (target `SDL3-static` is reported `[ 65%] Built target SDL3-static` with zero rebuild of SDL C files).
7. Configure, build, and test `asan-ubsan`:
   ```bash
   cmake --preset asan-ubsan
   cmake --build --preset asan-ubsan -j"$(nproc)"
   ctest --preset asan-ubsan
   ```
   Verify 100% tests pass (55 of 55 tests, including canary tests).
8. Run `scripts/fresh-clone-check.sh` and verify 100% tests pass in offline mode.
9. Run `scripts/check.sh` to verify it passes without rebuilding any dependencies.

## Risks

| Risk | Mitigation | In-Scope / Out-of-Scope |
|---|---|---|
| Clang, MSVC, or AppleClang presets fail with separate build dirs | All preset definitions inherit `base` identically; `binaryDir: ${sourceDir}/build-${presetName}` is already per-preset across all platforms. `FETCHCONTENT_BASE_DIR` simply defaults to `${CMAKE_BINARY_DIR}/_deps`. Verified that generator and architecture settings in Windows MSVC and macOS Clang are unaffected. | In-scope |
| CI cache key mismatch | `.github/workflows/ci.yml` caches `build/_deps` keyed by `CMakeLists.txt` hash. Since `BLAZE_DEPS_SOURCE_DIR` defaults to `${CMAKE_SOURCE_DIR}/build/_deps`, sources are restored to `build/_deps` in CI exactly as before. | In-scope |
| Initial download on brand-new checkout | `FetchContent` downloads to `SOURCE_DIR` if the directory does not exist. Verified with scratch probe that `FetchContent` clones the repo into `SOURCE_DIR` on fresh configure. | In-scope |
| JSON formatting error in `scripts/format.sh` | Addressed in Task 1 by configuring `.clang-format` to disable formatting for `Language: Json`. | In-scope |

## Open Questions

None. All technical choices and directory layouts were verified with concrete scratch probes and prototype builds.

## Acceptance Criteria

- [ ] Configuring any preset (for example `cmake --preset linux-gcc-release`) changes no file under `build/`, and a `scripts/check.sh` run afterwards rebuilds no dependency
- [ ] A Debug preset and a Release preset can be configured and built one after the other without rebuilding each other's dependency libraries
- [ ] The dependency sources are still downloaded only once and shared; a preset that downloads them on a new machine today still does
- [ ] `scripts/fresh-clone-check.sh` still passes in its offline mode
- [ ] `docs/BUILDING.md` says where each preset keeps its dependency sources and its dependency builds
- [ ] All tasks completed
- [ ] `scripts/lint.sh` and `scripts/format.sh --check` pass
- [ ] All tests pass across normal build, presets, and `fresh-clone-check.sh`
