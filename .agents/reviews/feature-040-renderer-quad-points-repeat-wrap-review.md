# Code Review: feature/040-renderer-quad-points-repeat-wrap

**Scope**: Branch `feature/040-renderer-quad-points-repeat-wrap` vs `main` (no commits yet; uncommitted + untracked changes), GitHub issue #88
**Recommendation**: APPROVE (with nits)

## Summary

Reviewed the additive renderer primitives for #88: `Vec2`, the namespace-scope `QuadVertex` + pure `quad_vertices(...)`, `GlQuadRenderer::draw_quad_points(...)`, and the `Texture::Wrap` / `Texture::Filter` options plumbed through `from_file` / `from_rgba` via the pure `texture_sampler_params(...)`, plus the new `gl_quad_renderer_test` and extended `texture_test`. All five acceptance criteria of #88 are met: corner order TL,TR,BR,BL with per-corner colours through the existing batch, `Wrap::Repeat` + a Nearest filter option with clamp/linear defaults, existing draw paths untouched (the `gl_quad_renderer.cpp` diff is additions only and the sampler defaults equal the old constants), unit tests for corner order / premultiplied per-vertex colour / UV range > 1, and a headless no-op. Only documentation and test-strength nits found.

`.agents/stories/todo-stories.md` (unrelated owner edit) is out of scope and was not reviewed.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/render/gl_quad_renderer.hpp:77-79` — gradient-exactness claim is broader than the maths.** The comment says linear gradients (`TL==TR && BL==BR`, or `TL==BL && TR==BR`) "are exact" for any quad. With the TL-BR triangle split, triangle 1 makes colour constant along TL-TR and triangle 2 along BL-BR, so a vertical gradient is only crease-free when those two edges are parallel (rects, parallelograms, horizontal-edged trapezoids); same for the horizontal case with TL-BL vs TR-BR. On a general convex quad with non-parallel edges the diagonal shows. Every planned consumer (theme::skew parallelograms) is in the exact case, so this is only a precision fix: e.g. "linear gradients are exact when the two constant-colour edges are parallel (any parallelogram)".

2. **`tests/gl_quad_renderer_test.cpp:44-61,171` — parity test compares against a hand copy, not the real `append_quad`.** `reference_rect_vertices` restates `append_quad`'s axis-aligned maths, so a future change to `append_quad` would not fail this test; it pins `quad_vertices` to a snapshot, not to the live path (the plan's "so the two paths cannot drift apart" overstates it). `vertices_` is private and there is no seam, so this is acceptable as-is; consider a one-line comment in `append_quad` pointing at the test copy so they are edited together.

3. **`tests/gl_quad_renderer_test.cpp:188-192` — `invalid` and `default_texture` are the same thing.** Both are default-constructed (invalid) `Texture`s, so the second `draw_quad_points` call exercises no new path. Harmless; either drop it or note that it is only checking the Add-blend + UV > 1 headless call. (Headless tests can only prove "does not crash"; `TEST_CHECK(!renderer.is_initialized())` is trivially true.)

**Noted, not a finding**: the live GL visual check (slanted gradient parallelogram + crisp tiled scanlines) was not run — the plan scopes it as an owner-only step to do with #89/#95. Per-path wrap/filter choice in `TextureCache` is explicitly deferred to #89.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC 16.2.1) | PASS |
| Warnings gate (changed TUs + unchanged `from_file` callers re-compiled `-fsyntax-only` with the build's own `-O3 -std=c++20 -Wall -Wextra -Wpedantic` flags from `flags.make`: `gl_quad_renderer.cpp`, `texture.cpp`, `noteskin.cpp`, `texture_cache.cpp`, `background_renderer.cpp`, `texture_test.cpp`, `gl_quad_renderer_test.cpp`) | PASS (0 warnings) |
| Lint | N/A (none configured; warnings gate above is the lint) |
| Tests (`bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`) | PASS — 43/43 (was 42; `gl_quad_renderer_test` added) |
| Sandbox check | Confirmed inside the sandbox that `/dev/snd` has 0 entries; `audio_test` ran against miniaudio's "NULL Playback Device" (a null-device fallback that still runs every assertion, not a skip) |
| Skipped / env-guarded tests | None. The `[Texture] No OpenGL context available` lines in `texture_test` are the asserted headless behaviour, not skips |

## What's Good

- `append_quad`, `draw_quad` and both `draw_textured_quad` overloads are byte-for-byte unchanged, a deliberate choice backed by the FMA-contraction probe in the plan, so existing output cannot shift by an ulp.
- The vertex/sampler logic is behind pure, GL-free seams (`quad_vertices`, `texture_sampler_params`), so the issue's test requirements are actually testable headless; `static_assert` on `QuadVertex` guards the GL attribute layout.
- Defaults reproduce the old sampler state exactly and are tested against the GL enum macros; all existing `from_file` callers compile unchanged.
- `draw_quad_points` mirrors `draw_textured_quad`'s guard → white-texture fallback → `bind_texture` flush pattern, and premultiplies per vertex (#59), with a test that transparent white carries no hidden RGB.
- Untrusted-image hardening in `from_file` still runs before the new parameters matter; the test exercises it against the committed `scanlines.png`.

## Recommendation

Ready to merge. The three Low items are optional polish (one doc-comment wording fix, two test-clarity nits) and can be folded in or skipped.
