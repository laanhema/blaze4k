# Blaze 4k — Cabinet theme pack

Everything needed to make the game look like the **Cabinet v3** mock-ups. The pack has been
imported into the repository (#87):

```
assets/fonts/                     Audiowide + Saira Condensed (Medium, Bold, ExtraBold) .ttf, with OFL licences
assets/theme/cabinet/             63 baked textures (@2x) + manifest.json + 2 bitmap digit fonts
src/render/theme.hpp              colours, text styles, layout metrics and skews as constexpr
docs/cabinet-theme/reference/     the four target screens (1280x720) + textures-overview.jpg
docs/cabinet-theme/IMPLEMENTATION_PLAN.md   how to wire it into the engine, step by step
```

`CMakeLists.txt` already copies the whole `assets/` folder next to the binary, so nothing changes
in the build to ship these files. `theme.hpp` only depends on `render/geometry.hpp` and compiles
as-is with `-std=c++20`.

## What is baked and what is drawn at runtime

Anything with gradients, glows, strokes or a fixed word is a **texture**: the logo, PRESS START,
the top-bar titles, every judgment word, every grade, the medallion, ribbons, the chrome bars and
frames, wheel and difficulty rows, the life bar frame and fill, and the three backgrounds.

Anything that changes per song or per run is **runtime text or quads**: song titles, artists, BPM,
meters, best scores, combo, counts, key hints, the meter ticks and the score-screen bars.
Big numbers on the score screen (the chrome percentage, MAX COMBO, DP, holds) use the two
**bitmap digit fonts** in `assets/theme/cabinet/` (`0-9 . % /` and space), so they keep the chrome
look without a gradient text shader.

The note field is untouched: Cel noteskin, receptors, holds, rolls, mines and explosions stay as
they are in the game today.

## Using the textures

- All textures are rendered at **2x** for a 1280x720 design, so they stay sharp up to 2560x1440.
  Draw at `size_px / 2 * s`, where `s` is the layout scale from `src/render/theme_layout.hpp`
  (`theme::layout_scale_factor(w, h)` = `min(h / 720, w / 1280)`).
- Each entry in `manifest.json` has `content_px`, the design box inside the image. The rest is
  padding that holds glows and drop shadows. Position the **content box** at `layout_720p`; the
  padding hangs outside it.
- `kind` says how to stretch it:
  - `fullscreen`: backgrounds, stretch to the window.
  - `sprite`: draw as-is, scaled.
  - `stretch_x` / `stretch`: plain gradients, stretch freely.
  - `slice3`: horizontal 3-slice (fixed left and right caps, stretch the middle). Used for slanted
    rows and plates, whose caps hold the slanted ends.
  - `slice9`: 9-slice (life bar frame).
  - `frame`: a ring with a transparent hole (`hole_px`) where the banner image goes.
  - `tile`: repeat with `GL_REPEAT`.
- `tint: "multiply"` means the texture is white on purpose: draw it with a vertex colour (meter
  ticks, the gameplay difficulty badge, option chips).
- Bitmap digits: `bitmap_fonts.<name>.glyphs[ch]` gives the atlas rect, the glyph's `origin_x`
  (padding before the ink) and `advance`.

## Fonts

| Font | Use | Licence |
| --- | --- | --- |
| Audiowide Regular | Logo, judgments, grades, big numbers (baked) | SIL OFL 1.1 |
| Saira Condensed Medium / Bold / ExtraBold | All runtime UI text | SIL OFL 1.1 |

Neither family has an italic. The mock-ups use synthetic oblique, so italic text is a 0.25
horizontal shear of the glyph quads (`theme::kItalicShear`). The fonts have no ★ or ▾: stars and
the pack caret are drawn shapes, baked into the grade textures and `wheel_pack.png`.
Both fonts cover Latin and Latin Extended. For titles with other scripts, keep the existing
fallback-box behaviour or add a fallback font later.

## Not in the mock-ups (extrapolated, change freely)

`life_fill_danger.png` (red fill below 30% life), `failed_ribbon.png`, the letter grades S+…D (the
mock only shows one star), and the colours for judgment words other than FANTASTIC. The Options
overlay, Input Remap and Calibration screens were not designed; the plan suggests how to restyle
them with the same parts.
