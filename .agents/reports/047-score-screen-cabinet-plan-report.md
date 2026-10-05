# Implementation Report

**Plan**: `.agents/plans/completed/047-score-screen-cabinet-plan.md`
**Branch**: `feature/047-score-screen-cabinet`
**Status**: COMPLETE (owner windowed visual check pending)
**GitHub Issue**: #95

## Summary

The results screen is now the Cabinet v3 score screen (`docs/cabinet-theme/reference/cabinet-v3-results.png`).
The old bitmap-text layout (`draw_text("RESULTS")`, `grade_color`, `results_difficulty_line`, the one-line window counts, the "FAILED" / "NEW RECORD" text) is gone.
In its place: `bg_results`, `bar_top` + `title_score_screen` (with a "SCORE SCREEN" text fallback), a code-drawn slanted difficulty badge plate, and the song title and artist right-aligned in the bar.
On the left are three `stat_panel` plates (MAX COMBO, DANCE POINTS "n / max", HOLDS OK / NG / MINES) with `digits_white` values.
In the centre are the `medallion`, `grade_<tier>` and the tier label, with the `digits_chrome` percentage counting up below. A `record_ribbon` or `failed_ribbon` sits underneath.
On the right are six judgment rows with straight code-drawn track and fill bars. At the bottom are the 44 px hint bar ("ENTER SKIP" during the reveal, "ENTER CONTINUE" once settled) and the scanlines overlay.
The pure reference-px layout and thin draw helpers live in the new `src/screens/results_art.{hpp,cpp}`.
`ResultsAnimator` curves are unchanged, and they now drive the Cabinet parts.
Submission, the best-score rule, input handling and `ResultsSummary` are unchanged.

Owner decisions applied:
1. A failed run shows the earned grade plus `failed_ribbon`.
2. Straight bars (`kJudgmentBarSkew = 0`).
3. Tier labels are "ONE STAR" / "TWO / THREE / FOUR STARS", or "GRADE S+" etc.
4. Negative DP shows 0.
5. MINES counts mines stepped on.
6. The hint reads ENTER SKIP, then ENTER CONTINUE.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure layout, constants, strings, `static_assert`s; add to core sources | `src/screens/results_art.{hpp,cpp}`, `CMakeLists.txt` | ✅ |
| 2 | Draw helpers (backdrop, bars, hint text, stat plates, judgment bars/text, skewed solid, scanlines) | `src/screens/results_art.{hpp,cpp}` | ✅ |
| 3 | `ResultsScreen` rewrite (cached strings in `enter`, fit cache, pinned draw order, accessors) | `src/screens/results_screen.{hpp,cpp}` | ✅ |
| 4 | Reveal comments name the Cabinet parts | `src/screens/results_anim.hpp` | ✅ |
| 5 | Results screen tests (layout, strings, grade textures, badge, headless real-theme render) | `tests/results_screen_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 6 | `test_reveal_order` + banner | `tests/results_anim_test.cpp` | ✅ |
| 7 | README line 51 + full validation | `README.md` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure + build (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8`) | ✅ |
| Lint (no new warnings in results_art / results_screen / results_anim TUs) | ✅ "no new warnings" (0 warnings in the build log) |
| Tests (sandboxed ctest -j8) | ✅ 48/48 passed |
| Static: no `draw_text|bitmap_font|grade_color|results_difficulty_line|"RESULTS"` in results screen + test | ✅ no hits |
| Static: no clock reads in `results_art.cpp` / `results_screen.cpp` | ✅ no hits |
| E2E 1: `results_screen_test` (end-to-end Gameplay → Results → Select + headless real-theme render, 6 sizes x 5 states) | ✅ |
| E2E 2: headless app smoke (`--headless --smoke-test 5 --start-screen select`, scratch data dir, bwrap) | ✅ "Blaze 4k shut down cleanly.", no new error lines (only the sandbox's expected ALSA no-card lines) |
| E2E 3: windowed visual check vs the mock (owner-only) | ⏳ pending (owner) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/results_art.hpp` | CREATE | +218 |
| `src/screens/results_art.cpp` | CREATE | +333 |
| `src/screens/results_screen.hpp` | UPDATE | +75/-21 |
| `src/screens/results_screen.cpp` | UPDATE | +308/-130 |
| `src/screens/results_anim.hpp` | UPDATE (comments only) | +21/-13 |
| `CMakeLists.txt` | UPDATE | +1/-0 |
| `tests/CMakeLists.txt` | UPDATE | +7/-0 |
| `tests/results_screen_test.cpp` | UPDATE | +406/-38 |
| `tests/results_anim_test.cpp` | UPDATE | +18/-1 |
| `README.md` | UPDATE | +1/-1 |

`.agents/stories/todo-stories.md` was not touched. It still shows as modified because of the owner's earlier uncommitted edits.

## Deviations from Plan

- **Extra accessors**: `display_title()`, `display_artist()` and `max_combo_text()` were added next to the plan's list, so tests can pin the cached title/artist and the MAX COMBO digit string ("every cached digit string").
- **Extra pure helpers**: `judgment_row_color(i)` / `judgment_label_color(i)`, plus the draw helpers `draw_judgment_text` and `draw_skewed_solid`. They are used by the screen, and the colour helpers are tested.
- **`enter` change before the log line**: `clear_cached_text()` is called at the start of `enter` (before the invalid-summary early return), so a NO RESULT re-entry clears stale strings. Submission and logging are byte-identical otherwise; `update` is unchanged.
- **Title draws left-aligned at `title_x`, artist right-aligned at 1240**: this keeps the 16 px plate → title gap exact when the title is truncated.
- **Hint wording on a settled failed run**: the old code said "[ENTER] RETURN TO WHEEL". Per owner decision 6, it is now "ENTER CONTINUE".
- **Headless render coverage**: the `results_art` draw helpers return early on an uninitialised renderer (as the plan pins), so the headless test exercises layout, fitting and caching, but not the GL draw calls themselves. Visual correctness is the owner's windowed check.
- The old `test_render_edit_chart` test was kept (not in the plan's keep list, but harmless and still green).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/results_screen_test.cpp` | `test_screen_title_text`, `test_grade_textures` (all 17 compiled tiers → 680x400 sprite; tier labels), `test_digit_strings` (negative DP/combo → "0", MINES = HitMine, glyph set incl. `format_percent` edge values), `test_top_bar_layout` (mock case 957/1040/1056/1190, no artist, 2000-px title, caps, no badge, NaN/inf), `test_stat_panels` (rects, slanted x 67.8/62.0, hold columns 0/123/178, wide value), `test_judgment_rows` (mock coords, fills 132.7/2/0/154, total==0, colours, HitMine excluded), `test_ribbon_and_grade_rects`, `test_enter_caches_badge` (HARD 9 kHard, Edit "JBEAN" kEdit, null song/chart, invalid clears, chart key unchanged), `test_render_cabinet_headless` (clear / NEW RECORD / failed / long Edit+title+artist / NO RESULT x 1280x720, 2560x1440, 3440x1440, 640x480, 320x240, 0x0); removed `test_difficulty_line` |
| `tests/results_anim_test.cpp` | `test_reveal_order` |
