# Implementation Report

**Plan**: `.agents/plans/completed/064-stub-clock-reset-between-plays-plan.md`
**Branch**: `feature/064-stub-clock-reset-between-plays`
**Status**: COMPLETE

## Summary

`GameplayView::init` now resets the three fields that say which clock a play uses (`stub_frames_`, `use_stub_`, `audio_started_`) before it loads the audio. `GameplayScreen` keeps one `GameplayView` for the whole session, so before this change the second play without audio started where the first one stopped (#138).

The same reset also fixes the two mixed sequences that the plan measured on `main`: a play with audio after a play without it now runs on the audio clock, and a play without audio after a play with it now runs on the stub clock from zero. The owner decided Open Question 1 (reset all three fields, keep both mixed-sequence tests) and Open Question 2 (new file and target `gameplay_view_test`) before this run.

The new test `gameplay_view_test` covers all three sequences on one reused view. The "Known defect" line is gone from the verify skill's gameplay feature file. The change came from the plan's prototype patch, which applied cleanly to `main` @ `3c96079`. Nothing is committed: the work is uncommitted changes on the branch.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | See the test fail without the fix, then pass with it | `src/gameplay/gameplay_view.cpp`, `tests/gameplay_view_test.cpp` | PASS |
| 2 | Read the patched code against the decisions (resets before the audio load and before `bind_clock_source`; one view per case; empty path for "no audio") | `src/gameplay/gameplay_view.cpp`, `tests/gameplay_view_test.cpp`, `tests/CMakeLists.txt` | PASS |
| 3 | Check the verify recipe in the real app | `.claude/skills/verify/features/gameplay-and-results.md` | PASS |
| 4 | Full checks | the four files of "Files to Change" | PASS |

## Validation Results

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Type check / build | PASS (no warning in project sources, `-DBLAZE_WERROR=ON`) | `scripts/check.sh` step 3, and `cmake --build build -j"$(nproc)"` — host, GCC, Release |
| Lint | PASS (`LINT: PASS`) | `scripts/check.sh` step 1, and `scripts/lint.sh` — host |
| Tests | PASS (53 passed of 53, `CHECK: PASS`) | `scripts/check.sh` — host, null audio device |
| Neighbour tests | PASS (6 passed of 6) | `ctest --test-dir build --output-on-failure -R "gameplay_view_test\|gameplay_screen_test\|score_keeper_test\|life_keeper_test\|perf_loop_test\|music_clock_test"` — host |
| New test, repeated | PASS (10 of 10 runs) | `./build/tests/gameplay_view_test` ten times — host |
| Sanitizers | PASS (55 passed of 55, no sanitizer report) | `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan` — host |
| Reproduction | PASS (red before the fix, green after it) | `cmake --build build -j"$(nproc)" && ctest --test-dir build --output-on-failure -R gameplay_view_test` — host |
| E2E 1: automated | PASS | `gameplay_view_test`, all three cases |
| E2E 2: real app | PASS | `verify` skill, fixture pack (stub clock), run dir `/tmp/blaze4k-verify/138-stub-clock` |
| E2E 3: mixed sequences in the real app | Not run (the plan marks it optional and not required; the repo ships no pack with one working and one broken song) | — |
| Scope guard | PASS (the four planned files, plus the `.agents/` artifacts) | `git status -sb` |

### Reproduction, red

With only the test and its registration applied (the fix not yet in `src/gameplay/gameplay_view.cpp`), the test aborted at the `music_time_seconds()` check that follows the second `init` in `test_stub_clock_starts_at_zero_on_each_load`, as the plan predicted:

```
1/1 Test #42: gameplay_view_test ...............Subprocess aborted***Exception:   0.16 sec
Assertion failed at /home/lauri/github/blaze4k/tests/gameplay_view_test.cpp:66: approx(view.music_time_seconds(), 0.0)
```

With the fix applied the same command passes (`gameplay_view_test ... Passed 0.04 sec`). The test's own log then reads `time source stub`, `time source stub`, `time source stub`, `time source audio` (case 2, second load), `time source audio`, `time source stub` (case 3, second load).

### E2E 2, real app

Three plays in one instance on the fixture pack (every play logs `time source stub`). Evidence is in `/tmp/blaze4k-verify/138-stub-clock/evidence/`.

| Play | Chart | `game.log` | Screenshot |
|---|---|---|---|
| 1 | `Blaze Anthem` Challenge 10 | `[GameplayView] Failed: life empty at 1.28333s`, `Score: DP -72/70 ... M 6 ... life 0.000 FAILED` | `01-first-play.png`: arrows on the field |
| 2 | `Blaze Anthem` Challenge 10 | `[GameplayView] Failed: life empty at 1.28333s`, `Score: DP -72/70 ... M 6 ... life 0.000 FAILED` | `02-second-play.png`: the same arrows on the field as play 1 |
| 3 | `Blaze Anthem` Beginner 1 | `Score: DP 0/30 (0.00%) grade D ... M 4 ... life 0.100 alive`, `[ResultsScreen] Blaze Anthem Beginner 1: D 0.00% DP 0/30 CLEARED NEW RECORD` | `04-beginner-results.png`: MISS `4`, `0 / 30`, NEW RECORD ribbon |

Play 3 is not in the plan's E2E list. It was added for Task 3, to check the recipe's "Pass and save" step in the real app as a third play of one instance. Every number in the recipe matched, so `.claude/skills/verify/features/gameplay-and-results.md` has no change beyond the removed "Known defect" line. `evidence/scores.json` holds one record (`"grade": "D"`, `"percent": 0.0`, `"dp": 0`), as the recipe says.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/gameplay_view.cpp` | UPDATE | +5/-0 |
| `tests/gameplay_view_test.cpp` | CREATE | +149 |
| `tests/CMakeLists.txt` | UPDATE | +11/-0 |
| `.claude/skills/verify/features/gameplay-and-results.md` | UPDATE | +0/-1 |

## Deviations from Plan

1. **Task 1 order.** The plan applies the whole patch and then reverses the fix. This run applied the test part first (`git apply --include='tests/*'`), saw it red, and then applied the rest (`git apply --exclude='tests/*'`), because `/implement` asks for the reproduction to go red before the first code change. The tree at the red run is the same one that the plan's two commands produce.
2. **E2E 2 needed a wait before Confirm on Results.** The plan's step reads "`$B wait-screen RUN Results 15`. Then `$B keys RUN Return` and `$B wait-screen RUN Select`". The first attempt followed it and stopped with `FAIL: screen is Results, not Select, after 15s`: a Confirm during the 2.4 s results reveal only skips the reveal (a gotcha that the feature file already lists). The second attempt ran `$B keys RUN wait:3` before each Confirm on Results, as the feature file's recipe does, and passed. No code or doc change came from this. The first attempt's log is kept in `/tmp/blaze4k-verify/138-stub-clock/evidence-attempt1/`.
3. **One extra play in E2E 2** (the Beginner pass, see above), added to cover Task 3's recipe check in the real app.

Out of scope and unchanged, as the plan's Risks table lists: the stub fallback itself, the "demo harness only" wording, and the `std::fixed` state that `GameplayView::shutdown` leaves on `std::cout` (the second `Speed X-mod` line of a session prints `1.000`; seen again in this run).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/gameplay_view_test.cpp` | `test_stub_clock_starts_at_zero_on_each_load` (no audio twice on one view: chart time `0.0` after each load, `0.25` after one step with no judgment, `2.0` with one Miss after eight steps); `test_audio_play_after_stub_play_uses_audio_clock` (no audio, then a 1 s WAV: the clock advances with no `update` call and thirty 1 s steps do not move it past the file's end); `test_stub_play_after_audio_play_uses_stub_clock` (a 1 s WAV, then no audio: the clock starts at `0.0` and reads `0.5` after two 0.25 s steps) |
