# Implementation Report

**Plan**: `.agents/plans/completed/061-edit-diff-row-match-baked-plan.md`
**Branch**: `feature/061-edit-diff-row-match-baked`
**Issue**: #130 ([TODO-35] Make the Edit difficulty row match the other rows' size and position in song select)
**Status**: COMPLETE

## Summary

The code-drawn Edit row in the song select difficulty list now has the baked rows' geometry. A new pure helper, `select_art::edit_row_rects(row, selected)`, returns the row's frame, inner and tab rects from four named constants (`kEditInsetX` 10, `kEditSelectedInsetX` 12, `kEditBorder` 1, `kEditSelectedBorder` 2). `draw_difficulty_row_art` draws the Edit row from those rects: the body over the whole frame, a four-strip ring on top (`kEditEdge`, gold when selected), and the 150 px tab inside the ring. The gold outset (`kEditSelectedOutset`) is gone, so the selected Edit row is no longer 4 px taller or 28 px wider than the other selected rows.

A new test, `test_edit_row_geometry`, decodes all ten `diff_row_*.png` textures with stb_image and holds the Edit frame, tab and height to their pixels within 1 px. The row rect (`difficulty_row_rect`) and every `kDiff*` value are unchanged. Presentation only: no clock, judgment, input or audio code changed.

The three open questions were resolved with the plan's defaults: geometry only (no baked glow, body alpha or tint, no `diff_row_edit` texture); `kDiffNameBudget` stays 118 with only its comment updated; the 1 px ring is drawn on all four sides of the unselected Edit row.

Nothing is committed. The changes are uncommitted on the branch.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Capture the "before" screenshots | `/tmp/blaze4k-verify/130-before/evidence/` | ✅ |
| 2 | Add the Edit row constants and the helper's declaration | `src/screens/select_art.hpp` | ✅ |
| 3 | Implement `edit_row_rects` and draw the Edit row from it | `src/screens/select_art.cpp` | ✅ |
| 4 | Update the two Edit-tab guards in the existing tests | `tests/select_art_test.cpp` | ✅ |
| 5 | Add `test_edit_row_geometry()` | `tests/select_art_test.cpp` | ✅ |
| 6 | TODO tick | `TODO.md` (gitignored) | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j`) | ✅ |
| Lint (no linter; zero compiler warnings after touching both TUs) | ✅ (0 warnings in the whole build log) |
| Tests (sandboxed ctest) | ✅ (51/51 passed, 2.86 s) |
| `select_art_test` run directly (sandboxed) | ✅ (`- Edit row geometry vs the baked rows ok.` printed) |
| Static: `kEditSelectedOutset` gone from `src` and `tests` | ✅ (grep exit 1, as expected) |
| Static: `kDiffRowWidth` / `kDiffNameX` / `kDiffNameSelectedX` / `kDiffMeterCentreX` / `kDiffTickX` / `kDiffBestRight` / `kDiffNameBudget` values unchanged | ✅ (grep exit 1, as expected) |
| Static: only `select_art.cpp` and `select_art.hpp` changed in `src/` | ✅ |
| Static: `src/timing`, `src/audio`, `src/input`, `src/gameplay`, `src/render`, `assets`, `tests/CMakeLists.txt` untouched | ✅ (empty diff stat) |
| Static: `.agents/issues/todo-issues.md` not staged | ✅ (0; nothing is staged) |
| E2E 1: automated (`test_edit_row_geometry`, `test_ticks`, `test_meter_clearance`, `test_name_margin`, `test_render_smoke`) | ✅ |
| E2E 2: `/verify` screenshots (AC 5), owner's packs, 1280x720 | ✅ (3 of 3 "after" shots and 2 of 2 "before" shots captured and read; every shot shows `Virtual Emotion`) |

### Sensitivity of the new pixel test

A scratch run with the test's tolerance changed (restored afterwards; the committed value is the plan's `kTol = 1.0f`):

| `kTol` | Result |
|--------|--------|
| 1.0 (as written) | passes |
| 0.6 | passes |
| 0.5 | fails on the tab's right edge |

This agrees with the plan's probe (worst frame error 0.34 px, worst tab error 0.54 px), so the test measures the PNGs and is not passing by default.

### Screenshot measurements (window px = reference px)

`01-edit-unselected.png` and `02-edit-selected.png`, before against after, with the baked row measured in the same shot:

| Shot, row, scanline | Quantity | Before | After | Baked reference | Plan's expected "after" |
|---|---|---|---|---|---|
| `01`, `BlueChaos` unselected, y 456 | tab fill left..right | 44..194 | 55..205 | `HARD` at y 510: 55..205 | 55..205 |
| `01`, `BlueChaos` unselected, y 456 | right ring pixel | none found (the row ended at 608) | x 597 (ends at 598) | `HARD` at y 510: x 597 | 598 |
| `01`, `BlueChaos` unselected, column x 120 | tab fill top..bottom | 434..478 | 435..477 | (inside the 1 px ring) | 1 px ring above and below |
| `02`, `BlueChaos` selected, y 452 | gold left, tab left..right | 56, 58..208 | 70, 72..222 | `CHALLENGE` in `01` at y 398: 70, 72..222 | 70, 72..222 |
| `02`, `BlueChaos` selected, y 452 | gold right end | 624 | 610 | `CHALLENGE` in `01`: 610 | 610 |
| `02`, `BlueChaos` selected, column x 500 | gold top..bottom | 424..480 (56 tall) | 426..478 (52 tall) | `CHALLENGE` in `01`: 372..424 (52 tall) | 426..478 |
| `03`, `Phrekwenci` selected, y 506 | gold left..right, tab left..right | not taken | 70..610, 72..222 | same as `CHALLENGE` | same as `CHALLENGE` |
| `03`, `Phrekwenci` selected, column x 500 | gold top..bottom | not taken | 480..532 (52 tall) | 52 tall | 52 tall |

Every "after" value equals the plan's expected value and the baked row's value.

### Screenshots

| File | Shows |
|------|-------|
| `/tmp/blaze4k-verify/130-before/evidence/01-edit-unselected.png` | Before. Virtual Emotion: `CHALLENGE` selected; Edit rows `BlueChaos`, `Phrekwenci`, `Rynker` start 10 px left of `HARD` and run 10 px past it on the right |
| `/tmp/blaze4k-verify/130-before/evidence/02-edit-selected.png` | Before. `BlueChaos` (Edit) selected: its gold frame sticks out left and right of the other rows and is taller |
| `/tmp/blaze4k-verify/130-after/evidence/01-edit-unselected.png` | After. Same state as "before 01": the three Edit rows line up with `HARD` on both sides, tabs included |
| `/tmp/blaze4k-verify/130-after/evidence/02-edit-selected.png` | After. `BlueChaos` (Edit) selected: gold frame at the same position and size as the selected `CHALLENGE` row in shot 01 |
| `/tmp/blaze4k-verify/130-after/evidence/03-edit-selected-lower.png` | After. `Phrekwenci` (Edit) selected between `HARD` and `Rynker`, same frame geometry |

`game.log` for each run is in the same `evidence/` directory. `/tmp/blaze4k-verify/130-before/evidence/00-first-song.png` and `/tmp/blaze4k-verify/130-keytest/evidence/` are harness checks (see Deviations), not evidence for the change.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/select_art.hpp` | UPDATE | +29/-6 |
| `src/screens/select_art.cpp` | UPDATE | +32/-15 |
| `tests/select_art_test.cpp` | UPDATE | +167/-5 |
| `TODO.md` | UPDATE (edited locally, gitignored) | +1/-1 |
| `.claude/skills/verify/features/song-select.md` | UPDATE (not in the plan; see Deviations) | +1/-0 |

## Deviations from Plan

- **Task 1 and E2E key timing.** The plan's `keys Up Up Up Up Up` straight after `wait-screen Select` landed on `Who`, not `Virtual Emotion`: two of the five taps were lost. The first pair of "before" shots was therefore discarded and retaken (same file names) with a 1.5 s pause after `wait-screen` and `wait:0.6` between taps. The "after" run used the same timing. A separate check (`/tmp/blaze4k-verify/130-keytest`) showed that the default 0.25 s tap spacing is fine once the screen has settled: the lost taps are the ones sent right after the transition.
- **Verify skill gotcha added.** Because of the above, one line was added to `.claude/skills/verify/features/song-select.md` (Gotchas), as the owner's global skill-optimization rule asks. This file is tracked and is outside the plan's file list.
- **`TODO.md` is gitignored** (`.gitignore:27`). Line 51 was ticked locally and confirmed by reading the file.
- **`kDiffNameBudget` comment** is wrapped over two lines to stay near the file's line width. The wording is the plan's.
- **`test_edit_row_geometry` details.** The rects in Part B are computed for a local `box` rect (the plan calls it `row`, which Part A already uses). One extra assertion checks that the manifest's content box lies inside the decoded image before any pixel is read. The two column scans share a small local lambda. The checks and tolerances are the plan's.
- The pre-existing uncommitted edit to `.agents/issues/todo-issues.md` was carried onto the feature branch untouched (not staged, edited or reverted), as the invoking request directed.

## Accepted outcomes and owner flags (from the plan's open questions and risks)

- **Selected Edit row shading (not changed, by decision).** The selected Edit row still has an opaque `#0B1030` body with no outer gold glow and no tab-colour tint. Only its geometry matches the baked selected rows.
- **Name budget (not changed, by decision).** `kDiffNameBudget` stays 118, so `M. Poveromo` is still shown as `M. Pover...` when selected, although the Edit tab now ends at x 161 (164 selected) instead of 150.
- **Edges are not antialiased (not changed).** The Edit row's slanted sides are slightly harder than the baked rows', as before.
- **Sub-pixel ring in small windows (not changed).** Below 1280x720 the 1 px ring is thinner than a pixel and parts of it can drop out, as the old top and bottom strips did.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` | `test_edit_row_geometry` (new): the frame / inner / tab rects and border for an unselected and a selected row; frame top and height equal the row's; every corner of the slanted frame stays inside the row rect; degenerate rows (`0x0`, `10x1`, `564x0`) give no negative size; for all five colours in both states, the frame's and the tab's left and right edges on every image row, the art's height and top, and the tab's top and bottom, against the decoded PNG |
| `tests/select_art_test.cpp` | `test_meter_clearance` (updated): the Edit tab's mid-height edge (161, 164 selected) stays inside the baked tab's cap-top edge, per row state |
| `tests/select_art_test.cpp` | `test_name_margin` (updated): the name's right limit clears the Edit tab's edge at the baseline, per row state |

No test executable was added, so the count stays 51.
