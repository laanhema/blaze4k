# Implementation Report

**Plan**: `.agents/plans/completed/059-diff-name-left-margin-plan.md`
**Branch**: `feature/059-diff-name-left-margin`
**Issue**: #126 ([TODO-33] Add left margin to the difficulty name text in song select)
**Status**: COMPLETE

## Summary

The difficulty name in the song select rows now starts further right inside its coloured tab: `kDiffNameX` 15 → 21 (+6 px, unselected rows) and a new `kDiffNameSelectedX = 25` (+10 px, selected row). `kDiffNameBudget` dropped 128 → 118, so the name's right limit stays at x 143 in the selected row (139 in the others). The draw site picks the x by row state, the same shape as `kWheelSongTextX` / `kWheelSelectedTextX`. The meter number, ticks, best % and all row art are untouched. Presentation only: no clock, judgment, input or audio code changed.

Nothing is committed. The changes are uncommitted on the branch.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Capture the "before" screenshot | `/tmp/blaze4k-verify/126-before/evidence/` | ✅ |
| 2 | Move the name constants | `src/screens/select_art.hpp` | ✅ |
| 3 | Use the selected offset at the draw site | `src/screens/select_art.cpp` | ✅ |
| 4 | Pin the margin in the tests | `tests/select_art_test.cpp` | ✅ |
| 5 | TODO tick | `TODO.md` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j`) | ✅ |
| Lint (no linter; zero compiler warnings after touching both TUs) | ✅ (0 warnings in the whole build log) |
| Tests (sandboxed ctest) | ✅ (51/51 passed, 2.91 s) |
| `select_art_test` run directly (sandboxed) | ✅ (`- name margin ok.` printed) |
| Static: only the name constants and one draw expression change in `src/` | ✅ |
| Static: `kDiffMeterCentreX` / `kDiffTickX` / `kDiffBestRight` unchanged | ✅ (grep exit 1, as expected) |
| Static: `src/timing`, `src/audio`, `src/input`, `src/gameplay`, `src/render`, `assets` untouched | ✅ (empty diff stat) |
| Static: `.agents/issues/todo-issues.md` not staged | ✅ (0; nothing is staged) |
| E2E 1: automated (`test_name_margin`, `test_ticks`, `test_meter_clearance`, `test_render_smoke`) | ✅ |
| E2E 2: `/verify` screenshots (AC 5), owner's packs, 1280x720 | ✅ (5 of 5 "after" shots plus the "before" shot captured and read) |

### Screenshot measurements (window px = reference px)

Measured along each row's mid line in `01-challenge-selected.png`, before against after:

| Row | Tab fill starts | Name ink before | Name ink after | Shift | Gap before | Gap after |
|-----|-----------------|-----------------|----------------|-------|------------|-----------|
| Selected (`CHALLENGE`, y 398) | x 72 | x 74 | x 84 | +10 | 2 px | 12 px |
| Unselected (`HARD`, y 456) | x 55 | x 60 | x 66 | +6 | 5 px | 11 px |

These match the plan's optional numeric check (72 / 84 / 74 and 55 / 66 / 60, ±1 px).

A pixel diff of the two `01-challenge-selected.png` shots is confined to x 59..188, y 390..626, which is the name text of the five rows. Nothing right of x 200 in the difficulty list differs, so the meter numbers, ticks and best % are pixel-identical.

### Screenshots

| File | Shows |
|------|-------|
| `/tmp/blaze4k-verify/126-before/evidence/01-challenge-selected.png` | Before. Anubis: `CHALLENGE` selected with its `C` against the gold frame; `HARD`, `MEDIUM`, `EASY`, `BEGINNER` unselected and tight to the tab's left edge |
| `/tmp/blaze4k-verify/126-after/evidence/01-challenge-selected.png` | After. Same state: `CHALLENGE` clear of the gold frame, the four unselected names with a visible left margin |
| `/tmp/blaze4k-verify/126-after/evidence/02-beginner-selected.png` | After. Anubis: `BEGINNER` selected, `CHALLENGE` unselected. Both end well inside the tab |
| `/tmp/blaze4k-verify/126-after/evidence/03-edit-unselected.png` | After. Virtual Emotion: `CHALLENGE` selected; Edit rows `BlueChaos`, `Phrekwenci`, `Rynker` unselected, starting at the same x as `HARD` |
| `/tmp/blaze4k-verify/126-after/evidence/04-edit-selected.png` | After. Virtual Emotion: `BlueChaos` (Edit) selected, inside the Edit tab |
| `/tmp/blaze4k-verify/126-after/evidence/05-edit-truncated.png` | After. Dawn (Perpetual Mix): Edit row selected and shown as `M. Pover...`, ending inside the Edit tab |

`game.log` for each run is in the same `evidence/` directory.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/select_art.hpp` | UPDATE | +6/-2 |
| `src/screens/select_art.cpp` | UPDATE | +2/-2 |
| `tests/select_art_test.cpp` | UPDATE | +44/-0 |
| `TODO.md` | UPDATE (edited locally, gitignored) | +1/-1 |

## Deviations from Plan

- `TODO.md` is gitignored (`.gitignore:27`), so the plan's Task 5 check `git diff --stat -- TODO.md` shows nothing. Line 49 was ticked locally and confirmed by reading the file.
- The `kDiffNameBudget` comment went on its own line above the constant instead of trailing it, to stay inside the file's line width. The wording is the plan's.
- The pre-existing uncommitted edit to `.agents/issues/todo-issues.md` was carried onto the feature branch untouched (not staged, edited or reverted), as the invoking request directed.

## Accepted outcomes and owner flags (from the plan's open questions)

- **`M. Poveromo` truncates while selected.** The Edit name on ITG3 / Dawn fits today and is now shown as `M. Pover...` when its row is selected (shot 05). Accepted as planned: the issue's technical notes expect the budget to drop by about the shift. Edit rows are not special-cased.
- **Edit-row art misalignment (out of scope, not changed).** The code-drawn Edit tab sits 11–14 px left of the baked tabs, so Edit names get a wider left gap than the baked rows (visible in shots 03 to 05) and the tightest right limit. Aligning it would make the gaps equal and free about 10 px for names.
- **Baked-texture offset (out of scope, not changed).** The baked row textures place their art about 10 px right of the mock inside the content box. This is the root cause of the cramping here and in #122; both compensate in the text and meter positions.

No follow-up issues were filed for either.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` | `test_name_margin()`: left gap to the baked tab fill at the cap top is at least 8 px in the normal and the selected row; the selected gap is at least the normal gap; `x + kDiffNameBudget` stays at or before x 143 for both offsets; x 143 plus 4 px clears the code-drawn Edit tab's right edge at the baseline; every standard name (`BEGINNER`, `EASY`, `MEDIUM`, `HARD`, `CHALLENGE`, `EDIT`) fits before x 143 at its own x with the real fonts in both styles; the limit plus 4 px clears every meter number 1..20 in the selected style |

No test executable was added, so the count stays at 51.
