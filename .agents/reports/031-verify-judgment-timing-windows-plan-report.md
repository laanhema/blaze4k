# Implementation Report

**Plan**: `.agents/plans/completed/031-verify-judgment-timing-windows-plan.md`
**Branch**: `feature/031-verify-judgment-timing-windows`
**Status**: COMPLETE

## Summary

Spike #57: Blaze 4k's judgment windows were verified against OpenITG
(`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`, references re-checked against a fresh clone). All 8 base windows, edge
inclusivity and the single global-offset application match. Three mismatches were found and handled:

- **M1 (fixed):** `judge_window_scale` / `judge_window_add` were loaded but never applied. A new
  `JudgmentConstants::effective_windows()` returns `base * scale + add` for the 8 judge windows (`pad_stick` is copied
  unchanged; the result has scale 1 / add 0, so it can't be applied twice). `classify_tap`, the mine window, miss
  expiry (threshold plus synthetic Miss/AvoidedMine `hit_time`/`delta_ms`) and hold/roll decay all read it, mirroring
  OpenITG `AdjustedWindowTap/Hold` (`Player.cpp:34-74`) and `GetMaxStepDistanceSeconds` (`:1710-1713`). `validate()`
  now rejects `judge_window_scale <= 0` and any effective window `<= 0`.
- **M2 (fixed, owner decision D1):** default `judge_window_add` = **0.0015**, the dedicated-cabinet
  `[Preferences-cabinet] JudgeWindowAdd` (`metrics.ini:262`, selected by `assets/arcade-patch/start-3.sh:17`), in the
  compiled defaults and the shipped JSON. Effective windows: Fantastic 23.0 ms, Excellent 44.5, Great 103.5, Decent
  136.5, Way Off 181.5, Mine 71.5, Hold OK 321.5, Roll 351.5.
- **M3 (follow-up):** OpenITG MercifulBeginner, filed as #67 (Low priority).

The verification report was posted on #57:
https://github.com/laanhema/blaze4k/issues/57#issuecomment-5957892004

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `effective_windows()`, `classify_tap` via effective windows, validation of scale and effective windows, provenance comments | `src/timing/judgment_constants.hpp`, `src/timing/judgment_constants.cpp` | ✅ |
| 2 | Route mine window, miss expiry and hold/roll decay through `effective_windows()`; `pad_stick` stays raw | `src/gameplay/judgment_engine.cpp` | ✅ |
| 3 | Default `judge_window_add = 0.0015` (compiled default plus seed JSON and its `source` string) | `src/timing/judgment_constants.*`, `assets/data/judgment_constants.json` | ✅ |
| 4 | Constants tests: parity, effective values, arithmetic, exact-edge inclusivity, validation, home-timing override | `tests/judgment_constants_test.cpp` | ✅ |
| 5 | Engine tests: `w = k.effective_windows()` swap; scale/add reach tap, mine, expiry, hold, roll; inclusive edges | `tests/judgment_engine_test.cpp` | ✅ |
| 6 | Offset-applied-once composition test (MusicClock → music_time_for_event → JudgmentEngine) | `tests/music_clock_test.cpp` | ✅ |
| 7 | Comment touch-up (effective way_off 0.1815 s) | `tests/gameplay_screen_test.cpp` | ✅ |
| 8 | Full suite plus headless smoke | — | ✅ |
| 9 | Verification report posted on #57 | GitHub | ✅ |
| 10 | MercifulBeginner follow-up filed (#67, labels bug/timing/gameplay/scoring) | GitHub | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j`, warnings grep as lint gate) | ✅ no warnings, no errors |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | ✅ 38/38 passed |
| Targeted (`judgment_constants|judgment_engine|music_clock|metronome_sync|gameplay_screen|score_keeper|life_keeper`) | ✅ 7/7 passed |
| No raw judge-window reads outside `judgment_constants.*` / loader (grep) | ✅ no output |
| Headless smoke (`./build/blaze-4k --headless --smoke-test 10`) | ✅ exit 0, `[JudgmentConstants] Loaded 'assets/data/judgment_constants.json'`, no fallback warning |
| Scope guard (`.agents/stories/todo-stories.md`) | ✅ still `??`, untouched |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/judgment_constants.hpp` | UPDATE | +23/-3 |
| `src/timing/judgment_constants.cpp` | UPDATE | +50/-7 |
| `src/gameplay/judgment_engine.cpp` | UPDATE | +16/-8 |
| `assets/data/judgment_constants.json` | UPDATE | +2/-2 |
| `tests/judgment_constants_test.cpp` | UPDATE | +104/-11 |
| `tests/judgment_engine_test.cpp` | UPDATE | +133/-21 |
| `tests/music_clock_test.cpp` | UPDATE | +62/-0 |
| `tests/metronome_sync_test.cpp` | UPDATE | +6/-2 |
| `tests/gameplay_screen_test.cpp` | UPDATE | +1/-1 |

## Deviations from Plan

1. **`tests/metronome_sync_test.cpp` changed (not in the plan's file list).** The plan expected it to pass untouched,
   but its window-boundary sweep (`0.999 * fw` → Fantastic, `1.001 * fw` → Excellent) used the base Fantastic
   (21.5 ms). With the 1.5 ms add, `1.001 * 21.5 ms` is still Fantastic, so it failed
   (`metronome_sync_test.cpp:139: delta.window == c.expected`). Fix: the sweep now uses
   `k.effective_windows().fantastic` (the real `classify_tap` boundary). The sync tolerance `one_window_ms` stays on
   the stricter base 21.5 ms, as the plan intended.
2. **Hard-coded mine edge in `judgment_engine_test.cpp` 16.1.** The plan's grep-and-replace only covered
   `k.windows.*` reads. Test 16.1 also had a literal `2.0 + 0.071` ("just outside" the 70 ms mine window), which now
   falls inside the effective 71.5 ms window. It is now `2.0 + w.hit_mine + 0.001`, which keeps the intent.
3. **Section 2 of `judgment_constants_test.cpp` rewritten.** It hard-coded base edges (`0.0215 + kEps` → Excellent,
   etc.), which are wrong under the new add. It now loops over the effective edges with `std::nextafter` (this also
   covers the plan's "exact-edge inclusivity" item), and it keeps the NaN → Miss and symmetry checks.
4. **Task ordering:** the 0.0015 default in the header and `compiled_defaults` went in during Task 1 rather than
   Task 3, so the Task 2 "38 still pass under identity" checkpoint was not observed separately. Instead the full
   suite was brought to 38/38 after Tasks 4–7.
5. **Extra tests beyond the plan:** roll-window widening (17.5), re-adjusting an effective set is a no-op, and
   `classify_tap` on a scaled set.
6. `grep -n "k.windows"` found 20 replacement lines (the plan estimated 23 sites; some lines hold two reads).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/judgment_constants_test.cpp` | 1: parity add 0.0015; 1b: effective values (8 windows, pad_stick unadjusted, scale 1 / add 0), idempotence, scale-before-add arithmetic, classify on scaled set; 2: inclusive/symmetric edges on all 5 tiers via `nextafter`, NaN; 9b3: scale 0 and add -0.03 rejected (direct + JSON loader fallback); 9b4: JSON add 0 → home 21.5 ms Fantastic |
| `tests/judgment_engine_test.cpp` | All window reads moved to effective windows; 17.1 tap, 17.2 mine (vs add-0 control), 17.3 miss expiry with exact `hit_time`/`delta_ms`, 17.4 hold OK, 17.5 roll, all under add 0.02; 18: inclusive step edge and no expiry at the edge |
| `tests/music_clock_test.cpp` | 7b: offset applied once through MusicClock → music_time_for_event → JudgmentEngine; delta 0 at +50 ms offset; −40 ms (early, Excellent) at +10 ms; aged event gets no second offset |
| `tests/metronome_sync_test.cpp` | Boundary sweep updated to effective Fantastic |
