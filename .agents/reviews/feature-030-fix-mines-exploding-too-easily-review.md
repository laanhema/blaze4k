# Code Review: feature/030-fix-mines-exploding-too-easily

**Scope**: Branch `feature/030-fix-mines-exploding-too-easily` vs `main`. There are no commits yet, so the scope is the 10 modified tracked files plus the untracked `.agents/plans/completed/030-…-plan.md` and `.agents/reports/030-…-plan-report.md`. `.agents/stories/todo-stories.md` is unrelated and out of scope. GitHub issue #56.
**Recommendation**: APPROVE (with nits)

## Summary

The fix replaces the old held-over-mine check with an OpenITG-style crossing cursor. Before, the check re-tested every past, unexpired mine on every update. Now each mine is checked once, as it crosses, `pad_stick` behind the music. The cursor also has a pad-stick held-duration gate and routes the explosion through the shared `step` routine. I checked the logic line by line against OpenITG `Player.cpp` at the pinned commit `f2c129fe`:

- the cursor at `:632-646`
- `CrossedMineRow` at `:1461-1488`
- `HandleStep`, including the roll refresh on `bHeld`, at `:846-1222`

It matches. The new constant follows the project's data-driven pattern end to end, and the tests cover every acceptance criterion of #56. I found only two Low items: a backward clock resync rewinds the crossing window, and the engine has no test for the `pad_stick = 0` path.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/gameplay/judgment_engine.cpp:355` (with `:194-197`): a backward clock resync rewinds the crossing window, so a mine can be crossed twice.**
   - **Cause.** The lower bound is taken from `last_update_time_`, and the backward-jump branch in `update` lowers that value. Mines that already crossed come back into the next update's `(lower, cursor]` window. That contradicts the comment at `:340` ("Each mine is evaluated exactly once").
   - **Reproduced in a scratch harness linked against `libblaze4k_core.a`:**
     - (a) A mine crossed while the column was unheld is crossed again after `update(1.95)` following `update(2.06)`. A later held update then explodes it, which is the #56 symptom.
     - (b) A mine that was already hit is crossed again. Under the accepted graded-mine quirk, that `step` judges the next same-column tap with no press (a `Decent` tap).
   - **Reachability.** It is narrow. `SoundStream::get_position_frames` clamps the cursor while playing, so the only reachable path is the stub→audio clock-source switch in `GameplayView::update` (`gameplay_view.cpp:166-170`), and seeking is out of scope for v1.
   - **Fix.** Keep a monotonic crossing high-water mark, for example a `mine_cursor_` that only moves forward, separate from `last_update_time_`. That makes the "exactly once" claim hold unconditionally.
2. **`tests/judgment_engine_test.cpp:541` (section 16): the engine has no test for `pad_stick = 0`.**
   - Pinned Semantics 5 and the comments in `judgment_constants.hpp` and `.cpp` say 0 is legal and gives OpenITG's `IsButtonDown` branch. In that branch a press in the same tick as the crossing counts, and `hit_time == now`.
   - `judgment_constants_test` 5b only proves that the value loads and validates. No engine case runs the crossing with `P = 0`.
   - Fix: add one small case, a copy of `k` with `pad_stick = 0` plus a mine at 2.0, `handle_step(0, 1.99)` and `update(2.0x, held)`. Expect `HitMine` with `hit_time == now`.

**Noted, not findings** (scoped out or accepted in the plan):
- Graded-mine quirk: a held crossing of a mine that was already hit runs the full step (Open Question 1, pinned by 16.11). The owner accepted it.
- Press time is tracked per column/action rather than per physical key.
- `judge_window_scale/add` are not applied to any window (#57).
- `pad_stick >= way_off` makes held crossings impossible. This was accepted.
- The seconds-vs-rows difference around stops and warps.
- A frame-hitch crossing beyond the mine window can judge a nearby same-column tap with no press. This is the same as OpenITG `Step(t, now - PadStick)`.

## Validation Results

| Check | Status |
|-------|--------|
| Type check / build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`; touched sources and tests force-rebuilt) | PASS (no warnings, no errors) |
| Lint (no linter configured; the compiler warnings above are the gate) | PASS |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | PASS, 38/38. No `GTEST_SKIP` or env-guarded skips; the only "Skipping" output is expected parser log lines |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | PASS (exit 0) |
| OpenITG parity (fetched `src/Player.cpp` at the pinned commit `f2c129fe`) | PASS. Cursor, pad-stick gate, `Step(now - P, bHeld)` and the roll refresh on held steps all match |
| Issue AC 1 (root cause posted on #56) | PASS. The comment is present |

## What's Good

- **Correct root cause and a faithful fix.** The diagnosis is right: crossings were re-checked until the mine expired. The cursor and pad-stick semantics map exactly onto OpenITG, and the provenance comments cite the right lines.
- **Grounded deviation from the plan.** Using `continue` instead of `break` is justified by the beat-ordered notes and the accepted warps (non-monotonic seconds).
- **`pad_stick` plumbed like the other constants.** It goes through the struct default, compiled default, loader, seed JSON, `validate` and `same_constants` parity. It is correctly kept out of `positive_windows` and out of judge-window scale/add.
- **Strong tests.** They include:
  - the exact #56 regression (16.2)
  - both pad-stick edges (16.5 and 16.6)
  - frame-rate independence across three update grids (16.7 and 16.10)
  - the hitch inside and beyond the window (16.9)
  - the pinned quirk (16.11)
  - section 10 hardened off an exact float boundary
- **Input audit gap closed.** The stuck-held state after a remap is fixed with a minimal change and has a test (7b).

## Recommendation

Mergeable as is. Optionally close the two Low items before the PR:

1. Add a monotonic mine-crossing cursor, so a backward resync cannot cross a mine twice.
2. Add a `pad_stick = 0` engine case.

Then run the owner's interactive play-test from the plan's End-to-End Verification on a mine-heavy ITG chart.
