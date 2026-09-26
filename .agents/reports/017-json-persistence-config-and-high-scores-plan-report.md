# Implementation Report

**Plan**: `.agents/plans/017-json-persistence-config-and-high-scores-plan.md`
**Branch**: `feature/017-json-persistence-config-and-high-scores`
**Status**: COMPLETE

## Summary

Added the offline JSON persistence layer (C2). `config.json` (video/audio/offset/
gameplay/input) and per-chart best scores in `scores.json` are written with an
atomic temp-file + rename and read through a non-throwing, defaulting, 1 MiB-capped
reader modelled on `judgment_constants_loader`. Data lives in a portable `data/`
folder next to the binary by default, with opt-in XDG placement (`--xdg`,
`TUNDRA_XDG=1`) and an explicit `--data-dir` override that wins. Stable content-based
chart keys (filename + metadata + note fingerprint) let scores survive pack moves.
`ScreenContext` gained additive `config`/`scores` pointers and `ScreenManager` a
`context()` accessor; `main.cpp` owns the boot/exit lifecycle (load config before
window creation, apply video, attach state, save both on clean exit). No C3/C4/C5/C7
screens, no gameplay/timing/input/render changes.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure config model | `src/data/config.hpp` | ✅ |
| 2 | Config loader/saver | `src/data/config_loader.{hpp,cpp}` | ✅ |
| 3 | High scores + stable chart key | `src/data/high_scores.{hpp,cpp}` | ✅ |
| 4 | Data path resolution | `src/data/data_paths.{hpp,cpp}` | ✅ |
| 5 | Extend `ScreenContext` + expose it | `src/screens/screen.hpp`, `src/screens/screen_manager.hpp` | ✅ |
| 6 | `main.cpp` lifecycle wiring + CLI | `src/main.cpp` | ✅ |
| 7 | Register sources and test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 8 | Persistence test suite | `tests/config_persistence_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ |
| Warnings (`-Wall -Wextra -Wpedantic`) | ✅ none new |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 17/17 |
| Explicit `./build/tests/config_persistence_test` | ✅ |
| Purity: `rg "SDL_\|glad\|miniaudio" src/data` | ✅ only `src/data/data_paths.cpp` |
| Purity: `rg "nlohmann\|json" src/data/config.hpp` | ✅ no matches |

## End-to-End Verification

| Step | Result |
|------|--------|
| 1. Fresh boot writes valid `config.json` + `scores.json` (exit 0) | ✅ |
| 2. Config reload honored (`[Config] Loaded ...`) | ✅ |
| 3. Corrupt JSON → warning, exit 0, config repaired | ✅ |
| 4. High-score best logic + chart-key stability (test 8–10) | ✅ |
| 5. XDG path used; `--data-dir` wins over `--xdg`; `TUNDRA_XDG=1` honored | ✅ |
| 6. Regression 17/17 + `--gameplay-demo` still runs (exit 0) | ✅ |
| 7. `git status` limited to planned files; no `timing/input/gameplay/render` changes | ✅ |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/data/config.hpp` | CREATE | +77 |
| `src/data/config_loader.hpp` | CREATE | +37 |
| `src/data/config_loader.cpp` | CREATE | +404 |
| `src/data/high_scores.hpp` | CREATE | +56 |
| `src/data/high_scores.cpp` | CREATE | +295 |
| `src/data/data_paths.hpp` | CREATE | +32 |
| `src/data/data_paths.cpp` | CREATE | +44 |
| `tests/config_persistence_test.cpp` | CREATE | +337 |
| `src/main.cpp` | UPDATE | +68 |
| `src/screens/screen.hpp` | UPDATE | +8/-1 |
| `src/screens/screen_manager.hpp` | UPDATE | +3 |
| `CMakeLists.txt` | UPDATE | +3 |
| `tests/CMakeLists.txt` | UPDATE | +10 |

## Deviations from Plan

1. **`validate_game_config` is a pure predicate; clamping happens in the loader.**
   The plan's declared signature is `validate_game_config(const GameConfig&, ...)`,
   which cannot clamp in place. `validate_game_config` therefore range-checks
   (returns false + reason), while the loader's `read_int_field`/`read_double_field`
   clamp numeric fields to the same documented ranges as they read. This preserves
   the declared API and satisfies "rejected or clamped".
2. **`read_bindings` fully replaces the defaults when a bindings map is present**
   (rather than merging). This makes save→load an exact round-trip for any config,
   which is what the plan's round-trip acceptance requires; a hand-edited partial
   map simply leaves unspecified actions unbound. Bindings remain opaque and are not
   applied.
3. **`default_executable_dir()` returns the executable's directory directly.**
   `SDL_GetBasePath()` already yields the containing directory (with a trailing
   separator), so the plan's literal `parent_path()` wording would have pointed one
   level too high; the fallback also returns the current working directory (not
   `cwd/"data"`) so `resolve_data_paths` does not double-append `data`.
4. **`src/data/config.hpp` includes `<string>`, `<utility>`, `<vector>`** (plus the
   plan itself uses `std::vector`) rather than only `<array>`/`<string>`; it stays
   pure (no platform/JSON includes), verified by the purity grep.
5. **`--no-vsync` is overridden by `game_config.video.vsync`** per the plan's boot
   order (CLI `--headless` still wins because headless is never persisted). With no
   config file this matches today's defaults exactly.
6. **`load_high_scores` reports an empty-but-valid document as `LoadedFromFile`**
   (message "no scores recorded"), consistent with "missing/corrupt → warning".

## Tests Written

| Test File | Test Cases |
|-----------|-----------|
| `tests/config_persistence_test.cpp` | 1 defaults+validation; 2 full round-trip incl. bindings + nested dir; 3 missing-file fallback; 4 corrupt-JSON fallback; 5 per-field tolerance + clamping; 6 size cap; 7 atomic save + failure cleanup; 8 stable chart key (incl. moved folder, case-insensitive title); 9 best-score replacement semantics; 10 high-score round-trip + corrupt/missing fallback; 11 data path resolution (portable/XDG/explicit) |
