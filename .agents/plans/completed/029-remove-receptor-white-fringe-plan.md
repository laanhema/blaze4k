# Plan: Remove the White Fringe Around Receptors (#59)

## Summary

The Cel receptors show stray light pixels at their edges. The cause is texture filtering on
**straight (non-premultiplied) alpha**, not the receptor's blend mode. Every texture is uploaded
straight-alpha and mipmapped (`src/render/texture.cpp:93-100`). The receptor art has binary alpha
(only 0 and 255), and the RGB of its fully transparent texels is a mix of white (255) and grey (64).
`glGenerateMipmap` and `GL_LINEAR_MIPMAP_LINEAR` average that invisible RGB into partially
transparent edge texels. Thin edge features, such as the 4–10 px sliver at the wing tips, then
come out light instead of dark grey. The fix is the standard one:

- **Premultiply alpha at load time**, so every texel uploaded to GL holds `rgb*a`.
- **Premultiply the vertex tint on the CPU** in `GlQuadRenderer::append_quad`, so the public
  `Color` API stays straight-alpha for every caller.
- **Switch the blend functions** to the premultiplied equivalents:
  - `Alpha`: `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`
  - `Add`: `GL_ONE, GL_ONE`

For fully opaque and fully transparent texels the result is the same as today. Only filtered,
partially transparent edges change, and they now blend toward the art's real colour instead of
toward the hidden RGB. This fixes receptors, tap notes, holds/rolls, mines and explosions in one
place, and the receptor flash/brightness tint behaves exactly as it does now.

## User Story

As a player
I want the receptors (and all note art) to blend cleanly into the song background
So that the note field looks clean like ITG/StepMania with the Cel noteskin, without a light halo

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | `render/texture` (premultiply on upload + pure helper), `render/geometry` (pure `premultiply(Color)`), `render/gl_quad_renderer` (vertex tint premultiply, blend funcs, doc comments), tests (`texture_test`) |
| GitHub Issue | #59 |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Release); `cmake --build build -j16` is incremental |
| C++ compiler | GCC 16.2.1 | C++20 (`std::span` available) |
| Baseline tests | **38/38 pass** | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build` reports "100% tests passed out of 38" (0.46 s), on `main` @ `68755da` |
| Texture upload | `src/render/texture.cpp:72-109` | Straight RGBA8, then `glGenerateMipmap` when `mipmaps` is set. MIN filter is `GL_LINEAR_MIPMAP_LINEAR`, MAG filter is `GL_LINEAR` |
| Cel loads mipmapped | `src/gameplay/noteskin.cpp:224, 252` | `Texture::from_file(..., true)` for all Cel art, which is authored at 2–8x the on-screen size, so it is always minified through the mip chain |
| Blend setup | `src/render/gl_quad_renderer.cpp:177, 188-199` | `Alpha` is `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` and `Add` is `SRC_ALPHA, ONE`. These are the only `glBlendFunc` calls in `src/` (grep) |
| Shader | `gl_quad_renderer.cpp:26-33` | `FragColor = texture(u_tex, v_uv) * v_color;`. The shader is unchanged; premultiplication happens in the data |
| Texture producers | `from_file`: `noteskin.cpp:224,252`, `background_renderer.cpp:35`, `texture_cache.cpp:15`. `from_rgba`: `noteskin.cpp:94` (procedural masks), `texture.cpp:128` (`solid`) | All go through `from_rgba`/`from_file`, so premultiplying there covers every texture |
| Framebuffer clear | `src/app/app.cpp:163` | `glClearColor(..., 1.0f)`, opaque. With `ONE, ONE_MINUS_SRC_ALPHA` the destination alpha stays exactly 1 (previously `a² + (1-a)`) |
| GL in tests | `GlQuadRenderer` and `Texture` uploads are no-ops headless | Testable logic must be pure helpers, following the established pattern (see `layout_hold`, `cel_tap_frame`) |

### Measured asset evidence (this session, Python/Pillow over `assets/noteskins/cel`)

| Asset | Size | Alpha values | RGB of alpha-0 texels |
|-------|------|--------------|-----------------------|
| `_Down Receptor tex 4x1 (res 256x64).png` | 2048x512, frames identical in alpha, bbox x 4..507 / y 12..495 of each 512 frame | **{0, 255} only** | 72,520 white (255), the rest grey (64) |
| `_Down Tap Note 16x16 (doubleres).png` | 2048x2048 | {0, 255} | 397,728 white, 1,793,120 black |
| `Down Hold Body/BottomCap *` | 512x2048 / 512x512 | {0, 255} | **all** white |
| `Down Roll Body/BottomCap *` | | some partial | white and black |
| Tap / hold explosions | 1000x1000 | many partial | mostly white |

Simulated mip level 2 (box-filter 4x, 512 → 128 px, about the drawn 96 px), composited over a dark
background with the 0.55 receptor rest tint:

- Straight alpha shows a visible light "<" speck at the left wing tip.
- Premultiplied alpha does not.
- Per-texel edge luminance excess of straight over premultiplied: up to **+0.187** on the
  receptor and **+0.226** on tap notes (both in 0–1 units).

The repro script is in End-to-End step 4.

**Start green, stay green:** 38 tests pass now. This plan adds cases to the existing `texture_test`
target and no new targets, so **38/38** are expected afterwards.

---

## Pinned Semantics (blend equivalence)

Let `T` be a straight texel `(t.rgb, t.a)` and `V` a straight vertex tint `(v.rgb, v.a)`.

| Mode | Today (straight) | After (premultiplied) | Equal for unfiltered texels? |
|------|------------------|------------------------|------------------------------|
| Alpha | `src = T*V`; `out = src.rgb*src.a + dst*(1-src.a)` = `t.rgb v.rgb t.a v.a + dst(1 - t.a v.a)` | `src = (t.rgb t.a, t.a) * (v.rgb v.a, v.a)`; `out = src.rgb + dst*(1-src.a)`, the same expression | Yes |
| Add (StepMania `BlendMode_Add`) | `out = src.rgb*src.a + dst` | `out = src.rgb + dst`, with `src.rgb` already multiplied by `t.a v.a` | Yes |

Results differ only where filtering mixes texels with different alpha. In those spots the
premultiplied result is the physically correct one. The receptor brightness flash
(`cel_receptor_brightness`, `noteskin.cpp:143-150`) is a tint with `a = 1`, so it is the same
before and after (AC 3).

No upstream constants are pinned by this change, because the ITG timing, scoring and art values
are untouched. Following "faithful, not novel", the visual intent (StepMania's Alpha and Add
blends) is preserved exactly. Only the internal pixel representation changes.

---

## Patterns to Follow

### Pure, GL-free helpers exposed in the header for headless tests
```cpp
// SOURCE: src/render/texture.hpp:19-21
// Probes an image header without decoding pixels. Returns ok=false when the
// file cannot be read/parsed or declares non-positive/oversized dimensions, so
// callers can reject untrusted images before any pixel allocation. Never throws.
[[nodiscard]] ImageHeader probe_image_header(const std::string& path);
```

### Constexpr colour math in geometry.hpp
```cpp
// SOURCE: src/render/geometry.hpp:33-40
[[nodiscard]] constexpr Color with_alpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

[[nodiscard]] constexpr Color multiply(Color a, Color b) {
    return Color{a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a};
}
```

### Error handling (log tag, never throw, invalid texture on bad input)
```cpp
// SOURCE: src/render/texture.cpp:75-82
if (width <= 0 || height <= 0 || rgba == nullptr) {
    std::cerr << "[Texture] Invalid RGBA upload request (" << width << "x" << height << ")\n";
    return texture;
}
if (!gl_available()) {
    std::cerr << "[Texture] No OpenGL context available; skipping texture upload\n";
    return texture;
}
```

### Tests (idiom)
```cpp
// SOURCE: tests/texture_test.cpp:11-19, 98-102
#define TEST_CHECK(expr) \
    do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
         << ": " << #expr << "\n"; std::abort(); } } while (0)

void test_missing_file_and_empty_path() {
    TEST_CHECK(!blaze4k::probe_image_header("does-not-exist-987654.png").ok);
    TEST_CHECK(!blaze4k::Texture::from_file("").valid());
    std::cout << "  - missing file / empty path rejected ok.\n";
}
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/texture.hpp` | UPDATE | Declare `premultiply_alpha(std::span<std::uint8_t>)`, and document that `from_rgba`/`from_file` take straight alpha and store premultiplied |
| `src/render/texture.cpp` | UPDATE | Implement the helper. `from_rgba` copies and premultiplies, `from_file` premultiplies the stbi buffer in place, and both share one private upload path |
| `src/render/geometry.hpp` | UPDATE | Add `constexpr Color premultiply(Color)`. Update the `BlendMode` comment to describe the premultiplied pipeline |
| `src/render/gl_quad_renderer.cpp` | UPDATE | `append_quad` writes `premultiply(color)` into vertices, and the blend funcs become `ONE, ONE_MINUS_SRC_ALPHA` and `ONE, ONE` |
| `src/render/gl_quad_renderer.hpp` | UPDATE | Class doc: colours in are straight alpha, and the pipeline is premultiplied internally |
| `tests/texture_test.cpp` | UPDATE | Unit tests for `premultiply_alpha` and `premultiply(Color)` |

No asset changes. The PNGs stay byte-identical, so user customization PNGs dropped into
`assets/noteskins/cel` still work and get the same fix.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure premultiply helpers

- **Files**: `src/render/geometry.hpp`, `src/render/texture.hpp`, `src/render/texture.cpp`
- **Action**: UPDATE
- **Implement**:
  - `geometry.hpp`: add
    `[[nodiscard]] constexpr Color premultiply(Color c) { return Color{c.r * c.a, c.g * c.a, c.b * c.a, c.a}; }`
    with a one-line comment saying it converts a straight colour to the premultiplied form the
    quad renderer feeds GL.
  - `texture.hpp`: `#include <span>`, then declare
    `void premultiply_alpha(std::span<std::uint8_t> rgba);`. Doc comment: "Multiplies each
    RGBA8 pixel's RGB by its alpha in place (round to nearest). Processes `rgba.size() / 4` whole
    pixels and ignores any trailing partial pixel. Pure (no GL)."
  - `texture.cpp`: implement it with integer math `c = static_cast<uint8_t>((c * a + 127) / 255)`
    (computed in `unsigned`). The alpha byte is left unchanged. Fast path: skip pixels with
    `a == 255`.
- **Mirror**: `probe_image_header` (a pure free function in the same header);
  `geometry.hpp:33-40` for constexpr colour helpers.
- **Validate**: `cmake --build build -j16`

### Task 2: Premultiply on every texture upload

- **File**: `src/render/texture.cpp` (+ doc comments in `texture.hpp:38-51`)
- **Action**: UPDATE
- **Implement**:
  - Move the GL body of `from_rgba` (from `glGenTextures` to the end, `texture.cpp:84-108`) into
    an anonymous-namespace or private static `upload_premultiplied(int w, int h, const uint8_t*
    rgba, bool mipmaps)` that uploads the bytes as given. Keep the existing argument and
    `gl_available()` checks and their log lines in the public factories, so headless behaviour
    and messages don't change.
  - `from_rgba(...)` (still takes straight alpha): after validation, copy into a
    `std::vector<uint8_t>` of `static_cast<std::size_t>(width) * height * 4` (size_t math, so no
    int overflow), call `premultiply_alpha`, then upload.
  - `from_file(...)`: after `stbi_load` succeeds, call
    `premultiply_alpha({pixels, static_cast<std::size_t>(width) * height * 4})` in place. Then
    upload directly. Don't route through `from_rgba`, so a 4096² image isn't copied a second
    time (64 MiB).
  - `solid(Color)` stays as is: it calls `from_rgba`, so it is premultiplied automatically. The
    only caller passes opaque white, which is an identity.
  - Doc comments: `from_rgba`: "`rgba` is straight (non-premultiplied) alpha; the texture stores
    premultiplied alpha so linear/mipmap filtering cannot bleed the RGB of transparent texels
    into edges (#59)." Add the same note to `from_file`.
- **Mirror**: the existing validate-then-upload structure (`texture.cpp:72-109`, `:131-179`).
- **Validate**: `cmake --build build -j16`

### Task 3: Premultiplied tint and blend functions in the quad renderer

- **Files**: `src/render/gl_quad_renderer.cpp`, `src/render/gl_quad_renderer.hpp`,
  `src/render/geometry.hpp`
- **Action**: UPDATE
- **Implement**:
  - `append_quad` (`gl_quad_renderer.cpp:209-218`): at the top, `const Color c = premultiply(color);`
    and build the four vertices from `c`. `draw_quad` (the white texel) and every textured draw then
    go through the same path. Callers keep passing straight colours (`with_alpha`, background dim,
    scrims, explosion fades). The public API does not change.
  - `begin` (`:177`) and the `Alpha` branch of `set_blend_mode` (`:197`): change to
    `glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
  - `Add` branch (`:195`): change to `glBlendFunc(GL_ONE, GL_ONE)`. The source RGB is already
    multiplied by alpha, which gives StepMania's `BlendMode_Add` (`src*alpha + dst`).
  - Leave the shader unchanged (`texture * v_color` of two premultiplied values is premultiplied).
  - Comments:
    - `geometry.hpp:5`: keep "Straight-alpha RGBA color" and add "(the quad renderer premultiplies
      it internally)".
    - `geometry.hpp:29-30` (`BlendMode`): note that `Alpha` is `ONE, ONE_MINUS_SRC_ALPHA` and `Add`
      is `ONE, ONE` over premultiplied source, which is equivalent to StepMania's
      `src*alpha (+ dst*(1-alpha) | + dst)`.
    - `gl_quad_renderer.hpp:9-14` class doc: one sentence saying textures and vertex colours are
      premultiplied internally.
- **Mirror**: the existing `set_blend_mode` structure; don't add new state.
- **Validate**: `cmake --build build -j16`

### Task 4: Headless unit tests

- **File**: `tests/texture_test.cpp`
- **Action**: UPDATE
- **Implement** (add `#include "render/geometry.hpp"` if it is not already pulled in through
  `texture.hpp`), as new `test_*` functions registered in `main` before the "All tests passed"
  line:
  1. `test_premultiply_alpha_bytes`, a buffer of pixels:
     - `{200,100,50,255}` stays unchanged (opaque identity)
     - `{255,255,255,0}` becomes `{0,0,0,0}` (the white transparent texel that caused the fringe)
     - `{64,64,64,0}` becomes `{0,0,0,0}`
     - `{255,255,255,128}` becomes `{128,128,128,128}`
     - `{255,0,100,1}` becomes `{1,0,0,1}` (`(255+127)/255=1`, `(100+127)/255=0`)
     - `{1,1,1,1}` becomes `{0,0,0,1}`

     Check that the alpha bytes are untouched.
  2. `test_premultiply_alpha_partial_and_empty`:
     - An empty span is a no-op and must not crash.
     - A 6-byte buffer has only its first pixel processed. Bytes 4–5 stay unchanged.
  3. `test_premultiply_color`:
     - `premultiply({1,1,1,1})` is unchanged.
     - `premultiply({0.5f,1.0f,0.2f,0.5f})` equals `{0.25f,0.5f,0.1f,0.5f}`, compared within
       1e-6.
     - `premultiply(Color{0.55f,0.55f,0.55f,1.0f})` (the receptor rest tint) is unchanged, which
       pins AC 3 at the data level.
     - Add a `static_assert` on a constexpr call to prove it's usable at compile time.
  - Update the banner text to `"Running Texture header-hardening + premultiply tests..."`.
- **Mirror**: `tests/texture_test.cpp:98-102` (`TEST_CHECK`, one `std::cout` "ok" line per test).
- **Validate**: `cmake --build build -j16 && ./build/tests/texture_test` (if the binary path
  differs, `ctest --test-dir build -R texture_test --output-on-failure`)

### Task 5: Full suite and headless smoke

- **Validate**: see Validation. Expect 38/38 and no new warnings in touched files.

---

## Validation

```bash
# Build
cmake --build build -j16

# Lint: none configured. Check that the touched files compile with no new warnings (-Wall -Wextra -Wpedantic).

# Tests (headless)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure
# expect: 100% tests passed out of 38
```

## End-to-End Verification

1. **Automated (headless, required):** `texture_test` covers the exact `premultiply_alpha` that
   `from_file`/`from_rgba` run on every upload, and the `premultiply(Color)` that `append_quad`
   applies to every vertex. The full ctest run covers `gameplay_screen_test` and
   `note_field_renderer_test`, which drive the render path headless as no-op smoke tests.
2. **App smoke (headless, required):**
   `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   must exit 0. Do **not** launch a windowed or interactive binary from the agent.
3. **Visual check (owner, interactive; the agent doesn't run it; ACs 1–3):**
   `./build/blaze-4k --gameplay-demo <song.sm> --speed 1x`, using one song with a **dark**
   background and one with a **bright** background from `songs/In The Groove*`. Repeat with
   `--downscroll`. Check:
   - receptors have no light speck or halo at the wing tips or the stem outline (AC 1);
   - tap notes, hold/roll bodies and caps, mines, and tap/hold/mine explosions show no light or
     dark fringe, and explosions still brighten additively (AC 2);
   - the receptor still flashes white on each beat and settles to grey within a quarter beat (AC 3);
   - the background dim and the HUD/title/select text and scrims look unchanged, since straight
     callers are now premultiplied internally.
4. **Offline repro (optional, agent-safe):** this script reproduces the measured evidence without
   GL. It needs numpy and Pillow and writes only to the scratchpad.
   ```bash
   cd assets/noteskins/cel && python3 - <<'EOF'
   import numpy as np; from PIL import Image
   a=np.asarray(Image.open('_Down Receptor tex 4x1 (res 256x64).png').convert('RGBA'))[:, :512].astype(float)/255
   def mip(x,k): h,w=x.shape[:2]; return x.reshape(h//k,k,w//k,k,-1).mean(axis=(1,3))
   s=mip(a,4); p=a.copy(); p[...,:3]*=p[...,3:4]; p=mip(p,4)
   edge=(s[...,3]>0.02)&(s[...,3]<0.98)
   print('max edge luminance excess', (s[...,:3]*s[...,3:4]-p[...,:3])[edge].mean(axis=1).max())  # ~0.187
   EOF
   ```

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A future caller adds a `glBlendFunc` or a second shader that assumes straight alpha | Only `gl_quad_renderer.cpp` touches blend state today (grep-verified). Document the premultiplied contract in `gl_quad_renderer.hpp` and `geometry.hpp` | In scope (docs) |
| Alpha-faded straight colours (scrims, `with_alpha` fades, explosion alpha up to 1.0) look different | They are mathematically identical (see Pinned Semantics). `premultiply` happens once per vertex in `append_quad`, so every draw path is covered | In scope (verified by the owner's visual check) |
| Integer premultiply loses precision at very low alpha (e.g. `rgb=1, a=1` becomes 0) | Below 1/255 of contribution, invisible. This is the standard trade-off of RGBA8 premultiplied | Accept |
| Extra load-time work: copying procedural masks (64x64) and in-place passes over Cel sheets (up to 2048x2048 = 4 Mpx) | It is a one-time cost at noteskin/background load, a few ms, with an `a == 255` fast path. No per-frame cost | Accept |
| Destination alpha changes from `a² + dst(1-a)` to `a + dst(1-a)` | The framebuffer is cleared to alpha 1 and stays 1 under both. If anything this removes a latent translucent-window risk on compositors that honour framebuffer alpha | Accept |
| The fringe is partly baked into the art (light outline pixels with alpha 255) | Measurement shows edge alpha-255 texels are grey 64, not white. Only alpha-0 texels are white. If a fringe remains after the fix, record it as a follow-up asset issue. Don't edit vendored PNGs in this change | Out of scope (flag only) |
| Bleeding between neighbouring receptor frames at deep mip levels | The receptor frames are 512 px wide with ≥4 px transparent padding, and mip boundaries align until level 9. Not a visible factor at the 96 px draw size | Out of scope |

---

## Decisions

- **Premultiply at load, not "bleed edge colours into transparent texels".** Edge-bleeding also
  fixes straight-alpha `GL_LINEAR` sampling, but it needs a flood-fill pass, and `glGenerateMipmap`
  still averages alpha and colour separately, which is only correct premultiplied. Premultiplying
  is the textbook fix, a few lines long, and covers every texture.
- **Don't change only the receptor's `BlendMode`** (the issue's TODO suggestion). The technical
  notes on #59 already warn it would alter the look, it would leave the same artefact on taps,
  holds and explosions (AC 2), and Add would wash out the receptor's grey.
- **Premultiply the vertex colour on the CPU** (`append_quad`) rather than in the shader. The
  results are identical, but the CPU version is a pure `constexpr` that can be tested headless.
  The public `Color` stays straight alpha, so there's no churn at call sites.
- **Keep mipmapping on.** It is correct once the data is premultiplied, and it is what keeps the
  2–8x downsampled Cel art from aliasing.

---

## Open Questions

- **Does the owner see any remaining fringe after the fix?** The proposed default is to treat the
  issue as closed if ACs 1–3 pass in the visual check. The evidence (only alpha-0 texels are
  white, alpha-255 edge texels are grey) points to filtering as the only source. If the fringe
  persists, open a follow-up to inspect the specific pixels instead of widening this change.

---

## Acceptance Criteria

- [ ] Receptors render without a visible light fringe over dark and bright song backgrounds (owner, End-to-End step 3)
- [ ] Tap notes, holds, rolls, mines and explosions don't gain or keep a similar fringe (owner, step 3)
- [ ] Receptor flash/brightness behaviour is unchanged (pinned by `premultiply(Color)` identity at `a = 1` and the owner's visual check)
- [ ] All tasks completed; the build has no new warnings in touched files
- [ ] 38/38 tests pass (start green at 38; new cases added to the existing `texture_test`)
- [ ] Follows existing patterns (pure headless-testable helpers, `[Texture]` log tags, `TEST_CHECK` idiom)
