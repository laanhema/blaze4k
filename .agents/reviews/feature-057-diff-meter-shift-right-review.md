# Code Review: feature/057-diff-meter-shift-right

**Scope**: branch `feature/057-diff-meter-shift-right` vs merge base `324ccad` (all changes uncommitted), issue #122
**Recommendation**: APPROVE WITH NITS

## Summary

This change moves the song select difficulty meter number and its 10 ticks 16 px right (`kDiffMeterCentreX` 174 → 190, `kDiffTickX` 194 → 210). It updates the `tick_rect` expectations and adds a real-font clearance test, `test_meter_clearance`. The change is small and correct. All five acceptance criteria are met. I checked the hard-coded tab-edge numbers in the new test against the baked PNGs, and they are accurate. The two findings are Low and only affect how precisely the new test guards the layout. Also in scope: a one-paragraph note in `.claude/skills/verify/SKILL.md` and the plan moving to `completed/`. Both are fine.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L1** `tests/select_art_test.cpp:607` — The Edit-tab guard compares `kEditTabWidth` (the code-drawn tab's width at mid-height) with `kTabTopNormal` (the baked tab's edge at the cap top). It ignores the `theme::skew::kRows` slant that `draw_difficulty_row_art` applies through `skewed_quad`. **Why:** today the code-drawn tab's edge at the cap top is 150 + 0.213 × 12 ≈ 152.6 (normal) and 150 + 0.213 × 16 ≈ 153.4 (selected), so the guard holds with room to spare. But if `kEditTabWidth` were widened to about 160, the guard would still pass while the code-drawn tab's cap-top edge (≈ 163.4 normal, ≈ 163.7 selected) sits closer to a 28 px `20` than the 4 px `kMinClear` the test promises. **Fix:** compare the slanted edge instead, for example `art::kEditTabWidth + theme::skew::kRows * (row_h * 0.5f - cap_top_y) <= tab_top` for each row height. Or add the Edit tab as a third tab-edge case in the loop.

- **L2** `tests/select_art_test.cpp:601` — `kTickInkInset = 3` is the tick's ink inset at mid-height. The tick sprite is slanted: in `diff_tick.png` the left ink edge is 5 px in at the top and 1 px in at the bottom. So near the digits' baseline, the first tick's ink is about 1.5 px left of the value the test assumes. **Why:** the right-side check (line 618) claims at least 4 px between the number and the tick, but the real minimum near the baseline is about 2.5 px. This is partly hidden because `measure()` returns the advance width, which includes the right side bearing. The `/verify` shot `03-twelve.png` looks clean, so nothing is visibly wrong today. **Fix:** use the tick's inset at the digits' baseline (about 1.5) in place of the mid-height value. Or rename the constant and comment so it is clear the margin is approximate.

**Noted, not a finding**: `kEditTabWidth` (150) is about 11 px narrower than the baked tab, and three-digit meters are not covered. The plan scoped both out explicitly. Delirium's missing-glyph box in the artist line was already there before this change.

## Acceptance Criteria (#122)

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Number and ticks move right by the same amount | ✅ | Both +16 (`src/screens/select_art.hpp:68-69`). The spacing is pinned by `TEST_CHECK(kDiffTickX - kDiffMeterCentreX == 20)` |
| A selected two-digit meter clears the tab and the first tick | ✅ | Real-font check for meters 1..20 in both styles. Screenshot `03-twelve.png` (re-inspected zoomed) |
| Last tick ends well before best %; gap check passes; `tick_rect` checks updated | ✅ | Last tick right = 392, limit 487. Values 254/416 and 268/430 are correct for `kDiffListX` 44 + selected shift 14 |
| Same for normal, selected and code-drawn Edit rows | ✅ | All three paths read the shared constants (`select_art.cpp:226`, `:602`). Edit row shown in `04-edit-11-y2z.png` |
| `/verify` screenshot with a two-digit meter | ✅ | `/tmp/blaze4k-verify/122-meter/evidence/01–04` |

Tab-edge numbers checked against the PNGs: `diff_row_hard.png` puts the tab edge at layout x 163.5 at y≈10 (165.0 at the top, 156.5 at the bottom). `diff_row_hard_selected.png` puts it at 167.5 at y≈10. The test's constants match those measurements.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `cmake --build build` (host, Release) |
| Warnings | NONE | Recompiled `src/screens/select_art.cpp` (`blaze4k_core` flags) and `tests/select_art_test.cpp` (`select_art_test` flags) to `/dev/null` with `-Wall -Wextra -Wpedantic`: 0 warnings |
| Lint | N/A | No linter configured |
| Tests | PASS | 51/51, 0 skipped. Run under `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build`, so audio falls back to the null device; every assertion still ran. `select_art_test` prints `meter clearance ok` |

## What's Good

- The `src/` change is minimal: two constants plus accurate comments, including the `tick_rect` doc comment.
- The new test measures with the real font, not guessed widths. Per the implementation report, it fails with the old 174/194 constants, so it does guard against a regression.
- The 16 px offset is backed by a measurement (the slanted tab's cap-top edge), not chosen by eye. The header comment records why.
- The deviations are reported honestly: Y2Z Edit 11 replaced the planned VerTex^3 shot, and the implementer did not move the owner's windows.

## Recommendation

Ready to merge. L1 and L2 are optional ways to tighten the test and can go in this branch or be left as they are.
