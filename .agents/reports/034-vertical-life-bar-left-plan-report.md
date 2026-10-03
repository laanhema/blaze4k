# Implementation Report

**Plan**: `.agents/plans/completed/034-vertical-life-bar-left-plan.md`
**Branch**: `feature/034-vertical-life-bar-left`
**Status**: COMPLETE

## Summary

The life bar now sits vertically at the left screen edge and fills from the bottom (empty) to the
top (full). Its geometry lives in a new pure, GL-free helper,
`layout_life_bar(life, screen_w, screen_h, field_left)`. The helper returns the frame, back and
fill rects plus a danger flag, and `HudRenderer::render_life` only draws those quads. The bar uses:
- a 24 px left margin and 16 px thickness, with a 2 px frame
- 60% of the screen height, centred vertically (inset `max(0.2h, 40)`)
- a clamp that keeps the frame at least 16 px left of the note field, sliding left and then
  shrinking to 6 px in very narrow windows (the bar is never hidden)

The new `NoteField::field_left(screen_w)` gives the HUD and `NoteFieldRenderer` one shared formula
for the field's left edge. The palette, the 2 px frame, the draw order and the strict `< 0.3`
danger tint are unchanged. The grade, percent and combo text is not touched.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `LifeBarLayout` + `layout_life_bar` (pure) + geometry constants | `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp` | ✅ |
| 2 | `render_life` draws from the layout (skips a zero-height fill) | `src/gameplay/hud_renderer.cpp` | ✅ |
| 3 | `NoteField::field_left` + callers | `src/gameplay/note_field.hpp`, `src/gameplay/note_field_renderer.cpp`, `src/gameplay/gameplay_view.cpp` | ✅ |
| 4 | New `hud_renderer_test` target (11 cases) | `tests/hud_renderer_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 5 | `note_field_test` case for `field_left` | `tests/note_field_test.cpp` | ✅ |
| 6 | Tick the TODO entry for #76 | `TODO.md` (gitignored, local only) | ✅ |
| 7 | Full validation + headless smoke | - | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | ✅ No compiler warnings or errors. The only warnings are pre-existing CMake configure warnings in third-party deps (SDL3 ALSA detection, glad `cmake_minimum_required`) |
| Lint | No linter configured. Compiler warnings are the gate: ✅ none |
| Tests (`bwrap ... ctest --test-dir build --output-on-failure`) | ✅ 100% tests passed out of 39 (baseline 38 + new `hud_renderer_test`) |
| Headless smoke (`bwrap ... ./build/blaze-4k --headless --smoke-test 10`) | ✅ Exit 0: "Smoke test finished (10 frames). Exiting cleanly." |
| Debug build sanity (optional) | Not run. The plan marks it optional, and a fresh `build-debug` would re-fetch every FetchContent dependency |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/hud_renderer.hpp` | UPDATE | +21/-4 |
| `src/gameplay/hud_renderer.cpp` | UPDATE | +51/-19 |
| `src/gameplay/note_field.hpp` | UPDATE | +5/-0 |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | +1/-1 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +1/-1 |
| `tests/hud_renderer_test.cpp` | CREATE | +213 |
| `tests/CMakeLists.txt` | UPDATE | +11/-0 |
| `tests/note_field_test.cpp` | UPDATE | +15/-0 |
| `TODO.md` | UPDATE (gitignored) | +1/-1 |

## Deviations from Plan

- **TODO.md is gitignored** (`.gitignore:27`). The checkbox for #76 is ticked locally, but the
  change does not appear in `git status` or `git diff`, and it will not be part of a commit.
- **Validation commands:** ctest and the smoke test ran inside the caller's `bwrap` sandbox (no
  real audio devices, no network) instead of the plan's `SDL_*_DRIVER=dummy` environment. Results
  are equivalent: 39/39.
- **Optional Debug build:** not run (see above).
- **Extra test coverage:** the narrow-window case also asserts `back.x == 2` at 400 px, and the
  danger case also asserts that a negative life is danger. Neither changes behaviour. Otherwise the
  implementation matches the plan and its resolved open questions: 60% height, slide/shrink rather
  than hide, 24 px margin, 16 px gap.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/hud_renderer_test.cpp` | Vertical, left and full at life 1.0; bottom-up fill at 0.5 and 0.25; clamp at -0.5 and 1.7; strict `< 0.3` danger; frame = back grown by 2 px; no field overlap at 1280x720 / 1920x1080 / 640x480 / 1024x768 / 800x600 (16 px gap, no shrink); below the percent text (including 320x240); grade text not overlapped at 1280x720 and 640x480; narrow clamp at 500x400 and 400x400; degenerate sizes are invisible; height scales with the screen (432 / 648 px) and stays centred |
| `tests/note_field_test.cpp` | Case 11: `field_left(1280) == 424`, `field_left(320) == -56`, and the field is centred at several widths |
