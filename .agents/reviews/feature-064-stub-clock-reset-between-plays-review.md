# Code Review: feature/064-stub-clock-reset-between-plays

**Scope**: branch `feature/064-stub-clock-reset-between-plays` against its merge base `3c96079` (`main`), issue #138. The branch has no commits; the whole change is uncommitted (3 modified files, 4 untracked).
**Recommendation**: APPROVE WITH NITS

## Summary

The change resets the three fields that say which clock a play uses (`stub_frames_`, `use_stub_`, `audio_started_`) in `GameplayView::init`, adds the test target `gameplay_view_test` with three cases on one reused view, and removes the "Known defect" gotcha from the verify skill's gameplay feature file. All three acceptance criteria of #138 are met, and each was checked here independently of the implementation report. There is no Critical, High or Medium finding. One Low finding: the third test case does not check its own precondition.

Files read in full: `src/gameplay/gameplay_view.cpp`, `src/gameplay/gameplay_view.hpp`, `tests/gameplay_view_test.cpp`, the `tests/CMakeLists.txt` hunk, `.claude/skills/verify/features/gameplay-and-results.md`, and the caller `GameplayScreen::enter` / `GameplayScreen::exit` (`src/screens/gameplay_screen.cpp`). `src/timing/music_clock.cpp` and `src/audio/sound_stream.cpp` were read for state that survives a reload (none: `MusicClock` holds only the source and the offset, and `SoundStream::load` / `unload` reset the position and the interpolator). The plan and the implementation report under `.agents/` were read as records, not reviewed line by line.

Acceptance criteria of #138:

| # | Criterion | Result | Evidence |
|---|-----------|--------|----------|
| 1 | The stub clock starts at zero each time a chart is loaded | Met | `src/gameplay/gameplay_view.cpp:81` (`stub_frames_ = 0.0;`), before the audio load and before `bind_clock_source()`. Real app, fixture pack, one instance: play 1 and play 2 of `Blaze Anthem` Challenge both log `Failed: life empty at 1.28333s` and `M 6`, in two separate runs |
| 2 | A test loads a chart twice on the stub clock and checks that both plays start at chart time zero | Met | `test_stub_clock_starts_at_zero_on_each_load` (`tests/gameplay_view_test.cpp:58`): one view, two `init` calls, `music_time_seconds() == 0.0` after each. Against the pre-fix `gameplay_view.cpp` (`HEAD`) it aborts at `tests/gameplay_view_test.cpp:66`; with the fix it passes |
| 3 | The "Known defect" gotcha about the stub clock is removed from `.claude/skills/verify/features/gameplay-and-results.md` | Met | The line is gone, and no other tracked file outside `.agents/` mentions the defect. The rest of the recipe was driven in the real app after the fix and still matches (fail, abort, pass and save) |

Each of the three resets is covered by a test: with one reset line removed at a time, the test aborts at line 66 (`stub_frames_`), line 107 (`use_stub_`) and line 130 (`audio_started_`).

## Issues Found

### Critical

None

### High Priority

None

### Medium Priority

None

### Suggestions (Low)

- **L1** `tests/gameplay_view_test.cpp:123` — `test_stub_play_after_audio_play_uses_stub_clock` does not check that its first play ran on the audio clock. **Why:** `GameplayView::init` returns true when the audio cannot be loaded and falls back to the stub clock, so the case then tests "stub after stub" and passes without testing what its name says. Measured: with a path that does not load passed to this case, it passes, and it still passes with the `audio_started_ = false;` reset removed from `init` (the regression that this case exists to catch). Today the case before it asserts that the same WAV plays, so the gap opens only when that case is removed or reordered, or when the audio stops working between the two. **Fix:** after the first `init` in this case, poll until `view.music_time_seconds() > 0.01` with no `update` call and `TEST_CHECK` it, as `test_audio_play_after_stub_play_uses_audio_clock` does at lines 100 to 107 (one shared helper for the poll keeps a single definition).

**Noted, not a finding**: the plan scoped out three things, and this review leaves them there. (1) The stub clock is advanced by the fixed frame step, and the real game reaches it when a song's audio is missing; whether such a song should be playable is an owner question. (2) The class comment in `src/gameplay/gameplay_view.hpp` and the log line call the stub "demo harness only", which is not true in the real game. (3) `GameplayView::shutdown` leaves `std::fixed` set on `std::cout`, so the second `Speed X-mod` line of a session prints `1.000`. None of them is caused by this change. The fix also covers the two mixed sequences (audio after no audio, no audio after audio); the issue comment records that as an owner decision, so it is not scope creep.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (the command CI runs), step 3, host, GCC, Release, `-DBLAZE_WERROR=ON` |
| Warnings | NONE | The two changed sources (`src/gameplay/gameplay_view.cpp`, `tests/gameplay_view_test.cpp`) recompiled to `/dev/null` with the build's own flags (`-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Werror`); no warning. `build/` was not written by this |
| Lint | PASS | `scripts/check.sh` step 1 (`scripts/lint.sh`), host. It searches untracked files too, so the new test file is covered |
| Tests | PASS | `scripts/check.sh` step 4, host, null audio device: 53 passed of 53, none skipped. `gameplay_view_test` passed 20 of 20 more runs. Pre-fix run: the branch's test object linked against `gameplay_view.cpp` of `HEAD` aborts at `tests/gameplay_view_test.cpp:66` (scratch build, `build/` untouched) |
| Sanitizers | PASS | `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan`, host: 55 passed of 55. `gameplay_view_test` ran all three cases with no report and is judged by its exit code. Pre-existing and not caused by this change: `audio_test`, `select_screen_test` and `preview_player_test` pass by the expected-report rule of #144 and stop at their first failed sound-file load; the two `sanitizer_canary_*` tests pass on their own expected report |
| UI verification | PASS | `verify` skill (`.claude/skills/verify/scripts/b4k.sh`), fixture pack (stub clock, `time source stub`), one 1280x720 window (the game has no other viewport). Run 1: Challenge twice, Beginner abort, Beginner pass, in one instance. Plays 1 and 2 both log `Failed: life empty at 1.28333s`, `M 6`; the abort logs a `Score:` line and no `[ResultsScreen]` line; the pass logs `M 4`, `life 0.100 alive`, `CLEARED NEW RECORD`, and `scores.json` holds one record. `02-gameplay-play1.png` and `04-gameplay-play2.png` both show arrows crossing the receptors. Run 2: two shots 0.3 s apart in each of the two plays differ, so the field scrolls in play 2. Captures: `/tmp/claude-1000/-home-lauri-github-blaze4k/ce1e308b-94d0-4d0b-ac0a-6b7e08c7c1b2/scratchpad/verify-138-review/evidence/` and `/tmp/claude-1000/-home-lauri-github-blaze4k/ce1e308b-94d0-4d0b-ac0a-6b7e08c7c1b2/scratchpad/verify-138-review-b/evidence/` (session scratch, not kept in the repo) |

## What's Good

- The reset sits where every other per-play field is reset, before the audio load and before `bind_clock_source()`, so the view starts clean however its owner uses it.
- One cause, three symptoms, and all three are fixed and tested. Each reset line has a test case that fails without it.
- The test asserts values it did not compute with the code's formula (`0.0`, `0.25`, `2.0`, one Miss event), reuses one view per case as `GameplayScreen` does, and runs on the null audio device.
- The audio case does not depend on machine speed for its second check: the clock of a 1 s file cannot pass 1 s.
- The empty audio path for "no audio" keeps the test clear of the known miniaudio report under the sanitizers (#144).

## Recommendation

The change is ready to merge. L1 is a small hardening of the new test and can be fixed with `/fix-findings .agents/reviews/feature-064-stub-clock-reset-between-plays-review.md L1` before the PR, or left.
