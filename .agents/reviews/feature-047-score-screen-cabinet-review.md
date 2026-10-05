# Code Review: feature/047-score-screen-cabinet (#95)

**Scope**: Branch `feature/047-score-screen-cabinet` vs `main` (uncommitted + untracked work; no commits on the branch yet). `.agents/stories/todo-stories.md` excluded (unrelated owner edit).
**Recommendation**: APPROVE WITH NITS (three Low findings)

## Summary

I reviewed the Cabinet v3 score screen: the new `results_art` module (pure layout in reference px plus thin draw helpers), the rewritten `ResultsScreen::render` and its `enter`-time string cache, the comment-only `ResultsAnimator` update, the reworked `results_screen_test` / `results_anim_test`, and the CMake and README edits. The work meets every acceptance criterion of #95, with the owner decisions recorded on the issue applied: earned grade plus FAILED ribbon, straight bars, tier wording, negative DP shown as 0, MINES = mines hit, and ENTER SKIP → ENTER CONTINUE. Submission, the best-score rule, input handling and `ResultsSummary` are unchanged. Nothing reads a clock, and only `digits_text` / `format_percent` output reaches `BitmapDigits`. Every theme or text call is null-guarded.

I checked these against the code and assets:
- `TapJudgment` order (Fantastic..Miss = 0..5, HitMine = 6) matches the row and total indexing.
- `BitmapDigits::draw` subtracts `origin_x`, so a Left-aligned pen x is the ink start, as the stat-panel x math assumes.
- `content_size(name, 1.0f)` returns reference px.
- `draw_sprite` takes the content-box top-left, which matches `grade_rect` / `ribbon_rect`.
- All 11 texture and digit names the screen uses exist in `assets/theme/cabinet/manifest.json`, and every text style it uses is in `kAllStyles`, so no atlas is baked mid-screen.
- `truncate_to_width` has a 1e-3 epsilon, so the `measure / scale * scale` round trip cannot add a spurious "..." to a title that fits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **Third copy of the private text helpers.** `src/screens/results_screen.cpp:28-38`
   - `with_color` and `ref_measure` are copied again in `results_screen.cpp`'s anonymous namespace. `results_art.cpp:26-41` (and `select_art.cpp`) already have identical copies.
   - **Fix:** expose them once from `results_art.hpp`, or better, from a shared screens/render helper, and call that from both TUs. This is not a bug, but it is the third copy of the same four lines.

2. **Hold labels and values are re-measured every frame.** `src/screens/results_screen.cpp:293-305`
   - The three constant labels ("HOLDS OK", "NG", "MINES") are TTF-measured every frame, and the three hold digit strings are digit-measured every frame. The results only change when the text scale changes (or on `enter`).
   - It does not allocate and costs little, but `refit_bar_text` already has a scale-keyed cache for exactly this.
   - **Fix:** compute `hold_cols` inside `refit_bar_text`, store it with `bar_layout_`, and reuse it each frame.

3. **The ENTER SKIP / ENTER CONTINUE choice (owner decision 6) is not pinned by a test.** `src/screens/results_screen.cpp:410`, `tests/results_screen_test.cpp` (`test_render_cabinet_headless`)
   - The hint word is picked inline in `render`. The headless render test uses an uninitialised `GlQuadRenderer`, so every `results_art` draw helper returns early, and no assertion sees which word was chosen.
   - **Fix:** move the choice into a small pure accessor (e.g. `hint_word()`: `"SKIP"` while `has_reveal() && !animator_.finished()`, else `"CONTINUE"`) and assert it before and after a skip, and on a NO RESULT screen.

**Noted, not a finding** (scoped out or owner-decided in the plan or on the issue):
- `results_screen.cpp` includes `screens/gameplay_screen.hpp` for `difficulty_badge_for` (plan Risks: "Accept").
- No mipmaps on the digit atlases (chrome-percent shimmer is left to the owner's visual check; plan Risks, out of scope).
- Straight judgment bars (`kJudgmentBarSkew = 0`), against the issue note's `skew::kRows` (Open Question 2, owner-decided).
- The title is left-aligned in its slot, so a truncated title can leave slightly more than 16 px before the artist (report deviation).
- A grade entry that is in the manifest but whose PNG failed to load draws `ThemeTextures`' flat fallback, not the text fallback (plan Risks: "a missing sprite falls back inside ThemeTextures").
- The medallion and the chrome percentage draw at full alpha from frame 0. That matches the plan's reveal table and the old C7 percent behaviour.
- The owner-only windowed visual check against `cabinet-v3-results.png` (E2E 3) is still pending. Headless runs return before any GL draw, so the real pixels are not exercised by automation.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j8`, Release) | PASS (exit 0) |
| Warnings gate: changed TUs recompiled with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`): `results_art.cpp`, `results_screen.cpp`, `results_anim.cpp`, `tests/results_screen_test.cpp`, `tests/results_anim_test.cpp` | PASS (0 warnings) |
| Lint (no separate linter; the `-Wall -Wextra -Wpedantic` build is the gate) | PASS |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build -j8 --output-on-failure` (confirmed inside the sandbox that `/dev/snd` and `/run/user/$UID` are empty) | PASS, 48/48 |
| Skipped / env-guarded tests | None (no SKIP output). Inside the sandbox, `GameplayView` falls back to its stub clock, and the theme and fonts load headless (measured, not drawn). Every assertion still runs, so these fallbacks are not skips |
| Digit glyph safety at runtime | `results_screen_test` logs no `[ThemeTextures] ... has no glyph` line and no unknown-texture warning over 5 states × 6 sizes × 30 steps |
| Static: `draw_text|bitmap_font|grade_color|results_difficulty_line|"RESULTS"` in `results_screen.*` / `results_screen_test.cpp` | No hits |
| Static: no clock reads (`steady_clock|SDL_GetTicks|music_clock|time_seconds`) in `results_art.cpp` / `results_screen.cpp` | No hits |

## What's Good

- The split matches `select_art` / `title_art`: pure reference-px layout that is headless-testable, and draw helpers that are no-ops on null services or an uninitialised renderer. Each measured constant names its source, and `static_assert`s tie the judgment row sum and the second panel's y to `theme.hpp`.
- There is no per-frame churn. Every string is built in `enter`, and the badge, title and artist are re-fitted only when the text scale changes. The percent string fits SSO, labels are `string_view` literals, and no text style outside `kAllStyles` is used.
- It is robust to bad input: every float input is `isfinite`-guarded, indices are clamped, the judged-tap total is computed in 64-bit and clamped, the fill handles `count > total`, `total == 0` and negative counts, and negative DP and max combo can never put a `-` into the digit atlas.
- The badge reuses #93's `difficulty_badge_for` / `badge_text_width` / `fit_badge_text`, so the label truncates while the foot rating survives (the #93 review's Low finding stays fixed here).
- The tests are thorough. All 17 compiled grade tiers are checked against the real manifest (sprite kind and 680x400 content). The top-bar, stat-panel and judgment-row geometry is pinned to the mock's measured pixels. There are NaN/inf cases and a 4-digit hold-column case. A real-theme, real-font headless render covers 5 states at 6 window sizes, including 0x0 and a 300-char UTF-8 title. The reveal-order invariants are added to `results_anim_test`.

## Recommendation

Approve with nits. The three Low items are optional cleanups (deduplicate the helpers, cache `hold_cols` with the bar fit, pin the hint word with a test) and can be folded in with `/fix-findings` or left for later. Before merging, the owner should still do the windowed visual check against `docs/cabinet-theme/reference/cabinet-v3-results.png` (plan E2E 3).
