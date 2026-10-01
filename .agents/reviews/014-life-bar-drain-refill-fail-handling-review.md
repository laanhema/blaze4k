# Code Review: feature/014-life-bar-drain-refill-fail-handling

**Scope**: Branch `feature/014-life-bar-drain-refill-fail-handling` vs `main`, including all uncommitted
modifications and untracked files (issue #14, [B6] Life bar with drain/refill and fail handling). The branch
has **no commits**; the entire change set is uncommitted. Untracked `src/gameplay/life_keeper.{hpp,cpp}`,
`tests/life_keeper_test.cpp`, and the `.agents` plan/report were read directly.
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the new pure `LifeKeeper`/`LifeState` module, the quads-only HUD life bar, the `GameplayView`
integration (`fail_enabled`, `exited_`, `outcome()`), the `--fail-off` demo flag, and the two new B2 constants
(`hot_downgrade`, `regen_combo_after_miss`) with their JSON seed/loader/validation/test updates. The life core
is genuinely event-sourced and pure (no SDL/GL/miniaudio/`<chrono>`/frame-delta input; verified by `rg`), row
grouping matches B5's exact-`Note.beat` rule, and the hot-downgrade, regen-window, clamp, freeze, and Fail-Off
behaviors all match OpenITG at `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`. Build is clean (0 warnings under
`-Wall -Wextra -Wpedantic`) and `ctest` is 14/14. Findings are narrow parity/robustness items; none is a
crash, data-loss, or core-logic defect.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

1. **`src/gameplay/life_keeper.cpp:220-233` — fail is decided per event, so a saving hit later in the same
   drained batch cannot rescue a player, unlike OpenITG.** OpenITG's `ChangeLife` never sets `bFailed`; it only
   freezes on `bFailed` (`LifeMeterBar.cpp:229-231`), and `bFailed` is set once per frame in
   `ScreenGameplay::Update` (`ScreenGameplay.cpp:1471-1493`). Because Blaze 4k sets `state_.failed` inside
   `apply()` and then zeroes every subsequent delta in the same `life_.consume(new_events_)` loop, a fatal Miss
   followed by a hit in the same frame fails the player, whereas OpenITG would see `life > 0` at its end-of-frame
   check and survive. The plan calls the two "equivalent"; they are only equivalent when no positive event
   follows the fatal loss within a frame. Reachable today (`handle_input_events` and `judge_.update` both append
   into the same `new_events_` batch). Narrow edge case; fix would be to evaluate fail once after consuming the
   whole batch (or document the intentional divergence).

### Low Priority / Suggestions

2. **`src/gameplay/gameplay_view.cpp:167-170` — the fail log reports the wrong music time in audio mode.**
   `audio_.stop()` calls `ma_sound_seek_to_pcm_frame(..., 0)` (`src/audio/sound_stream.cpp:112`), and `clock_`'s
   source reads the audio position, so `clock_.time_seconds()` in the `[GameplayView] Failed: life empty at <t>s`
   line is 0 (plus offset), not the fail time. The same reset also snaps the note field back to the song start in
   the next `render()` while the (out-of-scope) screen transition is still missing. Capture the time before
   stopping, or don't seek on fail.
3. **`src/gameplay/life_keeper.cpp:216-218` — combo-to-regain debt is re-armed to exactly 5 on every loss.**
   OpenITG accumulates successive losses up to `MaxRegenComboAfterMiss=10` and bumps to `RegenComboAfterFail=10`
   on the failing hit (`LifeMeterBar.cpp:221-227,244-253`; `PrefsManager.cpp:119-122`), so after two consecutive
   misses Blaze 4k lets the player recover after ~4 more hits while OpenITG needs ~9. The report documents this as
   an intentional omission of unrequested constants; flagging only because the task states "full OpenITG parity
   is intended" — confirm scope. (The single-loss window itself is correct: debt 5 suppresses the next 4 gains
   and the 5th pays, matching OpenITG.)
4. **`src/gameplay/gameplay_view.cpp:101-129,152` — gameplay input is still forwarded after failure.**
   `handle_input_events` does not check `exited_`, so post-fail presses keep appending `JudgmentEvent`s to the
   engine log (never drained, since `update` early-returns) and can mutate note-hidden state used by `render()`.
   Harmless while the screen transition is unimplemented, but the gameplay input path should be closed on fail.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`) | PASS (0 warnings/errors) |
| Lint | N/A (no linter configured for this project) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (14/14, incl. `life_keeper_test`) |

Additional checks:
- **Purity** (`rg` over `src/gameplay/life_keeper.{hpp,cpp}` for `SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter|GetTicksNS|fixed_dt`):
  clean — no matches. Life derives solely from the B4 `JudgmentEvent` log + chart.
- **Constants / parity** (OpenITG `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`): `hot_downgrade = -0.10` matches the
  hardcoded `-0.10f` (`LifeMeterBar.cpp:118-119,174-175`); `regen_combo_after_miss = 5` matches
  `PrefsManager.cpp:120`. `IsHot() = life >= 1` (`LifeMeterBar.cpp:280-282`) matches `is_hot()`; `score < TNS_GOOD`
  maps to WayOff/Miss/HitMine given the enum order `HIT_MINE<MISS<BOO<GOOD<...` (`GameConstantsAndTypes.h:130-139`),
  so hot never touches Decent/Great/Excellent/Fantastic; hold-NG-only hot matches `HNS_NG` (`:174-175`). The nine
  table deltas match the arcade override `metrics.ini:126-134` (Boo/Miss/HitMine −0.050/−0.100/−0.050,
  MercifulDrain=0), danger tint 0.3 matches `metrics.ini:2565`, and start 0.5 matches the fallback
  `InitialValue` (`fallback/metrics.ini:2385`) — all ratified.
- **B2 regression**: compiled defaults `LifeDeltas{..., false, -0.10, 5}` (`judgment_constants.cpp:33-34`) match
  struct order; loader overrides both fields with correct integral handling (`judgment_constants_loader.cpp:111-112`,
  `override_number` int path `:44-70`); `validate()` keeps all prior finite checks and adds
  `regen_combo_after_miss >= 0` (`judgment_constants.cpp:101-112`); seed deep-equality and JSON-override tests
  extended (`tests/judgment_constants_test.cpp:64-65,128-129,189-191`). `judgment_constants_test` still passes.
- **Float-vs-double underflow**: probed the 0.5-start/`-0.1`-miss sequence in both `float` (OpenITG) and `double`
  (Blaze 4k); both fail on the 6th miss, so the representation change does not shift fail timing here.
- **Row double-counting**: verified jump rows, mixed Great+Miss rows, and hold-head+outcome paths apply exactly one
  delta per row plus one per hold/roll outcome (tests 3, 8, 9, 13); per-note `note_scored_`/`hold_scored_` guards
  make consumption idempotent (an improvement over B5's row-only guard).
- **Purity of B4/B5**: `judgment.hpp`, `judgment_engine.*`, `score_keeper.*` are unmodified.

## What's Good

- The life core is exactly what AGENTS.md demands: a pure, value-type projection of the append-only B4 log with
  no clocks, platform headers, or re-judgment; the `rg` purity gate is clean.
- Row semantics are correctly duplicated from B5 (exact `Note.beat`, mines excluded, miss dominance, greatest
  offset with later-column tie-break), so jumps and mixed rows cannot double-count life.
- Hot-downgrade scope is precise (WayOff/Miss/HitMine/hold-NG only, evaluated against current life at
  resolution time) and the combo-to-regain window is the OpenITG `decrement-then-check` loop with no off-by-one.
- `apply()` faithfully mirrors the OpenITG order: merciful scale → regen/progressive block → failed freeze →
  clamp → fail detection, with `FAIL_THRESHOLD=0` / `life <= 0`.
- Fail-Off is modeled cleanly (`set_fail_enabled` never sets `failed`), life can sit at 0 and recover, and
  `outcome()` gives C1/C7 a clear `Failed`-takes-precedence signal.
- Test suite is broad and mostly hand-computed against OpenITG arithmetic (16 cases incl. engine-produced logs
  and the `GameplayView` boundary), and the B2 seed/loader tests were strengthened rather than regressed.
- HUD bar is solid-quads only (no font/asset), guards degenerate sizes, clamps life, and matches the 0.3 danger
  threshold.

## Recommendation

No changes required to merge. The Medium fail-granularity item is worth a decision (evaluate fail once per
drained batch, or document the deliberate divergence) and the three Low items are cheap polish; none blocks.
Given the explicit "full OpenITG parity" intent, please confirm the combo-to-regain accumulation omission
(item 3) is intended scope.
