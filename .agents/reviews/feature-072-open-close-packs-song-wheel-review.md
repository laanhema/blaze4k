# Code Review: feature/072-open-close-packs-song-wheel

**Scope**: branch `feature/072-open-close-packs-song-wheel` vs merge base `675a6c9` (main), including uncommitted changes (no commits on the branch yet; 7 tracked files modified). Checked against issue #166 (`--issue 166`).
**Recommendation**: APPROVE WITH NITS

## Summary

The Select wheel cursor becomes a row cursor over the visible rows. Pack headers are always visible and selectable, and only the one open pack shows its songs. Confirm on a header toggles the pack without leaving Select. The open pack and the cursor target are remembered by pack name or simfile path and restored in `rebuild()`. The code is small and consistent with the surrounding pattern (pure helpers in `select_art`, state in `SelectScreen`). The tests assert hand-computed values and the recorded-quads path. Untracked `.agents/plans/...` and `.agents/reports/...` files were read as context only.

Acceptance criteria for #166:
- Cursor can land on a header; Confirm closes an open pack or opens a closed one: **met**. `SelectScreen::update` Confirm branch calls `toggle_pack()` on a header (`src/screens/select_screen.cpp:596`). `test_pack_header_toggle` covers it, and my own `/verify` run showed the gold header bar, the collapse, no `[GameplayScreen] started` in the log, and the other pack opening.
- Closed pack shows only its header; Up/Down skip its hidden songs: **met**. `build_wheel_rows` gives hidden songs `song_row = -1` and no row (`src/screens/select_art.cpp:152`). Test: Down/Up with both packs closed only visit the two headers. Screenshot `03-closed.png` agrees.
- Open pack holds the highlighted song on entry and the song survives Gameplay, Results, Calibration, Input remap: **met**. `rebuild()` branch (a) re-finds the song by simfile path and opens its pack (`select_screen.cpp:172`). `test_pack_state_survives_reenter` covers Gameplay, Calibration, Input remap and Title. In my `/verify` run, Select -> Gameplay -> Back gave a screenshot byte-identical to the one before. Results goes through the same `enter()` -> `rebuild()` path.
- Open/close behaviour follows StepMania's music wheel sections, cited on the issue: **met**. The cited `WheelBase::Select`, `MusicWheel::SetOpenSection` and the post-toggle selection are in the owner's implementation comment on #166 (one pack open at a time; the cursor stays on the header). The code matches that. I did not re-fetch the upstream source.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L1** `src/screens/select_art.cpp:785` — The open/closed marker is drawn only on the selected header (`draw_markers(WheelArt::SelectedPack, ...)`), so every unselected header shows the down caret baked into `wheel_pack.png`, open or closed. **Why:** a closed pack that the cursor is not on looks open (visible in my capture `/tmp/rev166/run/evidence/03-closed.png`: PACKA, closed, shows a down caret while PACKB, selected and closed, shows a right triangle), which weakens the open/close affordance on a long wheel of packs. The implementation report and `.claude/skills/verify/features/song-select.md` disclose it as a deviation. **Fix:** draw the marker on every header over a cleaned-up `wheel_pack` art without the baked caret, or leave it and file a follow-up issue so the owner can decide.

**Noted, not a finding**: the hint bar still reads "ENTER PLAY" on a header (`kSelectHintItems`, `select_art.cpp:46`), and there is no pack banner on a header (the left panel shows the fallback banner). Both are listed as out of scope in the plan and in the owner's comment on #166, and no acceptance criterion covers them.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (the CI command, host, Release, `-DBLAZE_WERROR=ON`) step 4 |
| Warnings | NONE | `-DBLAZE_WERROR=ON` build in `scripts/check.sh` passed; the script fails the build on any project warning |
| Lint | PASS | `scripts/check.sh` steps 1-2 (`scripts/lint.sh`, `scripts/format.sh --check`) |
| Tests | PASS | `scripts/check.sh` step 5: 100% passed, 53 of 53, on the silent null audio device (the default; every assertion runs); no skipped or env-guarded tests seen. The sanitizer run (CI job `sanitizer`) was not re-run by me; the report says it passed for the select tests. |
| UI verification | PASS | `.claude/skills/verify/scripts/b4k.sh` on two copies of the fixture pack at 1280x720 (the fixed window size): first entry, Up onto the header, Confirm collapse, Down + Confirm opening the other pack, Down Down + Confirm into Gameplay and Escape back (screenshot identical to before), Tab + Escape. Captures in `/tmp/rev166/run/evidence/`. Calibration and Input remap re-entry rely on the unit test and the implementer's captures (`/tmp/blaze4k-verify/072-packs/evidence/`, which predate the last source touch, so they are not the gate). |

## What's Good

- Pure layer plus thin screen glue: `build_wheel_rows`, `wheel_marker_quad` and `WheelArt::SelectedPack` are testable without GL.
- Restore-by-name logic (`select_screen.cpp:117-216`) is defensive: it stores the key, not pointers, so a changed library cannot dereference stale entries, and it falls back to the first pack.
- The toggle resets the slide (`scroll_start_ = 0`) so a changed row set never animates wrong rows.
- Tests use hand-computed geometry and rows, cover degenerate cases (empty and all-hidden library, a vanished remembered target), and check the render path with the headless recorder.

## Recommendation

Mergeable. L1 is an owner design decision; optionally file a follow-up for it and for the header-specific hint bar. `/fix-findings .agents/reviews/feature-072-open-close-packs-song-wheel-review.md` if the owner wants L1 fixed.
