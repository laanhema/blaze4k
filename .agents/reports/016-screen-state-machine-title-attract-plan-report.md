# Implementation Report

**Plan**: `.agents/plans/016-screen-state-machine-title-attract-plan.md`
**Branch**: `feature/016-screen-state-machine-title-attract`
**Status**: COMPLETE (uncommitted, per git policy)

## Summary

Built the arcade shell foundation for issue #16 (C1): a reusable 5x7 `bitmap_font`,
the `Screen` interface + `ScreenManager` state machine (deferred transitions,
exit→enter ordering, idle-attract policy), fully-implemented `TitleScreen` and
`AttractScreen` (title loop), and a minimal `SelectPlaceholderScreen`. The App
gained an optional event callback so Escape quits only when the active screen does
not consume it (A2 preserved at Title). `main.cpp` now builds/registers the shell
by default with a `--attract-timeout` flag, keeping `--gameplay-demo` as an
override. `hud_renderer.cpp` now uses the shared font.

## Tasks Completed

| # | Task | File(s) | Status |
|---|------|---------|--------|
| 1 | Shared bitmap font | `src/render/bitmap_font.{hpp,cpp}`, `src/gameplay/hud_renderer.cpp` | ✅ |
| 2 | Screen interface/context | `src/screens/screen.hpp` | ✅ |
| 3 | ScreenManager state machine | `src/screens/screen_manager.{hpp,cpp}` | ✅ |
| 4 | Title screen | `src/screens/title_screen.{hpp,cpp}` | ✅ |
| 5 | Attract screen (title loop) | `src/screens/attract_screen.{hpp,cpp}` | ✅ |
| 6 | Placeholder Song Select | `src/screens/select_placeholder_screen.{hpp,cpp}` | ✅ |
| 7 | App event-callback hook | `src/app/app.{hpp,cpp}` | ✅ |
| 8 | main.cpp wiring + CLI | `src/main.cpp` | ✅ |
| 9 | Register sources + test | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 10 | Headless test suite | `tests/screen_manager_test.cpp` | ✅ |

## Validation Results

| Check | Result | Evidence |
|-------|--------|----------|
| Build (`cmake --build build -j16`) | ✅ | no warnings/errors; `-Wall -Wextra -Wpedantic` clean |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ | **16/16 passed** (baseline 15 + `screen_manager_test`) |
| `./build/tests/screen_manager_test` | ✅ | exit 0 |
| Headless boot smoke (`--headless --smoke-test 120`) | ✅ | logs `[ScreenManager] enter Title`, clean shutdown |
| Headless attract observed | ✅ | `--attract-timeout 0.05 --smoke-test 300000` logs `Title -> Attract` |
| Timing purity (`rg chrono|GetTicks|... src/screens`) | ✅ | no wall-clock reads |
| Phase B `--gameplay-demo` harness | ✅ | 60-frame smoke exits 0 |
| Scope diff | ✅ | only expected files; no changes to `src/timing/`, `src/input/`, `life_keeper.*`, `gl_quad_renderer.*` |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/bitmap_font.hpp` | CREATE | +25 |
| `src/render/bitmap_font.cpp` | CREATE | +105 |
| `src/screens/screen.hpp` | CREATE | +39 |
| `src/screens/screen_manager.hpp` | CREATE | +50 |
| `src/screens/screen_manager.cpp` | CREATE | +192 |
| `src/screens/title_screen.hpp` | CREATE | +19 |
| `src/screens/title_screen.cpp` | CREATE | +72 |
| `src/screens/attract_screen.hpp` | CREATE | +21 |
| `src/screens/attract_screen.cpp` | CREATE | +72 |
| `src/screens/select_placeholder_screen.hpp` | CREATE | +18 |
| `src/screens/select_placeholder_screen.cpp` | CREATE | +50 |
| `tests/screen_manager_test.cpp` | CREATE | +362 |
| `src/gameplay/hud_renderer.cpp` | UPDATE | font extracted, behavior-preserving |
| `src/app/app.hpp` | UPDATE | `EventCallback` + `set_event_callback` |
| `src/app/app.cpp` | UPDATE | Escape delegated; quit only if unconsumed |
| `src/main.cpp` | UPDATE | shell default path, `--attract-timeout`, help |
| `CMakeLists.txt` | UPDATE | registered 5 new sources |
| `tests/CMakeLists.txt` | UPDATE | registered `screen_manager_test` |

## Deviations from Plan

1. **Font glyph set extended with `[` and `]`.** The plan listed full A–Z, digits,
   space, and existing punctuation. The Select hint needs brackets; since the font
   is uppercase-only, the hint renders `[BACK] TO TITLE` (not the plan's
   `[Back] to title`). Rationale: avoids silently dropping lowercase letters.
2. **Idle-attract unit test uses `dt = 0.125` instead of `0.1`.** Ten `0.1` steps
   sum to `0.9999999999999999` in binary and never reach a `1.0` timeout. `0.125`
   is exactly representable so 8 steps sum to exactly `1.0`; the policy semantics
   under test are unchanged (7 steps → Title, 8th → Attract). This is a test-only
   robustness fix; the implementation is correct.
3. **E2E step 4 command adjusted.** The plan's exact invocation
   (`--smoke-test 600 --attract-timeout 1.0`) does not log a transition because the
   headless App advances its fixed-step accumulator from *real* wall time, and 600
   headless frames complete in microseconds (far less than 1 s). The idle policy is
   driven by `fixed_dt` and is proven deterministically by
   `test_idle_attract_policy`. The same E2E behavior was demonstrated with
   `--attract-timeout 0.05 --smoke-test 300000` (real time ≈ 0.42 s), which logs
   `[ScreenManager] Title -> Attract`. No implementation change; plan expectation
   about headless wall-time was inaccurate.
4. **Font centering "within 1 px" not pixel-verified.** `GlQuadRenderer::draw_quad`
   is non-virtual and records nothing when uninitialized, so centering is not
   observable through the headless seam. The test verifies monospace width
   arithmetic (`text_width`) and that `draw_text`/`draw_text_centered` are safe
   no-ops, per the skill's guidance not to add a test seam by refactoring beyond
   the plan.
5. **Source registration timing.** `src/render/bitmap_font.cpp` was added to
   `tundra_core` during Task 1 (rather than only at Task 9) so the build stayed
   green immediately after the HUD refactor. Remaining sources added at Task 9.
   No behavioral deviation.
6. **`screen.hpp`** includes `<optional>` exactly as the plan specified, though it
   is currently unused; kept verbatim to match the pinned interface.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/screen_manager_test.cpp` | boot lifecycle; transition ordering (exit before enter); deferred application/no self-destroy; unregistered target no-op; idle-attract + accumulator reset; idle only from Title/Select; attract returns to origin (Title/Select); back navigation + `back_navigates()`; headless render dispatch; real Title/Attract/Select screens; font sanity; App + shell headless smoke |

## E2E Verification Checklist

- [x] `./build/tests/screen_manager_test` exits 0 with all sub-checks passing
- [x] `ctest --test-dir build --output-on-failure` → 16/16
- [x] `./build/tundra-dance --headless --smoke-test 120` → `enter Title`, clean exit
- [x] Headless idle `Title -> Attract` observed (low timeout + adequate frames; see deviation 3)
- [x] Timing purity: no wall-clock reads under `src/screens`
- [x] Phase B `--gameplay-demo` smoke still passes
- [x] Scope diff: no edits to timing/input/life_keeper/gl_quad_renderer