# Code Review: feature/046-gameplay-hud-cabinet (#93)

**Scope**: Branch `feature/046-gameplay-hud-cabinet` vs `main` (uncommitted + untracked work; no commits on the branch yet). `.agents/stories/todo-stories.md` excluded (unrelated owner edit).
**Recommendation**: APPROVE WITH NITS (one Low finding)

## Summary

Reviewed the Cabinet gameplay HUD restyle: the rewritten `HudRenderer` (percent, chips and palette removed; chrome life bar with the field-clearance clamp; tinted `diff_badge` with a cached truncated label), the `JudgmentAnimator` changes (baked `judgment_<kind>` sprites with a bitmap fallback, the persistent "N COMBO" line from combo 4, the colour-only milestone flash), the `GameplayView` / `GameplayScreen` / `--gameplay-demo` wiring, `difficulty_badge_for`, and the updated tests. The code meets every acceptance criterion of #93. It is null-guarded and keeps the screens → gameplay layering (`src/gameplay` never includes `screens/`). It reads no clock, and the geometry is pinned by pure-layout tests. The draw order matches the issue (badge/frame → fill → stripes → judgment sprite → text). I checked the geometry against the assets: the `life_frame.png` chrome border is 8 texture px (4 ref px) on every side, which matches `kLifeFrameBorderRef`. All 9 judgment sprite names exist in the manifest, `diff_badge` is `slice3` and `life_frame` is `slice9`. The combo number and label share a baseline, so the per-draw shear (`x += shear * (baseline - y)`) shears them as a single group. The field is screen-centred, so `L.x(640)` is the field centre.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **Long Edit names truncate the meter off the badge.** `src/screens/gameplay_screen.cpp:19` together with `src/gameplay/hud_renderer.cpp:150-156`
   - `difficulty_badge_for` builds one string, `"<label> <meter>"`. `render_text` then "..."-truncates the whole string to the plate. The plate is capped at 300 ref px, which leaves 264 ref px for text at 24 px Saira ExtraBold with 3 px tracking (roughly 15–16 characters).
   - So an Edit chart whose description is longer than about 15 characters shows e.g. "My Very Long Edi..." and loses its foot rating. The rating is the one number the badge exists to show, and the difficulty display is meant to pass the simfile's label and rating through.
   - **Fix:** keep the label and meter apart in `DifficultyBadge` (e.g. `label` + `meter` strings). Truncate only the label to `text_max_w - measure(" " + meter)`, then draw the meter after it. Pin it with a `hud_renderer_test` / `gameplay_screen_test` case that uses a 40-character Edit name and checks that the drawn text still ends with the meter.

**Noted, not a finding** (scoped out or owner-decided in the plan):
- The note field is not scaled with the layout scale at 1440p (Open Question 2; the issue says to leave the field untouched).
- The Cel textures are loaded twice (forward reference from plan 044; out of scope).
- OpenITG miss-combo display (`ShowMissCombo`): not implemented, flagged out of scope in the plan.
- One judgment/combo position for both scroll directions (Open Question 4; no `ComboYReverse`).
- Combo display policy: persistent from combo 4, with a colour-only gold → white milestone/final flash (Open Question 1 default).
- Badge left-aligned at +18 ref px and growing to 300 ref px (Open Question 3 default).
- `judgment_animator.cpp` keeps the `hud_renderer.hpp` include for `format_combo` (report deviation 1).
- The badge text is measured twice per frame (once in `render_chrome` for the plate width, once in `render_text`). It does not allocate, and the cost is negligible on a ~10-character string.
- The owner-only windowed visual check vs `cabinet-v3-gameplay.png` (E2E 3) is still pending. Headless runs return before any HUD draw call, so the real pixels are not exercised by automation.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j8`, Release) | PASS (exit 0) |
| Warnings gate: changed TUs recompiled with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`): `hud_renderer.cpp`, `judgment_animator.cpp`, `gameplay_view.cpp`, `gameplay_screen.cpp`, `main.cpp`, `tests/hud_renderer_test.cpp`, `tests/judgment_animator_test.cpp`, `tests/gameplay_screen_test.cpp` | PASS (0 warnings) |
| Lint (no separate linter; the `-Wall -Wextra -Wpedantic` build is the gate) | PASS |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure -j8` (confirmed inside the sandbox that `/dev/snd` and `/run/user/$UID` are empty) | PASS, 48/48 |
| Skipped / env-guarded tests | None (no `SKIP` guards in `tests/`). With no audio device, `GameplayView` falls back to its stub clock and still runs every assertion; that fallback is not a skip |
| Static: `percent_text_rect`, `judgment_color`, `kLifeBarMinInset`, `draw_chip` in `src/gameplay` | None left |
| Static: no music-clock reads in `hud_renderer.*` / `judgment_animator.*` | PASS |

## What's Good

- The layering is clean. `GameplayScreen` (screens layer) builds a plain `DifficultyBadge` from `select_art`, so the label and colour rules match the select screen exactly, and `src/gameplay` stays free of `screens/`.
- The life fill crops the UVs over a fixed track, so the gradient does not slide as life changes. NaN life is clamped to 0, and the slide-then-shrink field clamp is kept and pinned at 500x400 and 400x400.
- Feedback cannot disappear: with no theme, or a sprite missing from the manifest, the judgment falls back to the bitmap label on the same curves.
- There are no per-frame allocations or atlas bakes. The truncated badge label is cached, the combo string fits SSO, sprite names are `string_view` literals, and the milestone flash changes colour only.
- `kShowComboAt = 4` is cited to OpenITG metrics and `Combo.cpp`.
- The tests are strong. The sprite mapping is checked against the real manifest. The mock geometry at 720p is pinned and scaled across 7 window sizes. The pop clearance is checked at peak scale. A headless render smoke uses the real theme and fonts.

## Recommendation

Ready to merge after the owner's windowed visual check. The Low finding (keep the meter visible for long Edit names) can be fixed now or tracked as a follow-up.
