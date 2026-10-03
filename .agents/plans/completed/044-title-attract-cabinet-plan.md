# Plan: Cabinet Title + Attract Screens (#92)

## Summary

Restyle the Title and Attract screens with the Cabinet v3 look (`docs/cabinet-theme/reference/cabinet-v3-title.png`). These are the first screens to use the theme services from #89 (`ThemeTextures`), #90 (`TextRenderer`) and #91 (`theme::layout_scale`).

**Title** draws, in this order:

1. `bg_title`, stretched to the window
2. the `logo` sprite
3. the `subtitle` sprite
4. four Cel tap notes from `NoteSkin::head`, one per column, at `kTitleArrowsTop`. Each column is rotated and has a fixed quantization colour: Left 4th (red), Down 8th (blue), Up 12th (purple), Right 16th (yellow), at a fixed beat.
5. the `press_start` plate, blinking at 1 Hz with exactly today's on/off rule
6. the footer: "SINGLE · 4 PANEL" left and "BLAZE 4K v<version>" right, in `theme::text::kFooter`
7. the `scanlines` overlay over the whole window

The version comes from a new `BLAZE4K_VERSION` compile definition, set from CMake's `PROJECT_VERSION`.

**Attract** draws the same background and logo, plus Cel receptors in the same row. It keeps its brightness pulse (on the logo and the PRESS START plate) and its 4 Hz receptor blink (the lit receptor gets the Cel flash brightness and a 1.15x zoom).

**Shared code.** Title and Attract share one small helper module, `src/screens/title_art.{hpp,cpp}`. It holds the pure layout functions (GL-free, unit-tested) and the thin draw helpers.

**NoteSkin access.** One `NoteSkin` is owned by `App` (loaded in `App::init` next to the theme textures, released before the GL context dies). Screens reach it through a new `ScreenContext::noteskin` pointer.

**What does not change.** Every pointer stays null-checked, so headless tests keep working. Screen transitions and timings are untouched. A new `title_screen_test` pins the layout at 720p, 1440p, 21:9 and 16:10, the blink and pulse rules, the version string, and the footer glyph coverage.

## User Story

As a player launching Blaze 4k
I want the title and attract screens to show the chrome Cabinet logo, Cel arrows and the PRESS START plate from the mock-up
So that the game looks like a finished arcade cabinet from the first frame, sharp at 720p and at 1440p.

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW-MEDIUM (issue says Small; 2 screens, 1 new helper TU, 1 new test, App/ScreenContext/CMake wiring) |
| Systems Affected | `src/screens` (title, attract, new `title_art`), `src/screens/screen.hpp` (`ScreenContext::noteskin`), `src/app` (App owns a `NoteSkin`), `src/main.cpp` (wiring), `CMakeLists.txt` (`BLAZE4K_VERSION`, new TU), `tests/` (new `title_screen_test`) |
| GitHub Issue | #92 (TODO-20; blocked by #89, #90, #91, all merged; blocks #98; answers #78, closed) |
| Plan sequence | 044 (running plan sequence; the issue number is #92) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build with `cmake --build build -j8` |
| C++ compiler | GCC 16.2.1 | C++20 with `-Wall -Wextra -Wpedantic` and no `-Werror`. Add **no new warnings** |
| Baseline tests | **46/46 pass** | Run on `main` @ `b8e1212` with the sandboxed command in Validation. Start green and stay green. After this change the count is **47** (`title_screen_test` added) |
| Sandbox requirement | — | `audio_test` opens the real audio device, so **always** run ctest (and any binary) through the `bwrap` command |
| Headless smoke | `./build/blaze-4k --headless --smoke-test 5 --data-dir <tmp>` | On `main` it prints `[TitleScreen] logo + "Press Start"`, `[ScreenManager] enter Title` and `Blaze 4k shut down cleanly.` (verified) |
| Reference image | `docs/cabinet-theme/reference/cabinet-v3-title.png` | The issue says `reference/cabinet-v3-title.png`, but the file lives under `docs/cabinet-theme/` |
| Title art (all baked) | `assets/theme/cabinet/manifest.json` | `bg_title` fullscreen 2560x1440 (streaks, grid and footer fade baked in, "Draw first, stretched to the window"). `logo` sprite content 1960x380 @2x, layout_720p `[150,168,980,190]`, mipmapped. `subtitle` `[380,362,520,30]`. `press_start` `[420,548,440,76]` ("Blink by alpha", text baked in). `scanlines` is a 1x3 tile, screen-pixel, nearest ("Title, select and score screens only") |
| Layout constants | `src/render/theme.hpp:177-183` | `kLogoTop 168`, `kSubtitleTop 362`, `kTitleArrowsTop 402` ("four Cel tap notes, 96px, 108px pitch (same as the field)"), `kPressStartTop 548`, `kFooterHeight 40`, `kFooterPadX 36` |
| Footer style | `src/render/theme.hpp:117-118` | `text::kFooter{SairaBold, 18, 4, false, kSteel}`. It is already in `kAllStyles`, so it is pre-baked |
| Layout scale | `src/render/theme_layout.hpp` | `theme::layout_scale(w, h)`: `s = min(h/720, w/1280)` with the column centred on both axes. 1280x720 is the identity, 2560x1440 an exact 2x, 3440x1440 gives s=2 and origin (440, 0), 1920x1200 gives s=1.5 and origin (0, 60) |
| NoteSkin | `src/gameplay/noteskin.hpp` | `head(type, column, quantization, beat)` returns a `SkinSprite` (Cel: rotated by `column_rotation`, tint white). `receptor(column, beat)` tint = `cel_receptor_brightness(beat)`: 1.0 on the beat, 0.55 at rest (`noteskin.cpp:52-53,143-150`). `init()` needs GL: headless logs "No GL context available" and returns false (`noteskin.cpp:196-213`). Today the only instance lives in `GameplayView::skin_` (`gameplay_view.hpp:101`) |
| Cel quantization colours | `cel_tap_frame` band = `NoteQuantization` index | 4th red, 8th blue, 12th purple, 16th yellow. These match the mock's left/down/up/right arrows |
| Invalid-texture hazard | `src/render/gl_quad_renderer.cpp:280,290` | `draw_textured_quad` with an **invalid** texture draws with the white texture, a solid white quad. An uninitialised `NoteSkin` returns non-null pointers to invalid procedural textures (`noteskin.cpp:321-324`). Guard before drawing |
| Version | `CMakeLists.txt:3` | `project(blaze-4k VERSION 0.1.0 …)`. The mock shows "v1.1.0", which is only illustrative |
| Quoted compile-definition precedent | `tests/CMakeLists.txt:455-457` | `BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"`. CMake escapes the quotes correctly on every generator |
| Window size | `src/app/window.hpp:11-15` | Defaults to 1280x720, resizable. There is no CLI size flag, so the 2560x1440 check is done by resizing or maximising on a 1440p display |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

---

## Patterns to Follow

### ScreenContext service pointers (null in headless/unit tests, screens null-check)

```cpp
// SOURCE: src/screens/screen.hpp:66-73
    // #89: Cabinet theme textures + bitmap digits, owned by App; null in
    // headless/unit tests (screens must null-check).
    const ThemeTextures* theme = nullptr;

    // #90: TrueType text in theme::TextStyle (measure, truncate, draw), owned by
    // App; null in headless/unit tests (screens must null-check). Non-const
    // because a size missing from theme::text::kAllStyles bakes lazily.
    TextRenderer* text = nullptr;
```

```cpp
// SOURCE: src/main.cpp:367-368
        shell->context().theme = &app.theme_textures();
        shell->context().text = &app.text_renderer();
```

### App-owned GL resources: load in init, release before the window

```cpp
// SOURCE: src/app/app.cpp:12-20
App::~App() {
    stop();
    // The text atlases and theme textures must be released while the GL context
    // is still alive: window_.shutdown() destroys it, before members are destroyed.
    text_renderer_.shutdown();
    theme_textures_.shutdown();
    window_.shutdown();
    SDL_Quit();
}
```

```cpp
// SOURCE: src/app/app.cpp:48-50
    // #89: load every Cabinet theme PNG once. Non-fatal: a missing pack logs
    // once and the draws fall back to flat quads; headless logs one line.
    theme_textures_.load(ThemeTextures::default_directory());
```

### Drawing a SkinSprite centred in a box (mirror it, plus a validity guard)

```cpp
// SOURCE: src/gameplay/note_field_renderer.cpp:23-35
// Draws `sprite` in a square of `box * sprite.scale` centered on (x, y).
int draw_sprite(GlQuadRenderer& renderer, const SkinSprite& sprite, double x, double y,
                double box) {
    if (sprite.texture == nullptr) {
        return 0;
    }
    const double size = box * sprite.scale;
    renderer.set_blend_mode(sprite.blend);
    renderer.draw_textured_quad(centered_quad(x, y, size, size), *sprite.texture, sprite.uv,
                                sprite.tint, sprite.rotation);
    renderer.set_blend_mode(BlendMode::Alpha);
    return 1;
}
```

### Current blink and pulse logic (keep these exactly)

```cpp
// SOURCE: src/screens/title_screen.cpp:69 (1 Hz: on for 0.5 s, off for 0.5 s)
    if ((static_cast<int>(blink_seconds_ * 2.0) % 2) == 0) {
// SOURCE: src/screens/attract_screen.cpp:47,53
    const float pulse = 0.65f + 0.35f * static_cast<float>(std::sin(phase_seconds_ * 2.0));
    const int active = static_cast<int>(phase_seconds_ * 4.0) % 4;
```

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, asset roots via compile definitions)

```cpp
// SOURCE: tests/screen_manager_test.cpp:17-24
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)
```

```cmake
# SOURCE: tests/CMakeLists.txt:487-501 (ttf_font_test registration with a source-root define)
add_executable(ttf_font_test ttf_font_test.cpp)
target_link_libraries(ttf_font_test PRIVATE blaze4k_core)
target_compile_definitions(ttf_font_test PRIVATE BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME ttf_font_test COMMAND ttf_font_test)
```

Headless loading of the real assets: `ThemeTextures::load(kAssets / "theme" / "cabinet")` returns true and uploads nothing (`tests/theme_textures_test.cpp:242`). `TextRenderer::load(kSourceDir)` measures headless (`tests/ttf_font_test.cpp:484`).

---

## Pinned Semantics (the contract `title_screen_test` pins)

Reference-space values, mapped with `L = theme::layout_scale(w, h)`. "Content box" is the manifest `content_px` box, so the glow padding hangs outside it.

| Element | Reference (1280x720) | 2560x1440 (s=2, origin 0,0) | 3440x1440 (s=2, origin 440,0) | 1920x1200 (s=1.5, origin 0,60) |
|---|---|---|---|---|
| Logo content top-left | (150, 168), size 980x190 | (300, 336) | (740, 336) | (225, 312) |
| Subtitle content top-left | (380, 362), size 520x30 | (760, 724) | (1200, 724) | (570, 603) |
| Arrow centres (x for columns 0..3; y) | x = 640 + (c − 1.5)·108 = 478, 586, 694, 802; y = 402 + 48 = 450 | x 956, 1172, 1388, 1604; y 900 | x 1396, 1612, 1828, 2044; y 900 | x 717, 879, 1041, 1203; y 735 |
| Arrow box | 96 (`NoteSkin::kNoteSize`) | 192 | 192 | 144 |
| PRESS START content top-left | (420, 548), size 440x76 | (840, 1096) | (1280, 1096) | (630, 882) |
| Footer band | y 680..720 (`kRefHeight − kFooterHeight`) | 1360..1440 | 1360..1440 | 1080..1140 |
| Footer left anchor (TextAlign::Left) | x = 36 | 72 | 512 | 54 |
| Footer right anchor (TextAlign::Right) | x = 1244 (1280 − 36) | 2488 | 2928 | 1866 |
| Footer text top | band top + (band height − `line_height(kFooter)`) / 2 | same formula | same formula | same formula |
| `bg_title` | the whole window `{0, 0, w, h}` (stretched, per manifest) | `{0,0,2560,1440}` | `{0,0,3440,1440}` | `{0,0,1920,1200}` |
| `scanlines` | tiled over the whole window `{0, 0, w, h}`, 1 texel per screen px | same | same | same |

- Sprites are centred on reference x = 640 using `theme->content_size(name, s)`: `pos.x = L.x(640) − size.x / 2`, `pos.y = L.y(top)`. Both are rounded to whole pixels with `std::round`. At 720p and 1440p every value above is already an integer, so rounding changes nothing there. At 1.5x it avoids half-pixel blur.
- Arrow quantization per column: `{Fourth, Eighth, Twelfth, Sixteenth}`. Fixed beat `kArrowBeat = 0.0`, which is frame 0 of the 2-beat Cel cycle.
- `prompt_visible(t) = (static_cast<int>(t * 2.0) % 2) == 0`: true at 0 and 0.49, false at 0.5 and 0.99, true at 1.0 and 1.49.
- Attract: `attract_pulse(t) = 0.65 + 0.35·sin(2t)`, always in [0.30, 1.00]. `attract_active_receptor(t) = int(t·4) % 4`. The lit receptor is `skin.receptor(c, 0.0)` (brightness 1.0) with `scale = 1.15`. The others are `skin.receptor(c, 0.5)` (brightness 0.55, scale 1.0). Receptors use the same centres and box as the title arrows. The logo tint is `{pulse, pulse, pulse, 1}`, and the PRESS START tint is `{1, 1, 1, pulse}`.
- Footer strings:
  - left `"SINGLE \xC2\xB7 4 PANEL"` (UTF-8 U+00B7, which is in the baked Latin-1 range);
  - right `"BLAZE 4K v" BLAZE4K_VERSION`, for example `"BLAZE 4K v0.1.0"` today.
- `skin_sprite_drawable(sprite) = sprite.texture != nullptr && sprite.texture->valid()`.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/title_art.hpp` | CREATE | Pure layout helpers + thin draw helpers shared by Title and Attract (namespace `blaze4k::title_art`) |
| `src/screens/title_art.cpp` | CREATE | Implementations. The only TU that reads `BLAZE4K_VERSION` (`#error` if it is missing) |
| `src/screens/title_screen.cpp` | UPDATE | Cabinet render. `update()` and `enter()` are unchanged. Drop the bitmap-font include and the palette constants |
| `src/screens/title_screen.hpp` | UPDATE | Comment only (describe the Cabinet render) |
| `src/screens/attract_screen.cpp` | UPDATE | `bg_title` + logo with pulse, Cel receptors with the blink, PRESS START plate with pulse, scanlines. `update()` unchanged |
| `src/screens/attract_screen.hpp` | UPDATE | Comment only |
| `src/screens/screen.hpp` | UPDATE | Forward-declare `class NoteSkin;` and add `const NoteSkin* noteskin = nullptr;` (#92 comment, null in headless/unit tests) |
| `src/app/app.hpp` | UPDATE | `#include "gameplay/noteskin.hpp"`, a `NoteSkin noteskin_` member, and a `const NoteSkin& noteskin() const` accessor |
| `src/app/app.cpp` | UPDATE | `noteskin_.init()` in `init()` (only when not headless); `noteskin_.shutdown()` in `~App` before `window_.shutdown()` |
| `src/main.cpp` | UPDATE | `shell->context().noteskin = &app.noteskin();` next to `theme` / `text` |
| `CMakeLists.txt` | UPDATE | Add `src/screens/title_art.cpp` to `blaze4k_core`, and `target_compile_definitions(blaze4k_core PUBLIC BLAZE4K_VERSION="${PROJECT_VERSION}")` |
| `tests/title_screen_test.cpp` | CREATE | Pinned Semantics + render smoke with real headless assets + transitions unchanged |
| `tests/CMakeLists.txt` | UPDATE | Register `title_screen_test` with `BLAZE4K_SOURCE_DIR` and `BLAZE4K_ASSETS_DIR` |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Version compile definition + new TU in the core library

- **File**: `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Add `src/screens/title_art.cpp` to the `add_library(blaze4k_core STATIC …)` list, after `src/screens/attract_screen.cpp`.
  - After `target_include_directories(blaze4k_core …)`, add:
    ```cmake
    # #92: title footer version ("BLAZE 4K v<version>"), from project(VERSION ...).
    target_compile_definitions(blaze4k_core PUBLIC BLAZE4K_VERSION="${PROJECT_VERSION}")
    ```
    Use PUBLIC so tests that link `blaze4k_core` see the same value.
- **Mirror**: `tests/CMakeLists.txt:455-457` (quoted string definition).
- **Validate**: done at Task 3, once the TU exists.

### Task 2: `ScreenContext::noteskin` + App-owned NoteSkin + wiring

- **Files**: `src/screens/screen.hpp`, `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `screen.hpp`: add `class NoteSkin;` to the forward declarations (lines 13-23). After `text`, add:
    ```cpp
    // #92: the Cel noteskin (title tap notes, attract receptors), owned by App
    // and loaded once at init; null in headless/unit tests (screens must
    // null-check). Gameplay still owns its own instance (GameplayView::skin_).
    const NoteSkin* noteskin = nullptr;
    ```
  - `app.hpp`:
    - add `#include "gameplay/noteskin.hpp"`;
    - add the member `NoteSkin noteskin_;` after `text_renderer_`;
    - add the accessor `[[nodiscard]] const NoteSkin& noteskin() const { return noteskin_; }` with a `// #92:` comment.
  - `app.cpp`:
    - In `init()`, after `text_renderer_.set_window_size(...)`, add:
      ```cpp
      // #92: the Cel noteskin for the title/attract art (needs GL; headless skips
      // it, and screens null-check / validity-check every sprite).
      if (!window_.is_headless()) {
          noteskin_.init();
      }
      ```
    - In `~App()`, add `noteskin_.shutdown();` with the text and theme shutdowns, before `window_.shutdown()`. Extend the comment to say "noteskin".
    - `NoteSkin::init` is idempotent (`noteskin.cpp:197-199`), so `run()` re-calling `init()` is safe.
  - `main.cpp:368`: add `shell->context().noteskin = &app.noteskin();`.
- **Mirror**: `app.cpp:12-20,48-56`, `main.cpp:367-368`, `screen.hpp:66-73`.
- **Validate**: `cmake --build build -j8`. There must be no warnings from the touched files.

### Task 3: `title_art` helper module

- **Files**: `src/screens/title_art.hpp`, `src/screens/title_art.cpp`
- **Action**: CREATE
- **Implement** (`namespace blaze4k::title_art`):
  - **Header comment.** The module is the Cabinet title/attract art (#92): reference-space layout per Pinned Semantics, plus thin draw helpers. Pure functions are GL-free. Presentation only: nothing reads the music clock, and animation runs on the screens' fixed `dt`.
  - **Pure:**
    - `inline constexpr double kArrowBeat = 0.0;`
    - `[[nodiscard]] NoteQuantization arrow_quantization(int column);` returns `{Fourth, Eighth, Twelfth, Sixteenth}`. Clamp out-of-range values to `Fourth`.
    - `[[nodiscard]] Vec2 arrow_centre(const theme::LayoutScale& L, int column);` returns `{L.x(kRefWidth/2 + (column − 1.5)·NoteSkin::kColumnWidth), L.y(layout::kTitleArrowsTop + NoteSkin::kNoteSize/2)}`.
    - `[[nodiscard]] float arrow_box(const theme::LayoutScale& L);` returns `L.px(NoteSkin::kNoteSize)`.
    - `[[nodiscard]] Vec2 centred_sprite_pos(const theme::LayoutScale& L, float ref_top, Vec2 content_size);` rounds `{L.x(640) − size.x/2, L.y(ref_top)}` with `std::round`.
    - `[[nodiscard]] bool prompt_visible(double blink_seconds);` uses the exact existing expression.
    - `[[nodiscard]] float attract_pulse(double phase_seconds);` and `[[nodiscard]] int attract_active_receptor(double phase_seconds);` use the exact existing expressions.
    - `[[nodiscard]] std::string_view footer_left_text();` returns `"SINGLE \xC2\xB7 4 PANEL"`.
    - `[[nodiscard]] std::string_view footer_right_text();` returns `"BLAZE 4K v" BLAZE4K_VERSION`, a static literal.
    - `[[nodiscard]] float footer_text_top(const theme::LayoutScale& L, float line_height);` returns `L.y(kRefHeight − kFooterHeight) + (L.px(kFooterHeight) − line_height) * 0.5f`.
    - `[[nodiscard]] bool skin_sprite_drawable(const SkinSprite& sprite);`
  - **Draw** (each a no-op on an uninitialised renderer, as `GlQuadRenderer` already is):
    - `void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& r, int w, int h);` calls `theme.draw_stretch(r, "bg_title", Rect{0, 0, w, h})`.
    - `void draw_centred_sprite(const ThemeTextures& theme, GlQuadRenderer& r, std::string_view name, const theme::LayoutScale& L, float ref_top, Color tint = {});` uses `content_size` + `centred_sprite_pos` + `draw_sprite`.
    - `void draw_skin_sprite(GlQuadRenderer& r, const SkinSprite& sprite, Vec2 centre, float box);` mirrors `note_field_renderer.cpp:23-35`, but returns early unless `skin_sprite_drawable`. That avoids the white-quad hazard.
    - `void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& r, int w, int h, float s);` calls `theme.draw_tiled(r, "scanlines", Rect{0, 0, w, h}, s)`.
  - **`title_art.cpp`:**
    - at the top, add:
      ```cpp
      #ifndef BLAZE4K_VERSION
      #error "BLAZE4K_VERSION must come from CMake project(VERSION ...)"
      #endif
      ```
    - includes: `render/theme_layout.hpp`, `render/theme_textures.hpp`, `render/gl_quad_renderer.hpp`, `gameplay/noteskin.hpp`, `<cmath>`.
- **Mirror**: `note_field_renderer.cpp:10-35` (centred quad + sprite draw), `theme_layout.hpp` (L usage), `theme.hpp:177-183` (constants).
- **Validate**: `cmake --build build -j8`.

### Task 4: Title screen render

- **Files**: `src/screens/title_screen.cpp`, `src/screens/title_screen.hpp`
- **Action**: UPDATE
- **Implement**:
  - Leave `enter()` and `update()` **unchanged**: the Confirm → Select edge, `blink_seconds_` and the log line stay as they are.
  - `render(ctx, renderer, w, h)`:
    - Return early when `w <= 0 || h <= 0` (as now).
    - Compute `const auto L = theme::layout_scale(w, h);`.
    - If `ctx.theme`, draw in this order:
      1. `draw_backdrop`
      2. `draw_centred_sprite("logo", L, kLogoTop)`
      3. `draw_centred_sprite("subtitle", L, kSubtitleTop)`
    - If `ctx.noteskin`, for each column `c` in 0..3: `draw_skin_sprite(renderer, ctx.noteskin->head(NoteType::Tap, c, arrow_quantization(c), kArrowBeat), arrow_centre(L, c), arrow_box(L))`.
    - If `ctx.theme && prompt_visible(blink_seconds_)`, draw `draw_centred_sprite("press_start", L, kPressStartTop)`.
    - If `ctx.text`, draw the footer:
      - left: `ctx.text->draw(renderer, footer_left_text(), L.x(kFooterPadX), top, text::kFooter, TextAlign::Left)`;
      - right: the same with `L.x(kRefWidth − kFooterPadX)` and `TextAlign::Right`;
      - with `top = footer_text_top(L, ctx.text->line_height(text::kFooter))`.
    - Last, if `ctx.theme`, call `draw_scanlines(*ctx.theme, renderer, w, h, L.s)`.
  - Remove `#include "render/bitmap_font.hpp"` and the six palette constants. Include `screens/title_art.hpp`, `render/theme_layout.hpp`, `render/theme_textures.hpp`, `render/ttf_font.hpp` and `gameplay/noteskin.hpp`.
  - Header comment: "Title screen (Cabinet v3, #92): bg_title, logo, subtitle, four Cel tap notes, blinking PRESS START plate, footer, scanlines. Confirm advances toward Select."
- **Mirror**: current `title_screen.cpp:28-73` structure.
- **Validate**: `cmake --build build -j8`. Then run sandboxed ctest: `screen_manager_test`, `select_screen_test` and `perf_loop_test` must still pass (all three construct `TitleScreen` with null services).

### Task 5: Attract screen render

- **Files**: `src/screens/attract_screen.cpp`, `src/screens/attract_screen.hpp`
- **Action**: UPDATE
- **Implement**:
  - Leave `enter()` and `update()` **unchanged**. Confirm stays handled by `ScreenManager`, so there are no timing changes.
  - `render`:
    - Compute `L`, `pulse = attract_pulse(phase_seconds_)` and `active = attract_active_receptor(phase_seconds_)`.
    - If `ctx.theme`, draw `draw_backdrop`, then the logo with tint `{pulse, pulse, pulse, 1}`.
    - If `ctx.noteskin`, for each column: `SkinSprite sp = ctx.noteskin->receptor(c, c == active ? 0.0 : 0.5); sp.scale = c == active ? 1.15f : 1.0f; draw_skin_sprite(renderer, sp, arrow_centre(L, c), arrow_box(L));`.
    - If `ctx.theme`, draw `press_start` with tint `{1, 1, 1, pulse}`, then `draw_scanlines`.
    - Add no subtitle and no footer (see Open Questions).
  - Remove the bitmap-font include, `kBackdropColor` / `kLogoColor` / `kReceptorColor` / `kActiveReceptorColor` and `scale_rgb`.
  - Header comment: "same bg/logo as Title (#92); pulse + receptor blink on Cel receptors".
- **Mirror**: current `attract_screen.cpp:38-74`.
- **Validate**: `cmake --build build -j8`, then sandboxed ctest.

### Task 6: `title_screen_test`

- **Files**: `tests/title_screen_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Implement**:
  - Copy the `TEST_CHECK` macro. Add `approx(a, b, eps = 1e-3f)`.
  - `test_arrow_layout`: check every Pinned Semantics arrow row (centres and box) for 1280x720 and 2560x1440 exactly (`==`). Check 3440x1440 and 1920x1200 with `approx`. Check `arrow_quantization(0..3)`, plus clamping for -1 and 4.
  - `test_centred_sprite_pos`:
    - with the real manifest content sizes (parse `kAssets/theme/cabinet/manifest.json`, or `ThemeTextures::load` headless then `content_size`), the logo, subtitle and press_start positions equal the Pinned Semantics table at 720p and 1440p exactly, and at 3440x1440 and 1920x1200;
    - `content_size("logo", 1) == {980, 190}`, `subtitle == {520, 30}` and `press_start == {440, 76}`. This proves the layout_720p in the manifest and the code agree.
  - `test_prompt_blink`: check the true/false table in Pinned Semantics, and over 0..10 s at the 60 Hz fixed dt that exactly one rising edge happens per second.
  - `test_attract_helpers`: `attract_pulse` stays within [0.30, 1.00] over a sweep, and `attract_active_receptor` gives 0,1,2,3,0 at t = 0, 0.25, 0.5, 0.75, 1.0.
  - `test_footer_text`:
    - `footer_right_text()` starts with `"BLAZE 4K v"`, equals `std::string("BLAZE 4K v") + BLAZE4K_VERSION`, and the version part matches `\d+\.\d+\.\d+` (`<regex>`);
    - `footer_left_text()` is the exact UTF-8 bytes;
    - with `TextRenderer::load(kSourceDir)`, `covers_text(footer_left_text(), Font::SairaBold)` and `covers_text(footer_right_text(), Font::SairaBold)` are both true;
    - `footer_text_top` at 720p is ≈ `680 + (40 − lh)/2`.
  - `test_skin_sprite_guard`: a default-constructed (uninitialised) `NoteSkin`'s `head(...)` and `receptor(...)` are **not** drawable, and a `SkinSprite{}` with a null texture is not drawable.
  - `test_render_smoke_with_services`:
    - Load `ThemeTextures` from `kAssets/theme/cabinet` and `TextRenderer` from `kSourceDir`, call `set_window_size` (headless), and use an uninitialised `NoteSkin` and an uninitialised `GlQuadRenderer`.
    - Put these in a `ScreenManager`'s context with Title + Attract + `SelectPlaceholderScreen`.
    - Render each screen at 1280x720, 2560x1440, 3440x1440, 1920x1200 and 0x0. There must be no crash.
    - Then check that Title + Confirm → Select, and that idle with `attract_timeout = 0.5` → Attract → Confirm → Title. These are unchanged edges.
  - `main` calls each test and prints `"title_screen_test: all passed"`.
  - `tests/CMakeLists.txt`: register after `ttf_font_test` with a `# #92:` comment, linking `blaze4k_core`, with `target_compile_definitions(title_screen_test PRIVATE BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}" BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")`.
- **Mirror**: `tests/screen_manager_test.cpp:17-24,361-386`, `tests/theme_textures_test.cpp:38-39,242`, `tests/ttf_font_test.cpp:47,484`, `tests/CMakeLists.txt:487-501`.
- **Validate**: `cmake --build build -j8`, then sandboxed ctest. Expect **47/47**.

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from the touched files
cmake --build build -j8 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides the audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 47

# Static checks
git status --short                                   # expect: only the files in "Files to Change" (+ the owner's todo-stories.md, untouched)
git diff --stat -- .agents/stories/todo-stories.md   # expect: no diff from this work
grep -n "bitmap_font" src/screens/title_screen.cpp src/screens/attract_screen.cpp   # expect: no match
grep -n "BLAZE4K_VERSION" CMakeLists.txt src/screens/title_art.cpp                  # expect: the CMake define + the #error guard/use
```

## End-to-End Verification

1. **Automated pure-path proof.** Run the sandboxed ctest command above.
   - `title_screen_test` pins the layout table and the blink and pulse rules. It checks the version string comes from CMake and that the footer glyphs are covered.
   - It also renders both screens with the real (headless) theme and text services at five sizes.
   - `screen_manager_test`, `select_screen_test` and `perf_loop_test` prove the transitions and timings are unchanged (Confirm → Select, attract timeout, Attract + Confirm → origin).
2. **Real app init (headless, safe for the agent).** Inside the same `bwrap` sandbox, from the repo root, run `./build/blaze-4k --headless --smoke-test 5 --data-dir "$(mktemp -d)"`. Expect the same lines as on `main` (`[TitleScreen] …`, `[ScreenManager] enter Title`, `Blaze 4k shut down cleanly.`). There must be **no** new NoteSkin "No GL context" line, because headless skips the init. This proves the App/NoteSkin/ScreenContext wiring links and runs in the real binary.
3. **Version plumbing.** Run `grep -o 'BLAZE4K_VERSION=[^ ]*' build/CMakeFiles/blaze4k_core.dir/flags.make`. It should print `BLAZE4K_VERSION="0.1.0"` (escaped). Use this file because the version is not printed to the console.
4. **Owner-only visual check (AC4). The agent must NOT launch the GUI**, because it opens a window and plays audio.
   - Owner runs `./build/blaze-4k`.
   - (a) At the default 1280x720 window, compare side by side with `docs/cabinet-theme/reference/cabinet-v3-title.png`. Check:
     - logo top at y 168 and centred;
     - subtitle at y 362;
     - four arrows centred at y 450 with x 478/586/694/802, coloured red/blue/purple/yellow and pointing left/down/up/right;
     - PRESS START at y 548, blinking once per second;
     - footer text in the bottom 40 px, in steel grey;
     - scanlines visible over everything.
   - (b) Maximise on a 2560x1440 display (or resize to it). The layout must be identical at 2x, with crisp logo edges (it is mipmapped, sampled at 1:1) and crisp text (the atlases re-bake).
   - (c) Leave the title idle for 120 s (or run `--attract-timeout 5`). Attract must show the same background and logo, pulsing, with Cel receptors lighting in sequence at 4 Hz, the lit one 1.15x. Press Confirm and it returns to the Title.
   - (d) Press Confirm on the Title and it goes to Select as before.
   - (e) Start a song once to confirm gameplay still loads its own Cel skin.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| An invalid NoteSkin texture draws a **solid white quad**, because the renderer substitutes its white texture (`gl_quad_renderer.cpp:280,290`). That happens when `NoteSkin::init` failed, or with a procedural fallback before init | `skin_sprite_drawable()` guard inside `draw_skin_sprite`, pinned by `test_skin_sprite_guard` | In scope |
| NoteSkin textures released after the GL context is destroyed | `noteskin_.shutdown()` in `~App` before `window_.shutdown()`, mirroring the text and theme order | In scope |
| Cel textures are loaded twice (App for the title, `GameplayView::skin_` per song), a few MB of extra VRAM | Acceptable for now and documented on `ScreenContext::noteskin`. A follow-up (natural fit: #93 gameplay HUD) can let `GameplayView` borrow App's skin | Out of scope (flag) |
| `BLAZE4K_VERSION` missing (a non-CMake build or a typo) silently shows a wrong version | `#error` in `title_art.cpp`, plus the `test_footer_text` regex and equality check | In scope |
| U+00B7 renders as a placeholder box if Saira lacks it, or if the literal is not UTF-8 | Hex-escaped UTF-8 literal, plus `covers_text` asserted against the real font in the test | In scope |
| Footer vertical position is a few px off the mock (`line_height` includes the descender, and the CSS line-height is not in the pack) | Centring formula pinned. The owner compares at step 4(a) and can nudge it by a constant in `footer_text_top` | In scope (verify) |
| Half-pixel blur at non-integer scales (1.5x) | Sprite positions rounded to whole pixels. 720p and 1440p are integer anyway | In scope |
| Headless and unit tests construct screens with null services | Every service is null-checked, and the existing tests plus the new smoke test cover both the null and the non-null path | In scope |
| Non-16:9 windows: `bg_title` stretched to the window distorts slightly. At 21:9 the baked streaks no longer line up exactly with the arrows | Follow the manifest ("stretched to the window"). The AC sizes (1280x720, 2560x1440) are 16:9, where every policy gives the same result. Raised as an Open Question | Out of scope (flag) |
| `App` now depends on `gameplay/noteskin.hpp` (layering) | One core library, a header include only. The same direction GameplayView already uses. Accept | Accept |
| The agent cannot do the AC4 visual check | Explicit owner step 4 in End-to-End Verification. The automated layout table pins every coordinate the check looks at | Owner step |

---

## Open Questions

- **Background on non-16:9 windows.** The manifest says `bg_title` is "stretched to the window". Two alternatives:
  - cover-fill (keeps the aspect, crops top and bottom on 21:9, which loses the footer fade);
  - draw into the content column with navy bands (keeps the streaks aligned with the arrows, but shows bands).

  **Proposed default:** stretch to the window, per the manifest. It is identical at the 16:9 AC sizes. Non-blocking, owner call.
- **What Attract shows beyond the AC.** The AC asks for the same background and logo plus the pulse and the receptor blink.
  - **Proposed default:** also draw the PRESS START plate with the pulse as alpha, replacing today's pulsing bitmap "PRESS START", and the scanlines overlay, so it reads as the same cabinet;
  - no subtitle and no footer, so Attract stays visibly distinct.

  Non-blocking.
- **Tap-note frame.** The issue says "fixed beat". **Proposed default:** `kArrowBeat = 0.0` (frame 0 of the Cel 2-beat cycle), static. Animating the arrows on a fake clock would be a later polish (#98). Non-blocking.
- **Version shown.** The mock says "v1.1.0", but `project(VERSION)` is 0.1.0. **Proposed default:** show `PROJECT_VERSION` (0.1.0) and do not bump the version in this PR. Owner call.
- **PRESS START blink shape.** The manifest says "blink by alpha", and the AC says "as now" (a hard 0.5 s on / 0.5 s off). **Proposed default:** keep the hard on/off (skip the draw when hidden). A smooth alpha fade would change the felt timing. Non-blocking.
- **NoteSkin ownership.** **Proposed default:** an App-owned shared instance through `ScreenContext::noteskin`, matching how `theme` and `text` reach screens. The alternative, TitleScreen/AttractScreen each loading their own on `enter()`, adds load hitches and needs GL in `enter()`. Non-blocking.
- **`TODO.md:16` ("Better general font at some point? (#55 → #90)").** The #90 plan deferred ticking it until the first screen shows TTF text, and this is that screen (the footer). **Proposed default:** tick line 16 in this PR. Leave line 17 (Cabinet theme #87–#98) unticked until #98. Owner call. `TODO.md` is not the off-limits `todo-stories.md`.

---

## Acceptance Criteria

- [ ] Title draws `bg_title`, `logo`, `subtitle`, four Cel tap notes (rotated per column, 4th/8th/12th/16th colours) at `kTitleArrowsTop`, `press_start` blinking at 1 Hz with the existing rule, and the `scanlines` overlay
- [ ] Footer shows "SINGLE · 4 PANEL" left and "BLAZE 4K v<version>" right in `text::kFooter`. The version comes from `BLAZE4K_VERSION`, set from CMake `project(VERSION …)`
- [ ] Attract uses the same background and logo and keeps its pulse and receptor-blink logic with Cel receptors
- [ ] Layout matches the Pinned Semantics table at 1280x720 and 2560x1440 (automated), and the owner's side-by-side check against `docs/cabinet-theme/reference/cabinet-v3-title.png` passes at both sizes
- [ ] Screen transitions and timings (Confirm → Select, attract timeout, Attract + Confirm → origin) are unchanged, and the existing screen tests pass
- [ ] The build has no new warnings, the sandboxed `ctest` passes 47/47, the headless smoke run is clean, and `todo-stories.md` is untouched
