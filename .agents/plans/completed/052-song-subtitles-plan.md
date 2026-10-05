# Plan: Show song subtitles in song select and results

## Summary

The parser already fills `SongMetadata::subtitle` / `subtitle_translit` (`src/chart/simfile_parser.cpp:96-103`), but no screen draws them. So the three ITG "Disconnected" songs (`-Hyper-`, `-Mobius-`, `-Hardkore-`) all read "Disconnected" on the wheel. This plan draws the subtitle **on the same line, right after the title, in a smaller secondary style that shares the title's baseline**. It does that in three places: the wheel rows (plain and selected), the song info panel under the banner, and the results top bar.

The subtitle is not put on a second line. The rows have no room for one: a plain wheel row is 62px tall with a 28px title, and the info panel has 56px between the title top (270) and the artist line (326). The issue's technical note allows the one-line form for exactly this case.

The code changes are:

- **New helper.** `song_display_subtitle(...)` (bitmap and TrueType overloads) joins `song_display_title` / `song_display_artist` in `src/screens/song_display_text.{hpp,cpp}`. It uses the same `select_display_text` coverage rule, applied to SUBTITLE / SUBTITLETRANSLIT on their own.
- **Shared fit.** One pure, unit-agnostic function, `fit_title_subtitle(title_w, subtitle_w, gap, budget)`, splits a measured width budget between the two texts. When both don't fit, the subtitle keeps at least 40% of the budget, because it is what tells songs apart.
- **Draw sites.** `select_art::draw_song_info`, `select_art::draw_wheel` (via a new `WheelRowView::subtitle`) and the results bar (`results_art::top_bar_layout` + `ResultsScreen::refit_bar_text`) use the fit. Each one truncates by measured width (`TextRenderer::truncate`).
- **No subtitle, no change.** When the subtitle is empty, every site keeps its current code path verbatim, so those songs look exactly as they do today.
- **New styles.** The four new text styles reuse existing (font, size) pairs, so no new atlas is baked.

This is display-only. Identity (high-score keys, `find_song`) and log lines keep the raw native title. Nothing touches timing, input or judgment.

## User Story

As a player browsing the song wheel
I want to see each song's subtitle (e.g. "Disconnected -Hyper-" vs "Disconnected -Mobius-")
So that songs that share a title are visibly different on select and on the results screen

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/song_display_text.{hpp,cpp}`, `src/render/theme.hpp`, `src/screens/select_art.{hpp,cpp}`, `src/screens/select_screen.cpp`, `src/screens/results_art.{hpp,cpp}`, `src/screens/results_screen.{hpp,cpp}`, tests (`bitmap_font_test`, `select_art_test`, `results_screen_test`, `ttf_font_test`, `setup_art_test`), `TODO.md` |
| GitHub Issue | #110 ([TODO-27] Show song subtitles in song select and results) |
| Branch (suggested) | `feature/052-song-subtitles` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and builds cleanly with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **49/49 pass** | Run on `main` @ `7bcb166` with the sandboxed ctest command in Validation (0.59 s). The count stays **49**, because no test executable is added |
| Sandbox requirement | — | Some tests open the real sound device. **Always** run ctest, and any test or app binary, inside the `bwrap` prefix in Validation |
| Subtitle data | `src/chart/song_metadata.hpp:9,13`, `src/chart/simfile_parser.cpp:96-103` | `subtitle` / `subtitle_translit` are already parsed. MSD values are trimmed (`src/chart/msd_file.cpp:11,121-161`), so a whitespace-only subtitle arrives as empty |
| Reference pack | `songs/` (local, owner-provided) | 23 `.sm` files have a non-empty `#SUBTITLE`. The subtitles carry their own brackets: `-Hyper-`, `-Mobius-`, `-Hardkore-` (all `#TITLE:Disconnected`), `(Folk Mix)`, `~Speedy Mix~`. The longest is `(Two Gees Radio Edit)` (21 chars). No `#SUBTITLETRANSLIT` is set in the pack |
| Draw observability | `tests/select_art_test.cpp:887-986` | Draws go to an uninitialised `GlQuadRenderer` (no-ops), so tests pin **pure layout** helpers plus smoke renders. Results draws work the same way (`tests/results_screen_test.cpp:665+`) |
| Style pins | `tests/ttf_font_test.cpp:810,816`, `tests/select_art_test.cpp:865`, `tests/setup_art_test.cpp:489` | `kAllStyles.size() == 26` is pinned in three tests, and the unique (font, size) pairs `== 13` in `ttf_font_test`. This plan makes it 30 styles with **still 13 pairs** |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #110

`grep -rn "#110\|TODO-27" src tests docs .agents/plans` finds nothing. The only hit is `TODO.md:43` (the source line, already tagged `(#110)`), which Task 9 ticks.

---

## Value Provenance (StepMania 5 semantics)

Source: `stepmania/stepmania` @ `825467bcd81c812b33ad684dc04dd151b2d5dec3` (branch `5_1-new`).

| Behaviour | SM5 source | Blaze 4k decision |
|---|---|---|
| Full title = main title + `" "` + subtitle, **no added brackets**, subtitle omitted when empty | `src/Song.cpp:1886-1893` (`GetDisplayFullTitle`), `:1895-1902` (`GetTranslitFullTitle`) | Draw `title`, a gap, then `subtitle` as written. Never wrap it in `( )`, because the simfile values already carry `-…-`, `(…)` or `~…~` |
| Subtitle native/translit choice is made per field, independently of the title | `src/Song.cpp:1869-1873` (`GetDisplaySubTitle`), `src/Song.h:219-222` (`GetTranslitSubTitle`: the translit if non-empty, else native) | `song_display_subtitle` = `select_display_text(subtitle, subtitle_translit, covers(subtitle))`. This is the same rule as title and artist (AC 4) |
| Wheel/banner text shows the subtitle as its own text actor next to the title | `src/TextBanner.cpp:25-27,59-67,74-83` (`m_textSubTitle`), `src/MusicWheelItem.cpp:61-66` (song rows use a `TextBanner`) | A separate, smaller style. Placement is theme-defined in SM5, and here it is inline on the title's baseline (see Open Question 1) |

---

## Pinned Semantics (the contract the tests pin)

### `song_display_subtitle(const SongMetadata&)` and `song_display_subtitle(const SongMetadata&, const TextRenderer*, theme::Font)` (screens/song_display_text)
- Bitmap overload: `select_display_text(m.subtitle, m.subtitle_translit, font_covers_text(m.subtitle))`.
- TrueType overload: the same with `text->covers_text(m.subtitle, font)`. A null `text` falls back to the bitmap overload.
- Both return a reference that aliases `metadata` (no copy). Empty native + empty translit → the empty `m.subtitle`.

### `fit_title_subtitle(float title_w, float subtitle_w, float gap, float budget) -> TitleSubtitleFit{title_max_w, subtitle_max_w}` (screens/song_display_text)
The widths can be in any one unit (window px or reference px). First, every input is sanitised: a non-finite or negative value → 0.
1. `budget == 0` → `{0, 0}`.
2. `subtitle_w == 0` → `{min(title_w, budget), 0}` (no subtitle: the title alone, as today).
3. `budget <= gap` → `{min(title_w, budget), 0}` (no room for a gap: title only).
4. `title_w + gap + subtitle_w <= budget` → `{title_w, subtitle_w}` (both whole).
5. Otherwise, with `avail = budget - gap`:
   - `subtitle_max_w = min(subtitle_w, max(avail - title_w, avail * kSubtitleMinShare))`
   - `title_max_w = min(title_w, avail - subtitle_max_w)`
   - `kSubtitleMinShare = 0.4f`. A short title stays whole and the subtitle takes the rest. A long title truncates so the subtitle keeps ≥ 40% (or its whole width if that is smaller).
- Invariants (tested): both outputs are finite and ≥ 0, `title_max_w <= title_w`, `subtitle_max_w <= subtitle_w`, and when `subtitle_max_w > 0`, `title_max_w + gap + subtitle_max_w <= budget + 1e-3`.

### Draw placement (select and results)
- The subtitle x is the title x + **measured width of the fitted (possibly truncated) title** + gap. This keeps the gap tight after truncation. The subtitle text is `truncate(subtitle, sub_style, subtitle_max_w)`.
- Baseline: the subtitle top is the title top + `ascent(title_style)` − `ascent(sub_style)`. Results already draws both on `bar.baseline` via `baseline_top` (`src/screens/results_screen.cpp:34-37`).
- **No subtitle → the existing code path is unchanged.** Every site branches on `subtitle.empty()` and, when it is empty, makes exactly today's `truncate(title, style, budget)` call.
- **Pack rows never get a subtitle.**

### `results_art::top_bar_layout(badge_w, title_w, artist_w, subtitle_w = 0.0f)`
- With `subtitle_w <= 0` (or non-finite), every field is identical to today. The existing `test_top_bar_layout` stays green unchanged.
- With a subtitle, the title slot is sized for `combined = title_w + kBarSubtitleGap + subtitle_w`. Then `fit_title_subtitle(title_w, subtitle_w, kBarSubtitleGap, title_slot_w)` gives `title_max_w` / `subtitle_max_w`, and `subtitle_x = title_x + title_max_w + kBarSubtitleGap` (nominal: the screen re-derives x from the measured fitted title). The plate sits left of `title_x` exactly as today. The new fields are `subtitle_x` and `subtitle_max_w`, both 0 without a subtitle.

### New text styles (`theme::text`, all on existing (font, size) atlases)
| Style | Value | Shares atlas with |
|---|---|---|
| `kSongSubtitle` (info panel) | `{SairaBold, 28, 0, true, kIce}` | `kWheelRow` (SairaBold 28) |
| `kWheelSubtitle` (plain row) | `{SairaBold, 20, 0, false, kIce}` | `kDiffBest` (SairaBold 20) |
| `kWheelSelectedSubtitle` (gold bar) | `{SairaBold, 28, 0, true, kSelectedInk}` | `kWheelRow` (SairaBold 28) |
| `kBarSongSubtitle` (results bar) | `{SairaBold, 18, 0, true, kIce}` | `kBarArtist` (SairaBold 18) |

`kAllStyles` goes from 26 to **30**, and the unique pairs stay **13**. Italic follows the title style the subtitle sits next to, and `kIce` is already documented as the "subtitle" colour (`src/render/theme.hpp:36`).

### Gaps (reference px)
- `select_art::kSubtitleGap = 10.0f` (wheel and info panel).
- `results_art::kBarSubtitleGap = 8.0f`.

### Unchanged on purpose
- `make_chart_key` (`src/data/high_scores.cpp:129-157`) is based on the filename, the title and the notes, and stays unchanged (the issue says so). The Disconnected songs already get distinct keys from the filename and the note hash.
- `SongLibrary::find_song` (title-only lookup, `src/chart/song_library.cpp:90-102`) is unchanged. See Risks.
- The log lines (`gameplay_screen.cpp:61`, `results_screen.cpp:71`) keep the raw native title.
- Gameplay HUD: no title is drawn there today, so nothing changes.

---

## Patterns to Follow

### Draw-only display helper pair (extend this file)
```cpp
// SOURCE: src/screens/song_display_text.cpp:16-42
const std::string& song_display_title(const SongMetadata& metadata) {
    return select_display_text(metadata.title, metadata.title_translit,
                               font_covers_text(metadata.title));
}
...
const std::string& song_display_title(const SongMetadata& metadata, const TextRenderer* text,
                                      theme::Font font) {
    if (text == nullptr) {
        return song_display_title(metadata);
    }
    return select_display_text(metadata.title, metadata.title_translit,
                               text->covers_text(metadata.title, font));
}
```

### Measured-width truncation at a draw site
```cpp
// SOURCE: src/screens/select_art.cpp:470-484
text.draw(renderer, text.truncate(title, title_style, L.px(kInfoWidth)), L.x(layout::kInfoX),
          L.y(layout::kSongTitleTop), title_style, TextAlign::Left);
const float bpm_w = text.measure(bpm, bpm_style);
const float artist_budget = std::max(0.0f, L.px(kInfoWidth) - bpm_w - L.px(kArtistBpmGap));
```

### Wheel text pass (grouped by style; label chosen by the screen)
```cpp
// SOURCE: src/screens/select_art.cpp:615-634
const Rect r = row_rect(view);
const float text_x = r.x + text_dx;
const float budget = std::max(0.0f, L.px(kWheelTextRight - text_x));
text->draw(renderer, text->truncate(view.label, style, budget), L.x(text_x),
           centred_top(*text, L, r.y, r.h, style), style, TextAlign::Left);
```

### Pure layout with non-finite sanitising (results bar)
```cpp
// SOURCE: src/screens/results_art.cpp:55-82
TopBarLayout top_bar_layout(float badge_text_w, float title_w, float artist_w) {
    badge_text_w = finite_or_zero(badge_text_w);
    title_w = finite_or_zero(title_w);
    ...
```

### Fit cache refreshed on scale change (results)
```cpp
// SOURCE: src/screens/results_screen.cpp:173-182
bar_layout_ = art::top_bar_layout(badge_w,
                                  ref_measure(*text, title_, theme::text::kBarSongTitle),
                                  ref_measure(*text, artist_, theme::text::kBarArtist));
...
fitted_title_ =
    text->truncate(title_, theme::text::kBarSongTitle, bar_layout_.title_max_w * scale);
```

### Error handling
Untrusted simfile text never throws. The display helpers only select between strings. `truncate` / `measure` are already safe on malformed UTF-8 (`tests/bitmap_font_test.cpp:164`, `tests/ttf_font_test.cpp:689` fuzz). Pure layout sanitises non-finite input to 0 (`finite_or_zero`, `results_art.cpp`).

### Tests (plain executable, `TEST_CHECK` abort macro, section prints, explicit call list in `main`)
```cpp
// SOURCE: tests/select_art_test.cpp:372-410
void test_display_text_coverage() {
    const blaze4k::TextRenderer& text = loaded_text();
    const theme::Font font = theme::text::kSongTitle.font;
    ...
    TEST_CHECK(&blaze4k::song_display_title(cjk, &text, font) == &cjk.title_translit);
    ...
    std::cout << "  - TrueType-coverage display text ok.\n";
}
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/song_display_text.hpp` | UPDATE | Declare `song_display_subtitle` (2 overloads), `TitleSubtitleFit`, `kSubtitleMinShare`, `fit_title_subtitle` |
| `src/screens/song_display_text.cpp` | UPDATE | Implement them |
| `src/render/theme.hpp` | UPDATE | 4 subtitle `TextStyle`s, appended to `kAllStyles` (26 → 30) |
| `src/screens/select_art.hpp` | UPDATE | `kSubtitleGap`. `WheelRowView::subtitle`. The `draw_song_info` signature gains `subtitle` |
| `src/screens/select_art.cpp` | UPDATE | Inline subtitle in `draw_song_info` and in `draw_wheel`'s Song/Selected passes |
| `src/screens/select_screen.cpp` | UPDATE | Pass `song_display_subtitle(...)` to the info panel and the wheel row views |
| `src/screens/results_art.hpp` | UPDATE | `kBarSubtitleGap`. `TopBarLayout::subtitle_x/subtitle_max_w`. `subtitle_w = 0.0f` parameter |
| `src/screens/results_art.cpp` | UPDATE | Subtitle-aware `top_bar_layout` |
| `src/screens/results_screen.hpp` | UPDATE | `subtitle_`, `fitted_subtitle_`, `subtitle_x_`. `display_subtitle()` / `fitted_subtitle()` test accessors |
| `src/screens/results_screen.cpp` | UPDATE | Build, clear, refit and draw the subtitle |
| `tests/bitmap_font_test.cpp` | UPDATE | Bitmap-rule subtitle choice + `fit_title_subtitle` table |
| `tests/select_art_test.cpp` | UPDATE | TrueType subtitle coverage, real-font Disconnected fit, prebaked styles, smoke render with subtitles, `26 → 30` |
| `tests/results_screen_test.cpp` | UPDATE | Subtitle-aware `top_bar_layout` cases, `enter()` caches the subtitle, headless render with a subtitle |
| `tests/ttf_font_test.cpp` | UPDATE | `kAllStyles` 26 → 30 (pairs stay 13) |
| `tests/setup_art_test.cpp` | UPDATE | `kAllStyles` 26 → 30 |
| `TODO.md` | UPDATE | Tick line 43 |

No file is created, and no CMake change is needed (`song_display_text.cpp` is already in `CMakeLists.txt:139`).

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Subtitle display choice + title/subtitle fit

- **File**: `src/screens/song_display_text.hpp`, `src/screens/song_display_text.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add both `song_display_subtitle` overloads next to `song_display_artist`, with the same doc-comment style. Note "SUBTITLE / SUBTITLETRANSLIT (SM5 Song::GetDisplaySubTitle), chosen independently of the title".
  - Add `struct TitleSubtitleFit { float title_max_w = 0.0f; float subtitle_max_w = 0.0f; };`, `inline constexpr float kSubtitleMinShare = 0.4f;` and `[[nodiscard]] TitleSubtitleFit fit_title_subtitle(float title_w, float subtitle_w, float gap, float budget);`. Implement them exactly per Pinned Semantics, with a local `finite_nonneg` (`std::isfinite(v) && v > 0 ? v : 0`). Include `<cmath>` and `<algorithm>`.
  - Doc-comment the "Title Subtitle on one line, no added brackets (SM5 Song::GetDisplayFullTitle)" rule.
- **Mirror**: `src/screens/song_display_text.cpp:16-42`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 2: Subtitle text styles

- **File**: `src/render/theme.hpp`
- **Action**: UPDATE
- **Implement**: Add `kSongSubtitle` and `kWheelSubtitle` + `kWheelSelectedSubtitle` under "Song select", and `kBarSongSubtitle` under "Score screen", with the values in Pinned Semantics. Append all four to `kAllStyles` and keep the "Keep it in sync" comment. Update the three `== 26` pins to `30`: `tests/ttf_font_test.cpp:810` + its print at `:816` ("30 styles, 13 unique"), `tests/select_art_test.cpp:865` and `tests/setup_art_test.cpp:489`. The `pairs.size() == 13` check must stay 13. If it doesn't, a style picked a new size, so fix the style, not the test.
- **Mirror**: `src/render/theme.hpp:119-159`
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R "ttf_font_test|select_art_test|setup_art_test"`

### Task 3: Info panel subtitle

- **File**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `inline constexpr float kSubtitleGap = 10.0f;` near `kInfoWidth`.
  - Change the signature to `draw_song_info(TextRenderer&, GlQuadRenderer&, const LayoutScale&, std::string_view title, std::string_view subtitle, std::string_view artist, std::string_view bpm)` and update its doc comment: "Title + subtitle on one line, sharing the 560 budget via fit_title_subtitle".
  - Body: if `subtitle.empty()`, keep today's title draw verbatim. Otherwise:
    - `budget = L.px(kInfoWidth)`, `gap = L.px(kSubtitleGap)`.
    - `fit = fit_title_subtitle(text.measure(title, kSongTitle), text.measure(subtitle, kSongSubtitle), gap, budget)`.
    - `t = text.truncate(title, kSongTitle, fit.title_max_w)`, then draw it at `(kInfoX, kSongTitleTop)`.
    - `s = text.truncate(subtitle, kSongSubtitle, fit.subtitle_max_w)`. Skip it if `s` is empty. Otherwise draw it at `x = L.x(kInfoX) + text.measure(t, kSongTitle) + gap` and `y = L.y(kSongTitleTop) + text.ascent(kSongTitle) - text.ascent(kSongSubtitle)`.
  - Add a small anonymous-namespace helper `baseline_aligned_top(text, title_top_px, title_style, sub_style)` next to `centred_top` (`select_art.cpp:61`) and reuse it in Task 4.
  - The artist/BPM line is unchanged.
- **Mirror**: `src/screens/select_art.cpp:470-484`
- **Validate**: `cmake --build build -j$(nproc)` (this fails until Task 5 updates the caller; do Tasks 3-5 as one compile unit)

### Task 4: Wheel row subtitle

- **File**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `std::string_view subtitle;` to `WheelRowView` (doc: "Song/Selected rows only; empty = title alone, exactly as before").
  - In `draw_wheel`'s `draw_text_pass`, for each drawn row compute `text_x`, `top` and `budget` as today. If `view.subtitle.empty()` or `art == WheelArt::Pack`, keep the current single `text->draw(...)` call verbatim. Otherwise:
    - Choose `sub_style = art == Selected ? kWheelSelectedSubtitle : kWheelSubtitle`.
    - Fit with `gap = L.px(kSubtitleGap)` and draw the truncated title at the same x/top.
    - Draw the truncated subtitle at `L.x(text_x) + measure(fitted_title) + gap`, with top `baseline_aligned_top(...)`.
  - Keep "text grouped by style": draw all titles of the pass first, then the subtitles of that pass in a second loop over the same rows. Recompute the fit in that loop, or keep a tiny fixed-size array of `{fitted_title_w, fit}` (≤ `kWheelVisibleRows + kWheelMaxSlideRows` entries). The selected subtitle pass stays after the selected bar art, so it is drawn on top of the gold bar.
  - Slide/cull behaviour is unchanged: the subtitle uses the same `row_rect(view)` and `drawn(view)`.
- **Mirror**: `src/screens/select_art.cpp:603-655`
- **Validate**: as Task 3

### Task 5: Select screen wiring

- **File**: `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Info panel (`:594-600`): `const std::string& subtitle = song_display_subtitle(song->metadata, text, theme::text::kSongSubtitle.font);` and pass it to `draw_song_info`.
  - Wheel (`:650-656`): next to `view.label`, set `view.subtitle = song_display_subtitle(row_song->metadata, text, selected ? theme::text::kWheelSelectedSubtitle.font : theme::text::kWheelSubtitle.font);`.
  - Pack rows leave `subtitle` empty. The views are value-initialised each frame (`views{}`), so a slot that was a song row last frame cannot keep a stale subtitle.
- **Mirror**: `src/screens/select_screen.cpp:594-600, 650-656`
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R "select"`

### Task 6: Results bar layout with a subtitle

- **File**: `src/screens/results_art.hpp`, `src/screens/results_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `inline constexpr float kBarSubtitleGap = 8.0f;` next to `kBarGap`.
  - Add `float subtitle_x = 0.0f; float subtitle_max_w = 0.0f;` to `TopBarLayout`.
  - Extend the signature to `top_bar_layout(float badge_text_w, float title_w, float artist_w, float subtitle_w = 0.0f)`. Sanitise `subtitle_w`. When it is > 0 and `title_w` > 0:
    - Size the title slot exactly as today, but with `combined = title_w + kBarSubtitleGap + subtitle_w` in place of `title_w`, so the slot's `title_max_w` is `min(combined, title_max)`.
    - Then `fit = fit_title_subtitle(title_w, subtitle_w, kBarSubtitleGap, slot_w)`. Set `out.title_max_w = fit.title_max_w`, `out.subtitle_max_w = fit.subtitle_max_w`, keep `out.title_x = title_right - slot_w`, and set `out.subtitle_x = out.title_x + fit.title_max_w + kBarSubtitleGap` (0 / unused when `subtitle_max_w == 0`).
    - The plate is positioned left of `title_x` as today.
  - Edge case: `title_w == 0` with a subtitle can't happen in practice ("UNKNOWN" is used when there is no song). Treat the subtitle as absent then.
  - Update the header doc comment.
- **Mirror**: `src/screens/results_art.cpp:55-82`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 7: Results screen caches, fits and draws the subtitle

- **File**: `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Members: `std::string subtitle_;`, `std::string fitted_subtitle_;`, `float subtitle_x_ = 0.0f;` (reference px). Accessors: `display_subtitle()` and `fitted_subtitle()` next to `display_title()`.
  - `clear_cached_text()` (`:100-104`): clear `subtitle_` / `fitted_subtitle_` and zero `subtitle_x_`.
  - `build_cached_text()` (`:108-115`): `subtitle_ = song ? song_display_subtitle(song->metadata, ctx.text, kBarSongSubtitle.font) : ""`.
  - `refit_bar_text()`:
    - In the `text == nullptr` branch, clear `fitted_subtitle_`.
    - Otherwise, measure `sub_w = subtitle_.empty() ? 0 : ref_measure(*text, subtitle_, kBarSongSubtitle)` and pass it as the 4th argument of `top_bar_layout`.
    - `fitted_title_` stays as today (it now gets the fit's `title_max_w`).
    - `fitted_subtitle_ = bar_layout_.subtitle_max_w > 0 ? text->truncate(subtitle_, kBarSongSubtitle, bar_layout_.subtitle_max_w * scale) : ""`.
    - `subtitle_x_ = bar_layout_.title_x + ref_measure(*text, fitted_title_, kBarSongTitle) + art::kBarSubtitleGap`.
  - `render()` (`:353-360`): after the title, if `!fitted_subtitle_.empty()`, draw it at `L.x(subtitle_x_)`, `baseline_top(*text, L, bar.baseline, sub_style)`, `TextAlign::Left`, with `sub_style = with_color(kBarSongSubtitle, with_alpha(kBarSongSubtitle.color, title_alpha))` so it fades in with the title.
  - Update the `refit_bar_text` comment ("badge / title / subtitle / artist").
- **Mirror**: `src/screens/results_screen.cpp:100-115, 138-182, 345-366`
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -R results`

### Task 8: Tests

- **Files**: `tests/bitmap_font_test.cpp`, `tests/select_art_test.cpp`, `tests/results_screen_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `bitmap_font_test.cpp`: new `test_subtitle_display_and_fit()`, added to `main`'s call list.
    - Bitmap rule: `subtitle = "\xE2\x98\x86Mix"` with translit `"Star Mix"` → translit. `"-Hyper-"` with translit `"X"` → native (covered native wins). An uncoverable native with an empty translit → native. Both empty → empty. Aliasing: `&song_display_subtitle(m) == &m.subtitle` / `&m.subtitle_translit`.
    - The choice is independent of the title: an ASCII title with a non-ASCII subtitle picks title native + subtitle translit.
    - `fit_title_subtitle` table: no subtitle `(100,0,8,400)` → `{100,0}`. No subtitle, long title `(500,0,8,400)` → `{400,0}`. Both fit `(100,50,8,400)` → `{100,50}`. Short title + long subtitle `(100,500,8,400)` → `{100,292}`. Long title + short subtitle `(600,100,8,400)` → `{292,100}`. Both long `(600,500,8,400)` → subtitle `392*0.4`, title `392*0.6`. `budget <= gap` `(100,50,8,6)` → `{6,0}`. Zero budget → `{0,0}`. NaN/inf/negative inputs → finite, ≥ 0.
    - Loop the invariants (sum ≤ budget + 1e-3 when there is a subtitle, each ≤ its input) over a small grid of values.
  - `select_art_test.cpp`:
    - Extend `test_display_text_coverage` (`:372-410`) with subtitle cases through the real TTF: Latin-1 `"Caf\xC3\xA9 Mix"` native chosen by TTF but translit by bitmap; CJK subtitle with translit → translit; null renderer → bitmap rule.
    - New `test_disconnected_subtitles_fit()`: with `loaded_text()` at 1280x720, for each of `-Hyper-`, `-Mobius-`, `-Hardkore-` and `kWheelRow`/`kWheelSubtitle`, `kWheelSelected`/`kWheelSelectedSubtitle` and `kSongTitle`/`kSongSubtitle` at their real budgets (wheel: `kWheelTextRight - (wheel_row_rect(slot, sel).x + text_dx)` for the worst indent; info: `kInfoWidth`), `fit_title_subtitle(measure("Disconnected"), measure(sub), gap, budget)` returns both whole. This is AC 1 "visibly different".
    - Also measure `"A Song With Many Charts And A Very Long Title That Will Not Fit"` + `"(Two Gees Radio Edit)"`. The fit keeps a subtitle ≥ 40% of the budget, `truncate` of the title ends in `...`, and the title's measured width + gap + the truncated subtitle's measured width ≤ the budget (no overlap, AC 5).
    - Extend `test_options_styles_prebaked` (`:851-866`) to check that the 4 new styles are prebaked.
    - In `test_render_smoke` (`:887-986`), add a `#SUBTITLE` to `make_sm` via an optional parameter. Give the long-title song a long subtitle and add two "Disconnected" songs with different subtitles to Pack A. Update `song_count()` / `wheel_row_count()` expectations (11 → 13 songs, 13 → 15 rows), and update the comment. Every render must still run at every size, with and without services.
  - `results_screen_test.cpp`:
    - `test_top_bar_layout` (`:469-513`): keep every existing check unchanged. They pin "no subtitle = identical".
      - Add `top_bar_layout(59,118,50,0)` == the 3-arg result, field by field, with `subtitle_max_w == 0`.
      - Add `top_bar_layout(59,118,50,60)`: `title_max_w == 118`, `subtitle_max_w == 60`, `subtitle_x == title_x + 118 + 8`, `subtitle_x + 60 == artist_x - kBarGap` (right edge of the group), plate right edge `== title_x - kBarGap`.
      - Long title + subtitle `(59,2000,50,300)`: plate ≥ `kBarLeftLimit`, `subtitle_max_w >= 0.4f * (slot - 8) - 1e-3` and `subtitle_x + subtitle_max_w <= artist_x - kBarGap + 1e-3`.
      - Non-finite subtitle → finite fields.
    - `test_enter_caches_badge` (`:596+`): the default fixture has `display_subtitle().empty()`. A fixture with `song.metadata.subtitle = "-Hyper-"` gives `display_subtitle() == "-Hyper-"` and `display_title() == "Blaze Anthem"` (unchanged). The bare fixture (no song) has an empty subtitle, and the invalid summary clears it.
    - In `test_render_cabinet_headless` (`:665+`), run one render pass with a subtitled song and check `!fitted_subtitle().empty()` with text services (and empty without).
- **Mirror**: `tests/select_art_test.cpp:372-410, 851-986`, `tests/bitmap_font_test.cpp:129-162`, `tests/results_screen_test.cpp:469-513, 596-631`
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`

### Task 9: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:43`: `- [ ]` → `- [x]`. Change only that line. Do **not** touch `.agents/stories/todo-stories.md`.
- **Validate**: `git diff --stat -- TODO.md .agents/stories/todo-stories.md` (`TODO.md` shows 1 line changed. `todo-stories.md` shows only its pre-existing unstaged diff)

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j$(nproc)

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
touch src/screens/song_display_text.cpp src/screens/select_art.cpp src/screens/select_screen.cpp src/screens/results_art.cpp src/screens/results_screen.cpp
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning" | grep -E "song_display_text|select_art|select_screen|results_art|results_screen|theme\.hpp" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **49/49 pass** (baseline 49/49 on `7bcb166`; no executable is added).

Static checks:

```bash
# Identity and logs keep the raw title (no subtitle leaked into keys/lookups)
git diff -- src/data src/chart   # expect empty
# Timing / judgment / input untouched
git diff --stat -- src/timing src/audio src/input src/gameplay assets   # expect empty
# No brackets added around the subtitle
git diff -- src | grep -nE '"\("|"\)"|" \(" ' || echo "no added brackets"
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-stories   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable):**
   - `bitmap_font_test` pins the subtitle display choice and the fit table.
   - `select_art_test` pins TTF coverage, the real-font "Disconnected -Hyper-/-Mobius-/-Hardkore-" fit at every wheel/info budget, overflow truncation without overlap, and smoke renders of a library that contains subtitled same-title songs (with and without services, at 6 window sizes).
   - `results_screen_test` pins "no subtitle → identical bar layout", the subtitle bar layout, and the cached/fitted subtitle strings.
2. **Headless app smoke against the reference pack (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --songs ./songs --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and no new error lines. This exercises the select render path over the real 23 subtitled songs. Check the flag spelling with `--help` first.
3. **Windowed check (owner; the agent must not launch the GUI, since it opens a window and plays audio):**
   - On Select, scroll to the In The Groove pack. The three "Disconnected" rows read `Disconnected -Hyper-`, `Disconnected -Mobius-`, `Disconnected -Hardkore-`, with the subtitle smaller and in ice blue on plain rows, and in dark ink on the gold selected bar. They share the title's baseline.
   - Select one. The info panel under the banner shows `Disconnected` (big white italic) followed by `-Hyper-` (smaller ice italic), and the artist/BPM line is unchanged.
   - Check `Da Roots (Folk Mix)` and `Summer ~Speedy Mix~`. The brackets come from the simfile, never doubled.
   - A song with no subtitle looks exactly as before.
   - Play one Disconnected song to the end. The results top bar reads `[badge] Disconnected -Hyper-   Inspector K`, and the subtitle fades in with the title.
   - Resize the window to 640x480 and to 21:9. Long title + subtitle combos truncate with `...` and never run into the BPM, the wheel edge or the artist.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Subtitle overlaps neighbouring elements | Every site truncates both texts by measured width inside the existing budget (560 info, `kWheelTextRight` wheel, the results title slot). The fit invariant (sum ≤ budget) and the real-font overflow test pin it | In scope |
| Songs without a subtitle change appearance | Every site branches on `subtitle.empty()` and keeps today's call verbatim. `top_bar_layout` with `subtitle_w = 0` is field-identical (tested) | In scope |
| A long title pushes the subtitle off, so same-title songs look identical again | `kSubtitleMinShare = 0.4` reserves ≥ 40% of the budget for the subtitle. Same-title songs differ in that visible part | In scope |
| Baseline mismatch between 40px italic title and 28px subtitle (or the Saira ascent differs per size) | Align on `ascent()` (the same baseline the TTF and bitmap fallback use, `ttf_font.cpp:944-947`). Owner visual check in E2E step 3 | In scope |
| Italic shear makes the last title glyph's ink pass its measured width | The 10px / 8px gap absorbs it (shear 0.25 × the cap height above the baseline). Measure-based truncation is unchanged from today | In scope |
| A new style triggers a lazy atlas bake mid-screen | All 4 new styles reuse existing (font, size) pairs. The pairs count stays 13 (pinned in `ttf_font_test`) and they are checked as prebaked in `select_art_test` | In scope |
| Adding songs to the smoke fixture shifts counts in `test_render_smoke` | Update the expected `song_count` / `wheel_row_count` with the new fixture, in the same edit | In scope |
| `SongLibrary::find_song(pack, title)` is ambiguous for same-title songs (returns the first "Disconnected") | Not used by select/results display. Flag only. A future fix would match title + subtitle (SM5 `Song::Matches` uses `GetTranslitFullTitle`, `Song.cpp:1964`) | Out of scope (flag) |
| Log lines (`[GameplayScreen] started 'Disconnected'`) don't distinguish the songs | Logs keep the raw title by the file's documented rule. Could append the subtitle later | Out of scope (flag) |
| Timing path touched (principle 1) | Only presentation code changes. The `git diff --stat` check on timing/audio/input/gameplay is in Validation | In scope |

---

## Open Questions

None of these blocks the work. The plan already uses each recommended default.

1. **One line or two?** The issue note suggests SM5's "smaller second line", or one line if a row has no room.
   **Recommendation: one line everywhere**, with a smaller, ice-blue subtitle on the title's baseline. A plain wheel row is 62px with a 28px title, the info panel has 56px between the title and the artist line, and the results bar is a single 64px strip. A second line would mean shrinking the titles or moving the artist/difficulty layout, which is a Cabinet layout change beyond this issue.
2. **Add brackets around the subtitle (`Title (subtitle)`)?**
   **Recommendation: no.** SM5 joins with a single space (`Song.cpp:1886-1893`), and the pack's subtitles already carry their own `-…-` / `(…)` / `~…~`. Adding brackets would give `Da Roots ((Folk Mix))`.
3. **What should win when the title and subtitle are both too long?**
   **Recommendation: the subtitle keeps at least 40% of the width** (`kSubtitleMinShare`). It is the disambiguator, and same-title songs usually have short titles anyway. Any value between 0.3 and 0.5 works. 0.4 keeps long titles readable on the 560px info panel.
4. **Subtitle colour on plain wheel rows: `kIce` or a dimmed `kWheelText`?**
   **Recommendation: `kIce`.** The palette already documents it as the subtitle colour (`theme.hpp:36`), and it separates the subtitle from the title at a glance. This is a one-constant change if the owner prefers otherwise after the visual check.

---

## Acceptance Criteria

- [ ] Wheel rows for songs with a non-empty `#SUBTITLE` show the subtitle next to the title, so songs sharing a title are visibly different (real-font fit test for the three Disconnected songs + owner check)
- [ ] The song info panel on the select screen shows the subtitle with the title
- [ ] The results screen title bar shows the subtitle too (fades in with the title)
- [ ] The subtitle uses `#SUBTITLETRANSLIT` when the active font can't draw the native text, using the same rule as title and artist (bitmap + TTF overloads tested)
- [ ] Long title + subtitle text is truncated by measured width and never overlaps other wheel or panel elements (fit invariant + overflow test). Songs without a subtitle look the same as today (verbatim code path + identical `top_bar_layout`)
- [ ] `kAllStyles` is 30 styles with still 13 unique (font, size) pairs (no new atlas)
- [ ] The build has no new warnings, and the sandboxed ctest passes 49/49
- [ ] High-score keys, `find_song`, log lines and the timing / judgment / input path are unchanged. `.agents/stories/todo-stories.md` is untouched
