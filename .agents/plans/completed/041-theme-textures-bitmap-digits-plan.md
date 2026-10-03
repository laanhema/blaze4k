# Plan: Theme Textures from the Manifest + Bitmap Digits (#89)

## Summary

Screens in the Cabinet theme (#92 to #95) need to draw baked textures by **name**, with no pixel offsets hard-coded in the screens. This change adds one module, `src/render/theme_textures.{hpp,cpp}`, with three layers:

1. **A pure, GL-free manifest parser.** `parse_theme_manifest(json_text, log)` turns `assets/theme/cabinet/manifest.json` into a validated `ThemeManifest`, which holds 63 `ThemeEntry` records and 2 `DigitFont` glyph tables. It never throws, caps sizes and counts, rejects path-like file names, bounds-checks every rect against the declared image size, and skips each bad entry with one log line.
2. **Pure placement math.** The functions map a screen-space **content box** (the manifest's `content_px`, not the padded image) to destination rects and UVs, one per kind: sprite placement, stretch, slice3, slice9, frame, tiling, cropped fill, and digit layout. All of the geometry the ACs ask to test lives here, so tests need no GL.
3. **`ThemeTextures`**, a thin owner of the textures. It loads every PNG once at app init through `Texture::from_file`, picking options per texture: mipmaps for `grade_*`, `judgment_*` and `logo`; `Wrap::Repeat` for `tile` kinds; `Filter::Nearest` for `scanlines`. It also exposes `draw_*` helpers that feed the pure math into `GlQuadRenderer`. A missing or unreadable PNG, or a missing manifest, logs **once** and the draw falls back to a flat quad. With no GL context (headless), nothing is uploaded, one line is logged, and every draw becomes a flat-quad call on the no-op renderer. **`BitmapDigits`** draws `0-9 . % /` and space from `digits_chrome` / `digits_white`, using each glyph's rect, `origin_x` and `advance`, with Left, Centre or Right alignment.

`App` owns the `ThemeTextures` instance and loads it in `App::init()` right after the judgment constants. It releases the textures **before** the GL context is destroyed. `main.cpp` publishes it to screens through a new `ScreenContext::theme` pointer, the same way `constants` reaches screens. No screen draws with it yet; the consumers are #92 to #95. The layout scale `s` (`window_height / 720`, #91) is a plain parameter, so this issue does not depend on #91.

## User Story

As the developer building the Cabinet screens
I want to draw any theme texture or digit string by name at a layout scale
So that the title, gameplay, select and score screens match the mock-ups without hard-coded pixel offsets, and still run when an asset is missing or there is no GPU.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM (one new module plus four small wiring edits; the logic is all pure math) |
| Systems Affected | `src/render` (new module), `src/app` (ownership + lifetime), `src/screens/screen.hpp` (context seam), `src/main.cpp` (wiring), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #89 (blocked by #87 and #88, both merged; blocks #92, #93, #94, #95) |
| Plan sequence | 041 (running plan sequence; the issue number is #89) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` already configured (Release, host GCC, `FETCHCONTENT_BASE_DIR=/home/lauri/github/blaze4k/build/_deps`). Build with `cmake --build build -j8` |
| C++ compiler | GCC 16.2.1 | C++20, with `-Wall -Wextra -Wpedantic` and no `-Werror`. Add **no new warnings** |
| Baseline tests | **43/43 pass** | On `main` @ `a2ec06c`, `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` printed "100% tests passed out of 43" (0.42 s). Start green, stay green. After this change: **44** (`theme_textures_test` added) |
| Sandbox requirement | — | Some tests open real audio hardware, so **always** run ctest through the `bwrap` command above |
| Manifest shape | `assets/theme/cabinet/manifest.json` (1482 lines) | Top level: `name`, `version`=1, `reference_resolution`=[1280,720], `texture_scale`=2, `about`, `textures` (63), `bitmap_fonts` (2). All rects are `[x, y, w, h]` in image px (logo `content_px` [80,80,1960,380] in a 2120×540 image ⇒ `layout_720p` [150,168,980,190] = content/2) |
| Kinds and their fields | manifest | `fullscreen` ×3, `sprite` ×37 (with `layout_720p`, `anchor`, and some with `tint`), `stretch_x` ×2, `stretch` ×2, `slice3` ×15 (`slice3_px{left,right}`, measured **from the image edges**, padding included: `diff_row_*_selected` has content x=52 and left=432), `slice9` ×1 (`slice9_px{top,right,bottom,left}`), `frame` ×1 (`hole_px`), `tile` ×2 |
| `scanlines` entry | manifest | **Has no `content_px`** (default to the full image) and has `"scale": "1x (screen pixels)"`: tile at 1 texel per screen pixel, independent of `s`. `life_stripes` tiles at the normal `s / texture_scale` |
| Tile art | PIL probe | `life_stripes.png` (16×24) and `scanlines.png` (1×3) have **uniform rows**, so tiling on both axes shows no seams. No per-axis option is needed |
| Digit atlases | PIL probe | `digits_white.png` is 1722×164 and `digits_chrome.png` is 2276×228. Every glyph has `origin_x`=28 and h = the full atlas height. The atlas `size_px` key is the **font size**, not the image size (`theme_test.cpp:139`), so atlas dims come from `probe_image_header` |
| PNG sizes | `du` | 5.7 MB on disk. The three 2560×1440 backgrounds are about 0.75 to 1 MB each, well under `kMaxImageBytes` (16 MiB) and `kMaxImageDimension` (4096) in `texture.cpp:17-18` |
| Existing manifest checks | `tests/theme_test.cpp:64-120` | Already checks that PNG headers equal `size_px` and that `content_px`/`hole_px`/slice insets fit the image. The new parser does the same checks at **runtime**, defensively |
| `Texture` options (#88) | `src/render/texture.hpp:46-50,77-78` | `Texture::Wrap::{Clamp,Repeat}`, `Texture::Filter::{Linear,Nearest}`, `from_file(path, mipmaps, wrap, filter)`. Headless `from_file` logs a line **per call** (`texture.cpp:198-201`), so `ThemeTextures` must check for GL **once** itself and skip uploads, or headless would print 65 lines |
| GL availability check | `src/gameplay/noteskin.cpp:200-203` | `glad_glGenTextures == nullptr` ⇒ no context. Mirror it |
| ScreenContext wiring | `src/main.cpp:360-375`, `src/screens/screen.hpp:35-63` | Services are raw pointers that default to null and are set on `shell->context()` in `main.cpp`. `constants` comes from `App` (`main.cpp:366`) |
| App lifetime hazard | `src/app/app.cpp:12-16` | `~App()` calls `window_.shutdown()` (which destroys the GL context, `window.cpp:136-140`) **before** members are destroyed. An `App`-owned `ThemeTextures` must therefore be `shutdown()` explicitly at the top of `~App()` |
| Layout scale | grep `src` | There is no helper yet (#91). Take `s` as a parameter |
| Cabinet doc | `docs/cabinet-theme/IMPLEMENTATION_PLAN.md:57-72`, `docs/cabinet-theme/README.md:34-52` | Source of the helper list and kind semantics. The issue's `Source:` path (`blaze4k-cabinet-theme/…`) is now at `docs/cabinet-theme/` |
| TODO tracking | `TODO.md:17` | This is the Cabinet umbrella entry (#87 to #98): **do not tick it**. `.agents/stories/todo-stories.md` has unrelated uncommitted owner edits and is **off-limits** (do not stage, revert or edit it) |

---

## Patterns to Follow

### Asset load with log-and-fall-back (the "fallback spirit" the issue cites)

```cpp
// SOURCE: src/gameplay/noteskin.cpp:196-212, 250-261
bool NoteSkin::init(const std::filesystem::path& cel_directory) {
    ...
    if (glad_glGenTextures == nullptr) {
        std::cerr << "[NoteSkin] No GL context available; noteskin disabled\n";
        return false;
    }
    ...
}
void NoteSkin::load_cel_explosions(const std::filesystem::path& directory) {
    const auto load = [&](Texture& texture, const char* file) {
        texture = Texture::from_file((directory / file).string(), true);
        if (!texture.valid()) {
            std::cerr << "[NoteSkin] Missing or invalid Cel explosion: " << file
                      << " (that explosion is disabled)\n";
        }
    };
```

### Locating an asset directory from cwd or the executable

```cpp
// SOURCE: src/gameplay/noteskin.cpp:186-194
std::filesystem::path NoteSkin::default_directory() {
    const std::filesystem::path relative =
        std::filesystem::path("assets") / "noteskins" / "cel" / kCelTapFile;
    const std::filesystem::path tap = resolve_first_existing({
        relative,
        default_executable_dir() / relative,
    });
    return tap.empty() ? std::filesystem::path{} : tap.parent_path();
}
```

### Defensive JSON loading (size cap, non-throwing parse, message out-param)

```cpp
// SOURCE: src/data/judgment_constants_loader.cpp:24, 161, 175, 222
constexpr std::uintmax_t kMaxConfigBytes = 1u << 20; // 1 MiB cap for untrusted config
...
    if (file_size > kMaxConfigBytes) {
...
    json document = json::parse(buffer.str(), nullptr, false);   // discarded on error, no throw
...
    } catch (const std::exception& ex) {                          // type errors from .get<T>()
```

### Pure, GL-free helpers next to the GL class, documented as pure

```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:24-33 (quad_vertices), src/render/texture.hpp:96-105 (texture_sampler_params)
// ... Pure (no GL).
[[nodiscard]] std::array<QuadVertex, 6> quad_vertices(const std::array<Vec2, 4>& corners,
                                                      const UVRect& uv,
                                                      const std::array<Color, 4>& colours);
```

### Context seam: a defaulted, forward-declared pointer, wired in main

```cpp
// SOURCE: src/screens/screen.hpp:17-22, 59-62 ; src/main.cpp:366, 374-375
class IUiSoundSink;                    // forward declaration keeps screens free of GL headers
...
    IUiSoundSink* ui_sounds = nullptr; // null in headless/unit tests
...
        shell->context().constants = &app.judgment_constants();
        shell->context().ui_sounds = &ui_sounds;
```

### Error/log style

`std::cerr << "[ThemeTextures] ..."` for warnings and `std::cout << "[ThemeTextures] Loaded N/M ..."` for the one success summary. The tag style is `[NoteSkin]`, `[Texture]`, `[JudgmentConstants]`. Never throw out of the module.

### Tests (plain executable, `TEST_CHECK` abort macro, one print line per case, explicit call list in `main`)

```cpp
// SOURCE: tests/theme_test.cpp:13-20, 33-37, 190-200 ; tests/CMakeLists.txt:444-458
#define TEST_CHECK(expr) \
    do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
         << ": " << #expr << "\n"; std::abort(); } } while (0)
const fs::path kAssets{BLAZE4K_ASSETS_DIR};
...
add_executable(theme_test theme_test.cpp)
target_link_libraries(theme_test PRIVATE blaze4k_core)
target_compile_definitions(theme_test PRIVATE BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME theme_test COMMAND theme_test)
```

Headless renderer: build an **uninitialized** `GlQuadRenderer` and call draws as no-ops (`tests/background_test.cpp:71-80`). Temp fixtures go under `fs::temp_directory_path() / "blaze4k_theme_textures_test"` (`tests/assist_tick_test.cpp:81`).

---

## Pinned Semantics (the contract the tests pin)

Notation: `T` = manifest `texture_scale` (2). `s` = layout scale (`window_height / 720`). `k = s / T` = screen px per image px. `C = content_px`, `I = (0, 0, size_px)` = the full image. All screen rects have a top-left origin.

| Helper | Input | Geometry |
|---|---|---|
| `draw_sprite(r, name, content_pos, s, tint)` | content box top-left on screen | content rect = `{pos, C.w·k, C.h·k}`. Image quad = `{pos.x − C.x·k, pos.y − C.y·k, I.w·k, I.h·k}`, UV = full `[0,1]` |
| `draw_stretch(r, name, content_rect, tint)` (`fullscreen`, `stretch`) | content rect | per-axis `kx = rect.w / C.w`, `ky = rect.h / C.h`. The image quad expands the rect by the padding × (kx, ky). Fullscreen has `C = I`, so it just fills the rect |
| `draw_stretch_x(r, name, x, y, width, s, tint)` (`stretch_x`) | top-left + width | content rect = `{x, y, width, C.h·k}`, then the same as `draw_stretch` (never stretched vertically) |
| `draw_slice3(r, name, content_rect, tint)` | content rect | uniform `k = rect.h / C.h` (caps keep their aspect, so slanted ends keep their angle). Image rect = rect expanded by the padding × k. Caps: `left·k` and `right·k` wide, UV u `[0, left/I.w]` and `[(I.w−right)/I.w, 1]`. Middle fills the rest. If the image rect is narrower than the caps, both caps shrink by `image.w / ((left+right)·k)` and the middle gets zero width |
| `draw_slice9(r, name, content_rect, s, tint)` | content rect | border insets × k (`k = s/T`). Image rect = rect expanded by the padding × k. 9 pieces with matching UVs. If the rect is smaller than the borders on an axis, that axis's two borders shrink proportionally and the centre gets zero size |
| `draw_frame(r, name, hole_rect, tint)` | where the banner sits | per-axis `kx = hole.w / H.w`, `ky = hole.h / H.h` (`H = hole_px`). Image quad = `{hole.x − H.x·kx, hole.y − H.y·ky, I.w·kx, I.h·ky}` |
| `draw_tiled(r, name, rect, s, tint, anchor)` | area to cover | tile = `I.w·k × I.h·k`, or `I.w × I.h` screen px for the `"1x (screen pixels)"` entry (`scanlines`). `u ∈ [0, rect.w/tile_w]`. `TileAnchor::TopLeft`: `v ∈ [0, rect.h/tile_h]`. `TileAnchor::Bottom`: `v1 = 1`, `v0 = 1 − rect.h/tile_h` (a tile boundary sits on the rect bottom, as `life_stripes` needs: "aligned to the track bottom") |
| `draw_fill_cropped(r, name, bar_rect, fraction, tint)` | full bar rect | `f = clamp(fraction, 0, 1)`, and NaN counts as 0. The quad is bottom-anchored: `{bar.x, bar.y + bar.h·(1−f), bar.w, bar.h·f}`. UV `v ∈ [1−f, 1]` within the content box's UV span, so the gradient stays fixed to the bar while the fill height changes. `f == 0` draws nothing |
| `BitmapDigits::measure(text, s)` | — | sum of `advance · k` over supported glyphs. Unsupported bytes take zero width (logged once per font) |
| `BitmapDigits::draw(r, text, x, y, s, align, tint)` | anchor x, glyph top y | start pen = `x` (Left), `x − w/2` (Centre), `x − w` (Right), with `w = measure`. Each glyph quad = `{pen − origin_x·k, y, g.w·k, g.h·k}`, UV = glyph rect / atlas dims, then `pen += advance·k` (manifest note: "Draw each glyph at (pen_x - origin_x, y), then pen_x += advance") |

**Load options** (`theme_texture_options(name, kind)`, pure):

| Texture | Load option |
|---|---|
| `logo`, any `grade_*`, any `judgment_*` | mipmaps on |
| everything else, including the digit atlases | mipmaps off |
| `kind == tile` | `Wrap::Repeat` |
| everything else | `Wrap::Clamp` |
| entry with `"1x (screen pixels)"` scale (`scanlines`) | `Filter::Nearest` (the manifest note says GL_NEAREST) |
| everything else | `Filter::Linear` |

**Fallback** (missing or unreadable PNG, failed upload, or headless): every helper draws **one flat quad** over the **content rect**, not the padded image, via `GlQuadRenderer::draw_quad`, with colour `theme_fallback_color(kind, tint)`:

- **Fullscreen:** opaque `multiply(theme::color::kNavyDeep, tint)`.
- **Everything else:** `with_alpha(tint, tint.a · 0.25f)`.
- **Frame:** draws the **ring** as 4 strips around the hole, so a fallback never covers the banner.
- **Digits:** one flat quad per glyph rect.

**Unknown names** (no manifest entry, which includes "manifest missing or malformed"):

- `draw_sprite`, `draw_stretch_x` and the digits have no geometry, so they **draw nothing**.
- The rect-based helpers (`draw_stretch`, `draw_slice3`, `draw_slice9`, `draw_frame`, `draw_tiled`, `draw_fill_cropped`) draw the flat fallback over the caller's rect.
- Each unknown name is logged **once**.

**Logging:**

- **At load:** one line per bad manifest entry and one per missing or unreadable PNG. A missing or malformed manifest is a single line. Headless gets a single line ("no GL context; drawing flat fallbacks"). On success, one `std::cout` summary.
- **At draw:** only the first miss per unknown name. That uses a `mutable` warned-set with a transparent hash, so the steady state does no allocation.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/theme_textures.hpp` | CREATE | `ThemeKind`, `PxRect`, `ThemeEntry`, `DigitGlyph`/`DigitFont`, `ThemeManifest`, `ThemePiece`, `TileAnchor`, `DigitAlign`. Pure parse/placement/options functions. `BitmapDigits`. `ThemeTextures` |
| `src/render/theme_textures.cpp` | CREATE | Parser, pure math, loader, draw helpers, fallbacks |
| `CMakeLists.txt` | UPDATE | Add `src/render/theme_textures.cpp` to `blaze4k_core`, after `src/render/note_art.cpp` (line ~102) |
| `src/app/app.hpp` / `src/app/app.cpp` | UPDATE | `ThemeTextures theme_textures_` member + `const ThemeTextures& theme_textures() const`. Load in `init()` after the constants. `theme_textures_.shutdown()` first thing in `~App()` |
| `src/screens/screen.hpp` | UPDATE | Forward-declare `class ThemeTextures;` and add `const ThemeTextures* theme = nullptr;` to `ScreenContext`, documented like `ui_sounds` |
| `src/main.cpp` | UPDATE | `shell->context().theme = &app.theme_textures();` next to `constants` (line 366) |
| `tests/theme_textures_test.cpp` | CREATE | Parser, placement, slice math, cropped-fill UVs, tiling, digit widths, load options, headless/fallback/log-once |
| `tests/CMakeLists.txt` | UPDATE | Register `theme_textures_test` with `BLAZE4K_ASSETS_DIR` (mirror `theme_test`, lines 444-458) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Header: types, pure API, classes

- **File**: `src/render/theme_textures.hpp`
- **Action**: CREATE
- **Implement** (namespace `blaze4k`; include `geometry.hpp` and `texture.hpp`; forward-declare `GlQuadRenderer`):
  - `enum class ThemeKind { Fullscreen, Sprite, StretchX, Stretch, Slice3, Slice9, Frame, Tile };`
  - `struct PxRect { int x = 0, y = 0, w = 0, h = 0; };` and `struct Slice3Px { int left = 0, right = 0; };`, `struct Slice9Px { int top = 0, right = 0, bottom = 0, left = 0; };`
  - `struct ThemeEntry { std::string file; ThemeKind kind; int width = 0, height = 0; PxRect content; std::optional<PxRect> hole; std::optional<Slice3Px> slice3; std::optional<Slice9Px> slice9; bool screen_pixel_tile = false; };`
  - `inline constexpr std::string_view kDigitChars = "0123456789.%/ ";` and `[[nodiscard]] int digit_glyph_index(char c);` (-1 when unsupported).
  - `struct DigitGlyph { PxRect src; float origin_x = 0, advance = 0; bool present = false; };` and `struct DigitFont { std::string file; std::array<DigitGlyph, 14> glyphs{}; };` (14 = `kDigitChars.size()`; `static_assert`).
  - A transparent `struct ThemeStringHash` (`is_transparent`, hashes `std::string_view`), used with `std::equal_to<>`, so `find(std::string_view)` never allocates (C++20 heterogeneous unordered lookup).
  - `struct ThemeManifest { float texture_scale = 2.0f; std::unordered_map<std::string, ThemeEntry, ThemeStringHash, std::equal_to<>> textures; std::unordered_map<std::string, DigitFont, ...> fonts; };`
  - Pure: `[[nodiscard]] ThemeManifest parse_theme_manifest(std::string_view json_text, std::vector<std::string>* warnings = nullptr);` — never throws.
  - Pure placement: `struct ThemePiece { Rect dst; UVRect uv; };` plus `sprite_content_rect`, `content_to_image_rect(entry, content_rect)`, `slice3_pieces(entry, content_rect) -> std::array<ThemePiece,3>`, `slice9_pieces(entry, content_rect, k) -> std::array<ThemePiece,9>`, `frame_image_rect(entry, hole_rect)`, `enum class TileAnchor { TopLeft, Bottom };` `tile_uv(entry, rect, k, anchor)`, `fill_cropped_piece(entry, bar_rect, fraction) -> std::optional<ThemePiece>`, `theme_fallback_color(ThemeKind, Color tint)`.
  - Pure options: `struct ThemeLoadOptions { bool mipmaps; Texture::Wrap wrap; Texture::Filter filter; };` `[[nodiscard]] ThemeLoadOptions theme_texture_options(std::string_view name, const ThemeEntry& entry);`
  - Pure digits: `enum class DigitAlign { Left, Centre, Right };` `digits_width(const DigitFont&, std::string_view, float k)`, `digits_start_x(float x, float width, DigitAlign)`, `digit_glyph_rect(const DigitGlyph&, float pen_x, float y, float k)`, `digit_glyph_uv(const DigitGlyph&, int atlas_w, int atlas_h)`.
  - `class BitmapDigits` (move-only, owns `DigitFont font_`, `Texture texture_`, `int atlas_w_, atlas_h_`, `float texture_scale_`, `mutable bool warned_unsupported_`): `measure(text, s)`, `height(s)` (glyph h·k), `draw(r, text, x, y, s, align, tint = white) const`, `valid()`, `has_texture()`.
  - `class ThemeTextures` (non-copyable): `static std::filesystem::path default_directory();` `bool load(const std::filesystem::path& dir);` (true when the manifest parsed; idempotent: it calls `shutdown()` first), `void shutdown();` `const ThemeEntry* entry(std::string_view) const;` `Vec2 content_size(std::string_view, float s) const;` (`{0,0}` when unknown, so callers can centre by the `anchor` without the manifest), the `draw_*` helpers from Pinned Semantics (all `const`, `tint` defaulting to white), `const BitmapDigits& digits_chrome() const;` / `digits_white() const;`, and the counters `entry_count()`, `loaded_texture_count()`.
  - Doc comments state: content box vs padded image, `s` = `window_height / 720` (#91), straight-alpha tint, the fallback rule, and that nothing here touches the music clock.
- **Mirror**: `src/render/gl_quad_renderer.hpp:24-33` (pure helper beside the GL class), `src/gameplay/noteskin.hpp:85-140` (owner class with `init`/`shutdown`/`default_directory`)
- **Validate**: `cmake --build build -j8` (header compiles once Task 2 adds the TU)

### Task 2: Parser + pure math + loader + draws

- **File**: `src/render/theme_textures.cpp`
- **Action**: CREATE
- **Implement**:
  - **Parser.**
    - Use `json::parse(text, nullptr, false)`, and return an empty manifest if the result `is_discarded()` or is not an object. The `textures` and `bitmap_fonts` objects are both optional.
    - Caps: at most 256 textures, 8 fonts and 32 glyph keys. `texture_scale` must be finite and in `(0, 8]`, else default 2 and warn. Image ints must be in `[1, 4096]`. Rect ints must be integers in `[0, 4096]`. Glyph `origin_x` and `advance` must be finite and in `[-4096, 4096]` / `[0, 4096]`.
    - `file` must be a non-empty plain file name. Reject `/`, `\`, `..`, a leading `.` and a drive colon. It must end in `.png`.
    - `kind` must be one of the 8 names. `size_px` is 2 ints. `content_px` defaults to the full image when absent (needed by `scanlines`), must be `w,h > 0`, and must lie inside the image.
    - Kind-required keys: `slice3_px` for slice3 (`left,right ≥ 0`, `left + right < width`), `slice9_px` for slice9 (both axes `<` the image dimension), and `hole_px` for frame (inside the image, `w,h > 0`). For tile, `screen_pixel_tile = (scale is a string starting with "1x")`.
    - Glyph keys must be one character from `kDigitChars`. Unknown keys are ignored with a warning. Glyph rect `w,h > 0`.
    - Parse each entry inside its own `try { … } catch (const std::exception&)`. A bad entry is skipped with a warning naming it. A good entry is kept.
  - **Pure math.** Exactly the Pinned Semantics table. Guard non-finite or non-positive sizes, a non-finite `s`, and zero content or hole dims by returning empty rects or `std::nullopt` (draw nothing). Never divide by zero.
  - **`load(dir)`.**
    - Call `shutdown()` first.
    - Read `dir / "manifest.json"` with a 1 MiB cap. A missing, oversize or unreadable file gives one warning and returns false. Parse, then print the warnings.
    - For each entry, `probe_image_header`. If the header is `!ok` or its dims differ from `size_px`, warn and leave the texture invalid. For a font, store the atlas dims and drop (with a warning) any glyph whose `src` exceeds them.
    - If `glad_glGenTextures == nullptr`, warn **once**, skip every upload and return true (geometry is still available, so the fallbacks keep the right shape).
    - Otherwise call `Texture::from_file(path, opts.mipmaps, opts.wrap, opts.filter)` per entry and per digit atlas, and warn on each invalid result.
    - Print the `std::cout` summary `Loaded N/63 textures, M/2 digit fonts from <dir>`.
  - **Draw helpers.**
    - Look up the entry. Unknown: log once, then apply the unknown-name rule.
    - Texture valid: issue `draw_textured_quad` per piece.
    - Otherwise: issue `draw_quad(content_rect, theme_fallback_color(...))`, or 4 ring strips for a frame.
    - No allocation on the hit path.
- **Mirror**: `src/gameplay/noteskin.cpp:186-261` (directory, GL check, load-or-warn), `src/data/judgment_constants_loader.cpp:24,161-175,222` (size cap, non-throwing parse, catch)
- **Validate**: `cmake --build build -j8`

### Task 3: Build registration

- **File**: `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/render/theme_textures.cpp` to `blaze4k_core` after `src/render/note_art.cpp`. The `assets/` POST_BUILD copy (`CMakeLists.txt:161-168`) already ships the manifest and PNGs.
- **Validate**: `cmake --build build -j8`

### Task 4: App ownership and GL-safe lifetime

- **Files**: `src/app/app.hpp`, `src/app/app.cpp`
- **Action**: UPDATE
- **Implement**:
  - `#include "render/theme_textures.hpp"`, add the member `ThemeTextures theme_textures_;` and the accessor `[[nodiscard]] const ThemeTextures& theme_textures() const`.
  - In `App::init()`, after the judgment-constants block (`app.cpp:31-42`), add `theme_textures_.load(ThemeTextures::default_directory());`. An empty directory makes `load` log once and return false; the result is non-fatal and ignored. Headless gets the one "no GL context" line.
  - In `~App()`, call `theme_textures_.shutdown();` **before** `window_.shutdown();`, with a comment that the textures must be released while the GL context is alive.
- **Mirror**: `src/app/app.hpp:40,64` (`judgment_constants_` accessor and member), `src/app/app.cpp:31-42`
- **Validate**: `cmake --build build -j8`; `app_test` (headless `App::init`) still passes

### Task 5: Screen seam + wiring

- **Files**: `src/screens/screen.hpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `screen.hpp`, add `class ThemeTextures;` to the forward declarations and append `const ThemeTextures* theme = nullptr;` at the end of `ScreenContext`, with a doc comment: "#89: Cabinet theme textures + bitmap digits, owned by App; null in headless/unit tests (screens must null-check)". Appending keeps the existing aggregate inits valid.
  - In `main.cpp` add `shell->context().theme = &app.theme_textures();` after line 366. The demo path is unchanged.
- **Mirror**: `src/screens/screen.hpp:59-62`, `src/main.cpp:366`
- **Validate**: `cmake --build build -j8`; all screen tests still pass (the pointer is null there)

### Task 6: Unit tests

- **Files**: `tests/theme_textures_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE: register `theme_textures_test` with `BLAZE4K_ASSETS_DIR`, mirroring lines 444-458)
- **Implement** (use `approx(a, b, 1e-4f)`; one `std::cout` line per case):
  1. **`test_parse_real_manifest`**. Read `kAssets/theme/cabinet/manifest.json` and parse it with no warnings. Expect 63 entries, 2 fonts and `texture_scale == 2`. Check these entries:
     - `logo`: content {80,80,1960,380}, kind Sprite.
     - `chip`: slice3 {44,44}.
     - `life_frame`: slice9 {16,8,16,8}.
     - `banner_frame`: hole {8,8,1120,314}.
     - `scanlines`: content = full image {0,0,1,3} and `screen_pixel_tile`.
     - `life_stripes`: not `screen_pixel_tile`.
     - Digit font `digits_white`: glyph `'1'` src {148,0,92,164}, advance 34.03.
  2. **`test_parse_malformed_never_throws`**. Each of these yields zero entries: `""`, `"not json"`, `"[]"`, `"{\"textures\": 5}"`, `"{\"textures\": {\"a\": 7}}"`. A mixed object keeps its one good entry and skips these bad ones, with a warning each:
     - `size_px` given as a string;
     - `content_px` out of bounds;
     - negative content;
     - `slice3` with `left + right >= width`;
     - `frame` without `hole_px`;
     - `file` `"../x.png"`, `"a/b.png"` or `"x.jpg"`;
     - an unknown kind;
     - a size of 100000.

     Also check that `texture_scale` 0 falls back to 2 with a warning.
  3. **`test_sprite_content_box_placement`**. `logo` at s=1, pos (150,168): content {150,168,980,190} (equals `layout_720p`), image quad {110,128,1060,270}. At s=2, pos (300,336): image {220,256,2120,540}. `content_size("logo", 1) == {980,190}`.
  4. **`test_slice3_rects`**. `chip` with content rect {100,50,140,30} (k=0.5):
     - image rect {96,46,148,38};
     - left cap {96,46,22,38} with u [0, 44/296];
     - middle {118,46,104,38} with u [44/296, 252/296];
     - right cap {222,46,22,38} with u [252/296, 1].

     Narrow case, content w=4: image w=12, caps 6 each, middle w 0.
  5. **`test_slice9_rects`**. `life_frame` with rect {40,120,40,480}, s=1:
     - TL {40,120,4,8} with uv {0,0,0.1,16/960};
     - centre {44,128,32,464} with uv {0.1,16/960,0.9,944/960};
     - BR {76,592,4,8}.

     Shrink case (rect h=10): the top and bottom borders shrink to 5 each and the centre h is 0.
  6. **`test_frame_rect`**. `banner_frame` with hole {48,100,560,157}: image {44,96,568,165}, which equals the manifest's `layout_720p`.
  7. **`test_tile_uv`**.
     - `life_stripes` with rect {44,128,32,464}, s=1, `Bottom`: u [0,4], `v1 == 1`, `v1 − v0 ≈ 464/12`.
     - `TopLeft`: v0 = 0.
     - `scanlines` at s=2 over {0,0,2560,1440}: u [0,2560] and v [0,480], independent of s.
  8. **`test_fill_cropped_uv`**. `life_fill` with bar {44,128,32,464}:
     - f=0.25: dst {44,476,32,116}, uv {0,0.75,1,1};
     - f=1: the full bar, uv [0,1];
     - f=0, −1 and NaN: `nullopt`;
     - f=2 clamps to 1.

     Also check that the bar rect at f=0.5 and at f=0.25 maps v=0.75 to the same screen y, which proves the gradient does not slide.
  9. **`test_digit_layout_widths`**.
     - `digits_white`, `"100%"` at s=1: width ≈ (34.03+88.84+88.84+83.16)·0.5 = 147.435.
     - `digits_chrome`, `"98.5"`: ≈ 190.795.
     - `"1a2"` = width(`"12"`); `""` = 0.
     - `digits_start_x(500, 100, Right) == 400` and `Centre == 450`.
     - `digit_glyph_rect('1' white, pen 10, y 20, k 0.5) == {−4,20,46,82}`; uv u [148/1722, 240/1722].
  10. **`test_load_options`**.
      - `logo`, `grade_S`, `grade_quad_star` and `judgment_miss` get mipmaps.
      - `bg_title`, `chip` and the digit atlases get no mipmaps.
      - `life_stripes` gets Repeat + Linear.
      - `scanlines` gets Repeat + Nearest.
      - `chip` gets Clamp + Linear.
  11. **`test_headless_load_and_draw`**.
      - `ThemeTextures::load(cabinet dir)` returns true, with `entry_count() == 63` and `loaded_texture_count() == 0`.
      - Every helper, plus both digit fonts' `draw`, runs on an uninitialized `GlQuadRenderer` without crashing, including zero/NaN/negative sizes.
      - `load("/no/such/dir")` returns false, and the draws still do not crash.
      - `shutdown()` is idempotent.
  12. **`test_missing_png_and_log_once`**. Build a temp dir with a manifest naming `ok.png` (copy `scanlines.png`, declared 1×3) and `wrong.png` (copy `scanlines.png`, declared 2×2), plus `gone.png` (absent). Redirect `std::cerr` to an `ostringstream` (restore it with an RAII guard).
      - Load: the mismatch and the missing file each warn once, and all three entries stay drawable via fallback.
      - Draw an unknown name 3 times: exactly one warning for that name.
      - Clean up the temp dir.
- **Mirror**: `tests/theme_test.cpp` (structure, `BLAZE4K_ASSETS_DIR`), `tests/background_test.cpp:71-80` (headless renderer), `tests/assist_tick_test.cpp:81` (temp dir)
- **Validate**: `cmake --build build -j8 && ./build/tests/theme_textures_test`, then the sandboxed ctest (44/44)

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from the touched files
cmake --build build -j8 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides the audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 44

# Static checks
git status --short                       # expect: only the files in "Files to Change" (+ the owner's todo-stories.md, untouched)
git diff --stat -- TODO.md .agents/stories/todo-stories.md   # expect: no diff from this work
```

## End-to-End Verification

1. **Automated pure-path proof.** `theme_textures_test` pins the manifest parse against the real pack, content-box placement (cross-checked against the manifest's own `layout_720p` for `logo` and `banner_frame`), the slice3/slice9 rect math, frame, tiling, the cropped-fill UVs, digit widths and alignment, and the load-option policy. It runs under the sandboxed ctest command.
2. **Real app init (headless, safe for the agent).** Run `./build/blaze-4k --headless --smoke-test 5 --data-dir "$(mktemp -d)"` from the repo root inside the same `bwrap` sandbox. Expect exactly one `[ThemeTextures]` line saying there is no GL context, no crash, and a clean exit ("Blaze 4k shut down cleanly."). Run it from `/tmp` too: the cwd lacks `assets/`, so it must resolve through the executable dir (`build/assets/theme/cabinet` from the POST_BUILD copy).
3. **Lifetime.** `app_test` (headless `App::init` + destructor) passes. Code review confirms `theme_textures_.shutdown()` precedes `window_.shutdown()` in `~App()`.
4. **Optional live GL check (owner only; the agent must not launch the GUI, because it opens a window and plays audio).**
   - Run `./build/blaze-4k`. Expect `[ThemeTextures] Loaded 63/63 textures, 2/2 digit fonts`.
   - For a visual check, temporarily add to `TitleScreen::render`, when `ctx.theme` is set, with `s = h / 720.0f`:
     - `draw_stretch("bg_title", {0,0,w,h})`;
     - `draw_sprite("logo", {150·s,168·s}, s)`;
     - `draw_tiled("scanlines", {0,0,w,h}, s)`;
     - `digits_chrome().draw(r, "98.76%", w/2, 500·s, s, Centre)`.
   - Compare with `docs/cabinet-theme/reference/`, then rename a PNG and confirm a single warning and a translucent placeholder. Remove the debug draw; the real consumers are #92 to #95.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Textures destroyed after the GL context (`~App` order) | Explicit `theme_textures_.shutdown()` at the top of `~App()`. `Texture::destroy` already skips GL when glad is unloaded | In scope |
| Headless `from_file` spam (one log per PNG, 65 lines) breaks the "logs once" AC | Check `glad_glGenTextures` once in `load` and skip all uploads | In scope |
| Per-frame allocations from string-keyed lookups (`"diff_row_challenge_selected"` exceeds SSO) | Transparent hash + `std::equal_to<>` for heterogeneous `find(std::string_view)`. The warned-set allocates only on a first miss | In scope |
| A malformed manifest crashes (`.get<int>()` type errors, huge numbers, path traversal in `file`) | Non-throwing parse, per-entry try/catch, numeric caps, plain-filename check, and image-dim cross-check via `probe_image_header`. Tested with malformed strings | In scope |
| Slice3 caps distort the slanted ends if scaled non-uniformly | Caps use the uniform `k = rect.h / C.h`, and only the middle stretches | In scope |
| Linear filtering bleeds across slice seams / atlas glyph neighbours | The slices are neighbours in the same image, so bleed is the intended continuity. Digit glyphs have ≥2px gaps and are not mipmapped | In scope (no action) |
| Startup time/VRAM: three 2560×1440 backgrounds are ~15 MB of VRAM each, plus ~65 decodes | Matches the Cabinet doc's budget (`IMPLEMENTATION_PLAN.md:166-171`). Loaded once at init. Shipping `bg_select`/`bg_results` at 1× is a later #98 polish call | Out of scope (flag) |
| `TileAnchor::Bottom` produces negative V | Valid with `GL_REPEAT`. The test pins `v1 = 1` and the span, not the sign | In scope |
| Screens dereference a null `ctx.theme` in tests | Pointer documented as null in headless/unit tests. #92 to #95 must null-check (no screen uses it in this PR) | Out of scope (documented) |
| Manifest entries change later (theme re-export) | The parser is data-driven, and `theme_test` already pins the 63/65 counts | Out of scope |

---

## Open Questions

- **Fallback colour.** The issue says "flat quad" but gives no colour. **Proposed default:**
  - fullscreen: opaque `kNavyDeep × tint`;
  - all other kinds: the tint at 25% alpha over the content box only;
  - frame: draws only its ring.

  This keeps layout visible without white blocks or covered banners. Non-blocking.
- **Missing manifest vs `draw_sprite`.** With no manifest the sprite size is unknown, so `draw_sprite` / `draw_stretch_x` / the digits **draw nothing**. They log once, and the rect-based helpers still draw flat quads. Compiled-in fallback sizes would duplicate the manifest. Non-blocking.
- **Owner: `App` vs `main.cpp`.** The issue's technical note says to load in `src/app/app.cpp`. **Proposed default:** `App` owns and loads it (mirrors `judgment_constants_`), and `main.cpp` only wires the pointer, because that is where all `ScreenContext` wiring lives. Non-blocking.
- **Digit atlas mipmaps.** The AC lists mipmaps for grades, judgments and logo only. The digits draw at 0.5× native at 720p. **Proposed default:** no mipmaps, following the AC; packed glyphs with 2px gaps would bleed at lower mip levels. Revisit in #95 if the chrome percent shimmers. Non-blocking.
- **Extra helpers beyond the AC list.** The AC names six helpers. **Proposed default:** also add `draw_stretch` (for `fullscreen`/`stretch`) and `draw_stretch_x` (for `stretch_x`), because the issue says "draw helpers for each manifest `kind`", and `content_size()` for the `anchor: centre` sprites. Non-blocking.
- **One file vs two.** The issue names only `theme_textures.{hpp,cpp}`. **Proposed default:** `BitmapDigits` lives in the same module. Split out a `bitmap_digits.*` only if the review asks. Non-blocking.

---

## Acceptance Criteria

- [ ] `manifest.json` is parsed with nlohmann/json, and every listed PNG is loaded once at app init (`App::init`), with mipmaps for `grade_*`/`judgment_*`/`logo`, Repeat for `tile` kinds, and Nearest for `scanlines`
- [ ] `draw_sprite`, `draw_slice3`, `draw_slice9`, `draw_frame`, `draw_tiled` and `draw_fill_cropped` (plus `draw_stretch`/`draw_stretch_x`) position the `content_px` box, not the padded image. `draw_fill_cropped` crops UVs so the life gradient does not slide
- [ ] `BitmapDigits` draws `0-9 . % /` and space from `digits_chrome`/`digits_white` using glyph rect, `origin_x` and `advance`, with Left/Centre/Right alignment
- [ ] A missing/unreadable PNG or manifest logs once and falls back to a flat quad. A malformed manifest never crashes. Headless logs one line and all tests keep running
- [ ] Unit tests cover manifest parsing, content-box placement, slice3/slice9 rect math, cropped-fill UVs and digit layout widths
- [ ] `ScreenContext::theme` is wired in `main.cpp`. Textures are released before the GL context is destroyed
- [ ] Build has no new warnings, and sandboxed `ctest` passes 44/44
- [ ] `TODO.md` and `.agents/stories/todo-stories.md` are untouched
- [ ] Follows existing patterns
