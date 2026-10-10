# Code Review: feature/059-diff-name-left-margin

**Scope**: branch `feature/059-diff-name-left-margin` vs merge base `01af2bf` (no commits on the branch; all changes uncommitted), issue #126
**Recommendation**: APPROVE WITH NITS

## Summary

This change moves the song select difficulty name right inside its coloured tab: `kDiffNameX` 15 → 21 for unselected rows, a new `kDiffNameSelectedX = 25` for the selected row, and `kDiffNameBudget` 128 → 118 so the name's right limit stays at x 143. The draw site picks the x by row state, and a new `test_name_margin` pins the gaps and the right limit. The change is small and correct, and all five acceptance criteria are met. I re-measured the hard-coded tab edges against the baked PNGs and the gaps against the before and after screenshots; the numbers in the code, the test and the issue comment hold. The two findings are Low and concern only how well the new test guards the change.

Reviewed in full: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`, `tests/select_art_test.cpp`. Read for context: the plan and the implementation report (both untracked, in scope). Excluded on request: `.agents/issues/todo-issues.md` (unrelated uncommitted edit, left untouched).

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L1** `src/screens/select_art.cpp:590` — The choice of name x by row state (`selected ? kDiffNameSelectedX : kDiffNameX`) exists only inside the GL draw call, so no test observes it. **Why:** this expression is the one behaviour change in `src/`, and `test_name_margin` checks the two constants, not which row gets which. Passing `kDiffNameX` for both rows, or swapping the two branches, leaves 51/51 green, so AC 2 is guarded only by the manual `/verify` screenshot. **Fix:** move the expression into a pure helper next to `tick_rect`, for example `difficulty_name_x(const Rect& row, bool selected)`, call it from the names loop, and assert both results in `test_name_margin`. Optional: the wheel's text x (`src/screens/select_art.cpp:651-653`) has the same inline shape today.

- **L2** `tests/select_art_test.cpp:674` — The real-font loop asserts `x + measure(name) <= kNameRight`, which adds nothing to the checks around it and does not test what the draw site does. **Why:** for the selected style it is the same inequality as `test_labels` at line 592 (`measure <= kDiffNameBudget`, because 25 + 118 = 143). For the normal style it allows a 122 px name, while `draw_difficulty_rows` truncates at 118. Nothing is wrong today (`CHALLENGE` measures 99.1 / 107.2), but the loop would still pass for a normal-style name that is shown truncated. **Fix:** assert the drawn result in the loop, `text.truncate(name, style, art::kDiffNameBudget) == name`, and keep lines 661-662 for the right limit.

**Noted, not a finding**:
- The Edit name `M. Poveromo` (ITG3 / Dawn) fit before and is shown as `M. Pover...` while its row is selected. The plan accepted this (Open Question 1) and the issue comment flags it to the owner. In `05-edit-truncated.png` the selected Edit row has a 26 px left gap and 18 px of free tab right of the ellipsis, so the owner may want to revisit it together with the Edit row alignment below.
- The code-drawn Edit row sits 11–14 px left of the baked rows, and the baked textures place their art about 10 px right of the mock. The plan scoped both out. No follow-up issue was filed, and no acceptance criterion asks for one.
- The AC 5 screenshots exist only under `/tmp/blaze4k-verify/`, which does not survive a reboot. The issue comment carries the measurements but not the images.
- The `TODO.md` tick is not in the diff because the file is gitignored.

## Acceptance Criteria (#126)

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Name starts further right in every row, about 6–10 px | Met | +6 (`kDiffNameX` 21) and +10 (`kDiffNameSelectedX` 25), `src/screens/select_art.hpp:69-70`. Before/after pixel diff shows the names of all five rows moved |
| Selected row's gap to the slanted edge at least as large as unselected | Met | Measured in `126-after/.../01-challenge-selected.png`: selected 12 px at mid-height and 11 px at the cap top, unselected (`HARD`) 11 px and 10 px. Before: 2 / 1 px and 5 / 4 px. Pinned by constants at `tests/select_art_test.cpp:658` |
| Longest names fit the tab and stay clear of the meter number | Met | Right limit 143 (selected) / 139 (normal), Edit tab edge 148.3 at the baseline, and the real-font test confirms every meter 1..20 starts at or right of x 147. `02-beginner-selected.png`, `04-edit-selected.png` and `05-edit-truncated.png` show `CHALLENGE`, `BEGINNER` and Edit names inside the tab |
| Meter number, ticks and best % unchanged | Met | `kDiffMeterCentreX`, `kDiffTickX`, `kDiffBestRight` untouched. Pixel diff of the two `01-challenge-selected.png` shots is confined to x 59..188, y 390..626 (name text only) |
| `/verify` screenshot shows the margin on a selected and an unselected row | Met | `/tmp/blaze4k-verify/126-before/evidence/01-challenge-selected.png` and `/tmp/blaze4k-verify/126-after/evidence/01..05-*.png`, all read. File times show the after shots were taken after the last rebuild of `libblaze4k_core.a` |

Tab-edge constants checked against the textures (content box from `manifest.json`, 2x): `diff_row_hard.png` fill starts at x 12.5 at the cap top (11.0 at mid, 9.5 at the baseline); `diff_row_hard_selected.png` fill starts at x 16.0 at the cap top (14.0 at mid, 12.5 at the baseline), just inside a 2 px gold frame. Both match `kTabLeftNormal` / `kTabLeftSelected` and the header comment at `src/screens/select_art.hpp:67-68`.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build -j` (host, Release, GCC); tree was already current with the working copy |
| Warnings | NONE | The incremental build recompiled nothing, so `src/screens/select_art.cpp` (`blaze4k_core` flags) and `tests/select_art_test.cpp` (`select_art_test` flags) were recompiled to `/dev/null` with `-O3 -std=c++20 -Wall -Wextra -Wpedantic`: 0 warnings. `tests/select_screen_test.cpp` (reads `kDiffNameBudget`) also 0 |
| Lint | N/A | No linter configured |
| Tests | PASS | 51 passed, 0 failed, 0 skipped. Run as `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; the sandbox hides the sound device, so audio tests use the null device and every assertion still runs. `select_art_test` run directly in the same sandbox prints `name margin ok` |

## What's Good

- The offsets come from measurement, not from taste: the plan shows why one shared offset cannot meet AC 2 (the selected row's fill starts 3.5 px further right at the cap top), and the textures confirm it.
- The `src/` change is three constants and one expression, with comments that say where each number comes from and which issue moved it.
- The right limit is kept at 143, so the change cannot push a name out of the Edit tab or towards the meter number, and the test pins that.
- The implementation report is honest about the one visible cost (`M. Poveromo` truncating while selected) and about what was left out of scope.
- No timing, audio, input, gameplay, render or asset file is touched.

## Recommendation

Ready to merge. L1 and L2 are optional test-tightening changes that can go in this branch or be left. The owner should confirm the accepted `M. Pover...` truncation when looking at the screenshots.
