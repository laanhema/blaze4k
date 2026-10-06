# Implementation Report

**Plan**: `.agents/plans/completed/055-mine-damage-openitg-parity-plan.md`
**Branch**: `feature/055-mine-damage-openitg-parity`
**Status**: COMPLETE (uncommitted on the branch, per the project git policy)

## Summary

Spike #118 checked Blaze 4k's mine damage against OpenITG (`f2c129fe…`) and StepMania 5 (`825467bc…`). Every mine value and the
life-path order of operations already match OpenITG, so **no values changed**. The spike found two small mismatches and fixed both:

- **M1**: `LifeKeeper` had no per-mine duplicate guard. `JudgmentKind::HitMine` now goes through a new `consume_hit_mine`, which
  mirrors `consume_hold_outcome`. It bounds-checks the index, accepts only `NoteType::Mine` notes and reuses `note_scored_` as the
  once-per-mine guard (OpenITG grades a hit mine at `Player.cpp:1096`).
- **M2**: the `hot_downgrade` comment in `src/timing/judgment_constants.hpp` now says the value is the hard-coded `-0.10f` literal at
  `LifeMeterBar.cpp:119` (tap/mine) and `:175` (hold NG). It previously cited `PrefsManager.cpp`, which has no such preference.

Both upstream line references were fetched again during implementation and still match (`LifeMeterBar.cpp:119,175`, `Player.cpp:1096`).

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Guarded `consume_hit_mine` (M1) | `src/gameplay/life_keeper.hpp`, `src/gameplay/life_keeper.cpp` | ✅ |
| 2 | Fix the `hot_downgrade` provenance comment (M2) | `src/timing/judgment_constants.hpp` | ✅ |
| 3 | Tests 10 and 11 now use real mine notes | `tests/life_keeper_test.cpp` | ✅ |
| 4 | New block 20 (20a–20f): mine parity | `tests/life_keeper_test.cpp` | ✅ |
| 5 | Full suite + headless smoke | — | ✅ |
| 6 | Verification Report posted on #118 | GitHub issue comment | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build`) | ✅ exit 0. Touched files were rebuilt: no warnings or errors |
| Lint (no linter; warnings gate) | ✅ no new warnings |
| Full tests (sandboxed ctest) | ✅ 51/51 passed (2.90 s) |
| Targeted tests (life_keeper, judgment_engine, score_keeper, judgment_constants, gameplay_screen) | ✅ 5/5 |
| `life_keeper_test` direct run | ✅ all blocks 1–20f pass |
| Red check: block 20 against the pre-fix keeper | ✅ fails as expected at 20a's duplicate assertion (life 0.40 ≠ 0.45). The fix was then restored |
| Values untouched (`git diff --exit-code` JSON + `judgment_constants.cpp`) | ✅ no diff |
| Scope guard (`git diff --stat`) | ✅ only the 4 planned files |
| Headless smoke (`--headless --smoke-test 10`, sandboxed) | ✅ exit 0. `[JudgmentConstants] Loaded 'assets/data/judgment_constants.json'` with no constants fallback warning. ALSA "cannot find card" lines come from the sandbox's tmpfs `/dev/snd` and are expected |
| Owner play-test (interactive) | ⏳ owner-only, not run. Mines should cost the same as before |
| Optional `/verify` visual check | not run. The plan marks it optional because no values or rendering change |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/life_keeper.cpp` | UPDATE | +16/-1 |
| `src/gameplay/life_keeper.hpp` | UPDATE | +2/-1 |
| `src/timing/judgment_constants.hpp` | UPDATE (comment only) | +4/-2 |
| `tests/life_keeper_test.cpp` | UPDATE | +205/-2 |

## Deviations from Plan

- **20a** has one extra assertion: a negative `note_index` (`make_mine(-1, 0)`) is also ignored. It covers the lower bounds check
  next to the planned out-of-range case.
- **20e** asserts `idx == 63` outright, which pins the plan's fill arithmetic (0.5 → 1.0 takes 63 rows).
- **Test 11 mine sub-case** also asserts `life() == 1.0` before the mine, matching the WayOff and hold sub-cases.

There were no functional deviations. The resolved decisions were applied as stated:
- **OQ1 (ScoreKeeper duplicate-mine DP guard)**: out of scope for #118 and not added. A possible follow-up is a 3-line guard in
  `score_keeper.cpp:81-85` plus one `score_keeper_test` case. Players are not affected today, because the engine emits one `HitMine` per mine.
- **OQ2 (gentler-than-OpenITG mine option)**: no. AGENTS.md principle 2 keeps the OpenITG values. Users can already tune
  `life_deltas.hit_mine`, `hot_downgrade` and `regen_combo_after_miss` in `data/judgment_constants.json` without a rebuild.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/life_keeper_test.cpp` | 10 (now uses a real mine note); 11 mine sub-case (a real mine at index 70); 20a duplicate/foreign/out-of-range `HitMine` events are ignored, and a mine event on a tap index does not consume the tap; 20b stepped-on then held mine → one `HitMine`, life 0.45 (engine → keeper); 20c held-through mine at 0.01/0.1/0.001 s update grids → one `HitMine`, life 0.45; 20d mine regain debt (4 suppressed, 5th pays) and the mine + miss debt shared up to 10; 20e full-bar mine → hot −0.10, then debt; 20f a same-beat mine never joins the tap row |
