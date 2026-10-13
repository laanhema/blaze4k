# Plan: Start the stub clock at zero on every chart load (#138)

## Summary

When a song's audio is missing or cannot be decoded, `GameplayView` runs the chart on a synthetic stub clock. `GameplayScreen` keeps one `GameplayView` for the whole session, and `GameplayView::init` never resets the three fields that say which clock a play uses: `stub_frames_`, `use_stub_` and `audio_started_`. So the second play without audio starts where the first one stopped, and every arrow is already missed.

The fix resets those three fields in `GameplayView::init`, next to the `clock_interpolation_` reset that is already there. A new test, `gameplay_view_test`, loads a chart twice on one view and checks that both plays start at chart time zero. The "Known defect" line leaves the verify skill's gameplay feature file.

The change was built and run in a scratch worktree, and it is saved as a prototype patch. Two things came out of that run that the issue does not say:

1. **The same missing reset breaks two more sequences**, both measured on `main`. A play with audio after a play without it runs on the stub clock while the music plays (the log says `time source stub`). A play without audio after a play with it gets a clock that stands still at zero, so the chart never moves (the log says `time source audio`). The patch fixes both, and the test covers both. Open Question 1 asks whether they belong in this issue.
2. **The issue's second-play number is not what two plays give.** The issue reports `12.8333s` for the second play. Two plays of `Blaze Anthem` Challenge through the real `GameplayScreen` on `main` give `3.28333s`. The issue's session had more plays before it. The rest matches: `M 10`, and 2.0167 s more on each further play.

## User Story

As a player whose pack has a song with missing or broken audio (and as an agent that proves screens with the fixture pack)
I want every play of a chart to start at chart time zero
So that the second play looks and scores like the first, instead of failing in its first frame

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | `gameplay/gameplay_view` (per-play clock-source reset), `tests/gameplay_view_test.cpp` (new), `tests/CMakeLists.txt`, `.claude/skills/verify/features/gameplay-and-results.md` |
| GitHub Issue | #138 (Stub clock is not reset between plays, so a second play without audio starts mid-chart) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/064-stub-clock-reset-between-plays-prototype.patch` |
| Branch (suggested) | `feature/064-stub-clock-reset-between-plays` |
| Plan sequence | `064`. `063` was #136. No branch or merge since then took a number |

---

## Environment Findings

Planned on `main` @ `3c96079`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| Build | `build/` (Release, `-DBLAZE_WERROR=ON`, GCC) | Already configured on this host |
| Baseline | `scripts/check.sh` prints `CHECK: PASS`, 52 of 52 tests | Run at planning time. With the patch: 53 of 53 (one new test, `gameplay_view_test`) |
| Prototype run | Lint passes, the build has no warning in project sources, 53 of 53 tests pass | Scratch worktree at `3c96079`, its own build directory, same flags as `scripts/check.sh` |
| New test, repeated | 10 of 10 runs pass, 0.04 s each | The audio case waits on the null device. It passed its first poll every time |
| New test under the sanitizers | Passes with `-DBLAZE_SANITIZE=ON` (Debug), no report | It never loads a missing file, so it does not reach the known miniaudio report (#144) |
| Audio in tests | The null device, opened by `SoundStream::load` on first use | No sandbox needed. The test opens no real device |
| Forward references to #138 | Only the "Known defect" line in `.claude/skills/verify/features/gameplay-and-results.md:38` | Nothing in `src/`, `tests/`, `docs/`, `scripts/`, `TODO.md` or `.agents/` |
| Other owners of a stub clock | `CalibrationScreen::enter` (`src/screens/calibration_screen.cpp:61-68`) | It already resets its own `stub_frames_` on every enter. No change there |
| Other `GameplayView` owners | `src/main.cpp` (`--gameplay-demo`, one load), `tests/score_keeper_test.cpp`, `tests/life_keeper_test.cpp`, `tests/perf_loop_test.cpp` | Each makes a new view per load, so none of them reuses one. That is why no test caught this |

### Bug reproduction (measured on `main`, headless)

**Through the real `GameplayScreen`** (a scratch program linked against `build/libblaze4k_core.a`; `Blaze Anthem` from the fixture pack, no input, 1/60 s steps, fail on):

| Play | Chart | `main` | With the patch |
|---|---|---|---|
| 1 | Challenge | `Failed: life empty at 1.28333s`, `M 6` | same |
| 2 | Challenge | `Failed: life empty at 3.28333s`, `M 10`, in its first step | `1.28333s`, `M 6` |
| 3, 4, 5 | Challenge | `5.3s`, `7.31667s`, `9.33333s`, each `M 10` | not measured (two loads were) |
| 1 | Beginner | Leaves Gameplay after 368 steps (248 to the clear, 120 of end delay), `M 4`, `life 0.100 alive` | Clears after 248 steps at chart time 4.13333 s, `M 4`, `life 0.100 alive` |
| 2 | Beginner | Leaves Gameplay after 121 steps: all four rows missed in the first step, then only the end delay | Clears after 248 steps again, same score line |

The "with the patch" column was measured on `GameplayView` directly (two loads of each chart on one view), so its step counts have no end delay.

**The three cases of the new test** (each run alone on `main`, then with the patch):

| Case | Sequence on one view | `main` | With the patch |
|---|---|---|---|
| 1 | No audio, 2.0 s of steps, shutdown, no audio again | Second load starts at chart time `2.0` | Starts at `0.0` |
| 2 | No audio, 1.0 s of steps, shutdown, then a 1 s WAV | Log: `time source stub`. Thirty 1 s steps move the clock to `31.0` while the file plays | Log: `time source audio`. The clock reads `0.02` and thirty 1 s steps do not move it |
| 3 | A 1 s WAV, shutdown, then no audio | Log: `time source audio`. Two 0.25 s steps leave the clock at `0.0` | Log: `time source stub`. The clock reads `0.5` |

On `main` the test stops at its first failing check, the second-load `music_time_seconds()` check of case 1 in `test_stub_clock_starts_at_zero_on_each_load`.

### Why cases 2 and 3 fail

`GameplayView::init` (`src/gameplay/gameplay_view.cpp:79-93`) only ever sets `audio_started_` when a file loads, and only ever sets `use_stub_` to true. Neither goes back.

- After a play without audio, `use_stub_` stays true. The next play loads and starts its audio, but `bind_clock_source` (`src/gameplay/gameplay_view.cpp:121-135`) still binds the stub.
- After a play with audio, `audio_started_` stays true. The next play has no audio, the `if (!audio_started_)` branch is skipped, and the clock is bound to a stream that holds no sound.

### Issue-stated numbers and notes, re-checked

| Issue note | Measured |
|---|---|
| "`GameplayScreen` reuses one `GameplayView` (`src/screens/gameplay_screen.hpp:60`)" | Correct. `enter` calls `init` (`src/screens/gameplay_screen.cpp:59`) and `exit` calls `shutdown` (`:134`) |
| "`stub_frames_` is only ever incremented (`src/gameplay/gameplay_view.cpp:188`)" | Correct. The field's only other write is its initializer (`src/gameplay/gameplay_view.hpp:140`) |
| First play: `1.28333s`, `M 6` | Reproduced (77 steps of 1/60 s) |
| Second play: `12.8333s`, `M 10` | `M 10` reproduced. The time is `3.28333s` after exactly one earlier play: 1.28333 s, plus the 2 s end delay (`GameplayScreen::kEndDelaySeconds`) during which the stub keeps running. The issue's session had more plays before it |
| "then `14.85s` and `16.8667s`" | The step matches: each further play adds 2.0167 s (the end delay plus one step) |
| "On Beginner, all four rows are missed before the first frame is drawn" | Reproduced: the second Beginner play resolves in its first step |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Reset in `GameplayView::init`, not in `shutdown` and not in `GameplayScreen` | `init` is where every other per-play field is reset (`exited_`, `judge_`, `score_`, `life_`, `press_time_`). The view then starts clean however its owner uses it. `CalibrationScreen::enter` resets its stub clock the same way |
| D2 | Reset all three clock-source fields, not `stub_frames_` alone | One cause, three symptoms, three adjacent assignments. Fixing one and leaving two would keep a known timing defect on `main`. See Open Question 1 |
| D3 | The test goes in a new file, `tests/gameplay_view_test.cpp` | `GameplayView` has no test file. Its existing checks sit as "boundary" blocks inside the keeper tests, and a view-lifecycle test does not belong in `score_keeper_test`. Open Question 2 |
| D4 | The test passes an empty audio path for "no audio", not the path of a missing file | An empty path skips `SoundStream::load`. A missing file would reach the known miniaudio report under the `asan-ubsan` preset (#144) |
| D5 | No change to the stub fallback itself, to its log wording, or to `CalibrationScreen` | The issue asks for the clock to start at zero. Whether a song with no audio should be playable at all is a different question (see Risks) |

---

## Patterns to Follow

### Per-play state is reset at the top of `init`

```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:47-57
    judge_.reset(&chart_, &constants);
    score_.reset(&chart_, &constants);
    life_.set_fail_enabled(options.fail_enabled);
    life_.reset(&chart_, &constants);
    exited_ = false;
    final_combo_celebrated_ = false;
    judge_anim_.reset();
    was_held_.fill(false);
    press_time_.fill(-1e9);
    explosions_.fill(ColumnExplosion{});
    mine_explosion_start_.fill(-1e9);
```

### The same reset, already done for the calibration stub clock

```cpp
// SOURCE: src/screens/calibration_screen.cpp:61-68
void CalibrationScreen::enter(ScreenContext& /*ctx*/) {
    calib_ = OffsetCalibration(config_);
    result_ = CalibrationResult{};
    saved_ = false;
    out_of_range_logged_ = false;
    synthetic_ = false;
    stub_frames_ = 0.0;
    phase_ = CalibrationPhase::CountIn;
```

### Tests: a `GameplayView` on the stub clock, stepped with `update`

```cpp
// SOURCE: tests/score_keeper_test.cpp:707-714
        blaze4k::GameplayView view;
        blaze4k::GameplayOptions options;
        TEST_CHECK(view.init(chart, k, "", options));
        TEST_CHECK(view.is_ready());

        for (int i = 0; i < 90; ++i) { // ~1.5 s of stub time, past the Miss threshold
            view.update(1.0 / 60.0, held_none());
        }
```

### Tests: real audio on the null device, polled until the clock moves

```cpp
// SOURCE: tests/music_clock_test.cpp:217-225
        bool advanced = false;
        for (int i = 0; i < 200; ++i) {
            if (audio_clock.time_seconds() > 0.01) {
                advanced = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST_CHECK(advanced);
```

The WAV comes from `write_test_wav` in `tests/test_wav_writer.hpp`, written under `std::filesystem::temp_directory_path()` and removed at the end (`tests/music_clock_test.cpp:203-206`, `:230`).

### Test registration

```cmake
# SOURCE: tests/CMakeLists.txt:147-155
add_executable(life_keeper_test
    life_keeper_test.cpp
)

target_link_libraries(life_keeper_test PRIVATE
    blaze4k_core
)

add_test(NAME life_keeper_test COMMAND life_keeper_test)
```

---

## Files to Change

| File | Action | In the patch | Purpose |
|------|--------|--------------|---------|
| `src/gameplay/gameplay_view.cpp` | UPDATE | Yes | `GameplayView::init` resets `stub_frames_`, `use_stub_` and `audio_started_` before it loads the audio |
| `tests/gameplay_view_test.cpp` | CREATE | Yes | Three cases on one reused view: no audio twice, no audio then audio, audio then no audio |
| `tests/CMakeLists.txt` | UPDATE | Yes | Registers `gameplay_view_test` after `gameplay_screen_test` |
| `.claude/skills/verify/features/gameplay-and-results.md` | UPDATE | Yes | Removes the "Known defect" gotcha (issue acceptance criterion 3) |

No other file. `src/gameplay/gameplay_view.hpp`, `GameplayScreen`, `CalibrationScreen`, `MusicClock`, `SoundStream` and the JSON constants stay as they are. `TODO.md` has no line for this issue.

---

## Tasks

The prototype patch carries the whole change. These tasks are what is left: prove it, and check the one doc it touches against the real app.

### Task 1: See the test fail without the fix

- **File**: `src/gameplay/gameplay_view.cpp`
- **Action**: VERIFY (no edit is kept)
- **Implement**: With the patch applied, take the fix out and keep the test: `git apply -R --include='src/gameplay/gameplay_view.cpp' .agents/plans/064-stub-clock-reset-between-plays-prototype.patch`. Build and run `gameplay_view_test`. It must abort in `test_stub_clock_starts_at_zero_on_each_load`, at the `music_time_seconds()` check that follows the second `init`. Then put the fix back: `git apply --include='src/gameplay/gameplay_view.cpp' .agents/plans/064-stub-clock-reset-between-plays-prototype.patch`. Both commands were run on a scratch worktree at planning time.
- **Mirror**: the evidence tables above
- **Validate**: `cmake --build build -j"$(nproc)" && ctest --test-dir build --output-on-failure -R gameplay_view_test` fails without the fix and passes with it

### Task 2: Read the patched code against the decisions

- **File**: `src/gameplay/gameplay_view.cpp`, `tests/gameplay_view_test.cpp`, `tests/CMakeLists.txt`
- **Action**: VERIFY (edit only on a mismatch)
- **Implement**: Check three things. (a) The three resets in `GameplayView::init` sit before the audio load and before `bind_clock_source` is called. (b) Every case in `gameplay_view_test.cpp` uses one `GameplayView` for both loads; a case that built a second view would pass on `main`. (c) "No audio" is the empty path (D4). If the owner answers Open Question 1 with the narrow fix, remove the `use_stub_` and `audio_started_` resets and the two test functions that take a WAV path, with the WAV helper and the includes only they use, and file the two sequences as a new issue.
- **Mirror**: `src/gameplay/gameplay_view.cpp:47-57`, `tests/score_keeper_test.cpp:699-722`
- **Validate**: `ctest --test-dir build --output-on-failure -R "gameplay_view_test|gameplay_screen_test|score_keeper_test|life_keeper_test|perf_loop_test|music_clock_test"`

### Task 3: Check the verify recipe in the real app

- **File**: `.claude/skills/verify/features/gameplay-and-results.md`
- **Action**: VERIFY (edit only on a mismatch)
- **Implement**: Run the End-to-End Verification below. The "Known defect" line is already gone with the patch. Then read the rest of the file's recipe against what the run showed. The headless measurements say that nothing else changes: the first Challenge play still fails at `1.28333s` with `M 6`, and the Beginner pass still ends `M 4`, `life 0.100 alive`, now after about 6 s (4.1 s of chart, then the 2 s end delay), well inside the recipe's `wait-screen RUN Results 20`. If the real app shows a different number, correct that line of the recipe.
- **Mirror**: the wording of the other gotchas in that file
- **Validate**: `scripts/lint.sh` (it lints `*.md` for line citations and test counts), and `grep -c "Known defect" .claude/skills/verify/features/gameplay-and-results.md` prints `0`

### Task 4: Full checks

- **File**: the four files of "Files to Change"
- **Action**: VERIFY
- **Implement**: Run the Validation block.
- **Validate**: `scripts/check.sh` prints `CHECK: PASS`

---

## Validation

```bash
# Lint, configure, build with warnings as errors, every test
scripts/check.sh

# Lint alone
scripts/lint.sh

# Build alone
cmake --build build -j"$(nproc)"

# Reproduction: red on the unfixed code (Task 1), green after the fix
ctest --test-dir build --output-on-failure -R gameplay_view_test

# Neighbours that load a GameplayView or the audio clock
ctest --test-dir build --output-on-failure -R "gameplay_screen_test|score_keeper_test|life_keeper_test|perf_loop_test|music_clock_test"

# Sanitizers (the new test loads real audio on the null device)
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan

# Scope guard: only the four files of "Files to Change"
git status -sb
```

## End-to-End Verification

1. **Automated (headless, required):** `gameplay_view_test` loads a chart twice on one `GameplayView` and checks chart time `0.0` after each load, `0.25` after one step with no judgment yet, and `2.0` with exactly one Miss after eight steps. Its other two cases check that a load picks the audio clock or the stub clock from its own audio, whatever the load before it used.
2. **Real app (required, issue reproduction steps):** use the `verify` skill with the fixture pack, which has placeholder music, so every song runs on the stub clock. Rebuild first (`cmake --build build -j"$(nproc)"`).
   - `B=.claude/skills/verify/scripts/b4k.sh`, then `$B launch --run /tmp/blaze4k-verify/138-stub-clock`.
   - Open select and highlight `Blaze Anthem` (`CHALLENGE 10`) as `features/song-select.md` describes.
   - Play 1: `$B keys RUN Return`, `$B wait-screen RUN Gameplay`, `$B shot RUN 01-first-play`, press nothing, `$B wait-screen RUN Results 15`. Then `$B keys RUN Return` and `$B wait-screen RUN Select` (one `Return` per `keys` call on Results).
   - Play 2: wait about 1.5 s, then the same again with `$B shot RUN 02-second-play` taken right after `wait-screen RUN Gameplay` returns.
   - `$B stop RUN`, then read `RUN/evidence/game.log`.
   - **Expected:** two `[GameplayView] Failed: life empty at 1.28333s` lines and two `Score:` lines with `M 6`. Both screenshots show arrows on the field. On `main` the second play logs `3.28333s` and `M 10`, and its screenshot has no arrows left to hit.
3. **Mixed sequences in the real app (optional):** they need a pack that has one song with working audio and one without, which the repo does not ship. The automated cases 2 and 3 are the proof. Not required.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The audio case of the new test waits on the null device's real-time pacing | It polls for up to 2 s, like `music_clock_test` does. It passed its first poll in 10 of 10 runs and under the sanitizers. Its second check does not depend on time: the clock of a 1 s file cannot pass 1 s, whatever the machine load | In scope (measured) |
| A reset placed after `bind_clock_source`, or a test that builds a new view per load, would look right and prove nothing | Task 2 checks both. Task 1 shows the test red without the fix | In scope |
| `init` returns early on a chart with no notes, before the resets | `ready_` stays false, so `update` and `render` do nothing, and the next successful `init` resets everything | In scope (read, no change needed) |
| The stub clock is advanced by the fixed frame step, and the real game reaches it when a song's audio is missing. Principle 1 says gameplay never runs on frame delta | Unchanged by this plan. The issue accepts the fallback and asks only that it start at zero. Whether such a song should be playable is an owner question for a new issue | Out of scope (flag only) |
| The class comment in `src/gameplay/gameplay_view.hpp` and the log line call the stub "demo harness only", which the issue shows is not true in the real game | Wording only. Left for the issue above | Out of scope (flag only) |
| The second `Speed X-mod` log line of a session prints `1.000` instead of `1` | `GameplayView::shutdown` leaves `std::fixed` set on `std::cout`. Cosmetic, seen in the probe output, not caused by this change | Out of scope (flag only) |
| The suite grows by one test, and a doc might state the count | No doc states it (lint rule `doc-test-count`) | In scope (no action) |

---

## Open Questions

1. **Should this fix also cover the two mixed sequences?** In plain words: the issue describes one symptom (no audio, then no audio again). The same three lines of missing reset also break "no audio, then audio" (the game runs on the frame-step clock while the music plays, so arrows drift from the music) and "audio, then no audio" (the chart freezes at time zero and the player has to press Back). Both were measured on `main`. *Recommendation and default in the patch:* fix all three now. It is two more one-line resets beside the first, each with a test that fails on `main`, and the "no audio, then audio" case is a real timing defect for any pack with one broken song. *The alternative:* reset only `stub_frames_`, and file the other two as a new issue. Task 2 says what to remove for that. Not blocking: the default satisfies all three acceptance criteria of #138 either way.
2. **New test file or a block in an existing test?** *Default:* the new `tests/gameplay_view_test.cpp` (D3). The alternative is a block in `tests/gameplay_screen_test.cpp`, which avoids a new target but puts view tests in the screen's file. Not blocking.

---

## Acceptance Criteria

- [ ] The stub clock starts at zero each time a chart is loaded (issue criterion 1; `GameplayView::init`, case 1 of `gameplay_view_test`)
- [ ] A test loads a chart twice on the stub clock and checks that both plays start at chart time zero (issue criterion 2; `test_stub_clock_starts_at_zero_on_each_load`)
- [ ] The "Known defect" gotcha about the stub clock is gone from `.claude/skills/verify/features/gameplay-and-results.md` (issue criterion 3)
- [ ] A load picks its clock source from its own audio, whatever the load before it used (cases 2 and 3; subject to Open Question 1)
- [ ] `gameplay_view_test` fails without the fix and passes with it (Task 1)
- [ ] In the real app, two plays of `Blaze Anthem` Challenge on the fixture pack both log `Failed: life empty at 1.28333s` and `M 6`
- [ ] `scripts/check.sh` prints `CHECK: PASS`, and the `asan-ubsan` preset passes
- [ ] The diff is limited to the four files in "Files to Change"
