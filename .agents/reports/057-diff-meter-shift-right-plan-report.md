# Implementation Report

**Plan**: `.agents/plans/completed/057-diff-meter-shift-right-plan.md`
**Branch**: `feature/057-diff-meter-shift-right`
**Status**: COMPLETE

## Summary

The song select difficulty meter number and its 10 ticks now sit 16 px further right: `kDiffMeterCentreX` 174 → 190 and `kDiffTickX` 194 → 210. They keep their 20 px spacing, so a two-digit meter now clears the slanted name tab. Normal, selected and code-drawn Edit rows all move together because they share the constants. A new real-font test pins the clearance for meters 1..20 in both row styles. This is presentation only. No timing, judgment or input code changed.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Shift the meter centre and tick x by 16 px. Update the constant and `tick_rect` doc comments | `src/screens/select_art.hpp` | ✅ |
| 2 | Move the `tick_rect` x values +16. Add `test_meter_clearance()`. Update the header comment | `tests/select_art_test.cpp` | ✅ |
| 3 | Tick line 47 (#122) | `TODO.md` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build`) | ✅ |
| Lint (no new warnings in select_art TUs) | ✅ "no new warnings" |
| Tests (sandboxed ctest) | ✅ 51/51 passed (2.92 s) |
| Static: `src/` diff has only the 4 constant lines plus comments | ✅ |
| Static: timing/audio/input/gameplay/assets untouched | ✅ empty |
| Static: `todo-issues.md` not staged | ✅ 0 |
| Guard check: clearance test fails with the old 174/194 constants | ✅ fails at `kDiffMeterCentreX - w * 0.5f >= tab_top + kMinClear` (checked temporarily, then restored) |
| E2E `/verify` screenshots (owner's `songs/`) | ✅ see below |

### E2E evidence (`/tmp/blaze4k-verify/122-meter/evidence/`)

| Shot | What it shows |
|------|---------------|
| `01-select.png` | Anubis. Selected CHALLENGE `10` sits clear of the slanted tab, with a gap before the first tick |
| `02-unselected-10.png` | Anubis. `10` in a normal (28 px) row. Clear on both sides |
| `03-twelve.png` | Delirium. Selected CHALLENGE `12` is clear of the tab and the ticks |
| `04-edit-11-y2z.png` | Y2Z. Selected code-drawn EDIT row `11`, with the same spacing |

In every shot the ticks end well before the best % (`---`). The name, row art and best % are unchanged.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/select_art.hpp` | UPDATE | +5/-4 |
| `tests/select_art_test.cpp` | UPDATE | +36/-4 |
| `TODO.md` | UPDATE (gitignored, edited locally) | +1/-1 |

## Deviations from Plan

- **E2E Edit row:** the plan named an ITG3 Edit 13 (Hasse Mich / VerTex^3). I shot Y2Z's Edit 11 instead. It also uses the code-drawn tab in the selected row, so it shows the same thing. A follow-up shot of VerTex^3 failed because the game window became unviewable (X map state `IsUnviewable`, likely a desktop workspace change) and `import` could not capture it. I did not change the desktop to get it.
- **`TODO.md` is gitignored**, so the tick does not appear in `git diff`.
- **`.agents/issues/todo-issues.md`** had no uncommitted changes at the start of this run, even though the plan said it would. It was not touched.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` | `test_ticks`: tick x values moved to 254/416 (normal) and 268/430 (selected). New `test_meter_clearance`: `kDiffTickX - kDiffMeterCentreX == 20`, `kEditTabWidth <= baked tab top`, and real-font left/right clearance ≥ 4 px for meters 1..20 in `kDiffMeter` and `kDiffMeterSelected` |

## Notes for the owner

- In `03-twelve.png`, Delirium's artist line shows a missing-glyph box. That glyph problem existed before this change, which does not touch it.
- The plan flagged two things as out of scope: `kEditTabWidth` (150) is about 11 px narrower than the baked tab, and three-digit meters are not covered.
