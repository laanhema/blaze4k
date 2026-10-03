# Code Review: feature/045-select-screen-cabinet (#94)

**Scope**: Branch `feature/045-select-screen-cabinet` vs `main` (uncommitted + untracked work; no commits on the branch yet). `.agents/stories/todo-stories.md` excluded (unrelated owner edit).
**Recommendation**: NEEDS WORK (one Medium finding)

## Summary

Reviewed the Cabinet v3 Song Select restyle: the new GL-free `select_art` module (layout, list window, wheel display rows, slide easing, row style/labels/ticks, chips, hint line/arrows, draw helpers), the rewritten `SelectScreen::render` and slide state, the TTF-coverage `song_display_*` overloads, the untruncated `chart_display_label`, the exported `format_speed_mod`, the two `theme.hpp` gap changes, and the new and updated tests. The code is clean, well commented, null-guarded and thoroughly pinned. Navigation, key repeat, preview, the options overlay and Confirm/Back are unchanged. One visual defect remains: on letterboxed (non-16:9) windows, sliding wheel rows draw into the bands outside the reference column.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **Sliding wheel rows leak into the letterbox bands on 16:10 / 4:3 windows.** `src/screens/select_screen.cpp:617-621` and `src/screens/select_art.cpp:539-578`
   - **What happens.** While the wheel slides, rows are drawn `offset` reference px away from their slots. Up to two extra rows are added on the exposed side (`from = first - 2`, `to = last + 2`). The plan (Risks table) and the code comment say "the bars drawn next cover what pokes out". That holds only at 16:9 and wider.
   - **Why the bars miss it.** `bar_top` starts at `L.y(0)` and `bar_hint` ends at `L.y(720)`. On a height-letterboxed window, the bands above and below the reference column show only `bg_select`, so nothing covers rows that land there.
   - **Measured at 1920x1200** (s = 1.5, origin y = 60; the bands are window y 0..60 and 1140..1200):
     - single Down press: extra slot -2 sits at window y -15..78 once the offset decays to 10, so it shows in the top band.
     - single Up press: extra slot 8 sits at window y 1095..1188 at offset -40, and 1140..1233 at offset -10. It shows in the bottom band for most of the 80 ms slide.
     - crossing a pack header Down (offset +152): slot 6 sits at window y 1155..1248.
     - crossing a pack header Up (offset -152): slot 0 sits at window y -30..63.
   - **Impact.** Every slide flashes partial wheel rows above the top bar or below the hint bar. Every pack crossing does too, because header crossings move the window 2 rows. The same happens at 640x480 and on any letterboxed size. Plan step 4e ("on a 16:10 window, nothing is cropped") covers only the at-rest layout.
   - **Fix (any one of these):**
     - (a) Keep the slide inside the reference column. Draw an extra row only while it is needed: slot -2 only while the offset is above the 50 px gap that slot -1 cannot fill, and slot 8 likewise below -50. Then cull or clamp any row whose rect leaves reference y 0..720. Note that slot 6 at +152 also needs culling.
     - (b) After the wheel, cover the bands outside `L.column()` vertically. For example, extend the top bar region to window y 0 and the hint bar region to `h`, or redraw the backdrop strips.
     - (c) Add a scissor or clip rect to `GlQuadRenderer` for the wheel pass.

     Then add a pure test: for every offset the slide can reach, no wheel rect drawn intersects reference y < 0 or > 720.

### Suggestions (Low)

1. **The new TTF-coverage display-text overloads have no direct test.** `src/screens/song_display_text.cpp:26-42`
   - `song_display_title/artist(metadata, const TextRenderer*, theme::Font)` changes which text the select screen shows. Example: a native "Café" title is now drawn natively, because the TTF covers é, where the bitmap rule picked the translit.
   - Only the null-renderer path is exercised, indirectly, by the render smokes. `bitmap_font_test` and `select_screen_test::test_special_character_titles` pin only the one-argument bitmap overloads.
   - Add a small test with the real headless `TextRenderer`. Cover a Latin-1 native title (native kept), a CJK native title with a translit (translit chosen), a CJK title without a translit (native kept), and the null-`text` fallback. Also assert that the returned reference aliases `metadata`.

**Noted, not a finding** (scoped out or owner-decided in the plan):
- Per-frame allocations: `truncate`, `"BPM " + …`, `std::to_string`, `format_percent`. Deferred to #98.
- The options overlay is still drawn with the bitmap font. Its restyle is #96.
- Hint key labels are fixed ("ENTER", "TAB", "ESC") and ignore remaps.
- `kDiffRowGap` 8 → 10 and `kWheelRowGap` 10 → 14 were measured from the mock (Open Question 1, default a).
- No "·" separators in the hint line; the plan follows the mock (Open Question 6).
- Inline pack headers (Open Question 2).
- The slide widened to |Δfirst| ≤ 2 for header crossings (report deviation 1).
- The gold selected bar does not slide, and row indents jump with the selection (plan: "it jumps to the new row").
- The owner-only GUI side-by-side check (E2E step 4) is still pending.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | PASS (up to date, exit 0) |
| Warnings gate: changed TUs recompiled with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`): `select_art.cpp`, `select_screen.cpp`, `song_display_text.cpp`, `options_menu.cpp`, `tests/select_art_test.cpp`, `tests/select_screen_test.cpp` | PASS (0 warnings) |
| Lint (no separate linter; the `-Wall -Wextra -Wpedantic` build is the gate) | PASS |
| Tests (sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`). Inside the sandbox `/dev/snd` and `/run/user/$UID` were confirmed empty first | PASS, 48/48 (47 baseline + `select_art_test`) |
| Skipped / env-guarded tests | None. `ctest -V` shows no SKIP. "No GL context" headless measuring is the designed path for theme/text (draws are no-ops, every assertion runs) |
| Headless real-app smoke (`./build/blaze-4k --headless --smoke-test 30 --start-screen select --data-dir <tmp>`, same sandbox) | PASS. `[SelectScreen] library: 221 songs, 1116 charts`, `[ScreenManager] enter Select`, `Blaze 4k shut down cleanly.`; no "Unknown theme texture" lines. Audio used the Null backend |

## What's Good

- **Clean split.** `select_art` is pure and GL-free, with thin draw helpers, mirroring `title_art`. `SelectScreen::render` reads as the plan's draw order.
- **Thorough pins.** Layout is pinned at four window sizes, plus the list-window rule (including the pre-#94 13-row behaviour), the visible-row derivation, the slide easing and start rule, and that every texture name is in the real manifest.
- **Slide timing.** The slide runs on the fixed `dt` only, never the music clock or wall time. `enter()` resets it, and held repeat is clamped to ±2 rows (tested on the real screen).
- **OpenITG classification.** `resolve_difficulty` drives row colour and art, while the simfile's own label is still shown. An Edit chart's name comes from one rule (`shows_edit_name`) shared by both `chart_display_label` overloads.
- **Untrusted text.** Edit names and pack names keep their UTF-8 bytes (`ascii_upper` changes ASCII only). Text is truncated by measured width, and malformed bytes go through `TextRenderer`'s placeholder path.
- **Chips refresh without per-frame work.** They rebuild only when the config strings change, including after `handle_back` closes the overlay.
- **Tidy-up.** The deleted helpers (`difficulty_color`, `difficulty_row_text`, `pack_names_`) leave no stale references.

## Recommendation

Fix the letterbox leak (Medium 1) and add a pure test that no slide offset puts a wheel rect outside reference y 0..720. Optionally add the TTF-coverage display-text test (Low 1). Then the owner runs the GUI side-by-side check (plan E2E step 4), including a 16:10 window while pressing Up/Down and crossing a pack header.
