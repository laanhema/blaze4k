# Code Review: feature/029-remove-receptor-white-fringe

**Scope**: Branch `feature/029-remove-receptor-white-fringe` vs `main` (no commits yet; all 6 changed files are uncommitted working-tree modifications). Relates to issue #59. The untracked `.agents/stories/todo-stories.md` is unrelated and was excluded.
**Recommendation**: APPROVE (with nits)

## Summary

The change moves the render pipeline to premultiplied alpha. Every texture upload (`from_rgba`, `from_file`) now premultiplies RGB by alpha before `glTexImage2D`/`glGenerateMipmap`. `append_quad` premultiplies the straight-alpha vertex tint. The blend functions become `ONE, ONE_MINUS_SRC_ALPHA` (Alpha) and `ONE, ONE` (Add). This is the textbook fix for the filtering halo in #59, and it covers receptors, taps, holds/rolls, mines and explosions in one place.

I checked the blend equivalence for both modes and found it correct. The public `Color` API is unchanged for callers. Validation is green, including a strict `-Werror` build. I found no correctness issues, only two cosmetic nits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`tests/texture_test.cpp:144`: the helper is named `near`, which is a reserved macro on Windows.** `<minwindef.h>` (pulled in by `<windows.h>`) defines `near` as an empty macro. If this TU ever includes `windows.h` transitively, for example through a future SDL or GL header, the MSVC build breaks with a confusing error. Nothing includes it today, so it compiles, but Windows is a supported target. Recommendation: rename it to `approx_equal` or similar.
2. **`src/render/texture.cpp:87-88` repeats the doc comment for `upload_premultiplied`** that is already in `src/render/texture.hpp:72-73`. The repo keeps contracts in the header, so drop the .cpp copy. (Also trivial: the declaration uses `uint8_t` and the definition uses `std::uint8_t`.)

### Verification notes (checked, no issue)

- **Blend math.** Let `T` be the texel and `V` the tint, both straight alpha.
  - Alpha: the old `T·V·(t.a v.a) + dst(1 − t.a v.a)` equals the new `(t.rgb t.a)(v.rgb v.a) + dst(1 − t.a v.a)`.
  - Add: the old `T·V·(t.a v.a) + dst` equals the new `(t.rgb t.a)(v.rgb v.a) + dst`.
  - The results differ only at filtered edges, which is the intended fix.
  - Destination alpha stays at 1 because the framebuffer is cleared opaque (`app.cpp:163`).
- **Out-of-range tints.** These would behave differently because GL clamps the fragment output before blending. I grepped the callers: explosion alpha is clamped with `std::min(alpha, 1.0)` (`noteskin.cpp:173`), `lerp`/`quantization_color`/`with_alpha` stay in [0, 1], and no `Color{...}` literal has components above 1. The receptor brightness tint is `a = 1`, where `premultiply` is the identity, so AC 3 holds.
- **Coverage.** Every texture producer goes through `from_rgba` or `from_file`: noteskin art and masks, background, texture cache, the `solid` white texel, and the bitmap font (via `draw_quad` on the white texel). Only `gl_quad_renderer.cpp` sets blend state, and nothing else uploads a straight-alpha texture.
- **`premultiply_alpha`.**
  - The integer math can't overflow (at most 65025 + 127, computed in `unsigned`).
  - It rounds to nearest, leaves alpha untouched, ignores a trailing partial pixel, and has an `a == 255` fast path.
  - `stbi_load` is forced to 4 channels, so the span length `w*h*4` is exact.
- **`from_file`.**
  - It no longer routes through `from_rgba`, but nothing is lost. `stbi_load` success guarantees positive dimensions, the header probe already enforces the 4096 px cap, and `gl_available()` is checked before the decode.
  - Premultiplying in place avoids a second 64 MiB copy for 4096² art.

**Noted, not a finding:**
- The visual ACs 1–2 (no fringe over dark and bright backgrounds) need the owner's interactive check, which the plan leaves to the owner (E2E step 3).
- The blend-func and vertex-premultiply wiring in `append_quad` has no direct test, because the GL path is a headless no-op. Testable logic was moved into pure helpers, as the plan intended.
- Integer premultiply precision loss at very low alpha is an accepted risk in the plan.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j`) | PASS |
| Strict build (scratch dir, `-DCMAKE_CXX_FLAGS=-Werror`, FetchContent disconnected against `build/_deps/*-src`; targets `texture_test` and `blaze-4k`, so all of `blaze4k_core`) | PASS. The only warnings shown come from third-party SDL C code |
| Lint | N/A (none configured) |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | PASS, 38/38. No skipped or env-guarded tests (the "Skipping" lines are parser log output, not test skips) |
| `./build/tests/texture_test` | PASS, including the 3 new premultiply cases |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | PASS (exit 0) |
| Visual check (ACs 1–3) | NOT RUN. Interactive, pending the owner |

## What's Good

- The fix addresses the root cause (filtering straight alpha) rather than the issue's suggested blend-mode workaround, and the plan backs that up with measured asset evidence.
- The public `Color` API stays straight alpha, so no call site changes. The premultiplied contract is documented in `geometry.hpp`, `gl_quad_renderer.hpp` and `texture.hpp`.
- The testable logic is in pure, GL-free helpers (`premultiply_alpha`, `constexpr premultiply`), with edge-case tests: rounding at `a = 1`, a trailing partial pixel, an empty span, and a `static_assert` that `premultiply` is constexpr.
- Validation and log messages in the public factories are unchanged, so headless behaviour is the same.

## Recommendation

Ready to merge after the owner's visual check (E2E step 3) confirms ACs 1–3. Both nits are optional and cosmetic.
