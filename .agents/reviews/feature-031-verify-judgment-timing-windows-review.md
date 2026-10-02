# Code Review: feature/031-verify-judgment-timing-windows

**Scope**: Branch `feature/031-verify-judgment-timing-windows` vs `main` (no commits on the branch; 9 uncommitted tracked modifications plus the untracked plan/report under `.agents/`), GitHub issue #57
**Recommendation**: APPROVE (with two Low nits)

## Summary

Spike #57 verifies Blaze 4k's judgment windows against OpenITG `f2c129fe` and fixes the two trivial mismatches it found:
`judge_window_scale` / `judge_window_add` are now applied through a single `JudgmentConstants::effective_windows()`
helper (M1), and the default `judge_window_add` is now the dedicated-cabinet `0.0015` (M2). MercifulBeginner (M3) is
filed as #67. I re-fetched the cited OpenITG files at the pinned commit and every reference the change relies on checks
out. The engine reads every judge window through the helper, `pad_stick` stays raw, and the tests discriminate between
base, default-effective and widened windows. Only two cosmetic/test-precision nits remain.

## Upstream verification (re-done for this review)

Fetched from `raw.githubusercontent.com/openitg/openitg/f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`:

| Claim | Upstream | Verified |
|---|---|---|
| `fSecs *= JudgeWindowScale; fSecs += JudgeWindowAdd` for tap and mine windows | `src/Player.cpp:34-58` (`AdjustedWindowTap`, scale `:49`, add `:50`) | Yes |
| Same for hold/roll windows | `src/Player.cpp:60-74` (`AdjustedWindowHold`) | Yes |
| Mine edge `<= ADJUSTED_WINDOW_TAP(TW_Mine)`; tap tiers `<=` | `src/Player.cpp:948`, `:957-961` | Yes |
| Miss expiry uses adjusted Boo | `GetMaxStepDistanceSeconds`, `src/Player.cpp:1710-1713` | Yes |
| Base windows and home add 0 | `metrics.ini:90-101` (`[Preferences]`, `JudgeWindowAdd=0.000000` at `:91`) | Yes |
| Cabinet add 0.0015 | `metrics.ini:262` in `[Preferences-cabinet]` (`:239`, `Fallback=Preferences-arcade`) | Yes |
| Cabinets launch with `--type=Preferences-cabinet` | `assets/arcade-patch/start-3.sh:17` (default branch) | Yes |
| `pad_stick` never adjusted | `PadStickSeconds` used raw at `Player.cpp:635,1474-1478` | Yes |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`tests/judgment_engine_test.cpp:870-891`: section 18 does not pin edge inclusivity in the engine.**
   Both cases probe `w.way_off - 1e-6`, so they would still pass if `expire_notes` used `>` instead of `>=`
   (`judgment_engine.cpp:223`) or if `handle_step_mine` used `>=` instead of `>` (`:143`). The mine edge is not probed
   at all. The exact-edge inclusivity is pinned for `classify_tap` in `judgment_constants_test` (section 2,
   `std::nextafter`), so tap tiers are covered. The engine-level comparisons are not.
   *Recommendation:* either rename the section and its log line to "just inside the edge" or add exact-edge probes
   that are exact in doubles. For example, use a note at `0.0` so `delta == music_time`: an `update(w.way_off)` gives no
   Miss, and a mine stepped at exactly `w.hit_mine` gives `HitMine`.

2. **`src/gameplay/judgment_engine.cpp:290-291`: the `ADJUSTED_WINDOW_HOLD` provenance comment is detached from the
   code it documents.** It sits after `held_now` and above `double life`. The `w` it describes is declared at `:266`,
   and the comment does not mention it.
   *Recommendation:* move the comment onto the `const TimingWindows w = ...` line at `:266`, matching the other two
   sites (`:141-142`, `:212-214`).

**Noted, not a finding:**
- Every effective window is 1.5 ms wider than before, so high scores are not directly comparable with older local
  records. The plan accepted this (Risks, D1/Open Question 1). It can be reverted with one JSON key.
- MercifulBeginner (M3) is not implemented. It is tracked in #67, as the plan intended.
- The cabinet `GlobalOffsetSeconds=-0.012` is not adopted (intentional: Blaze calibrates per user, #20).
- Miss expiry compares seconds, while OpenITG compares rows (difference of at most 1/48 beat). This predates the change and the plan flagged it.
- The effective windows have no upper bound. A huge user `judge_window_add` could push Way Off past
  `kStepSearchDistanceSeconds` (1.0 s). This predates the change (base windows were never upper-bounded either). It is user-config only.
- `metronome_sync_test` now uses the base Fantastic (21.5 ms) as its sync tolerance and the effective Fantastic
  (23.0 ms) as its classification boundary. The test comments document this deliberate split, and the stricter tolerance is the conservative choice.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j`) | PASS, no warnings or errors |
| Lint (no linter configured; `-Wall -Wextra -Wpedantic -Werror -fsyntax-only` re-run on all 6 touched C++ sources/tests using the build's own flags) | PASS |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | PASS, 38/38 |
| Skipped / env-guarded tests | None skipped. The guarded audio section of `music_clock_test` ran (AudioEngine initialized, 48 kHz) |
| Raw judge-window reads outside `judgment_constants.*` / loader (grep over `src/`) | PASS. Only `pad_stick` is read raw (`judgment_engine.cpp:363`), which is correct |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | PASS, exit 0, shipped JSON loaded with no fallback warning |
| Upstream references (OpenITG `f2c129fe`) | PASS, see table above |
| Scope guard (`.agents/stories/todo-stories.md`) | PASS, still `??`, untouched |

## What's Good

- **One formula, one place.** `effective_windows()` keeps `base * scale + add` in a single function, and returning
  scale 1 / add 0 makes double application a no-op. A test pins that.
- **Every OpenITG `ADJUSTED_WINDOW_*` site is mirrored.** That covers tap tiers, the mine window, miss expiry
  (including the synthetic Miss/AvoidedMine `hit_time`/`delta_ms`), and hold/roll decay. `pad_stick` is correctly left raw.
- **Validation is extended soundly.** `scale <= 0` and non-positive or non-finite effective windows are rejected, and
  the loader falls back to defaults. IEEE rounding is monotonic, so the claim that base monotonicity plus a positive
  scale and a common add keeps the effective windows monotonic holds.
- **The tests discriminate.** Section 17 probes values such as `k.windows.way_off + 0.005`, which lie past the
  *default-effective* window but inside the widened one. That proves the add is actually applied and not merely
  tolerated. The constants test sweeps every tier edge with `std::nextafter` on both signs.
- **The offset is shown to apply once.** `music_clock_test` 7b drives the real `MusicClock` → `music_time_for_event`
  → `JudgmentEngine` chain and checks both delta 0 and the early-negative sign.
- **Provenance is precise.** Every cited file:line resolves correctly at the pinned commit.
- **The deviations are honest.** The implementation report lists the `metronome_sync_test` and hard-coded mine-edge
  fixes the plan missed.

## Recommendation

Ready to commit and open a PR. The two Low items are optional polish: tighten or rename engine test section 18, and move
the hold-window comment. Neither blocks merge.
