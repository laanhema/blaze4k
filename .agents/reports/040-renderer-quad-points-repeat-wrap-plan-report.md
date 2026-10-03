# Implementation Report

**Plan**: `.agents/plans/completed/040-renderer-quad-points-repeat-wrap-plan.md`
**Branch**: `feature/040-renderer-quad-points-repeat-wrap`
**Status**: COMPLETE
**GitHub Issue**: #88

## Summary

Added two additive renderer primitives for the Cabinet theme (#89, #90, #95):

1. `GlQuadRenderer::draw_quad_points(corners, texture, uv, colours)` draws a general quad. Corners and colours come in TL, TR, BR, BL order, and each corner has its own straight-alpha colour. It is built on a new pure, GL-free `quad_vertices(...)`, which uses the same vertex layout, triangle order (TL,TR,BR / TL,BR,BL), UV corner mapping and per-vertex `premultiply` as `append_quad`. The vertices go through the existing batch and `bind_texture` flush, and an invalid texture falls back to the white texture. The private `Vertex` struct became the namespace-scope `QuadVertex`, with the same 8 floats in the same order, kept inside the class as `using Vertex = QuadVertex;` and checked by a `static_assert` on its size. `Vec2` was added to `geometry.hpp`.
2. `Texture::from_file` and `Texture::from_rgba` take two new defaulted parameters: `Texture::Wrap {Clamp, Repeat}` and `Texture::Filter {Linear, Nearest}`. A new pure function `texture_sampler_params(mipmaps, wrap, filter)` maps them to GL enum values, and `upload_premultiplied` now uses it in place of the hard-coded `GL_LINEAR` / `GL_CLAMP_TO_EDGE`. With the defaults it produces exactly the old values (tested). Nearest with mipmaps maps to `GL_NEAREST_MIPMAP_NEAREST`.

`append_quad`, `draw_quad` and both `draw_textured_quad` overloads are byte-for-byte unchanged: the diff of `gl_quad_renderer.cpp` only adds lines.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Add `Vec2` | `src/render/geometry.hpp` | ✅ |
| 2 | `QuadVertex`, pure `quad_vertices`, `draw_quad_points` | `src/render/gl_quad_renderer.hpp`, `src/render/gl_quad_renderer.cpp` | ✅ |
| 3 | `Texture::Wrap` / `Texture::Filter`, `SamplerParams`, `texture_sampler_params`, sampler plumbing | `src/render/texture.hpp`, `src/render/texture.cpp` | ✅ |
| 4 | New `gl_quad_renderer_test` + registration | `tests/gl_quad_renderer_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 5 | Extend `texture_test` | `tests/texture_test.cpp`, `tests/CMakeLists.txt` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Release build (`cmake --build build -j$(nproc)`; touched TUs force-recompiled) | ✅ exit 0, 0 compiler `warning:`/`error:` lines (the only output is the existing third-party CMake configure warnings from SDL3/glad) |
| Debug build (`cmake --build build-debug -j$(nproc)`, 49 TUs compiled) | ✅ exit 0, 0 compiler warnings |
| Lint (none configured; `-Wall -Wextra -Wpedantic` build is the gate) | ✅ |
| Tests, Release (sandboxed `bwrap ... ctest --test-dir build`) | ✅ 100% tests passed out of 43 (was 42) |
| Tests, Debug (sandboxed `ctest --test-dir build-debug`) | ✅ 100% tests passed out of 43 |
| `git diff -U0 src/render/gl_quad_renderer.cpp` removed lines | ✅ none (additions only) |
| `from_file(` / `from_rgba(` callers outside `src/render/texture*` | ✅ unchanged (`noteskin.cpp:94,224,252`, `background_renderer.cpp:35`, plus `texture_cache.cpp:16`, which the grep's `-v src/render/texture` filter hides) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/geometry.hpp` | UPDATE | +6/-0 |
| `src/render/gl_quad_renderer.hpp` | UPDATE | +39/-10 |
| `src/render/gl_quad_renderer.cpp` | UPDATE | +27/-0 |
| `src/render/texture.hpp` | UPDATE | +26/-3 |
| `src/render/texture.cpp` | UPDATE | +34/-10 |
| `tests/gl_quad_renderer_test.cpp` | CREATE | +210 |
| `tests/texture_test.cpp` | UPDATE | +61/-0 |
| `tests/CMakeLists.txt` | UPDATE | +15/-0 |

`TODO.md` and `.agents/stories/todo-stories.md` were not touched. The latter has the owner's unrelated uncommitted edits, which came onto the branch as they were and are not staged.

## Deviations from Plan

- **Branch created from a dirty `main`.** The only tracked change was the owner's off-limits `.agents/stories/todo-stories.md`. The invoking request asked for the feature branch explicitly, so that change came onto the branch untouched and unstaged.
- **`texture_test` gets `BLAZE4K_ASSETS_DIR`** (an optional part of the plan): `tests/CMakeLists.txt` adds a `target_compile_definitions` for `texture_test`, mirroring `background_test`. This lets `test_wrap_overloads_headless` probe the committed `assets/theme/cabinet/scanlines.png`, which passes header hardening, and confirm that `from_file(..., Repeat, Nearest)` is still invalid headless.
- **E2E step 4 (live GL visual check) not run.** The plan marks it owner-only, because launching the GUI opens a window and plays audio. Steps 1–3 (pure-path proof, existing output unchanged, headless safety) passed. To do: the owner runs the optional live check (a slanted gradient parallelogram plus crisp tiled scanlines) before or along with #89 and #95.
- **Validation grep note.** The plan's `git grep ... | grep -v "src/render/texture"` also filters out `src/render/texture_cache.cpp:16`. That caller was checked separately and is unchanged.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/gl_quad_renderer_test.cpp` (new) | `test_corner_order`, `test_per_vertex_colour_premultiplied` (incl. transparent white → `{0,0,0,0}`, #59), `test_uv_mapping_repeat_range` (`{0,0,4,2.5}` unclamped; `{-0.5,0.25,3.5,8.25}` exact), `test_parallelogram_skew` (`theme::skew::kRows`), `test_layout_parity_with_append_quad` (27 rect/UV/colour combos, exact field equality vs. a reference copy of `append_quad`'s axis-aligned maths), `test_headless_noop` |
| `tests/texture_test.cpp` | `test_sampler_params_defaults_unchanged`, `test_sampler_params_repeat_nearest` (scanlines, life_stripes, Nearest+mipmaps, Repeat+mipmaps), `test_wrap_overloads_headless` (empty path, missing file, committed `scanlines.png`, `from_rgba`) |
