# Code Review: feature/048-options-overlay-cabinet

**Scope**: branch `feature/048-options-overlay-cabinet` vs `main` (uncommitted + untracked work; no commits on the branch yet), GitHub issue #96
**Recommendation**: APPROVE WITH NITS

## Summary

The bitmap options overlay on song select is replaced by a Cabinet overlay drawn by the new GL-free module `src/screens/options_art.{hpp,cpp}`. It has a `kGameplayScrim` dim, a navy panel with a `bar_top` header, `wheel_row` / `wheel_row_selected` rows, values in `kGold` and the legend in `bar_hint`.
`select_art`'s legend builder is now shared (`HintItem` / `layout_hint_items` / `draw_hint_line`), and `SelectScreen` caches the value strings behind a dirty flag.
The change matches the plan and every acceptance criterion of #96. Input routing, `options_menu.*` and the transitions are untouched. Validation is clean. There is one Low finding in the new shared legend builder.

Out of scope: `.agents/stories/todo-stories.md` (the owner's unrelated uncommitted edit). It was not reviewed or touched.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/screens/select_art.cpp:274-283, 297-299`: `layout_hint_items` drops the trailing gap only when the last item is a Word.**
   `add_key` always adds `kHintTextKeyGap` (8), and `add_arrows` always adds `kHintArrowKeyGap` (10). A line that ends on a Key or an arrow pair, whether the item list ends that way or the `kHintPieceCount` cap cuts it there, keeps that gap in `line.width`, so the line is centred 4–5 px off. The cap can also leave a Key without its Word (for example "ENTER" with no "PLAY").
   No current caller hits this, because both legends end with a Word, and the tests only cover a capped all-Word line and an arrow pair at the cap.
   The implementation report's deviation 3 ("the last item that fits gets no trailing gap") overstates what the code does.
   *Recommendation:* drop the trailing gap from whichever piece is placed last (for example, subtract the last gap from `x` after the loop), or note the Word-only behaviour in the `HintItem` / `layout_hint_items` comment. Optionally, have the cap keep a Key and its Word together.

**Noted, not a finding**
- `wheel_row_selected`'s glow padding hangs about 30 ref px outside `row_rect`, so when the first or last row is selected it reaches about 6 px into the header and past the panel bottom. The plan says rects are content boxes with the glow outside them. I measured the outermost 6 ref px of the glow: its alpha is at most about 7/255, so it does not show.
- The `bar_top` header is opaque and covers the top 66 px of the panel's 2 px side ring. This is by design (the header is drawn over the body).
- The owner's windowed visual check (plan E2E step 3) is still pending. The agent must not launch the GUI.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | PASS |
| Warnings gate: the changed TUs (`options_art.cpp`, `select_art.cpp`, `select_screen.cpp`, `tests/select_art_test.cpp`, `tests/select_screen_test.cpp`) were recompiled to `/dev/null` with the build's own `flags.make` flags (`-O3 -std=c++20 -Wall -Wextra -Wpedantic`) | PASS (0 warnings) |
| Lint | N/A (no linter configured; the warnings gate stands in) |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` (inside the sandbox, `/dev/snd` and `/run/user/$UID` were confirmed empty first) | PASS (48/48, 0 skipped. The "Skipping …" lines in the `-V` output are parser log lines, not test skips) |
| Headless smoke: `blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>` in the same sandbox | PASS ("Blaze 4k shut down cleanly"; it ran on miniaudio's NULL playback device as the sandbox fallback, not a skip) |
| Static: `draw_text\|bitmap_font\|kPlaceholderColor\|kDimOverlay` in `select_screen.cpp` | PASS (no hits) |
| Static: clock reads in `options_art.cpp` | PASS (no hits) |
| Static: `options_menu.{cpp,hpp}` and `tests/options_menu_test.cpp` unchanged | PASS (empty diff) |

## What's Good

- **Room budget enforced at compile time.** The `static_assert`s in `options_art.hpp:89-93` tie `kOptionsRowCount` to the panel height, so an eighth row cannot quietly overlap the legend. This makes the "options room" regression structural instead of a matter of luck.
- **Value cache is correct and cheap.** The dirty flag is set on open and on every press the overlay handles, the cache is rebuilt once after the event loop, and the `enter()` reopen path rebuilds it directly. `render()` only reads it. `select_screen_test` pins the cache after each adjustment and after the calibration round trip, including a freshly written offset.
- **Behaviour-preserving refactor.** Select's legend comes from the generalised builder, and the existing `test_hint_layout` passes unmodified.
- **Fit tested with real fonts.** `test_options_text_fits` covers the whole X grid, C/M 9999 and ±3600 s, selected and unselected, so the rare-path `truncate` never runs for real values.
- **No new atlases.** `test_options_styles_prebaked` shows every style shares a pre-baked (font, size), and `kAllStyles` stays at 26.
- The `.hpp` comments follow the `select_art` / `results_art` house style: reference-px layout, null-guarded draw helpers, no clock reads.

## Recommendation

Ready to merge after the owner's windowed visual check. Finding 1 is optional hardening of the shared legend builder for future callers (#97 is likely to reuse it). It can be fixed with `/fix-findings` or left as is.
