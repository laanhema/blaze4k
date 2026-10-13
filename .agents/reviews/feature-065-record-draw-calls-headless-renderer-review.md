# Code Review: feature/065-record-draw-calls-headless-renderer

**Scope**: branch `feature/065-record-draw-calls-headless-renderer` (against merge-base with `main`, including uncommitted changes)
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed headless draw-call recording in `GlQuadRenderer` implementing GitHub issue #141. The implementation cleanly captures geometry, color, texture ID, and blend mode when uninitialized, equips `select_art_test` with comprehensive verification of the code-drawn Edit difficulty row (all 6 component quads), updates the draw-decision testing rule in `AGENTS.md`, and maintains zero performance or allocation cost in initialized OpenGL gameplay.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
- **L1** `src/render/gl_quad_renderer.cpp:167` — In `GlQuadRenderer::shutdown()`, `recorded_quads_` is not cleared alongside `vertices_`. **Why:** While headless tests typically construct local renderer instances, any renderer reused headlessly after `shutdown()` retains recorded quads until `begin()` or `clear_recorded()` is called. **Fix:** Add `recorded_quads_.clear();` inside `GlQuadRenderer::shutdown()`.
- **L2** `tests/gl_quad_renderer_test.cpp:193` — `test_headless_recording()` tests `draw_quad_points` and `draw_quad`, but does not exercise `draw_textured_quad`. **Why:** The uninitialized recording branch in `draw_textured_quad` (including center-origin rotation calculations) remains unexercised in the renderer's own unit test. **Fix:** Add a call to `renderer.draw_textured_quad(...)` with a non-zero rotation angle and assert its recorded quad in `test_headless_recording()`.

**Noted, not a finding**: Non-solid draws (sprites, fonts) remain out of scope for headless recording per issue #141, as textures are invalid without GL and text rendering checks `gl_present()`. Modifying the remaining 21 `is_initialized()` call sites across 8 other screen files is deliberately deferred until those screens receive draw-site tests.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (`cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLAZE_WERROR=ON && cmake --build build -j16`); host Linux |
| Warnings | NONE | Recompiled changed sources (`gl_quad_renderer.cpp`, `select_art.cpp`, `gl_quad_renderer_test.cpp`, `select_art_test.cpp`) to `/dev/null` with `-Wall -Wextra -Wpedantic -Werror`; zero warnings |
| Lint | PASS | `scripts/lint.sh` via `scripts/check.sh`; zero violations |
| Tests | PASS | 53 of 53 tests passed under `ctest --test-dir build --output-on-failure`; 0 failed; 0 skipped; ASan+UBSan passed on `gl_quad_renderer_test` and `select_art_test` |
| UI verification | PASS | Verified `/tmp/blaze4k-verify/065-record-draw-calls/evidence/02-blaze-anthem.png` (captured on host X11 via `/verify song-select` at 1280x720); Song Select and difficulty rows match reference appearance |

## What's Good

- Headless recording is zero-cost in the real game: `init()` sets `initialized_ = true`, which completely bypasses `recorded_quads_` with zero overhead or memory allocations.
- Centralizing `rect_corners` in `src/render/geometry.hpp` eliminates duplication across `gl_quad_renderer.cpp` and unit tests.
- `test_edit_row_recorded_quads` thoroughly checks all 6 quads of the Edit row across both unselected and selected states (body, 4 ring strips, tab), asserting exact corner coordinates, colors, texture ID 0, blend mode Alpha, and verifying negative sensitivity against incorrect strip rectangles or colors.
- All 5 acceptance criteria of issue #141 are satisfied.

## Recommendation

The branch is ready to merge. The two Low suggestions (clearing `recorded_quads_` in `shutdown()` and adding a `draw_textured_quad` call to `test_headless_recording`) can be addressed in `/fix-findings` or as minor polish.
