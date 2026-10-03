# Implementation Report

**Plan**: `.agents/plans/completed/042-ttf-text-rendering-plan.md`
**Branch**: `feature/042-ttf-text-rendering`
**Status**: COMPLETE
**GitHub Issue**: #90

## Summary

Added TrueType text rendering with stb_truetype atlases, in a new module `src/render/ttf_font.{hpp,cpp}` and one stb implementation TU:

- **`validate_sfnt` + `FontFace`** check the sfnt directory before any stb call. The checks are: magic, table count, every table extent in 64-bit math, the required tables and their minimum lengths, and unitsPerEm. `FontFace` then caches the glyph index and advance for the 319 baked code points, plus the V metrics and the cap height.
- **`FontAtlas::bake`** (CPU only) packs U+0020–U+007E, U+00A0–U+00FF and U+0100–U+017F at `STBTT_POINT_SIZE(P)` with 2x2 oversampling, skip-missing and padding 1. It also packs a 1:1 hollow placeholder box. The first width comes from the estimated area and doubles on failure up to the cap. If 2x2 still fails, it retries once at 1x1. The height is cropped to the rows used and rounded to a multiple of 4. `upload()` sends white RGBA with the coverage in alpha and frees the CPU coverage.
- **Pure layout.** `measure_text` and `for_each_text_quad` share one glyph walker that handles zero-width skip, native glyph, ASCII fold or placeholder, kerning, and tracking after every glyph. On top of that come alignment, the line-box-top anchor (baseline = y + ascent), shear `x += shear·(baseline − y)` and Hard2/Hard3 shadows (shadow pass first). `resolve_text_layout` and `text_layout_scale` are pure helpers.
- **`truncate_to_width`** (in `unicode_text`) is generic, measure-based `...` truncation. `truncate_to_cells` is now a thin wrapper over it.
- **`TextRenderer`**:
  - It owns the 4 faces and up to 32 atlases.
  - It pre-bakes the 13 unique (font, size) pairs in the new `theme::text::kAllStyles` at `size_px·s`, and re-bakes only when the height changes.
  - A missing or corrupt font falls back to the bitmap font and logs once.
  - Headless, it logs one no-GL line and bakes nothing; measure still works.
- **Wiring.**
  - `App` owns a `TextRenderer`. It calls `load()` and `set_window_height()` in `init()`, calls `set_window_height()` before each `on_render`, and calls `shutdown()` before `theme_textures_`/`window_`.
  - `ScreenContext::text` is a non-const pointer, wired in `main.cpp`.
  - No screen draws with it yet; the consumers are #92 to #95.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | stb_rect_pack + stb_truetype implementation TU, CMake | `src/render/stb_truetype_impl.cpp`, `CMakeLists.txt` | ✅ |
| 2 | `truncate_to_width`; `truncate_to_cells` wrapper; ported tests | `src/render/unicode_text.{hpp,cpp}`, `src/render/bitmap_font.{hpp,cpp}`, `tests/unicode_text_test.cpp`, `tests/bitmap_font_test.cpp` | ✅ |
| 3 | Types and pure API | `src/render/ttf_font.hpp` | ✅ |
| 4 | Face, atlas, layout | `src/render/ttf_font.cpp` | ✅ |
| 5 | `TextRenderer` | `src/render/ttf_font.cpp` | ✅ |
| 6 | `kAllStyles`, App ownership, context wiring | `src/render/theme.hpp`, `src/app/app.{hpp,cpp}`, `src/screens/screen.hpp`, `src/main.cpp` | ✅ |
| 7 | `ttf_font_test` | `tests/ttf_font_test.cpp`, `tests/CMakeLists.txt` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, `-Wall -Wextra -Wpedantic`) | ✅ 0 compiler warnings, 0 errors. The only `warning` lines come from the pre-existing CMake configure output for SDL/glad |
| Lint | N/A. No linter is configured; the warning-clean build is the lint gate |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 100% tests passed out of 45 (baseline 44, plus `ttf_font_test`) |
| `git diff -- .agents/stories/todo-stories.md` | ✅ No change from this work. The diff shown is the owner's pre-existing uncommitted edit, which was carried onto the branch untouched |

### End-to-end

| Step | Result |
|------|--------|
| 1. Automated pure path (`ttf_font_test`, `unicode_text_test`, `bitmap_font_test`) | ✅ |
| 2a. `./build/blaze-4k --headless --smoke-test 5 --data-dir <tmp>` from the repo root, in bwrap | ✅ `[TextRenderer] Loaded 4/4 fonts`, one no-GL line, `Blaze 4k shut down cleanly.` |
| 2b. Same from a directory outside the repo (fonts resolve via the exe dir, `build/assets/fonts`) | ✅ Same output |
| 2c. Temp copy of the build with `SairaCondensed-Bold.ttf` truncated to 1 KiB | ✅ One line, `Font unavailable: …/SairaCondensed-Bold.ttf (table 'DSIG' extends past the end of the file); SairaBold text uses the bitmap fallback`, then `Loaded 3/4 fonts` and a clean exit |
| 3. Lifetime | ✅ `app_test` passes headless (one `[TextRenderer]` no-GL line per App). `~App()` order is `stop()`, then `text_renderer_.shutdown()`, then `theme_textures_.shutdown()`, then `window_.shutdown()` |
| 4. Live GL check (owner only) | Not run. The plan reserves it for the owner, because it opens a window and plays audio |

In the sandbox, the smoke runs also print ALSA "cannot find card" lines. They come from the bwrap sandbox hiding `/dev/snd`, not from this change.

### Bake probe (scratch, CPU only, deleted afterwards)

All 13 pairs baked at 2x2 oversampling with no 1x fallback:

| Scale | Total RGBA | Bake time | Largest atlas |
|---|---|---|---|
| s=1 | 17.9 MB | 51 ms | 1024x828 |
| s=1.5 | 37.8 MB | 69 ms | — |
| s=2 | 65.9 MB | 97 ms | — |
| s=3 | 143 MB | 318 ms | 4096x1848 |

Every figure is below the plan's estimates, thanks to the cropped heights.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/stb_truetype_impl.cpp` | CREATE | +16 |
| `src/render/ttf_font.hpp` | CREATE | +279 |
| `src/render/ttf_font.cpp` | CREATE | +1068 |
| `tests/ttf_font_test.cpp` | CREATE | +795 |
| `CMakeLists.txt` | UPDATE | +2/-0 |
| `src/app/app.cpp` | UPDATE | +11/-2 |
| `src/app/app.hpp` | UPDATE | +5/-0 |
| `src/main.cpp` | UPDATE | +1/-0 |
| `src/render/bitmap_font.cpp` | UPDATE | +11/-32 |
| `src/render/bitmap_font.hpp` | UPDATE | +2/-0 |
| `src/render/theme.hpp` | UPDATE | +12/-0 |
| `src/render/unicode_text.cpp` | UPDATE | +71/-0 |
| `src/render/unicode_text.hpp` | UPDATE | +23/-3 |
| `src/screens/screen.hpp` | UPDATE | +6/-0 |
| `tests/CMakeLists.txt` | UPDATE | +16/-0 |
| `tests/bitmap_font_test.cpp` | UPDATE | +5/-57 |
| `tests/unicode_text_test.cpp` | UPDATE | +141/-0 |

## Deviations from Plan

1. **Bitmap fallback after a failed atlas bake.** When the face loaded but its atlas could not bake or upload, the 5x7 cells are drawn at the face's pen positions (kerning plus advance plus tracking). They do not use `6·pixel + tracking`. Pinned Semantics has `measure` use the face metrics in that case, so this keeps draw identical to measure and truncation. When the face itself is missing, the plan's `6·pixel + tracking` model applies to both measure and draw, as written.
2. **`covers_text` for a font that did not load** returns the bitmap font's `font_covers_text(text)`, because the bitmap fallback is what will draw it. The plan does not define this case.
3. **`measure`, `ascent`, `line_height` and `truncate` are `const`.** The plan's signatures omitted `const`; only `draw` (lazy bakes) and `set_window_height` mutate. Also, `FontFace::kern_units` takes two baked slots, not raw glyph indices.
4. **Extra sfnt hardening.** Beyond the listed checks, `validate_sfnt` requires minimum lengths for the tables stb reads at fixed offsets: head ≥ 54, hhea ≥ 36, maxp ≥ 6, cmap ≥ 4, hmtx ≥ 4, loca ≥ 2.
5. **stb behaviour adaptations.**
   - With skip-missing on, `stbtt_PackFontRanges` returns 0 whenever a code point was skipped (Audiowide), so its return value is ignored. Success is checked per glyph instead: a packed glyph sits at `x0 ≥ padding`.
   - stb packs a 1-texel rect even for empty glyphs such as space. Glyphs where `stbtt_IsGlyphEmpty` is true are therefore stored as zero-area quads (present, no ink), which matches the "space has a zero-area quad" test.
6. **`for_each_text_quad` scales the quads** by `layout.pixel_size / atlas.pixel_size`, so a mismatched atlas still lines up. This is defensive; in normal use the ratio is 1.
7. **The load summary** is `[TextRenderer] Loaded N/4 fonts`, without a directory, because fonts can resolve individually from the cwd or the exe dir.
8. **No TODO.md edit, and #55 was not touched,** as instructed by the invoking request (adopting the plan's default).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/ttf_font_test.cpp` | 1. `validate_sfnt`: 4 real fonts pass. Rejected: empty, 11 B, random 4 KiB, OTTO, ttcf, numTables 0/1000, table offset past EOF, length overflow (64-bit), missing glyf, unitsPerEm 3, 1 KiB and 50% prefixes; none of these reach stb. 2. Missing, 0-byte, garbage and truncated files. A pack with a corrupt Bold logs exactly one line and `font_available(SairaBold)` is false. Also covered: bitmap-fallback measure, line height, truncation and coverage; two Bold draws log exactly one fallback line; ExtraBold draws log nothing. 3. CPU bake at 24 px: dims, height % 4, UVs in [0,1], all 319 present, `A`/`é` with ink, space zero-area, placeholder with full edges and a hollow middle, `coverage_to_white_rgba`. Audiowide has exactly one absent Latin Extended-A slot. Sizes 0, NaN, 5000 and inf are rejected; a small max_dim is clamped; headless upload fails cleanly. `baked_glyph_slot` table. 4. Measure: empty, kerning (AV), tracking per visible glyph, zero-width, placeholder at 0.6em for CJK, `\xFF`, C0 and C1, folds (’ – TAB fullwidth), degenerate sizes, em scale, linear scaling 720→1440, ascent. 5. Layout: quad count, Left/Centre/Right offsets w/2 and w, first and last glyph pen vs measure, `'H'` on the baseline and cap height, upright edges, italic plus group shear, Hard3 shadow at s=1 and 2 (colour, alpha, offset), Hard2 and tracking scale, placeholder UVs and advance, atlas size mismatch. 6. Real-font truncation over budgets 0..full: no overrun, ellipsis, byte prefix, marks stay attached, NaN/negative budgets. 7. Fuzz, 2000 random strings: measure, truncate, covers_text, draw and for_each_text_quad with random align/shear/shadow/tracking; corners stay finite and quads ≤ 2 × bytes. 8. Headless `TextRenderer`: one no-GL line, no atlases, same height silent, no-op draws, coverage rules, idempotent reload, shutdown. 9. `kAllStyles`: 26 styles, 13 pairs |
| `tests/unicode_text_test.cpp` | Every `truncate_to_cells` assertion ported to `truncate_to_width` with a cell measure, including the 10000-case fuzz. Variable-width measure (`W` = 3): the cut follows width, the ε tolerance holds, and fractional budgets never overrun. Degenerate budgets (NaN, negative, 0, inf, zero-width-only text) |
| `tests/bitmap_font_test.cpp` | `truncate_to_cells` wrapper smoke (`mDaWg` at 17 cells, budgets 2 and 0, fits unchanged) |
