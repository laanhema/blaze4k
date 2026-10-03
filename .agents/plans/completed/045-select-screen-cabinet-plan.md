# Plan: Cabinet Song Select Screen (#94)

## Summary

Restyle the Song Select screen with the Cabinet v3 look (`docs/cabinet-theme/reference/cabinet-v3-select.png`). It is the third screen on the theme services: #89 (`ThemeTextures`), #90 (`TextRenderer`) and #91 (`theme::layout_scale`). #92 (Title/Attract) is the model to copy.

**What the screen draws**, back to front (the order keeps texture flushes low and lets the bars cover wheel rows while they slide):

1. `bg_select`, stretched to the window.
2. The banner: the song's banner image (or `banner_fallback`) stretched into the `kBanner` hole, then `banner_frame` on top.
3. Song title (`kSongTitle`), artist (`kArtist`) and "BPM 140-175" (`kBpm`, right-aligned to the banner edge) below the banner. The title and artist are truncated to a measured width.
4. Difficulty rows: `diff_row_<name>[_selected]` for Beginner…Challenge, and a code-drawn neutral parallelogram row for Edit (#84). Each row has the name, the meter, 10 `diff_tick` sprites (lit up to the meter in the difficulty colour, else `kTickOff`) and the best % ("---" when there is none). At most 5 rows show. A song with more charts gets a sliding window around the selected chart.
5. The wheel: `wheel_pack` header rows inline with the songs (StepMania-style sections), `wheel_row`, and `wheel_row_selected`. Rows are indented with `kWheelIndent` by their distance from the selection. Titles only, no artist. 7 rows show (the count comes from the layout). The window logic is the same centred-and-clamped algorithm used today. When the window moves, the rows slide vertically with an ease-out over 80 ms, advanced on the fixed `dt`.
6. `bar_top` + `title_select_music`, and the speed/scroll chips (`chip` tinted `kCyan` / `kGreen`, with text such as "SPEED 2.5x", "UPSCROLL").
7. `bar_hint` with "↑↓ SONG  ←→ DIFFICULTY  ENTER PLAY  TAB OPTIONS  ESC TITLE". Keys are gold and words grey. The font has no arrow glyphs, so the arrows are small solid quads.
8. The `scanlines` overlay (the manifest lists it for title, select and score screens).

**Code layout.** Pure layout and draw helpers go in a new `src/screens/select_art.{hpp,cpp}`, mirroring `title_art`. They are GL-free and unit-tested in a new `select_art_test`. `SelectScreen::render` becomes a thin sequence of calls to them.

**What does not change.** Navigation, the clamped chart cursor, held-key repeat, the preview, Confirm → Gameplay, the options overlay's input handling, and Back are not touched. The existing `select_screen_test` behaviour tests keep passing. The options overlay is still drawn by today's bitmap code (its restyle is #96), now over the Cabinet screen instead of black.

## User Story

As a player choosing a song
I want the select screen to look like the Cabinet mock-up (chrome banner, slanted difficulty rows with meter ticks, a curved slanted wheel, gold key hints)
So that browsing my packs feels like a finished arcade cabinet, sharp at 720p and at 1440p, with no change to how navigation works.

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | HIGH (issue says Large: one 700-line screen rewritten, one new helper TU, one new test, small API additions in three modules) |
| Systems Affected | `src/screens/select_screen.*`, new `src/screens/select_art.*`, `src/screens/song_display_text.*` (TTF coverage and untruncated label), `src/screens/options_menu.*` (expose `format_speed_mod`), `src/render/theme.hpp` (two gap constants), `CMakeLists.txt`, `tests/` (new `select_art_test`, updated `select_screen_test`) |
| GitHub Issue | #94 (TODO-22; blocked by #89, #90, #91, all merged; blocks #96, #97, #98) |
| Plan sequence | 045 (running plan sequence; the issue number is #94) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **47/47 pass** | Run on `main` @ `6a98fe4` with the sandboxed command in Validation. Start green, stay green. After this change the count is **48** (`select_art_test` added) |
| Sandbox requirement | — | `audio_test` opens the real sound device, so **always** run ctest (and any binary) inside the `bwrap` command |
| Headless select smoke | `./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <tmp>` | On `main` (inside bwrap) it prints `[SelectScreen] library: 221 songs, 1116 charts`, `[ScreenManager] enter Select`, `Blaze 4k shut down cleanly.` (verified). The repo's `songs/` holds ITG 1–3, so some songs have more than 5 charts |
| Reference image | `docs/cabinet-theme/reference/cabinet-v3-select.png` (1280x720) | The issue says `reference/…`; the file lives under `docs/cabinet-theme/` |
| Select art (manifest) | `assets/theme/cabinet/manifest.json` | `bg_select` fullscreen. `bar_top` stretch_x, content 32x132 @2x = 66 ref tall (64 + 2 px rule). `bar_hint` stretch_x, content 108 @2x = 54 ref (52 + 2 px rule on top). `title_select_music` sprite, layout_720p `[40,9,300,46]`. `chip` slice3 (caps 44/44), "multiply by the chip colour". `banner_frame` frame, layout `[44,96,568,165]`, hole 560x157. `banner_fallback` sprite 560x157 ref. `diff_row_<name>` slice3 content 564x44 ref; `_selected` content 564x52 ref (gold frame + glow in the padding). `diff_tick` sprite 20x18 ref, tint multiply. `wheel_pack` / `wheel_row` slice3 content 564x62 ref. `wheel_row_selected` **sprite**, fixed 640x92 ref. `scanlines` 1x3 tile, screen-pixel |
| Diff-row textures | beginner, easy, medium, hard, challenge (+ `_selected`) | **No Edit texture.** Edit is code-drawn (issue technical note) |
| Theme constants | `src/render/theme.hpp:186-204` | `kBanner{48,100,560,157}`, `kSongTitleTop 270`, `kArtistTop 326`, `kDiffListX 44`, `kDiffListTop 372`, row 44 / selected 52, `kDiffRowGap 8`, `kDiffRowSelectedShiftX 14`, `kDiffTickPitch 18`, `kWheelX 676`, `kWheelTop 92`, `kWheelWidth 640`, rows 62 / selected 92, `kWheelRowGap 10`, `kWheelIndent {0,46,76,96}`. Skews `kRows 0.213`, `kWheel 0.249` |
| Text styles | `src/render/theme.hpp:119-134` | `kSongTitle`, `kArtist`, `kBpm`, `kWheelRow`, `kWheelSelected`, `kWheelPack`, `kDiffName[Selected]`, `kDiffMeter[Selected]`, `kDiffBest[Selected]`, `kChip`, `kHintWord`, `kHintKey`. All are already in `kAllStyles`, so they are pre-baked |
| Fonts | Saira Condensed | No ↑↓←→ glyphs (`docs/cabinet-theme/README.md`, issue note), so the hint arrows are code-drawn |
| Layout scale | `src/render/theme_layout.hpp` | `L = theme::layout_scale(w, h)`. 1280x720 → identity. 2560x1440 → s=2. 3440x1440 → s=2, origin (440, 0). 1920x1200 → s=1.5, origin (0, 60) |
| `format_speed_mod` | `src/screens/options_menu.cpp:70-80` | In an anonymous namespace today. Needed for the speed chip ("2.5x", "C450", "M600") |
| Translit choice | `src/screens/song_display_text.cpp:15-23` | Still uses bitmap `font_covers_text`. Plan 042 (`042-ttf-text-rendering-plan.md:192,465`) leaves a forward reference: "#94 and #95 switch `chart_display_label` and `song_display_title` to `TextRenderer::truncate` and `covers_text`" |
| Untrusted labels | `src/chart/chart.hpp:12-75` | `resolve_difficulty(label, description, meter)` mirrors OpenITG `StringToDifficulty` + `Steps::TidyUpData` |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Mock measurements vs `theme.hpp` (measured with PIL on the reference PNG)

| Item | `theme.hpp` / manifest note | Mock | Plan uses |
|---|---|---|---|
| Diff row pitch | 44 + `kDiffRowGap` 8 = 52 | tops 373, 427, 481, 536 (sel), 599 → pitch **54** (gap 10) | **gap 10** (`kDiffRowGap` 8 → 10) |
| Wheel row pitch | 62 + `kWheelRowGap` 10 = 72 | tops 94, 168, 246 (sel content), 352, 428 → pitch **76** (gap 14). With gap 10 the 5th row would sit 18 px too high | **gap 14** (`kWheelRowGap` 10 → 14) |
| Diff name x | "x+24" (manifest note) | ink starts at row x + 15 | **x + 15** |
| Diff meter centre | "x+183" | row x + 174 (beginner "1", challenge "10" and selected "8" all agree) | **x + 174** |
| First tick | "ticks from x+216" | tick sprite content left = row x + 194 (ink at mid-height x + 197) | **x + 194** |
| Best % right edge | "right-aligned" | row x + 547 (17 px in from the content right edge) | **x + 547** |
| Chips | 140x30 ref content | boxes y 12..51 (40 tall, centred in the 64 px bar), right edge ≈ 1243, gap ≈ 3, text padding ≈ 22 each side | top 12, h 40, pad 22, gap 4, right edge 1240 (`kRefWidth − kTopBarPadX`) |
| Wheel indents | `{0,46,76,96}` | pack (d=2) at 752, aurora (d=1) 722, selected 676 | matches, keep |
| Hint line | `kHintBarHeight 52`, `kHintGap 34` | rule 666–667, bar 668–720, text caps 687–701, line centred on x 640 (308..970) | matches, keep |

---

## Patterns to Follow

### Screen render: layout scale, null-checked services, thin helpers (copy #92)

```cpp
// SOURCE: src/screens/title_screen.cpp:33-76
void TitleScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    namespace layout = theme::layout;
    const theme::LayoutScale L = theme::layout_scale(w, h);
    if (ctx.theme != nullptr) {
        title_art::draw_backdrop(*ctx.theme, renderer, w, h);
        title_art::draw_centred_sprite(*ctx.theme, renderer, "logo", L, layout::kLogoTop);
        ...
    if (ctx.text != nullptr) {
        const float top = title_art::footer_text_top(L, ctx.text->line_height(theme::text::kFooter));
        ctx.text->draw(renderer, title_art::footer_left_text(), L.x(layout::kFooterPadX), top,
                       theme::text::kFooter, TextAlign::Left);
```

### Helper module shape: pure layout functions + thin draw helpers (copy `title_art`)

```cpp
// SOURCE: src/screens/title_art.hpp:1-13, 39-41
// Cabinet v3 title/attract art (#92), shared by TitleScreen and AttractScreen.
//  - Pure layout helpers: reference-space (1280x720) positions mapped to window
//    pixels with theme::layout_scale (#91). GL-free, so title_screen_test pins
//    them headless.
//  - Thin draw helpers over ThemeTextures (#89). Each one is a no-op on an
//    uninitialised GlQuadRenderer.
[[nodiscard]] Vec2 centred_sprite_pos(const theme::LayoutScale& L, float ref_top,
                                      Vec2 content_size);
```

### ThemeTextures draw API (pick the call that matches the manifest `kind`)

```cpp
// SOURCE: src/render/theme_textures.hpp:595-618
void draw_sprite(GlQuadRenderer&, std::string_view name, Vec2 content_pos, float s, Color tint = Color{}) const;
void draw_stretch(GlQuadRenderer&, std::string_view name, const Rect& content_rect, Color tint = Color{}) const;   // any kind: content box fills the rect
void draw_stretch_x(GlQuadRenderer&, std::string_view name, float x, float y, float width, float s, Color tint = Color{}) const;
void draw_slice3(GlQuadRenderer&, std::string_view name, const Rect& content_rect, Color tint = Color{}) const;
void draw_frame(GlQuadRenderer&, std::string_view name, const Rect& hole_rect, Color tint = Color{}) const;
void draw_tiled(GlQuadRenderer&, std::string_view name, const Rect& rect, float s, Color tint = Color{}, TileAnchor = TileAnchor::TopLeft) const;
```

### Solid parallelograms (Edit row, hint arrows): `draw_quad_points` with an invalid texture draws solid

```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:74-83
    // General quad from four corners (TL, TR, BR, BL; any convex quad, e.g. a
    // parallelogram from a theme::skew), each with its own straight-alpha colour.
    // An invalid `texture` draws solid (the white texture), like draw_textured_quad.
    void draw_quad_points(const std::array<Vec2, 4>& corners, const Texture& texture,
                          const UVRect& uv, const std::array<Color, 4>& colours);
```

### Text: measure, truncate, draw (headless measuring works; draws are no-ops)

```cpp
// SOURCE: src/render/ttf_font.hpp:231-251
    [[nodiscard]] float measure(std::string_view text, const theme::TextStyle& style) const;
    [[nodiscard]] float line_height(const theme::TextStyle& style) const;
    [[nodiscard]] std::string truncate(std::string_view text, const theme::TextStyle& style, float max_width) const;
    [[nodiscard]] bool covers_text(std::string_view text, theme::Font font) const;
    void draw(GlQuadRenderer& renderer, std::string_view text, float x, float y,
              const theme::TextStyle& style, TextAlign align = TextAlign::Left, float extra_shear = 0.0f);
```

### Window logic to keep (generalise, do not change its rule)

```cpp
// SOURCE: src/screens/select_screen.cpp:646-654
    // window is 13 rows tall; the highlight stays centered whenever the window
    // can slide, and the window fills the full 13 rows at either end of the list.
    constexpr int kHalfRows = 6;
    const int window_rows = 2 * kHalfRows + 1;
    int first = selected_song_ - kHalfRows;
    first = std::clamp(first, 0, std::max(0, count - window_rows));
    const int last = std::min(count - 1, first + window_rows - 1);
```

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real assets headless)

```cpp
// SOURCE: tests/title_screen_test.cpp:75-76, 211-212
    static blaze4k::ThemeTextures theme;
    static const bool loaded = theme.load(kCabinet);
    TEST_CHECK(text.load(kSourceDir));
    text.set_window_size(1280, 720);
```

```cmake
# SOURCE: tests/CMakeLists.txt:503-519 (title_screen_test registration with asset roots)
add_executable(title_screen_test title_screen_test.cpp)
target_link_libraries(title_screen_test PRIVATE blaze4k_core)
target_compile_definitions(title_screen_test PRIVATE
    BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME title_screen_test COMMAND title_screen_test)
```

---

## Pinned Semantics (the contract `select_art_test` pins)

All values are in reference space (1280x720) and are mapped with `L = theme::layout_scale(w, h)`. Every rect is a manifest **content box**; the glow padding hangs outside it.

### Chrome

| Element | Reference | 2560x1440 (s=2) | 3440x1440 (s=2, origin 440,0) | 1920x1200 (s=1.5, origin 0,60) |
|---|---|---|---|---|
| `bg_select` | whole window `{0,0,w,h}` | `{0,0,2560,1440}` | `{0,0,3440,1440}` | `{0,0,1920,1200}` |
| `bar_top` | x 0..w (whole window width), top `L.y(0)`, natural height 66 | y 0, h 132 | y 0, h 132 | y 60, h 99 |
| `title_select_music` content top-left | (40, 9) | (80, 18) | (520, 18) | (60, 73.5) |
| Chips | top 12, height 40; width = `measure(text, kChip) + 2·22`; right-aligned, last chip's right edge at 1240, gap 4; order left→right: speed, scroll | ×2 | ×2, +440 x | ×1.5, +60 y |
| `bar_hint` | x 0..w, top `L.y(720) − content_size("bar_hint").y` (ref 666), height 54 | top 1332 | top 1332 | top 1059 |
| Hint text top | `L.y(668) + (L.px(52) − line_height(kHintWord)) / 2` | same formula | same formula | same formula |
| `scanlines` | tiled over `{0,0,w,h}` at 1 texel per screen px | same | same | same |

- Chip text: speed = `"SPEED " + format_speed_mod(mod)` (e.g. "SPEED 2.5x", "SPEED C450", "SPEED M600"); scroll = "UPSCROLL" or "DOWNSCROLL". The source is `ctx.config->gameplay` (`speed_mod` parsed with `parse_speed_mod`, falling back to 1x; `scroll == "down"`). With no config: "SPEED 1x" and "UPSCROLL". The chip texture is tinted `kCyan` / `kGreen`, and the text uses `kChip` with its colour replaced by the same tint. The text is centred in the chip, line box centred vertically.

### Banner and info

| Element | Reference rect / anchor |
|---|---|
| Banner image | `draw_stretch`-style into `kBanner {48,100,560,157}` (the `banner_frame` hole). A song banner uses `renderer.draw_textured_quad` as today; with no valid banner texture, use `banner_fallback` via `draw_stretch` |
| `banner_frame` | `draw_frame(..., L.rect(kBanner))`, drawn after the image |
| Title | left 48, top 270, `kSongTitle`, `truncate` to 560 |
| BPM | `"BPM " + format_bpm_range(timing)`, `kBpm`, `TextAlign::Right` at x 604, top 326 |
| Artist | left 48, top 326, `kArtist`, `truncate` to `560 − measure(bpm) − 16` (never negative) |

- Display text: title and artist go through the translit rule with **TTF coverage**: `select_display_text(native, translit, text->covers_text(native, style.font))`. Without a `TextRenderer`, fall back to the bitmap rule (`song_display_title`, as today). Malformed bytes and unknown scripts keep #77's placeholder-box behaviour, which `TextRenderer` already implements.

### Difficulty rows

- Visible rows: `kDiffVisibleRows = 5`. The rule: the most rows that fit between `kDiffListTop` (372) and the hint-bar rule (666) with one selected row: `1 + floor((666 − 372 − 52) / 54) = 5`. Window = `list_window(selected_chart, chart_count, 5)` (the same rule as the wheel).
- Row `i` of the window, with the selected chart at window slot `k`:
  - `y = 372 + i·54 + (i > k ? 8 : 0)`; `h = (i == k) ? 52 : 44`;
  - `x = 44 + (i == k ? 14 : 0)`; `w = 564`.
  - Example (5 charts, selected index 3): y = 372, 426, 480, **534** (x 58, h 52), 596. The mock measures 373, 427, 481, 536, 599.
- Style: `resolve_difficulty(chart.difficulty, chart.description, chart.meter)` → Beginner/Easy/Medium/Hard/Challenge pick `diff_row_<name>` / `diff_row_<name>_selected` and `theme::difficulty::k<Name>`. Edit (and Invalid, defensively) gets the neutral Edit row and `theme::difficulty::kEdit`.
- Label: an Edit chart with a description shows its description as written (via `chart_display_label`, untruncated); every other chart shows its passthrough label in ASCII upper case ("Hard" → "HARD", "Expert" → "EXPERT"). The label is truncated with `text->truncate` to a 128 px name budget (the tab is about 150 px wide at mid-height).
- Inside a row (row content x = `rx`, row y/h as above; text line boxes centred vertically: `top = y + (h − line_height) / 2`):

  | Part | Unselected | Selected |
  |---|---|---|
  | Name | `kDiffName`, colour = `DifficultyColors::ink`, left at `rx + 15` | `kDiffNameSelected`, ink, `rx + 15` |
  | Meter | `std::to_string(meter)`, `kDiffMeter` (white), centred at `rx + 174` | `kDiffMeterSelected`, centred at `rx + 174` |
  | Ticks | 10 × `diff_tick` content `{rx + 194 + n·18, y + (h − 18)/2, 20, 18}` | `{rx + 194 + n·18, y + (h − 20)/2, 20, 20}` |
  | Tick tint | `n < clamp(meter, 0, 10)` → `DifficultyColors::fill`, else `color::kTickOff` | same |
  | Best | `format_percent(record->percent)` or "---", `kDiffBest` (steel), `TextAlign::Right` at `rx + 547` | `kDiffBestSelected` (gold), right at `rx + 547` |

- Edit row (no texture), drawn with `draw_quad_points` + an invalid texture, skew `theme::skew::kRows`:
  - `skewed_quad(rect, k)` corners: TL `(x + k·h/2, y)`, TR `(x + w + k·h/2, y)`, BR `(x + w − k·h/2, y + h)`, BL `(x − k·h/2, y + h)`. This is the CSS `skewX(−12°)` about the row's vertical centre, matching the baked rows' slant.
  - Body: `skewed_quad(row)` in `hex(0x0B1030, 0.85)` (the baked rows' body colour, sampled from `diff_row_beginner.png`), with 1 px top and bottom strips in `hex(0x27325A)`.
  - Tab: `skewed_quad({rx, y, 150, h})` in `kEdit.fill`.
  - Selected: first a `kGold` parallelogram outset by 2 px on every side, then the body at alpha 1.0, then the tab.
- Rows are drawn first (all textures), then all ticks, then the text grouped by style, so flushes stay low.

### Wheel

- Display rows: `build_wheel_rows(songs)` interleaves one `Pack` row before the first song of each pack, then that pack's `Song` rows, in library order. Navigation still moves over songs only (`selected_song_` and every existing accessor are unchanged); the selected display row is `song_row_index_[selected_song_]`.
- Visible rows: `kWheelVisibleRows = 7`: `1 + floor((666 − 92 − 92) / 76) = 7` (centre slot 3). Window = `list_window(selected_row, row_count, 7)` with `first = clamp(sel − 3, 0, max(0, n − 7))`, `last = min(n − 1, first + 6)`. This is the same rule as today's 13-row window, with the count taken from the layout.
- Slot `j` with the selection at slot `k`:
  - `y = 92 + j·76 + (j > k ? 30 : 0)`; `h = (j == k) ? 92 : 62`;
  - indent `d = min(|j − k|, 3)`, `x = 676 + kWheelIndent[d]`.
  - Example (the mock: pack, song, **selected**, song, song): y = 92, 168, **244**, 350, 426; x = 752, 722, **676**, 722, 752.
- Row art: `Pack` → `wheel_pack` slice3; `Song` → `wheel_row` slice3; selected → `wheel_row_selected` via `draw_stretch` over `{676, y, 640, 92}`.
  - The slice3 widths are `max(L.px(640), w − L.x(x))` so rows always run off the window's right edge, even on 21:9.
  - The selected sprite is stretched only horizontally, and only when the window is wider than the column (it is exactly 640 at 16:9).
- Text, line box centred vertically in the row:
  - song row `kWheelRow` at `x + 26`;
  - selected `kWheelSelected` at `x + 30`;
  - pack `kWheelPack` at `x + 54`, pack name in ASCII upper case;
  - each truncated to `1256 − text_x` (24 px right margin at the column edge).
  - Titles use the TTF translit rule. No artist.
- Slide: `kWheelScrollSeconds = 0.08`.
  - On every song move, compute `delta_first = new_first − old_first`.
  - If `|delta_first| == 1`: `scroll_start = clamp(current_offset + delta_first · 76, −152, 152)` and `scroll_elapsed = 0`. Otherwise (no window move, or a wrap/jump): leave a running slide alone if `delta_first == 0`, and snap to 0 if `|delta_first| > 1`.
  - `update()` adds `fixed_dt` to `scroll_elapsed`.
  - `wheel_scroll_offset(start, elapsed) = start · (1 − t)^3`, with `t = clamp(elapsed / 0.08, 0, 1)` (ease-out cubic). Rows are drawn at `y + offset` (screen: `L.px(offset)`). While the offset is non-zero, one extra row is drawn above `first` and one below `last`; the bars drawn later cover them.
  - `enter()` resets the slide to 0.
  - The gold selected bar does not slide; it jumps to the new row, as the selection height changes instantly.
  - Nothing here reads the music clock or wall time.

### Hint bar

- Pairs, left to right: `[↑↓] SONG`, `[←→] DIFFICULTY`, `[ENTER] PLAY`, `[TAB] OPTIONS`, `[ESC] TITLE`. Keys use `kHintKey` (gold), words `kHintWord` (`kHint`).
- Spacing:
  - key → word: 10 px after an arrow pair, 8 px after a text key (whose measure already includes 2 px trailing tracking);
  - pair → pair: `kHintGap` 34.
  - The whole line is centred on x 640. The mock spans 308..970.
- Arrow glyphs (solid `kGold` quads; the cap band is 14 px tall, centred on the hint line's cap middle):
  - ↑/↓: a 2 px stem the full 14 px high, plus a triangle head 4 px wide and 4 px tall at the pointing end. Cell 4 wide; ↑ and ↓ centres 16 apart.
  - ←/→: a 2 px stem 14 px long, plus a triangle head 4 long and 4 tall. Cell 14 wide; centres 24 apart.
  - These were measured from the mock. A triangle is a quad with two coincident corners.
- `hint_layout(measure)` returns a fixed-size array (12 pieces, no heap) with each piece's kind, text or arrow and x. It is pure: `measure` is a callback, so tests use the real headless `TextRenderer`.

### Empty library

Draw the bg, bars, title sprite, chips, hint bar and scanlines. Show "NO SONGS FOUND" centred at y 340 in `kWheelRow`. No banner, rows or wheel.

### Options overlay

`render()` draws the full Cabinet screen first, then today's overlay code unchanged (its 72% dim and panel), instead of returning before the screen is drawn. The #96 restyle replaces the overlay itself.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | CREATE | Pure layout helpers (row rects, windows, indents, slide easing, chip and hint layout, `skewed_quad`, row style and label) and thin draw helpers |
| `src/screens/select_art.cpp` | CREATE | Their implementation |
| `src/screens/select_screen.hpp` | UPDATE | Remove `difficulty_color` / `difficulty_row_text`. Add wheel display rows, the song→row map, slide state, cached chip strings, and a `wheel_scroll_offset()` / `wheel_first_row()` test accessor |
| `src/screens/select_screen.cpp` | UPDATE | New `render()`; slide trigger in `move_song`; slide advance in `update`; `rebuild` builds the display rows; chip refresh; remove the old colour constants and helpers |
| `src/screens/song_display_text.hpp/.cpp` | UPDATE | TTF-coverage overloads `song_display_title/artist(metadata, const TextRenderer*, theme::Font)` (null → bitmap rule); `chart_display_label(chart)` overload with no truncation |
| `src/screens/options_menu.hpp/.cpp` | UPDATE | Move `format_speed_mod` out of the anonymous namespace and declare it |
| `src/render/theme.hpp` | UPDATE | `kDiffRowGap` 8 → 10, `kWheelRowGap` 10 → 14, with a "measured from cabinet-v3-select.png (#94)" comment |
| `CMakeLists.txt` | UPDATE | Add `src/screens/select_art.cpp` to `blaze4k_core` (next to `title_art.cpp`) |
| `tests/select_art_test.cpp` | CREATE | Pins every Pinned Semantics value at 4 window sizes, the window and slide rules, row style, labels, chips, hint layout and arrow geometry. Render smoke with the real headless theme and text |
| `tests/select_screen_test.cpp` | UPDATE | Replace `test_difficulty_colors` / `test_difficulty_row_text` and the `difficulty_row_text` use in `test_named_edit_charts`. Add slide and window behaviour tests on a 12-song, 2-pack fixture, and a render smoke with real services. Keep every behaviour test |
| `tests/CMakeLists.txt` | UPDATE | Register `select_art_test`; add `BLAZE4K_SOURCE_DIR` / `BLAZE4K_ASSETS_DIR` to `select_screen_test` |

---

## Tasks

Execute in order. Build after each task; run the full sandboxed ctest after Tasks 5, 7 and 8.

### Task 1: Small API additions (no behaviour change)

- **Files**: `src/screens/options_menu.hpp/.cpp`, `src/screens/song_display_text.hpp/.cpp`, `src/render/theme.hpp`
- **Implement**:
  - `[[nodiscard]] std::string format_speed_mod(const SpeedMod& mod);` declared in `options_menu.hpp`, moved out of the anonymous namespace. `format_number` stays private.
  - `song_display_title(const SongMetadata&, const TextRenderer* text, theme::Font font)` and the `artist` twin: when `text` is null, return the existing bitmap-rule result; else `select_display_text(native, translit, text->covers_text(native, font))`. The existing one-argument versions stay for results (#95).
  - `chart_display_label(const Chart&)`: the same rule as the two-argument form without truncation (implement the two-argument form as `truncate_to_cells(chart_display_label(chart), n)` for the Edit case, so the rule lives in one place).
  - `theme.hpp`: `kDiffRowGap = 10.0f`, `kWheelRowGap = 14.0f`, each with a comment citing the mock measurement.
- **Mirror**: `src/screens/song_display_text.cpp:15-32`
- **Validate**: `cmake --build build -j$(nproc)`; the sandboxed ctest stays 47/47 (`bitmap_font_test` pins the old overloads)

### Task 2: `select_art` pure layout

- **Files**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp` (CREATE); `CMakeLists.txt` (add the TU)
- **Implement** (namespace `blaze4k::select_art`, reference-space, GL-free):
  - Constants: `kDiffVisibleRows = 5`, `kWheelVisibleRows = 7`, `kWheelScrollSeconds = 0.08`, `kWheelPitch = 76`, the diff-row inner offsets (15, 174, 194, 547), the tick sizes, the name budget 128, the wheel text offsets (26/30/54), the text right limit 1256, the chip metrics (top 12, h 40, pad 22, gap 4, right 1240), the hint spacing and arrow sizes, the BPM anchor 604, and the artist/BPM gap 16. Each constant gets a one-line provenance comment ("theme.hpp" / "manifest" / "measured from mock").
  - `struct ListWindow { int first; int last; };` and `list_window(selected, count, visible)`, which is today's rule generalised (count 0 → `{0, −1}`).
  - `visible_rows(list_top, list_bottom, row_pitch, selected_h)`: returns `1 + floor((bottom − top − selected_h) / pitch)`, clamped ≥ 1. The constants above are asserted against it with `static_assert` or a test.
  - `difficulty_row_rect(slot, selected_slot)` and `wheel_row_rect(slot, selected_slot)` (with the indent); `wheel_indent(distance)`.
  - `struct WheelRow { enum class Kind { Pack, Song } kind; int pack_index; int song_index; };` and `build_wheel_rows(pack indices of the wheel songs)`, which returns the rows plus the song→row map.
  - `wheel_scroll_offset(start, elapsed)` and `wheel_scroll_start(current_offset, delta_first)`, implementing the slide rule above.
  - `struct DifficultyRowStyle { StepsDifficulty kind; std::string_view texture; std::string_view texture_selected; theme::DifficultyColors colors; bool baked; };` and `difficulty_row_style(const Chart&)`. Names come from a static table, so there is no allocation.
  - `difficulty_row_label(const Chart&)` (upper-case rule), `meter_ticks_lit(meter)`, `tick_rect(row, selected, n)`.
  - `skewed_quad(Rect, skew) -> std::array<Vec2, 4>`.
  - `speed_chip_text(const GameConfig*)` and `scroll_chip_text(const GameConfig*)`; `chip_rects(speed_w, scroll_w)` (right-aligned).
  - Hint pieces: `enum class HintArrow { Up, Down, Left, Right }`, `hint_layout(measure_key, measure_word)` → `HintLine { std::array<HintPiece, 12> pieces; int count; float width; }`, centred on 640; `hint_arrow_quads(HintArrow, Vec2 centre)` → stem + head corner arrays.
- **Mirror**: `src/screens/title_art.hpp/.cpp` (comment header, `[[nodiscard]]`, pure helpers first)
- **Validate**: `cmake --build build -j$(nproc)` (no warnings)

### Task 3: `select_art` draw helpers

- **File**: `src/screens/select_art.cpp`
- **Implement** thin helpers taking `(const ThemeTextures&, GlQuadRenderer&, const theme::LayoutScale& L, …)`:
  - `draw_backdrop` (bg_select over the window)
  - `draw_top_bar` (bar_top + title sprite)
  - `draw_chips` (texture + text; takes `TextRenderer*`)
  - `draw_hint_bar` (bar + pieces + arrows)
  - `draw_banner` (image or fallback, then frame)
  - `draw_difficulty_row_art` (baked slice3, or the Edit parallelograms)
  - `draw_ticks`
  - `draw_wheel_row_art`
  - `draw_scanlines` (reuse `title_art::draw_scanlines`; do not duplicate)

  The solid quads use a function-local `static const Texture kSolid;` (id 0 → the renderer substitutes white). Every helper is a no-op when its service pointer is null.
- **Mirror**: `src/screens/title_art.cpp:73-93`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 4: `SelectScreen` state: display rows, slide, chips

- **Files**: `src/screens/select_screen.hpp/.cpp`
- **Implement**:
  - Members: `std::vector<select_art::WheelRow> wheel_rows_;`, `std::vector<int> song_row_index_;`, `float scroll_start_ = 0.0f;`, `double scroll_elapsed_ = 0.0;`, plus cached `chip_speed_text_`, `chip_scroll_text_` and the config strings they were built from (`chip_src_speed_`, `chip_src_scroll_`).
  - `rebuild()` fills `wheel_rows_` and `song_row_index_` after `songs_` (the selection-restore logic is untouched).
  - `move_song()`: record `old_first = list_window(row(selected), n, 7).first` before the change and `new_first` after it, then `scroll_start_ = wheel_scroll_start(current_offset(), new_first − old_first)` and `scroll_elapsed_ = 0` when that rule says so.
  - `update()`: `scroll_elapsed_ += fixed_dt` at the top (before input), then `refresh_chips(ctx)` at the end (it compares strings and rebuilds only on change).
  - `enter()`: reset the slide; `refresh_chips(ctx)`.
  - Test accessors: `[[nodiscard]] float wheel_scroll_offset() const;`, `[[nodiscard]] int wheel_first_row() const;`, `[[nodiscard]] std::size_t wheel_row_count() const;`.
  - Delete `difficulty_color`, `difficulty_row_text`, the old colour constants and `iequals` if it is now unused (it is still used by `rebuild` for SELECTABLE, so keep it).
- **Validate**: build; the sandboxed ctest is expected to fail to *compile* `select_screen_test` until Task 7 removes the deleted-helper tests. Do Tasks 4–7 together before running ctest, or temporarily keep the old helpers until Task 7.

### Task 5: `SelectScreen::render`

- **File**: `src/screens/select_screen.cpp`
- **Implement**, following the draw order in the Summary:
  - `L = theme::layout_scale(w, h)`. Every theme and text call is null-guarded.
  - The empty-library and options-overlay branches as pinned.
  - The banner keeps `texture_cache_` (`get(song->resolved_banner_path)`, `valid()` check).
  - Best % via `best_score_for` + `format_percent`.
  - Group the text draws by style (all wheel-row titles, then the selected title, then the pack names; then diff names, meters and bests) to keep atlas flushes low.
- **Validate**: `cmake --build build -j$(nproc)`; then the sandboxed ctest (once Task 7 compiles)

### Task 6: `select_art_test`

- **Files**: `tests/select_art_test.cpp` (CREATE), `tests/CMakeLists.txt`
- **Implement** these test functions:
  - `test_list_window`: counts 0, 1, 7, 8, 30, with the selection at the ends and in the middle; the 13-row behaviour reproduced with `visible = 13`.
  - `test_visible_rows`: 5 and 7 derived from the layout constants.
  - `test_difficulty_row_rects`: the 5-chart / selected-3 example at all 4 window sizes.
  - `test_wheel_row_rects`: the mock example (y 92/168/244/350/426, x 752/722/676/722/752) at all 4 sizes; indent clamp at d ≥ 3.
  - `test_build_wheel_rows`: 2 packs → headers interleaved; the song→row map; an empty list.
  - `test_scroll_easing`: offset at t = 0, 0.04, 0.08, 1.0 (start, start/8, 0, 0); the start rule for Δ = 0, ±1, ±5; the clamp at ±152.
  - `test_row_style`:
    - "Beginner"/"easy"/"MEDIUM"/"Hard"/"Challenge"/"Expert" → the right texture names and colours;
    - "Edit" → not baked, `kEdit`;
    - "Novice" meter 1 → Beginner and meter 5 → Medium;
    - unknown meter 9 → Hard.
  - `test_labels`: "Hard" → "HARD"; Edit with "JBEAN" → "JBEAN"; Edit without a description → "EDIT".
  - `test_ticks`: meters −1, 0, 3, 10, 15 → lit counts 0, 0, 3, 10, 10; tick rects for both row heights.
  - `test_chip_text`: no config, "2.5x", "c450", "m600", "garbage" (falls back to 1x), scroll "down".
  - `test_chip_rects`: right-aligned, gap and padding.
  - `test_hint_layout`: piece order and kinds, centred on 640 (|centre − 640| < 0.5) with the real headless `TextRenderer`; the arrow quad corners.
  - `test_skewed_quad`: the corners for a known rect.
  - `test_texture_names_exist`: every texture name the screen uses (all 10 diff-row names, `diff_tick`, the 3 wheel names, `bg_select`, `bar_top`, `bar_hint`, `title_select_music`, `chip`, `banner_frame`, `banner_fallback`, `scanlines`) has `theme.entry(name) != nullptr` in the real manifest.
  - `test_render_smoke`: a `ScreenManager` + `SelectScreen` with the real headless `ThemeTextures` + `TextRenderer` (+ the null-service path), rendering at 1280x720, 2560x1440, 3440x1440, 1920x1200 and 640x480 on a populated library, an empty library and with the options overlay open; no crash.
- **Mirror**: `tests/title_screen_test.cpp` (structure, `approx`, asset roots)
- **Validate**: the sandboxed ctest → 48/48

### Task 7: Update `select_screen_test`

- **Files**: `tests/select_screen_test.cpp`, `tests/CMakeLists.txt`
- **Implement**:
  - Delete `test_difficulty_colors` and `test_difficulty_row_text`. Their replacements live in `select_art_test`.
  - In `test_named_edit_charts`, replace the `difficulty_row_text` width assertion with a check that `text.truncate(difficulty_row_label(chart), kDiffName, 128)` measures ≤ 128 with the real headless `TextRenderer`.
  - Add `test_wheel_slide`, using a 12-song / 2-pack fixture (14 display rows) and `kDt = 1.0 / 60.0` for this test only. The main fixture's `kDt` is 0.1, longer than the whole slide:
    - at the top (first = 0), Down moves the selection but not the window → offset 0;
    - at selected row 3, Down → `wheel_first_row()` + 1 and offset 76; after 5 ticks (83 ms) offset 0;
    - Up mirrors it with −76;
    - wrap from the last to the first song → offset snaps to 0;
    - held repeat (the `action_down` stub, as in `test_held_navigation_repeat`) never leaves |offset| > 152.
  - Add `test_wheel_skips_pack_rows`: Down from a pack's last song lands on the next pack's first song, never on a header (`selected_song()` non-null, `selected_song_index()` + 1).
  - Add a populated render smoke with the real services in `main` next to the existing one.
  - Add the two compile definitions to `select_screen_test` in CMake.
- **Validate**: the sandboxed ctest → 48/48; every pre-existing behaviour test (`wheel_load`, `song_navigation`, `difficulty_navigation`, `held_navigation_repeat`, `best_score`, `confirm_handoff`, `selection_preserved_on_reenter`, `empty_library`, `special_character_titles`, the options/calibration/remap tests) passes unchanged

### Task 8: Final checks

- **Implement**: run the full Validation block and the End-to-End steps 1–3. Confirm `git status` shows only the files in "Files to Change" plus the owner's untouched `todo-stories.md`.
- **Validate**: as listed below

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from touched files
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides the audio device + the user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 48

# Static checks
git status --short                                   # only the files in "Files to Change" (+ the owner's todo-stories.md, untouched)
git diff --stat -- .agents/stories/todo-stories.md   # no diff from this work
grep -n "difficulty_row_text\|difficulty_color" src tests -r   # expect: no match
grep -n "draw_text(" src/screens/select_screen.cpp   # expect: only inside the options-overlay block (#96 restyles it)
```

## End-to-End Verification

1. **Automated pure-path proof.** Run the sandboxed ctest above.
   - `select_art_test` pins every coordinate in Pinned Semantics at 4 window sizes, plus the window, slide, style, label, chip and hint rules.
   - `select_screen_test` proves navigation, key repeat, preview, the options overlay and Confirm → Gameplay are unchanged, and pins the slide on the real screen.
   - `perf_loop_test` stays green.
2. **Real app, headless (safe for the agent).** Inside the same `bwrap` sandbox, from the repo root:
   ```bash
   D=$(mktemp -d); ./build/blaze-4k --headless --smoke-test 30 --start-screen select --data-dir "$D"; rm -rf "$D"
   ```
   Expect the same lines as on `main`: `[SelectScreen] library: 221 songs, 1116 charts`, `[ScreenManager] enter Select`, `Blaze 4k shut down cleanly.`, and no new warnings or unknown-theme-name lines. This runs the new `render()` against the real ITG library (songs with more than 5 charts, long titles, many packs) in the real binary.
3. **Unknown texture names.** In the step-2 output, `grep "Unknown theme texture"` must find nothing. `ThemeTextures::find` logs each unknown name once (`theme_textures.cpp:918`), so a typo in `diff_row_<name>` shows up here. `select_art_test` also asserts that every name `difficulty_row_style` returns (and every fixed name the screen uses) has a `theme.entry(name)` in the real manifest, which covers the case where headless App skips the draw.
4. **Owner-only visual check (AC). The agent must NOT launch the GUI**, because it opens a window and plays preview audio.
   - Owner runs `./build/blaze-4k --start-screen select`.
   - (a) At 1280x720, compare side by side with `docs/cabinet-theme/reference/cabinet-v3-select.png`:
     - top bar, SELECT MUSIC, and the cyan/green chips at the right;
     - the banner in its chrome frame, with the title, artist and gold BPM below;
     - 5 slanted difficulty rows with lit ticks; the selected row 14 px right with a gold frame and a gold best %;
     - the wheel with the orange pack header, the indented curve and the gold selected bar;
     - the hint bar with gold keys and arrows;
     - the scanlines.
   - (b) Hold Down: the wheel slides smoothly (no jumps, no gaps at the top or bottom edge), accelerating with key repeat. Release and press Up and Down at the list ends.
   - (c) Pick an ITG song with an Edit chart: a neutral grey-tabbed row shows the chart name. A song with more than 5 charts scrolls its difficulty list.
   - (d) Open Options (Tab): the old overlay draws over the dimmed Cabinet screen. Change Speed to 2.5x and Scroll to Down, close it, and the chips read "SPEED 2.5x" and "DOWNSCROLL".
   - (e) At 2560x1440 the layout is identical at 2x with crisp text. On an ultrawide or a 16:10 window, nothing is cropped, and the wheel rows still run off the right edge of the window.
   - (f) Enter starts the song as before; Esc returns to the title.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Changing `kDiffRowGap` / `kWheelRowGap` disagrees with the pack's `theme.hpp` | Values measured from the reference PNG (a table above, 5 rows each). The constants are only used by select today (grep). Raised as an Open Question; reverting is a two-number change | In scope (flag) |
| Mock-measured inner offsets (15/174/194/547) differ from the manifest notes (24/183/216) | One named constant each with provenance comments; the owner's side-by-side check (step 4a) decides | In scope (verify) |
| More than 5 charts (ITG songs with several Edits) would overrun the hint bar | 5-row window using the same `list_window` rule, tested | In scope |
| A song with many Edits whose names are long | Measured truncation to the 128 px tab budget, with "..." | In scope |
| Sliding rows reveal empty space or draw over the top bar | One extra row above and below while the offset is non-zero; bars drawn after the wheel cover them | In scope |
| The slide fights the 40 ms key repeat (new moves before 80 ms) | Restart from the current offset + Δ·76, clamped to ±2 rows; tested with held repeat | In scope |
| Wrap (last → first song) would slide across the whole list | `|Δfirst| > 1` snaps to 0 | In scope |
| Invalid/unknown texture → flat quads or nothing | `ThemeTextures` fallback contract (#89); End-to-End step 3 greps the log for unknown names | In scope |
| Headless/unit tests have null `theme`/`text`/renderer | Every call null-guarded; smoke renders cover both the null and the real-service path | In scope |
| Per-frame string allocations (`truncate`, `"BPM " + …`, `std::to_string`) | No worse than today (`difficulty_row_text` allocates every frame); `perf_loop_test` stays green. Caching display strings per selection/scale is left to #98 polish | Out of scope (flag) |
| Texture flushes higher than the plan's 10–25 estimate (5 diff-row textures + 3 wheel textures + ~12 atlases + bars) | Group draws by texture/style as specified; expect ~30. Trivial for GL 3.3; `--perf-report` on the owner's run | Accept |
| Hint keys are fixed labels ("ENTER", "TAB", "ESC") even when the player has remapped keys (#C6) | Matches the mock and today's fixed hint text. Remap-aware hints would be a follow-up | Out of scope (flag) |
| The selected wheel sprite is stretched horizontally on windows wider than 16:9 | Only the right side grows; at 16:9 it is exactly the mock. Owner checks in step 4e | In scope (verify) |
| Text vertical centring uses the line box (it includes the descender), so caps may sit 1–3 px high | Same rule as #92's footer; one formula, nudgeable by a constant after step 4a | In scope (verify) |
| `song_display_title` overload adds a `ttf_font.hpp` dependency to `song_display_text` | Same core library, header include only; results (#95) keep the bitmap overload until restyled | Accept |
| The options overlay still uses the bitmap font over the new screen | Intentional: #96 owns its restyle | Out of scope |

---

## Open Questions

All are non-blocking; the plan proceeds with the stated default.

1. **Row spacing: mock or `theme.hpp`?** What the player would notice: with the pack's spacing, the wheel rows sit closer together than in the mock (the 5th row about 18 px higher), and the difficulty rows are 2 px tighter each. Options:
   - (a) use the spacing measured from the mock (wheel gap 14, difficulty gap 10), editing two numbers in `theme.hpp`;
   - (b) keep `theme.hpp` as shipped (10 and 8).

   **Default: (a)**, because the issue asks to match the reference image.
2. **Pack header placement.** What the player would notice: whether the pack name scrolls away with its songs or always sits at the top. Options:
   - (a) inline headers, one row before each pack's songs, which scroll with the list (StepMania/ITG section style; with several packs you see where one ends and the next begins);
   - (b) a fixed header at the top showing the current song's pack.

   **Default: (a).** The mock (list start) looks the same either way.
3. **Short lists don't slide.** What the player would notice: when the whole pack fits on screen (7 rows or fewer), or at the very top or bottom of the list, the rows stay still and only the gold bar jumps to the next song. The slide happens only when the list itself moves. Options:
   - (a) as described;
   - (b) also animate the gold bar between rows, which is more code because the selected row is taller.

   **Default: (a).**
4. **Difficulty colours for odd labels.** What the player would notice: a chart labelled "Expert" or "Novice", or with a missing label. Options:
   - (a) colour the row the way OpenITG classifies it ("Expert" → blue Challenge row, "Novice" → by its meter, an unknown label → by its meter) while still showing the simfile's own label text;
   - (b) keep today's rule ("Novice" always purple, anything unrecognised grey).

   **Default: (a)**, which follows the "faithful to OpenITG" principle and reuses the parser's existing `resolve_difficulty`.
5. **Label case.** What the player would notice: "HARD" vs "Hard". **Default:** standard labels and pack names in capitals, as in the mock. Edit chart names and song titles stay exactly as written.
6. **Hint separators.** The issue text writes "↑↓ SONG · ←→ DIFFICULTY · …", but the mock shows plain gaps with no dots. **Default:** gaps, as in the mock.
7. **Scanlines on select.** The issue does not list them, but the manifest says title, select and score screens. **Default:** draw them, as the title screen does.
8. **Banner fallback text.** The manifest says the song title "can be drawn on" `banner_fallback`. **Default:** don't, because the title is printed right below the banner anyway.

---

## Acceptance Criteria

- [ ] Draws `bg_select`, `bar_top` + `title_select_music`, and the speed/scroll chips (`chip` tinted `kCyan` / `kGreen`, text such as "SPEED 2.5x", "UPSCROLL") from the live config
- [ ] The banner draws inside `banner_frame` (`banner_fallback` when the song has none), with the title (`kSongTitle`), artist (`kArtist`) and BPM (`kBpm`) below; long titles are truncated by measured width
- [ ] Difficulty rows use `diff_row_<name>[_selected]` with name, meter, 10 × `diff_tick` (filled up to the meter in the difficulty colour, else `kTickOff`) and best % ("---" when none); Edit charts get a neutral row with their chart name (#84)
- [ ] The wheel uses `wheel_pack`, `wheel_row` and `wheel_row_selected` with `kWheelIndent` by distance from the selection, shows no artist, and scrolls with an eased 80 ms vertical slide on fixed `dt`
- [ ] The hint bar shows "↑↓ SONG  ←→ DIFFICULTY  ENTER PLAY  TAB OPTIONS  ESC TITLE" with gold keys
- [ ] The existing `select_screen_test` behaviour (window logic, key repeat, preview audio, options, Confirm handoff) still passes; the layout matches Pinned Semantics (automated), and the owner's side-by-side check against `docs/cabinet-theme/reference/cabinet-v3-select.png` passes at 1280x720 and 2560x1440
- [ ] The build has no new warnings, the sandboxed `ctest` passes 48/48, the headless select smoke run is clean, and `todo-stories.md` is untouched
