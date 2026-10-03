# Implementation Report

**Plan**: `.agents/plans/completed/037-edit-chart-names-plan.md`
**Branch**: `feature/037-edit-chart-names`
**Status**: COMPLETE

## Summary

Edit difficulty charts now show their chart name (the description) instead of "Edit" on the song-select difficulty rows and on the results difficulty line, as SM5 `StepsDisplay.cpp:199-202` and OpenITG `DifficultyMeter.cpp:158` do. An Edit chart with no name still shows "Edit". Long names are shortened with ASCII `...` so the select row stays inside the highlight bar. The SSC loader now follows SM5 `NotesLoaderSSC.cpp:319-349`. `#CHARTNAME` no longer overwrites the description, and `#DESCRIPTION` is the description when `#VERSION` ≥ 0.74 (0.83 when the tag is absent). Below 0.74 it is a chart name and is not stored. The result no longer depends on tag order. Non-Edit labels, `difficulty_color` and high-score keys are unchanged.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | SSC `#CHARTNAME`/`#DESCRIPTION`/`#VERSION` per SM5 | `src/chart/simfile_parser.cpp` | ✅ |
| 2 | `truncate_to_cells` (code-point safe) | `src/render/bitmap_font.hpp/.cpp` | ✅ |
| 3 | `chart_display_label` shared helper | `src/screens/song_display_text.hpp/.cpp` | ✅ |
| 4 | `difficulty_row_text` + `kMinDifficultyLabelCells`, select draw site | `src/screens/select_screen.hpp/.cpp` | ✅ |
| 5 | `results_difficulty_line`, results draw site | `src/screens/results_screen.hpp/.cpp` | ✅ |
| 6 | Parser tests (.sm and .ssc named edits, precedence, version) | `tests/parser_test.cpp` | ✅ |
| 7 | Truncation and label unit tests (incl. 10k fuzz) | `tests/bitmap_font_test.cpp` | ✅ |
| 8 | Select and results tests (pure builders and real library/render path) | `tests/select_screen_test.cpp`, `tests/results_screen_test.cpp` | ✅ |
| 9 | Tick `(#84)` | `TODO.md` (gitignored) | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | ✅ warning-clean |
| Lint | n/a (no linter configured) |
| Tests (sandboxed bwrap ctest) | ✅ 100% tests passed out of 41 |
| E2E 1: real-pack scan (`songs/`, scratch program, deleted afterwards) | ✅ `JBEAN` (Aliens in Our Midst, Dance All Night), `mDaWg` (All That Matters), `mDaWg & Hatena...` (Bagpipe). 34 Edit charts in total; only Y2Z shows `Edit`, and its `#NOTES` description really is empty |
| E2E 2: visual check | Not run (owner only; the agent must not launch the GUI) |
| E2E 3: sandboxed ctest | ✅ 41/41 |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/chart/simfile_parser.cpp` | UPDATE | +34/-3 |
| `src/render/bitmap_font.cpp` | UPDATE | +36/-0 |
| `src/render/bitmap_font.hpp` | UPDATE | +9/-0 |
| `src/screens/results_screen.cpp` | UPDATE | +18/-4 |
| `src/screens/results_screen.hpp` | UPDATE | +9/-0 |
| `src/screens/select_screen.cpp` | UPDATE | +22/-4 |
| `src/screens/select_screen.hpp` | UPDATE | +14/-0 |
| `src/screens/song_display_text.cpp` | UPDATE | +9/-0 |
| `src/screens/song_display_text.hpp` | UPDATE | +13/-2 |
| `tests/bitmap_font_test.cpp` | UPDATE | +110/-0 |
| `tests/parser_test.cpp` | UPDATE | +100/-0 |
| `tests/results_screen_test.cpp` | UPDATE | +64/-0 |
| `tests/select_screen_test.cpp` | UPDATE | +152/-0 |
| `TODO.md` | UPDATE (gitignored, local only) | +1/-1 |

## Deviations from Plan

1. **Branch from a dirty tree**: `main` had an unrelated modified `.agents/stories/todo-stories.md`. The invoking request said to create the branch and leave that file alone, so it came along untouched and unstaged.
2. **Cell counting in `difficulty_row_text`**: the fixed prefix and suffix are counted with `text_width(...)/6` rather than `size()`. The result is the same for ASCII, and it stays correct if `best` ever contains non-ASCII.
3. **Extra defensive guard**: both builders treat `pixel <= 0` or a non-positive width as 0 row cells, which clamps the name to 9, and they cap the float→int cast. Tests cover both cases.
4. **Results minimum**: `results_difficulty_line` uses a local `kMinNameCells = 9` instead of `kMinDifficultyLabelCells`, so `results_screen` does not have to include `select_screen.hpp`. The value is the same.
5. **Extra tests beyond the plan**: the exact 0.74 boundary; `inf`/`nan`/`1e999`/empty `#VERSION` → old; a steps-level `#VERSION` that applies in file order; a `#CHARTNAME`-only SSC edit through the real library (falls back to `Edit`); high-score key invariance under a description change.
6. **`TODO.md` is gitignored**: it was edited locally and does not show in git status or the diff.
7. **E2E 2 (visual)** was left to the owner, as the plan says.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/parser_test.cpp` | SM `#NOTES` named Edit; SSC modern; reversed tag order (last-wins regression); CHARTNAME only; no `#VERSION`; 0.70; 0.74 boundary; junk/non-finite versions; steps-level `#VERSION` ordering; NOTEDATA reset |
| `tests/bitmap_font_test.cpp` | `truncate_to_cells`: fits unchanged, Bagpipe 17-cell, multi-byte boundary, combining mark kept, budgets 0–4, 10k fuzz (seed 84). `chart_display_label`: Edit named/empty, lowercase passthrough, non-Edit ignores description, no truncation of standard labels, invalid label resolves via description, long name budget |
| `tests/select_screen_test.cpp` | `difficulty_row_text`: exact non-Edit format, JBEAN, empty → Edit, Bagpipe, 60-char name fits the 1280 px bar, 320 px / zero-width / zero-pixel clamp. Real path: .sm + .ssc temp pack scanned by SongLibrary, every chart visited with Right and rendered headlessly at 1280×720 and 640×480 |
| `tests/results_screen_test.cpp` | `results_difficulty_line`: nullptr, Hard 9, JBEAN 10, full Bagpipe name, Edit 10, long name fits, degenerate widths; high-score key ignores description; headless render smoke with a long UTF-8 Edit name |

## Out of Scope (flagged, not fixed)

- At narrow widths (≤ ~800 px), standard rows already overflow the highlight bar. Edit rows are never worse than `Challenge` rows.
- Pass 1 of the SSC parser walks every tag, so per-chart `#BPMS`/`#OFFSET`/`#STOPS` can leak into song-level timing. Possible follow-up.
