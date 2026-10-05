# Plan: Cabinet Input Remap and Calibration Screens (#97)

## Summary

Replace the bitmap-font `render()` of the two setup screens reached from the options overlay, `InputRemapScreen` (`src/screens/input_remap_screen.cpp:152-232`, colours at `:17-23`) and `CalibrationScreen` (`src/screens/calibration_screen.cpp:136-181`, colours at `:21-24`), with Cabinet parts the select screen and options overlay already use.
Both screens get the same chrome:
- `bg_select` over the window;
- `bar_top` across the top with a **runtime** title ("REMAP INPUT" / "CALIBRATE OFFSET") in the options overlay's title style (`options_art::kTitleStyle`, SairaExtraBold 44 italic);
- `bar_hint` along the bottom with a per-state legend (gold arrows and keys, grey words) built by `select_art::layout_hint_items`;
- the scanlines overlay last, like every other Cabinet surface.

**Remap** shows the binding table as slanted rows on the options overlay's row geometry (560 px wide, centred, `wheel_row` 48 tall, the gold `wheel_row_selected` bar 80 tall). Each row is one binding slot: the action on the left, the bound key right-aligned in gold (dark ink on the gold bar). The table is grouped under two `wheel_pack` header rows, KEYBOARD and PAD (the same pack/song idea the song wheel uses), so the device column is no longer needed. RESET TO DEFAULTS is the last row. The list scrolls with `select_art::list_window` (9 visible rows). The model's transient message ("BOUND TO LEFT", "IN USE: CONFIRM", "DEFAULTS RESTORED") becomes a gold chip on the right of the top bar.

**Calibration** shows the phase word (GET READY / TAP ON THE BEAT / DONE) centred in a large gold Saira style. Below it are two slanted `stat_panel` plates side by side, SAMPLES and OFFSET (the score screen's plate), each with a centred label and value. The no-audio notice is a centred red line.

All new layout and drawing lives in one new GL-free module, `src/screens/setup_art.{hpp,cpp}`, split into pure reference-px layout and thin draw helpers, the same pattern as `options_art`, `select_art` and `results_art`. A new `setup_art_test` pins the layout, the fit with the real fonts and the legends, and renders both screens with the real theme and fonts.
Input handling, the remap model (`input_remap.*`), capture mode, the calibration timing path and math, the save rule and Esc-back-to-options (`ScreenManager`) are **not touched**. `input_remap_screen_test` and `calibration_screen_test` are **not modified** and must stay green.

## User Story

As a player
I want the Input Remap and Calibration screens to look like the rest of the Cabinet theme
So that the whole loop (title, select, options, remap, calibrate, gameplay, score) looks like one game, while rebinding keys and calibrating my offset work exactly as before

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | MEDIUM (the issue says M: two `render()` bodies replaced, one new art module, one new test executable) |
| Systems Affected | new `src/screens/setup_art.*`; `src/screens/input_remap_screen.*` and `src/screens/calibration_screen.*` (render only); `src/screens/options_art.hpp` (one comment); `CMakeLists.txt` (core sources); `tests/CMakeLists.txt` + new `tests/setup_art_test.cpp` |
| GitHub Issue | #97 (TODO-25; blocked by #94, closed; blocks #98) |
| Plan sequence | 049 (running plan sequence; the issue number is #97) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release, `/usr/bin/c++`). Build with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **48/48 pass** | Run on `main` @ `a0b72e7` with the sandboxed ctest command in Validation. After this plan the count is **49**, because `setup_art_test` is added |
| Sandbox requirement | — | The screen tests and `audio_test` open the real sound device. **Always** run ctest (and any binary) inside the `bwrap` command in Validation |
| Remap rows | `src/data/config_loader.cpp:251-273`, `src/screens/input_remap.cpp:48-55` | The defaults give **12 keyboard + 12 pad slots** (Left/Down/Up/Right ×2, Confirm ×2 / ×1, Back ×1, Options ×1 / ×2). Keyboard rows always come first, then pad rows. The count depends on the config, so the layout must not assume 24 |
| Remap display today | `input_remap_screen.cpp:166-215` | ACTION, [DEVICE] and KEY columns. During capture **no row is highlighted** and the value reads `<PRESS>` (`remap_row_value_text`, `input_remap.cpp:159-167`). RESET is a trailing row selected by `reset_selected_` |
| Calibration display today | `calibration_screen.cpp:146-180` | Phase word. "SAMPLES n / min" (or "SAMPLES n" once ready). "OFFSET ±x.xxx s" only when `result_.ready && !synthetic_`. The no-audio notice replaces the key legend |
| Theme art (manifest) | `assets/theme/cabinet/manifest.json` | `bg_select` fullscreen ("Song select / options background"). `bar_top` stretch_x 132 @2x (64 + 2 rule). `bar_hint` stretch_x 108 @2x. `wheel_row` slice3 564x62 ref (caps 68/68). `wheel_pack` slice3 564x62 ref (caps 120/68, caret baked in, text x+54). `wheel_row_selected` **sprite** 640x92 ref (keep the aspect; see `options_art::kRowSelectedHeight`). `stat_panel` slice3 360x96 ref (the score screen draws it 360x111). `chip` slice3, tinted. Title sprites `title_*` are 300x46 at (40, 9); a runtime title replaces them here |
| `digits_white` glyphs | manifest `bitmap_fonts` | Only `0-9 . % / space`. There is **no `+`, `-` or `s`**, so the offset cannot use the digit font. Values use TTF |
| Reusable layout | `src/screens/options_art.hpp:41-93, 108-117` | `kRowX 360`, `kRowWidth 560`, `kRowHeight 48`, `kRowSelectedHeight 80`, `kRowPitch 58`, `kNameValueGap 24`, `name_x()`, `value_right()`, `kTitleStyle`. All public |
| Reusable helpers | `select_art.hpp:146-151, 272-299, 333-334, 353-354, 405-407`; `results_art.hpp:64-68` | `list_window`, `visible_rows`, `HintItem` / `layout_hint_items` / `draw_hint_line`, `draw_backdrop` (bg_select), `draw_scanlines`, `kWheelPackTextX 54`, `kHintBarTop 666`; `kStatPanelHeights[0] 111`, `kStatLabelTop 14`, `kValueBaseline 86` |
| Pre-baked atlases | `theme.hpp:154-161` (`kAllStyles`, 26 entries, pinned by `tests/ttf_font_test.cpp`) | Every style used below shares a pre-baked (font, size). **Do not** add to `kAllStyles` |
| Text widths (PIL, real fonts, ref px @720p) | `assets/fonts/SairaCondensed-*.ttf` | Title (XB 44, tracking 4): "REMAP INPUT" 263, "CALIBRATE OFFSET" 355. Phase (XB 44, tracking 6): "TAP ON THE BEAT" 360, "GET READY" 225, "DONE" 110. Row names (Bold 28 / XB 40): "CONFIRM" 91 / 132, "RESET TO DEFAULTS" 195 / 281. Keys (XB 28 + 3 tracking / XB 40): "rightshoulder" 177 / 197, "Keypad Enter" 171 / 193, "<PRESS>" 112 / 130. Pack (XB 28 + 3): "KEYBOARD" 133. Chip (XB 18 + 3): "DEFAULTS RESTORED" 185. Notice (Bold 28): "AUDIO UNAVAILABLE - OFFSET WILL NOT BE SAVED" 494. Plate values (XB 34): "-3600.000 s" 150, "12 / 8" 67. Legend word "PRESS A KEY OR PAD BUTTON" (Bold 20 + 2) 257 |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #97

| Where | Note | This plan |
|---|---|---|
| `src/screens/options_art.hpp:124-125` | "`draw_panel` … Public so the remap / calibration restyle (#97) can reuse it." | **Not used**. These are full screens, not overlays, so they use the screen chrome (bg + bars), not the options panel. Reword the comment to "The navy panel body + 2px ring, the bar_top header and the "OPTIONS" title." (drop the #97 sentence). The row geometry, `name_x`, `value_right` and `kTitleStyle` **are** reused |
| `.agents/reviews/feature-048-options-overlay-cabinet-review.md:62` | The `layout_hint_items` trailing-gap finding, "#97 is likely to reuse it" | Already fixed in `ceec6fb` (`select_art.cpp:309`: `x -= gap` whatever the last kind; Key+Word kept whole). The legends below rely on it. `setup_art_test` pins a line that ends on a Word after a Key |
| `docs/cabinet-theme/IMPLEMENTATION_PLAN.md:131-134` | Step 7: "`bg_select`, the top bar with a runtime title, slanted rows for the binding table (gold selected row) and the hint bar. … add a baked `GET READY` / `TAP ON THE BEAT` / `DONE` set if you want it fully chrome." | Followed. The baked phase sprites are **optional and not done** (the issue's technical notes say runtime text is fine) |
| #98 AC "No screen still uses `bitmap_font` for player-facing text" | — | After this plan neither setup screen includes `render/bitmap_font.hpp` (static check in Validation) |
| #74 (calibration offset bug) | Independent (issue note) | Not touched. No change to `update()`, `MusicClock`, `OffsetCalibration` or the save path |

---

## Patterns to Follow

### Screen render: layout scale, null-checked services, back to front

```cpp
// SOURCE: src/screens/select_screen.cpp:571-584, 664-677
const theme::LayoutScale L = theme::layout_scale(w, h);
const ThemeTextures* theme = ctx.theme;
TextRenderer* text = ctx.text;
if (theme != nullptr) {
    select_art::draw_backdrop(*theme, renderer, w, h);
}
...
select_art::draw_hint_bar(theme, text, renderer, L, w);
...
if (theme != nullptr) {
    select_art::draw_scanlines(*theme, renderer, w, h, L);
}
```

### Art module: constants + pure reference-px layout + thin draw helpers

```cpp
// SOURCE: src/screens/options_art.hpp:1-16
// Cabinet options overlay art (#96), drawn by SelectScreen over song select.
// A free design (the Cabinet mock-ups have no options screen) built from select's parts ...
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so select_art_test
//    pins them headless. Every rect is a manifest *content box*.
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
// Presentation only: nothing here reads the music clock or wall time.
```

### Rows: art first, then text grouped by style, truncation only on the rare path

```cpp
// SOURCE: src/screens/options_art.cpp:39-63 (draw_name: budget, truncate only if over),
//         src/screens/options_art.cpp:145-179 (draw_rows: wheel_row slice3, then the
//         wheel_row_selected stretch, then names / values / selected pair)
theme->draw_slice3(renderer, "wheel_row", L.rect(row_rect(i, selected)));
theme->draw_stretch(renderer, "wheel_row_selected", L.rect(row_rect(selected, selected)));
```

### Legend from items (shared builder)

```cpp
// SOURCE: src/screens/options_art.cpp:25-36, 182-198
using Item = select_art::HintItem;
constexpr std::array<Item, 8> kOptionsHintItems = {{ {Item::Kind::VArrows, {}}, {Item::Kind::Word, "ROW"}, ... }};
select_art::draw_hint_line(*text, renderer, L,
    select_art::layout_hint_items(items,
        [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintKey); },
        [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintWord); }));
```

### Scrolling list window (centred while it can slide, clamped at the ends)

```cpp
// SOURCE: src/screens/select_art.cpp:95-116
ListWindow list_window(int selected, int count, int visible);
int visible_rows(float list_top, float list_bottom, float row_pitch, float selected_h);
```

### Slanted plate + text on a baseline

```cpp
// SOURCE: src/screens/results_art.cpp:84-97 (stat_panel_rect, stat_text_x skew correction),
//         src/screens/results_art.cpp:233-235 (top = L.y(baseline) - text->ascent(style))
theme.draw_slice3(renderer, "stat_panel", L.rect(stat_panel_rect(i)), ...);
```

### Chip (tinted slice3 + same-colour text)

```cpp
// SOURCE: src/screens/select_art.cpp:385-407 (draw_chips)
theme->draw_slice3(renderer, "chip", L.rect(rect), tint);
text->draw(renderer, label, L.x(rect.x + rect.w * 0.5f), centred_top(*text, L, rect.y, rect.h, tinted), tinted, TextAlign::Centre);
```

### Error handling (presentation code: null-guard, never throw)

```cpp
// SOURCE: src/screens/options_art.cpp:127-138
if (theme != nullptr) { theme->draw_stretch_x(renderer, "bar_top", ...); }
if (text != nullptr) { text->draw(renderer, "OPTIONS", ...); }
```

There are no exceptions or error codes on this path. A missing service skips its draws, a missing texture falls back inside `ThemeTextures`, and the renderer is a no-op when uninitialised (headless tests).

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real assets headless)

```cpp
// SOURCE: tests/select_art_test.cpp:43-51, 104-118; tests/CMakeLists.txt:549-565
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
blaze4k::ThemeTextures& loaded_theme();   // real manifest (BLAZE4K_ASSETS_DIR), headless
blaze4k::TextRenderer& loaded_text();     // real fonts (BLAZE4K_SOURCE_DIR), set_window_size(1280, 720)
// Fake IAudioStream + injected MusicClock source: tests/calibration_screen_test.cpp:48-75, 109-142
```

---

## Pinned Semantics (the contract the tests pin)

All `setup_art` values are **reference px** (1280x720). Screen px = `L.x / L.y / L.px / L.rect` with `L = theme::layout_scale(w, h)`.

### Shared chrome (both screens)

| Element | Value |
|---|---|
| Backdrop | `select_art::draw_backdrop` (`bg_select` over `{0, 0, w, h}`), theme only |
| Top bar | `bar_top` via `draw_stretch_x(renderer, "bar_top", 0, L.y(0), w, L.s)` (as `select_art::draw_top_bar`, without the sprite) |
| Title | the runtime string at `x = theme::layout::kTopBarPadX (40)`, `TextAlign::Left`, line box centred in the band `[0, 64]` (`kTopBarHeight`), style `options_art::kTitleStyle` (SairaExtraBold 44, tracking 4, italic, white, Hard3). Constants `kRemapTitle = "REMAP INPUT"`, `kCalibrateTitle = "CALIBRATE OFFSET"` |
| Hint bar | `bar_hint` exactly as `options_art::draw_hint_bar` places it, then `select_art::draw_hint_line(…, layout_hint_items(items, …))` |
| Scanlines | `select_art::draw_scanlines`, the **last** draw (theme only) |

### Legends (`setup_art.hpp`, public `constexpr std::array<select_art::HintItem, N>`)

| Constant | Items | Pieces | When |
|---|---|---|---|
| `kRemapBrowseHint` | ↑↓ SELECT, ENTER REBIND, ESC BACK | 6 | not capturing, on a binding row |
| `kRemapResetHint` | ↑↓ SELECT, ENTER RESET, ESC BACK | 6 | not capturing, on the RESET row |
| `kRemapCaptureHint` | PRESS A KEY OR PAD BUTTON (Word), ESC CANCEL | 3 | capturing |
| `kCalibrateHint` | ↑↓ ←→ TAP, ENTER SAVE, ESC CANCEL | 9 | audio available |
| `kCalibrateNoAudioHint` | ↑↓ ←→ TAP, ESC CANCEL | 7 | synthetic clock (Enter cannot save) |

The words follow today's legends (`input_remap_screen.cpp:225-227`, `calibration_screen.cpp:177-179`), with `[BACK]` shown as ESC (as on select and the options overlay) and `[ANY ARROW]` drawn as the four gold arrows. "Esc" here is the reserved Back. Pad Back does the same, but it is left out to keep the line short.

### Remap table

| Constant / function | Value | Derivation |
|---|---|---|
| Row x / width / heights / pitch | `options_art::kRowX 360`, `kRowWidth 560`, `kRowHeight 48`, `kRowSelectedHeight 80`, `kRowPitch 58` | The options overlay's rows, so the slant and the gold bar's aspect are the same |
| `kRemapListTop` | `90` | `kTopBarHeight + 2 + 24` |
| `kRemapListBottom` | `642` | `select_art::kHintBarTop (666) − 24` |
| `kRemapVisibleRows` | `9` | `select_art::visible_rows(90, 642, 58, 80)`. Pinned in a test, and a `static_assert` that `90 + 8·58 + 32 + 48 = 634 ≤ 642` |
| `remap_row_rect(slot, selected_slot)` | `{360, 90 + slot·58 + (slot > selected_slot ? 32 : 0), 560, slot == selected_slot ? 80 : 48}` | The `options_art::row_rect` formula with a different top. `slot` is relative to the window's first row |
| Display rows | a `wheel_pack` header before each run of same-device rows (KEYBOARD, PAD), then the binding rows, then one RESET row. With the defaults: header 0, keyboard rows 1-12, header 13, pad rows 14-25, RESET 26 (27 rows) | Pure, non-allocating functions over `std::span<const RemapRow>`: `remap_display_count(rows)`, `remap_display_row(rows, d) -> {Kind Header / Binding / Reset, int binding (Binding), DeviceType device (Header)}`, `remap_display_index(rows, binding_row)` (clamped; empty rows → 0), and `remap_reset_display_index(rows) = count − 1`. Headers are **display only**: navigation still moves over the model's rows (`update()` is unchanged) |
| Selected display row | `reset_selected_ ? remap_reset_display_index : remap_display_index(model.row)`. While **capturing**, the gold bar stays on the row being rebound (Open Question 1) | |
| Window | `select_art::list_window(selected_display, count, kRemapVisibleRows)` | Header rows scroll like any row |
| Header row | `draw_slice3("wheel_pack")` at the 48 px row rect. The label at `row.x + select_art::kWheelPackTextX (54)` in `theme::text::kWheelPack` (pack ink) | The song wheel's pack header |
| Binding row, unselected | `wheel_row`. The action at `options_art::name_x(row, false)` in `kWheelRow`. The key right-aligned at `options_art::value_right(row, false)` in `with_color(kWheelPack, kGold)` (the options value style) | |
| Binding row, selected | `wheel_row_selected`. The action and key in `kWheelSelected` (dark ink), at `name_x(row, true)` / `value_right(row, true)` | |
| RESET row | `wheel_row` with "RESET TO DEFAULTS" at `name_x` in `with_color(kWheelRow, theme::color::kMissLabel)`. Selected: the gold bar with `kWheelSelected` ink. No value | Today's distinct reset colour (`kResetColor`), now a theme colour |
| Key truncation | if `name_w + kNameValueGap + key_w` exceeds `value_right − name_x`, the **key** is truncated to the remaining budget with `text->truncate` (rare path only; action names are a fixed short set) | `test_remap_text_fits` proves it never triggers for the default bindings or for `<PRESS>` |
| Labels without allocation | `remap_action_label(GameAction) -> std::string_view` (a switch of literals, pinned equal to `remap_action_name` for all seven actions) and `remap_value_view(model, i) -> std::string_view` (`"<PRESS>"` on the capturing row, else `rows[i].name`; pinned equal to `remap_row_value_text`) | `render()` then builds no strings |
| Message chip | if `model.message` is non-empty: `chip` slice3 tinted `kGold` at `{1240 − (text_w + 44), 12, text_w + 44, 40}` (`select_art::kChipRight / kChipTop / kChipHeight / kChipPadX`), with the text centred in `with_color(theme::text::kChip, kGold)` | Where select's chips sit. The title ends at 40 + 355 = 395 at most, and the widest message ("DEFAULTS RESTORED", 185) starts at 1011 |

### Calibration

| Constant / function | Value | Derivation |
|---|---|---|
| `kPhaseStyle` | `{SairaExtraBold, 44, 6, true, kGold, Hard3}` | The pre-baked 44 atlas (as `kTitleStyle`), wider tracking for a headline |
| Phase word | centred on x 640, line box centred in the band `{top 200, h 64}` | Today's phase-word position (32% of the height) moved into the free area under the bar |
| `kCalPlateWidth` / `kCalPlateHeight` / `kCalPlateGap` | `360` / `111` (`results_art::kStatPanelHeights[0]`) / `20` | The score screen's `stat_panel` size |
| `calibration_plate_rect(i)` | `i == 0` (SAMPLES): `{270, 300, 360, 111}`; `i == 1` (OFFSET): `{650, 300, 360, 111}` (clamped) | Centred pair: 270 + 360 + 20 + 360 = 1010, so the centre is (270 + 1010) / 2 = 640 |
| Plate label | "SAMPLES" / "OFFSET" in `theme::text::kStatLabel` (ice, 18, tracking 4), `TextAlign::Centre`, line top `plate.y + results_art::kStatLabelTop (14)` | The score screen's label |
| Plate value | in `theme::text::kComboNumber` (SairaExtraBold 34 italic white, Hard3), `TextAlign::Centre`, baseline `plate.y + results_art::kValueBaseline (86)` (top = `L.y(baseline) − text.ascent(style)`) | The score screen's value baseline |
| Centred x in a slanted plate | `plate_centre_x(plate, centre_y) = plate.x + plate.w / 2 + theme::skew::kStatPanel · (plate.y + plate.h / 2 − centre_y)` | `results_art::stat_text_x`, centred instead of left |
| SAMPLES value | `calibration_samples_text(count, min, ready)`: `ready ? "12" : "3 / 8"` | Today's "SAMPLES n" / "SAMPLES n / min" minus the label. At most 15 chars, so it fits SSO and does not heap-allocate |
| OFFSET value | `result.ready && !synthetic ? format_offset(offset) : "---"`. The "---" is drawn in `with_color(kComboNumber, kSteel)` | Today the readout is hidden until ready, and always hidden on the synthetic clock (`calibration_screen.cpp:166-173`). The plate stays, so the layout does not jump |
| No-audio notice | `synthetic_` only: "AUDIO UNAVAILABLE - OFFSET WILL NOT BE SAVED", centred on x 640, line box centred in `{top 450, h 40}`, style `with_color(theme::text::kWheelRow, theme::color::kMissLabel)` | Today the notice replaces the legend. Now the legend switches to `kCalibrateNoAudioHint` and the notice gets its own line |
| Phase → word | stays in `calibration_screen.cpp` (the existing `switch`, `:149-160`) and is passed to `setup_art` as a `string_view` | `setup_art` does not include the audio / clock headers |

### Draw order

- **Remap**: backdrop → top bar + title → message chip → header/row art → row text in style groups (headers, unselected actions, unselected keys, RESET label, selected pair) → `bar_hint` + legend → scanlines.
- **Calibration**: backdrop → top bar + title → `stat_panel` plates (`draw_slice3("stat_panel", …)`) → phase word → plate labels → plate values → notice → `bar_hint` + legend → scanlines.

Both `render()`s keep their `w <= 0 || h <= 0` early return. Neither reads a clock: calibration's `render()` uses only `phase_`, `result_`, `sample_count()`, `config_.min_samples` and `synthetic_`, as today.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/setup_art.hpp` | CREATE | Header comment (house style). Constants and legends (Pinned Semantics); the remap display-row functions, `remap_row_rect`, `remap_action_label`, `remap_value_view`, `remap_chip_rect(text_w)`; `calibration_plate_rect`, `plate_centre_x`, `calibration_samples_text`; draw helpers `draw_chrome`, `draw_hint_bar`, `draw_remap_table`, `draw_message_chip`, `draw_calibration`; `static_assert`s |
| `src/screens/setup_art.cpp` | CREATE | Their implementation. A private `centred_top` copy (as `options_art.cpp:18-21` did) |
| `src/screens/input_remap_screen.cpp` / `.hpp` | UPDATE | `render()` calls `setup_art`. Delete the 7 colour constants, the `render/bitmap_font.hpp` include and the old layout. Add one line to the class comment ("drawn in the Cabinet look by setup_art, #97"). **`enter` / `update` / `exit` / `handle_back` / `commit` / capture are byte-identical** |
| `src/screens/calibration_screen.cpp` / `.hpp` | UPDATE | Same for calibration: delete the 4 colours, the bitmap include and the inline `backdrop`. **Constructors, `enter`, `update`, `exit` are byte-identical** |
| `src/screens/options_art.hpp` | UPDATE | Reword the `draw_panel` comment (drop the #97 forward reference) |
| `CMakeLists.txt` | UPDATE | Add `src/screens/setup_art.cpp` to the core sources after `src/screens/options_art.cpp` (line 131) |
| `tests/setup_art_test.cpp` | CREATE | Layout, display rows, labels, legends, text fit, pre-baked styles, manifest names, render smoke of both screens with real services |
| `tests/CMakeLists.txt` | UPDATE | Register `setup_art_test` with `BLAZE4K_SOURCE_DIR` / `BLAZE4K_ASSETS_DIR` (copy the `select_art_test` block, `:549-565`) |

`input_remap.{hpp,cpp}`, `offset_calibration.*`, `music_clock.*`, `metronome.*`, `screen_manager.*`, `options_menu.*`, `select_screen.*`, `theme.hpp`, the manifest, `tests/input_remap_screen_test.cpp`, `tests/calibration_screen_test.cpp` and `tests/input_remap_test.cpp` are **unchanged**.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Build after each with `cmake --build build -j$(nproc)`.

### Task 1: `setup_art` pure layout and labels

- **File**: `src/screens/setup_art.hpp`, `src/screens/setup_art.cpp`, `CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**:
  - Namespace `blaze4k::setup_art`. The header comment in the `options_art.hpp:1-16` style: the Cabinet Input Remap and Calibration screens (#97), a free design (not in the mock-ups) built from select's, the options overlay's and the score screen's parts; pure reference-px layout plus thin draw helpers; presentation only, no clock. Include `options_art.hpp`, `select_art.hpp`, `results_art.hpp`, `input_remap.hpp` (no audio/clock headers).
  - Every constant from Pinned Semantics as `inline constexpr`, each with a one-line derivation comment, including the five legend arrays and the two title strings.
  - `static_assert`s: `kRemapListTop + (kRemapVisibleRows − 1) · kRowPitch + (kRowSelectedHeight − kRowHeight) + kRowHeight <= kRemapListBottom`, and adding one row breaks it (`+ kRowPitch > kRemapListBottom`); the plate pair is centred on `kRefWidth / 2`; the plates and the notice band end above `select_art::kHintBarTop`.
  - Pure functions: `remap_display_count`, `remap_display_row`, `remap_display_index`, `remap_reset_display_index`, `remap_row_rect`, `remap_chip_rect(float text_w)`, `remap_action_label`, `remap_value_view`, `calibration_plate_rect`, `plate_centre_x`, `calibration_samples_text`. None allocates except `calibration_samples_text` (SSO-sized).
  - Add `src/screens/setup_art.cpp` to `CMakeLists.txt` after `options_art.cpp`.
- **Mirror**: `src/screens/options_art.hpp:36-117`, `src/screens/options_art.cpp:69-111`, `src/screens/select_art.cpp:95-116`, `src/screens/results_art.cpp:84-97`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 2: `setup_art` draw helpers

- **File**: `src/screens/setup_art.hpp`, `src/screens/setup_art.cpp`
- **Action**: UPDATE
- **Implement** (services are pointers and null-guarded; the helpers rely on `GlQuadRenderer`'s own no-op when uninitialised, as `options_art` does):
  - `draw_chrome(const ThemeTextures*, TextRenderer*, GlQuadRenderer&, const LayoutScale& L, int w, int h, std::string_view title)`: backdrop, `bar_top`, title.
  - `draw_hint_bar(theme, text, renderer, L, int w, std::span<const select_art::HintItem> items)`: `bar_hint` plus `draw_hint_line(layout_hint_items(items, …))`.
  - `draw_message_chip(theme, text, renderer, L, std::string_view message)`: no-op when empty or `text == nullptr` (the width needs text).
  - `draw_remap_table(theme, text, renderer, L, const InputRemapModel& model, bool reset_selected)`: window, art pass, text pass, per Pinned Semantics.
  - `struct CalibrationView { std::string_view phase_word; std::string_view samples; std::string_view offset; bool offset_ready; bool synthetic; };` and `draw_calibration(theme, text, renderer, L, const CalibrationView&)`: plates, phase word, labels, values, notice.
- **Mirror**: `src/screens/options_art.cpp:122-209`, `src/screens/select_art.cpp:379-407`, `src/screens/results_art.cpp:216-264`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 3: `InputRemapScreen::render` uses `setup_art`

- **File**: `src/screens/input_remap_screen.cpp`, `src/screens/input_remap_screen.hpp`
- **Action**: UPDATE
- **Implement**:
  - Replace the body of `render(ScreenContext& ctx, …)` (now uses `ctx`): the early return; `L`; `draw_chrome(…, setup_art::kRemapTitle)`; `draw_message_chip(…, model_.message)`; `draw_remap_table(…, model_, reset_selected_)`; `draw_hint_bar(…, model_.capturing ? kRemapCaptureHint : reset_selected_ ? kRemapResetHint : kRemapBrowseHint)`; scanlines if the theme is non-null.
  - Delete `kBackdrop … kResetColor` and their anonymous namespace (if it becomes empty), plus the `render/bitmap_font.hpp` and `<algorithm>` includes if unused. Add `render/theme_layout.hpp`, `render/theme_textures.hpp` and `screens/setup_art.hpp`.
  - Add the class-comment line. **No other change.**
- **Mirror**: `src/screens/select_screen.cpp:571-584, 660-678`
- **Validate**: build, then the sandboxed ctest `-R "input_remap"` (both `input_remap_test` and `input_remap_screen_test`, unmodified)

### Task 4: `CalibrationScreen::render` uses `setup_art`

- **File**: `src/screens/calibration_screen.cpp`, `src/screens/calibration_screen.hpp`
- **Action**: UPDATE
- **Implement**:
  - Replace the body of `render`: the early return; `L`; `draw_chrome(…, kCalibrateTitle)`; keep the existing phase `switch` to pick the word; build `samples = calibration_samples_text(sample_count(), config_.min_samples, result_.ready)` and `offset = (result_.ready && !synthetic_) ? format_offset(result_.offset_seconds) : "---"` (both SSO-sized locals); `draw_calibration(…, view)`; `draw_hint_bar(…, synthetic_ ? kCalibrateNoAudioHint : kCalibrateHint)`; scanlines.
  - Delete `kDimColor`, `kTitleColor`, `kAccentColor` and `kHintColor` and the `render/bitmap_font.hpp` include. Keep `kFallbackBpm` and `is_panel_action`. Keep `options_menu.hpp` (for `format_offset`).
  - Add the class-comment line. **`update()`, `enter()`, `exit()` and the constructors are not edited** (principle 1: presentation only).
- **Mirror**: `src/screens/results_art.cpp:216-264` (bars, then plates, then text)
- **Validate**: build, then the sandboxed ctest `-R "calibration|offset_calibration|metronome"`

### Task 5: `options_art` comment

- **File**: `src/screens/options_art.hpp:124-125`
- **Action**: UPDATE (comment only): drop "Public so the remap / calibration restyle (#97) can reuse it."
- **Validate**: build

### Task 6: `setup_art_test`

- **File**: `tests/setup_art_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement** (copy `TEST_CHECK`, `approx`, `rect_eq`, `press`, `loaded_theme`, `loaded_text` and the size table from `select_art_test.cpp:43-118`. Copy a minimal `FakeAudioStream` from `calibration_screen_test.cpp:48-75`. The header comment lists what is pinned):
  - `test_remap_display_rows`: with the defaults, count 27; row 0 is the KEYBOARD header and row 13 the PAD header; `remap_display_index(rows, 0) == 1`, `(rows, 11) == 12`, `(rows, 12) == 14`, `(rows, 23) == 25`; the reset index is 26. Round trip: for every binding row `b`, `remap_display_row(rows, remap_display_index(rows, b)) == {Binding, b}`. Keyboard-only rows give one header. Empty rows give count 1 (RESET only), and `remap_display_index(empty, 0) == 0`. Out-of-range `d` clamps.
  - `test_remap_layout`: `kRemapVisibleRows == select_art::visible_rows(90, 642, 58, 80) == 9`; `remap_row_rect(0, 0) == {360, 90, 560, 80}`, `remap_row_rect(1, 0) == {360, 180, 560, 48}`, `remap_row_rect(8, 8) == {360, 554, 560, 80}`. For every selected slot 0..8, the rows don't overlap, stay within `[90, 642]`, and exactly one is 80 tall. 1440p mapping: `L.rect(remap_row_rect(0, 0)) == {720, 180, 1120, 160}`. `remap_chip_rect(185)` ends at 1240 and starts after the widest title (395). `list_window` with the defaults at selected 0, 13 and 26 shows the selection.
  - `test_remap_labels`: `remap_action_label(a) == remap_action_name(a)` for the seven actions. `remap_value_view(model, i) == remap_row_value_text(model, i)` for every row, capturing and not (capture on rows 0 and 23).
  - `test_calibration_layout`: the plate rects; centred on 640; inside `[66, kHintBarTop]`; the notice band is below the plates; `plate_centre_x` equals the plate centre at the plate's mid-height and shifts by `kStatPanel · dy`. `calibration_samples_text(3, 8, false) == "3 / 8"` and `(12, 8, true) == "12"`.
  - `test_legends`: for each of the five legends with the real measures, the piece counts are 6 / 6 / 3 / 9 / 7, the kinds/texts are in order, the line is centred on 640 (± 1e-3) and its width is `< 1280 − 2·40`.
  - `test_text_fits` (`loaded_text()` at 1280x720): both titles' widths `< remap_chip_rect(widest message).x − 40 − 24`; the title fits the 64 px band (same intent check as `test_options_text_fits`, see 048 report deviation 4); the phase words `< 1280 − 2·40`. For every default binding row (selected and unselected) and `<PRESS>`: `name_x + name_w + kNameValueGap ≤ value_right − key_w`. "RESET TO DEFAULTS" fits both styles. "KEYBOARD"/"PAD" fit the pack row. The plate values ("-3600.000 s", "+0.023 s", "---", "12 / 8", "999") and labels fit `kCalPlateWidth − 2·20`. The notice is `< 1280 − 2·40`.
  - `test_styles_prebaked`: `kPhaseStyle`, `options_art::kTitleStyle`, `kWheelRow`, `kWheelPack`, `kWheelSelected`, `kChip`, `kStatLabel`, `kComboNumber`, `kHintKey` and `kHintWord` each share some `kAllStyles` (font, size_px); `kAllStyles.size() == 26`.
  - `test_texture_names_exist`: `bg_select`, `bar_top`, `bar_hint`, `wheel_row`, `wheel_pack`, `wheel_row_selected`, `stat_panel`, `chip` and `scanlines` have `loaded_theme().entry(name) != nullptr`.
  - `test_render_smoke`: for services real and null, at the sizes {1280x720, 2560x1440, 3440x1440, 1920x1200, 0x0}:
    - **Remap**: an `InputRemapScreen` with `ctx.config` / `ctx.theme` / `ctx.text` set (`ctx.input` null). Render after `enter`. Then press `Down` ×30 (render after each, so every row and RESET is the selection, clamped), `Up`, then `Down` back onto RESET and `Confirm` (a reset sets the message chip), then `Up` + `Confirm` to start capture (render: the capture legend), then `handle_back` to cancel.
    - **Calibration**: a `CalibrationScreen(stream, source, ccfg)` with a frame-driven source. Render in CountIn, then after the lead-in (Sampling), then after `min_samples` taps (Ready, offset shown). Also a `CalibrationScreen(failing_stream, {}, ccfg)` (stream `load_result = false`): render the synthetic path (notice + no-audio legend).
  - `tests/CMakeLists.txt`: the `setup_art_test` block mirrors `select_art_test` (`:549-565`), with a comment line "#97: Cabinet Input Remap + Calibration art".
- **Mirror**: `tests/select_art_test.cpp:653-887` (options tests), `tests/calibration_screen_test.cpp:109-153`, `tests/input_remap_screen_test.cpp:80-97`
- **Validate**: the sandboxed ctest `-R setup_art_test`

### Task 7: Full validation

- Run the whole Validation block. **49/49** must pass, with no new warnings.

---

## Validation

```bash
# Build (host, existing build dir)
cmake --build build -j$(nproc)

# Lint: no linter configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning" | grep -E "setup_art|input_remap_screen|calibration_screen|options_art" || echo "no new warnings"

# Tests (MUST run sandboxed: the tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **49/49 pass** (baseline 48/48 on `a0b72e7`, plus `setup_art_test`). This includes `input_remap_screen_test`, `calibration_screen_test`, `input_remap_test` and `offset_calibration_test`, all unmodified.

Static checks:

```bash
# No bitmap text left on either screen
grep -nE "draw_text|bitmap_font" src/screens/input_remap_screen.cpp src/screens/calibration_screen.cpp   # expect no hits
# Nothing in the new art reads a clock
grep -nE "steady_clock|SDL_GetTicks|music_clock|time_seconds|MusicClock" src/screens/setup_art.cpp src/screens/setup_art.hpp   # expect no hits
# Behaviour files and the two screen tests are untouched
git diff --stat -- src/screens/input_remap.cpp src/screens/input_remap.hpp src/timing src/audio src/screens/screen_manager.cpp tests/input_remap_screen_test.cpp tests/calibration_screen_test.cpp tests/input_remap_test.cpp   # expect empty
# Only render() changed in the two screens (review the hunks: every hunk lies inside render(), the includes, the colour block or the class comment)
git diff -U0 -- src/screens/input_remap_screen.cpp src/screens/calibration_screen.cpp
```

## End-to-End Verification

1. **Automated (agent-runnable):** `input_remap_screen_test` and `calibration_screen_test` (unmodified) drive the real input, capture, conflict, reset, measure, save, abort and synthetic paths. `setup_art_test` renders both screens through every state (every remap row as the selection, RESET, the message chip, capture; calibration CountIn / Sampling / Ready / no audio), with the real manifest and fonts at five window sizes and with null services. All must pass under the sandboxed ctest.
2. **Headless app smoke (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and no new error lines (the new TU links and the app starts). `--start-screen` accepts only `title` / `select` (`src/main.cpp:42, 382-386`), so the two setup screens are not reachable headless from the CLI. Step 1's render walk is their automated coverage. Do **not** add a CLI flag for this.
3. **Windowed visual check (owner; the agent must not launch the GUI, since it opens a window and plays audio):** start the game, go to song select, press Tab.
   - **Remap**: on REMAP INPUT, press Enter. Expect the `bg_select` backdrop, the chrome top bar with an italic white "REMAP INPUT", and a centred column of slanted rows: a gold-orange KEYBOARD header, then LEFT/DOWN/… with gold keys on the right. The selected row is the gold bar with dark text. Up/Down scroll the list through the PAD header to RESET TO DEFAULTS (red text). The bottom bar reads "↑↓ SELECT  ENTER REBIND  ESC BACK" (ENTER RESET on the reset row).
     - Press Enter on a row: the key shows `<PRESS>` and the legend reads "PRESS A KEY OR PAD BUTTON  ESC CANCEL". Press a free key: it binds, and a gold "BOUND TO …" chip shows at top right. Press a key that is in use: "IN USE: …" shows.
     - Esc returns to the options overlay on REMAP INPUT.
   - **Calibration**: on CALIBRATE OFFSET, press Enter. Expect "CALIBRATE OFFSET" in the bar, a big gold "GET READY", then "TAP ON THE BEAT" once the clicks start. The SAMPLES plate counts "n / 8", and the OFFSET plate shows "---" until ready. Then "DONE" with the offset. Enter saves and returns. Esc cancels back to the options overlay without saving.
   - Repeat maximised (2560x1440 if available) and in a 4:3 or 21:9 window: the text is sharp and centred, and the scanlines cover everything.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A render edit leaks into behaviour (capture, reset, save, Esc-back) | Only `render()`, the includes, the colour constants and the class comments change. Both screen tests stay **unmodified** and green. The `git diff -U0` hunk review is in Validation | In scope |
| Calibration timing path touched (principle 1) | `update()` / `enter()` / clock wiring are not edited. `setup_art` includes no clock or audio header. The static grep is in Validation | In scope |
| Header rows break the row ↔ selection mapping (wrong row highlighted) | Pure `remap_display_*` functions with a round-trip test over every binding row, keyboard-only and empty models. Navigation still moves over the model's rows | In scope |
| Fewer visible rows than today (9 vs about 15), so more scrolling | Every row and RESET stay reachable (`list_window` keeps the selection visible; the render walk covers all 27 rows). This is the same density as the options overlay and the wheel. Flagged in Open Question 2 | In scope (flag) |
| Long or custom SDL key names overflow the row | Rare-path key truncation against the name budget. The fit test covers the defaults and `<PRESS>` | In scope |
| `wheel_row_selected` slant distorted | The options overlay's 560x80 (0.6% off the sprite aspect), already pinned in `select_art_test` | In scope |
| A new style triggers a mid-screen atlas bake | Every style shares a pre-baked (font, size). `test_styles_prebaked` pins it, and `kAllStyles` is not edited | In scope |
| Per-frame allocations | Labels via `string_view` (`remap_action_label`, `remap_value_view`). The calibration values are SSO-sized locals. The legends are `constexpr`. The `HintMeasure` lambdas capture one pointer (inside `std::function`'s small buffer, as select does today) | In scope |
| Visual tuning (band tops 200/300/450, the chip, the red reset/notice colour) looks off in the real window | Owner windowed check. Every value is one named constant | In scope (flag) |
| Baked chrome `GET READY` / `TAP ON THE BEAT` / `DONE` sprites | Optional per the issue; runtime text is used | Out of scope |
| #74 calibration offset bug | Independent (issue note) | Out of scope |
| Scanlines toggle, perf pass, cross-platform screenshots | #98 | Out of scope |

---

## Open Questions

None of these block the work. The plan already uses each recommended default.

1. **The selected row while capturing a key.** Today the highlight disappears while the screen waits for a key, and only `<PRESS>` marks the row.
   **Recommendation:** keep the gold bar on the row being rebound, so it is obvious which binding will change. `<PRESS>` shows in dark ink, and the bottom bar switches to "PRESS A KEY OR PAD BUTTON  ESC CANCEL".
2. **Fewer rows on screen.** With the slanted rows at the same size as the options overlay, 9 rows fit at once instead of about 15. The list scrolls as you move, the same way the song wheel does.
   **Recommendation:** accept 9. If it feels cramped, shrinking the rows to 40 px would fit 10, but they would no longer match the options overlay.
3. **KEYBOARD / PAD as headers instead of a column.** Today every row says [KEYBOARD] or [PAD].
   **Recommendation:** group the rows under two gold header rows, like the packs on the song wheel. Each row then shows just the action and its key, which is what the issue asks for.
4. **Offset before it is ready.** Today the offset line is hidden until enough taps are in, and always hidden without audio.
   **Recommendation:** always show the OFFSET plate and write "---" until there is a usable result, so nothing jumps when it appears. Without audio it stays "---", and the red notice explains why.
5. **Colour of the phase word.** **Recommendation:** gold for all three phases, as today. Per-phase colours (for example green for DONE) would be a one-line change if wanted.

---

## Acceptance Criteria

- [ ] Both screens draw `bg_select`, `bar_top` with a runtime title ("REMAP INPUT" / "CALIBRATE OFFSET") in a Saira style (`options_art::kTitleStyle`), and `bar_hint` with a legend of gold keys and arrows
- [ ] Remap shows the binding table as slanted rows (action, bound key), with the selected row on the gold bar and RESET TO DEFAULTS as a row; every row is reachable by scrolling
- [ ] Calibration shows its phase word (GET READY / TAP ON THE BEAT / DONE), sample count and offset in the new type, centred, fitting at 720p and mapping to 1440p (pinned by `setup_art_test`)
- [ ] Input handling, calibration math and Esc-back-to-options are unchanged; `input_remap_screen_test` and `calibration_screen_test` (unmodified) pass
- [ ] 49/49 tests pass under the sandboxed ctest command; no new compiler warnings
- [ ] No bitmap text, clock reads or new heap allocations per frame in either screen's render
- [ ] `.agents/stories/todo-stories.md` untouched
