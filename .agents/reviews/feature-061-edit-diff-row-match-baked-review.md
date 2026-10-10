# Code Review: feature/061-edit-diff-row-match-baked

**Scope**: branch `feature/061-edit-diff-row-match-baked` vs merge base `299c2d3` (no commits on the branch; all changes uncommitted), issue #130
**Recommendation**: APPROVE WITH NITS

## Summary

This change puts the code-drawn Edit difficulty row on the baked rows' geometry. A new pure helper, `select_art::edit_row_rects(row, selected)`, returns the frame, inner and tab rects from four named constants (insets 10 / 12 selected, ring 1 / 2 selected), and `draw_difficulty_row_art` draws the body, a four-sided ring and the 150 px tab from them. The gold outset (`kEditSelectedOutset`) is removed. The change is small and correct, and all five acceptance criteria are met. I checked the constants against all ten `diff_row_*.png` textures with a separate script and compared the Edit rows with the baked rows in the before and after screenshots, scanline by scanline. The numbers in the code, the test and the issue comment hold. The two findings are Low and concern only how well the tests guard the change.

Reviewed in full: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`, `tests/select_art_test.cpp`, `.claude/skills/verify/features/song-select.md`. Read for context: the implementation report in full and the plan in part (both untracked, in scope). Excluded on request: `.agents/issues/todo-issues.md` (unrelated uncommitted edit, left untouched).

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L1** `src/screens/select_art.cpp:555` — The ring's two side strips, and the choice of which rect feeds which quad, exist only inside the GL draw call, so no test observes them. **Why:** `test_edit_row_geometry` holds `edit_row_rects` to the PNGs, but the draw site builds the left and right strips from its own `Rect{...}` expressions (lines 555 and 558), and no test reads rendered pixels. Drawing the body from `row` instead of `g.frame`, or misplacing the right strip, leaves 51/51 green, so at the draw site AC 1 and AC 2 are guarded only by the `/verify` screenshots. **Fix:** add the two strips to `EditRowRects` (for example `left` and `right`) so the helper is the single source for every quad, and assert them in Part A of the test: `left.x == frame.x`, `right.x + right.w == frame.x + frame.w`, both `border` wide and as tall as `inner`. Optional.

- **L2** `tests/select_art_test.cpp:902` — The scan offsets are hard-coded in image px (`c.y + 8` here, `c.x + 160` at line 952), while the rest of the test converts with `px = 1 / texture_scale()`. **Why:** the comments say "4 reference px" and "reference x 80", which is true only at the manifest's current `texture_scale` of 2. After a re-bake at another scale the scan moves without notice; at 1x the x 160 column leaves the slanted tab in the lower part of the row, so the tab-height check would fail for the wrong reason. **Fix:** derive both from the scale (`std::lround(4.0f / px)` and `c.x + std::lround(80.0f / px)`), or assert `tex.texture_scale() == 2.0f` next to `px`.

**Noted, not a finding**:
- The selected Edit row is still plainer than the baked selected rows: opaque body, no outer glow, no tint. This is visible in `02-edit-selected.png` next to the selected `CHALLENGE` row in `01-edit-unselected.png`. The issue's technical notes scope it out ("the fix matches geometry only").
- `kDiffNameBudget` stays 118, so `M. Poveromo` is still shown as `M. Pover...` while selected, although the Edit tab now ends 11 to 14 px further right. The plan decided this and the issue comment flags it to the owner.
- Below 1280x720 the 1 px ring is thinner than a pixel and parts of it can drop out. The old top and bottom strips behaved the same way, and the implementation report records it as accepted.
- The new gotcha line in the verify recipe says key taps sent right after the Title → Select transition can be lost with the owner's 221-song library. The line is a harness workaround. Whether the game itself misses input in the first frames of song select was not investigated, here or in the implementation report, and it is outside this change.
- The AC 5 screenshots exist only under `/tmp/blaze4k-verify/`, which does not survive a reboot. The issue comment carries the measurements but not the images.
- The `TODO.md` tick is not in the diff because the file is gitignored.

## Acceptance Criteria (#130)

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Unselected Edit row has the same visible width, height and left/right edges as an unselected baked row | Met | Frame `{row.x + 10, row.y, row.w - 20, row.h}` (`src/screens/select_art.cpp:243`). In `130-after/.../01-edit-unselected.png`, `BlueChaos` (Edit) against `HARD` on all 44 scanlines: tab left and right edges and the row's right end agree within 1 px (mid-height: tab 55..204, right end 597 for both). Before: the Edit row started 11 px further left and ran 10 px past `HARD` on the right |
| Selected Edit row has the same position and visible size as any other selected row, including its gold frame | Met | Gold ring inside the row rect (inset 12, 2 px). `BlueChaos` selected in `02-edit-selected.png` against `CHALLENGE` selected in `01-edit-unselected.png`: gold extent agrees within 1 px on all 52 scanlines (mid-height 70..609 for both) and no gold above or below the 52 px row. Before: 14 to 15 px wider on each side and 4 px taller |
| Edit tab starts and ends at the same x as the other rows' tab; name, meter, ticks and best % sit where they do in the other rows | Met | Tab is the first 150 px of `inner` (x 11..161 at mid-height, 14..164 selected). Screenshot tab edges match the baked rows within 1 px (above). `difficulty_row_rect` and every `kDiff*` offset are unchanged, and all rows share them |
| A test in `tests/select_art_test.cpp` checks the Edit row's geometry against the baked rows' | Met | `test_edit_row_geometry` (`tests/select_art_test.cpp:820`) decodes all ten PNGs and holds the frame and tab sides, the height and the tab's top and bottom to them within 1 px (0.5 px for the vertical checks). See L1 and L2 for its limits |
| A `/verify` screenshot of a song with an Edit chart shows the Edit row lined up, selected and unselected | Met | `/tmp/blaze4k-verify/130-after/evidence/01-edit-unselected.png`, `02-edit-selected.png`, `03-edit-selected-lower.png` (`Virtual Emotion`, 1280x720), all read. File times: `select_art.cpp` saved 20:41:05, `build/blaze-4k` linked 20:41:10, shots taken 20:41:35 to 20:41:39, so they show the reviewed code |

Constants checked against the textures with a separate script (content box from `manifest.json`, 2x, every image row from 4 px below the top to 4 px above the bottom): for all five colours the worst frame-side error is 0.34 px unselected and 0.31 px selected, and the worst tab-side error is 0.50 to 0.54 px. The art fills the content box top to bottom (44 px, 52 px selected). This agrees with the implementation report's figures and with the header comment at `src/screens/select_art.hpp:83-92`.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build -j` on the host (existing Release tree); everything up to date, no errors |
| Warnings | NONE | Recompiled `select_art.cpp`, the seven other `src/screens` sources that include `select_art.hpp`, and `select_art_test.cpp`, `select_screen_test.cpp`, `setup_art_test.cpp` to `/dev/null` with the flags from each target's `flags.make` (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`): no output |
| Lint | N/A | The project has no linter |
| Tests | PASS | 51 passed, 0 failed, 0 skipped. Run as `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; inside the sandbox `/dev/snd` and `/run/user/<uid>` are empty, so audio tests use their null-device fallback and still run every assertion. `select_art_test` run directly in the same sandbox prints `Edit row geometry vs the baked rows ok.` |

## What's Good

- The geometry lives in one pure helper with a clear contract, so the draw code has no magic numbers and the test can hold it to the art.
- The test measures the real PNGs for all five colours in both states. It does not repeat constants: an independent probe gives the same sub-pixel errors, and the implementation report shows the test fails when the tolerance drops to 0.5 px.
- Degenerate rows are handled: sizes are clamped, the border is capped at half the smaller side, and the draw code skips the ring when the border is 0, so there is no division by zero.
- The body is drawn under the opaque ring and the strips share exact vertex coordinates with the tab, so no seam can open at fractional scales.
- The existing guards in `test_meter_clearance` and `test_name_margin` were updated for both row states, and the `kDiffNameBudget` and header comments were kept truthful.
- The scope is tight: two source files and one test file, no layout constant shared with the baked rows changed.

## Recommendation

Ready to commit and merge. L1 and L2 are optional test-hardening nits and can be fixed now with `/fix-findings` or left as they are.
