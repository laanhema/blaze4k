# Code Review: feature/028-fix-hold-roll-tail-length

**Scope**: Branch `feature/028-fix-hold-roll-tail-length` vs `main`. The branch has no commits yet. The scope is the uncommitted changes to 7 tracked files, plus the untracked `tests/note_field_renderer_test.cpp`, plan and implementation report. `.agents/stories/todo-stories.md` is excluded because it is unrelated. GitHub issue #62.
**Recommendation**: APPROVE (with nits)

## Summary

Before this change, the Cel hold/roll end cap was drawn entirely past the tail, so every hold and roll looked one full note box (96 px) longer than its release time. Now the body stops `HoldSprites::tail_inset_scale` note sizes before the tail and the cap starts at that point. For Cel the value is `kCelHoldBodyStopFromTail = 0.5`, from `metrics.ini` `StopDrawingHoldBodyOffsetFromTail=-32`, which centres the cap on the tail.

The geometry moved into a pure, headless-tested `layout_hold()`. `draw_hold` now takes the scroll direction explicitly instead of inferring it from `tail_y < head_y`. The body and cap share one float edge, so no seam appears between them.

I checked the provenance against the upstream sources:

- OpenITG `NoteDisplay.cpp` @ `f2c129f`: `fYBodyBottom` / `fYCapTop = fYTail + StopDrawingHoldBodyOffsetFromTail` at `:649` and `:755`, the head-centre clip at `:774-775`, and the `fTopDistFromTail` texture offset at `:799-800`.
- SM5 v5.0.12 `NoteDisplay.cpp`: in reverse, `y_head -= StopDrawing…` (`:1110`) with the swapped caps (`:1094-1099`), which also centres the cap on the tail in reverse.

The logic is correct in both scroll directions:

- Body and cap UVs flip correctly in reverse.
- The clipped cap's `v` offset matches OpenITG.
- The culling margin in `compute_visible` (`note_height / 2` = 48 px) now exactly covers the new 48 px overshoot. Before the change, the 96 px overshoot could be culled while still partly on screen.
- The parser rejects exotic timing (`note_parser.cpp:63-77`), so the explicit `reverse` flag can't disagree with the head/tail order.

I found three Low-severity nits and nothing blocking.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions

1. **Low**: `src/gameplay/note_field_renderer.cpp:119-121` (and plan Decisions, "SM5 :1056"). The comment presents the head-centre clip of the cap as `OpenITG DrawHoldBottomCap / SM5 DrawHoldPart` behaviour in both directions, but SM5 v5.0.12 only clips the cap that ends up at the bottom of the screen.
   - SM5 v5.0.12 applies `part_args.y_start_pos = max(part_args.y_start_pos, y_head)` (`NoteDisplay.cpp:1056`) only to the screen-bottom part.
   - In reverse, Cel's bottom-cap art is swapped into the *top* part (`:1096-1099`). That part is clipped only to the draw window, not to the head, so SM5 draws the whole reverse cap even when the hold is held into its last half-note.
   - The visual difference stays inside the pinned head sprite's ±48 px box, and the symmetric clip is a reasonable choice. But it is OpenITG's up-scroll rule mirrored on purpose, not SM5 parity.
   - **Recommendation:** reword the comment to something like "OpenITG DrawHoldBottomCap (up-scroll), mirrored for reverse". Keep the behaviour.
2. **Low**: `src/gameplay/note_field_renderer.cpp:89-103`. The cap's texture orientation logic isn't covered by tests.
   - This logic is the `cap_v_near` placement in the reverse vs. up-scroll `UVRect`, plus the `clipped ? cap_near_y : junction_y` edge choice. It lives only in the GL draw path, which is a no-op headless.
   - `layout_hold` pins the geometry, but a swapped `v0`/`v1` in the reverse cap UV would still pass every test.
   - **Recommendation:** optionally add the cap and junction-side `UVRect` to `HoldLayout`, or write a small pure helper, so `note_field_renderer_test` can assert both directions. The owner's visual check (AC 4) currently covers this.
3. **Low**: `src/gameplay/noteskin.hpp:38-39`. The edited `tile_scale` comment wraps unevenly: line 38 is 76 columns and line 39 is 98, while the neighbouring comment block wraps at about 78. Re-wrap the comment so the two lines are similar in length.

**Noted, not a finding** (settled in the plan or by the owner):
- The cap is centred on the tail (OpenITG/SM5 Cel parity), not flush with it, so an unheld hold's rounded end passes the receptor centre half a note after the release beat. The owner chose this in the plan's Open Questions, and switching to flush only means changing the single constant `kCelHoldBodyStopFromTail`.
- No permanent hold/roll sync chart was added, by owner decision.
- The visual verification for AC 4 (1x/8x, up/down scroll, holds and rolls) is still pending with the owner. The agent can't run a windowed binary.
- `tail_inset_scale` is scaled by `kNoteSize`, while the cap height is scaled by `kNoteSize * width_scale`. Only Cel has caps, and its `width_scale` is 1. This matches SM, where the offset is in arrow units and is independent of the cap frame height.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build (`cmake --build build -j`, GCC 16.2.1, `-Wall -Wextra -Wpedantic`) | PASS: exit 0, no warnings |
| Strict build (scratch dir, `-DCMAKE_CXX_FLAGS=-Werror`, FetchContent disconnected against `build/_deps/*-src`; targets `note_field_renderer_test`, `noteskin_test` and therefore `blaze4k_core`) | PASS. The only warnings come from third-party C code (SDL libm, glad), which `CXX_FLAGS` does not cover |
| Lint | N/A: no linter is configured, so the compiler warnings above serve as the lint gate |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | PASS: 38/38 (baseline 37, plus `note_field_renderer_test`) |
| Skipped / env-guarded tests | None. The "Skipping …" lines in the verbose output are parser log messages, not skipped tests |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | PASS: exit 0 |
| `git diff main --check` | PASS: no whitespace errors |
| Visual check (AC 4) | PENDING: the owner has to run this manually |

## What's Good

- The geometry is a pure, `[[nodiscard]]` free function with a small result struct. This follows the existing `cel_tap_frame` / `cel_explosion_tween` pattern, so the on-screen layout is the one under test.
- The tests are thorough: up and down scroll, an exact `==` check that there is no seam (including non-integer geometry), a regression check against the old overshoot, a hold clipped while held near its end, the exact tail time in both directions (which shows why the explicit `reverse` matters), the head past the whole cap, a short hold, and the cap-less fallback.
- Passing the direction explicitly removes a real ambiguity. At `head_y == tail_y`, the old inferred `reverse` would have drawn the remnant on the wrong side in down-scroll.
- The provenance comments cite the metric, its source file and line, and the upstream functions. The values are correct.
- The change is small and well contained. Holds and rolls share one path, the procedural fallback skin behaves exactly as before, and nothing in judgment or timing was touched.

## Recommendation

This is ready to merge after the owner finishes the visual check for AC 4. The three Low nits are optional: rewording the provenance comment, an optional UV test hook, and a comment re-wrap. None of them blocks the merge.
