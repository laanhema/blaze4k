# Implementing the Cabinet look in Blaze 4k

Target: the four screens in `reference/`, at the game's current performance (a few hundred batched
quads per frame, no per-frame allocations, nothing touching the music clock).

## 1. Dependencies: none new

| Need | Choice | Why |
| --- | --- | --- |
| TrueType text | **stb_truetype** + **stb_rect_pack** | Already downloaded by CMake (the `stb` FetchContent pulls the whole repo); single-header, no build changes. The UI is short uppercase Latin text, so FreeType's hinting and HarfBuzz shaping buy nothing here. |
| Gradients, glows, strokes | **Baked textures** (this pack) | Pixel-identical to the mock-ups, zero shader work, cheapest at runtime. |
| Slanted shapes, gradients, italic | **Two small renderer additions** | The vertex format already carries a colour per vertex; it only needs an entry point that takes four corners. |

Considered and not needed now:

- **FreeType + HarfBuzz / SDL3_ttf:** two more native deps per OS for no visible gain. SDL3_ttf also renders to SDL surfaces rather than into your own atlas.
- **msdfgen / MSDF text:** worth it later only if you want runtime text with outlines and glows at any size, for example animating dynamic song titles in chrome. Everything chrome in this design is static, so it is baked.

## 2. Engine additions

### 2.1 Renderer (`src/render/gl_quad_renderer.*`, `texture.*`): small

- `draw_quad_points(const std::array<Vec2,4>& corners, const Texture&, UVRect, const std::array<Color,4>& colours)`:
  a general quad (corner order TL, TR, BR, BL). It gives you:
  - parallelograms: meter ticks, judgment bars, chips (`theme::skew`);
  - vertical and horizontal gradients (life-fill fallback, score-screen bar tint);
  - sheared glyph quads for italic text.
  It reuses `append_quad`'s batching; only the four positions and colours differ.
- `Texture::from_file(path, mipmaps, Wrap::Repeat)`: an optional wrap mode for `scanlines.png` and
  `life_stripes.png` (today every texture is `GL_CLAMP_TO_EDGE`). For `scanlines.png` also use
  `GL_NEAREST`.
- Keep `BlendMode::Add` for explosions as now. No new shaders.
- Unit tests in the style of `texture_test` / `note_field_renderer_test`: corner order, colour per
  vertex, UV mapping for the repeat case.

### 2.2 TrueType text (`src/render/ttf_font.*`): medium

- `FontAtlas`: one per (font file, pixel size). It bakes printable ASCII + Latin-1 + Latin
  Extended-A with `stbtt_PackBegin` / `stbtt_PackSetOversampling(2,2)` /
  `stbtt_PackFontRanges` into a single-channel atlas, uploaded as white RGBA with the coverage in
  alpha, so the existing premultiplied pipeline and tinting just work.
- `TextRenderer`:
  - `measure(text, TextStyle)` and `draw(text, x, y, TextStyle, align, extra_shear)`;
  - applies tracking, `kItalicShear` (plus the group shear for the combo), and the hard drop
    shadow (`Shadow::Hard2/3` = draw once in black offset down, then the colour);
  - decodes UTF-8 with the existing `unicode_text` helpers, keeping today's rules (zero-width
    skipped, malformed bytes → U+FFFD), and draws the existing placeholder box for glyphs a font
    lacks;
  - truncates with `...` using measured widths instead of cell counts.
- Atlases are baked at `size_px * (window_height / 720)` and **re-baked only when the window
  height changes**, so text stays pixel-crisp at every size. Expect around 15 (font, size) pairs.
  At 1440p each fits in a 512² or 1024² 8-bit atlas, a few MB in total.
- Keep `bitmap_font` for debug overlays, or delete it once no screen uses it. Port the
  `truncate_to_cells` tests to the new measure-based truncation.

### 2.3 Theme textures (`src/render/theme_textures.*`): small

- Load `assets/theme/cabinet/manifest.json` with nlohmann/json (already a dependency) and every
  PNG with `Texture::from_file` at startup, once. Mipmaps on for sprites that are often drawn
  smaller than native (grades, judgments, logo).
- Helpers that read the manifest so screens never hard-code pixel offsets:
  - `draw_sprite(name, content_x, content_y, scale, tint)`: positions the content box;
  - `draw_slice3(name, x, y, w, h, tint)`: left cap, stretched middle, right cap;
  - `draw_slice9(name, rect)`;
  - `draw_frame(name, hole_rect)`;
  - `draw_tiled(name, rect)`;
  - `draw_fill_cropped(name, rect, fraction)` for the life bar: map the full gradient to the bar
    and crop UVs, so the colours don't slide as life changes.
- `BitmapDigits`: draws strings from `bitmap_fonts.digits_chrome` / `digits_white` (glyph rect,
  `origin_x`, `advance`), with centre and right alignment.
- Missing files log once and fall back to flat quads (same spirit as the noteskin's optional
  explosions), so headless tests keep running.

### 2.4 Layout scale

Add one helper used by every screen (`render/theme_layout.hpp`, #91): `s = min(window_height / 720,
window_width / 1280)`, so the whole layout always fits, and a 1280·s x 720·s content column centred
on both axes. Wider windows (21:9) show more background at the sides, as the game already does for
the note field; narrower ones (16:10, 4:3) show bands above and below, and nothing is cropped.
Every `theme::layout` value is multiplied by `s`, and text atlases bake at the same `s`.

## 3. Screen by screen

Each step is one PR; check against `reference/` side by side at 1280x720 and 2560x1440.

1. **Title (`title_screen.cpp`)**: small.
   - Draw `bg_title`, then `logo`, `subtitle` and four Cel tap notes, rotated per column (reuse
     `NoteSkin::head` with a fixed beat).
   - Then `press_start`, blinking at 1 Hz as now; the footer text ("SINGLE · 4 PANEL" left, version
     right); and the scanlines overlay.
   - Read the version from a `BLAZE4K_VERSION` compile definition set by CMake `project(VERSION …)`.
2. **Attract (`attract_screen.cpp`)**: small. Same background and logo, keeping its pulse and
   receptor-blink logic with the Cel receptors.
3. **Song select (`select_screen.cpp`)**: large.
   - Draw `bg_select`, `bar_top` + `title_select_music`, and chips (`chip` tinted, with text).
   - Banner inside `banner_frame` (`banner_fallback` when missing), then title, artist and BPM.
   - Difficulty rows: `diff_row_<name>[_selected]`, with name, meter, `diff_tick` ×10 and best %.
   - Wheel: `wheel_pack`, `wheel_row`, `wheel_row_selected`, using `kWheelIndent` by distance from
     the selection; no artist on the wheel.
   - Hint bar with gold keys.
   - Keep the 13-row window logic. Add an eased vertical scroll (~80 ms) when the selection moves,
     on fixed `dt`.
4. **Options overlay (same file)**: medium, not in the mock-ups.
   - Dim with 72% black (`theme::color::kGameplayScrim`), then a centred panel: a `stat_panel`-style plate or `bar_top` as the header.
   - Use `diff_row`-style slanted rows (or `wheel_row`) for the options, the selected row in gold
     (`wheel_row_selected`), and values right-aligned in `kGold`.
5. **Gameplay (`hud_renderer.cpp`, `judgment_animator.cpp`, `gameplay_view.cpp`)**: medium.
   - **Remove** the live percent, the judgment chips and the per-window chip colours from the HUD.
     The score keeper keeps counting; only drawing goes.
   - The life bar becomes `life_frame` (9-slice) plus `life_fill` / `life_fill_danger` (cropped)
     plus `life_stripes` (tiled). Keep `layout_life_bar`'s field-clearance logic and start from
     `theme::layout::kLifeBar`.
   - Draw `diff_badge` top-left, tinted by `theme::difficulty`, with e.g. "HARD 8" in the ink
     colour.
   - The judgment pop draws `judgment_<kind>` (keep the current scale and fade curve), with the
     combo in `kComboNumber` + `kComboLabel` under it (group shear −10°).
   - Background: keep the existing dim. No scanlines, no lane backing, no bottom bar. The note
     field and Cel noteskin are untouched.
   - Update `hud_renderer_test` and `gameplay_screen_test` for the removed elements.
6. **Score screen (`results_screen.cpp`, `results_anim.cpp`)**: large.
   - Draw `bg_results`, `bar_top` + `title_score_screen`, and the badge, title and artist on the
     right of the bar.
   - Stat panels (`stat_panel` + label + `digits_white`) on the left, and `medallion` + `grade_<tier>`
     + tier label in the centre.
   - The percentage in `digits_chrome`, counting up as now; `record_ribbon` or `failed_ribbon`.
   - Judgment bars on the right (`draw_quad_points` for track and fill), and the hint bar.
   - Map the existing reveal: title fades in, the grade slams 2.4x→1x on the medallion, stats fade,
     the ribbon pops. Keep the white flash on NEW RECORD if you like it.
   - Update `results_screen_test` / `results_anim_test` strings ("SCORE SCREEN").
7. **Input Remap and Calibration**: medium, not in the mock-ups. Use `bg_select`, the top bar with
   a runtime title, slanted rows for the binding table (gold selected row) and the hint bar.
   Calibration's phase word can reuse the `judgment_*` look: add a baked `GET READY` / `TAP ON THE
   BEAT` / `DONE` set if you want it fully chrome.
8. **Polish**: small.
   - A config toggle for scanlines.
   - A slow rotation of the results sunburst: split it into its own texture if you want motion.
   - Verify on Windows, macOS and Linux per `docs/CROSS_PLATFORM_VERIFICATION.md`.

## 4. Performance budget

- **Draw calls:** the batcher flushes on texture change. Order each screen's draws as
  background → bars/frames → rows → text (one atlas per font size) → overlays. That gives roughly
  10–25 flushes per frame on the busiest screen (select), trivial for GL 3.3.
- **Gameplay** gets *cheaper* than today: the percent and eight chips go away, and the life bar is
  three textured quads.
- **Memory:** the baked PNGs are ~7 MB on disk. On the GPU the three full-screen backgrounds at 2x
  are ~15 MB each. If VRAM matters, ship `bg_select` and `bg_results` at 1x: they are soft
  gradients and lose nothing. Keep `bg_title` at 2x for the grid lines.
- **Startup:** loading ~65 PNGs with stb_image plus baking ~15 font atlases takes well under a
  second. Do it once in the app init, next to the noteskin load.
- **Timing rule unchanged:** UI animations run on the fixed `dt` the screens already use, and
  nothing in the theme reads or affects the music clock.

## 5. Suggested order and size

| Step | Size | Depends on | Issue |
| --- | --- | --- | --- |
| 2.1 renderer primitives | S | — | #88 |
| 2.3 theme textures + digits | S | 2.1 | #89 |
| 2.2 TrueType text | M | 2.1 | #90 |
| 2.4 layout scale helper | S | — | #91 |
| 3.1–3.2 title + attract | S | 2.2, 2.3 | #92 |
| 3.5 gameplay HUD | M | 2.2, 2.3 | #93 |
| 3.3 song select | L | 2.2, 2.3 | #94 |
| 3.6 score screen | L | 2.2, 2.3 | #95 |
| 3.4 / 3.7 options, remap, calibration | M | 3.3 | #96, #97 |
| 3.8 polish + cross-platform check | S | all | #98 |

Setup: importing this pack into the repo is #87 (blocks #89, #90, #91).

Doing gameplay right after the title gives the biggest visible change early with the least layout
work. To hand this to Claude Code inside the repo, give it this file, the pack and one step at a
time, one PR per step.
