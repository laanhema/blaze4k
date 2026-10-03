# Implementation Report

**Plan**: `.agents/plans/completed/045-select-screen-cabinet-plan.md`
**Branch**: `feature/045-select-screen-cabinet`
**Status**: COMPLETE (owner-only GUI visual check pending)

## Summary

Restyled the Song Select screen with the Cabinet v3 look (#94). A new GL-free `select_art` module holds the reference-space layout (difficulty row and wheel row rects, list windows, the wheel slide easing, row styles/labels/ticks, chip text and boxes, hint line and arrow geometry) plus thin draw helpers over `ThemeTextures` / `TextRenderer`. `SelectScreen::render` is now a sequence of those calls: `bg_select`, banner in `banner_frame` (or `banner_fallback`), title/artist/BPM, up to 5 slanted difficulty rows (baked `diff_row_*` or a code-drawn Edit row) with 10 meter ticks and best %, the wheel with inline pack header rows, distance indents and an 80 ms eased slide on fixed `dt`, `bar_top` + `title_select_music` + cyan/green chips, `bar_hint` with gold keys and code-drawn arrows, and scanlines. The options overlay still uses the bitmap font (#96) but now draws over the Cabinet screen. Navigation, key repeat, preview, Confirm/Back are unchanged.

Owner decisions applied: mock row spacing (`kDiffRowGap` 8 -> 10, `kWheelRowGap` 10 -> 14), plain gaps between hints (no separators), inline pack headers, slide only when the window moves, OpenITG classification for row colours with the simfile label shown, capitalised standard labels and pack names, scanlines drawn, no text on the fallback banner.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `format_speed_mod` exported; TTF-coverage `song_display_title/artist` overloads; untruncated `chart_display_label(chart)`; row gaps from the mock | `src/screens/options_menu.*`, `src/screens/song_display_text.*`, `src/render/theme.hpp` | ✅ |
| 2 | `select_art` pure layout + CMake registration | `src/screens/select_art.{hpp,cpp}`, `CMakeLists.txt` | ✅ |
| 3 | `select_art` draw helpers (reuses `title_art::draw_scanlines`) | `src/screens/select_art.cpp` | ✅ |
| 4 | SelectScreen state: wheel display rows, song->row map, slide, cached chips, test accessors; old colour helpers removed | `src/screens/select_screen.{hpp,cpp}` | ✅ |
| 5 | New `SelectScreen::render` (draw order per plan, overlay drawn on top) | `src/screens/select_screen.cpp` | ✅ |
| 6 | `select_art_test` (16 test functions) | `tests/select_art_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 7 | `select_screen_test` updated (old helper tests removed, slide + pack-row tests, real-service render smoke) | `tests/select_screen_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 8 | Final checks + E2E | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, touched TUs force-recompiled) | ✅ exit 0, no compiler warnings |
| Lint (the `-Wall -Wextra -Wpedantic` build is the lint gate) | ✅ no warnings |
| Tests (sandboxed `bwrap ... ctest --test-dir build --output-on-failure`) | ✅ 48/48 passed (47 baseline + `select_art_test`) |
| `grep difficulty_row_text\|difficulty_color src tests` | ✅ no match |
| `grep draw_text( select_screen.cpp` | ✅ only the 3 calls inside the options-overlay block (#96) |
| `git status` | ✅ only plan-listed files + the owner's untouched `todo-stories.md` (its diff is the owner's pre-existing edit) |
| E2E 1: automated pure-path proof (ctest) | ✅ |
| E2E 2: headless real app `--smoke-test 30 --start-screen select` (in bwrap) | ✅ `[SelectScreen] library: 221 songs, 1116 charts`, `[ScreenManager] enter Select`, `Blaze 4k shut down cleanly.` (only sandbox ALSA noise and the expected fresh-data-dir config/scores notices) |
| E2E 3: `grep "Unknown theme texture"` on the smoke log | ✅ no match; `test_texture_names_exist` asserts every name in the real manifest |
| E2E 4: owner side-by-side GUI check vs `cabinet-v3-select.png` (720p, 1440p, ultrawide/16:10, slide feel, Edit row, chips after Options) | ⏳ owner-only, not run by the agent |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/select_art.hpp` | CREATE | +314 |
| `src/screens/select_art.cpp` | CREATE | +602 |
| `tests/select_art_test.cpp` | CREATE | +653 |
| `src/screens/select_screen.cpp` | UPDATE | +200/-197 |
| `src/screens/select_screen.hpp` | UPDATE | +28/-23 |
| `src/screens/song_display_text.cpp` | UPDATE | +36/-5 |
| `src/screens/song_display_text.hpp` | UPDATE | +16/-0 |
| `src/screens/options_menu.cpp` | UPDATE | +2/-2 |
| `src/screens/options_menu.hpp` | UPDATE | +5/-0 |
| `src/render/theme.hpp` | UPDATE | +2/-2 |
| `CMakeLists.txt` | UPDATE | +1/-0 |
| `tests/CMakeLists.txt` | UPDATE | +25/-0 |
| `tests/select_screen_test.cpp` | UPDATE | +257/-100 |

## Deviations from Plan

1. **Slide rule widened to |Δfirst| <= 2.** The plan slides only for |Δfirst| == 1 and snaps otherwise. With inline pack headers, one song step across a header moves the window 2 rows, so every pack crossing would have snapped. `wheel_scroll_start` now slides for |Δ| <= 2 (`kWheelMaxSlideRows`, still clamped to ±152) and snaps for larger jumps (wraps). Pinned in `test_scroll_easing` and `test_wheel_skips_pack_rows`.
2. **Up to two extra wheel rows while sliding, only on the side the rows moved away from** (the plan says one above and one below). Under 40 ms key repeat the offset settles near 86 px, more than one pitch, so one extra row would leave a gap. Drawing only on the exposed side avoids extra rows peeking out on the other side.
3. **Selected wheel bar drawn after the sliding rows' text** (art, then song/pack text, then the selected bar, then its title). Rows sliding under the fixed gold bar would otherwise print their titles on top of it. This adds about 2 texture flushes.
4. **Empty passthrough label** shows the resolved difficulty's name (e.g. "HARD") instead of an empty tab. This is OpenITG's displayed difficulty. Tested in `test_labels`.
5. **`pack_names_` removed** from SelectScreen. It became dead after the render rewrite, and `pack_labels_` (upper case) replaces it.
6. Hint arrows are vertically centred on the hint band centre (ref y 694). This matches the mock's cap band 687..701 and avoids a cap-height metric that `TextRenderer` does not expose.
7. The Edit row's 1 px edge strips are drawn as bands of the body parallelogram, with an `if (row.h > 2)` guard.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` (new) | `test_list_window`, `test_visible_rows`, `test_difficulty_row_rects` (4 sizes), `test_wheel_row_rects` (mock example at 4 sizes, indent clamp, slide rows), `test_build_wheel_rows`, `test_scroll_easing`, `test_row_style` (incl. Expert/Novice/unknown by meter, Edit), `test_labels` (incl. measured truncation to the 128 px budget), `test_ticks`, `test_chip_text`, `test_chip_rects`, `test_hint_layout` (real headless TextRenderer; line 304..976 vs mock 308..970; arrow corners), `test_skewed_quad`, `test_chrome_layout` (title sprite, bar heights, hint bar top at 4 sizes vs manifest), `test_texture_names_exist`, `test_render_smoke` (populated incl. a 7-chart song with Edits, empty library, options overlay, real and null services, 6 window sizes) |
| `tests/select_screen_test.cpp` (updated) | Removed `test_difficulty_colors` / `test_difficulty_row_text` (replaced in `select_art_test`). `test_named_edit_charts` now checks measured truncation with the real `TextRenderer` and renders with real services. Added `test_wheel_slide` (12-song/2-pack fixture: no slide at the top, ±76 on window moves, settles in 5 ticks, wrap snaps, held repeat stays within ±152, re-enter resets) and `test_wheel_skips_pack_rows`. Added a populated render smoke with real services at 5 sizes, with and without the overlay. Every pre-existing behaviour test is unchanged and passing |
