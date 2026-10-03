# Plan: TrueType Text Rendering with stb_truetype Atlases (#90)

## Summary

The Cabinet screens (#92 to #95) draw song titles, artists, meters, hints and the combo in Saira Condensed with tracking, synthetic italic and hard drop shadows (`theme::TextStyle`). This change adds one module, `src/render/ttf_font.{hpp,cpp}`, plus one stb implementation TU. It has four layers:

1. **`FontFace`** holds the bytes of one `.ttf`. It validates the sfnt table directory, then wraps `stbtt_fontinfo`. It caches the glyph index and advance of each of the 319 baked code points, and answers kerning queries. A missing, truncated or garbage file is rejected with an error string and is never handed to stb.
2. **`FontAtlas`** is one per (font, size). It bakes printable ASCII, Latin-1 and Latin Extended-A with `stbtt_PackFontRanges` and 2x2 oversampling, then packs one extra rect for the placeholder box. The coverage is uploaded as white RGBA with the coverage in alpha (`Texture::from_rgba`), so the existing premultiplied pipeline and tinting just work.
3. **Pure layout.** `measure_text` and `for_each_text_quad` turn UTF-8 plus a resolved layout (pixel size, tracking, shear, alignment, shadow) into sheared glyph quads in draw order. Tests run them against a CPU-baked atlas with no GL. `truncate_to_width` is the generic, measure-based `...` truncation. It lives in `unicode_text` next to the decoder, and `truncate_to_cells` becomes a thin wrapper over it.
4. **`TextRenderer`** owns the 4 faces and the atlases. It resolves a `TextStyle` at layout scale `s = window_height / 720` and draws through `GlQuadRenderer::draw_quad_points` (#88). Atlases are baked at `size_px * s` and re-baked only when the window height changes. All 13 (font, size) pairs used by `theme::text` are pre-baked up front, so no bake happens during a song. If a font is missing or corrupt, that font logs once and falls back to the 5x7 bitmap font. With no GL context (headless), measuring still works because it uses font metrics, and drawing is a no-op.

`App` owns the `TextRenderer`, the same way it owns `ThemeTextures` (#89). `App::init` loads the fonts, and `App::run` calls `set_window_height` before each render (a cheap compare). It is released before the GL context dies. `ScreenContext::text` exposes it to screens. No screen draws with it yet: the consumers are #92 to #95, and `bitmap_font` stays for the current screens and debug overlays (deleting it is #98 or later).

## User Story

As the developer building the Cabinet screens
I want to measure, truncate and draw UTF-8 text in a `theme::TextStyle` at any window size
So that titles, artists, meters, hints and the combo match the mock-ups and stay crisp at 720p to 4K, without crashing on hostile simfile text or a broken font install.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM (one new module plus small wiring edits; the logic is pure and testable headless) |
| Systems Affected | `src/render` (new `ttf_font`, `stb_truetype_impl.cpp`, `unicode_text` truncation, `bitmap_font` wrapper, `theme.hpp` style list), `src/app` (ownership, lifetime, per-frame height), `src/screens/screen.hpp` (context seam), `src/main.cpp` (wiring), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #90 (implements #55 / TODO-1; blocked by #87 and #88, both merged; blocks #92, #93, #94, #95) |
| Plan sequence | 042 (running plan sequence; the issue number is #90) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build with `cmake --build build -j$(nproc)`. If you need to reconfigure, add `-DFETCHCONTENT_BASE_DIR=/home/lauri/github/blaze4k/build/_deps` |
| C++ compiler | GCC 16.2.1 | C++20 with `-Wall -Wextra -Wpedantic` and no `-Werror`. Add **no new warnings** |
| Baseline tests | **44/44 pass** | Run on `main` @ `0d77032` with the sandboxed command in Validation. Start green and stay green. After this change the count is **45** (`ttf_font_test` added) |
| Sandbox requirement | — | `audio_test` opens the real audio device, so **always** run ctest through the `bwrap` command |
| stb | `build/_deps/stb-src` @ `2c980bb` | `stb_truetype.h` v1.26 and `stb_rect_pack.h` are present. The `stb` INTERFACE target is already linked to `blaze4k_core` (`CMakeLists.txt:59-64,75-78,141`), so the build needs no new dependency |
| stb warnings | scratch probe | The impl TU (`STB_RECT_PACK_IMPLEMENTATION` + `STB_TRUETYPE_IMPLEMENTATION`) compiles with **0 warnings** under `-Wall -Wextra -Wpedantic` |
| stb safety | `stb_truetype.h:6` | "NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES". The fonts are bundled, but the AC requires a corrupt `.ttf` not to crash, so validate the sfnt directory **before** `stbtt_InitFont` |
| Font files | `assets/fonts/*.ttf` (70 to 97 KB) | All four are TrueType: sfnt version `00 01 00 00`, with `glyf`, `loca`, `cmap`, `head`, `hhea`, `hmtx`, `maxp`, `GPOS`. Audiowide also has a `kern` table |
| Glyph coverage | scratch probe | Saira Medium, Bold and ExtraBold cover **all** 319 baked code points. Audiowide lacks **1** in Latin Extended-A. **No font has U+FFFD**, so malformed bytes always draw the placeholder box |
| Units per em | scratch probe | Saira is 1000 (hhea ascent 1135, descent −439, lineGap 0, cap height 688). Audiowide is 2048 (cap height 1434). `ScaleForPixelHeight(44)` = 0.02795, but `ScaleForMappingEmToPixels(44)` = 0.044, so the two differ by 1.57x. CSS `font-size` is the em size, so bake with `STBTT_POINT_SIZE(px)` (`stb_truetype.h:602,614-617`) |
| Kerning | scratch probe | Saira has GPOS pair kerning: 224 of 9025 ASCII pairs are non-zero, for example A/V = −33 units. `stbtt_GetGlyphKernAdvance` costs about **0.19 µs** per pair |
| Pairs used at runtime | `src/render/theme.hpp:110-142` | **13** unique (font, size) pairs, all Saira Bold or ExtraBold, sizes 18 to 44. Saira Medium and Audiowide are not used by any `TextStyle`, because Audiowide is baked into textures |
| Atlas size and bake time | scratch probe, 2x2 oversampling, em-size, power-of-two square or 2:1 | Total RGBA for all 13 pairs: **720p 24 MB** (max 1024², about 86 ms including retries). **1080p 54 MB**. **1440p 92 MB** (max 2048², 226 ms). **2160p 192 MB** (ExtraBold 40/44 need **4096x2048**, 551 ms). Cropping the atlas height to the rows actually used and sizing the first try from the glyph area cuts both. A 20 px test atlas bakes in about 7 ms |
| Window height | `src/app/window.cpp:123-126,154-158`, `app.cpp:150-151` | `Window::height()` is **pixels**: it comes from `SDL_GetWindowSizeInPixels` and is updated on `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`. That is the same value `quad_renderer.begin` uses (`main.cpp:392-399`) |
| GL availability check | `src/render/theme_textures.cpp:858-862`, `src/gameplay/noteskin.cpp:201` | `glad_glGenTextures == nullptr` means there is no context. Mirror it |
| App lifetime | `src/app/app.cpp:12-19` | `~App()` must release GL objects before `window_.shutdown()`. `theme_textures_.shutdown()` already does this at line 16; add the text renderer next to it |
| Layout scale helper (#91) | not merged | Compute `s` locally from the window height (a pure helper here). #91 can switch to the shared helper later |
| `truncate_to_cells` users | `src/screens/song_display_text.cpp:29` | `chart_display_label` (select and results screens, bitmap cells). It must keep working until #94 and #95 move those screens to TTF |
| TODO tracking | `TODO.md:16` | "Better general font at some point? (#55 → #90)". See Open Questions. `.agents/stories/todo-stories.md` has unrelated uncommitted owner edits and is **off-limits**: do not stage, revert or edit it |

---

## Patterns to Follow

### Single stb implementation TU

```cpp
// SOURCE: src/render/stb_image_impl.cpp:1-10
// Single translation unit that instantiates the vendored stb_image decoder.
// Every other file only includes <stb_image.h> for declarations.
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

### Asset path from the cwd or the executable dir

```cpp
// SOURCE: src/render/theme_textures.cpp:745-753
std::filesystem::path ThemeTextures::default_directory() {
    const std::filesystem::path relative =
        std::filesystem::path("assets") / "theme" / "cabinet" / "manifest.json";
    const std::filesystem::path manifest = resolve_first_existing({
        relative,
        default_executable_dir() / relative,
    });
    return manifest.empty() ? std::filesystem::path{} : manifest.parent_path();
}
```

### Load once, log once, check GL once, never throw

```cpp
// SOURCE: src/render/theme_textures.cpp:763-790, 858-862
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(manifest_path, ec);
    if (ec || size > kMaxManifestBytes) {
        std::cerr << "[ThemeTextures] Missing, unreadable or oversize manifest: " ...
        return false;
    }
    ...
    if (glad_glGenTextures == nullptr) {
        std::cerr << "[ThemeTextures] No GL context available; drawing flat fallbacks for " ...
        return true;
    }
```

Use the tag `[TextRenderer]` (or `[FontAtlas]` for bake failures). Write warnings to `std::cerr` and the one success summary to `std::cout`, for example `[TextRenderer] Baked 13 atlases for 1440p (N MB)`. Log-once flags are `mutable bool` or a small fixed array indexed by `Font`, so the steady state never allocates (`theme_textures.hpp:213-214`).

### Log-once draw guard

```cpp
// SOURCE: src/render/theme_textures.cpp:697-705
    if (!font_present_) {
        if (!warned_missing_) {
            warned_missing_ = true;
            std::cerr << "[ThemeTextures] Digit font '" << name_ << "' unavailable; digits are not drawn\n";
        }
        return;
    }
```

### Alignment and pure pen math (mirror the naming)

```cpp
// SOURCE: src/render/theme_textures.hpp:216-226
enum class DigitAlign { Left, Centre, Right };
[[nodiscard]] float digits_start_x(float x, float width, DigitAlign align);
```

Name the new enum `TextAlign { Left, Centre, Right }` (British "Centre", as in `DigitAlign`).

### General quad (italic shear)

```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:72-84
    void draw_quad_points(const std::array<Vec2, 4>& corners, const Texture& texture,
                          const UVRect& uv, const std::array<Color, 4>& colours);   // TL, TR, BR, BL
```

### UTF-8 resolution order (keep today's rules)

```cpp
// SOURCE: src/render/bitmap_font.cpp:157-173
// Resolution order: zero-width -> nothing (no cell); native glyph; ASCII fold; placeholder box.
```

### Context seam and ownership

```cpp
// SOURCE: src/screens/screen.hpp:22,65-67 ; src/app/app.hpp:40-41,67 ; src/main.cpp:367
class ThemeTextures;
    const ThemeTextures* theme = nullptr;   // owned by App; null in headless/unit tests
...
        shell->context().theme = &app.theme_textures();
```

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, stderr capture)

```cpp
// SOURCE: tests/bitmap_font_test.cpp:17-25 ; tests/theme_textures_test.cpp:90-101 (CerrCapture) ;
//         tests/CMakeLists.txt:444-458 (compile definition for an asset path)
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ \
    << ":" << __LINE__ << ": " << #expr << "\n"; std::abort(); } } while (0)
```

For a headless renderer, call draws on an **uninitialized** `GlQuadRenderer`; they are no-ops (`tests/background_test.cpp:71-80`). Put temp fixtures (corrupt `.ttf` files) under `fs::temp_directory_path() / "blaze4k_ttf_font_test"`.

---

## Pinned Semantics (the contract the tests pin)

Notation:

- `h` is the window height in pixels. `s = h / 720`; when `h <= 0`, `s = 1`.
- `P` is the bake pixel size (the em size): `P = size_px * s`.
- `k_em = stbtt_ScaleForMappingEmToPixels(P)`, which is font units to pixels.
- "Visible" means not `is_zero_width`.

| Topic | Rule |
|---|---|
| Baked set | Three `stbtt_pack_range`s: U+0020–U+007E (95), U+00A0–U+00FF (96) and U+0100–U+017F (128), so **319** code points. `font_size = STBTT_POINT_SIZE(P)`. Then `stbtt_PackSetOversampling(&pc, 2, 2)`, `stbtt_PackSetSkipMissingCodepoints(&pc, 1)` and padding 1. Code points whose `stbtt_FindGlyphIndex` is 0 are "absent" |
| Placeholder | After `stbtt_PackFontRanges`, pack one extra rect with `stbtt_PackFontRangesPackRects(&pc, &rect, 1)`. It is not oversampled and its quad maps 1:1. Draw a hollow box into it: ink from x 0.08em to 0.52em, and from y = baseline − cap height to the baseline. Cap height comes from the box of `'H'`, or 0.7em when the font has no `'H'`. The stroke is `max(1, round(0.06·P))` px. Its advance is 0.6em |
| Atlas size | The first width is the smallest power of two ≥ √(estimated glyph area × 1.2), clamped to [256, cap]. The area is estimated from `stbtt_GetGlyphBitmapBox` at 2·k_em. The height is `cap` while packing, then cropped to the largest used `y1` + 1, rounded up to a multiple of 4. On failure, double the width up to `cap`. If it still fails at the cap, retry once at 1x1 oversampling. If that fails too, the atlas is unavailable: log once and that style uses the bitmap fallback. `cap = min(4096, GL_MAX_TEXTURE_SIZE)`, queried once when GL is present; CPU-only bakes in tests use 4096 |
| Upload | `coverage_to_white_rgba(coverage)` produces `(255, 255, 255, c)` per texel, then `Texture::from_rgba(w, h, rgba, /*mipmaps*/false, Clamp, Linear)`. That function premultiplies, so the RGB becomes `c` (`texture.hpp:59-64`). Free the CPU coverage after the upload |
| Glyph resolution | Zero-width: skip (no advance, no tracking, and the kerning predecessor is unchanged). Else a baked, present code point gives its glyph. Else, if `fold_to_ascii(cp)` is baked and present, use that glyph. Else the placeholder. U+FFFD (malformed bytes), C0/C1 controls and CJK all draw the placeholder. TAB folds to space |
| Advance | For each visible glyph, `pen += kern + advance_units·k_em + tracking`. `kern` is `stbtt_GetGlyphKernAdvance(prev, cur)·k_em`, and is 0 when there is no predecessor or either side is the placeholder. `tracking = tracking_px·s` after **every** visible glyph, including the last (CSS `letter-spacing`). `measure` = the final pen. The **same** function feeds measure and draw, so they cannot disagree |
| Vertical anchor | `y` is the **top of the line box** (CSS `line-height: normal`). `baseline = y + ascent·k_em`, using hhea ascent from `stbtt_GetFontVMetrics`. Expose `ascent(style)` and `line_height(style) = (ascent − descent + lineGap)·k_em` for layout |
| Alignment | The start pen is `x` (Left), `x − w/2` (Centre) or `x − w` (Right), where `w = measure` |
| Glyph quad | Take the quad from `stbtt_GetPackedQuad(..., align_to_integer = 0)` at pen (0, 0), then translate by `(pen, baseline)`. Corners are TL `(x0, y0)`, TR `(x1, y0)`, BR `(x1, y1)`, BL `(x0, y1)`. **Shear:** each corner gets `x += shear · (baseline − corner_y)`, where `shear = (italic ? theme::kItalicShear : 0) + extra_shear`. `extra_shear` is the group shear, for example `theme::text::kComboGroupShear`. Do not snap to the pixel grid: 2x oversampling handles subpixel pen positions |
| Shadow | `Hard2` and `Hard3`: pass 1 draws every glyph in `theme::color::kShadow`, with its alpha set to `style.color.a`, offset by `(0, +2·s)` or `(0, +3·s)`. The offset applies to the baseline too, so the shape is just translated. Pass 2 draws in `style.color`. `None` is a single pass. Both passes use the same atlas, so it is one batch |
| Measure ignores | The shear overhang and the shadow offset, as CSS boxes do |
| Truncation (`truncate_to_width(text, max_width, measure_fn)`) | `max_width` that is NaN or < 0 counts as 0. Compare with ε = 1e-3 px. (1) If `measure(text) ≤ max`, return the text unchanged, byte for byte. (2) Else, if `measure("...") ≤ max`, return the longest prefix P cut on a code-point boundary **before a visible code point** (zero-width marks stay attached to the last kept glyph) with `measure(P + "...") ≤ max`, plus ASCII `"..."`. (3) Else return the longest such P with `measure(P) ≤ max` and no ellipsis. It never rewrites malformed bytes, never throws, and always returns `measure(out) ≤ max` when the text was cut. `truncate_to_cells(t, n)` is exactly `truncate_to_width(t, 6n, visible_cps·6)`, so every existing assertion holds unchanged |
| Bitmap fallback (font missing or corrupt, or atlas bake failed) | `pixel = P / 10`, so the 7-row cap is about 0.7em. A visible code point advances `6·pixel + tracking`. Its rows come from `glyph_rows(cp)`, which already handles folds and the placeholder. The rows' top is `y + (1.2·P − 7·pixel)/2`. There is no italic and no kerning. Shadow and alignment still apply. Draw with `draw_quad` |
| Headless (no GL) | `load` reads and validates the fonts, so `measure`, `truncate` and `covers_text` work. `set_window_height` bakes **nothing** and logs one line once. `draw` returns early: no atlas, no fallback, nothing logged per draw |
| Re-bake | `set_window_height(h)`: if `h` equals the current height, return (an O(1) compare every frame). Otherwise drop every atlas and bake every unique (font, size_px) in `theme::text::kAllStyles`. A style not in the list bakes lazily on first use and logs one line, with at most 32 atlases. Past the cap, that style uses the bitmap fallback and logs once |
| Coverage | `covers_text(text, font)` is true when every visible code point is baked and present **natively** (folds and placeholders do not count). This is the TTF counterpart of `font_covers_text`, for #94's translit choice. It is not wired yet |
| Logging | At `load`: one line per missing or corrupt font, giving the path and reason, plus one `std::cout` summary. At bake: one line per failed atlas and one summary. At draw: at most one line per `Font` for "unavailable, using the bitmap fallback" |
| Timing | Presentation only. Nothing touches the music clock or the judgment path. Bakes happen at init and on a height change, never mid-song unless the window is resized |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/stb_truetype_impl.cpp` | CREATE | The only TU with `STB_RECT_PACK_IMPLEMENTATION` + `STB_TRUETYPE_IMPLEMENTATION` (rect_pack first, so stb_truetype uses the real packer) |
| `src/render/ttf_font.hpp` | CREATE | `TextAlign`, `validate_sfnt`, `FontFace`, `AtlasGlyph`, `FontAtlas`, `coverage_to_white_rgba`, `TextLayout`, `GlyphQuad`, `measure_text`, `for_each_text_quad`, `text_layout_scale`, `TextRenderer` |
| `src/render/ttf_font.cpp` | CREATE | Implementation |
| `src/render/unicode_text.hpp` / `.cpp` | UPDATE | Add `truncate_to_width(std::string_view, float max_width, const std::function<float(std::string_view)>& measure)`. Update the header comment ("never throws"; only truncation allocates its result) |
| `src/render/bitmap_font.hpp` / `.cpp` | UPDATE | `truncate_to_cells` becomes a wrapper over `truncate_to_width` with the 6-unit cell measure. The doc comment is unchanged, plus "see truncate_to_width" |
| `src/render/theme.hpp` | UPDATE | Add `inline constexpr std::array kAllStyles = {kFooter, kSongTitle, ..., kJudgmentCount};` at the end of `namespace text`, with a comment to keep it in sync. It holds all 26 styles and is used for pre-baking |
| `CMakeLists.txt` | UPDATE | Add `src/render/stb_truetype_impl.cpp` and `src/render/ttf_font.cpp` to `blaze4k_core`, after `src/render/theme_textures.cpp` (line 103) |
| `src/app/app.hpp` / `app.cpp` | UPDATE | Add a `TextRenderer text_renderer_` member and `TextRenderer& text_renderer()`. In `init()`, call `load()` after the theme load, then `set_window_height(window_.height())`. In `run()`, call `text_renderer_.set_window_height(window_.height())` just before `on_render(alpha)` (line 92). In `~App()`, call `text_renderer_.shutdown()` next to `theme_textures_.shutdown()` |
| `src/screens/screen.hpp` | UPDATE | Forward-declare `class TextRenderer;` and add `TextRenderer* text = nullptr;`. It is non-const because lazy bakes mutate it. Document it like `theme` (null in headless and unit tests; screens must null-check) |
| `src/main.cpp` | UPDATE | `shell->context().text = &app.text_renderer();` next to line 367 |
| `tests/ttf_font_test.cpp` | CREATE | sfnt validation, corrupt fonts, CPU atlas bake, layout (tracking, kerning, shear, shadow, alignment), UTF-8 rules, measure-based truncation with the real font, fuzz, headless `TextRenderer`, log-once |
| `tests/unicode_text_test.cpp` | UPDATE | **Port** the `truncate_to_cells` cases (`bitmap_font_test.cpp:190-263`) to `truncate_to_width` with a cell measure, plus a variable-width measure case |
| `tests/bitmap_font_test.cpp` | UPDATE | Shrink `test_truncate_to_cells` to a short wrapper smoke test (the `mDaWg` case and one budget below 3). The ported cases now live in `unicode_text_test` |
| `tests/CMakeLists.txt` | UPDATE | Register `ttf_font_test` with `BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}"`. `theme::kFontFiles` paths start with `assets/`, so the root is the source dir |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: stb implementation TU + CMake

- **File**: `src/render/stb_truetype_impl.cpp`, `CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**: Add a header comment in the style of `stb_image_impl.cpp`. Say that stb_truetype is not hardened, and that `FontFace` validates the sfnt directory before any stb call. Then `#define STB_RECT_PACK_IMPLEMENTATION`, `#include <stb_rect_pack.h>`, `#define STB_TRUETYPE_IMPLEMENTATION`, `#include <stb_truetype.h>`. Add the two `.cpp` files to `blaze4k_core` (`ttf_font.cpp` arrives in Task 3; add the TU line once it exists, or create an empty stub first).
- **Mirror**: `src/render/stb_image_impl.cpp:1-10`
- **Validate**: `cmake --build build -j$(nproc) 2>&1 | grep -iE "warning|error"` prints nothing

### Task 2: Generic measure-based truncation (+ port the tests)

- **Files**: `src/render/unicode_text.{hpp,cpp}`, `src/render/bitmap_font.{hpp,cpp}`, `tests/unicode_text_test.cpp`, `tests/bitmap_font_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `truncate_to_width` per Pinned Semantics.
    - Collect the candidate cut offsets: each byte offset where a visible code point starts, plus `text.size()`, using `next_code_point` and `is_zero_width`.
    - Find the longest valid prefix by binary search over the candidates. Width is monotonic in the prefix because a glyph's advance is larger than any kern. Then **verify** the result, and step back one candidate while the final string measures over `max`, so the invariant holds even for an odd font.
    - Reuse one `std::string` scratch for the `P + "..."` candidates.
  - `truncate_to_cells(text, n)` returns `truncate_to_width(text, 6.0f * n, cell_width)`, where `cell_width` counts visible code points × 6.
  - Port tests to `unicode_text_test`. Port **every** assertion from `bitmap_font_test.cpp:198-262` against `truncate_to_width` with the cell measure: fits unchanged; `"mDaWg & Hatena Zubon"` at 17 cells gives `"mDaWg & Hatena..."`; multibyte cuts; the combining mark stays attached; budgets 0 to 4; the 4096-input random-byte fuzz with no rewrites and no budget overrun.
  - Add a **variable-width** case: a measure where `'W'` is 3 units and everything else is 1. Check that the cut follows width, not count.
  - Add degenerate cases: NaN, negative and 0 `max_width`.
- **Mirror**: `src/render/bitmap_font.cpp:202-234` (cut-before-visible logic), `tests/bitmap_font_test.cpp:190-263`
- **Validate**: build, then the sandboxed ctest: `unicode_text_test` and `bitmap_font_test` pass, and the total is still 44/44

### Task 3: `ttf_font.hpp`: types and pure API

- **File**: `src/render/ttf_font.hpp`
- **Action**: CREATE
- **Implement** (namespace `blaze4k`; include `geometry.hpp`, `texture.hpp`, `theme.hpp`; forward-declare `GlQuadRenderer` and `struct stbtt_fontinfo`, or keep the fontinfo in a pImpl `std::unique_ptr` so stb stays out of the header):
  - `enum class TextAlign { Left, Centre, Right };`
  - `inline constexpr std::uintmax_t kMaxFontBytes = 16u << 20;` and `inline constexpr std::size_t kBakedGlyphCount = 319;`. Add `[[nodiscard]] int baked_glyph_slot(char32_t cp);`, which returns the index into the baked table, or −1.
  - `[[nodiscard]] bool validate_sfnt(std::span<const std::uint8_t> data, std::string* error);` (pure). It checks:
    - size ≥ 12 and ≤ `kMaxFontBytes`;
    - version `0x00010000` or `'true'` (reject `OTTO` and `ttcf`);
    - `1 ≤ numTables ≤ 64` and `12 + 16·numTables ≤ size`;
    - every table has `offset + length ≤ size` (64-bit math);
    - the required tables `cmap`, `head`, `hhea`, `hmtx`, `loca`, `glyf` and `maxp` are present;
    - `head.unitsPerEm` is in [16, 16384].
  - `class FontFace` (move-only):
    - `static std::optional<FontFace> from_bytes(std::vector<std::uint8_t>, std::string* error)` runs `validate_sfnt`, then `stbtt_InitFont != 0`, then requires `FindGlyphIndex('A') != 0`.
    - `static std::optional<FontFace> from_file(const std::filesystem::path&, std::string* error)` checks the size cap with `file_size` before reading.
    - Accessors: `units_per_em()`, `ascent/descent/line_gap()` in units, `em_scale(P)`, `glyph_index(slot)`, `advance_units(slot)`, `kern_units(g1, g2)`, `has(slot)`, `cap_height_units()`.
    - The bytes are stored in a `std::vector`, and `stbtt_fontinfo` points into them, so moves must keep the buffer address. A `std::unique_ptr<Impl>` holding both is the simplest way.
  - `struct AtlasGlyph { float x0, y0, x1, y1; UVRect uv; bool present; };` These are quad offsets relative to (pen, baseline), in px.
  - `struct FontAtlas` (move-only):
    - Fields: `float pixel_size`, `int width`, `int height`, `int oversample`, `std::vector<std::uint8_t> coverage`, `std::array<AtlasGlyph, kBakedGlyphCount> glyphs`, `AtlasGlyph placeholder`, `Texture texture`.
    - `static std::optional<FontAtlas> bake(const FontFace&, float pixel_size, int max_dim, std::string* error);` is CPU only. It returns nullopt for a pixel size that is not finite, < 1 or > 1024, or when nothing fits.
    - `bool upload();` uploads to GL and frees `coverage` on success.
  - `[[nodiscard]] std::vector<std::uint8_t> coverage_to_white_rgba(std::span<const std::uint8_t> coverage);`
  - `struct TextLayout { float pixel_size; float tracking; float shear; TextAlign align; Color color; float shadow_offset = 0; /* 0 = none */ };`
  - `struct GlyphQuad { std::array<Vec2, 4> corners; UVRect uv; Color color; };`
  - `[[nodiscard]] float measure_text(const FontFace&, std::string_view, float pixel_size, float tracking);`
  - `void for_each_text_quad(const FontFace&, const FontAtlas&, std::string_view, float x, float y, const TextLayout&, const std::function<void(const GlyphQuad&)>& emit);` This is pure: it emits shadow quads first, then colour quads, skipping zero-size glyphs (space).
  - `[[nodiscard]] float text_layout_scale(int window_height);` returns `h / 720`, or 1 when `h <= 0`.
  - `[[nodiscard]] TextLayout resolve_text_layout(const theme::TextStyle&, float s, TextAlign, float extra_shear);` (pure; maps a style to a layout per Pinned Semantics).
  - `class TextRenderer` (non-copyable):
    - Loading: `bool load();` (resolves each `theme::kFontFiles[i]` from the cwd, then the exe dir), `bool load(const std::filesystem::path& root);` (tests), `void shutdown();`, `void set_window_height(int h);`, `[[nodiscard]] float scale() const;`
    - Metrics: `float measure(std::string_view, const theme::TextStyle&);`, `float ascent(const theme::TextStyle&);`, `float line_height(const theme::TextStyle&);`
    - `std::string truncate(std::string_view, const theme::TextStyle&, float max_width);`
    - `bool covers_text(std::string_view, theme::Font) const;`
    - `void draw(GlQuadRenderer&, std::string_view, float x, float y, const theme::TextStyle&, TextAlign = TextAlign::Left, float extra_shear = 0.0f);`
    - Counters: `font_available(theme::Font)`, `atlas_count()`, `baked_height()`.
  - Doc comments state: the line-box-top `y`, straight-alpha colours, `s = window_height / 720` (#91), the fallback rule, that measure works headless, and that it is presentation-only.
- **Mirror**: `src/render/theme_textures.hpp` (pure functions beside a thin GL owner, documented conventions block at the top)
- **Validate**: compiles once Task 4 lands

### Task 4: `ttf_font.cpp`: face, atlas, layout

- **File**: `src/render/ttf_font.cpp`
- **Action**: CREATE
- **Implement**:
  - **Big-endian readers.** Bounds-checked `u16` and `u32` reads for `validate_sfnt`. Never read past `data.size()`.
  - **`FontFace::from_bytes`.** For each of the 319 slots, cache the glyph index (`stbtt_FindGlyphIndex`) and the advance (`stbtt_GetGlyphHMetrics`). Cache the V metrics and the cap height from `'H'`.
  - **`FontAtlas::bake`.** Estimate the area, then loop over widths:
    - `stbtt_PackBegin(&pc, buf, W, cap_h, 0, 1, nullptr)`, the oversampling and skip-missing settings, the three ranges, `stbtt_PackFontRanges`, then the placeholder rect via `stbtt_PackFontRangesPackRects(&pc, &rect, 1)`;
    - check `rect.was_packed`, then `stbtt_PackEnd`.
    - Then crop the height. Convert each `stbtt_packedchar` with `stbtt_GetPackedQuad(chars, W, H_cropped, i, &px, &py, &q, 0)` from the pen origin (0, 0) to an `AtlasGlyph`. Use the **cropped** height for the UVs.
    - Mark absent glyphs as `present = false`. Write the placeholder box pixels and its `AtlasGlyph`.
  - **`coverage_to_white_rgba`** (trivial).
  - **The shared glyph walker.** One internal function steps `next_code_point` and resolves zero-width, baked, folded or placeholder per Pinned Semantics, then yields `(slot or placeholder, kern, advance)`. Both `measure_text` and `for_each_text_quad` use it, so they cannot drift.
  - **`for_each_text_quad`.** It returns immediately for an empty text or non-finite `x`, `y` or pixel size. Its arithmetic is all in floats; skip any quad whose corners are not finite.
- **Mirror**: `src/render/bitmap_font.cpp:157-173,236-257` (resolution and the draw loop), `src/render/theme_textures.cpp:697-735` (pen loop)
- **Validate**: `cmake --build build -j$(nproc)` with no new warnings

### Task 5: `TextRenderer`

- **File**: `src/render/ttf_font.cpp`
- **Action**: UPDATE
- **Implement**:
  - **`load(root)`.** For each `Font`, call `FontFace::from_file(root / kFontFiles[i])`. A failure logs one line (`[TextRenderer] Font unavailable: <path> (<reason>); <Font> text uses the bitmap fallback`) and leaves that slot empty. Print one `std::cout` summary. Return true when at least one face loaded. `load()` with no argument resolves each file with `resolve_first_existing({rel, default_executable_dir() / rel})`. It is idempotent: call `shutdown()` first.
  - **`set_window_height(h)`.**
    - If `h` equals the baked height, return.
    - Check GL once (`glad_glGenTextures == nullptr`): if there is no GL, record the height, log once and return.
    - Otherwise query `GL_MAX_TEXTURE_SIZE` once, clear the atlases, and bake and upload each unique `(font, size_px)` from `theme::text::kAllStyles` at `size_px · s`.
    - Log a failed bake once. Print one summary line with the count, MB and ms.
    - Store atlases in a fixed-capacity `std::vector` reserved to 32, keyed by `(Font, size_px)`, and find them by linear search, so the steady state never allocates.
  - **`measure`, `ascent`, `line_height`.** Use the face metrics, or the bitmap-fallback model when the face is missing.
  - **`truncate`.** Calls `truncate_to_width` with a lambda over `measure`.
  - **`draw`.** If the face is missing (or its atlas bake failed), log once per `Font` **before** any renderer check, then draw the bitmap fallback (a no-op on an uninitialized renderer). This keeps the log-once rule testable headless. Otherwise, if headless (no atlas because there is no GL) or the renderer is not initialized, return. Otherwise call `for_each_text_quad`, which calls `renderer.draw_quad_points(q.corners, atlas.texture, q.uv, {c, c, c, c})`.
  - **Lazy bake for an unlisted size.** Only when GL is present; log once.
  - **`shutdown`.** Destroys every atlas texture and face. It is safe to call twice.
- **Mirror**: `src/render/theme_textures.cpp:755-890` (load, shutdown, summary)
- **Validate**: build

### Task 6: Style list, App ownership, context wiring

- **Files**: `src/render/theme.hpp`, `src/app/app.{hpp,cpp}`, `src/screens/screen.hpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**: as listed in Files to Change.
  - In `~App()`, the order is `stop()`, `text_renderer_.shutdown()`, `theme_textures_.shutdown()`, `window_.shutdown()`.
  - In `App::run`, call `set_window_height` before `on_render`. A height change from a resize is handled on the next frame.
  - `theme_test` must still compile `theme.hpp` standalone.
- **Mirror**: `src/app/app.cpp:12-19,47-49`, `src/screens/screen.hpp:63-67`, `src/main.cpp:367`
- **Validate**: build; `app_test` passes headless (one `[TextRenderer]` no-GL line, no crash)

### Task 7: `ttf_font_test`

- **Files**: `tests/ttf_font_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement** (load the faces from `BLAZE4K_SOURCE_DIR / theme::kFontFiles[i]`; bake CPU atlases at small sizes, 20 to 28 px, to keep the test fast):
  1. **`validate_sfnt`.**
     - The four real fonts pass.
     - These fail with a message: empty input, 11 bytes, random 4 KiB, `OTTO`/`ttcf` magic, `numTables` = 0 and 1000, a table offset past EOF (patch one directory entry), a missing `glyf` (rename its tag), and the real font truncated to 1 KiB and to 50%.
  2. **Corrupt or missing file.**
     - `FontFace::from_file` on a missing path, a 0-byte file, a garbage file, and the truncated real font: each returns nullopt with an error, and nothing crashes.
     - Copy `assets/fonts` to a temp root with `SairaCondensed-Bold.ttf` corrupted. Then `TextRenderer::load(temp_root)`: under `CerrCapture`, exactly **one** line names that font; `font_available(SairaBold) == false`; the other fonts are available.
     - `measure` with a `SairaBold` style returns the bitmap-fallback width (`visible·(6·P/10 + tracking)`).
     - Under `CerrCapture`, two `draw` calls with a `SairaBold` style on an uninitialized renderer log **exactly one** fallback line. Two draws with a `SairaExtraBold` style log nothing.
  3. **Atlas bake (CPU).** For Saira ExtraBold at 24 px:
     - `bake` succeeds; width and height are > 0 and ≤ 4096; the height is a multiple of 4.
     - Every present glyph's UV lies in [0, 1].
     - `'A'` and `'é'` (U+00E9) are present; `' '` has a zero-area quad and a positive advance.
     - The placeholder has a positive area and non-zero coverage along its edges.
     - `coverage_to_white_rgba({0, 128, 255})` gives `{255,255,255,0, 255,255,255,128, 255,255,255,255}`.
     - Audiowide at 24 px has exactly one absent Latin Extended-A slot.
     - A pixel size of 0, NaN or 5000 returns nullopt.
  4. **Measure.**
     - Empty text gives 0.
     - `measure("AV") < measure("A") + measure("V")` (kerning applied).
     - Tracking adds exactly `n_visible · tracking`.
     - `"é"` equals `"e"` (zero-width ignored).
     - An unknown CJK code point equals the placeholder advance (0.6em).
     - `"\xFF"` equals the placeholder (U+FFFD).
     - `"’"` (right single quote) equals `"'"` (fold).
     - `measure` scales linearly with `s` (720 vs 1440, within 1e-3 relative).
  5. **Layout** (`for_each_text_quad` collecting the `GlyphQuad`s):
     - The quad count equals the number of non-space visible glyphs.
     - Left, Centre and Right start x differ by exactly `w/2` and `w`.
     - Italic: for each quad, `TL.x − BL.x == shear · (BL.y − TL.y)`. An extra shear of `kComboGroupShear` adds to `kItalicShear`.
     - Upright: `TL.x == BL.x`.
     - `Shadow::Hard3`: the first half of the quads are in `kShadow` with `alpha == color.a`, and each is offset by `(0, 3·s)` from its partner in the second half.
     - `y` is the line top: the `'H'` quad bottom ≈ `y + ascent·k_em` (within 1 px).
  6. **Truncation with the real font.**
     - `truncate(text, style, measure(text))` returns the text unchanged.
     - For budgets from 0 to `measure(text)` in steps, check:
       - `measure(out) ≤ budget + 1e-3`;
       - the output ends with `"..."` whenever `measure("...") ≤ budget` and it was cut;
       - the prefix is a byte prefix of the input;
       - combining marks stay attached.
  7. **Fuzz.** 2000 random byte strings, each 0 to 64 bytes, seeded `std::mt19937{90}`, run through `measure`, `truncate` (random budget), `covers_text` and `for_each_text_quad` (random align, shear and shadow). Check: no crash, all quad corners finite, and the quad count ≤ 2 × input bytes.
  8. **Headless `TextRenderer`.**
     - `load(root)`, then `set_window_height(1440)` logs exactly one no-GL line under `CerrCapture`, and `atlas_count() == 0`.
     - Calling it again with the same height logs nothing.
     - `draw` on an uninitialized renderer is a no-op.
     - `measure` is positive.
     - `text_layout_scale(0) == 1` and `text_layout_scale(1440) == 2`.
  9. **`theme::text::kAllStyles`** has 26 entries and exactly 13 unique (font, size) pairs.
- **Mirror**: `tests/theme_textures_test.cpp` (CerrCapture, temp dirs), `tests/CMakeLists.txt:444-458` (asset compile definition)
- **Validate**: the sandboxed ctest shows **45/45**

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from the touched files
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"
# (If reconfigure is needed:)
# cmake -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=/home/lauri/github/blaze4k/build/_deps

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides the audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 45

# Static checks
git status --short                       # expect: only the files in "Files to Change" (+ the owner's todo-stories.md, untouched)
git diff --stat -- .agents/stories/todo-stories.md   # expect: no diff from this work
```

## End-to-End Verification

1. **Automated pure-path proof.** Under the sandboxed ctest command:
   - `ttf_font_test` pins sfnt validation, corrupt-font fallback, the CPU atlas bake against the real Saira and Audiowide files, tracking, kerning, shear, shadow and alignment, UTF-8 resolution (zero-width, U+FFFD, fold, placeholder), measure-based truncation and the fuzz run;
   - `unicode_text_test` pins the ported truncation contract;
   - `bitmap_font_test` pins the `truncate_to_cells` wrapper.
2. **Real app init (headless, safe for the agent).** Inside the same `bwrap` sandbox, from the repo root, run `./build/blaze-4k --headless --smoke-test 5 --data-dir "$(mktemp -d)"`. Expect:
   - one `[TextRenderer]` summary saying 4/4 fonts loaded;
   - exactly one no-GL line;
   - no crash, and a clean exit with "Blaze 4k shut down cleanly.".

   Repeat from `/tmp` so the fonts resolve through the exe dir (`build/assets/fonts`, from the POST_BUILD copy). Then repeat with a temp copy of `build/` whose `assets/fonts/SairaCondensed-Bold.ttf` is truncated. Expect one "Font unavailable" line and still a clean exit.
3. **Lifetime.** `app_test` passes (headless `App::init` and the destructor), and code review confirms `text_renderer_.shutdown()` precedes `window_.shutdown()` in `~App()`.
4. **Optional live GL check (owner only).** The agent must not launch the GUI, because it opens a window and plays audio.
   - Run `./build/blaze-4k` and expect `[TextRenderer] Baked 13 atlases for 720p (…)`.
   - For a visual check, temporarily add to `TitleScreen::render`, when `ctx.text` is set:
     - `draw("CAFÉ ÅNGSTRÖM – Ωmega", 48·s, 270·s, theme::text::kSongTitle)`
     - `draw("HARD 8", w/2, 400·s, kDiffMeter, TextAlign::Centre)`
     - `draw("123", w/2, 368·s, kComboNumber, Centre, kComboGroupShear)`
   - Expect crisp italic text, a 3px black shadow, a placeholder box for `Ω`, and an en dash folded to `-`.
   - Resize the window and expect one re-bake summary line. Compare with `docs/cabinet-theme/reference/cabinet-v3-select.png`.
   - Remove the debug draw; the real consumers are #92 to #95.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| stb_truetype is not hardened, and a corrupt `.ttf` crashes or asserts | `validate_sfnt` checks the magic, table count, every table extent and the required tables before `stbtt_InitFont`. There is a 16 MiB size cap, and the init result plus `'A'` lookup are sanity-checked. Fonts are bundled, not user-supplied. A deliberately crafted font that passes the directory check can still hit stb bugs: that is accepted and documented, since fonts are not untrusted input like simfiles | In scope (validation); out of scope (full hardening) |
| GPU memory: RGBA atlases are about 92 MB at 1440p and 192 MB at 4K (probe) | Crop the height to the used rows, size the first try from the glyph area, bake only the 13 used pairs, and free the CPU coverage after upload. An R8 atlas with a swizzle would cut 4x, but it needs a new `Texture` format and conflicts with the AC's "white RGBA"; leave it as a #98 follow-up if VRAM matters | In scope (crop); out of scope (R8) |
| Bake time on startup and on resize (about 0.15 s at 1440p, 0.4 s at 4K) | Bake at init and on a height change only, never per frame. A resize during a song stalls rendering for one frame. The music clock is audio-driven and input is timestamped, so judgments are unaffected, and `max_frame_dt` caps the update catch-up | In scope (documented) |
| A first-use bake mid-song causes a hitch | Pre-bake every pair in `theme::text::kAllStyles`. Unlisted sizes log a line, so missing list entries are visible | In scope |
| `kAllStyles` drifts from the styles in `theme.hpp` | The test pins 26 styles and 13 pairs, and the comment says to keep the list in sync | In scope |
| The em-size vs pixel-height mix-up makes text 1.57x too small | Pinned to `STBTT_POINT_SIZE` and `ScaleForMappingEmToPixels`, and the test checks the `'H'` quad height ≈ cap height × k_em | In scope |
| Measure and draw drift (truncation passes, but drawn text overflows) | A single shared glyph walker feeds both, and the layout test compares the last quad's pen with `measure` | In scope |
| Kerning and shadow cost per frame | About 0.19 µs per pair (probe). Even the select screen (~400 glyphs, two passes) adds < 0.2 ms | In scope (no action) |
| A large `GL_MAX_TEXTURE_SIZE` or a huge window | Cap the atlas at `min(4096, GL max)`, retry at 1x1 oversampling, then fall back to the bitmap font for that style with one log line | In scope |
| The texture-switch flush between the fallback (white texture) and atlas text | Only in the broken-font path. Normal text from one atlas is one batch, and the placeholder is inside the atlas | In scope (no action) |
| A screen dereferences a null `ctx.text` in tests | Documented as null in headless and unit tests. #92 to #95 must null-check | Out of scope (documented) |
| `truncate_to_cells` behaviour changes during the refactor | It is now a wrapper, every old assertion is ported verbatim, and `chart_display_label` callers are untouched | In scope |

---

## Open Questions

- **Vertical anchor.** `theme::layout` gives "top" values (for example `kSongTitleTop = 270`), but the CSS line-height is not in the pack. **Proposed default:** `y` is the top of the line box under CSS `line-height: normal`, so `baseline = y + hhea ascent`. `ascent()` and `line_height()` are exposed so #92 to #95 can adjust per screen against the reference PNGs. Non-blocking.
- **Kerning.** The issue does not mention it. **Proposed default:** apply GPOS/`kern` pair kerning, because the mock-ups are browser renders and browsers kern by default. It is cheap (see Risks). Non-blocking. It is easy to switch off with one flag if the owner prefers a monospaced feel for numbers.
- **Trailing tracking.** **Proposed default:** follow CSS `letter-spacing`, which adds the space after the last glyph too and counts it in `measure`. Centred tracked labels therefore sit `tracking/2` left of true centre, as in the mock-ups. Non-blocking.
- **Shadow and tracking scale.** `TextStyle` values are "at 720p". **Proposed default:** multiply `tracking_px` and the 2 or 3 px shadow offset by `s`, like every other reference value. Non-blocking.
- **Fallback choice for a missing or corrupt `.ttf`.** The AC allows "bitmap font or no-op". **Proposed default:** use the bitmap 5x7 font scaled to about 0.7em cap height, so menus stay usable in a broken install. Measure stays consistent with the draw, so truncation keeps working. Non-blocking.
- **Where truncation lives, and whether to retarget `chart_display_label` now.** **Proposed default:** the generic `truncate_to_width` goes in `unicode_text` (the font-agnostic module whose header already names "#55"). `truncate_to_cells` stays as a wrapper while the bitmap screens use it. #94 and #95 switch `chart_display_label` and `song_display_title` to `TextRenderer::truncate` and `covers_text`. Non-blocking.
- **Owner and seam.** **Proposed default:** `App` owns `TextRenderer` (mirroring `ThemeTextures`), and `ScreenContext::text` is a **non-const** pointer because lazy bakes mutate it. Non-blocking.
- **`TODO.md:16` and issue #55.** **Proposed default:** leave the TODO line unticked and #55 open until the first screen shows TTF text (#92), because nothing user-visible changes in this PR. Mention "implements #55's engine part" in the PR body. Owner call. Non-blocking.

---

## Acceptance Criteria

- [ ] `FontAtlas` bakes U+0020–U+007E, U+00A0–U+00FF and U+0100–U+017F per (font file, pixel size) with `stbtt_PackFontRanges` and 2x2 oversampling, uploaded as white RGBA with the coverage in alpha
- [ ] `TextRenderer::measure` and `draw` apply tracking, `kItalicShear` plus an extra group shear, and `Shadow::Hard2/3` (a black copy offset down, then the colour), with Left, Centre and Right alignment
- [ ] UTF-8 goes through `unicode_text`: zero-width is skipped, malformed bytes become U+FFFD and draw the placeholder, folds apply, and missing code points draw the in-atlas placeholder box. The fuzz test with malformed input passes
- [ ] Atlases bake at `size_px * window_height / 720` and re-bake only when the height changes, with all 13 theme pairs pre-baked
- [ ] Measure-based `...` truncation (`truncate_to_width` / `TextRenderer::truncate`) replaces the cell logic, `truncate_to_cells` is a wrapper, and its tests are ported
- [ ] A missing or corrupt `.ttf` logs once and falls back to the bitmap font, without crashing. Headless logs one line, and every test keeps running
- [ ] `ScreenContext::text` is wired in `main.cpp`, and the atlases are released before the GL context is destroyed
- [ ] The build has no new warnings, and the sandboxed `ctest` passes 45/45
- [ ] `.agents/stories/todo-stories.md` is untouched
- [ ] Follows existing patterns
