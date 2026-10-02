# Implementation Report

**Plan**: `.agents/plans/completed/030-fix-mines-exploding-too-easily-plan.md`
**Branch**: `feature/030-fix-mines-exploding-too-easily`
**Status**: COMPLETE

## Summary

Fixed #56: mines exploding too easily. `JudgmentEngine::cross_mines` used to re-check every past, unexpired mine in a held column on every update, for up to 180 ms after the mine's time. As a result, stepping on the next same-column arrow exploded a mine the player had already passed. The crossing is now an OpenITG-style cursor (`Player.cpp:632-646`): it checks each mine once, when it crosses, and runs `pad_stick` (OpenITG `PadStickSeconds` = 0.05 s, arcade `metrics.ini:103`) behind the music. A held panel counts only once it has been down for `pad_stick` (`CrossedMineRow`, `Player.cpp:1461-1488`). The explosion goes through the shared step routine (`Player::Step`), which is now extracted from `handle_step`. `pad_stick` is a data-driven constant (struct default, compiled default, validation, JSON loader and seed JSON).

Two items are kept for OpenITG parity, as resolved by the owner:
- **Open Question 1:** OpenITG's `CrossedMineRow` has no graded-mine guard. Test 16.11 pins this quirk.
- **Open Question 2:** `pad_stick = 0.05` is adopted.

The input stuck-held audit found one gap, a key held across a remap. `InputManager::apply_bindings` now clears the held action state. The root cause was posted on #56.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `pad_stick` data-driven constant (default, validation, loader, JSON) | `src/timing/judgment_constants.{hpp,cpp}`, `src/data/judgment_constants_loader.cpp`, `assets/data/judgment_constants.json` | ✅ |
| 2 | Extract shared `step()` and record per-column press time; fix stale OpenITG cites | `src/gameplay/judgment_engine.{hpp,cpp}` | ✅ |
| 3 | Crossing cursor with pad-stick (`cross_mines` rewrite) | `src/gameplay/judgment_engine.cpp` | ✅ |
| 4 | Engine tests: section 10 hardened, new section 16 (16.1–16.11) | `tests/judgment_engine_test.cpp` | ✅ |
| 5 | `apply_bindings` clears held action state, plus test 7b | `src/input/input_manager.cpp`, `tests/input_test.cpp` | ✅ |
| 6 | Constants tests (parity, `same_constants`, override, validation) | `tests/judgment_constants_test.cpp` | ✅ |
| 7 | Root cause posted on #56 | GitHub issue comment | ✅ |
| 8 | Full suite | – | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`; touched sources force-rebuilt) | ✅ no warnings or errors |
| Lint (no linter configured; the compiler warnings are the lint gate) | ✅ |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | ✅ 38/38 passed |
| Targeted (`judgment_engine_test`, `judgment_constants_test`, `input_test`, `metronome_sync_test`, `score_keeper_test`, `life_keeper_test`) | ✅ 6/6 passed |
| Regression proof (old `judgment_engine.cpp` with the new tests) | ✅ fails as expected, at `judgment_engine_test.cpp:366` (crossing hit time is not `now − pad_stick`); source restored |
| E2E smoke (`./build/blaze-4k --headless --smoke-test 10`, dummy drivers) | ✅ exit 0, and `assets/data/judgment_constants.json` (with `pad_stick`) loaded |
| Scope guard (`git diff --name-only \| grep -E "render\|note_field\|screens/"`) | ✅ no output; `.agents/stories/todo-stories.md` is still untracked and untouched |
| Owner play-test (interactive) | Not run by the agent, per the plan |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `assets/data/judgment_constants.json` | UPDATE | +2/-1 |
| `src/data/judgment_constants_loader.cpp` | UPDATE | +1/-0 |
| `src/gameplay/judgment_engine.cpp` | UPDATE | +49/-28 |
| `src/gameplay/judgment_engine.hpp` | UPDATE | +12/-4 |
| `src/input/input_manager.cpp` | UPDATE | +3/-0 |
| `src/timing/judgment_constants.cpp` | UPDATE | +7/-0 |
| `src/timing/judgment_constants.hpp` | UPDATE | +6/-0 |
| `tests/input_test.cpp` | UPDATE | +9/-0 |
| `tests/judgment_constants_test.cpp` | UPDATE | +30/-0 |
| `tests/judgment_engine_test.cpp` | UPDATE | +205/-1 |

## Deviations from Plan

1. **The `cross_mines` inner loop uses `continue` instead of `break` for mines not yet crossed.** The plan's pseudocode said `break` because "mines are time-ordered". In fact the parser sorts notes by **beat** (`note_parser.cpp:255`), and warps (negative stops) are accepted with a warning (`timing_data.cpp:47,134`). Seconds can therefore be non-monotonic in index order, and `break` could skip a mine for good. `continue` costs the same as the old loop.
2. **Test 16.10 (frame-rate independence) shares a lambda with test 16.7.** The same hold-through-mine scenario runs at 0.01, 0.1 and 0.001 s update grids. The hit-time bound is `[2.0, 2.0 + grid]` with 1e-9 slack, and update times are computed as `1.0 + i * grid` (not accumulated) to avoid float drift.
3. **Test 16.2 needs no separate update at `2.0 + way_off + 1e-3`.** The plan's `update(2.2, ...)` already passes the expiry threshold (2.02), so the `AvoidedMine` check runs after a final later update. The intent is unchanged.
4. **Extra test coverage.** Test 16.9 also checks `hit_time == now − pad_stick`. `judgment_constants_test` 9b2 also checks `validate()` directly, including that the reason mentions `pad_stick`.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/judgment_engine_test.cpp` | Section 10 hardened (3.06, `hit_time = now − pad_stick`). Section 16, each with the OpenITG rule it pins:<br>16.1 stepping on a mine, in and out of the window<br>16.2 #56 regression: the mine is crossed unheld, then the next same-column tap is stepped<br>16.3 a tap before a mine<br>16.4 the closer mine wins the step<br>16.5 pad-stick on a late press<br>16.6 pad-stick on an early release<br>16.7/16.10 hold through a mine at three update grids<br>16.8 a mine is never re-checked after crossing<br>16.9 a frame hitch past or within the window<br>16.11 the graded-mine quirk (Decent tap) |
| `tests/judgment_constants_test.cpp` | `pad_stick` default parity, `same_constants` field, JSON override to 0 (valid), negative value rejected (loader fallback plus `validate` reason) |
| `tests/input_test.cpp` | 7b: `apply_bindings` releases a held action |

## Follow-ups (from the plan's Risks; out of scope)

- `last_press_time_` and `action_states_` are tracked per column/action, not per physical key. Two keys mapped to one column can under-report held state.
- `judge_window_scale/add` are not applied to any window (#57).
