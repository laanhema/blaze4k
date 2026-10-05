# Code Review: feature/050-judgment-text-half-size

**Scope**: Branch `feature/050-judgment-text-half-size` vs `main` (no commits yet; uncommitted + untracked changes), GitHub issue #111. `.agents/stories/todo-stories.md` excluded (unrelated owner edit). Reviewed `src/gameplay/judgment_animator.{hpp,cpp}`, `src/render/theme.hpp`, `tests/judgment_animator_test.cpp`, `tests/hud_renderer_test.cpp`, plus the untracked plan and implementation report.
**Recommendation**: APPROVE WITH NITS

## Summary

The change adds one constant, `JudgmentAnimator::kJudgmentDisplayScale = 0.5f`, and one pure helper, `judgment_draw_scale(elapsed) = kJudgmentDisplayScale * pop_scale(elapsed, kJudgmentPopSeconds)`. `render_judgment` uses that helper as its only scale for both the themed sprite and the bitmap fallback label. `pop_scale`, `pop_alpha`, `judgment_pop_rect`, `kJudgmentTop` and `kComboTop` are unchanged. I checked the geometry by hand. `draw_sprite` scales the content rect and its glow by `scale_k(L.s * scale) = L.s * scale / texture_scale`, and `judgment_pop_rect` scales about the full-size box centre (640, 329). So at rest the sprite is {529, 312.5, 222, 33} at 720p and {1058, 625, 444, 66} at 1440p. At peak (0.625), the glow bottom lands at ref 367.125, which is below the combo line top at 368. All five acceptance criteria of #111 are met. Nothing in the timing, judgment, scoring or asset path changed. There is one Low documentation nit.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/gameplay/judgment_animator.hpp:70-73` and `src/render/theme.hpp:209-210`: the new comments say "the pop is drawn centred" on the full-size box, but that is true only for the sprite.** The fallback label (`judgment_animator.cpp:213-215`) is still top-anchored at `kJudgmentTop`. Before this change, both paths had their top at ref 296 at rest. Now the sprite's top is 312.5, while the fallback label spans 296–313.5. So the two paths no longer share a position, and the comments hide that. The `render_judgment` comment also says "centred" twice ("centred on the field (centred on the full-size content box …)").
   *Recommendation*: Change both comments to say the **sprite** is centred on the full-size box, and that the fallback label hangs from `kJudgmentTop`. For example, in the header: "The judgment sprite, drawn at `judgment_draw_scale` and centred on the full-size content box whose top is kJudgmentTop, …; the fallback label is top-anchored at kJudgmentTop." Changing the anchor itself is out of scope (see below).

**Noted, not a finding** (the plan scoped these out explicitly or pinned them on purpose):
- The fallback label stays top-anchored and still ignores `L.s`, so it is the same pixel size at 1440p as at 720p. This was already the case before the change (plan Risks rows 4–5, Open Question 2).
- The manifest `layout_720p` for `judgment_*` ([418, 296, 444, 66]) no longer matches the drawn rect. It is documentation only, and the issue forbids editing the art (plan Risks).
- `test_judgment_draw_scale` pins `kJudgmentDisplayScale == 0.5f`, and `test_judgment_pop_rect` pins exact half-size rects. A playtest retune will have to update those numbers too. The plan's Pinned Semantics asks for this on purpose.
- At peak, the glow clears the combo line by only 0.875 ref px. It is still clear, and `test_judgment_clears_combo` guards it at five window sizes against the real manifest.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j`, Release, GCC 16.2.1) | PASS |
| Warnings gate: changed TUs recompiled with the build's own `flags.make` flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`): `judgment_animator.cpp` (blaze4k_core), `judgment_animator_test.cpp`, `hud_renderer_test.cpp` | PASS (0 warnings) |
| Lint | N/A (no linter configured; the warnings gate above stands in) |
| Tests: sandboxed `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` (I checked inside the sandbox first: `/dev/snd` and `/run/user/$UID` were empty) | PASS 49/49 |
| Skipped / env-guarded tests | None. The verbose log has no test-level skips (the "Skipping" lines come from parser and library fixtures, which is expected behaviour) |
| Sandbox fallbacks (all assertions still run) | Audio tests use miniaudio's Null playback device. Render tests run headless ("No GL context": flat fallbacks, text measured but not drawn) |
| `judgment_animator_test` direct run (sandboxed) | PASS. The new "judgment draw scale (half size, same pop curve)" and "half-size judgment clears the combo line" cases print ok |

## What's Good

- The factor lives in exactly one constant, and one pure helper is the only scale source in `render_judgment`, so the sprite and the fallback cannot drift apart. This matches the technical note on #111.
- The pop-curve shape is protected by a 1001-sample `draw == K · pop_scale` equality check instead of a single spot value.
- The clearance test reads the glow pad from the real manifest (across all 9 `judgment_*` sprites) and covers letterboxed (1280x1024) and pillarboxed (2560x1080) windows as well as 720p, 1080p and 1440p.
- `hud_renderer_test::test_judgment_pop_clear` now scans the scale that is actually drawn, not the raw curve.
- It is presentation-only and as small as it can be. Pure functions keep their contracts, and the header comments explain how the scale and the full-size `content` relate.

## Recommendation

Ready to merge once the owner has done the windowed visual check at 1280x720 (and 2560x1440 if available). Optionally fix Low #1 (comment wording only) first.
