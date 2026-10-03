# Code Review: feature/045-select-screen-cabinet (#94), re-review r1

**Scope**: Branch `feature/045-select-screen-cabinet` vs `main`, commits `f2b7488` (restyle) and `8723939` (fixes for the first review). This pass focuses on the fix commit and checks each finding from `feature-045-select-screen-cabinet-review.md`. `.agents/stories/todo-stories.md` is excluded (unrelated owner edit).
**Recommendation**: APPROVE

## Summary

The fix commit resolves both earlier findings. Sliding wheel rows that would leave reference y 0..720 are now skipped (art and text), so the letterbox bands on 16:10 and 4:3 windows stay clean. A full-sweep pure test pins this. The TrueType-coverage display-text overloads now have a direct test with the real headless `TextRenderer`. The fix adds no new issues. Build, warnings gate, the 48/48 sandboxed tests and the headless select smoke all pass.

## Fix Verification

| # | Prior finding | Fix | Status |
|---|---------------|-----|--------|
| Medium 1 | Sliding wheel rows leak into the letterbox bands on 16:10 / 4:3 windows (`select_screen.cpp:617-621`, `select_art.cpp:539-578`) | Took option (a), culling. New pure helpers in `select_art.cpp:173-190`: `wheel_slide_range` (the old inline `from`/`to` rule), `wheel_slide_rect` (slide offset, selected bar fixed) and `wheel_row_in_column` (`y >= 0 && y + h <= 720`). `draw_wheel` (`select_art.cpp:558-607`) skips culled rows in all three passes (sliding art, text, selected art). `SelectScreen::render` (`select_screen.cpp:612-626`) uses `wheel_slide_range`. The view array is sized from `kWheelMaxSlideRows`. | **Resolved** |
| Low 1 | TTF-coverage `song_display_title/artist` overloads have no direct test (`song_display_text.cpp:26-42`) | `test_display_text_coverage` (`tests/select_art_test.cpp:365-403`) uses the real headless `TextRenderer`. It covers a Latin-1 native title (native kept, unlike the bitmap rule), CJK with a translit (translit chosen), CJK without a translit (native kept) and the null-renderer fallback. Every check compares addresses, so it also proves the returned reference aliases `metadata`. | **Resolved** |

### How Medium 1 was checked

- **All four measured leaks are culled.** These are the leaks measured at 1920x1200 in the first review: Down press slot -2 at offset 10, Up press slot 8 at offsets -40 and -10, header Down slot 6 at +152, and header Up slot 0 at -152. Each is now asserted as `!wheel_row_in_column` (`select_art_test.cpp:336-340`). In `draw_wheel`, every draw goes through the same `drawn(view)` predicate, so the test covers what actually gets drawn.
- **The texture padding cannot leak.** The rect being tested is the *content* rect. `draw_slice3` draws the whole image, including padding: `wheel_row` and `wheel_pack` have 8 px of padding at 2x (4 reference px) above and below the 124 px content. Checked with PIL: the alpha in those padding strips is (0, 0) across the full width of both PNGs. So a row whose content touches y = 0 or y = 720 paints nothing into the bands. The selected bar never slides, and its 34 px glow stays inside 58..674.
- **The sweep is thorough.** `test_wheel_slide_stays_in_column` sweeps every offset in [-152, 152] in 0.25 px steps, plus the eased offsets of every possible start, at every selection of a 40-row list. For each case it asserts:
  - the range stays inside the list and holds at most 9 rows;
  - every drawn rect stays inside 0..720;
  - mid-list, the drawn rows still reach the rest top (92) and the rest bottom.
- **Selected-bar rule unchanged.** `wheel_slide_rect` keys the fixed bar on `slot == selected_slot`, where the old code used `art == WheelArt::Selected`. These are equivalent: `select_screen.cpp` gives `Selected` art only to `r == selected_row`, and `selected_slot = selected_row - window.first`.
- **Top edge is clean.** A row is culled at the top only when its content y < 0. Its content bottom is then < 62, which lies wholly under the 66 px top bar. No visible change at any aspect ratio.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

**Noted, not a finding**:
- **A short pop at the bottom edge.** Culling at the bottom drops a whole row once its content passes y = 720, so a strip of at most 8 px just above the hint-bar rule (y 658..666) vanishes a moment early. This happens on every aspect ratio, including 16:9, where the hint bar used to cover the overhang.
  - It lasts only for offsets in (80, 88) going Down and (-72, -64) going Up. On the ease-out curve from ±152 that is about 2 to 2.5 ms of the 80 ms slide, under one 60 Hz frame.
  - The fix review offered "cull or clamp", and the header comment at `select_art.hpp:180-185` documents this trade-off. Acceptable as is.
- All "Noted, not a finding" items from the first review still apply: per-frame allocations (#98), the bitmap options overlay (#96), fixed hint key labels, the measured row gaps, no "·" separators, inline pack headers, the slide widened to ±2 rows, the gold bar not sliding, and the pending owner GUI side-by-side check.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | PASS (exit 0, `select_art_test` relinked) |
| Warnings gate: changed TUs recompiled with the build's own flags from `flags.make` (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`, outside the build tree): `select_art.cpp`, `select_screen.cpp`, `song_display_text.cpp`, `options_menu.cpp` (blaze4k_core flags), `tests/select_art_test.cpp`, `tests/select_screen_test.cpp` (their test-target flags) | PASS (0 warnings, exit 0) |
| Lint (no separate linter; the `-Wall -Wextra -Wpedantic` build is the gate) | PASS |
| Tests (sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`). Inside the sandbox, `/dev/snd` and `/run/user/$UID` were confirmed empty first | PASS, 48/48. `select_art_test` prints "wheel slide stays inside reference y 0..720 ok." and "TrueType-coverage display text ok." |
| Skipped / env-guarded tests | None. `ctest -V` has no `[SKIP]`, `SKIPPED` or "Not Run" lines; the only "Skipping" lines are expected NoteParser logs from parser fuzz fixtures. The "No GL context" headless path for theme and text is by design: draws are no-ops and every assertion runs |
| Headless real-app smoke (`./build/blaze-4k --headless --smoke-test 30 --start-screen select --data-dir <tmp>`, same sandbox) | PASS. Logs `[SelectScreen] library: 221 songs, 1116 charts`, `[ScreenManager] enter Select` and `Blaze 4k shut down cleanly.`, with no "Unknown theme texture" lines |

## What's Good

- **Minimal fix.** The fix is pure and testable. It lifts the inline `from`/`to` and offset logic into three small `select_art` helpers, and the draw path and the test share the same predicate.
- **No magic numbers.** The extra-row count and the view array size both come from `kWheelMaxSlideRows`, so they cannot drift apart.
- **Strong test.** It sweeps densely, adds the real eased offsets, asserts that culling opens no hole at the rest positions, and pins each leak the first review measured by name.
- **Honest comment.** The header comment states the one visible cost of culling (an 8 px strip for a few ms) rather than hiding it.

## Recommendation

Ready to merge once the owner runs the GUI side-by-side check (plan E2E step 4). That check should include a 16:10 window while pressing Up/Down and crossing a pack header, to confirm the bands stay clean.
