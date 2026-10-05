# Implementation Report

**Plan**: `.agents/plans/completed/048-options-overlay-cabinet-plan.md`
**Branch**: `feature/048-options-overlay-cabinet`
**Status**: COMPLETE (the owner's windowed visual check is still pending)
**GitHub Issue**: #96

## Summary

The options overlay on song select now uses the Cabinet look instead of the old bitmap font.
It dims the whole window with `kGameplayScrim`, then draws a centred navy panel (2px `kEditEdge` ring) with a `bar_top` header and an italic white "OPTIONS" title.
The seven option rows are slanted `wheel_row` slices.
The selected row is the gold `wheel_row_selected` bar (560x80, which keeps the sprite's slant).
Names sit on the left. Values are right-aligned in `kGold`, or in dark ink (`kSelectedInk`) on the selected bar.
`bar_hint` is redrawn at the bottom with the legend "↑↓ ROW  ←→ CHANGE  ENTER NEXT  ESC CLOSE".

The new GL-free module `src/screens/options_art.{hpp,cpp}` holds the reference-px layout and the draw helpers.
`select_art`'s legend builder is now a shared `HintItem` / `layout_hint_items` / `draw_hint_line` API, and select's own legend is unchanged (`test_hint_layout` is unmodified and green).
`SelectScreen` keeps the value strings in a cache that is marked dirty on open and on every overlay press. The cache is refreshed after the event loop and on the `enter()` reopen path, so `render()` never builds the value strings.
Scanlines are now the last draw, so they cover the overlay too.
Input routing, `options_menu.*`, sounds and the Calibrate / Remap transitions are not changed.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Generalise the select hint line (`HintItem`, `layout_hint_items`, `draw_hint_line`; `hint_layout` / `draw_hint_bar` are thin wrappers) | `src/screens/select_art.{hpp,cpp}` | ✅ |
| 2 | `options_art` pure layout: constants, `static_assert` room budget, `panel_rect` / `header_rect` / `row_rect` / `name_x` / `value_right` / `row_names` / `hint_layout`; CMake registration | `src/screens/options_art.{hpp,cpp}`, `CMakeLists.txt` | ✅ |
| 3 | `options_art` draw helpers: `draw_panel`, `draw_rows`, `draw_hint_bar`, `draw_overlay` | `src/screens/options_art.{hpp,cpp}` | ✅ |
| 4 | `SelectScreen` uses the overlay: bitmap block, colours and include removed; value cache added; scanlines moved last | `src/screens/select_screen.{hpp,cpp}` | ✅ |
| 5 | `select_art_test` additions | `tests/select_art_test.cpp` | ✅ |
| 6 | `select_screen_test` cache checks | `tests/select_screen_test.cpp` | ✅ |
| 7 | Full validation | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, touched TUs forced to recompile) | ✅ exit 0 |
| Warning gate (warnings in `options_art` / `select_art` / `select_screen`) | ✅ "no new warnings" (0 warnings in the whole rebuild log) |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 48/48 passed |
| Static: `grep -nE "draw_text\|bitmap_font\|kPlaceholderColor\|kDimOverlay" src/screens/select_screen.cpp` | ✅ no hits |
| Static: clock grep over `options_art.cpp` and `select_screen.cpp` | ✅ no hits |
| Static: `git diff --stat` on `options_menu.{cpp,hpp}` and `tests/options_menu_test.cpp` | ✅ empty |
| E2E 1: `select_screen_test` and `select_art_test` (real overlay flow, every overlay row rendered at 6 sizes with real and null services) | ✅ |
| E2E 2: headless smoke (`blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>`, in bwrap) | ✅ "Blaze 4k shut down cleanly." The ALSA lines come from the sandbox hiding `/dev/snd` (null device), not from this change |
| E2E 3: windowed visual check | ⏳ owner only (the agent must not launch the GUI) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/options_art.hpp` | CREATE | +146 |
| `src/screens/options_art.cpp` | CREATE | +211 |
| `src/screens/select_art.hpp` | UPDATE | +20/-0 |
| `src/screens/select_art.cpp` | UPDATE | +76/-28 |
| `src/screens/select_screen.hpp` | UPDATE | +12/-1 |
| `src/screens/select_screen.cpp` | UPDATE | +22/-61 |
| `CMakeLists.txt` | UPDATE | +1/-0 |
| `tests/select_art_test.cpp` | UPDATE | +215/-1 |
| `tests/select_screen_test.cpp` | UPDATE | +37/-0 |

`.agents/stories/todo-stories.md` (the owner's unrelated edit) was not touched or staged.

## Deviations from Plan

1. **`kSelectHintItems` has 10 items, not 9.** The plan writes `std::array<HintItem, 9>`, but select's legend has 10 items: ↑↓, SONG, ←→, DIFFICULTY, ENTER, PLAY, TAB, OPTIONS, ESC and TITLE. The array has size 10, and the output is identical (`test_hint_layout` is unmodified and green).
2. **The options legend has 8 items, not "6 items".** The arrow pairs and their words are separate items (VArrows, ROW, HArrows, CHANGE, ENTER, NEXT, ESC, CLOSE). It still lays out to the pinned **10 pieces**.
3. **`layout_hint_items` cap behaviour.** Before laying anything out, it counts how many items fit in `kHintPieceCount`. An arrow pair that would straddle the cap is dropped whole, and the last item that fits gets no trailing gap. Tests pin both.
4. **Title-fit check.** The plan pins "title line height ≤ 64". SairaExtraBold 44 has a line box of about 70 px (ascent 50 + descent 20, the same as `kSongTitle`), so that literal check cannot pass with the pinned style. The test pins the intent instead: when the line box is centred in the 64 px band, the baseline and one em above it stay inside the band. With PIL metrics, the caps land at y 111..142 in the band 95..159, centred within 0.5 px.
5. **Draw order inside the overlay.** Each helper draws its own text: `draw_panel` draws the title, `draw_rows` draws its row text, and `draw_hint_bar` draws the legend. The resulting order is: scrim → panel → header → title → row art → row text (names, values, selected pair) → `bar_hint` → legend. The plan's order put all text in one final pass. The two orders look the same, because these elements do not overlap.
6. **Extra cache check in `select_screen_test`.** On the second calibration visit, the test sets `config.offset.global_offset_seconds = -0.011` (standing in for an accepted wizard). It then checks that the reopened overlay shows "-0.011 s" on its first frame, and restores the offset afterwards. No existing assertion was changed.
7. **Branch from a dirty `main`.** The owner's unrelated `.agents/stories/todo-stories.md` change is carried on the branch uncommitted, as the invoking request said to proceed.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` | `test_options_layout`: panel and header rects, row rects for every selection, no overlap, room inside the bars, gold-bar aspect, 1440p and 21:9 mapping, clamping, insets, `row_names` |
| | `test_options_text_fits`: every row with its widest real values (including the whole X grid, ±3600 s), selected and unselected, real fonts; title width and band fit |
| | `test_options_hint_layout`: 10 pieces in order, spacing, centring, margins; the `layout_hint_items` cap (20 words → 12, no arrow-pair split) |
| | `test_options_styles_prebaked`: each style shares a pre-baked (font, size); `kAllStyles` is still 26 |
| | `test_render_smoke` extended: walks every overlay row as the selection (Down ×7 with clamping, Up, Back) with real and null services |
| `tests/select_screen_test.cpp` | `test_options_overlay`: the cache matches `options_row_value_text` after open and every adjustment; pins CMOD / C400 / DOWN / OFF |
| | `test_calibration_launch_from_options`: the reopened overlay's offset text equals `format_offset(config offset)`, including a freshly written offset |

## Pending (owner)

- Windowed visual check (plan E2E step 3): Tab on song select, every row, the legend, the Calibrate / Remap round trips, then maximised and 4:3 / 21:9 windows.
