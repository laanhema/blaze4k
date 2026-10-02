# Implementation Report

**Plan**: `.agents/plans/completed/029-remove-receptor-white-fringe-plan.md`
**Branch**: `feature/029-remove-receptor-white-fringe`
**Status**: COMPLETE (code + automated validation); visual check pending owner verification

## Summary

Fixed the light fringe around Cel receptors (and all note art), issue #59, by moving the render pipeline to
premultiplied alpha:

- Every texture upload (`Texture::from_rgba`, `Texture::from_file`) now premultiplies RGB by alpha
  before `glTexImage2D` / `glGenerateMipmap`. This means filtering can no longer average the hidden
  white/grey RGB of fully transparent texels into edge texels.
- `GlQuadRenderer::append_quad` premultiplies the straight-alpha vertex tint on the CPU, so the
  public `Color` API is unchanged for every caller.
- The blend functions are now the premultiplied equivalents: `Alpha` uses `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`
  and `Add` uses `GL_ONE, GL_ONE`. Both match the previous output for unfiltered texels, and the
  shader is unchanged.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure helpers `premultiply(Color)` and `premultiply_alpha(std::span<uint8_t>)` | `src/render/geometry.hpp`, `src/render/texture.hpp`, `src/render/texture.cpp` | ✅ |
| 2 | Premultiply on every texture upload, shared private `upload_premultiplied` | `src/render/texture.cpp`, `src/render/texture.hpp` | ✅ |
| 3 | Premultiplied vertex tint + blend funcs, doc comments | `src/render/gl_quad_renderer.cpp`, `src/render/gl_quad_renderer.hpp`, `src/render/geometry.hpp` | ✅ |
| 4 | Headless unit tests | `tests/texture_test.cpp` | ✅ |
| 5 | Full suite + headless smoke | n/a | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`) | ✅ (0 warnings after forced rebuild of the touched files) |
| Lint | n/a (none configured) |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | ✅ 100% tests passed out of 38 |
| `./build/tests/texture_test` | ✅ All tests passed (3 new cases) |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | ✅ exit 0, "Smoke test finished (10 frames). Exiting cleanly." |
| Offline repro (E2E step 4, optional) | ✅ Ran. Max edge luminance excess of straight over premultiplied is 0.1756 on the receptor (the plan quoted ~0.187) |
| Visual check (E2E step 3, ACs 1–3) | ⏳ Pending owner verification. It needs an interactive window, which the agent can't run |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/geometry.hpp` | UPDATE | +10/-1 |
| `src/render/gl_quad_renderer.cpp` | UPDATE | +11/-7 |
| `src/render/gl_quad_renderer.hpp` | UPDATE | +5/-0 |
| `src/render/texture.cpp` | UPDATE | +42/-11 |
| `src/render/texture.hpp` | UPDATE | +15/-0 |
| `tests/texture_test.cpp` | UPDATE | +66/-1 |

## Deviations from Plan

- `upload_premultiplied` is a **private static member** of `Texture`, not an anonymous-namespace
  function. It has to set the private `id_`/`width_`/`height_` fields. The plan allowed either option.
- The premultiplied vertex colour in `append_quad` is named `pm`, not `c`, because `c` is already
  used for `std::cos(radians)` in the rotation block of the same function.
- Offline repro measured an edge excess of 0.1756, against the plan's "~0.187". The plan's
  figure came from a broader measurement. The direction and size of the defect are confirmed.
- The visual check (E2E step 3) was not run by the agent, as the plan intends. It is pending
  owner verification.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/texture_test.cpp` | `test_premultiply_alpha_bytes` (opaque identity, white/grey transparent → 0, half alpha, rounding at a=1, alpha bytes untouched); `test_premultiply_alpha_partial_and_empty` (empty span no-op, trailing partial pixel ignored); `test_premultiply_color` (identity at a=1, half-alpha math within 1e-6, receptor rest tint unchanged, `static_assert` constexpr) |
