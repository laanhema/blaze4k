# Code Review: feature/017-json-persistence-config-and-high-scores

**Scope**: Branch `feature/017-json-persistence-config-and-high-scores` vs `main`, including uncommitted changes and untracked files (`src/data/{config,config_loader,data_paths,high_scores}.*`, `tests/config_persistence_test.cpp`). Issue #17 (C2: JSON persistence, config + high scores).
**Recommendation**: NEEDS WORK (two small correctness fixes; rest is solid)

## Summary

The change adds a pure config model, a tolerant size-capped JSON config loader/saver, atomic high-score persistence, deterministic data-path resolution, a stable content-based chart key, and a focused test suite. The design follows the codebase's existing `judgment_constants_loader` conventions, everything builds warning-free under `-Wall -Wextra -Wpedantic`, and all 17 tests pass. Two robustness gaps remain around untrusted numeric input in the score loader and invalid string enums in the config loader.

## Issues Found

### Critical
None.

### High Priority

1. **`src/data/high_scores.cpp:74-76`** — `read_integer` accepts any finite, integral-looking JSON number and then performs `static_cast<std::int64_t>(value)` with no range check; converting an out-of-range double to a signed integer is undefined behaviour (C++ [conv.fpint]/1) and silently corrupts persisted score data. Confirmed against the built binary: `{"dp":1e300,"timestamp":1e300}` loads and is re-saved as `timestamp: -9223372036854775808`, `dp: 0`. Untrusted/hand-edited `scores.json` must not be able to do this. Add a range guard (compare against `numeric_limits<std::int64_t>::lowest()/max()`, as `judgment_constants_loader.cpp` already does).
2. **`src/data/high_scores.cpp:199`** — `dance_points = static_cast<int>(dance_points)` narrows an unchecked `int64_t` to `int` (implementation-defined wrap for values outside `int` range). Bound-check before narrowing; related to finding 1.

### Medium Priority

3. **`src/data/config_loader.cpp:98-109` + `271-324`** — Invalid string fields are not defaulted, contradicting the documented contract. `read_string_field` stores whatever string is present; `validate_game_config`'s failure is only appended as a warning (`:310-313`), so invalid `gameplay.scroll` (e.g. `"sideways"`) and empty `gameplay.speed_mod` are returned and re-persisted on exit. Verified live: a config with `"scroll":"sideways","speed_mod":""` round-trips both bad values back to disk. The header (`config_loader.hpp:20-22`, `config.hpp:70-74`) promises "a config loaded from disk is always valid" / "that field keeps its default". Coerce/reject invalid enum strings during read, or have `load_config` substitute defaults when validation fails.
4. **`src/main.cpp:143-145`** — `video.fullscreen` is part of the persisted model, validated and saved, but is never applied: only `width`, `height`, `vsync` are copied into `AppConfig`, and `WindowConfig` has no fullscreen field at all. The setting is currently a silent no-op. Either wire it through `WindowConfig`/window creation or mark it explicitly as reserved/not-yet-applied.

### Suggestions (Low)

5. **`src/main.cpp:124-125`** — `BLAZE4K_XDG=1` is honored but absent from `print_help()` (only the `--xdg` flag is documented). Document it, or drop the env var.
6. **`src/data/data_paths.cpp:33-41`** — `SDL_GetBasePath()` returns an `SDL_malloc`'d string that must be released with `SDL_free()`; the pointer is discarded, a one-time leak per call.
7. **`src/data/high_scores.cpp:84-86`** — Chart-key note tokens use `std::to_string(double)`, which is locale-sensitive for the decimal separator (and rounds to 6 decimals). Since keys are persisted, running under a non-"C" C locale (`setlocale`) could change keys and orphan existing scores. Prefer a locale-independent formatting helper.
8. **`src/main.cpp:106-111`** — `--data-dir` with a missing argument prints a warning but continues with the default directory instead of failing; matches some sibling flags, but silently ignoring the user's intent is surprising.
9. **`src/data/high_scores.cpp:241-262` / `config_loader.cpp:372-392`** — Fixed `<path>.tmp` temp names can collide between two concurrent instances writing the same data dir; and `std::filesystem::rename`-over-existing-file semantics should be confirmed on Windows (the primary save path replaces an existing `config.json` every run).
10. **`src/data/high_scores.hpp:10`** — Including `config_loader.hpp` solely for `ConfigLoadStatus` couples the score module to config; a small shared `data/load_status.hpp` would decouple them.
11. **`src/data/high_scores.cpp:228`** — The scores file is written with `version = kConfigVersion` (config's constant); a distinct score-schema version would be clearer. `version` is written but never read/validated on load (also true for config).

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS (no warnings) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (17/17; `config_persistence_test` included) |
| Lint | N/A (no separate linter configured) |

Notes: no tests were skipped or env-guarded. Manual hostile-input probes against the built binary confirmed findings 1/2 and 3; the config parser rejected an out-of-range numeric literal (`1e400`) by discarding the whole document. Deeply nested JSON (500k levels, <1 MiB) parsed without stack overflow under nlohmann 3.11.3 (iterative parser), so that is not a concern here.

## What's Good

- Consistent with existing loader conventions (`ConfigLoadStatus`, `[Tag]` messages, `error_code` filesystem calls, `-Werror`-clean).
- Sensible untrusted-input posture: 1 MiB caps, exception-free `json::parse`, per-field tolerance, numeric clamping, and graceful fallback with status.
- Atomic save is implemented correctly on POSIX (temp in same dir, rename, temp cleanup on failure) and covered by a real failure-path test (rename onto an existing directory).
- Chart-key stability (folder moves, case-insensitivity, note edits) and strict-best score semantics are well tested.
- `resolve_data_paths` is pure/deterministic and precedence is explicit, with tests for every branch.
- `ScreenContext` wiring keeps config/scores owned by `main` and passed through the existing seam rather than a new global.

## Recommendation

Fix findings 1-2 (bound the score integers before casting) and either fix or explicitly document findings 3-4. The Low items are safe to defer. After that, this is ready to merge.
