# Code Review: feature/054-calibration-wide-delay-window

**Scope**: Branch `feature/054-calibration-wide-delay-window` vs `main`, uncommitted working tree (no commits on the branch yet), for issue #74. 10 files: `docs/AUDIO_LATENCY.md`, `src/screens/calibration_screen.{cpp,hpp}`, `src/screens/setup_art.{cpp,hpp}`, `src/timing/offset_calibration.{cpp,hpp}`, `tests/{calibration_screen,offset_calibration,setup_art}_test.cpp`, plus the untracked plan and report. `.agents/stories/todo-stories.md` is excluded because its changes are unrelated.
**Recommendation**: NEEDS WORK (one Medium: at minimum a docs correction)

## Summary

The change replaces nearest-beat matching with matching to the most recent beat, using an asymmetric slot `[-50 ms, +450 ms)`. It adds a circular-mean unwrap so taps that jitter across the slot edge cannot split the cluster. Results outside `[-50, +420] ms` are flagged `OUT OF RANGE` and Confirm does not save them. The late side is correct: +0.28 s and +0.40 s give -0.28 s and -0.40 s, and 420 to 450 ms is refused. Builds, the warnings gate and all 51 sandboxed tests are clean.

The early side has a gap that neither the docs nor the plan describe. A total delay below about -80 ms (the player taps early) now silently saves a large **negative** offset of the wrong sign, and `main` measured that case correctly. The docs claim that delays below -50 ms are shown as `OUT OF RANGE`.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **Early-side aliasing: a total delay below about -80 ms is silently saved with the wrong sign, and the docs say it is refused.**
   `src/timing/offset_calibration.cpp:92-95` (centre placement), `docs/AUDIO_LATENCY.md:128` (and the summary bullet at `:34-37`).
   After the circular mean, a centre below `-max_early` is moved up by one period into `[0.45, 0.25+0.5)`. The out-of-range arc is therefore only the 30 ms band `(0.42, 0.45)`. On the early side, that band corresponds to true delays from -80 ms to -50 ms. Any cluster earlier than about -80 ms lands at `+0.42 - (|D| - 0.08)` and passes as in range. I probed the real `OffsetCalibration` with 16 jittered taps (sigma 15 ms):

   | True total delay | Saved offset | Correct offset | `main` behaviour |
   |---|---|---|---|
   | -0.05 / -0.06 / -0.07 | OUT OF RANGE | +0.05 / +0.06 / +0.07 | saved correctly |
   | -0.08 | **-0.413** | +0.08 | saved correctly |
   | -0.10 | **-0.393** | +0.10 | saved correctly |
   | -0.20 | **-0.293** | +0.20 | saved correctly |
   | -0.30 | -0.193 | +0.30 | (beyond +/-0.25, mismatched) |

   This is the mirror image of the #74 bug (a silent save with the wrong sign), and it is a regression for players with an early bias between -50 ms and -250 ms. It needs a tap bias of about -100 ms on a wired setup (wired calibrates at about +23 ms), so it is rare. The owner accepted aliasing **above ~450 ms** as a limit. This early-side alias was not part of that decision: the plan (OQ1, `:353`) and the docs (`:128`) both state that "below -50 ms" is refused, and that statement is inaccurate.
   **Recommendation (owner choice):**
   (a) The minimum fix: correct `docs/AUDIO_LATENCY.md:128` and `:34-37`. Say that -50 to -80 ms shows `OUT OF RANGE`, and that an early total delay beyond about -80 ms aliases to about +0.42 s and is saved wrong. Add a test that pins the chosen behaviour (for example, constant -0.10).
   (b) Move part of the 30 ms detection arc to the early side, for example by treating a centre in `[-0.10, -0.05)` as out of range before the `+period` wrap. This costs late range: about 0.40 s, which sits exactly on AC1. It is a one-constant trade-off, the same one the plan's OQ2 raised.

### Suggestions (Low)

1. **The docs example has the wrong direction.** `docs/AUDIO_LATENCY.md:135`: "a tap 480 ms after one click is also 20 ms **after** the next". The next click is at 500 ms, so the tap is 20 ms **before** it, and it aliases to -20 ms (offset +0.02, which matches the probe). Change it to "20 ms before the next", or use 520 ms with "20 ms after the next".
2. **Redundant `result()` recomputation every frame.** `src/screens/calibration_screen.cpp:112`: `CalibrationResult current = calib_.result();` runs the full sort, MAD and sin/cos pass and allocates two vectors on every update, on top of the one at `:144`. At the start of `update`, `result_` is always equal to `calib_.result()`: `enter()` resets both, and `:144` re-syncs them at the end of each update. `CalibrationResult current = result_;` gives the same behaviour without the extra work. This is a minor cost per frame, not a correctness issue.

**Noted, not a finding:** delays above ~450 ms alias to a small value with the 120 BPM click (accepted by the owner, no 60 BPM or extended mode in #74). `docs/AUDIO_LATENCY.md:134-139` documents this correctly ("above about 450 ms ... saved as if it were 0.5 s shorter"), apart from the Low #1 wording. The probe confirms it: +0.46 gives offset +0.047, and +0.48 gives +0.027.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC) | PASS (up to date) |
| Warnings gate: changed sources recompiled with the build's own flags (`-O3 -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`) for `offset_calibration.cpp`, `calibration_screen.cpp`, `setup_art.cpp` and the 3 changed tests | PASS (0 diagnostics) |
| Lint | N/A (no linter configured) |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` | PASS 51/51 |
| Sandbox check | `/dev/snd` and `/run/user/$UID` are empty inside the sandbox. The calibration tests use the synthetic-clock fallback (`[Calibration] audio unavailable; using synthetic clock`), and every assertion still runs. That is a fallback, not a skip |
| Skipped / env-guarded tests | None (`ctest -V` shows no skip markers; the "Skipping" lines are parser log output) |
| Doc `file:line` citations (`AUDIO_LATENCY.md:49-56, 116-130, 144`) | PASS: all point to the cited code in the working tree |
| Ad-hoc model probe (scratchpad, not committed) | Confirms the late side and the Medium finding (table above) |

## What's Good

- The fix lives in tap-to-beat matching and estimation only. The music clock and input aging are unchanged (principle 1).
- `matching_beat_index` keeps the clamp-in-double overflow guard and handles NaN. The test pins the `+0.44 -> beat 0` and `+0.47 -> beat 1` edges.
- The circular-mean unwrap is a sound way to handle slot-edge jitter, and `test_cluster_straddles_slot_edge` (`0.38/0.41` and `-0.02/-0.06`) pins it well.
- `OutOfRange` is derived each update rather than sticky, and `test_out_of_range_recovers` pins it. Confirm cannot save in that state. The hint bar drops `ENTER SAVE` and the notice reuses the existing red slot. The synthetic notice takes priority, which is correct.
- AC1 and AC3 are tested through both the pure model and the real screen path, including config-unchanged checks and render smoke tests at every size.

## Recommendation

Fix or decide Medium #1: correct the docs (and the plan's OQ1 wording if it is kept as a record) and add a test that pins the early-side behaviour, or rebalance the detection arc. Optionally take the two Low items. The owner's hardware checks listed in the implementation comment are still pending. Re-run the wizard on wired and on WH-1000XM4 to confirm about -0.02 s and about -0.22 s.
