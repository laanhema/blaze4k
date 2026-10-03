# Code Review: feature/034-vertical-life-bar-left

**Scope**: Branch `feature/034-vertical-life-bar-left` vs `main`. The branch has no commits, so the scope is all uncommitted and untracked changes: `src/gameplay/{gameplay_view.cpp, hud_renderer.cpp, hud_renderer.hpp, note_field.hpp, note_field_renderer.cpp}`, `tests/{CMakeLists.txt, note_field_test.cpp, hud_renderer_test.cpp (new)}`. GitHub issue #76.
**Recommendation**: APPROVE (with two optional Low nits)

## Summary

The life bar moves from a horizontal bar at the bottom centre to a vertical bar on the left edge that fills from the bottom up. The geometry now lives in a pure, testable `layout_life_bar()` helper, and the note field's left edge is shared through a new `NoteField::field_left()` accessor. The change is small, correct and well covered by tests, and it meets every acceptance criterion on #76. The build has no warnings and all 39 tests pass.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`tests/hud_renderer_test.cpp:25-26, 149`: test constants copy the HUD layout by hand.**
   - `kGap` (16), `kPercentBottom` (8 + 21) and the grade rect at line 149 repeat private values from `hud_renderer.cpp`.
   - If the HUD text moves or the gap constant changes, these assertions keep passing against stale geometry.
   - Recommendation: accept this as a known coupling, or expose the few layout constants (or a `percent_text_rect` / `grade_text_rect` helper) from `hud_renderer.hpp` so the test reads the real values.

2. **`tests/hud_renderer_test.cpp:126`: one claim in the comment is not asserted.**
   - The comment says the combo and judgment pop lie inside the field rect. That is true today: the widest pop, "FANTASTIC" at 5 × 1.25 px per pixel, is about 338 px wide, which fits the 432 px field. The animator owns those sizes, though, and no test checks them.
   - Recommendation: add an assertion that the widest judgment label at peak pop scale, centred on the screen, still clears the bar at 640x480. Or soften the comment.

**Noted, not a finding**

- In windows narrower than about 484 px, the bar still touches or overlaps the leftmost lane once it has shrunk to its 6 px minimum (`hud_renderer.cpp:200`). The plan accepts this explicitly (Risks table and Open Question 2): the bar is never hidden, and the field itself spills off-screen below 432 px.
- Small cosmetic change: at life 0 the fill quad is no longer drawn. Before, a zero-width quad was drawn, so nothing changes on screen.
- The #76 issue comment says the bar "slides left and shrinks" below about 484 px. In the code, the slide and shrink start below about 548 px (`field_left < 58`). Below about 484 px the 16 px gap can no longer be kept. This affects documentation only, not code.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release) | PASS (already up to date) |
| Warnings gate: the 3 changed `src/` files and 2 changed test files recompiled with `-fsyntax-only` using the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, taken from `flags.make`) | PASS: 0 warnings |
| Lint | N/A: the project has no linter configured |
| Tests (`ctest --test-dir build --output-on-failure` inside `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net`) | PASS: 39/39, including the new `hud_renderer_test` (11 cases) and the new `note_field_test` case 11 |
| Sandbox check | Confirmed inside the sandbox that `/dev/snd` and `/run/user/$UID` are empty, so no real audio device is reachable |
| Skipped or env-guarded tests | None skipped. `score_keeper_test` (#13) and `life_keeper_test` (#15) log `Audio unavailable; using synthetic stub clock` because the sandbox hides audio. They still ran and passed on the stub clock, so the real-audio path was not exercised |

## What's Good

- **Pure layout helper.** `layout_life_bar()` returns plain rects, and `render_life` only draws them. That makes the geometry testable without a GL context, the same pattern the project already uses for `layout_hold`.
- **One source of truth for the field edge.** `NoteField::field_left(screen_w)` replaces the formula that lived inline in `note_field_renderer.cpp`. The HUD and the field renderer can't drift apart, and the call site passes the live `field_` config.
- **Unchanged behaviour stays unchanged.** The palette, draw order (frame, then back, then fill), the strict `< 0.3` danger threshold and the clamp to [0,1] all match the old code. The grade, percent and combo text code is untouched.
- **Good edge-case coverage.** Tests cover degenerate sizes, narrow windows (500 px and 400 px), finiteness, symmetric centring at 720p and 1080p, and clearance from the percent and grade text. All of it is checked at the issue's common sizes.
- **Constants are named and commented.** They are clearly marked as unsourced Blaze 4k presentation values, consistent with the existing palette comment.

## Recommendation

Ready to merge. The two Low items are optional cleanups for the tests. The owner play-test the implementation report lists (1280x720, 1920x1080 and 640x480, with and without Reverse) is still the remaining manual gate.
