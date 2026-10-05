# Implementation Report

**Plan**: `.agents/plans/completed/052-song-subtitles-plan.md`
**Branch**: `feature/052-song-subtitles`
**Status**: COMPLETE (owner windowed check pending)
**GitHub Issue**: #110

## Summary

Song subtitles (`#SUBTITLE` / `#SUBTITLETRANSLIT`) are now drawn on the same line as the title, right after it, in a smaller secondary style that sits on the title's baseline. This applies in three places: the select wheel rows (plain and selected), the song info panel under the banner, and the results top bar, where the subtitle fades in with the title. So "Disconnected -Hyper-", "-Mobius-" and "-Hardkore-" now read differently.

- `song_display_subtitle` (bitmap and TrueType overloads) picks native or translit with the same coverage rule as the title and artist, but on the subtitle fields alone (SM5 `Song::GetDisplaySubTitle`).
- `fit_title_subtitle` is a pure, unit-agnostic split of a measured width budget between the title and the subtitle. When both don't fit, the subtitle keeps at least 40% (`kSubtitleMinShare`).
- Every draw site truncates by measured width. With an empty subtitle, every site takes today's code path verbatim. `top_bar_layout` with `subtitle_w <= 0` gives a layout identical field by field (tested).
- Four new styles reuse existing (font, size) atlases: `kAllStyles` goes from 26 to 30, and the unique pairs stay at 13.
- No brackets are added. Identity (high-score keys, `find_song`), log lines and the timing, input and judgment code are untouched.

Resolved open questions (the plan's defaults): one line, no added brackets, a 40% minimum share for the subtitle, `kIce` on plain wheel rows.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Subtitle display choice + `fit_title_subtitle` | `src/screens/song_display_text.{hpp,cpp}` | ✅ |
| 2 | 4 subtitle styles, `kAllStyles` 26 → 30 (+ 3 test pins) | `src/render/theme.hpp`, `tests/{ttf_font,select_art,setup_art}_test.cpp` | ✅ |
| 3 | Info panel subtitle (`kSubtitleGap`, `baseline_aligned_top`) | `src/screens/select_art.{hpp,cpp}` | ✅ |
| 4 | Wheel row subtitle (`WheelRowView::subtitle`, titles then subtitles per pass) | `src/screens/select_art.{hpp,cpp}` | ✅ |
| 5 | Select screen wiring | `src/screens/select_screen.cpp` | ✅ |
| 6 | Subtitle-aware `top_bar_layout` (`kBarSubtitleGap`, `subtitle_x/subtitle_max_w`) | `src/screens/results_art.{hpp,cpp}` | ✅ |
| 7 | Results caches, fits and draws the subtitle | `src/screens/results_screen.{hpp,cpp}` | ✅ |
| 8 | Tests | `tests/{bitmap_font,select_art,results_screen}_test.cpp` | ✅ |
| 9 | Tick TODO line 43 | `TODO.md` (gitignored) | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC 16.2.1) | ✅ exit 0 |
| Lint (no new warnings: touched the 5 screen TUs + 3 test TUs, rebuilt, grepped for warning/error) | ✅ none |
| Tests (sandboxed ctest) | ✅ 49/49 passed (0.71 s) |
| `git diff -- src/data src/chart` | ✅ empty |
| `git diff --stat -- src/timing src/audio src/input src/gameplay assets` | ✅ empty |
| No added brackets grep | ✅ "no added brackets" |
| `todo-stories.md` not staged | ✅ 0 |
| Headless smoke (`--headless --smoke-test 5 --start-screen select --songs ./songs`, bwrap, scratch data dir) | ✅ exit 0, "Blaze 4k shut down cleanly.", 221 songs. The only error lines are ALSA "cannot find card" lines, caused by the sandbox's empty `/dev/snd` |
| Owner windowed check (E2E step 3) | ⏳ pending (owner only; the GUI was not launched) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/theme.hpp` | UPDATE | +8/-1 |
| `src/screens/song_display_text.hpp` | UPDATE | +31/-0 |
| `src/screens/song_display_text.cpp` | UPDATE | +46/-0 |
| `src/screens/select_art.hpp` | UPDATE | +13/-3 |
| `src/screens/select_art.cpp` | UPDATE | +73/-4 |
| `src/screens/select_screen.cpp` | UPDATE | +7/-1 |
| `src/screens/results_art.hpp` | UPDATE | +11/-1 |
| `src/screens/results_art.cpp` | UPDATE | +19/-4 |
| `src/screens/results_screen.hpp` | UPDATE | +9/-1 |
| `src/screens/results_screen.cpp` | UPDATE | +29/-1 |
| `tests/bitmap_font_test.cpp` | UPDATE | +97/-0 |
| `tests/select_art_test.cpp` | UPDATE | +111/-7 |
| `tests/results_screen_test.cpp` | UPDATE | +89/-2 |
| `tests/ttf_font_test.cpp` | UPDATE | +2/-2 |
| `tests/setup_art_test.cpp` | UPDATE | +1/-1 |
| `TODO.md` | UPDATE (gitignored, local only) | +1/-1 |

## Deviations from Plan

- `TODO.md` is gitignored, so the Task 9 tick is a local-only edit. It shows in neither `git status` nor `git diff`, so the plan's `git diff --stat -- TODO.md` check reports nothing for it. The line was verified directly (`sed -n 43p TODO.md` → `- [x] ...`).
- Wheel subtitles (Task 4): the second, subtitle-only loop recomputes the fit per row instead of caching it in a fixed-size array. The plan allowed either approach. Recomputing avoids assuming a bound on the span size that `draw_wheel` receives.
- `top_bar_layout`: the plate's `title_gap` now keys on the title slot width (`slot_w > 0`) instead of `title_max_w > 0`. With no subtitle, `slot_w == title_max_w`, so the output is unchanged (pinned by the field-identical tests). With a subtitle, the plate keeps its gap even in the degenerate case where the fitted title gets 0 width.
- The headless smoke has no GL or text services, so it exercises the select update/render path over the real pack but not the TrueType draw calls. Those are covered by the `select_art_test` and `results_screen_test` real-font smoke renders.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/bitmap_font_test.cpp` | `test_subtitle_display_and_fit`: bitmap subtitle choice (translit for uncovered, covered native wins, no translit keeps native, empty, aliasing, choice independent of the title, null renderer), `fit_title_subtitle` table (11 cases), NaN/inf/negative sanitising, and an invariant grid (each output ≤ its input, sum ≤ budget, subtitle ≥ min(whole, 40%)) |
| `tests/select_art_test.cpp` | `test_display_text_coverage` subtitle cases (TTF Latin-1 native vs bitmap translit, CJK → translit, CJK with no translit → native, null renderer). New `test_disconnected_subtitles_fit` (real fonts, the 3 Disconnected subtitles whole at the deepest wheel indent, the selected bar and the info panel; the long title + "(Two Gees Radio Edit)" overflow truncates with "...", keeps ≥ 40% and never overlaps). `test_options_styles_prebaked` covers the 4 new styles. `test_render_smoke` adds a subtitled long-title song plus 2 "Disconnected" songs (13 songs / 15 rows) |
| `tests/results_screen_test.cpp` | `test_top_bar_layout`: no subtitle (0/negative/NaN/inf) is field-identical, subtitle group geometry, long title + subtitle ≥ 40% within the slot, no title → subtitle absent, non-finite → finite. `test_enter_caches_badge`: subtitle cached, title unchanged, bare/invalid clear it. `test_render_cabinet_headless`: a subtitled song is fitted with text services and empty without |
| `tests/{ttf_font,setup_art}_test.cpp` | `kAllStyles` 26 → 30 (pairs stay 13) |
