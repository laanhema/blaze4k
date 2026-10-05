# Plan: Cabinet Score Screen (#95)

## Summary

Rebuild `ResultsScreen::render` to match `docs/cabinet-theme/reference/cabinet-v3-results.png`, drawing only presentation.
The bitmap-text layout (`draw_text("RESULTS")`, `grade_color`, the one-line window counts, the "FAILED" / "NEW RECORD" text) is replaced by Cabinet art:
`bg_results`, `bar_top` + `title_score_screen`, a code-drawn difficulty badge plus the song title and artist right-aligned in the bar,
three `stat_panel` plates on the left (MAX COMBO, DANCE POINTS "n / max", HOLDS OK / NG / MINES) with `digits_white` values,
the `medallion` with `grade_<tier>` and a tier label ("ONE STAR") in the centre, the `digits_chrome` percentage counting up below it, a `record_ribbon` / `failed_ribbon`,
six judgment rows on the right (coloured label, `kBarTrack` track + fill via `draw_quad_points`, right-aligned count), the 44 px hint bar ("ENTER CONTINUE") and the scanlines overlay.
A new `src/screens/results_art.{hpp,cpp}` holds the pure layout (reference px, headless-testable) and thin draw helpers, the same split as `select_art` / `title_art`.
The existing `ResultsAnimator` curves are reused unchanged and mapped onto the new parts: the bar text fades in, the grade slams 2.4x → 1x on the medallion, the panels and bars fade, the ribbon pops, and the white NEW RECORD flash stays.
Submission, the best-score rule, input handling (skip, then continue) and `ResultsSummary` are not touched. Nothing reads the music clock; everything runs on the fixed `dt`.

## User Story

As a player
I want the score screen to look like the Cabinet mock-up, with a grade medallion, a chrome percentage, stat panels and judgment bars
So that the end of a song matches the restyled title, select and gameplay screens, while my score, record and controls behave exactly as before

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | HIGH (issue says L: one screen rewritten, one new art module, two tests reworked) |
| Systems Affected | `src/screens/results_screen.*`, `src/screens/results_anim.hpp` (comments only), new `src/screens/results_art.*`, `CMakeLists.txt` (core source list), `tests/results_screen_test.cpp`, `tests/results_anim_test.cpp`, `tests/CMakeLists.txt` (asset roots), `README.md:51` |
| GitHub Issue | #95 (TODO-23; blocked by #88, #89, #90, #91, all merged; blocks #98) |
| Plan sequence | 047 (running plan sequence; the issue number is #95) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release, GCC, FetchContent deps in `build/_deps`). Build with `cmake --build build -j8` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **48/48 pass** | Run on `main` @ `374727a` with the sandboxed command in Validation. The count stays **48**: no test executable is added (the art helpers are tested inside `results_screen_test`) |
| Sandbox requirement | — | `audio_test` opens the real sound device, so **always** run ctest (and any binary) inside the `bwrap` command in Validation |
| Reaching Results headless | — | `--start-screen` only accepts `title` / `select`, and `--gameplay-demo` never enters Results. The automated render check is `results_screen_test` with the real theme and fonts loaded headless (draws are no-ops, measuring works) |
| Reference image | `docs/cabinet-theme/reference/cabinet-v3-results.png` (1280x720) | The issue says `reference/…`; the file is under `docs/cabinet-theme/` |
| Score art (manifest) | `assets/theme/cabinet/manifest.json` | `bg_results` fullscreen. `bar_top` stretch_x 132 @2x (64 + 2 rule). `bar_hint` stretch_x 108 @2x ("44px on the score screen, stretching vertically is fine"). `title_score_screen` sprite, layout `[40,9,300,46]`. `stat_panel` slice3, content 720x192 @2x, caps 64/64, "label 18px … at x+20, y+14; value in the white digit font". `medallion` sprite content 760x760 @2x = `kMedallion` 380x380, anchor centre. `grade_*` (17 names, see below) sprite content 680x400 @2x = **340x200 ref**, anchor centre, mipmapped. `record_ribbon` / `failed_ribbon` sprite content 680x88 @2x = `kRecordRibbon` 340x44, anchor centre. `scanlines` tile, "title, select and score screens only" |
| Grade textures | `grade_{quad,triple,double,single}_star`, `grade_{S,A,B,C}{_plus,,_minus}`, `grade_D` | Exactly the 17 compiled labels of `JudgmentConstants::grade_tiers` (`src/timing/judgment_constants.cpp:49-66`) after `+` → `_plus`, `-` → `_minus`. Labels are **not** configurable (`src/data/judgment_constants_loader.cpp:142-144` ignores JSON `label`). No F texture |
| Digit fonts | `digits_white` (Audiowide 96 @2x = 48 ref), `digits_chrome` (156 @2x = 78 ref) | Glyphs `0-9 . % /` and space only (`kDigitChars`). Proportional advances. Every glyph rect has a 28 @2x = **14 ref** pad (`origin_x`). Ink rows inside a glyph rect: white 49..115 @2x (cap top **24.5 ref**, baseline **57.5 ref**); chrome bright face 62..166 @2x (top **31 ref**) |
| Theme constants | `src/render/theme.hpp:106-115, 140-147, 203-217, 224-229` | `color::kFantastic … kMiss, kMissLabel, kHoldOkLabel, kBarTrack, kSteel, kText`. `text::kBarSongTitle, kBarArtist, kBarBadge, kStatLabel, kTierLabel, kJudgmentLabel, kJudgmentCount, kHintKey, kHintWord` (all already in `kAllStyles`, so pre-baked). `layout::kMedallion, kTierLabelTop, kPercentTop, kRecordRibbon, kStatPanel{X,Top,Width,Gap}, kJudgmentBars{X,Top,Width}, kJudgmentBarRowPitch/Height/LabelWidth/CountWidth`. `skew::kRows, kStatPanel` |
| Reusable helpers | `src/gameplay/hud_renderer.hpp:55-75`, `src/screens/gameplay_screen.cpp:17-23` | `DifficultyBadge {label, meter, colors}`, `badge_text_width`, `fit_badge_text` (only the label truncates) and `difficulty_badge_for(chart)` ("HARD 8", Edit name or "EDIT", `kEdit` colours) from #93. `format_percent` (digits, `.` and `%` only; clamps to [0, 1]) |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Mock measurements vs `theme.hpp` (PIL pixel scans of the reference PNG)

| Item | `theme.hpp` / manifest | Mock | Plan uses |
|---|---|---|---|
| Top bar | 64 + 2 rule | bar y 0..63, black rule 64..65 | **matches** (`bar_top` via `draw_stretch_x`) |
| Bar badge plate | not in theme.hpp ("results badge" uses the difficulty fill) | solid red parallelogram, y 16..47, slope ≈ 0.21 (= `skew::kRows`), mid-height x 957..1040 | **code-drawn** `draw_quad_points` parallelogram, `{x, 16, text_w + 2·12, 32}`, `skew::kRows`, colour `colors.fill`, text `kBarBadge` in `colors.ink`, line box centred in the plate |
| Bar title / artist | `kTopBarPadX 40` | artist ink right edge 1239; title caps 24..39, artist caps 25..38 (shared baseline ≈ 40); plate → title gap and title → artist gap ≈ 16 | artist right at `1280 − 40 = 1240`, shared **baseline 40**, both gaps **16** (`kBarGap`) |
| Hint bar | `kHintBarHeight 52` ("score screen: 44") | rule y 674..675, band 676..719 (44), "ENTER CONTINUE" caps centred ≈ 698 | `bar_hint` stretched to `{0, 674, w, 46}`; text centred in the band `[676, 720]` |
| Stat panels | `kStatPanelX 44, Top 120, Width 360, Gap 14`; no heights | border rows 120..230, 245..355, 370..471 (heights **111, 111, 102**; gap 14 ✓). Mid-height x 44..403 ✓ | rects `{44,120,360,111}`, `{44,245,360,111}`, `{44,370,360,102}` (`kStatPanelHeights`, measured) |
| Panel text x | manifest "label at x+20, y+14" | label ink x 69, value ink x 64 (holds 63). The text follows the panel's −8° slant | `x = panel.x + 20 + skew::kStatPanel · (panel centre y − element centre y)`. Label line top `panel.y + 14` (manifest) |
| Panel values | white digit font | MAX COMBO / DP numerator: cap height 33 (scale **1.0**), baseline `panel.y + 86`. "/ 2000": steel, cap 19 (scale **28/48**), same baseline, starts 8 px after the numerator. Holds: cap 28 (scale **40/48**), baseline `panel.y + 79`. Hold columns: label ink x 69 / 192 / 246 (label width + **34**) | constants `kValueBaseline 86`, `kHoldBaseline 79`, `kDpMaxScale 28/48`, `kHoldScale 40/48`, `kDpGap 8`, `kHoldColumnGap 34`; glyph top = baseline − 57.5·scale |
| Medallion | `kMedallion {450,110,380,380}` | ring y 110..~490, centred on x 640 | **matches** |
| Grade sprite | "centred on the medallion" (300) | star gold-ink centre ≈ (640, 286); the sprite's ink is centred in its image | grade centre **(640, 286)** (`kGradeCentreY`), 14 px above the medallion centre |
| Tier label | `kTierLabelTop 388` | "ONE STAR" caps y 382..394, centred on x 640 | **baseline 395** (`kTierLabelBaseline`); 388 is the caps' centre, not a line top |
| Percent | `kPercentTop 500` | bright face y 512..565, ink x 486..795 (centred on 640, scale 1.0) | glyph top **481** (`kPercentGlyphTop`, = 512 − 31). Reading 500 as the CSS line top would put it at 486 (5 px low) |
| Ribbon | `kRecordRibbon {470,600,340,44}` | red ink x 467..816, y 602..645 | **matches** |
| Judgment rows | `kJudgmentBarsX 878, Top 120, Width 360, RowPitch 45, BarHeight 14, LabelWidth 130, CountWidth 56` | label ink x 879, caps 130..144 (row 0, baseline 145); bar x 1018..1171, y 131..144; count ink right 1236; pitch 45 ✓ | row top `120 + 45i`; label at x 878, **baseline row top + 25**; bar `{878 + 130 + 10, row top + 11, 154, 14}` (`kBarGapX 10`); count right-aligned at 1238 |
| Bar shape | issue note: "`draw_quad_points` and `skew::kRows`" | **not slanted**: the bar's left edge is x 1018 on every row y 131..144. Flat colours, no gradient | skew **0** by default (`kJudgmentBarSkew`, one constant to flip; Open Question 2) |
| Bar fill | — | FANTASTIC 132 / 154 px for 312 of 362 taps (0.857 vs 0.862 ✓ count / total judged taps). GREAT 6 → 3 px, DECENT 1 → 2 px, MISS 2 → 2 px, WAY OFF 0 → none | `fill_w = count > 0 ? clamp(bar_w · count / total, 2, bar_w) : 0`, total = Fantastic..Miss (`kMinBarFill 2`) |
| Bar colours | `kFantastic … kWayOff, kMiss`, labels `kMissLabel` for MISS | fills: 2FD2FF, 6DFF6A, FFD633, FFFFFF, (none), FF4A4A; MISS label FF6A6A; counts EAF1FF | fills use the judgment colour (MISS fill `kMiss`, MISS label `kMissLabel`); counts `kJudgmentCount` re-tinted to **`kText`** (the style says white; the mock is EAF1FF) |
| Scanlines | manifest: score screen yes | 1 dark row in 3 over the whole frame | drawn last, like select |

### Forward references to #95 found in prior plans

| Where | Note | This plan |
|---|---|---|
| `.agents/plans/completed/041-theme-textures-bitmap-digits-plan.md:434` | "Digit atlas mipmaps … Revisit in #95 if the chrome percent shimmers" | **Not changed.** The percent counts up for 0.8 s and then holds still; owner checks for shimmer in the windowed run (Risks) |
| `.agents/plans/completed/042-ttf-text-rendering-plan.md:465`, `045-select-screen-cabinet-plan.md:315, 497` | "#95 switches `song_display_title` / `chart_display_label` to TTF" | **Done for results:** the bar uses `song_display_title(meta, text, kBarSongTitle.font)` / `song_display_artist(…, kBarArtist.font)` and `difficulty_badge_for` (which uses `chart_display_label(chart)`). The bitmap overloads stay (select tests and the TTF fallback use them) |
| `.agents/plans/completed/040-renderer-quad-points-repeat-wrap-plan.md:10` | "#95 will call `draw_quad_points`" | **Done:** badge plate, judgment tracks and fills |

---

## Patterns to Follow

### Screen render: layout scale, null-checked services (copy #94)

```cpp
// SOURCE: src/screens/select_screen.cpp:563-576
void SelectScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;
    if (theme != nullptr) {
        select_art::draw_backdrop(*theme, renderer, w, h);
    }
```

### Art module: pure reference-px layout + thin draw helpers (copy `select_art`)

```cpp
// SOURCE: src/screens/select_art.hpp:1-13, 41-47
// Cabinet v3 Song Select art (#94), used by SelectScreen.
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so select_art_test
//    pins them headless. ...
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
inline constexpr float kHintBarTop = theme::layout::kRefHeight - theme::layout::kHintBarHeight - 2.0f;
```

### Private text helpers (copy into `results_art.cpp`'s anonymous namespace)

```cpp
// SOURCE: src/screens/select_art.cpp:47-62, 74-79
float ref_measure(const TextRenderer& text, std::string_view s, const theme::TextStyle& style);
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style);
theme::TextStyle with_color(theme::TextStyle style, Color color);
void draw_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const std::array<Vec2, 4>& ref_corners, Color color);  // static Texture kSolid (id 0)
```

`select_art::skewed_quad(rect, skew)` (`select_art.cpp:220-225`, public) gives the CSS skewX parallelogram about the rect's vertical centre. Reuse it.

### Hint bar (adapt: one key + one word, 44 px band)

```cpp
// SOURCE: src/screens/select_art.cpp:372-408
const float bar_h = theme->content_size("bar_hint", L.s).y;
theme->draw_stretch_x(renderer, "bar_hint", 0.0f, L.y(layout::kRefHeight) - bar_h, static_cast<float>(w), L.s);
...
text->draw(renderer, piece.text, L.x(piece.x), centred_top(*text, L, kHintBandTop, kHintBandHeight, style), style, TextAlign::Left);
```

The score screen needs a 46 px bar, so use `theme->draw_stretch("bar_hint", Rect{0, L.y(674), w, L.px(46)})` (`draw_stretch` accepts any kind, `theme_textures.cpp:946-954`).

### Difficulty badge text (reuse #93, do not duplicate)

```cpp
// SOURCE: src/screens/gameplay_screen.cpp:17-23 and src/gameplay/hud_renderer.hpp:65-73
DifficultyBadge difficulty_badge_for(const Chart& chart);   // label + meter + colours
float badge_text_width(const DifficultyBadge&, const std::function<float(std::string_view)>& measure);
std::string fit_badge_text(const DifficultyBadge&, float max_w, const std::function<float(std::string_view)>& measure);
```

### Bitmap digits

```cpp
// SOURCE: src/render/theme_textures.hpp:225-236
[[nodiscard]] float measure(std::string_view text, float s) const;   // screen px
void draw(GlQuadRenderer& renderer, std::string_view text, float x, float y, float s,
          DigitAlign align, Color tint = Color{}) const;              // y = glyph-rect top
```

A smaller digit line passes `L.s * scale` as `s` (for example `L.s * 28/48` for "/ 2000").

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real assets headless)

```cpp
// SOURCE: tests/select_screen_test.cpp:55-80
const std::filesystem::path kSourceDir{BLAZE4K_SOURCE_DIR};
const std::filesystem::path kCabinet = std::filesystem::path{BLAZE4K_ASSETS_DIR} / "theme" / "cabinet";
blaze4k::ThemeTextures& loaded_theme() { static blaze4k::ThemeTextures theme; static const bool loaded = theme.load(kCabinet); ... }
blaze4k::TextRenderer& loaded_text() { static blaze4k::TextRenderer text; static const bool loaded = text.load(kSourceDir); ... text.set_window_size(1280, 720); return text; }
```

```cmake
# SOURCE: tests/CMakeLists.txt:195-200
target_compile_definitions(select_screen_test PRIVATE
    BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)
```

---

## Pinned Semantics (the contract the tests pin)

All `results_art` layout values are **reference px** (1280x720). Screen px = `L.x / L.y / L.px / L.rect` with `L = theme::layout_scale(w, h)`. Text and digit widths passed to the pure helpers are reference px (`measure / scale`).

### Strings (computed once in `ResultsScreen::enter`, never per frame)

- `results_art::kScreenTitleText = "SCORE SCREEN"`: drawn in `kBarSongTitle` at x 40 (baseline 40) only when the `title_score_screen` sprite is unavailable (`theme == nullptr` or `theme->entry("title_score_screen") == nullptr`) and `text` is non-null. Normally the baked sprite is drawn.
- `grade_texture_name(label)`: `""` for an empty label, else `"grade_"` + label with every `+` → `_plus` and `-` → `_minus`. `"quad_star"` → `"grade_quad_star"`, `"S+"` → `"grade_S_plus"`, `"A-"` → `"grade_A_minus"`, `"D"` → `"grade_D"`. All 17 compiled labels resolve to a `ThemeKind::Sprite` entry in the real manifest.
- `grade_tier_text(label)`: `quad_star` → `"FOUR STARS"`, `triple_star` → `"THREE STARS"`, `double_star` → `"TWO STARS"`, `single_star` → `"ONE STAR"`, any other non-empty label → `"GRADE " + label` (e.g. `"GRADE S+"`), empty → `""` (Open Question 3).
- `digits_text(int v)`: `std::to_string(std::max(v, 0))`. DP can be negative (a Miss is −12 DP), and `-` is not a digit glyph, so negative values show `0` (Open Question 4). Every string that reaches `BitmapDigits` is built from `digits_text` or `format_percent`, so it contains only `kDigitChars`.
- Panel values: MAX COMBO `digits_text(max_combo)`; DP `digits_text(actual_dp)` and `"/ " + digits_text(possible_dp)`; holds `digits_text(hold_counts[Ok])`, `digits_text(hold_counts[Ng])`, `digits_text(tap_counts[HitMine])` (MINES = mines hit, same as today's "MINE"; Open Question 5).
- Judgment rows: labels `FANTASTIC, EXCELLENT, GREAT, DECENT, WAY OFF, MISS` (`kJudgmentRowLabels`), counts `std::to_string(tap_counts[i])`, `total = Σ tap_counts[Fantastic..Miss]` (HitMine excluded).
- Badge: `difficulty_badge_for(*chart)` (null chart → `DifficultyBadge{}` with empty text: no plate drawn). Title / artist: `song_display_title(meta, ctx.text, kBarSongTitle.font)` / `song_display_artist(meta, ctx.text, kBarArtist.font)`; null song → `"UNKNOWN"` / `""`.

### Top bar group (`top_bar_layout(badge_text_w, title_w, artist_w)`, right to left)

- `kBarRight = 1240`, `kBarGap = 16`, `kBarLeftLimit = 360` (clear of the 40..340 title sprite), `kBarArtistMax = 240`, `kBarBadgeTextMax = 200`, `kBarBadgePadX = 12`, `kBarBadgeTop = 16`, `kBarBadgeHeight = 32`, `kBarTextBaseline = 40`.
- `artist_w' = min(artist_w, 240)`; `artist_x = 1240 − artist_w'`. An empty artist (`artist_w ≤ 0`) takes no width and no gap.
- `badge_w' = min(badge_text_w, 200)`; `plate_w = badge_w' + 24` (0 when `badge_text_w ≤ 0`: no plate, no gap).
- `title_max = max(0, artist_x − gap_if_artist − 360 − plate_w − gap_if_plate)`; `title_w' = min(title_w, title_max)`; `title_right = artist_x − gap_if_artist`; `title_x = title_right − title_w'`.
- `plate = {title_x − 16 − plate_w, 16, plate_w, 32}` (no gap when the title is empty).
- Outputs: `plate`, `badge_text_x = plate.x + 12`, `badge_text_max_w = badge_w'`, `title_x`, `title_max_w = title_w'`, `artist_x`, `artist_max_w = artist_w'`, `baseline = 40`.
- Mock check: artist ≈ 50 and title ≈ 118 wide give `artist_x ≈ 1190`, `title_x ≈ 1056`, plate right ≈ 1040 (mock ink: 1189, 1057, 1040 at mid-height). Accept ±4.
- Draw: plate `draw_solid(skewed_quad(plate, skew::kRows), colors.fill · title_alpha)`; badge `fit_badge_text(badge, badge_text_max_w, measure)` in `with_color(kBarBadge, colors.ink)`, line box centred in the plate; title `text->truncate(title, kBarSongTitle, L.px(title_max_w))`, artist likewise; both line tops `L.y(40) − text->ascent(style)`. The three fitted strings are cached and recomputed only when `text->scale()` changes.

### Stat panels (`stat_panel_rect(i)`, `stat_text_x(panel, centre_y)`, `hold_columns(...)`)

- `stat_panel_rect(i)` for i = 0, 1, 2: `{44, 120 / 245 / 370, 360, 111 / 111 / 102}` (`kStatPanelHeights`, measured). Out-of-range i clamps to [0, 2].
- `stat_text_x(panel, centre_y) = panel.x + 20 + skew::kStatPanel · (panel.y + panel.h / 2 − centre_y)` (`kStatTextPadX 20`).
- Labels (`kStatLabel`, `kIce`): `"MAX COMBO"`, `"DANCE POINTS"`; line top `panel.y + 14`; x = `stat_text_x(panel, panel.y + 14 + 14.5)` (the cap centre is ≈ 14.5 below the line top).
- Values (`digits_white`, white, scale 1): glyph top `panel.y + 86 − 57.5`, x = `stat_text_x(panel, panel.y + 86 − 16.5)`, `DigitAlign::Left`.
- DP denominator (`kSteel`, scale `28/48`): glyph top `panel.y + 86 − 57.5·28/48`, x = numerator pen end + 8.
- Holds panel: three columns. Labels `"HOLDS OK"` (`kHoldOkLabel`), `"NG"` and `"MINES"` (`kMissLabel`), `kStatLabel` style re-tinted. `hold_columns(label_w[3], value_w[3])` returns column offsets `{0, c1, c2}` with `c[i+1] = c[i] + max(label_w[i], value_w[i]) + 34`. Values (scale `40/48`) glyph top `panel.y + 79 − 57.5·40/48`, at the same column offset from their own row's `stat_text_x`.
- Mock check: `"HOLDS OK"` ≈ 89 wide gives NG at +123 and MINES at +178 from the first column (mock ink: 69 → 192 → 246). Accept ±3.

### Centre

- Medallion: `draw_sprite("medallion", {L.x(450), L.y(110)}, s)` (content box = `kMedallion`), always at full alpha.
- Grade: `grade_rect(content, scale)`: `content` is the grade sprite's content size in ref px (`kGradeContentRef {340, 200}`), the centre is `(640, 286)`, the size is `content × scale`. Draw `draw_sprite(grade_texture_, {rect.x, rect.y}, s · scale, alpha)` with `scale = grade_scale(elapsed)`, `alpha = grade_alpha(elapsed)`. Fallback (no theme, or no manifest entry for the name): `format_grade` text in `kSongTitle` (gold), centred on (640, 286), same alpha, no scale (no mid-screen atlas bake).
- Tier label: `kTierLabel`, centred on x 640, line top `L.y(395) − ascent`, alpha `grade_alpha`.
- Percent: `format_percent(percent · percent_progress(elapsed))` in `digits_chrome`, `DigitAlign::Centre` at x 640, glyph top 481, full alpha (counts up from 0.00% exactly as today).
- Ribbon: `ribbon_rect(scale)`: centred on the centre of `kRecordRibbon` (640, 622), size `340·scale × 44·scale`. NEW RECORD (`shows_record_finale()`): `record_ribbon` at `record_scale(elapsed)` with alpha `record_alpha(elapsed)`, drawn only when the scale > 0. Failed: `failed_ribbon` at scale 1 with `failed_alpha(elapsed)`. Neither for a normal clear. The grade stays the earned tier on a fail (Open Question 1).
- Flash: `shows_record_finale()` and `record_flash(elapsed) > 0` → one white full-window quad with that alpha, right after `bg_results` (behind everything else, as today).

### Judgment rows (`judgment_row_layout(i)`, `judgment_bar_fill_width(count, total, bar_w)`)

- Row top `120 + 45·i`. `label_x = 878`, `baseline = row top + 25`, `bar = {1018, row top + 11, 154, 14}`, `count_right = 1238`.
- `judgment_bar_fill_width`: 0 when `count ≤ 0` or `total ≤ 0`; else `clamp(bar_w · count / total, kMinBarFill = 2, bar_w)`. Mock numbers: (312, 362, 154) → ≈ 132.7; (1, 362, 154) → 2; (0, 362, 154) → 0; (5, 3, 154) → 154.
- Draw: track `draw_solid(skewed_quad(bar, kJudgmentBarSkew = 0), kBarTrack)`, fill `{bar.x, bar.y, fill_w, bar.h}` skewed the same in the judgment colour (`kFantastic, kExcellent, kGreat, kDecent, kWayOff, kMiss`). Label `with_color(kJudgmentLabel, label colour)` (MISS uses `kMissLabel`), count `with_color(kJudgmentCount, kText)` right-aligned. All at `stats_alpha`.

### Hint bar

- `bar_hint` stretched over `{0, 674, w, 46}` (ref y; full window width). Text: `"ENTER"` (`kHintKey`) + 8 + `"CONTINUE"` (`kHintWord`), the pair centred on x 640 and centred vertically in the band `[676, 720]`. While the reveal runs (valid summary, not finished) the word is `"SKIP"` instead (Open Question 6). A NO RESULT screen shows `"ENTER CONTINUE"` immediately.

### Draw order in `ResultsScreen::render` (background → bars/plates → sprites → digits → text → overlay)

1. `bg_results` over the window (`draw_stretch`). Then the NEW RECORD flash.
2. `bar_top` + `title_score_screen`, `bar_hint`.
3. NO RESULT path: `"NO RESULT"` in `kWheelRow`, centred on x 640 at y 340 (like select's empty message), the hint text, scanlines, return.
4. Medallion; stat panels (`draw_slice3("stat_panel", L.rect(panel), {1,1,1,stats_alpha})`); judgment tracks + fills; badge plate.
5. Grade sprite; ribbon.
6. Digits: panel values (white), percent (chrome).
7. TTF text: badge, title, artist (`title_alpha`); stat labels and judgment labels/counts (`stats_alpha`); tier label (`grade_alpha`); hint text.
8. `scanlines` (`title_art::draw_scanlines`).

A null `theme` skips every texture and digit call. A null `text` skips every text call. `render` returns early for `w ≤ 0 || h ≤ 0`; every `results_art` draw helper also returns on an uninitialised renderer.

### Reveal mapping (`ResultsAnimator`, curves unchanged)

| Part | Curve |
|---|---|
| Badge, title, artist | `title_alpha` (0 → 0.25 s) |
| Grade sprite + tier label | `grade_scale` 2.4 → 0.8 → 1.0 and `grade_alpha` (from 0.15 s) |
| Percent | `percent_progress` count-up (0.45 → 1.25 s) |
| Stat panels, values, judgment rows | `stats_alpha` (from 0.70 s) |
| NEW RECORD ribbon | `record_scale` punch 0.6 → 1.15 → 1.0 and `record_alpha` pulse (from 1.15 s); white flash `record_flash` |
| FAILED ribbon | `failed_alpha` (from 0.70 s) |

Skip (first Confirm / Options / Right) still jumps to the settled frame, and the next press returns to Select.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/results_art.hpp` | CREATE | Constants (measured, reference px), pure layout helpers (`top_bar_layout`, `stat_panel_rect`, `stat_text_x`, `hold_columns`, `grade_rect`, `ribbon_rect`, `judgment_row_layout`, `judgment_bar_fill_width`), string helpers (`grade_texture_name`, `grade_tier_text`, `digits_text`, `kScreenTitleText`, `kJudgmentRowLabels`), draw helper declarations |
| `src/screens/results_art.cpp` | CREATE | Their implementation, plus draw helpers: `draw_backdrop`, `draw_top_bar` (bar + sprite or text fallback), `draw_hint_bar`, `draw_stat_panels`, `draw_judgment_rows`, `draw_scanlines` wrapper. Private `ref_measure` / `centred_top` / `with_color` / `draw_solid` |
| `CMakeLists.txt` | UPDATE | Add `src/screens/results_art.cpp` to the core sources next to `results_screen.cpp` (line ~135) |
| `src/screens/results_screen.hpp` | UPDATE | Remove `results_difficulty_line`. Add cached members (badge, display strings, digit strings, grade texture name, tier text, fitted bar strings + cached scale) and test accessors (`badge()`, `grade_texture()`, `tier_text()`, `dp_text()`). Update the class comment (Cabinet layout, reveal mapping) |
| `src/screens/results_screen.cpp` | UPDATE | Delete the palette, `grade_color`, `results_difficulty_line` and the bitmap layout. `enter` builds the cached strings; `render` draws in the pinned order. Submission, logging and `update` stay byte-identical |
| `src/screens/results_anim.hpp` | UPDATE | Comments only: the curves now drive the Cabinet parts (medallion slam, ribbon pop); drop "bitmap text has no rotation" and "reproduces the exact C7 static layout". No constant or signature changes |
| `tests/results_screen_test.cpp` | UPDATE | Replace `test_difficulty_line` with badge tests; add pure-layout, string and fill tests; headless render with real theme + fonts at several sizes and states |
| `tests/results_anim_test.cpp` | UPDATE | Add `test_reveal_order` (the mapping's stagger); keep every existing test |
| `tests/CMakeLists.txt` | UPDATE | `BLAZE4K_SOURCE_DIR` / `BLAZE4K_ASSETS_DIR` for `results_screen_test` |
| `README.md` | UPDATE | Line 51: "Results screen: grade, %, DP, judgment breakdown, max combo, "NEW RECORD" flag" → "Score screen: grade medallion, chrome percentage, dance points, judgment bars, max combo, holds and mines, NEW RECORD / FAILED ribbon" |

`theme.hpp`, the manifest, `ResultsSummary`, `ResultsAnimator`'s behaviour and `ScreenContext` are unchanged.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Build after each with `cmake --build build -j8`.

### Task 1: `results_art` pure layout and strings

- **File**: `src/screens/results_art.hpp`, `src/screens/results_art.cpp`, `CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**:
  - Namespace `blaze4k::results_art`. Header comment in the `select_art.hpp:1-13` style: Cabinet v3 score screen (#95); pure reference-px layout + thin draw helpers; presentation only, fixed `dt`, no clock.
  - Every constant from Pinned Semantics as `inline constexpr`, each with a one-line source note ("measured from cabinet-v3-results.png (#95)" or the theme/manifest value it derives from): `kScreenTitleText`, `kBarRight`, `kBarGap`, `kBarLeftLimit`, `kBarArtistMax`, `kBarBadgeTextMax`, `kBarBadgePadX`, `kBarBadgeTop`, `kBarBadgeHeight`, `kBarTextBaseline`, `kHintBarTop 674`, `kHintBarHeight 46`, `kHintBandTop 676`, `kHintBandHeight 44`, `kHintKeyGap 8`, `kStatPanelHeights {111,111,102}`, `kStatTextPadX 20`, `kStatLabelTop 14`, `kStatLabelCapCentre 14.5`, `kValueBaseline 86`, `kHoldBaseline 79`, `kDigitBaselineRef 57.5`, `kDigitCapHalfRef 16.5`, `kDpMaxScale 28/48`, `kHoldScale 40/48`, `kDpGap 8`, `kHoldColumnGap 34`, `kGradeCentre {640, 286}`, `kGradeContentRef {340, 200}`, `kTierLabelBaseline 395`, `kPercentGlyphTop 481`, `kBarGapX 10`, `kBarRowTopOffset 11`, `kJudgmentBaselineOffset 25`, `kMinBarFill 2`, `kJudgmentBarSkew 0`, `kJudgmentRowLabels` (6 `string_view`s), `kEmptyMessageTop 340`.
  - `static_assert`s that tie measured values to `theme.hpp`: `kJudgmentBarsX + kJudgmentBarLabelWidth + 2·kBarGapX + bar_w + kJudgmentBarCountWidth == kJudgmentBarsX + kJudgmentBarsWidth` (bar_w = 154), and `kStatPanelTop + h0 + kStatPanelGap == 245`.
  - Pure functions and structs from Pinned Semantics: `TopBarLayout top_bar_layout(float badge_text_w, float title_w, float artist_w)`, `Rect stat_panel_rect(int)`, `float stat_text_x(const Rect&, float centre_y)`, `std::array<float, 3> hold_columns(std::array<float, 3> label_w, std::array<float, 3> value_w)`, `Rect grade_rect(Vec2 content, float scale)`, `Rect ribbon_rect(float scale)`, `JudgmentRowLayout judgment_row_layout(int)`, `float judgment_bar_fill_width(int count, int total, float bar_w)`, `std::string grade_texture_name(std::string_view)`, `std::string grade_tier_text(std::string_view)`, `std::string digits_text(int)`, `int judged_tap_total(const ResultsSummary&)`. Guard every float input with `std::isfinite` (non-finite → 0) so nothing returns NaN.
  - Add `src/screens/results_art.cpp` to the core library source list in `CMakeLists.txt` (next to `results_screen.cpp`).
- **Mirror**: `src/screens/select_art.hpp:36-120` (constants + pure layout), `select_art.cpp:93-235` (implementations).
- **Validate**: `cmake --build build -j8`

### Task 2: `results_art` draw helpers

- **File**: `src/screens/results_art.hpp`, `src/screens/results_art.cpp`
- **Action**: UPDATE
- **Implement** (each helper returns on an uninitialised renderer; services are pointers where the helper must tolerate null):
  - Private `ref_measure`, `centred_top`, `with_color`, `draw_solid` (copies of `select_art.cpp:47-79`).
  - `draw_backdrop(const ThemeTextures&, renderer, w, h)`: `bg_results` over the window.
  - `draw_bars(const ThemeTextures*, TextRenderer*, renderer, L, w)`: `bar_top` (`draw_stretch_x`, as `select_art::draw_top_bar`), `title_score_screen` at `L.point({40, 9})` when its entry exists, else the `kScreenTitleText` fallback; `bar_hint` via `draw_stretch` over `{0, L.y(674), w, L.px(46)}`.
  - `draw_hint_text(TextRenderer&, renderer, L, std::string_view word)`: "ENTER" + word, centred as pinned.
  - `draw_stat_panel_plates(const ThemeTextures&, renderer, L, float alpha)` and `draw_judgment_bars(renderer, L, const ResultsSummary&, float alpha)` (tracks + fills, code-drawn, so no theme needed).
  - `draw_scanlines(const ThemeTextures&, renderer, w, h, L)` → `title_art::draw_scanlines(theme, renderer, w, h, L.s)`.
  - Keep the screen-specific text (badge, title, panel values, tier, counts) in `ResultsScreen::render` or small helpers taking the cached strings; the helpers above must not allocate.
- **Mirror**: `src/screens/select_art.cpp:337-408` (backdrop, top bar, hint bar), `select_art.cpp:441-465` (code-drawn skewed quads).
- **Validate**: `cmake --build build -j8`

### Task 3: `ResultsScreen` rewrite

- **File**: `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Delete the seven palette constants, `grade_color`, `results_difficulty_line` (declaration and definition) and the `render/bitmap_font.hpp` include. Keep `format_percent` / `format_grade` (via `gameplay/hud_renderer.hpp`, `gameplay/score_keeper.hpp`).
  - `enter`: keep everything up to and including the log line unchanged. Then build the cached strings from Pinned Semantics (badge via `difficulty_badge_for` from `screens/gameplay_screen.hpp`, title/artist via the TTF `song_display_*` overloads with `ctx.text`, digit strings, judgment counts, `grade_texture_`, `tier_text_`), and reset the fit cache (`fitted_scale_ = -1`). An invalid summary clears them all.
  - `update`: unchanged.
  - `render(ScreenContext& ctx, …)`: un-comment `ctx`; follow the pinned draw order. Refresh the three fitted bar strings only when `ctx.text->scale() != fitted_scale_`. Use `top_bar_layout` with ref-px widths (`measure / scale`). Use `ctx.theme->content_size(grade_texture_, 1.0f)` for the grade content when present, else `kGradeContentRef`.
  - Test accessors: `badge()`, `grade_texture()`, `tier_text()`, `dp_text()`, `dp_max_text()`, `hold_texts()`, `count_texts()` (const refs to the cached members).
  - Class comment: Cabinet v3 score screen (#95); data flow and submission unchanged from C7; the D3 reveal now drives the Cabinet parts (table in Pinned Semantics); null services draw nothing (headless).
- **Mirror**: `src/screens/select_screen.cpp:563-610` (render with null-guarded services).
- **Validate**: `cmake --build build -j8` (the app and `results_screen_test` may fail to compile until Task 5 removes `test_difficulty_line`; fix the screen TU's own errors first)

### Task 4: `ResultsAnimator` comments

- **File**: `src/screens/results_anim.hpp`
- **Action**: UPDATE
- **Implement**: Rewrite the class comment and the `grade_scale` / `record_*` comments to name the Cabinet parts (grade sprite on the medallion, record ribbon punch/pulse, white flash). Remove "bitmap text has no rotation" and "At completion it reproduces the exact C7 static layout" (say instead: at completion every part is at its settled value). No code change; `results_anim.cpp` is untouched.
- **Validate**: `cmake --build build -j8`

### Task 5: `results_screen_test`

- **File**: `tests/results_screen_test.cpp`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - CMake: `target_compile_definitions(results_screen_test PRIVATE BLAZE4K_SOURCE_DIR=… BLAZE4K_ASSETS_DIR=…)`, mirroring `select_screen_test`. Add the `loaded_theme()` / `loaded_text()` helpers from `select_screen_test.cpp:55-80` and a `real_manifest()` (`parse_theme_manifest`).
  - Remove `test_difficulty_line` and the `render/bitmap_font.hpp` include.
  - `test_screen_title_text`: `results_art::kScreenTitleText == "SCORE SCREEN"`, and the manifest has `title_score_screen` (`ThemeKind::Sprite`).
  - `test_grade_textures`: every `JudgmentConstants::compiled_defaults().grade_tiers[i].label` maps via `grade_texture_name` to a sprite entry in the real manifest with content 680x400 (pins `kGradeContentRef`). Exact cases: `quad_star`, `S+`, `A-`, `D`, `""`. `grade_tier_text`: the four star names, `"GRADE S+"`, `""`.
  - `test_digit_strings`: `digits_text(-48) == "0"`, `digits_text(1928) == "1928"`. After `enter` on a summary with `actual_dp = -30`, every cached digit string and `format_percent(p)` for p in {−2.4, 0, 0.9642, 1, 1.5} contain only characters with `digit_glyph_index(c) >= 0`.
  - `test_top_bar_layout`: the mock case (badge 59, title 118, artist 50 → `artist_x 1190`, `title_x 1056`, plate `{957, 16, 83, 32}`, matching the mock's mid-height plate x 957..1040), an empty artist (title ends at 1240), a 2000-px title (clamped so `plate.x >= kBarLeftLimit`), zero badge width (no plate), non-finite input (no NaN).
  - `test_stat_panels`: the three rects, `stat_text_x` on the mock label and value rows (≈ 67.8 and ≈ 62.0 for panel 0), `hold_columns({89, 21, 56}, {59, 9, 33})` → `{0, 123, 178}`, and a 4-digit hold count pushes the next column right (`max(label, value)` rule).
  - `test_judgment_rows`: `judgment_row_layout(0)` and `(5)` at the mock coordinates, the four fill cases above plus `total == 0`, and `judged_tap_total` excludes HitMine.
  - `test_ribbon_and_grade_rects`: `ribbon_rect(1) == kRecordRibbon`, `ribbon_rect(2)` keeps the centre; `grade_rect({340,200}, 1)` centred on (640, 286), `grade_rect(…, 2.4)` keeps the centre.
  - `test_enter_caches_badge`: the fixture chart (Hard 9) gives badge "HARD" / "9" with `theme::difficulty::kHard`; an Edit chart named "JBEAN" gives label "JBEAN"; the chart name does not change the high-score key (moved from the old test).
  - `test_render_cabinet_headless`: with `ctx.theme` / `ctx.text` set to the loaded services, for a clear, a NEW RECORD, a failed run, a named Edit chart (the existing 100-W UTF-8 name) and an invalid summary, call `manager.update(0.1, {})` 30 times and `manager.render` at 1280x720, 2560x1440, 3440x1440, 640x480, 320x240 and 0x0 each step (uninitialised `GlQuadRenderer`). No crash; `valid()` / `shows_record_finale()` as expected. Restore `text.set_window_size(1280, 720)` afterwards.
  - Keep every other existing test unchanged (submission, skip/continue, Back, re-enter, end-to-end).
- **Validate**: `cmake --build build -j8 && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R results_screen_test`

### Task 6: `results_anim_test`

- **File**: `tests/results_anim_test.cpp`
- **Action**: UPDATE
- **Implement**: add `test_reveal_order`: title starts first (`title_alpha(0.05) > 0`, `grade_alpha(0.05) == 0`), then the grade (`kGradeDelay < kPercentDelay < kStatsDelay < kRecordDelay`), the grade slam ends before the record ribbon starts (`kGradeDelay + kGradePopSeconds <= kRecordDelay`), and every element has settled by `kRevealSeconds` (`kRecordDelay + kRecordSeconds <= kRevealSeconds`, `kPercentDelay + kPercentCountSeconds <= kRevealSeconds`). Update the banner line to `"[results_anim_test] Running ResultsAnimator (score screen reveal) tests..."`. Keep every existing test.
- **Validate**: `... ctest ... -R results_anim_test` (sandboxed command)

### Task 7: README and full validation

- **File**: `README.md`
- **Action**: UPDATE line 51 (see Files to Change).
- **Validate**: the full Validation block below. 48/48 must pass, and the build adds no warnings.

---

## Validation

```bash
# Build (host, existing build dir)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8

# Lint: no linter configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j8 2>&1 | grep -iE "warning" | grep -E "results_art|results_screen|results_anim" || echo "no new warnings"

# Tests (MUST run sandboxed: the tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -j8
```

Expected: **48/48 pass** (baseline 48/48 on `374727a`).

Static checks:

```bash
# The bitmap layout is gone from the results screen
grep -nE "draw_text|bitmap_font|grade_color|results_difficulty_line|\"RESULTS\"" src/screens/results_screen.* tests/results_screen_test.cpp   # expect no hits
# Nothing in the score screen reads a clock (std::time stays only for the high-score timestamp in enter)
grep -nE "steady_clock|SDL_GetTicks|music_clock|time_seconds" src/screens/results_art.cpp src/screens/results_screen.cpp   # expect no hits
```

## End-to-End Verification

1. **Automated (agent-runnable):** `results_screen_test` drives a real `GameplayScreen` → `ResultsScreen` → `Select` run (`test_gameplay_to_results_end_to_end`, unchanged) and renders every state with the real manifest and fonts at six window sizes (`test_render_cabinet_headless`). Both must pass under the sandboxed ctest.
2. **Headless app smoke (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and no new error lines (the new TU is linked and the shell starts).
3. **Windowed visual check (owner; the agent must not launch the GUI, since it opens a window and plays audio):** start the game, play "In The Groove / Anubis" on Hard, and compare the score screen with `docs/cabinet-theme/reference/cabinet-v3-results.png` at 1280x720, then maximised (2560x1440 if available).
   - Top bar: "SCORE SCREEN" chrome title on the left; a red "HARD 8" slanted plate, the song title (italic) and the artist right-aligned on the right.
   - Left: three slanted panels. MAX COMBO and DANCE POINTS "n / max" (the "/ max" smaller and grey), then HOLDS OK (green), NG and MINES (red) with their numbers.
   - Centre: the medallion over the sunburst, the grade sprite slamming in from 2.4x and settling, the tier label in gold ("ONE STAR" etc.), the chrome percentage counting up underneath.
   - A first clear shows the NEW RECORD ribbon popping in with the white flash. A failed run (play with fail on and miss on purpose) shows the FAILED ribbon and the earned grade. Replaying for a lower score shows no ribbon.
   - Right: six coloured labels, bars proportional to the counts (a single Decent still shows a thin sliver), counts right-aligned.
   - Bottom: "ENTER SKIP" during the reveal, "ENTER CONTINUE" after it. Enter during the reveal skips; Enter again returns to the song wheel; Escape returns immediately.
   - Scanlines are visible over the frame. The chrome percentage does not shimmer once it stops counting.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A character outside `0-9 . % /` and space reaches `BitmapDigits` (a negative DP, a minus sign) | All digit strings come from `digits_text` (clamps at 0) or `format_percent` (clamps to [0, 1]); `test_digit_strings` checks every cached string with `digit_glyph_index` | In scope |
| Per-frame allocations from text fitting | Every string is built in `enter`. The three bar strings are re-fitted only when the text scale changes. The percent string (≤ 7 chars) fits the small-string buffer | In scope |
| A mid-screen atlas bake (hitch) from a scaled text style | Only sprites scale (grade, ribbon). Every text style used is in `kAllStyles`. The grade text fallback does not scale | In scope |
| Long titles, artists or Edit names overflow the bar into the title sprite | `top_bar_layout` caps the artist (240) and badge text (200) and gives the title only what is left down to x 360; `test_top_bar_layout` pins a 2000-px title | In scope |
| Very large counts (four-digit hold counts) overlap the next holds column | `hold_columns` uses `max(label, value)` width per column; pinned by a test | In scope |
| Theme or fonts missing (headless, broken install) | Every draw is null-guarded; a missing sprite falls back inside `ThemeTextures`; a missing title sprite draws "SCORE SCREEN" text; a missing grade sprite draws the grade text | In scope |
| Mock-measured constants drift from `theme.hpp` | Each measured constant names its source, and `static_assert`s tie the judgment-row and panel sums to `theme.hpp`. Deltas are listed in the measurement table | In scope |
| The judgment bars were meant to be slanted (`skew::kRows`, issue note) | One constant (`kJudgmentBarSkew`); Open Question 2 | In scope (flag) |
| Chrome percentage shimmer (no mipmaps on digit atlases; forward reference from plan 041) | Owner visual check; add mipmaps only if it shows | Out of scope (flag) |
| Music-clock contamination | No clock reads; the reveal advances only in `update` by `fixed_dt`; `render` only reads state. The static grep pins it | In scope |
| `results_screen.cpp` now includes `screens/gameplay_screen.hpp` for `difficulty_badge_for` | Same core library and layer (screens); avoids duplicating the badge rule. If the include is unwanted, moving `difficulty_badge_for` to `select_art` is a follow-up | Accept |

---

## Open Questions

None of these block the work; each has a recommended default that the plan already uses.

1. **What grade to show on a failed run.** There is no "F" picture in the theme pack. The current screen shows the grade you earned plus the word FAILED.
   **Recommendation:** keep that: show the earned grade on the medallion with the red FAILED ribbon underneath, as the issue's notes suggest. If you would rather see a big "F", that needs a new picture (a follow-up).
2. **Slanted or straight judgment bars.** The issue's notes say the bars should lean like the difficulty rows, but in the mock-up picture they are straight.
   **Recommendation:** straight, to match the picture. Switching to slanted is a one-line change if you prefer it.
3. **The gold words under the grade.** The mock-up only shows "ONE STAR". For the other star grades the plan uses "TWO STARS", "THREE STARS" and "FOUR STARS"; for letter grades it shows "GRADE S+", "GRADE A-" and so on.
   **Recommendation:** use those. Alternative: show nothing under letter grades, since the letter is already on the medallion.
4. **Negative dance points.** A run with lots of misses can end below zero dance points, and the chrome number font has no minus sign.
   **Recommendation:** show 0 (the percentage already shows 0.00% in that case).
5. **What "MINES" counts.** **Recommendation:** mines you stepped on (shown in red, like NG), which is what the current screen shows. Alternative: mines you avoided, out of the total.
6. **The hint while the score is still animating.** The mock-up shows "ENTER CONTINUE". During the 2.4-second reveal the first Enter only skips the animation.
   **Recommendation:** show "ENTER SKIP" during the reveal and "ENTER CONTINUE" once it settles, so the hint always says what Enter does.

---

## Acceptance Criteria

- [ ] Draws `bg_results`, `bar_top` + `title_score_screen`, and the badge ("HARD 8"), song title and artist right-aligned in the bar
- [ ] Left stat panels (`stat_panel` + label + `digits_white`): MAX COMBO, DANCE POINTS "n / max", and HOLDS OK / NG / MINES
- [ ] Centre: `medallion` + `grade_<tier>` (`quad_star` → `grade_quad_star`, `S+` → `grade_S_plus`) + tier label ("ONE STAR"), the `digits_chrome` percentage counting up as now, and `record_ribbon` on a new record or `failed_ribbon` on fail
- [ ] Right: six judgment rows (FANTASTIC…MISS) with labels in judgment colours, a `draw_quad_points` track (`kBarTrack`) + fill proportional to count / total judged taps, and right-aligned counts. The hint bar shows "ENTER CONTINUE" (settled)
- [ ] Reveal: title fades in, the grade slams 2.4x → 1x on the medallion, stats fade, the ribbon pops, and the white flash on NEW RECORD is kept
- [ ] Only `0-9 . % /` and space reach `BitmapDigits`
- [ ] `results_screen_test` and `results_anim_test` are updated ("SCORE SCREEN") and pass; 48/48 tests pass under the sandboxed ctest command
- [ ] Submission, best-score rule, skip/continue/back input and `ResultsSummary` unchanged (existing tests green)
- [ ] No new compiler warnings; no clock reads; no per-frame heap allocations added
- [ ] `.agents/stories/todo-stories.md` untouched
