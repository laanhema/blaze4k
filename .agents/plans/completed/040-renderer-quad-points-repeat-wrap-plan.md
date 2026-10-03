# Plan: General-Quad and Repeat-Wrap Renderer Primitives (#88)

## Summary

The Cabinet theme needs three things the renderer cannot draw today: parallelograms (meter ticks, judgment bars, chips, using `theme::skew`), per-corner gradients, and sheared italic glyphs. It also needs tiling textures (`scanlines.png` at 1×3 and `life_stripes.png` at 16×24). This change adds two small, additive primitives.

1. **`GlQuadRenderer::draw_quad_points(corners, texture, uv, colours)`** takes four corners in TL, TR, BR, BL order, and each corner has its own straight-alpha colour. It builds its six vertices through a new **pure, GL-free** helper `quad_vertices(...)`. The helper uses the same `x, y, u, v, r, g, b, a` layout, the same triangle order (TL, TR, BR / TL, BR, BL), the same UV corner mapping and the same `premultiply` as `append_quad`. The vertices then go through the existing batch and the existing texture-change flush (`bind_texture`).
2. **`Texture::from_file(path, mipmaps, wrap, filter)`** (and `from_rgba`, for symmetry) gets two optional parameters: `Texture::Wrap::{Clamp, Repeat}` and `Texture::Filter::{Linear, Nearest}`. The defaults are `Clamp` and `Linear`, which are today's values. A new pure function `texture_sampler_params(mipmaps, wrap, filter)` turns them into GL enum values, and `upload_premultiplied` uses its result in place of the hard-coded `GL_LINEAR` / `GL_CLAMP_TO_EDGE` at `texture.cpp:105-109`.

`append_quad`, `draw_quad` and both `draw_textured_quad` overloads stay **byte-for-byte unchanged**. A scratch probe showed why: moving the rotation math into a shared helper is bit-identical on baseline x86-64, but on FMA-contracting targets (`-march=native`, and ARM64 by default) it differs in about 1133 of 12000 cases by about 1 ulp. A new test pins the layout parity between `quad_vertices` and a reference copy of `append_quad`'s maths, so the two paths cannot drift apart. The pure helpers are tested in a new `tests/gl_quad_renderer_test.cpp`: corner order, per-vertex premultiplied colour, UV ranges above 1 passed through unclamped, and a headless no-op. They are also tested in the existing `tests/texture_test.cpp`: sampler params, and the new overloads staying invalid without GL. No new shaders, no change to `BlendMode`, and no consumer changes. The theme loaders in #89, #90 and #95 will call these APIs.

## User Story

As the developer of Blaze 4k
I want a four-corner, per-vertex-colour quad call and a repeat/nearest texture option in the renderer
So that the Cabinet theme screens (#89, #90, #95) can draw parallelograms, gradients, italic glyphs and tiled overlays without new shaders or changes to existing draw output

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT (additive renderer API) |
| Complexity | LOW |
| Systems Affected | `src/render/geometry.hpp`, `src/render/gl_quad_renderer.{hpp,cpp}`, `src/render/texture.{hpp,cpp}`, `tests/gl_quad_renderer_test.cpp` (new), `tests/texture_test.cpp`, `tests/CMakeLists.txt` |
| GitHub Issue | #88 (blocks #89, #90, #95; blocked by none) |
| Story | `[TODO-16]` in `.agents/stories/todo-stories.md:532` (that file is **off-limits** in this change, see Environment Findings) |
| Branch suggestion | `feature/040-renderer-quad-points-repeat-wrap` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build incrementally with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `CMAKE_CXX_EXTENSIONS OFF` (`CMakeLists.txt:5-7`), with `-Wall -Wextra -Wpedantic` (`CMakeLists.txt:13`) and no `-Werror`. The change must add **no new warnings** |
| Baseline tests | **42/42 pass** | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` printed "100% tests passed out of 42" (0.43 s) on `main` @ `af6611b`. Start green and stay green. After this change the count is **43** (`gl_quad_renderer_test` added) |
| Sandbox requirement | — | `audio_test` opens the real audio device, so **always** run ctest through the `bwrap` command above |
| GL headers in tests | `CMakeLists.txt:143-150` | `blaze4k_core` links `glad` **PUBLIC**, so a test can `#include <glad/glad.h>` and compare against `GL_REPEAT`, `GL_NEAREST` and the rest without calling GL. Those are macros and need no context |
| Headless renderer | `tests/background_test.cpp:71-73`, `tests/bitmap_font_test.cpp:165` | Tests build an **uninitialized** `GlQuadRenderer` and call draw methods as no-ops. `vertices_` is private, so batch contents cannot be seen. That is why the vertex math needs a pure seam |
| FMA / refactor probe | scratch `probe.cpp` | The old in-place rotation was compared with the same maths moved into a non-inlined helper. With `g++ -O0` and `-O2` there were 0 mismatches. With `-O2 -march=native` there were **1133/12000** mismatches (about 1 ulp, from FMA contraction differing by inlining context). ⇒ Do **not** refactor `append_quad`. Leave its body untouched |
| `Vec2` type | grep `src/` | **Does not exist.** Add `struct Vec2 { float x; float y; }` to `src/render/geometry.hpp`, next to `Rect` |
| Theme textures that need the new options | `assets/theme/cabinet/` | `scanlines.png` is 1×3 RGBA and needs `Repeat` + `Nearest`. `life_stripes.png` is 16×24 RGBA and needs `Repeat` + `Linear`. Both are NPOT, and GL 3.3 core fully supports `GL_REPEAT` on NPOT textures |
| Skews that will use the quad | `src/render/theme.hpp:218-224`, `theme.hpp:139` | `theme::skew::{kLogo, kRows, kWheel, kStatPanel}` and `kComboGroupShear` are tangents, so a consumer computes `dx = h * skew` per corner. Consumers are out of scope here |
| Current `from_file` callers | `src/gameplay/noteskin.cpp:224,252` (`from_file(p, true)`), `src/render/background_renderer.cpp:35`, `src/render/texture_cache.cpp:16` (`from_file(p)`) | The new trailing parameters are defaulted, so **no caller changes** |
| TODO tracking | `TODO.md:17` | This is the Cabinet umbrella entry (#87–#98), so **do not tick it** for this issue. `.agents/stories/todo-stories.md` has unrelated uncommitted owner edits and is **off-limits** (do not stage, revert or edit it) |

---

## Patterns to Follow

### Vertex layout, triangle order and premultiply (the contract `quad_vertices` must reproduce)

```cpp
// SOURCE: src/render/gl_quad_renderer.cpp:209-243 (append_quad) — DO NOT MODIFY
    const Color pm = premultiply(color);

    Vertex top_left{x0, y0, uv.u0, uv.v0, pm.r, pm.g, pm.b, pm.a};
    Vertex top_right{x1, y0, uv.u1, uv.v0, pm.r, pm.g, pm.b, pm.a};
    Vertex bottom_right{x1, y1, uv.u1, uv.v1, pm.r, pm.g, pm.b, pm.a};
    Vertex bottom_left{x0, y1, uv.u0, uv.v1, pm.r, pm.g, pm.b, pm.a};
    ...
    vertices_.push_back(top_left);
    vertices_.push_back(top_right);
    vertices_.push_back(bottom_right);
    vertices_.push_back(top_left);
    vertices_.push_back(bottom_right);
    vertices_.push_back(bottom_left);
```

### Draw entry point: headless guard, then invalid-texture → white fallback, then bind (flushes on change), then append

```cpp
// SOURCE: src/render/gl_quad_renderer.cpp:257-265
void GlQuadRenderer::draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv,
                                        Color color, float radians) {
    if (!initialized_) {
        return;
    }
    const unsigned int id = texture.valid() ? texture.id() : white_.id();
    bind_texture(id);
    append_quad(rect, uv, color, radians);
}
```

### Header doc comments describe the contract, including straight vs premultiplied alpha

```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:40-43
    // Textured quad rotated `radians` about its center (clockwise on screen,
    // since y grows downward). Lets one-direction art serve every column.
    void draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color,
                            float radians);
```

### Pure, GL-free helpers live in the render headers next to the GL class and are documented as pure

```cpp
// SOURCE: src/render/texture.hpp:24-27
// Multiplies each RGBA8 pixel's RGB by its alpha in place (round to nearest).
// Processes `rgba.size() / 4` whole pixels and ignores any trailing partial
// pixel. Pure (no GL).
void premultiply_alpha(std::span<std::uint8_t> rgba);
```

### Hard-coded sampler state to replace (the only GL change)

```cpp
// SOURCE: src/render/texture.cpp:102-109
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

### Error handling

```cpp
// SOURCE: src/render/texture.cpp:118-128 — invalid input / no GL: log a [Texture] line, return an invalid texture, never throw
    if (width <= 0 || height <= 0 || rgba == nullptr) {
        std::cerr << "[Texture] Invalid RGBA upload request (" << width << "x" << height << ")\n";
        return texture;
    }
```

There are no new failure paths. `draw_quad_points` on an uninitialized renderer returns at once, and an invalid texture falls back to `white_`, matching `draw_textured_quad`. The untrusted-image hardening in `from_file` (size cap, header probe, `kMaxImageDimension`) stays unchanged and still runs **before** the new parameters matter.

### Tests (plain executable, `TEST_CHECK` abort macro, one print line per case, explicit call list in `main`)

```cpp
// SOURCE: tests/texture_test.cpp:13-21, 151-158, 196-207
#define TEST_CHECK(expr) \
    do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr << "\n"; std::abort(); } } while (0)
...
bool approx_equal(float a, float b) { return std::fabs(a - b) <= 1e-6f; }
...
int main() {
    std::cout << "[texture_test] Running Texture header-hardening + premultiply tests...\n";
    test_oversized_header_rejected_before_decode();
    ...
    std::cout << "[texture_test] All tests passed!\n";
    return 0;
}
```

```cmake
# SOURCE: tests/CMakeLists.txt:257-265
add_executable(texture_test
    texture_test.cpp
)

target_link_libraries(texture_test PRIVATE
    blaze4k_core
)

add_test(NAME texture_test COMMAND texture_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/geometry.hpp` | UPDATE | Add `struct Vec2 { float x = 0.0f; float y = 0.0f; };` (pixel-space point, top-left origin) |
| `src/render/gl_quad_renderer.hpp` | UPDATE | Add `#include <array>`. Add the namespace-scope `struct QuadVertex` (the vertex layout, moved out of the private `Vertex`) and the pure `quad_vertices(...)`. Add the public `draw_quad_points(...)` and a private `append_vertices` (or inline the push in `draw_quad_points`) |
| `src/render/gl_quad_renderer.cpp` | UPDATE | Implement `quad_vertices` and `draw_quad_points`. `append_quad` and the other draw calls stay **untouched** |
| `src/render/texture.hpp` | UPDATE | Add the nested `enum class Wrap { Clamp, Repeat }` and `enum class Filter { Linear, Nearest }`, `struct SamplerParams`, and the pure `texture_sampler_params(...)`. Extend `from_file`, `from_rgba` and the private `upload_premultiplied` with defaulted `wrap` / `filter` parameters |
| `src/render/texture.cpp` | UPDATE | Implement `texture_sampler_params`, and thread `wrap` / `filter` through to `upload_premultiplied`, which uses the params in place of the hard-coded constants |
| `tests/gl_quad_renderer_test.cpp` | CREATE | Tests for corner order, per-vertex premultiplied colour, UV pass-through (repeat range > 1), parallelogram/shear corners, `append_quad` layout parity, and headless no-op |
| `tests/texture_test.cpp` | UPDATE | `texture_sampler_params` default equals today's constants, plus Repeat/Nearest mappings. The new `from_file` / `from_rgba` overloads stay invalid headless |
| `tests/CMakeLists.txt` | UPDATE | Register `gl_quad_renderer_test` (mirror `texture_test` block) |

Not changed: `CMakeLists.txt` (no new source files in `blaze4k_core`), the shaders, `BlendMode`, every existing caller, `TextureCache` (theme-texture loading with per-manifest wrap is #89), `TODO.md`, and `.agents/stories/todo-stories.md`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Add `Vec2`

- **File**: `src/render/geometry.hpp`
- **Action**: UPDATE
- **Implement**: After `Rect` (line 15-20), add:
  ```cpp
  // Pixel-space point with a top-left origin (y grows downward).
  struct Vec2 {
      float x = 0.0f;
      float y = 0.0f;
  };
  ```
  It is a plain aggregate like `Rect`. Do not add operators, because consumers compute skew offsets inline.
- **Mirror**: `src/render/geometry.hpp:14-20` (`Rect`)
- **Validate**: `cmake --build build -j$(nproc)` (no new warnings)

### Task 2: Pure vertex builder + `draw_quad_points`

- **File**: `src/render/gl_quad_renderer.hpp`, `src/render/gl_quad_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Header: add `#include <array>`. Move the private `struct Vertex` (hpp:53-62) out to namespace scope as `struct QuadVertex` with the **same eight float fields in the same order** (x, y, u, v, r, g, b, a). Inside the class, keep `using Vertex = QuadVertex;` so `vertices_`, `flush()` (`sizeof(Vertex)`) and `append_quad` compile unchanged, and the attribute pointer offsets in `init()` (cpp:117-123) stay valid.
  2. Header: declare the pure helper at namespace scope:
     ```cpp
     // Builds the six vertices (two triangles: TL,TR,BR and TL,BR,BL) of a general
     // quad. `corners` and `colours` are in TL, TR, BR, BL order; UVs map
     // TL=(u0,v0), TR=(u1,v0), BR=(u1,v1), BL=(u0,v1) and are passed through
     // unclamped (a Repeat-wrapped texture tiles when the range exceeds 1).
     // Colours are straight alpha and are premultiplied per vertex (#59), exactly
     // like append_quad. Pure (no GL).
     [[nodiscard]] std::array<QuadVertex, 6> quad_vertices(const std::array<Vec2, 4>& corners,
                                                           const UVRect& uv,
                                                           const std::array<Color, 4>& colours);
     ```
  3. Header: add the public draw call next to `draw_textured_quad` (after hpp:43):
     ```cpp
     // General quad from four corners (TL, TR, BR, BL; any convex quad, e.g. a
     // parallelogram from a theme::skew), each with its own straight-alpha colour.
     // An invalid `texture` draws solid (the white texture), like draw_textured_quad.
     // Colour and UV are interpolated per triangle (split along TL-BR): linear
     // gradients (TL==TR and BL==BR, or TL==BL and TR==BR) are exact; four distinct
     // colours or a non-parallelogram with texture shows the diagonal.
     void draw_quad_points(const std::array<Vec2, 4>& corners, const Texture& texture,
                           const UVRect& uv, const std::array<Color, 4>& colours);
     ```
  4. Source: implement `quad_vertices`. Build the four `QuadVertex`es with `premultiply(colours[i])` and the UV mapping above, and return `{tl, tr, br, tl, br, bl}`. Implement `draw_quad_points` exactly like `draw_textured_quad` (cpp:257-265): `if (!initialized_) return;` → `bind_texture(texture.valid() ? texture.id() : white_.id());` → `const auto quad = quad_vertices(corners, uv, colours); vertices_.insert(vertices_.end(), quad.begin(), quad.end());`.
  5. **Do not touch** `append_quad`, `draw_quad`, or either `draw_textured_quad` (see the FMA finding in Environment Findings).
  6. Class doc comment (hpp:9-19): add one sentence noting that `draw_quad_points` is the general-quad path that shares the vertex layout and premultiplication.
- **Mirror**: `src/render/gl_quad_renderer.cpp:209-243` (vertex/UV/premultiply contract), `:257-265` (guard + bind + append)
- **Validate**: `cmake --build build -j$(nproc)`; `git diff src/render/gl_quad_renderer.cpp` must show **no** modified lines inside `append_quad`, `draw_quad` or `draw_textured_quad` (additions only)

### Task 3: Wrap / filter options for textures

- **File**: `src/render/texture.hpp`, `src/render/texture.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Header, inside `class Texture` (public, before the factories):
     ```cpp
     // Texture-coordinate wrap. Clamp (default) is GL_CLAMP_TO_EDGE; Repeat tiles
     // when a UV range exceeds [0,1] (scanlines.png, life_stripes.png).
     enum class Wrap { Clamp, Repeat };
     // Sampling filter. Linear (default) is bilinear (trilinear with mipmaps);
     // Nearest keeps pixel-exact art crisp (scanlines.png).
     enum class Filter { Linear, Nearest };
     ```
  2. Header, namespace scope after the class (or before it, with forward use via `Texture::Wrap`):
     ```cpp
     // GL sampler enum values for a texture's options. Pure (no GL calls); the
     // defaults reproduce the pre-#88 state (LINEAR[_MIPMAP_LINEAR] + CLAMP_TO_EDGE).
     struct SamplerParams { int min_filter; int mag_filter; int wrap; };
     [[nodiscard]] SamplerParams texture_sampler_params(bool mipmaps, Texture::Wrap wrap,
                                                        Texture::Filter filter);
     ```
     Mapping:
     | filter | mipmaps | min | mag |
     |---|---|---|---|
     | Linear | false | `GL_LINEAR` | `GL_LINEAR` |
     | Linear | true | `GL_LINEAR_MIPMAP_LINEAR` | `GL_LINEAR` |
     | Nearest | false | `GL_NEAREST` | `GL_NEAREST` |
     | Nearest | true | `GL_NEAREST_MIPMAP_NEAREST` | `GL_NEAREST` |

     `wrap`: Clamp → `GL_CLAMP_TO_EDGE`, Repeat → `GL_REPEAT` (applied to both S and T).
  3. Change the signatures (keeping the defaults so every caller compiles unchanged):
     - `from_rgba(int width, int height, const uint8_t* rgba, bool mipmaps = false, Wrap wrap = Wrap::Clamp, Filter filter = Filter::Linear)`
     - `from_file(const std::string& path, bool mipmaps = false, Wrap wrap = Wrap::Clamp, Filter filter = Filter::Linear)`
     - private `upload_premultiplied(int, int, const std::uint8_t*, bool mipmaps, Wrap wrap, Filter filter)`

     Extend the `from_file` doc comment (hpp:52-61) with one line about `wrap` / `filter`. `solid()` keeps calling `from_rgba(1, 1, rgba)` (defaults).
  4. Source: implement `texture_sampler_params`, which only reads the `GL_*` macros from `glad.h`. In `upload_premultiplied`, replace cpp:105-109 with the four `glTexParameteri` calls fed from `texture_sampler_params(mipmaps, wrap, filter)` (cast to `GLint`). Pass `wrap` / `filter` from `from_rgba` (cpp:133) and `from_file` (cpp:205). The hardening order in `from_file` (empty path → stat → size cap → header probe → GL check → decode) stays exactly as is.
- **Mirror**: `src/render/texture.hpp:24-27` (pure helper doc style), `src/render/texture.cpp:87-116`
- **Validate**: `cmake --build build -j$(nproc)` (no warnings: watch `-Wextra` on unused parameters and enum switches, and give every `switch` all cases with no `default` so `-Wswitch` stays useful)

### Task 4: New `gl_quad_renderer_test`

- **File**: `tests/gl_quad_renderer_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Action**: CREATE / UPDATE
- **Implement**: Use the `TEST_CHECK` macro, one `std::cout << "  - ... ok.\n"` per case, and an explicit call list in `main`. Cases:
  1. `test_corner_order`: corners TL(10,20), TR(110,25), BR(105,80), BL(5,75) (a non-rect). Assert the 6 positions are exactly `[TL, TR, BR, TL, BR, BL]` (`==`, no math involved).
  2. `test_per_vertex_colour_premultiplied`: four distinct colours with alpha < 1, e.g. `{1,0,0,0.5}`, `{0,1,0,1}`, `{0,0,1,0.25}`, `{1,1,1,0}`. Each output vertex's rgba must equal `premultiply(colours[corner])` for its corner (including the duplicated TL/BR), and the fully transparent white corner must become `{0,0,0,0}` (#59).
  3. `test_uv_mapping_repeat_range`: `UVRect{0.0f, 0.0f, 4.0f, 2.5f}` maps TL=(0,0), TR=(4,0), BR=(4,2.5), BL=(0,2.5) and is **not clamped** (u > 1 is kept). Also check a negative/offset range `{-0.5f, 0.25f, 3.5f, 8.25f}` (scrolling scanlines) round-trips exactly.
  4. `test_parallelogram_skew`: build corners for a rect `{x=100, y=50, w=200, h=30}` sheared by `theme::skew::kRows` (TL.x = x + h*skew, BL.x = x, …; include `render/theme.hpp`). Assert that TL→TR and BL→BR are parallel and equal in length, and that the vertex positions equal the inputs. This documents the intended consumer usage.
  5. `test_layout_parity_with_append_quad`: a local `reference_rect_vertices(rect, uv, color)` copies **only the axis-aligned part** of `append_quad` (gl_quad_renderer.cpp:210-226 and 238-243). Compare it field-by-field with `==` against `quad_vertices({TL,TR,BR,BL of rect}, uv, {c,c,c,c})` for a few rects, UVs and colours. Use `std::memcmp` on the arrays or compare per field. This pins the two paths to the same layout, triangle order and premultiply. There is no rotation, so no FMA sensitivity.
  6. `test_headless_noop`: an uninitialized `GlQuadRenderer` gets `begin(1280, 720)`, `draw_quad_points(...)` with an invalid `Texture{}`, then with a default-constructed texture and `set_blend_mode(BlendMode::Add)`, then `end()`. It must not crash, and `!renderer.is_initialized()` holds.

  Also add to `tests/CMakeLists.txt` a block mirroring `texture_test` (lines 257-265): `add_executable(gl_quad_renderer_test gl_quad_renderer_test.cpp)`, link `blaze4k_core`, and `add_test(NAME gl_quad_renderer_test COMMAND gl_quad_renderer_test)`.
- **Mirror**: `tests/texture_test.cpp:13-21, 151-158, 196-207`; `tests/background_test.cpp:71-73` (headless renderer); `tests/CMakeLists.txt:257-265`
- **Validate**: `cmake --build build -j$(nproc) && ./build/tests/gl_quad_renderer_test`

### Task 5: Extend `texture_test`

- **File**: `tests/texture_test.cpp`
- **Action**: UPDATE
- **Implement**: `#include <glad/glad.h>` (macros only, no GL calls). New cases, each added to `main`:
  1. `test_sampler_params_defaults_unchanged`: `texture_sampler_params(false, Clamp, Linear)` gives `{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE}` and `(true, Clamp, Linear)` gives `{GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE}`. Together these equal the old hard-coded state exactly (the "existing draws unchanged" AC on the texture side).
  2. `test_sampler_params_repeat_nearest`: `(false, Repeat, Nearest)` gives `{GL_NEAREST, GL_NEAREST, GL_REPEAT}` (the scanlines config). `(false, Repeat, Linear)` gives `{GL_LINEAR, GL_LINEAR, GL_REPEAT}` (life_stripes). `(true, Clamp, Nearest)` gives `{GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE}`.
  3. `test_wrap_overloads_headless`: `Texture::from_file("", false, Texture::Wrap::Repeat, Texture::Filter::Nearest)` is invalid. `from_file` on the committed `scanlines.png` with Repeat/Nearest is invalid headless but must not crash. Locate it via a `BLAZE4K_ASSETS_DIR` compile definition like `background_test` (`tests/CMakeLists.txt:274-279`). If that is not worth the CMake edit, use only the empty path and the missing-file path. `from_rgba(1, 1, px, false, Repeat, Nearest)` is invalid headless.
- **Mirror**: `tests/texture_test.cpp:98-104` (`test_missing_file_and_empty_path`)
- **Validate**: `cmake --build build -j$(nproc) && ./build/tests/texture_test`

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from the touched files
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Debug build too (asserts / different inlining)
cmake --build build-debug -j$(nproc) 2>&1 | grep -iE "warning|error" ; echo "debug build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net \
  ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 43

# Static checks for the "existing output unchanged" AC
git diff -U0 src/render/gl_quad_renderer.cpp | grep -E '^[-]' | grep -v '^---'   # expect: no removed lines
git grep -n "from_file(\|from_rgba(" -- src | grep -v "src/render/texture"       # expect: same 4 callers as before, unmodified
```

## End-to-End Verification

1. **Pure-path proof (automated).** `gl_quad_renderer_test` covers corner order, per-vertex premultiplied colour, UV ranges above 1 left unclamped, skew corners and parity with `append_quad`'s layout. `texture_test` proves the default sampler params equal the old constants and that Repeat/Nearest map to `GL_REPEAT` / `GL_NEAREST`. Both run under the sandboxed ctest command.
2. **Existing output unchanged.** `git diff` shows only additions in `gl_quad_renderer.cpp`, with no line removed or changed in `append_quad`, `draw_quad` or `draw_textured_quad`. In `texture.cpp`, the only changed lines are the `glTexParameteri` block and the parameter plumbing. All 42 existing tests still pass, including `note_field_renderer_test`, `hud_renderer_test`, `bitmap_font_test` and `perf_loop_test`, which drive the headless renderer through every screen's `render`.
3. **Headless safety.** `test_headless_noop` plus the existing `perf_loop_test` headless render pass.
4. **Optional live GL check (owner only; the agent must not launch the GUI, because it opens a window and plays audio).** Temporarily add a debug draw on the title screen: `draw_quad_points` with a `theme::skew::kRows` parallelogram and a red-to-blue vertical gradient, plus a full-screen `scanlines.png` loaded with `Repeat` + `Nearest` and `UVRect{0, 0, 1, h/3.0f}`. Check that the shape is slanted, the gradient is smooth, and the scanlines are crisp 1-px rows with no smear at the edges. Then remove the debug draw. The real consumers arrive in #89 and #95.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Refactoring `append_quad` through the new helper changes existing output by about 1 ulp on FMA targets (probe: 1133/12000 mismatches with `-march=native`; ARM64 contracts by default) | Leave `append_quad`, `draw_quad` and `draw_textured_quad` untouched. The new path is additive, and a parity test (axis-aligned, no rotation) pins the shared layout contract | In scope |
| Moving `Vertex` to namespace-scope `QuadVertex` changes the layout or stride and breaks the attribute pointers | Same 8 floats in the same order. `using Vertex = QuadVertex;` keeps `sizeof(Vertex)` / offsets in `init()` and `flush()` identical. Optionally add `static_assert(sizeof(QuadVertex) == 8 * sizeof(float))` | In scope |
| Triangle-split interpolation: a quad with 4 distinct colours, or a textured non-parallelogram (trapezoid), shows the TL–BR diagonal | Document it in the `draw_quad_points` comment. Every planned use (parallelograms, shears, linear 2-colour gradients) is exact under affine interpolation. Bilinear or projective correction would need a shader, and the issue says "no new shaders" | Out of scope (documented) |
| Wrong corner order from a caller (bow-tie or back-facing) | The contract is documented (TL, TR, BR, BL). GL face culling is not enabled (`begin()` only disables depth), so a clockwise or counter-clockwise mix-up still draws. The theme's own helpers in #89/#90 will build corners in one place | Out of scope |
| `Nearest` + mipmaps semantics are not specified by the issue | Choose `GL_NEAREST_MIPMAP_NEAREST` and pin it in a test. No current or planned asset uses Nearest with mipmaps | In scope (see Open Questions) |
| Repeat on a mipmapped or NPOT texture | GL 3.3 core fully supports NPOT + `GL_REPEAT` + mipmaps. `scanlines.png` (1×3) is loaded without mipmaps | In scope (no action) |
| Repeat-wrapped textures sampled at `[0,1]` edges with Linear filter bleed the opposite edge in | This is the intended tiling behaviour. Clamp stays the default, so no existing texture changes | Out of scope |
| A new GL include in `texture_test.cpp` adds an unwanted dependency | `glad` is already a PUBLIC dependency of `blaze4k_core`, and only macros are used (no GL calls in a headless test) | In scope (no action) |

---

## Open Questions

- **Enum spelling and placement.** The issue says `Wrap::Repeat`. **Proposed default:** nested `Texture::Wrap` / `Texture::Filter`, so call sites read `Texture::Wrap::Repeat` and `Texture` stays the single owner of texture options. Non-blocking.
- **Separate `filter` parameter versus a combined options struct.** The issue says `from_file(path, mipmaps, wrap)` "plus a nearest-filter option". **Proposed default:** a fourth defaulted parameter `Filter filter = Filter::Linear`. That is the smallest API change, and every existing call stays the same. A `TextureOptions` struct would be cleaner if more options appear, but nothing needs one now. Non-blocking.
- **`from_rgba` gets the same options?** The issue names only `from_file`. **Proposed default: yes**, defaulted, because it costs nothing (shared `upload_premultiplied`) and lets procedural or test textures tile. Non-blocking.
- **Nearest with mipmaps.** **Proposed default:** `GL_NEAREST_MIPMAP_NEAREST`, the crispest choice and consistent with "nearest". No asset uses this combination today. Non-blocking.
- **Single-colour convenience overload of `draw_quad_points`.** It is not requested. **Proposed default: no.** Callers pass `{c, c, c, c}`. #89/#90 can add one if it turns out to be noisy. Non-blocking.

---

## Acceptance Criteria

- [ ] `GlQuadRenderer::draw_quad_points(corners, texture, uv, colours)` draws a TL, TR, BR, BL quad with per-corner colours through the existing batch and texture-change flush
- [ ] `Texture::from_file(path, mipmaps, wrap, filter)` accepts `Wrap::Repeat` and `Filter::Nearest`, with the defaults left at clamp + linear
- [ ] Existing draw calls are unchanged: `append_quad`, `draw_quad` and `draw_textured_quad` are not modified, the default sampler params equal the old constants (tested), and all 42 existing tests pass
- [ ] Unit tests cover corner order, premultiplied per-vertex colour, and UV mapping for a repeat range above 1
- [ ] An uninitialized renderer's `draw_quad_points` is a safe no-op (tested)
- [ ] Build has no new warnings (Release + Debug), and sandboxed `ctest` passes 43/43
- [ ] `TODO.md` and `.agents/stories/todo-stories.md` are untouched
- [ ] Follows existing patterns
