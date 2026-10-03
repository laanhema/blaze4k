# Code Review: feature/038-remove-live-grade-hud

**Scope**: Branch `feature/038-remove-live-grade-hud` vs `main` (no commits yet; uncommitted changes to `src/gameplay/hud_renderer.{hpp,cpp}` and `tests/hud_renderer_test.cpp`, plus untracked plan and implementation report). GitHub issue #83.
**Recommendation**: APPROVE (one Low suggestion, optional)

## Summary

The change removes the bottom-centre live grade from the gameplay HUD. It deletes the draw block in `HudRenderer::render`, the `grade_text_rect` layout helper (whose only callers were that block and one test), and the `test_grade_text_clear` life-bar clearance test. It also updates three comments that still mentioned a HUD grade. `format_grade`, `ScoreState::grade`, the results screen, high-score records and the gameplay console log line are untouched. The diff is a small, clean deletion that meets all four acceptance criteria in #83.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/gameplay/hud_renderer.hpp:20-22`: `format_grade` now has no HUD consumer.** After this change, the only production caller of `format_grade` is `src/screens/results_screen.cpp:178`, which includes `gameplay/hud_renderer.hpp` only for that function. The helper therefore lives in the HUD module but serves only the results screen. Optional follow-up: move it next to the grade data (`score_keeper` / `GradeTier`) or into a small results-presentation helper, and drop the results screen's HUD include. Leaving it in place is fine for a Small issue, and the plan deliberately kept it where it is.

**Noted, not a finding** (the plan scoped these out explicitly):
- No regression test proves that the HUD draws no grade. `GlQuadRenderer` has no draw-recording seam, and adding one is a refactor beyond this issue (plan Open Questions).
- With `test_grade_text_clear` gone, nothing checks clearance at the bottom edge for a future bottom-of-screen HUD element. The life bar's own geometry tests still pin its position (plan Risks).
- The `TODO.md` tick is a local-only edit because `TODO.md` is gitignored (`.gitignore:27`). The implementation report already records this.

## Correctness Checks Performed

- `grep -rn grade_text_rect src tests`: no matches, so no dangling declaration, definition or call remains.
- `render` still reads `screen_h` in its `<= 0` guard, and `main_pixel`, `kGlyphRows` and `kHudEdgeMargin` are all still used, so the removal leaves no unused variables or constants.
- `hud_renderer.hpp:14` ("The grade itself uses the unclamped `ScoreState::percent`") is still accurate, because the grade is still computed in `score_keeper.cpp:199`.
- The only remaining grade users are outside the HUD: `results_screen.cpp:178` (`format_grade`), `results.cpp` (`ScoreState::grade`), `gameplay_view.cpp:388` (console log) and `score_keeper.cpp:199-201` (writer).
- `test_grade_text_clear` used the helpers `intersects`, `Size` and `layout_at`. Other cases still use all three, so removing it leaves no unused helpers.
- README (`README.md:50`) already omitted the grade from its HUD list. The PRD and `docs/` do not mention a live HUD grade.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j`, Release, GCC, `-Wall -Wextra -Wpedantic`) | PASS (exit 0) |
| Warnings gate (`g++ -fsyntax-only` on `src/gameplay/hud_renderer.cpp` and `tests/hud_renderer_test.cpp`, with the flags from `flags.make` for `blaze4k_core` / `hud_renderer_test`) | PASS (zero warnings) |
| Lint | N/A: no linter is configured; the warning-flag recompile above is the lint gate |
| Tests (`bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`) | PASS: 100% of 41 |
| Sandbox check | Inside bwrap, `/dev/snd` and `/run/user/$UID` were empty. `audio_test` ran on miniaudio's `NULL Playback Device` |
| Skipped or env-guarded tests | None. Gameplay tests 13, 15 and 19 log "Audio unavailable; using synthetic stub clock". That is their designed headless fallback and they still run every assertion; no test was skipped |
| `hud_renderer_test` direct run | PASS: all 11 remaining cases |

## What's Good

- Tight, deletion-only diff that matches the plan exactly. Percent, combo, chip and life-bar code is byte-identical.
- Comments were updated as well as the code (`kHudTextPixel`, the layout-helper doc and the `HudRenderer` class doc), so no stale "grade" wording remains in the HUD.
- `format_grade` and `ScoreState::grade` were correctly kept for the results screen and high scores, so the results path is unaffected and its tests still pass.
- The plan's static greps and E2E notes give a clear audit trail for an acceptance criterion that unit tests cannot observe.

## Recommendation

Ready to commit and open a PR. The Low suggestion about where `format_grade` lives is optional and can wait for a later cleanup. The owner can do the plan's optional visual check: play a song, confirm the bottom centre is empty, then confirm the results screen still shows the tier-coloured grade.
