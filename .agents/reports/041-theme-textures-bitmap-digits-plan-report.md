# Implementation Report

**Plan**: `.agents/plans/completed/041-theme-textures-bitmap-digits-plan.md`
**Branch**: `feature/041-theme-textures-bitmap-digits`
**Status**: COMPLETE

## Summary

Added `src/render/theme_textures.{hpp,cpp}` (#89), which has three layers:

- **Manifest parser.** A pure, GL-free parser for the Cabinet `manifest.json`. It never throws. It caps file size and entry counts, accepts only plain `*.png` file names, bounds-checks every rect against `size_px`, and skips each bad entry with one warning.
- **Placement math.** Pure functions that map a screen-space content box to destination rects and UVs, for these kinds: sprite, stretch, `stretch_x`, slice3, slice9, frame plus its fallback ring, tiling (TopLeft and Bottom anchors, and the screen-pixel `scanlines`), the cropped life fill, and digit layout. A pure function also picks the load options for each texture.
- **`ThemeTextures` and `BitmapDigits`.** These own the textures:
  - Every PNG is loaded once in `App::init()`.
  - Mipmaps are on for `logo`, `grade_*` and `judgment_*`. `tile` kinds use Repeat wrap. `scanlines` uses Nearest filtering.
  - Each missing or mis-sized PNG is logged once at load, and its draws fall back to a flat quad over the content box.
  - Headless runs log one line and upload nothing.
  - An unknown name is logged once, at its first draw.

`App` owns the instance and calls `shutdown()` on it before `window_.shutdown()`, so the textures are released while the GL context is still alive. `main.cpp` wires it into the new `ScreenContext::theme` pointer. No screen uses the pointer yet; #92 to #95 will.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Types, pure API, `BitmapDigits`, `ThemeTextures` | `src/render/theme_textures.hpp` | ✅ |
| 2 | Parser, pure math, loader, draw helpers, fallbacks | `src/render/theme_textures.cpp` | ✅ |
| 3 | Build registration in `blaze4k_core` | `CMakeLists.txt` | ✅ |
| 4 | App ownership, load in `init()`, shutdown before the GL context goes | `src/app/app.hpp`, `src/app/app.cpp` | ✅ |
| 5 | `ScreenContext::theme` seam and `main.cpp` wiring | `src/screens/screen.hpp`, `src/main.cpp` | ✅ |
| 6 | Unit tests and registration | `tests/theme_textures_test.cpp`, `tests/CMakeLists.txt` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j8`; touched TUs rebuilt, then grepped for warning/error) | ✅ exit 0, no compiler warnings |
| Lint (the `-Wall -Wextra -Wpedantic` build is the lint gate) | ✅ |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 100% tests passed out of 44 |
| `theme_textures_test` | ✅ all 12 cases pass |
| E2E headless smoke run from the repo root (`--headless --smoke-test 5`) | ✅ exactly one `[ThemeTextures] No GL context available; drawing flat fallbacks for 63 theme textures` line, then "Blaze 4k shut down cleanly.", exit 0 |
| E2E headless smoke run with cwd `/tmp` | ✅ same single line (63 textures, so the manifest was found next to the executable), clean exit 0 |
| Lifetime | ✅ `app_test` passes; `theme_textures_.shutdown()` comes before `window_.shutdown()` in `~App()` |
| `TODO.md` / `.agents/stories/todo-stories.md` | ✅ `TODO.md` has no diff. `todo-stories.md` still carries only the owner's earlier uncommitted edits; this work did not touch it |

The configure step prints its usual warnings (SDL cannot find ALSA, and a deprecation warning from glad's CMake). The smoke runs print ALSA messages because the sandbox hides `/dev/snd`. Both kinds of output were there before this change and come from neither the build nor the code.

The live GL check (E2E step 4) is for the owner only, and was not run, as the plan requires.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/theme_textures.hpp` | CREATE | +333 |
| `src/render/theme_textures.cpp` | CREATE | +1055 |
| `tests/theme_textures_test.cpp` | CREATE | +568 |
| `CMakeLists.txt` | UPDATE | +1/-0 |
| `src/app/app.hpp` | UPDATE | +4/-0 |
| `src/app/app.cpp` | UPDATE | +7/-0 |
| `src/screens/screen.hpp` | UPDATE | +5/-0 |
| `src/main.cpp` | UPDATE | +1/-0 |
| `tests/CMakeLists.txt` | UPDATE | +15/-0 |

## Deviations from Plan

1. **Unknown name in `draw_frame`.** The plan says to draw a flat quad over the caller's rect, and the owner approved that. For `draw_frame` the caller's rect is the banner hole, so this fallback does cover the banner, at 25% of the tint's alpha. The known-entry fallback still draws only the ring. A ring cannot be drawn for an unknown name because there is no `hole_px` geometry.
2. **Extra pure helpers.** I added `frame_ring_rects()` (the four strips of the frame fallback) and `fill_cropped_rect()` (the fill geometry without an entry, used by the unknown-name fallback). Both are covered by tests.
3. **`load()` return value.** `load()` returns false when the manifest gives no textures and no fonts. That covers malformed JSON, a non-object root, and an empty `{}`. The plan said "true when the manifest parsed". Each case logs one line.
4. **Fonts the module does not expose.** `bitmap_fonts` entries other than `digits_chrome` and `digits_white` are parsed but not loaded.
5. **Load options for the digit atlases.** These come from `theme_texture_options(name, ThemeEntry{})`, a default Sprite entry, so the atlases get no mipmaps and Clamp + Linear.
6. **Extra test coverage.** The tests go a little beyond the plan's list:
   - more malformed inputs: a float size, `slice9` borders that are too tall, a drive-colon file name, a font with no glyphs, and unsupported or invalid glyph keys;
   - asymmetric slice3 caps on `diff_row_challenge_selected`;
   - the frame ring rects;
   - an unsupported digit byte logged only once;
   - a missing digit font logged only once.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/theme_textures_test.cpp` | `test_parse_real_manifest`, `test_parse_malformed_never_throws`, `test_sprite_content_box_placement`, `test_slice3_rects`, `test_slice9_rects`, `test_frame_rect`, `test_tile_uv`, `test_fill_cropped_uv`, `test_digit_layout_widths`, `test_load_options`, `test_headless_load_and_draw`, `test_missing_png_and_log_once` |
