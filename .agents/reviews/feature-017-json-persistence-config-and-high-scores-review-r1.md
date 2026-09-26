# Code Review (Re-review r1): feature/017-json-persistence-config-and-high-scores

**Scope**: Branch `feature/017-json-persistence-config-and-high-scores` vs `main` (single commit `0ce0f14`; `git status --porcelain` clean — no uncommitted or untracked files). Issue #17 (C2: JSON persistence, config + high scores). Re-review of the 11 findings in `.agents/reviews/feature-017-json-persistence-config-and-high-scores-review.md`.
**Recommendation**: APPROVE WITH NITS (one Medium residual on the finding-1 boundary; not merge-blocking for realistic inputs)

## Summary

All 11 prior findings were re-checked against the current tree. Findings 2, 3, 4, 5, 7, 9, 10, and 11 are fully fixed with tests/evidence; finding 6 (won't-fix) is confirmed correct against the vendored SDL3 source; finding 8 (Low) was deliberately left as-is and is consistent with sibling flags. Finding 1 is fixed for the demonstrated exploit but retains a narrow boundary hole: a double exactly equal to `2^63` still passes the new guard and is then cast to `std::int64_t` (UB/implementation-defined), and any valid `int64` value that rounds to `2^63` (e.g. `INT64_MAX`) is corrupted to `INT64_MIN` before clamping. Reproduced with a standalone probe against `libtundra_core.a` (see Validation).

## Fix-Verification Table

| # | Prior finding | Severity | Status | Evidence |
|---|---------------|----------|--------|----------|
| 1 | `read_integer` unchecked out-of-range double → UB on cast | High | **Partial** | Guard added at `high_scores.cpp:102-106` correctly rejects `1e300` (test 10b passes). Residual: `value > (double)INT64_MAX` is false when `value == 2^63` (since `(double)INT64_MAX` rounds to `2^63`), so `9223372036854775808` / `9223372036854775807` still reach `static_cast<std::int64_t>` at `:107`. Probe: both load as `dp=-2147483648` (INT64_MIN). See New Findings N1. |
| 2 | `dance_points` narrowed int64→int without bound check | High | **Fixed** | Clamp to `[int::lowest(), int::max()]` before the cast at `high_scores.cpp:231-237`; probe `dp=5000000000` → `INT_MAX`, covered by test 10b (`tests/config_persistence_test.cpp:337-363`). |
| 3 | Invalid string enums loaded and re-persisted | Medium | **Fixed** | Defaults captured and invalid `speed_mod` (empty) / `scroll` (∉{up,down}) coerced at `config_loader.cpp:217-231`. Test 5b (`tests/config_persistence_test.cpp:184-203`) asserts `"sideways"` is not re-saved. |
| 4 | `video.fullscreen` persisted but never applied | Medium | **Fixed** | `WindowConfig::fullscreen` added (`window.hpp:14`), applied as `SDL_WINDOW_FULLSCREEN` (`window.cpp:82-84`), copied from config (`main.cpp:147`). |
| 5 | `TUNDRA_XDG` honored but undocumented | Low | **Fixed** | Documented at `main.cpp:33-34`. |
| 6 | `SDL_GetBasePath()` result must be `SDL_free`d | Low | **Won't-fix — reasoning VERIFIED CORRECT** | SDL3 caches the string in a static `CachedBasePath` (`build/_deps/sdl3-src/src/filesystem/SDL_filesystem.c:475-482`) and frees it in `SDL_QuitFilesystem` (`:519-521`); the header does not document caller ownership (unlike SDL2). `data_paths.cpp:33-43` correctly discards it. |
| 7 | `std::to_string(double)` chart-key tokens are locale-sensitive | Low | **Fixed** | `format_double` uses locale-independent `std::to_chars` (`high_scores.cpp:56-62`, used at `:116`); `<charconv>` included at `:6`. |
| 8 | `--data-dir` missing arg warns and continues | Low | **Not fixed (deferred)** | `main.cpp:107-112` still warns and falls back — identical to `--gameplay-demo`/`--speed`; acceptable, Low. |
| 9 | Fixed `<path>.tmp` temp-name collisions / Windows rename semantics | Low | **Fixed** | `unique_suffix()` (pid ^ atomic counter) at `high_scores.cpp:46-54` / `config_loader.cpp:39-47`, used at `high_scores.cpp:279-280` and `config_loader.cpp:403-404`; rename semantics documented at `:299-301` / `:423-425`. Test 10c (`tests/config_persistence_test.cpp:365-389`). |
| 10 | `high_scores.hpp` coupled to config via `ConfigLoadStatus` | Low | **Fixed** | Own `ScoresLoadStatus` enum at `high_scores.hpp:16-18`; no config include. |
| 11 | Scores file used `kConfigVersion`; version unvalidated | Low | **Fixed** | Distinct `kScoresVersion` at `high_scores.hpp:13-14`, written at `high_scores.cpp:266`. (Still not read/validated on load — same as config; acceptable.) |

## New Findings

### Medium

**N1. Residual out-of-range cast in `read_integer` at the `2^63` boundary — `src/data/high_scores.cpp:102-107`**

The new guard compares against `static_cast<double>(std::numeric_limits<std::int64_t>::max())`, which rounds up to exactly `2^63`. A JSON number equal to `2^63` therefore fails to satisfy `value > max` and is passed to `static_cast<std::int64_t>(value)` — undefined behaviour (implementation-defined on MSVC/GCC, yielding `INT64_MIN`). The same path corrupts a *valid* `INT64_MAX` because readability goes through `it->get<double>()` (`:101`). Reproduced (standalone probe, `-O0` and default `-O3` alias-free build):

```
{"dp":9223372036854775808, ...} -> dp=-2147483648
{"dp":9223372036854775807, ...} -> dp=-2147483648   (valid int64, silently corrupted)
{"dp":9.2233720368547758e18, ...} -> dp=-2147483648
```

Impact is narrow (only doubles that round to exactly `2^63`; realistic dp/timestamps unaffected) and on common platforms the conversion saturates rather than traps, so this is not memory-unsafe. Suggested hardening: bounds-check with the open upper bound (`value >= 9223372036854775808.0` is out of range), or read integers natively (`is_number_integer`/`is_number_unsigned` + `get<std::int64_t>()`) instead of round-tripping through `double`. Adding the `2^63` case to test 10b would lock it in. The prior exploit (`1e300`) is genuinely fixed.

### Low / observations

- Invalid-but-nonempty speed mods (e.g. `"banana"`) still round-trip; only emptiness is rejected (`config_loader.cpp:224-227`, `:293`). This matches `validate_game_config`'s definition of valid, so not a defect — noted for awareness.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS (incremental; all 20 targets up to date) |
| Warning check (`-Wall -Wextra -Wpedantic -fsyntax-only` on the 3 new `data/` sources) | PASS (no diagnostics) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (17/17; `config_persistence_test` included) |
| Lint | N/A (no separate linter configured) |

Skipped/env-guarded tests: none — `ctest` reported 17/17 executed, 0 skipped. Prior findings 1 and 3 were re-probed against the built code with a throwaway `/tmp/opencode` program (not added to the repo).

## What's Good

- The fixes are surgical and preserve the existing loader conventions (`ConfigLoadStatus`, `[Tag]` messages, atomic temp+rename).
- Finding 3's fix is backed by a real re-persistence assertion, not just a load assertion.
- Finding 9's unique temp suffix plus the tested failure-path cleanup is a solid concurrency hardening over the initial fixed `.tmp`.
- Finding 6 was correctly left alone; the SDL3 `CachedBasePath` ownership claim in the comment and in the prior report checks out against vendored source.

## Recommendation

Merge is acceptable. Follow up with a one-line hardening of the `read_integer` upper bound (N1) and a boundary test case; findings 8 and the speed-mod observation need no action.
