# Code Review: feature/042-ttf-text-rendering

**Scope**: Branch `feature/042-ttf-text-rendering` vs `main` (no commits yet; all changes uncommitted/untracked), GitHub issue #90. `.agents/stories/todo-stories.md` is an unrelated owner edit and is excluded.
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the new `src/render/ttf_font.{hpp,cpp}` module (sfnt validation, `FontFace`, CPU `FontAtlas::bake` + upload, the shared glyph walker behind `measure_text` / `for_each_text_quad`, `TextRenderer`), the stb implementation TU, `truncate_to_width` in `unicode_text` with `truncate_to_cells` reduced to a wrapper, `theme::text::kAllStyles`, the `App` / `ScreenContext` / `main.cpp` wiring, CMake, and the three test files. Every acceptance criterion of #90 is met: 319-code-point atlases at 2x2 oversampling uploaded as white RGBA, tracking / shear / Hard2-Hard3 shadow / three alignments, UTF-8 through `unicode_text` with placeholder boxes, `size_px * h / 720` bakes that re-bake only on a height change, measure-based truncation with all old tests ported, and log-once bitmap fallback for missing or corrupt fonts. Only two Low nits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/render/ttf_font.cpp:280-287` (with `:296` and `:373-375`) — `validate_sfnt` validates the last duplicate table record, but stb uses the first.**
   The required-table loop overwrites `entry.offset` on every matching tag, so the `unitsPerEm` check at `:296` reads the *last* `head` record. `stbtt__find_table` (`stb_truetype.h:1306-1316`) and `FontFace::from_bytes` (`:374`, via `info.head`) use the *first*. A file with two `head` records can pass validation with `unitsPerEm = 0` in the one stb actually reads; `STBTT_POINT_SIZE` then divides by zero inside the packer. This needs a crafted font, which the plan accepts as a risk, but it is a gap in the in-scope "validate before stb" layer and is a two-line fix. *Recommendation:* reject a duplicate required tag in `validate_sfnt` (`if (entry.found) return fail(error, "duplicate table ...")`), or keep the first match like stb does.

2. **`src/render/ttf_font.cpp:1022` vs `:918-925` — the bitmap fallback with a loaded face does not sit on the face's baseline.**
   When the face loaded but its atlas failed to bake or upload, `ascent()` still returns the face ascent (Saira: 1.135em, from `hhea`), but `draw_bitmap_fallback` puts the 7 rows at `y + 0.25P .. y + 0.95P`, so the text ends 0.185em (about 8 px for a 44 px title at 720p) above the baseline that a screen aligns to with `ascent()`. Deviation 1 already keeps the horizontal pen in sync with `measure()`. This only matters on the degraded path (an atlas that does not fit at very large window heights, or a failed upload). *Recommendation:* when `source != nullptr`, anchor the rows so they end at `y + face ascent` (the baseline), the same way the horizontal pen follows the face.

**Noted, not a finding**
- Full stb_truetype hardening: `validate_sfnt` checks only the table directory, not `loca` / `glyf` / `cmap` subtable or `GPOS` internals. A bit-flipped font that passes the directory check can still hit stb. The plan's Risks table accepts this ("In scope (validation); out of scope (full hardening)"), and a truncated file, the realistic corruption, is caught by the table-extent check.
- Re-bake stalls: each height change re-bakes all 13 pairs (about 51 ms at 720p, 318 ms at 4K per the report). A continuous drag-resize re-bakes on each frame that changes height. The plan documents this as an accepted, presentation-only cost (Risks, "Bake time on startup and on resize"). The music clock and judgments are unaffected.
- GPU memory of RGBA atlases (about 143 MB at 4K): the R8 alternative is deferred to #98 by the plan.
- `ScreenContext::text` is null in headless and unit tests. #92 to #95 must null-check; the plan documents this.
- The `kAllStyles` count pin (26) in `ttf_font_test` catches removals from the list but not a new style that was never added. That is inherent to C++ without reflection. The lazy-bake log line makes an omission visible at runtime.

## Verification notes

- **stb packing semantics**, checked against the vendored `stb_truetype.h`. With skip-missing on, missing code points get 0x0 rects, which leave their `chardata` zeroed and make `stbtt_PackFontRanges` return 0. So ignoring the return value and checking `x0 >= padding` for each glyph is correct. Packed rects are placed in a `pw - padding` target and shifted by `+pad`, so the placeholder box at `rect.{x,y} + 1` stays inside its rect and the atlas. `stbtt_GetPackedQuad` is called with the cropped height, so the `t` coordinates match the uploaded texture.
- **Layout.** Measure and draw share `walk_glyphs`, so kerning, tracking (CSS letter-spacing, the last glyph included), zero-width skip, folds and the 0.6em placeholder advance cannot diverge. The shear pivot of the shadow pass is the shadow's own baseline, so the shadow is an exact offset copy. `kItalicShear` (0.25) and `kComboGroupShear` add as the issue asks.
- **Truncation.** `truncate_to_width` with the 6-unit cell measure reproduces the old `truncate_to_cells` contract exactly: ellipsis iff `max_cells >= 3`, keep `max_cells - 3` cells, cut only right before a visible code point, so marks stay attached and leading marks are kept. Every old assertion, including the 10000-case fuzz, was ported to `unicode_text_test`. The binary search only accepts measured candidates, so the no-overrun guarantee holds even with kerning.
- **Lifetime.** `~App` calls `text_renderer_.shutdown()` before `theme_textures_.shutdown()` and `window_.shutdown()`. `shell` is declared after `app` in `main.cpp`, so it is destroyed first. `atlases_` is reserved to 32 and capped at 32, so the `AtlasSlot&` / `FontAtlas*` returned by `bake_slot` / `atlas_for` are never invalidated by a reallocation. The `stbtt_fontinfo` points into heap-owned bytes behind a `unique_ptr`, so moving a `FontFace` is safe.
- **Coordinate space.** `Window::height()` is the pixel height (`SDL_GetWindowSizeInPixels` / `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`), and `GlQuadRenderer` projects in framebuffer pixels. So `s = h / 720` gives crisp atlases on HiDPI. Headless detection uses the same `glad_glGenTextures` check as `texture.cpp:22` and `theme_textures.cpp:858`.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | PASS |
| Warnings gate: every changed/new TU (`ttf_font.cpp`, `stb_truetype_impl.cpp`, `unicode_text.cpp`, `bitmap_font.cpp`, `app.cpp`, `main.cpp`, `ttf_font_test.cpp`, `unicode_text_test.cpp`, `bitmap_font_test.cpp`) recompiled to `/dev/null` with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic` plus the `flags.make` includes) | PASS (0 warnings) |
| Lint | N/A (no linter configured; the warnings gate stands in for it) |
| Tests (`bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; inside the sandbox `/dev/snd` and `/run/user/$UID` were confirmed empty first) | PASS (45/45, including the new `ttf_font_test`) |
| Skipped / env-guarded tests | None. `ttf_font_test` prints `[Texture] No OpenGL context available; skipping texture upload` once: that is the deliberate headless-upload assertion (upload fails cleanly), not a skipped test. Every assertion runs |
| Live GL render (window + audio) | Not run. The plan reserves it for the owner |

## What's Good

- The validate-before-stb layer is careful: 64-bit extent math, minimum lengths for the tables stb reads at fixed offsets, a 16 MiB cap checked before reading, and readable reasons in the single log line.
- One shared glyph walker makes "measure equals draw" structural rather than tested-for, and the pure `for_each_text_quad` / CPU `bake` split lets the tests pin real-font geometry with no GL.
- The atlas bake is economical: area-estimated first width, cropped height, and coverage freed after upload. The report backs the memory and time claims with a probe.
- `truncate_to_width` is a clean generalisation with a precise contract, and the variable-width and degenerate-budget tests (NaN, negative, inf, zero-width-only) are thorough.
- Logging follows the project's log-once conventions, and the steady state allocates nothing per frame (`set_window_height` is O(1) when the height is unchanged).

## Recommendation

Mergeable as is. Optionally fold in the two Low nits before the PR: the duplicate-table check is a two-line hardening fix, and the fallback baseline only affects the degraded path. The owner's live GL check (plan step 4: compare against `cabinet-v3-select.png` and resize once to see one re-bake line) remains before the screens in #92 to #95 start drawing with `ctx.text`.
