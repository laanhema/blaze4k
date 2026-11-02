# Code Review: feature/072-open-close-packs-song-wheel (re-review r1)

**Scope**: branch `feature/072-open-close-packs-song-wheel` vs merge base `675a6c9` (main), including uncommitted changes (no commits on the branch yet; 11 tracked files modified, 1094 insertions, 181 deletions). Checked against issue #166 (`--issue 166`). This re-reviews the first report (`feature-072-open-close-packs-song-wheel-review.md`, finding L1) after four later changes: the marker on every header (L1 fix, including the edited `wheel_pack.png`), the hint bar word, the all-closed first entry, and the pack banner on a header. Untracked `.agents/plans/...`, `.agents/reports/...` and the earlier review were read as context only. The untracked `.lavish/` directory (session scratch for the Lavish pages) is not part of the change.
**Recommendation**: APPROVE WITH NITS

## Summary

The change is correct and complete for its scope. The wheel cursor is a row cursor, a pack header is selectable, and Confirm on it opens or closes the pack without leaving Select. One pack is open at a time. Every header draws a code-drawn open/closed marker. The hint bar word follows what Confirm does. First entry starts with all packs closed and the first header selected. A selected header shows its pack's banner (or the library fallback), with no song info or preview. Re-entry restores the remembered song or header and the open pack by name or simfile path. The only finding is a Low one: the issue's implementation comment and the implementation report still describe the first version.

Acceptance criteria for #166:
- Cursor can land on a header; Confirm closes an open pack or opens a closed one: **met**. `SelectScreen::update` Confirm branch calls `toggle_pack()` on a header (`src/screens/select_screen.cpp:627`). In my own `/verify` run (`/tmp/rev166b/evidence/`), Down onto the header, Confirm opened it (`03-open.png`), and the log had no `[GameplayScreen] started` until Confirm on a song.
- Closed pack shows only its header; Up/Down skip its hidden songs: **met**. `build_wheel_rows` gives hidden songs `song_row = -1` and no row (`src/screens/select_art.cpp:156`). `01-first.png` shows two closed headers and nothing else, and the unit tests step over both closed headers.
- Open pack holds the highlighted song on entry and the song survives Gameplay, Results, Calibration, Input remap: **met**, with the owner's change. First entry has no highlighted song, so its all-closed state does not contradict the criterion as written, and once a song or header is highlighted `rebuild()` restores it with its pack open (`select_screen.cpp:189-224`). In my run, Gameplay then Escape returned to the same song with Pack One open (`05-back.png`), and Tab then Escape kept it (`07-after-options.png`, `06-options.png`). Results shares the same `enter()` -> `rebuild()` path and was not driven separately. The all-closed first entry is the owner's later decision, given in chat, so it is not on the issue (L2).
- Open/close behaviour follows StepMania's music wheel sections, cited on the issue: **met**. I fetched `stepmania/stepmania` at `825467bc` and checked each cited range. `WheelBase::Select`'s section case (`src/WheelBase.cpp:285-308`) collapses an expanded section or expands a collapsed one and "cannot end the screen". `MusicWheel::SetOpenSection` (`src/MusicWheel.cpp:1439-1467`) keeps one `m_sExpandedSectionName` and skips songs outside it. Lines 1512-1525 re-select by `sLastOpenSection`. The new first-entry claim also holds: `MusicWheel::BeginScreen` (`src/MusicWheel.cpp:138-210`) calls `SetOpenSection("")` when `SelectSongOrCourse()` finds no previous song. The code comment at `select_screen.cpp:135-139` cites that correctly.

## Fix Verification

| ID | Status | Evidence |
|----|--------|----------|
| L1 | fixed | `draw_markers(WheelArt::Pack, theme::color::kPackInk)` (`src/screens/select_art.cpp:798`) now draws the marker on every unselected header, so a closed unselected pack shows a right marker. The baked caret is gone from `assets/theme/cabinet/wheel_pack.png` (documented in `docs/cabinet-theme/README.md:64-68`, manifest note updated at `assets/theme/cabinet/manifest.json:305`). `/tmp/rev166b/evidence/01-first.png` and `03-open.png` show right markers on closed headers and a down marker on the open one. |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

- **L2** issue #166 implementation comment (2026-11-02T10:11:19Z) and `.agents/reports/072-open-close-packs-song-wheel-plan-report.md` — Both still describe the first version: "First entry opens the first pack with song 0 highlighted", "The marker is now drawn only on the selected header; unselected headers keep the baked caret", and "the hint bar still reads ENTER PLAY on a header, and there is no pack banner" as out of scope. **Why:** the owner has since decided that first entry opens all closed, and the hint bar word and the pack banner were added; the issue is the record that a later reader (or `/issue-flow-done`, which links the report) goes by, and it now contradicts the code, the verify doc and the screenshots. **Fix:** post one short update comment on #166 (and a matching "Later changes" note in the implementation report) that records the four owner-requested changes (marker on every header with the baked caret removed from `wheel_pack.png`; the hint bar word OPEN / CLOSE / PLAY; first entry all closed, which follows `MusicWheel::BeginScreen`; the pack banner on a selected header, falling back to the library fallback banner).

**Noted, not a finding**:
- The pack banner and the hint bar word are not in #166's acceptance criteria. They were requested by the owner (in the Lavish review) and are small and self-contained, so they are in scope here.
- No recorded-quads test covers the pack banner being drawn: the test fixture banner files are fake bytes that cannot decode. The choice of path is a pure accessor, `cursor_banner_path()`, and `test_header_banner` asserts it (header with a banner, header without, song row); the draw path is covered by the real-window run (`02-header-banner.png`, `01-first.png`).
- The sanitizer job (CI job `sanitizer`) is a separate command; I ran its select tests myself (see Validation Results).
- The wheel lists packs in scan order, not alphabetically (`01-first.png` shows Pack Two first). Sorting is #158.
- `.lavish/` (the Lavish pages and their screenshots) is untracked and not in `.gitignore`: delete it or ignore it before committing.
- Remap re-entry and a vanished remembered song's pack (falls back to all closed, does not reopen the pack) rest on unit tests (`test_pack_state_survives_reenter`, the vanished-library test) and the implementer's captures.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (the CI command; host, Release, `-DBLAZE_WERROR=ON`) step 4 |
| Warnings | NONE | the same `-DBLAZE_WERROR=ON` build; the script fails on any project warning |
| Lint | PASS | `scripts/check.sh` steps 1-2 (`scripts/lint.sh`, `scripts/format.sh --check`) |
| Tests | PASS | `scripts/check.sh` step 5: 100% passed, 53 of 53, on the silent null audio device (the default; every assertion runs); no skipped or env-guarded tests seen. Sanitizer: `cmake --build --preset asan-ubsan` then `ctest --preset asan-ubsan -R select`: `select_screen_test` and `select_art_test` passed, 2 of 2. |
| UI verification | PASS | `.claude/skills/verify/scripts/b4k.sh` at 1280x720 (the fixed window size), two scratch packs `/tmp/blaze4k-verify/songs072b` ("Pack One" with a generated `banner.png`, "Pack Two" without one). Driven: first entry, Down onto Pack One's header, Confirm open, Down Down to a song, Confirm into Gameplay, Escape back, Tab options overlay, Escape. Captures `/tmp/rev166b/evidence/01-first.png` to `07-after-options.png`, all viewed; `game.log` shows `Title -> Select`, `[GameplayScreen] started 'Glacier:Groove;Part 1' Medium 4`, `Select -> Gameplay`, `Gameplay -> Select`, no errors. The Input Remap screen, which shares `wheel_pack`, rests on the implementer's capture `/tmp/blaze4k-verify/072-fix/run/evidence/06-remap.png` (taken after the last edit of `setup_art.cpp` and the PNG, and viewed by me: KEYBOARD header with its down marker). |

## What's Good

- The marker and the hint word each have one definition (`wheel_marker_quad` / `draw_pack_marker`, `confirm_hint_word` / `select_hint_items`), and the Input Remap table reuses the marker instead of keeping the art's baked caret.
- The `wheel_pack.png` edit is minimal and reproducible: 574 pixels inside the caret's box, size and mode unchanged, with the recipe recorded in `docs/cabinet-theme/README.md`. No other asset, checksum or import script depends on that file.
- `cursor_banner_path()` returns the same fallback a bannerless song resolves to, and `TextureCache::get("")` returns null, so a missing banner draws the theme fallback and never a stale song banner.
- Navigation edge cases are safe with the cursor on a header: `selected_song()` is null, so `move_chart`, `selected_chart()` and the preview request are no-ops or stop the preview.
- Restore-by-name keeps no pointers, so a rescanned or changed library cannot dereference stale entries.
- The implementer ran mutation checks (hint always Play, first entry opening a pack, pack banner disabled), each failing a test.

## Recommendation

Mergeable. Fix L2 (an issue comment and a note in the implementation report) before or with `/issue-flow-done 166`; `/fix-findings .agents/reviews/feature-072-open-close-packs-song-wheel-review-r1.md` can do it.
