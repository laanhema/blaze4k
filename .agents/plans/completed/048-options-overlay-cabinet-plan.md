# Plan: Cabinet Options Overlay (#96)

## Summary

Replace the bitmap-font options overlay that `SelectScreen::render` still draws (`src/screens/select_screen.cpp:666-717`, colour constants at `:42-47`) with Cabinet parts the song select screen already uses.
The new overlay does the following:
- dims the whole window with `theme::color::kGameplayScrim`;
- draws a centred navy panel whose header is a `bar_top` strip with an "OPTIONS" title;
- lists the seven option rows as slanted `wheel_row` slices with the selected row on the gold `wheel_row_selected` bar, names on the left (`kWheelRow` / `kWheelSelected`) and values right-aligned in `kGold`;
- redraws `bar_hint` at the bottom with an options key legend (gold arrows and keys, grey words) in the same style as select's.

A new GL-free module `src/screens/options_art.{hpp,cpp}` holds the reference-px layout and thin draw helpers, the same split as `select_art` / `results_art`.
`select_art`'s hint line builder is generalised so both legends share one layout and draw path, and select's own legend stays pixel-identical (`test_hint_layout` unchanged).
The overlay's value strings are cached in `SelectScreen` and refreshed only when the menu changes, so the restyle adds no per-frame allocations.
Input handling, the options model (`options_menu.cpp`), sounds (`ScreenManager`) and the Calibrate / Remap transitions are not touched. `options_menu_test` is not modified and must stay green.

## User Story

As a player
I want the options overlay to look like the rest of the Cabinet song select screen
So that changing my speed, scroll or calibration no longer drops me into the old bitmap UI, while every option still works exactly as before

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | MEDIUM (issue says M: one overlay restyled, one new art module, one small refactor in `select_art`) |
| Systems Affected | new `src/screens/options_art.*`, `src/screens/select_art.*` (hint line refactor), `src/screens/select_screen.*` (overlay render + value cache), `CMakeLists.txt` (core source list), `tests/select_art_test.cpp`, `tests/select_screen_test.cpp` |
| GitHub Issue | #96 (TODO-24; blocked by #94, closed; blocks #98) |
| Plan sequence | 048 (running plan sequence; the issue number is #96) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release, `/usr/bin/c++`). Build with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **48/48 pass** | Run on `main` @ `46c5b76` with the sandboxed command in Validation. The count stays **48**: no test executable is added. The new tests go into `select_art_test` (which already has the real theme and fonts) and `select_screen_test` |
| Sandbox requirement | — | `audio_test` and the screen tests open the real sound device, so **always** run ctest (and any binary) inside the `bwrap` command in Validation |
| Where the overlay lives | `src/screens/select_screen.cpp:666-717` | The issue's note says `options_menu.cpp`, but that file holds only the pure model (rows, adjust, value text). The drawing is in `SelectScreen::render`, and the input routing is in `SelectScreen::update` (`:359-463`). Only the drawing changes |
| Theme art (manifest) | `assets/theme/cabinet/manifest.json` | `bar_top` stretch_x 132 @2x (64 + 2 rule), **opaque**. `bar_hint` stretch_x 108 @2x, **opaque** (so redrawing it over the scrim fully hides select's legend). `wheel_row` slice3, content 1128x124 @2x = **564x62 ref**, caps 68/68, uniform k = h / C.h (the slant is kept at any height). `wheel_row_selected` **sprite** (no slice info), content 1280x184 @2x = **640x92 ref**, so stretching changes its slant unless the aspect is kept. `stat_panel` slice3 is only 96 ref tall; scaling it to a 500+ px panel would blow up its −8° slant and caps, so it is **not** used for the body |
| Theme constants | `src/render/theme.hpp:31, 43-44, 123-125, 133-134, 172-175` | `color::kGameplayScrim` (0x000000 @ 0.72, the same value as today's `kDimOverlay`), `kGold`, `kSelectedInk`, `kNavyPanel`; `text::kWheelRow`, `kWheelSelected`, `kWheelPack`, `kHintKey`, `kHintWord`, `kSongTitle`; `layout::kTopBarHeight 64`, `kHintBarHeight 52`, `kHintGap 34` |
| Pre-baked atlases | `theme.hpp:154-161` (`kAllStyles`, 26 entries, pinned by `tests/ttf_font_test.cpp:810`) | TextRenderer bakes one atlas per (font, size_px) in `kAllStyles`. Every style this plan uses is in it already, or shares its (font, size) (the title: SairaExtraBold 44 = `kSongTitle`). **Do not** add to `kAllStyles` (that would break the count pin and add a bake) |
| Text widths (PIL, real fonts, ref px @720p) | `assets/fonts/SairaCondensed-*.ttf` | Widest name "CALIBRATE OFFSET": 184 (`kWheelRow` 28), 265 (`kWheelSelected` 40). Widest value: the config allows an offset of ±3600 s (`config_loader.cpp:208`), so "-3600.000 s" is 157 (`kWheelPack` 28, tracking 3) and 177 (`kWheelSelected` 40). "OPTIONS" in SairaExtraBold 44 is ≈ 138 + tracking |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #96

| Where | Note | This plan |
|---|---|---|
| `src/screens/select_screen.cpp:42` | "Options overlay colours (bitmap font; the overlay's Cabinet restyle is #96)" | **Removed** with the five colour constants (`kTitleColor`, `kSelectedColor`, `kTextColor`, `kHintColor`, `kPlaceholderColor`) |
| `src/screens/select_screen.cpp:669` | "Options overlay over the Cabinet screen (bitmap font until #96)" | **Replaced** by the `options_art::draw_overlay` call |
| `.agents/plans/completed/045-select-screen-cabinet-plan.md:447` | `grep -n "draw_text(" select_screen.cpp`: "only inside the options-overlay block (#96 restyles it)" | After this plan the grep returns **no hits**, and `render/bitmap_font.hpp` is no longer included by `select_screen.cpp` |
| `.agents/reviews/feature-045-select-screen-cabinet-review-r1.md:46` | "per-frame allocations (#98), the bitmap options overlay (#96)" | The overlay part is closed. This plan adds no per-frame allocations (value cache). #98 still owns the existing select-screen allocations |
| `docs/cabinet-theme/IMPLEMENTATION_PLAN.md:103-106` | "Dim with 72% black (`kGameplayScrim`), then a centred panel: a `stat_panel`-style plate or `bar_top` as the header. Use `diff_row`-style slanted rows (or `wheel_row`) … the selected row in gold (`wheel_row_selected`), and values right-aligned in `kGold`" | Followed: `bar_top` header, `wheel_row` rows, `wheel_row_selected` selection, `kGold` values |
| #97 (remap / calibration restyle, part 2 of the same plan step) | Not part of this issue | `options_art::panel_rect` / `draw_panel` are public so #97 can reuse the panel and header. Nothing else is built for it |

---

## Patterns to Follow

### Screen render: layout scale, null-checked services

```cpp
// SOURCE: src/screens/select_screen.cpp:563-576
void SelectScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;
```

### Art module: constants + pure reference-px layout + thin draw helpers

```cpp
// SOURCE: src/screens/select_art.hpp:1-13
// Cabinet v3 Song Select art (#94), used by SelectScreen.
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so select_art_test
//    pins them headless. Every rect is a manifest *content box*; glow padding
//    hangs outside it.
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
```

### Row art + text grouped by style (copy the wheel's two-pass order)

```cpp
// SOURCE: src/screens/select_art.cpp:530-595
case WheelArt::Song:     theme.draw_slice3(renderer, "wheel_row", dst); break;
case WheelArt::Selected: theme.draw_stretch(renderer, "wheel_row_selected", dst); break;
...
text->draw(renderer, ..., L.x(text_x), centred_top(*text, L, r.y, r.h, style), style, TextAlign::Left);
```

### Text helpers (public in `ttf_font.hpp:292-306`; copy `centred_top` / `draw_solid` privately)

```cpp
// SOURCE: src/render/ttf_font.hpp:293-306, src/screens/select_art.cpp:47-68
constexpr theme::TextStyle with_color(theme::TextStyle style, Color color);
inline float ref_measure(const TextRenderer& text, std::string_view s, const theme::TextStyle& style);
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h, const theme::TextStyle& style);
void draw_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L, const std::array<Vec2, 4>& ref_corners, Color color);
```

### Hint bar (to be split into a reusable builder + line drawer)

```cpp
// SOURCE: src/screens/select_art.cpp:235-274 (hint_layout lambdas add_arrows/add_key/add_word),
//         src/screens/select_art.cpp:360-396 (draw_hint_bar: bar_hint, arrows via draw_solid, text)
```

### Error handling (presentation code: null-guard, never throw)

```cpp
// SOURCE: src/screens/select_art.cpp:398-411 (draw_banner: theme may be null, flat fallback)
if (theme != nullptr) { theme->draw_frame(renderer, "banner_frame", hole); }
```

There are no exceptions or error codes on this path. A missing service skips its draws, a missing texture falls back inside `ThemeTextures`, and the renderer is a no-op when uninitialised (headless tests).

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real assets headless)

```cpp
// SOURCE: tests/select_art_test.cpp:37-113, 596-628, 664-752
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
blaze4k::ThemeTextures& loaded_theme();   // real manifest, headless
blaze4k::TextRenderer& loaded_text();     // real fonts, set_window_size(1280, 720)
TEST_CHECK(rect_eq(r, x, y, w, h));       // approx 1e-3
```

---

## Pinned Semantics (the contract the tests pin)

All `options_art` values are **reference px** (1280x720). Screen px = `L.x / L.y / L.px / L.rect` with `L = theme::layout_scale(w, h)`.

### Geometry (`options_art.hpp` constants)

| Constant | Value | Derivation |
|---|---|---|
| `kPanel` | `{320, 95, 640, 542}` | width = row 560 + 2 × 40 padding, centred on x 640. Height = header 66 + 24 + rows 428 + 24. Centred vertically in the free band between the select top bar (66) and `select_art::kHintBarTop` (666): 66 + (600 − 542) / 2 = 95 |
| `kHeaderHeight` | `66` | `bar_top` content 132 @2x (64 + 2 black rule), drawn with `draw_stretch_x` at `{panel.x, panel.y, panel.w}` |
| `kTitleX` | `panel.x + 24 = 344` | Title "OPTIONS" left-aligned, line box centred in the 64 px bar band `[95, 159]` |
| `kTitleStyle` | `{SairaExtraBold, 44, 4, italic, kWhite, Hard3}` | Same (font, size) as `kSongTitle` (pre-baked), with tracking added to echo the wide-tracked `title_select_music` sprite. Tracking does not need a new atlas |
| `kRowX` / `kRowWidth` | `360` / `560` | panel.x + 40 |
| `kRowHeight` / `kRowSelectedHeight` | `48` / `80` | `80 / 560 = 0.1429` vs the sprite's `92 / 640 = 0.1437` (0.6 % off), so the stretched gold bar keeps its −14° slant. `wheel_row` is slice3 (uniform k), so 48 keeps the slant automatically |
| `kRowGap` / `kRowPitch` | `10` / `58` | |
| `kRowsTop` | `panel.y + 66 + 24 = 185` | |
| `row_rect(row, selected)` | `{360, 185 + row·58 + (row > selected ? 32 : 0), 560, row == selected ? 80 : 48}` | `row` and `selected` are clamped to `[0, kOptionsRowCount − 1]`. Last row bottom = 185 + 6·58 + 32 + 48 = **613** for every selection, so the panel's bottom padding is 24 |
| Name x | `row.x + select_art::kWheelSongTextX (26)`; selected `row.x + select_art::kWheelSelectedTextX (30)` | Same insets as the wheel |
| Value right edge | `row.x + row.w − 30`; selected `− 36` | Clears the right slant (0.249 · h/2 ≤ 10 px) plus the italic overhang |
| `kNameValueGap` | `24` | Minimum gap between a name's end and its value's start (pinned with real fonts) |
| Panel body | solid `kNavyPanel` quad over `kPanel`, plus a 2 px `select_art::kEditEdge` ring | Code-drawn, unslanted (see the `stat_panel` finding) |

Pinned checks: `kPanel.y ≥ kTopBarHeight + 2 (66)`, `kPanel.y + kPanel.h ≤ select_art::kHintBarTop (666)`, every `row_rect` lies inside `[kRowsTop, kPanel bottom − 24]`, and rows never overlap. This is the "no regression of the options room fixes" AC: today rows share a band so a new row shrinks the spacing. Now a `static_assert` ties `kOptionsRowCount` to the height budget, so adding an eighth row fails to compile until the layout is revisited.

### Text and colours

| Element | Style | Colour |
|---|---|---|
| Title "OPTIONS" | `kTitleStyle` | `kWhite` + hard shadow |
| Unselected name | `theme::text::kWheelRow` | `kWheelText` |
| Unselected value | `with_color(theme::text::kWheelPack, kGold)` (SairaExtraBold 28, tracking 3) | **`kGold`**, right-aligned |
| Selected name | `theme::text::kWheelSelected` (40, italic) | `kSelectedInk` |
| Selected value | `theme::text::kWheelSelected`, right-aligned | `kSelectedInk` (gold on the gold bar would be unreadable; Open Question 1) |
| Legend | `kHintKey` / `kHintWord` + gold code-drawn arrows | as select |

Names come from `options_row_name(i)`, built once into a function-local static array (`options_art::row_names()`). Values come from the `SelectScreen` cache (below). Neither path allocates per frame. If a name or value would collide with the other (`measure > budget`, only possible with an absurd value), `text->truncate` runs on that rare path only. The fit test proves it never triggers for real values.

### Legend (`options_art::hint_layout`)

`[↑↓] ROW  [←→] CHANGE  ENTER NEXT  ESC CLOSE`: 10 pieces, centred on x 640 in select's hint band (`kHintBandTop 668`, height 52). It uses the same arrow cells, key → word gap (8), arrow → word gap (10) and pair gap (`kHintGap` 34) as select. These are today's words (`[UP/DOWN] ROW [LEFT/RIGHT] CHANGE [ENTER] NEXT [BACK] CLOSE`), with BACK shown as ESC to match select's "ESC TITLE" (Open Question 2).

### Draw order in `SelectScreen::render`

1. Unchanged: backdrop, banner, song info, difficulty rows, wheel, top bar + chips, select hint bar, empty message.
2. **If `options_open_`**: `options_art::draw_overlay(theme, text, renderer, L, w, h, options_values_, options_.row)`:
   1. scrim `kGameplayScrim` over `{0, 0, w, h}` (the whole window, letterbox bands included, as today);
   2. panel body + ring (solid), `bar_top` header (theme only);
   3. unselected row art (`draw_slice3("wheel_row")`), then the selected bar (`draw_stretch("wheel_row_selected")`) (theme only);
   4. `bar_hint` at the bottom (as `select_art::draw_hint_bar` does it), then the legend arrows;
   5. text grouped by style: title, unselected names, unselected values, selected name + value, legend keys/words (text only).
3. **Scanlines last** (moved from before the overlay to after it, so the overlay gets the same CRT overlay as every other Cabinet surface). When the overlay is closed, the frame is identical to today's.

### Value cache in `SelectScreen`

- Members: `std::array<std::string, kOptionsRowCount> options_values_{}` and `bool options_values_dirty_ = true`.
- `refresh_options_values()`: `options_values_[i] = options_row_value_text(options_, i)` for every row, then clears the dirty flag.
- The flag is set dirty when the overlay opens (wheel-mode `Options`) and on **every** pressed event the overlay branch handles. `update()` refreshes at its end when `options_open_ && options_values_dirty_`. `enter()` refreshes directly on the reopen path (so a freshly calibrated offset shows on the first frame back).
- `render()` only reads the cache.
- Test accessor: `[[nodiscard]] const std::array<std::string, kOptionsRowCount>& options_values() const`.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/options_art.hpp` | CREATE | Constants (Pinned Semantics), `panel_rect()`, `header_rect()`, `row_rect(row, selected)`, `name_x(row_rect, selected)`, `value_right(row_rect, selected)`, `row_names()`, `hint_layout(measure_key, measure_word)`, and the draw helpers `draw_panel`, `draw_rows`, `draw_hint_bar`, `draw_overlay` |
| `src/screens/options_art.cpp` | CREATE | Their implementation. Private `centred_top` / `draw_solid` copies (as `results_art` did) |
| `src/screens/select_art.hpp` / `.cpp` | UPDATE | Generalise the legend: a public `HintItem {Kind: VArrows, HArrows, Key, Word; text}`, `HintLine layout_hint_items(std::span<const HintItem>, measure_key, measure_word)` (guards `count ≤ kHintPieceCount`) and `void draw_hint_line(TextRenderer&, GlQuadRenderer&, const LayoutScale&, const HintLine&)`. `hint_layout` and `draw_hint_bar` become thin wrappers with **identical output** |
| `src/screens/select_screen.hpp` | UPDATE | Value cache members, `refresh_options_values()`, `options_values()` accessor. Class comment: the overlay is drawn by `options_art` (#96) |
| `src/screens/select_screen.cpp` | UPDATE | Delete the five overlay colours, the `render/bitmap_font.hpp` include and the inline overlay block. Call `options_art::draw_overlay`. Move scanlines to the end. Set and refresh the dirty flag in `update` / `enter`. Input routing is otherwise **byte-identical** |
| `CMakeLists.txt` | UPDATE | Add `src/screens/options_art.cpp` to the core sources after `src/screens/select_art.cpp` (line 130) |
| `tests/select_art_test.cpp` | UPDATE | Options layout, text-fit, legend, style-prebake tests. The render smoke walks every overlay row |
| `tests/select_screen_test.cpp` | UPDATE | The value cache follows adjustments and the calibration round trip |

`options_menu.{hpp,cpp}`, `tests/options_menu_test.cpp`, `theme.hpp`, the manifest, `ScreenManager` (sounds) and `ScreenContext` are **unchanged**.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Build after each with `cmake --build build -j$(nproc)`.

### Task 1: Generalise the select hint line (refactor, no visual change)

- **File**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `struct HintItem { enum class Kind { VArrows, HArrows, Key, Word }; Kind kind; std::string_view text; };`.
  - Add `HintLine layout_hint_items(std::span<const HintItem> items, const HintMeasure& measure_key, const HintMeasure& measure_word)`. It reproduces today's lambdas: `VArrows` → Up/Down pair (cell 4, pitch 16), `HArrows` → Left/Right pair (cell 14, pitch 24), each followed by `kHintArrowKeyGap`; `Key` + `kHintTextKeyGap`; `Word` + `kHintGap` unless it is the last item. It then centres on `kHintCentreX` and stops adding pieces at `kHintPieceCount`.
  - `hint_layout(...)` = `layout_hint_items(kSelectHintItems, ...)` with a file-local `constexpr std::array<HintItem, 9> kSelectHintItems` (↑↓ SONG, ←→ DIFFICULTY, ENTER PLAY, TAB OPTIONS, ESC TITLE).
  - Extract `draw_hint_line(TextRenderer& text, GlQuadRenderer&, const theme::LayoutScale&, const HintLine&)` from `draw_hint_bar` (arrows via `draw_solid`, then keys/words). `draw_hint_bar` keeps its exact behaviour: bar, `return` if `text == nullptr`, `draw_hint_line(*text, …, hint_layout(...))`.
- **Mirror**: `src/screens/select_art.cpp:235-274, 360-396`
- **Validate**: `cmake --build build -j$(nproc)`, then the sandboxed ctest `-R select_art_test` (the existing `test_hint_layout` must pass **unmodified**: it is the regression guard)

### Task 2: `options_art` pure layout

- **File**: `src/screens/options_art.hpp`, `src/screens/options_art.cpp`, `CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**:
  - Namespace `blaze4k::options_art`. Header comment in the `select_art.hpp:1-13` style: Cabinet options overlay (#96), a free design (not in the mock-ups) built from select's parts; pure reference-px layout + thin draw helpers; presentation only, no clock.
  - Every constant from Pinned Semantics as `inline constexpr`, each with a one-line derivation comment.
  - `static_assert`s:
    - `kPanel.y >= theme::layout::kTopBarHeight + 2`;
    - `kPanel.y + kPanel.h <= select_art::kHintBarTop`;
    - `kRowsTop + (kOptionsRowCount − 1) · kRowPitch + (kRowSelectedHeight − kRowHeight) + kRowHeight + 24 == kPanel.y + kPanel.h`. This is the room budget: an eighth row fails here.
  - `Rect panel_rect()`, `Rect header_rect()`, `Rect row_rect(int row, int selected)` (clamped inputs), `float name_x(const Rect& row, bool selected)`, `float value_right(const Rect& row, bool selected)`, `const std::array<std::string, kOptionsRowCount>& row_names()` (function-local static from `options_row_name`).
  - `select_art::HintLine hint_layout(const select_art::HintMeasure& key, const select_art::HintMeasure& word)` via `select_art::layout_hint_items` with `{VArrows, "ROW"}, {HArrows, "CHANGE"}, {Key "ENTER"}, {Word "NEXT"}, {Key "ESC"}, {Word "CLOSE"}` (6 items → 10 pieces).
  - Add `src/screens/options_art.cpp` to `CMakeLists.txt` after `select_art.cpp`.
- **Mirror**: `src/screens/select_art.hpp:38-120`, `src/screens/select_art.cpp:104-124`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 3: `options_art` draw helpers

- **File**: `src/screens/options_art.hpp`, `src/screens/options_art.cpp`
- **Action**: UPDATE
- **Implement** (like `select_art`, the helpers rely on `GlQuadRenderer`'s own no-op when it is uninitialised, which is how the headless tests run; services are pointers and null-guarded):
  - `draw_panel(const ThemeTextures*, TextRenderer*, GlQuadRenderer&, L)`: solid `kNavyPanel` body, the 2 px `kEditEdge` ring (four solid strips), `bar_top` via `theme->draw_stretch_x(renderer, "bar_top", L.x(kPanel.x), L.y(kPanel.y), L.px(kPanel.w), L.s)`, and the title "OPTIONS".
  - `draw_rows(const ThemeTextures*, TextRenderer*, GlQuadRenderer&, L, std::span<const std::string> values, int selected)`:
    - art: unselected `draw_slice3("wheel_row", L.rect(r))`, then the selected `draw_stretch("wheel_row_selected", L.rect(r))`;
    - text in style groups (names, values, selected pair). Names left at `name_x`. Values right-aligned at `value_right`.
    - Collision guard: when `ref_measure(name) > value_right − value_w − kNameValueGap − name_x`, draw `text->truncate(name, style, L.px(budget))` (rare path only).
    - `values.size() < kOptionsRowCount` draws empty values (defensive).
  - `draw_hint_bar(const ThemeTextures*, TextRenderer*, GlQuadRenderer&, L, int w)`: `bar_hint` exactly as `select_art::draw_hint_bar` places it, then `select_art::draw_hint_line(*text, …, hint_layout(...))` when text is non-null.
  - `draw_overlay(theme, text, renderer, L, w, h, values, selected)`: the scrim `renderer.draw_quad(Rect{0, 0, w, h}, theme::color::kGameplayScrim)`, then `draw_panel`, `draw_rows`, `draw_hint_bar`, in the pinned order.
- **Mirror**: `src/screens/select_art.cpp:330-396, 530-595`, `src/screens/results_art.cpp:256-290`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 4: `SelectScreen` uses the new overlay

- **File**: `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Delete `kTitleColor`, `kSelectedColor`, `kTextColor`, `kHintColor`, `kPlaceholderColor` and their comment (`:42-47`), the `render/bitmap_font.hpp` include and the overlay block (`:666-717`). Add `#include "screens/options_art.hpp"`.
  - `render`: after `draw_empty_message`, `if (options_open_) options_art::draw_overlay(...)`. Then `if (theme != nullptr) select_art::draw_scanlines(...)` as the final draw.
  - Value cache per Pinned Semantics:
    - set `options_values_dirty_ = true` at the wheel-mode `Options` case (`:465-470`) and once at the top of the overlay branch for every pressed event it receives (`:359`);
    - `if (options_open_ && options_values_dirty_) refresh_options_values();` right after the event loop (before `refresh_chips`);
    - `refresh_options_values()` in `enter()`'s reopen path.
  - **No other change** to `update`, `handle_back` or `exit`.
  - Header: members, the private `refresh_options_values()`, the public `options_values()` accessor, and the class comment line "the options overlay is drawn by options_art (#96)".
- **Mirror**: `src/screens/select_screen.cpp:214-226` (`refresh_chips`: the same cache-on-change idea)
- **Validate**: `cmake --build build -j$(nproc)` and `grep -n "draw_text\|bitmap_font" src/screens/select_screen.cpp` (expect no hits)

### Task 5: `select_art_test` additions

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement** (add `#include "screens/options_art.hpp"` and `screens/options_menu.hpp`, and register each test in `main`):
  - `test_options_layout`:
    - `panel_rect() == {320, 95, 640, 542}` and `header_rect() == {320, 95, 640, 66}`.
    - `row_rect(0, 0) == {360, 185, 560, 80}`, `row_rect(1, 0) == {360, 275, 560, 48}`, `row_rect(6, 0)` bottom == 613, `row_rect(6, 6) == {360, 533, 560, 80}`.
    - For every `selected` in 0..6: rows are inside `[185, 613]`, `row[i].y + row[i].h < row[i+1].y`, and exactly one row is 80 tall.
    - Panel inside `[66, select_art::kHintBarTop]`.
    - `|80/560 − 92/640| / (92/640) < 0.02`.
    - At 2560x1440, `L.rect(row_rect(0, 0)) == {720, 370, 1120, 160}`. At 3440x1440 the panel is centred on x 1720.
    - Out-of-range `row` / `selected` (−1, 99) clamp.
  - `test_options_text_fits` (`loaded_text()` at 1280x720): for every row, selected and unselected, with the row's widest real values ("XMOD"/"CMOD"/"MMOD"; "2.5x"/"C9999"/"M9999"; "DOWN"; "OFF"; "OFF"; "-3600.000 s" and "+0.023 s"; ">"), check `name_x + ref_measure(name) + kNameValueGap ≤ value_right − ref_measure(value)` in the pinned styles. Also `ref_measure("OPTIONS", kTitleStyle) < kPanel.w − 48`. The title's line height ≤ 64 (it fits the bar band).
  - `test_options_hint_layout`: with the real measures, `count == 10`; kinds/texts in order (Arrow Up, Arrow Down, Word ROW, Arrow Left, Arrow Right, Word CHANGE, Key ENTER, Word NEXT, Key ESC, Word CLOSE); centred (`pieces[0].x + width / 2 == 640` ± 1e-3); `width < 1280 − 2 · 40`. Also `layout_hint_items` with 20 Word items stops at `kHintPieceCount`.
  - `test_options_styles_prebaked`: for `kTitleStyle`, `kWheelRow`, `kWheelPack`, `kWheelSelected`, `kHintKey` and `kHintWord`, some `kAllStyles` entry has the same `(font, size_px)`.
  - Extend `test_render_smoke`: after opening the overlay, render all sizes, then press `Down` 7 times (rendering after each, so every row is the selection, and clamped at the end), then `Up` once, then `Back`. This runs for both `services` true and false. Update the file's header comment ("options overlay in the Cabinet look (#96)").
- **Mirror**: `tests/select_art_test.cpp:174-222, 536-594, 664-752`
- **Validate**: sandboxed ctest `-R select_art_test`

### Task 6: `select_screen_test` cache checks

- **File**: `tests/select_screen_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `test_options_overlay`, after each adjustment, assert `select->options_values()` matches `options_row_value_text(select->options_menu(), row)` for every row. Pin the concrete cases: `[SpeedType] == "CMOD"`, `[SpeedValue] == "C400"`, `[Scroll] == "DOWN"`, `[Fail] == "OFF"`.
  - In `test_calibration_launch_from_options`, after returning with the overlay reopened, assert `options_values()[CalibrateOffset] == format_offset(config.offset.global_offset_seconds)`.
  - Leave every existing assertion untouched.
- **Mirror**: `tests/select_screen_test.cpp:611-675`
- **Validate**: sandboxed ctest `-R "select_screen_test|options_menu_test"`

### Task 7: Full validation

- Run the full Validation block. **48/48** must pass, and the build adds no warnings.

---

## Validation

```bash
# Build (host, existing build dir)
cmake --build build -j$(nproc)

# Lint: no linter configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning" | grep -E "options_art|select_art|select_screen" || echo "no new warnings"

# Tests (MUST run sandboxed: the tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **48/48 pass** (baseline 48/48 on `46c5b76`), including `options_menu_test` (unmodified), `select_screen_test` and `select_art_test`.

Static checks:

```bash
# The bitmap overlay is gone from select
grep -nE "draw_text|bitmap_font|kPlaceholderColor|kDimOverlay" src/screens/select_screen.cpp   # expect no hits
# Nothing in the overlay reads a clock
grep -nE "steady_clock|SDL_GetTicks|music_clock|time_seconds" src/screens/options_art.cpp src/screens/select_screen.cpp   # expect no hits
# The options model and its test are untouched
git diff --stat -- src/screens/options_menu.cpp src/screens/options_menu.hpp tests/options_menu_test.cpp   # expect empty
```

## End-to-End Verification

1. **Automated (agent-runnable):** `select_screen_test` drives the real overlay end to end (open → adjust → close → Confirm into Gameplay → persistence → Calibration and Remap launches and the Esc return to the reopened overlay, all unchanged) and now also checks the value cache. `select_art_test`'s render smoke draws every overlay row as the selection with the real manifest and fonts at six window sizes and with null services. Both must pass under the sandboxed ctest.
2. **Headless app smoke (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and no new error lines (the new TU links and the select screen starts).
3. **Windowed visual check (owner; the agent must not launch the GUI, since it opens a window and plays audio):** start the game, go to song select, press Tab.
   - The select screen dims (72% black). A centred navy panel shows a chrome `bar_top` header with an italic white "OPTIONS".
   - Seven slanted navy rows (SPEED TYPE, SPEED, SCROLL, FAIL, ASSIST TICK, CALIBRATE OFFSET, REMAP INPUT), names on the left and gold values on the right. The selected row is the gold/orange bar with dark italic text, at the same slant as the other rows.
   - The bottom bar reads "↑↓ ROW  ←→ CHANGE  ENTER NEXT  ESC CLOSE" with gold arrows and keys. No row touches it, and nothing of select's own legend shows through.
   - Up/Down move the gold bar (with the move sound), and Left/Right/Enter change values (they update immediately; the SPEED / SCROLL chips change after closing). Tab or Esc closes.
   - On CALIBRATE OFFSET, Enter opens the calibration wizard, and Esc returns to the overlay on that row with the offset shown. The same holds for REMAP INPUT (the preview keeps playing).
   - Repeat maximised (2560x1440 if available) and in a 4:3 or 21:9 window: the panel stays centred and sharp, and the scanlines cover the overlay.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Stretching the `wheel_row_selected` sprite changes its baked slant | The selected row keeps the sprite's aspect (560x80 vs 640x92, 0.6% off), pinned by a test | In scope |
| Text collides or overflows (long names, the ±3600 s offset) | Widths measured with the real fonts. `test_options_text_fits` pins every row with its widest real value. A rare-path `truncate` guards anything else | In scope |
| Adding a future option row silently overlaps the legend | A `static_assert` ties the row count to the height budget, plus the panel-inside-bars test | In scope |
| Stale value text (cache not refreshed after an adjust or after calibration) | Dirty flag set on every overlay press and on open, refreshed in `update` and on the `enter` reopen path. `select_screen_test` pins it, including the calibration round trip | In scope |
| The refactor changes select's own legend | `layout_hint_items` reproduces the old lambdas exactly. The existing `test_hint_layout` must pass unmodified | In scope |
| A new text style triggers a mid-screen atlas bake | Every style shares a pre-baked (font, size). `test_options_styles_prebaked` pins it, and `kAllStyles` is not edited (the count pin stays 26) | In scope |
| Moving scanlines after the overlay changes the closed-overlay frame | Order is unchanged when the overlay is closed (scanlines were already the last draw then) | In scope |
| Behaviour regression (input, sounds, transitions) | `update`'s routing, `handle_back`, `exit` and `ScreenManager` are not edited. `options_menu_test` and the select overlay / calibration / remap tests stay unchanged and green | In scope |
| Visual tuning (insets 26/30, value insets 30/36, panel padding) looks off in the real window | Owner windowed check. Every value is one named constant | In scope (flag) |
| Remap / Calibration screens still use the old look | That is #97. `draw_panel` is public for reuse | Out of scope |
| Existing per-frame allocations on the select screen | Not made worse (the cache and static names). The remaining ones are #98's | Out of scope |

---

## Open Questions

None of these block the work. The plan already uses each recommended default.

1. **Value colour on the selected (gold) row.** The issue says values are gold. On the gold selected bar, gold text would be invisible.
   **Recommendation:** gold values on every other row, and dark text (the same ink as the song name on the selected wheel bar) on the selected row.
2. **The words in the bottom legend.** Today's overlay says "[UP/DOWN] ROW [LEFT/RIGHT] CHANGE [ENTER] NEXT [BACK] CLOSE".
   **Recommendation:** keep the same words in the new style, with arrows drawn as on the select screen and BACK written as ESC (the select screen already says "ESC TITLE"). Tab also closes the overlay, but it is left out to keep the line short.
3. **Where the legend sits.** The old overlay had its key line inside the panel.
   **Recommendation:** put it in the normal bottom bar, the same place as on every other Cabinet screen. The select screen's own legend there is covered while options are open, which is right because those keys do something else then.

---

## Acceptance Criteria

- [ ] Opening options dims the select screen with `theme::color::kGameplayScrim` and shows a centred panel with a `bar_top` header ("OPTIONS")
- [ ] Each option row is a slanted `wheel_row`. The selected row is the gold `wheel_row_selected` bar. Values are right-aligned in `kGold` (dark ink on the selected bar; Open Question 1)
- [ ] All rows fit at 1280x720 between the top bar and the hint bar without overlapping the legend (pinned by tests and a `static_assert`)
- [ ] Option behaviour, sounds and transitions (to Calibrate / Remap and back with Esc) are unchanged; `options_menu_test` (unmodified) passes
- [ ] Select's own legend is unchanged (`test_hint_layout` unmodified and green)
- [ ] 48/48 tests pass under the sandboxed ctest command; no new compiler warnings
- [ ] No bitmap text, clock reads or new per-frame allocations in the overlay
- [ ] `.agents/stories/todo-stories.md` untouched
