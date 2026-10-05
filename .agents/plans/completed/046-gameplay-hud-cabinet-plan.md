# Plan: Cabinet Gameplay HUD and Judgment Pop (#93)

## Summary

Restyle the gameplay HUD to match `docs/cabinet-theme/reference/cabinet-v3-gameplay.png`, drawing only presentation.
The live percent, the top-centre live combo and the eight per-window judgment chips are no longer drawn, and their palette (`judgment_color`) goes too.
The vertical life bar becomes `life_frame` (9-slice), with `life_fill` or `life_fill_danger` cropped inside its 32 px track and `life_stripes` tiled over the fill. It starts from `theme::layout::kLifeBar` and keeps the existing field-clearance clamp.
A `diff_badge` plate tinted with `theme::difficulty` sits top-left, showing for example "HARD 8" in the difficulty's ink colour.
The judgment pop draws the baked `judgment_<kind>` sprite with the existing scale and fade curves. A persistent combo line ("212" in `kComboNumber` plus "COMBO" in `kComboLabel`, sheared by `kComboGroupShear`) sits under it at `kComboTop`, shown from a combo of 4 (OpenITG `ShowComboAt=4`).
`ScoreKeeper`, `LifeKeeper`, the judgment engine, the note field, receptors, the Cel noteskin and the background dim are not touched. All animation stays on the fixed `dt`, and nothing reads the music clock.

## User Story

As a player
I want the gameplay screen to look like the Cabinet mock-up, with a chrome life bar, a difficulty badge, baked judgment sprites and a combo line under the judgment
So that gameplay matches the restyled title and select screens, with less clutter (no live percent or judgment chips) and no change to timing or scoring

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | MEDIUM (issue says M: two gameplay TUs rewritten in place, small API changes in `GameplayView` / `GameplayScreen` / `main.cpp`, three tests updated) |
| Systems Affected | `src/gameplay/hud_renderer.*`, `src/gameplay/judgment_animator.*`, `src/gameplay/gameplay_view.*`, `src/screens/gameplay_screen.*`, `src/main.cpp` (`--gameplay-demo` wiring), `tests/hud_renderer_test.cpp`, `tests/judgment_animator_test.cpp`, `tests/gameplay_screen_test.cpp`, `tests/CMakeLists.txt` (asset roots for two tests), `README.md:50` |
| GitHub Issue | #93 (TODO-21; blocked by #89, #90, #91, all merged; blocks #98) |
| Plan sequence | 046 (running plan sequence; the issue number is #93) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC, FetchContent deps in `build/_deps`). Build with `cmake --build build -j8` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **48/48 pass** | Run on `main` @ `97d202f` with the sandboxed command in Validation. Start green, stay green. The count stays **48**: no test executable is added or removed |
| Sandbox requirement | — | `audio_test` opens the real sound device, so **always** run ctest (and any binary) inside the `bwrap` command in Validation |
| Headless gameplay smoke | `./build/blaze-4k --headless --smoke-test 5 --data-dir <scratch> --gameplay-demo "songs/In The Groove/Anubis/Anubis.sm"` (inside bwrap) | On `main` it prints `[GameplayView] Loaded chart 'Hard' (meter 8) ...` and `Blaze 4k shut down cleanly.` (verified). Anubis Hard 8 is the mock's "HARD 8" chart, which makes it a good windowed check |
| Reference image | `docs/cabinet-theme/reference/cabinet-v3-gameplay.png` (1280x720) | The issue says `reference/…`. The file is under `docs/cabinet-theme/` |
| Gameplay art (manifest) | `assets/theme/cabinet/manifest.json` | `diff_badge`: slice3, content 260x80 @2x = **130x40 ref**, caps 100/100 @2x, `tint: multiply`, layout `[36,28,130,40]`. `life_frame`: slice9, 80x960 @2x = **40x480 ref**, borders 16/8/16/8 @2x = **4 px ref**, layout `[40,120,40,480]`, "fill goes inside the 32px-wide track". `life_fill` / `life_fill_danger`: stretch 16x512, "map the full texture to the full bar height and crop UVs". `life_stripes`: tile 16x24 @2x (12 px period), "aligned to the track bottom". `judgment_{fantastic,excellent,great,decent,wayoff,miss,mine,ok,ng}`: sprite, content 888x132 @2x = **444x66 ref**, layout `[418,296,444,66]`, anchor centre, mipmapped (`theme_texture_options`) |
| Judgment sprites | `judgment_fantastic.png` (viewed) | The italic slant is **baked into the sprite**. Only the runtime combo text gets `kComboGroupShear`. Do not shear the sprite again |
| Theme constants | `src/render/theme.hpp:65, 77-84, 136-139, 207-210` | `color::kLifeDangerThreshold 0.3`. `difficulty::k{Beginner,Easy,Medium,Hard,Challenge,Edit}` (fill + ink). `text::kBadge` (SairaExtraBold 24, tracking 3), `kComboNumber` (34, italic, Hard3 shadow, white), `kComboLabel` (22, tracking 3, italic, gold), `kComboGroupShear 0.176`. `layout::kDiffBadge{36,28,130,40}`, `kLifeBar{40,120,40,480}`, `kJudgmentTop 296`, `kComboTop 368`. All four styles are already in `kAllStyles`, so they are pre-baked (no mid-game atlas bake) |
| Layout scale | `src/render/theme_layout.hpp` | `L = theme::layout_scale(w, h)`. 1280x720 gives the identity. 2560x1440 gives s=2. 3440x1440 gives s=2 with origin (440,0). 640x480 gives s=0.5 with origin (0,60). `L.x(640) == w/2` at every size, which is also the note-field centre (`NoteField::field_left(w) = w/2 − 216`) |
| Note field | `src/gameplay/noteskin.hpp:109-110` | Fixed 108 px columns (not scaled by `s`). Unchanged by this issue |
| Difficulty label + colours | `src/screens/select_art.hpp:262-272`, `select_art.cpp:192-208` | `difficulty_row_style(chart).colors` (Edit/Invalid → `kEdit`) and `difficulty_row_label(chart)` (Edit with description → chart name as written; empty label → resolved name; else ASCII upper) already implement the issue's "chart name (#84) or EDIT" rule. Reuse them, don't duplicate |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Mock measurements vs `theme.hpp` (PIL pixel scans of the reference PNG)

| Item | `theme.hpp` / manifest | Mock | Plan uses |
|---|---|---|---|
| Life frame | `kLifeBar {40,120,40,480}`, 4 px border | chrome x 40..79, y 120..599. Track x 44..75, y 124..595 | **matches**: frame = `kLifeBar`, track = frame inset 4 → `{44,124,32,472}` |
| Life stripes | 12 px period, track-bottom aligned | dark 3 px bands every 12 px, the last at y 593..595 (flush with the track bottom) | **matches**: `TileAnchor::Bottom` on the fill rect (its bottom is the track bottom) |
| Badge plate | `kDiffBadge {36,28,130,40}` | red plate rows y 28..~70, slanted | **matches** |
| Badge text | "e.g. HARD 8", no x rule | "HARD 8" ink x 55..134, caps y 42..59 (centre ≈ 50.5; plate centre 48) | **left-aligned at plate x + 18** (`kBadgeTextPadX`), line box centred vertically in the plate. Width grows past 130 only for long Edit names (Open Question 3) |
| Judgment sprite | content top 296, centred on the field | sprite ink lands where content `{418,296,444,66}` predicts (bright band y 323..340) | **matches**: content centre `(L.x(640), L.y(296) + 33·s)`, scaled about that centre by `pop_scale` |
| Combo line | `kComboTop 368` | "212" caps y 380..402, "COMBO" caps 388..402 (shared baseline ≈ 403). Number ink x 573..619, label ink x 622..692 | number line-box top = `kComboTop`, label on the **same baseline**, pen gap **6 ref** (`kComboGap`), group pen box **centred on x 640**. The mock's ink centre is ≈ 633, a few px left of 640 after the shear. Accept ±8 px; no constant change |

### Value Provenance (OpenITG)

| Value | Source | Used for |
|---|---|---|
| `ShowComboAt=4` | openitg/openitg @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`, `assets/patch-data/Themes/default/metrics.ini:2985` (`[Combo]`). `src/Combo.cpp` `SetCombo`: hides number and label while `iNum < SHOW_COMBO_AT` | Combo line visible iff live combo ≥ 4 (`JudgmentAnimator::kShowComboAt`) |
| `DangerThreshold=0.3` | same commit, `metrics.ini:2565` (`[LifeMeterBar]`) | `life_fill_danger` below 0.3, via the existing `theme::color::kLifeDangerThreshold` |

### Forward references to #93 found in prior plans

| Where | Note | This plan |
|---|---|---|
| `.agents/plans/completed/043-layout-scale-helper-plan.md:266` | "Scaling the field is part of the gameplay screen work (#93)" | **Not done.** The issue says "Leave the note field untouched" and the AC says the note field is unchanged. Flagged as Open Question 2 (follow-up issue) |
| `.agents/plans/completed/044-title-attract-cabinet-plan.md:421` | "#93 can let `GameplayView` borrow App's skin" (Cel textures loaded twice) | **Not done.** The AC says the Cel noteskin is unchanged. Still flagged, out of scope |
| Issue technical note | "`kLifeBarMinInset` exists to keep the frame below the percent text. Revisit it once the percent is gone" | **Removed**, together with `kLifeBarInsetFraction`, `kLifeBarLeft`, `kLifeBarThickness` and `kLifeBarBorder`. The vertical placement now comes from `kLifeBar` × `L` |

---

## Patterns to Follow

### Screen render: layout scale, null-checked services (copy #92 / #94)

```cpp
// SOURCE: src/screens/title_screen.cpp:33-45
void TitleScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    namespace layout = theme::layout;
    const theme::LayoutScale L = theme::layout_scale(w, h);
    if (ctx.theme != nullptr) {
        title_art::draw_backdrop(*ctx.theme, renderer, w, h);
        title_art::draw_centred_sprite(*ctx.theme, renderer, "logo", L, layout::kLogoTop);
```

### Pure layout + thin draw, tested headless (existing life bar shape, keep it)

```cpp
// SOURCE: src/gameplay/hud_renderer.hpp:34-46
struct LifeBarLayout {
    bool visible = false;
    bool danger = false;  // clamped life < 0.3
    Rect frame{};         // backing frame quad (inner rect grown by the border)
    Rect back{};          // empty-bar background
    Rect fill{};          // filled portion, bottom-anchored inside `back`
};
[[nodiscard]] LifeBarLayout layout_life_bar(double life, int screen_w, int screen_h,
                                            double field_left);
```

### ThemeTextures calls this issue needs

```cpp
// SOURCE: src/render/theme_textures.hpp:282-312
void draw_sprite(GlQuadRenderer&, std::string_view name, Vec2 content_pos, float s, Color tint = Color{}) const;
void draw_slice3(GlQuadRenderer&, std::string_view name, const Rect& content_rect, Color tint = Color{}) const;
void draw_slice9(GlQuadRenderer&, std::string_view name, const Rect& content_rect, float s, Color tint = Color{}) const;
void draw_tiled(GlQuadRenderer&, std::string_view name, const Rect& rect, float s, Color tint = Color{}, TileAnchor anchor = TileAnchor::TopLeft) const;
void draw_fill_cropped(GlQuadRenderer&, std::string_view name, const Rect& bar_rect, float fraction, Color tint = Color{}) const;
[[nodiscard]] const ThemeEntry* entry(std::string_view name) const;
[[nodiscard]] Vec2 content_size(std::string_view name, float s) const;
```

### Text: measure / ascent / truncate / draw with a group shear

```cpp
// SOURCE: src/render/ttf_font.hpp:236-252
[[nodiscard]] float measure(std::string_view text, const theme::TextStyle& style) const;
[[nodiscard]] float ascent(const theme::TextStyle& style) const;
[[nodiscard]] float line_height(const theme::TextStyle& style) const;
[[nodiscard]] std::string truncate(std::string_view text, const theme::TextStyle& style, float max_width) const;
// `extra_shear` adds a group skew (e.g. theme::text::kComboGroupShear).
void draw(GlQuadRenderer& renderer, std::string_view text, float x, float y,
          const theme::TextStyle& style, TextAlign align = TextAlign::Left, float extra_shear = 0.0f);
```

### Tinted text style (copy the private helper pattern)

```cpp
// SOURCE: src/screens/select_art.cpp:54-62
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style) {
    return L.y(ref_top) + (L.px(ref_h) - text.line_height(style)) * 0.5f;
}
theme::TextStyle with_color(theme::TextStyle style, Color color) {
    style.color = color;
    return style;
}
```

### Event-sourced, fixed-dt presentation (keep this contract)

```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:215-222
    score_.consume(new_events_);
    life_.consume(new_events_);
    // Display-only first, so a recorded judgment in the same tick wins the popup.
    judge_anim_.consume(display_events_);
    judge_anim_.consume(new_events_);
    judge_anim_.update(fixed_dt, score_.state().combo);
```

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real assets headless)

```cpp
// SOURCE: tests/theme_textures_test.cpp:38-39, 63-66
const fs::path kAssets{BLAZE4K_ASSETS_DIR};
const fs::path kCabinet = kAssets / "theme" / "cabinet";
const ThemeManifest& real_manifest() {
    static const ThemeManifest manifest =
        blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"));
```

```cmake
# SOURCE: tests/CMakeLists.txt:194-199 (select_screen_test asset roots)
target_compile_definitions(select_screen_test PRIVATE
    BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)
```

---

## Pinned Semantics (the contract the tests pin)

All rects are in screen px. `L = theme::layout_scale(w, h)`, `s = L.s`, `field_left = NoteField::field_left(w)` (= w/2 − 216).

### Life bar (`layout_life_bar(life, w, h, field_left)`, signature unchanged)

- `fraction = clamp(life, 0, 1)` (NaN → 0). `danger = fraction < theme::color::kLifeDangerThreshold` (strict).
- `border = L.px(4)` (`kLifeFrameBorderRef = 4`, the manifest's 8 px @2x slice9 border).
- Start with `frame = L.rect(theme::layout::kLifeBar)`.
- Field clearance (same rule as today, now on the frame): `max_right = field_left − kLifeBarFieldGap` (16 screen px, unchanged). If `frame.x + frame.w > max_right`, set `frame.x = max(0, max_right − frame.w)`. If it still overflows, set `frame.w = max(min_w, max_right − frame.x)` with `min_w = 2·border + kLifeBarMinTrack` (`kLifeBarMinTrack = 6` screen px, the old `kLifeBarMinThickness`).
- `track = {frame.x + border, frame.y + border, frame.w − 2·border, frame.h − 2·border}`.
- `fill = fill_cropped_rect(track, fraction)` (bottom-anchored), or `{track.x, track.y + track.h, track.w, 0}` when the fraction is 0.
- `visible = false` for a non-positive w or h.

| Window | s, origin | frame | track |
|---|---|---|---|
| 1280x720 | 1, (0,0) | `{40,120,40,480}` | `{44,124,32,472}` |
| 2560x1440 | 2, (0,0) | `{80,240,80,960}` | `{88,248,64,944}` |
| 1920x1080 | 1.5, (0,0) | `{60,180,60,720}` | `{66,186,48,708}` |
| 3440x1440 | 2, (440,0) | `{520,240,80,960}` | `{528,248,64,944}` |
| 640x480 | 0.5, (0,60) | `{20,120,20,240}` | `{22,122,16,236}` |
| 1024x768 | 0.8, (0,96) | `{32,192,32,384}` | `{35.2,195.2,25.6,377.6}` |
| 500x400 (field_left 34) | 0.390625, (0,59.375) | slid: x = 18 − 15.625 = **2.375**, w 15.625 (no shrink) | inset 1.5625 |
| 400x400 (field_left −16) | 0.3125, (0,87.5) | x **0**, w = min_w = **8.5** | w **6** |

Draw (`HudRenderer::render_chrome`, theme non-null): `draw_slice9("life_frame", frame, s)`, then, if `fill.h > 0`, `draw_fill_cropped(danger ? "life_fill_danger" : "life_fill", track, fraction)` and `draw_tiled("life_stripes", fill, s, Color{}, TileAnchor::Bottom)`.

### Difficulty badge (`layout_diff_badge(text_w, w, h, field_left)`)

- `DifficultyBadge { std::string text; theme::DifficultyColors colors; }` is built once per song by `difficulty_badge_for(chart)`: `select_art::difficulty_row_label(chart) + " " + std::to_string(chart.meter)` and `select_art::difficulty_row_style(chart).colors`. Examples: Hard 8 → "HARD 8" with `kHard`. Edit with description "Crazy Edit", meter 11 → "Crazy Edit 11" with `kEdit`. Edit without description → "EDIT 5" with `kEdit`. Empty label with meter 1 → "BEGINNER 1" (the resolved name).
- `plate = L.rect(kDiffBadge)`, with `plate.w = clamp(text_w + 2·L.px(kBadgeTextPadX), L.px(130), L.px(kBadgeMaxWidthRef))` (`kBadgeTextPadX = 18`, `kBadgeMaxWidthRef = 300`).
- Field clearance: if `plate.x + plate.w > field_left − kLifeBarFieldGap`, shrink `plate.w` to fit. If that leaves less than `L.px(130)`, set `visible = false` (the badge would overlap the receptors in a very narrow window).
- `text_x = plate.x + L.px(18)`, `text_max_w = plate.w − 2·L.px(18)`, and the text top centres the line box in the plate (`plate.y + (plate.h − line_height(kBadge)) / 2`).
- At 1280x720, "HARD 8" (≈ 82 px) gives `plate == {36,28,130,40}`. At 2560x1440 it is `{72,56,260,80}`. At 500x400 the badge is hidden.
- Draw: plate `draw_slice3("diff_badge", plate, colors.fill)` in `render_chrome` (before the frame). Text `with_color(kBadge, colors.ink)`, truncated to `text_max_w`, in `render_text`.

### Judgment pop (`JudgmentAnimator`)

- Sprite per event (`judgment_sprite(e)`, static, `std::string_view`): Tap Fantastic/Excellent/Great/Decent/WayOff → `judgment_fantastic|excellent|great|decent|wayoff`. Tap Miss and `JudgmentKind::Miss` → `judgment_miss`. Tap HitMine and `JudgmentKind::HitMine` → `judgment_mine`. HoldOk/RollOk → `judgment_ok`. HoldNg/RollNg → `judgment_ng`. Tap `Num`, AvoidedMine and RollHit → empty (no pop). It is non-empty exactly when `judgment_label(e)` is non-empty. `judgment_label` and `popup_label()` stay (score_keeper_test and `GameplayView::judgment_popup_label` use them).
- `judgment_pop_rect(L, content, scale)` (static, pure): `content` is the sprite's content size at `s` (`theme->content_size(sprite, s)`; reference `kJudgmentContentRef {444,66}`). The centre is `(L.x(640), L.y(kJudgmentTop) + content.y / 2)`, the size is `content × scale`, and the rect is centred on that point. At 1280x720 with scale 1 that is `{418,296,444,66}`.
- Draw: `draw_sprite(sprite, {rect.x, rect.y}, s · scale, Color{1,1,1,alpha})`, with `scale = pop_scale(...)` and `alpha = pop_alpha(...)` (curves unchanged).
- Fallback (theme null, or `theme->entry(sprite) == nullptr`, meaning a missing or broken manifest): the old bitmap `draw_text_centered(pop_label_, ...)` in white with that alpha, centred on `L.x(640)` at `L.y(kJudgmentTop)`, so feedback never disappears. A missing PNG with a valid manifest already falls back to a flat quad inside `ThemeTextures`.

### Combo line (`JudgmentAnimator`)

- `update(fixed_dt, combo)` also stores `live_combo_ = max(combo, 0)`. `reset()` zeroes it.
- `combo_visible() = live_combo_ >= kShowComboAt` (4, OpenITG). This is **persistent**: it does not fade with the judgment pop.
- `combo_line_layout(L, number_w, label_w, number_ascent, label_ascent)` (static, pure): `gap = L.px(kComboGap)` (6). The pen box total is `number_w + gap + label_w`, and `number_x = L.x(640) − total / 2`. `number_top = L.y(kComboTop)`, `baseline = number_top + number_ascent`, `label_x = number_x + number_w + gap`, `label_top = baseline − label_ascent`.
- Draw (text non-null): `format_combo(live_combo_)` in `kComboNumber`, then "COMBO" in `kComboLabel`, both `TextAlign::Left` with `extra_shear = theme::text::kComboGroupShear`.
- Milestone / final-combo pop (existing `update` / `celebrate` logic, unchanged): while `has_combo_pop_`, the number colour is `combo_number_color(combo_elapsed_, kComboPopSeconds)` (static, pure). It is `theme::color::kGold` at 0 and lerps linearly to `kWhite` at the duration. Outside a pop it is `kWhite`. Only the colour changes, never the size, because a size change would bake a new atlas mid-song (Open Question 1).

### Draw order in `GameplayView::render` (issue technical note: badge/frames → fill → stripes → judgment sprite → text)

After `field_renderer_.render(...)`:
1. `hud_.render_chrome(badge_, life, w, h, field_left, theme, renderer)`: badge plate, life frame, fill, stripes.
2. `judge_anim_.render_judgment(renderer, w, h, theme)`: the sprite (or its bitmap fallback).
3. `hud_.render_text(badge_, w, h, field_left, text, renderer)`: the badge text.
4. `judge_anim_.render_combo(renderer, w, h, text)`: number, then label.

A null `theme` skips 1 (and 2 uses the fallback). A null `text` skips 3 and 4. `render` already returns early when the renderer is uninitialised.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/hud_renderer.hpp` | UPDATE | Remove `judgment_color`, `percent_text_rect`, `HudRenderer::render(ScoreState…)`. Add `DifficultyBadge`, `DiffBadgeLayout` + `layout_diff_badge`. Reshape `LifeBarLayout` (`frame`, `track`, `fill`, `fraction`, `border`). Add `render_chrome` / `render_text`. Keep `format_percent`, `format_combo`, `kLifeBarFieldGap` |
| `src/gameplay/hud_renderer.cpp` | UPDATE | Delete the palette, percent, top-centre combo and chips. New life-bar and badge layout and drawing over `ThemeTextures` / `TextRenderer`. Truncated-badge-text cache |
| `src/gameplay/judgment_animator.hpp` | UPDATE | Remove `judgment_color` and `pop_color_`. Add `judgment_sprite`, `judgment_pop_rect`, `combo_line_layout`, `combo_number_color`, `kShowComboAt`, `kComboGap`, `kJudgmentContentRef`, `live_combo_`, `combo_visible()`, `live_combo()`. Replace `render` with `render_judgment` + `render_combo` |
| `src/gameplay/judgment_animator.cpp` | UPDATE | Their implementation. Sprite pop, bitmap fallback, persistent combo line with gold milestone flash |
| `src/gameplay/gameplay_view.hpp` | UPDATE | `render(renderer, w, h, const ThemeTextures* theme = nullptr, TextRenderer* text = nullptr)`, `set_difficulty_badge`, `difficulty_badge()`, `DifficultyBadge badge_` |
| `src/gameplay/gameplay_view.cpp` | UPDATE | New draw order (4 calls) at the end of `render` |
| `src/screens/gameplay_screen.hpp` | UPDATE | `difficulty_badge_for(const Chart&)` free function. Test accessor `difficulty_badge()` |
| `src/screens/gameplay_screen.cpp` | UPDATE | `enter`: `view_.set_difficulty_badge(difficulty_badge_for(chart))`. `render`: pass `ctx.theme`, `ctx.text` |
| `src/main.cpp` | UPDATE | `--gameplay-demo`: set the badge and pass `&app.theme_textures()`, `&app.text_renderer()` to `gameplay.render` |
| `tests/hud_renderer_test.cpp` | UPDATE | Re-pin the life bar to `kLifeBar` × `L`, add badge layout, judgment-pop and combo-line clearance tests. Drop the percent-text test |
| `tests/judgment_animator_test.cpp` | UPDATE | Replace the colour test with sprite mapping (against the real manifest), combo visibility, gold flash and new headless render calls |
| `tests/gameplay_screen_test.cpp` | UPDATE | `difficulty_badge_for` cases, plus an enter/render smoke with real theme + fonts headless |
| `tests/CMakeLists.txt` | UPDATE | `BLAZE4K_SOURCE_DIR` / `BLAZE4K_ASSETS_DIR` definitions for `judgment_animator_test` and `gameplay_screen_test` |
| `README.md` | UPDATE | Line 50: "Live HUD: score %, combo, judgment counts, life bar" → "Gameplay HUD: difficulty badge, life bar, judgment pops and combo" |

No new files. No `CMakeLists.txt` source-list change. `theme.hpp` and the manifest are unchanged.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Build after each with `cmake --build build -j8`.

### Task 1: Judgment animator: sprites, combo line, pure helpers

- **File**: `src/gameplay/judgment_animator.hpp`, `src/gameplay/judgment_animator.cpp`
- **Action**: UPDATE
- **Implement**:
  - Remove `judgment_color` (static), `pop_color_` and the `#include "gameplay/hud_renderer.hpp"` that existed only for the palette. Keep `#include "render/bitmap_font.hpp"` for the fallback.
  - Add constants: `kShowComboAt = 4` (comment with OpenITG `metrics.ini:2985` @ `f2c129f`, `Combo.cpp` `SetCombo`), `kComboGap = 6.0f` (reference px, measured from the mock), `kJudgmentContentRef = Vec2{444, 66}` (manifest content 888x132 @2x). Keep `kJudgmentPopPixel` (fallback only; update its comment).
  - Add the pure statics `judgment_sprite`, `judgment_pop_rect`, `combo_line_layout` (returns `struct ComboLineLayout { Vec2 number; Vec2 label; }`, top-left pen positions) and `combo_number_color`, exactly as in Pinned Semantics. Include `render/theme_layout.hpp`.
  - Store `std::string_view pop_sprite_` (it points at string literals, so no allocation) next to `pop_label_` in `consume`.
  - `update`: add `live_combo_ = std::max(combo, 0);` and leave the milestone logic byte-identical. `reset`: clear `pop_sprite_` and `live_combo_`.
  - Replace `render(renderer, w, h)` with `void render_judgment(GlQuadRenderer&, int w, int h, const ThemeTextures* theme) const` and `void render_combo(GlQuadRenderer&, int w, int h, TextRenderer* text) const`. Both return early on an uninitialised renderer or a non-positive size. Follow Pinned Semantics for the sprite, the fallback and the combo line.
  - Header comment: the class now draws the baked `judgment_*` sprites and the persistent combo line. Presentation only. Fixed `dt`, no clock.
- **Mirror**: `src/screens/title_art.hpp:1-13` (pure helpers + thin draw), `src/screens/select_art.cpp:59-62` (`with_color`).
- **Validate**: `cmake --build build -j8` (the build fails in `gameplay_view.cpp` and the tests until Tasks 3 and 6-7. That is expected; fix the animator TU's own errors first).

### Task 2: HUD renderer: remove percent/chips, chrome life bar, badge

- **File**: `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  - Delete: the nine judgment colour constants, `kTextColor`, the four life colours, the local `kLifeDangerThreshold` (use `theme::color::kLifeDangerThreshold`), `kLifeBarLeft`, `kLifeBarThickness`, `kLifeBarBorder`, `kLifeBarInsetFraction`, `kLifeBarMinInset`, `kHudEdgeMargin`, `kHudTextPixel`, `kGlyphRows`, `percent_text_rect`, `judgment_color`, `HudRenderer::render(const ScoreState&…)` (percent, top-centre combo, chips) and `render_life`. Drop the `bitmap_font.hpp` include.
  - Keep `format_percent` (results and select use it; update its header comment: no longer drawn during gameplay), `format_combo` and `kLifeBarFieldGap`.
  - Add `DifficultyBadge`, `DiffBadgeLayout { bool visible; Rect plate; float text_x; float text_max_w; }`, `layout_diff_badge(float text_w, int screen_w, int screen_h, double field_left)`, constants `kBadgeTextPadX = 18`, `kBadgeMaxWidthRef = 300`, `kLifeFrameBorderRef = 4`, `kLifeBarMinTrack = 6`.
  - Reshape `LifeBarLayout` to `{visible, danger, fraction, border, frame, track, fill}` and rewrite `layout_life_bar` per Pinned Semantics (same signature). Use `fill_cropped_rect` from `render/theme_textures.hpp` for `fill`.
  - `void render_chrome(const DifficultyBadge&, double life, int w, int h, double field_left, const ThemeTextures* theme, const TextRenderer* text, GlQuadRenderer&) const`. The badge plate needs `text->measure` for its width; with a null `text`, use `text_w = 0`, which gives the 130 px plate. Then draw the life frame, fill and stripes. No-op for a null theme.
  - `void render_text(const DifficultyBadge&, int w, int h, double field_left, TextRenderer* text, GlQuadRenderer&)` (non-const). It caches the truncated label in `mutable`-free members `cached_text_`, `cached_source_`, `cached_max_w_`, `cached_scale_`, and re-truncates only when the badge text, `text->scale()` or `text_max_w` changes, so nothing allocates per frame.
  - Class comment: Cabinet HUD (badge + life bar) over `ThemeTextures` / `TextRenderer`. The live percent and per-window chips were removed in #93 while the score keeper keeps counting.
- **Mirror**: the current `layout_life_bar` clamp (`src/gameplay/hud_renderer.cpp:157-196`). Keep its slide-then-shrink rule. `select_art::draw_chips` (`src/screens/select_art.cpp:348-370`) shows a tinted slice3 with text.
- **Validate**: `cmake --build build -j8` (the `hud_renderer.cpp` TU compiles).

### Task 3: GameplayView wiring and draw order

- **File**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - Forward-declare `ThemeTextures` and `TextRenderer`. Change to `void render(GlQuadRenderer& renderer, int screen_w, int screen_h, const ThemeTextures* theme = nullptr, TextRenderer* text = nullptr);`.
  - Add `void set_difficulty_badge(DifficultyBadge badge) { badge_ = std::move(badge); }`, `[[nodiscard]] const DifficultyBadge& difficulty_badge() const`, and the member `DifficultyBadge badge_;`. `init` / `shutdown` leave it alone (the caller sets it per song).
  - Replace lines 323-328 (`hud_.render`, `hud_.render_life`, `judge_anim_.render`) with the four calls in the Pinned draw order. Compute `field_left = field_.field_left(screen_w)` once. Update the comment.
  - Nothing above line 323 changes (background, field, receptors, explosions).
- **Validate**: `cmake --build build -j8`

### Task 4: GameplayScreen and the demo harness

- **File**: `src/screens/gameplay_screen.hpp`, `src/screens/gameplay_screen.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `gameplay_screen.hpp`: `[[nodiscard]] DifficultyBadge difficulty_badge_for(const Chart& chart);` (doc: label rule from `select_art::difficulty_row_label`, colours from `difficulty_row_style`, "<LABEL> <meter>"). Test accessor `[[nodiscard]] const DifficultyBadge& difficulty_badge() const { return view_.difficulty_badge(); }`.
  - `gameplay_screen.cpp`: implement it with `#include "screens/select_art.hpp"`. In `enter`, call `view_.set_difficulty_badge(difficulty_badge_for(chart));` before `view_.init(...)`. In `render`, un-comment `ctx` and call `view_.render(renderer, w, h, ctx.theme, ctx.text);`.
  - `main.cpp` (`--gameplay-demo` branch, around line 319 and line 346): `gameplay.set_difficulty_badge(blaze4k::difficulty_badge_for(parser.charts().front()));` before `init`. Capture `&app` (already captured) and call `gameplay.render(quad_renderer, w, h, &app.theme_textures(), &app.text_renderer());`. `App` loads the theme and fonts in `init` for both paths and calls `text_renderer_.set_window_size` every frame (`src/app/app.cpp:52-58, 108`).
- **Validate**: `cmake --build build -j8` (the app builds; tests may still fail to compile until Tasks 5-7).

### Task 5: `hud_renderer_test`

- **File**: `tests/hud_renderer_test.cpp`
- **Action**: UPDATE
- **Implement** (keep `TEST_CHECK`, `near`, `field_left_for`, `layout_at`; drop `percent_bottom` and the `bitmap_font.hpp` include):
  - `test_life_bar_matches_mock_720p`: frame `{40,120,40,480}`, track `{44,124,32,472}`, full fill == track, `border == 4`.
  - `test_life_bar_scales`: every row of the life-bar table (2560x1440, 1920x1080, 3440x1440, 640x480, 1024x768), and `frame == L.rect(kLifeBar)` at those sizes (no clamp).
  - `test_bottom_up_fill`, `test_clamp`, `test_danger` (0.29 danger, 0.3 not, −1 danger) on the track. `fill == *fill_cropped_rect(track, fraction)` for fraction > 0.
  - `test_track_inside_frame`: the track is the frame inset by `4·s` on every side, at all common sizes.
  - `test_no_field_overlap_common_sizes`: `frame.x + frame.w + kGap <= field_left`, and no intersection with the field rect (as today).
  - `test_below_diff_badge` (replaces `test_below_percent_text`): `frame.y >= badge.plate.y + badge.plate.h` at the common sizes and 320x240.
  - `test_judgment_pop_clear` (rewritten): at peak `pop_scale` (scan the curve as today), `judgment_pop_rect(L, kJudgmentContentRef·s, peak)` is centred on `w/2 == field_left + 216`, and its left edge is `>= frame.x + frame.w + kGap` at 640x480, 500x400, 400x400 and 1280x720.
  - `test_narrow_clamp`: 500x400 gives x 2.375, w 15.625 and right edge ≤ 18. 400x400 gives x 0, w 8.5 and track w 6. All rects finite.
  - `test_diff_badge_layout`: 720p "HARD 8"-sized text (82) gives `{36,28,130,40}`, `text_x 54`, `text_max_w 94`. A text width of 200 gives width 236. 400 is capped at 300. 2560x1440 is ×2. 500x400 is hidden. Non-positive size is not visible.
  - `test_combo_line_layout`: given widths/ascents, the pen box is centred on `w/2`, `number.y == L.y(368)`, and the label shares the baseline. Checked at 1280x720 and 3440x1440 (origin 440).
  - `test_degenerate`: unchanged intent (invisible for non-positive sizes).
- **Mirror**: the existing file structure and `main` call list.
- **Validate**: `cmake --build build -j8 && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R hud_renderer_test`

### Task 6: `judgment_animator_test`

- **File**: `tests/judgment_animator_test.cpp`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - CMake: add `target_compile_definitions(judgment_animator_test PRIVATE BLAZE4K_SOURCE_DIR=… BLAZE4K_ASSETS_DIR=…)`, mirroring `select_screen_test`.
  - Replace `test_color_mapping` with `test_sprite_mapping`. Over every (kind, window) pair (loop as in `hud_renderer_test.cpp:168-176`), `judgment_sprite(e).empty() == judgment_label(e).empty()`. The nine named mappings are exact. Every non-empty name exists in the real manifest (`parse_theme_manifest`) as a `ThemeKind::Sprite` with content 888x132, which pins `kJudgmentContentRef`.
  - `test_combo_visibility`: `update(dt, 3)` → not visible. `4` → visible with `live_combo() == 4`. `0` → hidden. `reset()` → 0.
  - `test_combo_number_color`: gold at 0, white at ≥ the duration, strictly between the two at half.
  - `test_headless_render_and_reset`: call `render_judgment(renderer, 1280, 720, nullptr)` and `render_combo(renderer, 1280, 720, nullptr)` on an uninitialised renderer, plus a headless-loaded `ThemeTextures` + `TextRenderer` (real assets). No crash at 0x0.
  - Keep the label, curve, consume and milestone tests unchanged.
- **Validate**: `... ctest ... -R judgment_animator_test` (sandboxed command)

### Task 7: `gameplay_screen_test`

- **File**: `tests/gameplay_screen_test.cpp`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - CMake: asset-root definitions for `gameplay_screen_test` (same as above).
  - `test_difficulty_badge_for`: Hard 8 → "HARD 8" + `kHard` fill/ink. Challenge → `kChallenge`. Edit "Crazy Edit" 11 → "Crazy Edit 11" + `kEdit`. Edit with no description → "EDIT 5" + `kEdit`. Empty label, meter 1 → resolved "BEGINNER 1" + `kBeginner`. Build charts like `select_art_test.cpp:85` (`make_chart(label, description, meter)`).
  - `test_enter_sets_badge_and_renders_headless`: as in `test_end_delay_before_results`, with the chart labelled "Hard" meter 8 and `ctx.theme` / `ctx.text` set to headless-loaded real assets. After `start(Gameplay)`, `gameplay->difficulty_badge().text == "HARD 8"`. Then `manager.render(renderer, 1280, 720)` with an uninitialised `GlQuadRenderer` does not crash (`ScreenManager::render(renderer, w, h)`, `src/screens/screen_manager.hpp:31`).
  - Keep `test_end_delay_before_results` unchanged.
- **Validate**: `... ctest ... -R gameplay_screen_test` (sandboxed command)

### Task 8: README and full validation

- **File**: `README.md`
- **Action**: UPDATE line 50 (see Files to Change).
- **Validate**: the full Validation block below. 48/48 must pass and the build must add no new warnings (`cmake --build build -j8 2>&1 | grep -i warning` shows nothing from the touched files).

---

## Validation

```bash
# Build (host, existing build dir)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8

# Lint: no linter configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j8 2>&1 | grep -iE "warning" | grep -E "hud_renderer|judgment_animator|gameplay_view|gameplay_screen|main\.cpp" || echo "no new warnings"

# Tests (MUST run sandboxed: the tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -j8
```

Expected: **48/48 pass** (baseline 48/48 on `97d202f`).

Static checks:

```bash
# The removed elements are gone from the gameplay path
grep -nE "percent_text_rect|judgment_color|draw_chip|kLifeBarMinInset" src tests -r   # expect no hits
# format_percent survives for results/select only
grep -rn "format_percent" src | grep -v hud_renderer   # results_screen.cpp, select_screen.cpp
# Nothing in the HUD/animator reads the clock
grep -nE "clock_|time_seconds|steady_clock|SDL_GetTicks" src/gameplay/hud_renderer.cpp src/gameplay/judgment_animator.cpp   # expect no hits
```

## End-to-End Verification

1. **Headless smoke, demo path** (agent-runnable, inside bwrap, scratch data dir):
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 30 --data-dir <scratch>/data --gameplay-demo "songs/In The Groove/Anubis/Anubis.sm"`
   Expected: `[GameplayView] Loaded chart 'Hard' (meter 8)`, the final score line, `Blaze 4k shut down cleanly.`, and no new error lines.
2. **Headless smoke, shell path**: `... ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits cleanly (gameplay is not entered, but the TU wiring is linked and the shell starts).
3. **Windowed visual check (owner, cannot be automated here)**: run `./build/blaze-4k --gameplay-demo "songs/In The Groove/Anubis/Anubis.sm"` at 1280x720 next to `docs/cabinet-theme/reference/cabinet-v3-gameplay.png`, then again at 2560x1440 (or with the window maximised).
   - Top-left: a red "HARD 8" badge with dark ink. There is no percent at the top-left and no chips at the top-right.
   - Left: a chrome life frame at 40..80 x 120..600 with a cyan gradient fill and 12 px stripes. The colours stay put as life changes. Below 30% (miss on purpose, `--fail-off`) the fill switches to `life_fill_danger`.
   - Hits show the baked FANTASTIC / EXCELLENT / … / MISS sprites centred on the field, popping 1 → 1.25 → 1 and fading as before. Hold OK/NG and mines show `judgment_ok` / `judgment_ng` / `judgment_mine`.
   - From combo 4, "N COMBO" sits under the judgment at y≈368, skewed, and stays visible between judgments. A miss hides it. At 50 / 100 / … and at the song end the number flashes gold and returns to white.
   - The note field, receptors, explosions and background dim look exactly as on `main`.
   - Through the shell (Title → Select → play an Edit chart, if one exists): the badge shows the chart name, or "EDIT" in neutral steel.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Per-frame allocations from text (truncate, `to_string`) | The badge's truncated label is cached and recomputed only on text/scale/width change. The combo string fits SSO (≤ 15 chars). Sprite names are `string_view` literals and the theme lookup is heterogeneous (no allocation) | In scope |
| A mid-song atlas bake (hitch) from a scaled text style | The combo line never scales its text; the milestone pop is colour-only. All four styles are in `kAllStyles`, so they are pre-baked | In scope |
| Double shear on the judgment | The sprite already has the slant baked in; only the combo text gets `kComboGroupShear` | In scope |
| The HUD scales with `s` but the note field does not, so narrow windows could make the HUD overlap the field | The life bar keeps the slide/shrink clamp against `field_left`, and the badge hides when it cannot fit. Tests pin 500x400 and 400x400, plus pop clearance at peak scale | In scope |
| A missing or broken manifest hides judgment feedback | Bitmap label fallback (white, same curves) when `entry(sprite)` is null. The life bar falls back to flat quads inside `ThemeTextures` | In scope |
| Music-clock contamination | No new clock reads. Animation stays on `fixed_dt` in `update`, and render only reads animator state. The static grep in Validation pins it | In scope |
| `select_art` (screens) is used from gameplay code | Avoided: `GameplayScreen` (screens layer) builds the badge and hands `GameplayView` a plain `DifficultyBadge` (strings + colours). `src/gameplay` does not include `screens/` | In scope |
| The note field is not scaled at 1440p (forward reference from plan 043) | Not changed: the issue requires the field untouched. Open Question 2 | Out of scope (flag) |
| Cel textures loaded twice (forward reference from plan 044) | Not changed: the noteskin stays untouched | Out of scope (flag) |
| OpenITG miss-combo display (`ShowMissCombo=1`, red miss count) | Not implemented; `ScoreState` has no miss-combo counter | Out of scope (flag) |

---

## Open Questions

1. **Combo display policy (non-blocking; default chosen).** The issue moves the combo under the judgment but does not say when it shows. Today the HUD draws the live combo top-centre at all times ("0x" included), and the animator pops it at top-centre every 50 combo and at the song end.
   **Default:** a persistent combo line from combo ≥ 4 (OpenITG `ShowComboAt=4`), replacing both top-centre draws. The milestone and final pop become a gold → white flash of the number, because scaling the text would bake a new font atlas mid-song.
   **Alternative:** show the combo only while the judgment pop is visible (fading with it). That is less faithful to ITG.
2. **Note-field scaling (non-blocking).** Plan 043 assigned "scaling the field" to #93, but #93's own text says to leave the field untouched. **Default:** do not scale it. Open a follow-up issue if the field should grow with `s` at 1440p (it currently stays 108 px per column, so the field looks smaller next to the 2x HUD).
3. **Badge text alignment and long Edit names (non-blocking).** The mock places "HARD 8" left-aligned 18 px in from the plate, not centred. **Default:** left at +18, with the plate growing to fit up to 300 ref px, then "..." truncation. **Alternative:** a fixed 130 px plate with heavier truncation.
4. **Downscroll placement (non-blocking).** The judgment and combo use the same y in both scroll directions, as the pop does today (`h·0.42`). ITG offsets the combo for reverse (`ComboYReverse`). **Default:** keep one position. The mock is upscroll only.

---

## Acceptance Criteria

- [ ] The live percent, the judgment chips and their per-window colours are no longer drawn (`judgment_color`, `percent_text_rect` and the chip code are removed). `ScoreKeeper` is untouched and scoring, combo and life still update (score_keeper_test and life_keeper_test are green)
- [ ] Life bar = `life_frame` (9-slice) + `life_fill`, or `life_fill_danger` below 0.3 (`theme::color::kLifeDangerThreshold`), cropped with stable colours, + `life_stripes` tiled (bottom-anchored). It starts from `theme::layout::kLifeBar` and keeps `layout_life_bar`'s field-clearance clamp
- [ ] `diff_badge` at `kDiffBadge` is tinted by `theme::difficulty` with e.g. "HARD 8" in the ink colour. Edit charts show their name or "EDIT" in `kEdit`
- [ ] The judgment pop draws `judgment_<kind>` with the existing scale and fade curves. The combo (`kComboNumber` + `kComboLabel` "COMBO") sits under it at `kComboTop`, sheared by `kComboGroupShear`
- [ ] Note field, receptors, Cel noteskin and background dim are unchanged (no diff above the HUD calls in `GameplayView::render`)
- [ ] `hud_renderer_test`, `judgment_animator_test` and `gameplay_screen_test` are updated and pass. 48/48 tests pass under the sandboxed ctest command
- [ ] No new compiler warnings. No clock reads in the HUD or animator. No per-frame heap allocations added
- [ ] `.agents/stories/todo-stories.md` untouched
