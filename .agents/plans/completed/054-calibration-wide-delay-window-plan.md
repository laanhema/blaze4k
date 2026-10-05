# Plan: Calibration wizard measures delays up to 420 ms and reports out-of-range delays (#74)

## Summary

The calibration wizard pairs every tap with the **nearest** beat (`src/screens/calibration_screen.cpp:113`), and `OffsetCalibration::add_sample` throws away any tap more than 0.25 s from its beat (`src/timing/offset_calibration.hpp:25`). Beats are 0.5 s apart (120 BPM). So a total delay above 250 ms gets paired with the *next* beat, and the wizard quietly saves a **positive** offset (`+0.5 − D`). The owner's Bluetooth headphones measure about −0.222 s, which is only 28 ms from that limit.

This plan uses the issue's preferred **Option 1**. The wizard stays at 120 BPM, and the music clock stays the only timing source (AGENTS.md principle 1). Only the step that pairs taps with beats and turns the deltas into an offset changes:

1. **Asymmetric matching (Option 1).** Latency only ever makes taps late. Each tap is paired with the **most recent beat no more than `max_early_seconds` (0.05 s) after it**: `index = floor((hit − lead_in + max_early) / period)`, clamped. Every tap's delta then falls in `[−0.05, +0.45)`, not `[−0.25, +0.25)`.
2. **Wrap-safe estimate.** Splitting the beat into fixed slots on its own only moves the bug from 250 ms to 450 ms. When a player's taps scatter across the slot edge, part of the cluster wraps to the next beat. The MAD filter then gives the "mix of both, unpredictable" result the issue describes. The prototype showed that a split 0.43/0.46 cluster would be saved silently as about −0.195 s. To prevent this, `OffsetCalibration::result()` first finds the cluster's **circular centre** (the circular mean of the tap phases), places it in the supported slot, and moves each delta by a whole number of periods to sit next to that centre. Only then does it run the existing median, MAD, mean and spread code. A delta that already sits next to the centre is moved by 0 periods, so it stays exactly the same (bit-exact). That is why the existing tests keep passing.
3. **Range check.** The supported total delay is **[−50 ms, +420 ms]**. If the final inlier mean falls outside it, `CalibrationResult::out_of_range` is set and `ready` stays false. The screen then shows the phase word `OUT OF RANGE`, a red notice `DELAY OUT OF RANGE - OFFSET WILL NOT BE SAVED`, `---` on the OFFSET plate and the no-save hint bar. It logs one readable `[Calibration]` line, and Confirm refuses to save.

A periodic click track cannot tell a delay `D` apart from `D + 0.5 s`. So delays above about 450 ms still alias to a small value and cannot be detected. This is documented, and Open Question 1 asks the owner to decide on it.

## User Story

As a player on Bluetooth (or another high-latency) audio output
I want the calibration wizard to measure my real total delay, or tell me plainly when it can't
So that it never silently saves an offset with the wrong sign that makes every song unplayable

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW–MEDIUM (pure math in 1 module, 1 screen, 1 art constant, tests, docs; no audio/clock changes) |
| Systems Affected | `src/timing/offset_calibration.{hpp,cpp}`, `src/screens/calibration_screen.{hpp,cpp}`, `src/screens/setup_art.{hpp,cpp}`, `tests/offset_calibration_test.cpp`, `tests/calibration_screen_test.cpp`, `tests/setup_art_test.cpp`, `docs/AUDIO_LATENCY.md` |
| GitHub Issue | #74 (Calibration wizard saves a wrong positive offset when total delay exceeds 250 ms) |
| Related | #58 (latency spike that found it), #70 (per-device offsets, not implemented), #81 (aging pair, merged — unchanged here) |
| Branch (suggested) | `feature/054-calibration-wide-delay-window` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` configured (Release); `cmake --build build -j$(nproc)` is clean on `main` @ `8d73afe` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | `main` @ `8d73afe`, sandboxed ctest (Validation), 2.8 s. Target after this plan: **51/51** (no new executables; new cases go into 3 existing tests) |
| Sandbox | `bwrap … --tmpfs /dev/snd …` | No audio device inside; every touched test is headless (fake stream / injected source) |
| Algorithm prototype | scratchpad `proto.cpp` (not committed) | Pinned semantics below, run on synthetic deltas (8 taps each). Results are in the table under "Pinned Semantics" |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #74

- `docs/AUDIO_LATENCY.md:34-36` (summary bullet) and `:112-120` ("Wizard headroom", which ends "Not fixed in this spike."). Rewrite both (Task 6).
- `docs/AUDIO_LATENCY.md:51-52` cite `calibration_screen.cpp:63, :98-121, :99-100` and `offset_calibration.cpp:62-120`. These line numbers move, so refresh them (Task 6).
- `.agents/plans/completed/049-remap-calibration-cabinet-plan.md:64, 397` and `.agents/stories/todo-stories.md:894` only say "#74 is independent". No action needed.
- `.agents/plans/completed/020-global-offset-calibration-wizard-plan.md:166-168`: `max_abs_delta=0.25 s` (OQ1) and `bpm=120` (OQ3) were open-question defaults. This plan replaces OQ1's wild cap and keeps OQ3.

---

## Value Provenance

| Value / behavior | Source | Notes |
|---|---|---|
| 120 BPM (`beat_period_seconds = 0.5`), lead-in 2 s, 64 beats | `src/timing/offset_calibration.hpp:20-22`; plan 020 OQ3 | **Kept** (issue: Option 1 keeps the 120 BPM feel) |
| `max_abs_delta_seconds = 0.25` | `src/timing/offset_calibration.hpp:25`; plan 020 OQ1 | **Removed**, replaced by the two values below |
| `max_early_seconds = 0.05` | Blaze 4k design choice (issue Technical Notes: "OpenITG has no equivalent wizard", PRD §7.5 `.agents/PRDs/PRD.md:208-209` does not fix it) | The AC asks that "early taps of a few tens of ms still count". This limits the **cluster mean**, not single taps: a single tap up to about 0.25 s from the centre still lines up correctly |
| `max_late_seconds = 0.42` | Design choice | The AC needs ≥ 400 ms. 0.42 keeps 20 ms of float/jitter margin above 0.40 and leaves a 30 ms detection band `(0.42, 0.45)` below the slot edge `period − max_early = 0.45`. Issue example: "−0.1 s / +0.45 s". With a 0.5 s period, −0.1/+0.45 overlap, so a tap could match two beats. 0.05 + 0.42 < 0.5 keeps the rule unambiguous |
| Offset sign `offset = −mean(hit − beat)` | `src/timing/offset_calibration.hpp:16-18` (plan 020 OQ4) | Unchanged |
| MAD multiplier 3.0, floor 0.05 s | `src/timing/offset_calibration.hpp:26-27` (plan 020 OQ2) | Unchanged; applied to the unwrapped deltas |
| Hit time = aged music-clock time | `src/screens/calibration_screen.cpp:99-112`, `src/gameplay/judgment_input.hpp:12-27` | Unchanged; the music clock stays the only timing source |

---

## Pinned Semantics (the contract the tests pin)

### `CalibrationConfig` (`src/timing/offset_calibration.hpp`)

```cpp
double lead_in_seconds = 2.0;
double beat_period_seconds = 0.5;   // 120 BPM (OQ3, kept by #74)
int max_beats = 64;
int min_samples = 8;                // OQ1
int max_samples = 32;               // OQ1
double max_early_seconds = 0.05;    // #74: supported early limit of the measured delay
double max_late_seconds = 0.42;     // #74: supported late limit (total delay)
double mad_multiplier = 3.0;        // OQ2
double mad_floor_seconds = 0.05;    // OQ2

[[nodiscard]] double beat_time(int index) const;               // unchanged
[[nodiscard]] int matching_beat_index(double music_seconds) const; // replaces nearest_beat_index
```

- `matching_beat_index(t)`: if `beat_period_seconds <= 0 || max_beats <= 0` → 0. `raw = floor((t − lead_in + max_early) / period)`. Non-finite → 0. Clamp **in double** to `[0, max_beats − 1]` before the cast, keeping the existing Windows `long` overflow comment (`offset_calibration.cpp:34-37`). Meaning: the most recent beat `b` with `t ≥ b − max_early`. Delta `t − b ∈ [−max_early, period − max_early)` everywhere except the clamped ends.
- `nearest_beat_index` is **removed**. Its only caller is `calibration_screen.cpp:113`, and `offset_calibration_test.cpp:30-34` is its only test, which is rewritten.
- Header doc comment (`:9-18`): replace the `nearest_beat_index` line with the matching formula and add one line on the wrap-safe estimate.
- Invariant (documented, pinned by a test on the defaults): `0 < max_early`, `0 < max_late`, `max_early + max_late < beat_period`.

### `OffsetCalibration::add_sample(beat, hit)`

- `delta = hit − beat`. Reject it and increment `rejected_wild_` when `!isfinite(delta)`, `delta < −max_early`, or `delta ≥ period − max_early`. This only happens for taps clamped to beat 0 or the last beat, or for direct API callers. With in-range matching it never fires.
- The rest is unchanged: it returns false at the `max_samples` cap without counting it as wild.
- Taps between 0.25 s and 0.45 s are **no longer** rejected one by one. A real mistap is rejected later by MAD, around the cluster centre.

### `OffsetCalibration::result()` (pure, recomputed)

Steps, with `p = beat_period_seconds`, `E = max_early_seconds`, `Lmax = max_late_seconds`:

1. Empty → non-ready, zero fields (as today).
2. `d_i = hit_i − beat_i`.
3. **Circular centre.** `S = Σ sin(2π d_i / p)`, `C = Σ cos(2π d_i / p)`, `c = atan2(S, C) · p / (2π)`, so `c ∈ [−p/2, p/2]`. If `c < −E`, then `c += p`. That puts `c` in `[−E, p − E)`. `atan2(0, 0) = 0`, so no NaN. Use `std::numbers::pi` (`<numbers>`).
4. **Unwrap.** `u_i = d_i − p · std::round((d_i − c) / p)`. When the shift is 0, `u_i == d_i` bit-exact.
5. Run the existing median → MAD → threshold → inliers → mean → population stddev on `u_i`. The `rejected_outlier` count and the "inliers empty → non-ready" guard work as today.
6. `out.out_of_range = (mean > Lmax) || (mean < −E)`.
7. `out.ready = has_min_samples && !out.out_of_range`. `mean_delta_seconds`, `offset_seconds = −mean` and `spread_seconds` are filled in even when out of range, for the log line.

`CalibrationResult` gains `bool out_of_range = false;` (doc: "measured delay outside [−max_early, +max_late]; never savable").
`OffsetCalibration::ready()` keeps its meaning, **enough accepted samples** (`sample_count() >= min_samples`). Its doc comment changes to "sample gate only; a savable result is `result().ready`". `test_readiness_and_cap` stays unchanged.

### Prototype results (scratchpad, 8 taps each, defaults above)

| True delay pattern | Centre | Mean | Saved offset | Verdict |
|---|---|---|---|---|
| +0.02 | +0.020 | +0.020 | −0.020 | ok (AC2) |
| −0.03 (early player) | −0.030 | −0.030 | +0.030 | ok (AC2) |
| +0.28 | +0.280 | +0.280 | −0.280 | ok (AC1) |
| +0.40 | +0.400 | +0.400 | −0.400 | ok (AC1) |
| 0.222 ± 30 ms jitter | +0.220 | +0.220 | −0.220 | ok (owner BT) |
| 0.40 ± 40 ms jitter | +0.401 | +0.401 | −0.401 | ok |
| alternating 0.38 / 0.41 | +0.395 | +0.395 | −0.395 | ok |
| alternating −0.02 / −0.06 (−0.06 matched to the previous beat as +0.44) | −0.040 | −0.040 | +0.040 | ok (wrap fixed) |
| 8 × +0.44 then 24 × +0.03 (recovery) | +0.0095 | +0.030 | −0.030 | ok (24/32 inliers) |
| 8 × +0.02 + one mistap +0.30 | +0.016 | +0.020 | −0.020 | ok (MAD rejects 1) |
| +0.44 | +0.440 | +0.440 | — | **OUT OF RANGE** (AC3) |
| alternating 0.43 / 0.46 | +0.445 | +0.445 | — | **OUT OF RANGE**. Without the circular centre this would be saved as −0.195 |
| −0.07 | +0.430 | +0.430 | — | **OUT OF RANGE** |
| +0.48 | −0.020 | −0.020 | +0.020 | **aliased** (undetectable; Open Question 1) |

### `CalibrationScreen` (`src/screens/calibration_screen.{hpp,cpp}`)

- `enum class CalibrationPhase : int { CountIn = 0, Sampling, Ready, OutOfRange };`. The value is appended, so the existing values do not change.
- Tap path (`:110-115`): `config_.matching_beat_index(hit)` replaces `nearest_beat_index`. Nothing else changes, and the aging pair stays exactly as it is.
- Confirm (`:117-129`): take one `const CalibrationResult current = calib_.result();` before the event loop, and refresh it after each accepted `add_sample`. Use it for the gate and the saved value. Gate on `current.ready`, not `calib_.ready()`. When `calib_.ready() && current.out_of_range`, log `[Calibration] delay out of range; offset not saved` and do nothing else. Keep the synthetic and null-config branches as they are.
- Phase after the loop (`:132-135`). The phase is **derived** once sampling has started: `CountIn` stays until lead-in, as today. After that, `phase_ = result_.ready ? Ready : (calib_.ready() && result_.out_of_range) ? OutOfRange : Sampling`.
- One readable log line per entry the first time `OutOfRange` is reached (new member `bool out_of_range_logged_`, reset in `enter()`). It reads `[Calibration] measured delay <+NNN> ms is outside the supported range (-50..+420 ms); offset not saved`, with the numbers formatted from the config and `mean_delta_seconds` in ms. Use the `std::cerr` pattern from `:119`.
- Render (`:148-172`): phase word `OUT OF RANGE` for the new phase. `offset_ready` stays `result_.ready && !synthetic_`, so out-of-range shows `---` and the no-save hint. Samples text: `calibration_samples_text(sample_count(), min_samples, calib_.ready())`. This uses the sample gate, so the out-of-range plate reads `12`, not `12 / 8`. Pass `out_of_range = (phase_ == OutOfRange)` in `CalibrationView`.
- Out of range is not sticky. If later taps bring the mean back into range, the phase returns to `Ready`. There is no auto-reset: Back cancels, and re-entering resets (`enter()`, `:56-63`). See Open Question 3.

### `setup_art` (`src/screens/setup_art.{hpp,cpp}`)

- New constant next to `kNoAudioNotice` (`setup_art.hpp:153`): `inline constexpr std::string_view kOutOfRangeNotice = "DELAY OUT OF RANGE - OFFSET WILL NOT BE SAVED";`
- `CalibrationView` gains `bool out_of_range = false;`. `draw_calibration` (`setup_art.cpp:312-316`) draws `kNoAudioNotice` when `synthetic`. Otherwise it draws `kOutOfRangeNotice` when `out_of_range`, in the same slot, style and band. Synthetic takes priority because synthetic can never save anyway.

---

## Patterns to Follow

### Naming / module purity

```cpp
// SOURCE: src/timing/offset_calibration.hpp:9-12
// Analytic steady-metronome schedule + robust offset estimation for the C5
// calibration wizard (PRD section 7.5). Pure: standard-library <cstddef>/<vector>
// only, no SDL/GL/audio/clock/wall-clock. Testable headless (AGENTS.md principle 1).
```

Config fields are `snake_case_seconds` with a trailing `// OQn` / `// #74` provenance comment (`offset_calibration.hpp:20-27`). The `.cpp` may add `<numbers>`. The header stays `<cstddef>/<vector>`.

### Overflow-safe index (mirror exactly)

```cpp
// SOURCE: src/timing/offset_calibration.cpp:27-39
const double raw = (music_seconds - lead_in_seconds) / beat_period_seconds;
if (!std::isfinite(raw)) { return 0; }
// Clamp in double before rounding: std::lround returns `long`, which is
// 32-bit on Windows, ...
const double clamped = std::clamp(raw, 0.0, static_cast<double>(max_beats - 1));
return static_cast<int>(std::lround(clamped));
```

For the new function, use `std::floor(... + max_early_seconds ...)` and then `static_cast<int>(clamped)`. After the clamp, `clamped` is already integral.

### Error handling (log, never crash, never save a bad value)

```cpp
// SOURCE: src/screens/calibration_screen.cpp:117-128
if (synthetic_) {
    std::cerr << "[Calibration] audio unavailable; offset not saved\n";
} else if (ctx.config == nullptr) {
    std::cerr << "[Calibration] no config available; offset not saved\n";
} else { ... }
```

### Tests (plain executable, `TEST_CHECK` abort macro, section prints)

```cpp
// SOURCE: tests/offset_calibration_test.cpp:7-14, 50-61
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << "Assertion failed at " ... ; std::abort(); } } while (0)
void test_late_bias_sign() {
    CalibrationConfig config; OffsetCalibration calib(config);
    for (int i = 0; i < config.min_samples; ++i) { const double beat = config.beat_time(i); TEST_CHECK(calib.add_sample(beat, beat + 0.030)); }
    ...
    std::cout << "  - late taps -> negative offset ok.\n";
}
```

Screen tests use the `Fixture` and `collect_eight_late_taps(fx, bias)` (`tests/calibration_screen_test.cpp:111-146`). Each tap is aged 5 ms, so the hit lands exactly on `beat + bias`. Render coverage goes through `render_all` in `tests/setup_art_test.cpp:572-610`, and the width budget for the notice is checked at `:460-461`.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/offset_calibration.hpp` | UPDATE | Config fields (`max_early_seconds`, `max_late_seconds` replace `max_abs_delta_seconds`), `matching_beat_index`, `CalibrationResult::out_of_range`, doc comments |
| `src/timing/offset_calibration.cpp` | UPDATE | `matching_beat_index`, new `add_sample` acceptance, circular centre + unwrap + range check in `result()` |
| `src/screens/calibration_screen.hpp` | UPDATE | `CalibrationPhase::OutOfRange`, `out_of_range_logged_` |
| `src/screens/calibration_screen.cpp` | UPDATE | Matching call, Confirm gate on the result, derived phase, log line, render word/view |
| `src/screens/setup_art.hpp` / `.cpp` | UPDATE | `kOutOfRangeNotice`, `CalibrationView::out_of_range`, notice draw |
| `tests/offset_calibration_test.cpp` | UPDATE | Rewrite `test_schedule`'s index lines, add new cases (Task 2) |
| `tests/calibration_screen_test.cpp` | UPDATE | Large-delay save + out-of-range refusal through the screen (Task 4) |
| `tests/setup_art_test.cpp` | UPDATE | Notice and phase-word width, out-of-range render pass (Task 5) |
| `docs/AUDIO_LATENCY.md` | UPDATE | Summary bullet, citation rows, "Wizard headroom" rewrite (Task 6) |

No CMake changes, because there are no new source or test files.

---

## Tasks

Execute in order. Build after every task with `cmake --build build -j$(nproc)`.

### Task 1: Pure model (matching, acceptance, wrap-safe result, range flag)

- **File**: `src/timing/offset_calibration.hpp`, `src/timing/offset_calibration.cpp`
- **Action**: UPDATE
- **Implement**: Everything under "Pinned Semantics → CalibrationConfig / add_sample / result()". Remove `max_abs_delta_seconds` and `nearest_beat_index`, and update the header doc block. Do not change the median/MAD/mean/stddev code. Only feed it `u_i` in place of `d_i`.
- **Mirror**: `src/timing/offset_calibration.cpp:27-39` (index clamp), `:62-120` (result pipeline)
- **Validate**: The build fails only at `calibration_screen.cpp:113` (`nearest_beat_index`) and `offset_calibration_test.cpp:30-34`. Both are fixed in Tasks 2 and 3. `grep -rn "max_abs_delta_seconds\|nearest_beat_index" src tests` → only those sites.

### Task 2: Model tests

- **File**: `tests/offset_calibration_test.cpp`
- **Action**: UPDATE
- **Implement**: Keep every existing test function **unchanged except `test_schedule`'s `nearest_beat_index` lines**, which become `matching_beat_index`: `lead_in → 0`, `lead_in − 0.04 → 0`, `lead_in + 0.3 → 0` (was 1, which is the fix), `lead_in + 0.44 → 0`, `lead_in + 0.47 → 1`, `−100 → 0`, `1e9 → max_beats − 1`, `NaN → 0`. Keep test inputs away from exact slot edges, where float rounding decides. Add a helper `feed(calib, config, i, delta)`. It computes `hit = beat_time(i + 1) + delta` and `beat = beat_time(config.matching_beat_index(hit))`, then calls `add_sample(beat, hit)`, exactly as the screen does. New cases:
  1. `test_config_invariants`: the defaults satisfy `max_early > 0`, `max_late > 0`, `max_early + max_late < beat_period`.
  2. `test_large_late_delays` (AC1): for `D ∈ {0.28, 0.40}`, 8 taps → `ready`, `!out_of_range`, `offset ≈ −D` (1e-9), `rejected_wild == 0`.
  3. `test_low_latency_unchanged` (AC2): `D = 0.02` → `−0.02`. `D = −0.03` → `+0.03` (early taps still count).
  4. `test_cluster_straddles_slot_edge`: alternating `0.38/0.41` → mean `0.395`, in range. Alternating `−0.02/−0.06` → `offset ≈ +0.04`, in range. The `−0.06` taps are matched to the previous beat. Assert `rejected_outlier == 0`.
  5. `test_out_of_range` (AC3): constant `0.44`, alternating `0.43/0.46`, and constant `−0.07`. Each gives `out_of_range`, `!ready`, `accepted == 8`, and finite fields.
  6. `test_mistap_far_from_cluster`: 8 × `0.02` plus one `0.30` → mean `0.02` (1e-9), `rejected_outlier == 1`, `rejected_wild == 0`. The 0.30 tap is now accepted per tap and then pruned.
  7. `test_unwrap_is_identity_in_range`: 9 deltas spread over `[−0.04, 0.40]`, fed via `add_sample(beat, beat + d)`. The result equals a hand-computed mean (1e-12), which shows the unwrap step did not change them.
  Call them all from `main()`.
- **Mirror**: `tests/offset_calibration_test.cpp:50-61`, `:91-106`
- **Validate**: `bwrap … ctest --test-dir build -R offset_calibration_test --output-on-failure`

### Task 3: Screen wiring

- **File**: `src/screens/calibration_screen.hpp`, `src/screens/calibration_screen.cpp`
- **Action**: UPDATE
- **Implement**: Everything under "Pinned Semantics → CalibrationScreen". Keep the clock, aging and synthetic paths byte-for-byte the same, apart from the lines named there.
- **Mirror**: `src/screens/calibration_screen.cpp:105-135`, `:148-172`
- **Validate**: build. `calibration_screen_test` and `setup_art_test` still pass unchanged (all their taps are 0–30 ms late or early).

### Task 4: Screen tests

- **File**: `tests/calibration_screen_test.cpp`
- **Action**: UPDATE
- **Implement**: Add these cases and call them from `main()`:
  1. `test_large_delay_saves_negative_offset`: `collect_eight_late_taps(fx, 0.40)` → `result().offset_seconds ≈ −0.40` (1e-9), phase `Ready`. Confirm saves `−0.40` and transitions to Select. Repeat with `0.28`, in a fresh fixture.
  2. `test_out_of_range_refuses_to_save`: `fx.start(0.123)`, then `collect_eight_late_taps(fx, 0.44)` → phase `OutOfRange`, `result().out_of_range`, `!result().ready`, `sample_count() == 8`. Confirm → `!saved()`, still on Calibration, config still `0.123`. Render headless once (`GlQuadRenderer renderer; fx.manager.render(renderer, 1280, 720);`).
  3. `test_out_of_range_recovers`: from case 2's state, feed 24 more taps at `+0.03` on later beats (`max_samples` = 32). The prototype gives centre +0.0095. The 8 × 0.44 taps unwrap to −0.06 and MAD prunes them, which leaves 24 inliers and a mean of +0.030. Assert phase `Ready`, `offset ≈ −0.03` (1e-9) and `rejected_outlier == 8`. This pins "not sticky".
- **Mirror**: `tests/calibration_screen_test.cpp:167-181`, `:221-232`
- **Validate**: `bwrap … ctest --test-dir build -R calibration_screen_test --output-on-failure`

### Task 5: Art constant, notice draw, art tests

- **File**: `src/screens/setup_art.hpp`, `src/screens/setup_art.cpp`, `tests/setup_art_test.cpp`
- **Action**: UPDATE
- **Implement**: Everything under "Pinned Semantics → setup_art". In `setup_art_test.cpp`:
  - Next to `:461`, add `measure(setup::kOutOfRangeNotice, setup::kNoticeStyle) < 1280 − 80` and `measure("OUT OF RANGE", setup::kPhaseStyle) < 1280 − 80`.
  - In the calibration render block (`:572-610`), add a third scenario that feeds 8 taps at `beat_time(i + 1) + 0.44`. It asserts `phase() == CalibrationPhase::OutOfRange` and `render_all`s with real and null services.
- **Mirror**: `tests/setup_art_test.cpp:460-465`, `:572-610`
- **Validate**: `bwrap … ctest --test-dir build -R setup_art_test --output-on-failure`

### Task 6: Documentation (AC4)

- **File**: `docs/AUDIO_LATENCY.md`
- **Action**: UPDATE
- **Implement**:
  - `:34-36` summary bullet: the limit is now +420 ms (fixed in #74). The WH-1000XM4 at about 222 ms has about 200 ms of headroom.
  - `:51` and `:52` table rows: refresh the `calibration_screen.cpp` and `offset_calibration.cpp/.hpp` line citations to the new code.
  - Rewrite "Wizard headroom" (`:112-120`) in plain language: taps are matched to the most recent beat (up to 50 ms early allowed). The estimate is centred on the tap cluster, so jitter near a slot edge cannot flip the sign. The supported total delay is −50 ms to +420 ms. Results from 420 ms to about 450 ms (or below −50 ms) are shown as `OUT OF RANGE` and not saved. Above about 450 ms a steady 120 BPM click track cannot tell `D` from `D − 0.5 s`, so such delays alias and are saved as a small value. If this happens, use wired output or a lower-latency codec/profile. Note Open Question 1's outcome if the owner chose something else.
- **Validate**: `grep -n "250 ms\|Not fixed in this spike" docs/AUDIO_LATENCY.md` → no stale hits about the wizard.

### Task 7: Full validation

- Run the whole Validation block. Expect **51/51** passing and no new warnings.

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j$(nproc)

# Lint: no linter configured. Gate on zero new warnings in touched TUs:
touch src/timing/offset_calibration.cpp src/screens/calibration_screen.cpp src/screens/setup_art.cpp
cmake --build build -j$(nproc) 2>&1 | grep -i "warning" | grep -E "offset_calibration|calibration_screen|setup_art" || echo "no new warnings"

# Tests (run sandboxed, as in prior plans: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **51/51 pass** (baseline 51/51 on `8d73afe`). `metronome_sync_test` stays green (it is untouched).

Static checks:

```bash
grep -rn "nearest_beat_index\|max_abs_delta_seconds" src tests docs   # expect none
grep -n "#include" src/timing/offset_calibration.hpp                 # only <cstddef>/<vector>
grep -rn "SDL\|chrono" src/timing/offset_calibration.*               # expect none (pure)
git diff --cached --name-only | grep -c todo-stories                 # expect 0
git status --short .agents/stories/todo-stories.md                   # still " M" (untouched)
```

## End-to-End Verification

1. **Automated (agent, sandboxed):** the three updated tests pin AC1 (`+0.28`/`+0.40` → `−0.28`/`−0.40`), AC2 (`+0.02` → `−0.02`, early `−0.03` counts, all old cases unchanged) and AC3 (out-of-range → `OUT OF RANGE`, not saved, config unchanged). They run through both the pure model and the real screen path (`music_time_for_event` aging + `matching_beat_index`).
2. **Headless app smoke (agent, sandboxed, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --songs ./songs --data-dir <scratch>/data`. It ends with `Blaze 4k shut down cleanly.`, with no crash. This only checks the binary links and boots: the wizard itself needs taps.
3. **Owner verification (real hardware; the agent must not play audible audio):**
   - **a. Wired / speaker:** run Options → Calibrate. Tap normally. The result stays about −0.02 s, as before.
   - **b. WH-1000XM4 (A2DP):** calibrate. The result is still about −0.22 s, with no sign flip. Earlier runs could already land here; the difference is that there is now about 200 ms of margin.
   - **c. Out-of-range demo:** deliberately tap about half a beat late, between clicks. After 8 taps the screen shows `OUT OF RANGE` and the red notice, ENTER does nothing, ESC cancels, and the old offset is kept. One `[Calibration] measured delay … outside the supported range` line appears in the log.
   - **d. Optional, if available:** the HFP profile (mic on) or a slower codec. The result is negative and below 420 ms, or `OUT OF RANGE`, and never a positive value.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Delays above about 450 ms alias to a small value and are saved silently (periodic stimulus, `D ≡ D − 0.5 s`) | Documented limit (Task 6). The detection band 420–450 ms catches the edge zone. Owner decision in Open Question 1 (Option 2 tempo or a hybrid as a follow-up) | Flag (owner) |
| Slot-edge jitter gives the issue's "mix of both" result at the new 450 ms edge | Circular centre + unwrap (prototype rows "0.43/0.46", "−0.02/−0.06"). Pinned by `test_cluster_straddles_slot_edge` / `test_out_of_range` | In scope |
| A real 0.40 s setup with a late player bias lands at 0.42–0.45 and gets `OUT OF RANGE` | Reported, not mis-saved, which is the AC's intent. The constant is easy to adjust (Open Question 2) | In scope (flag tuning) |
| The unwrap changes values in existing low-latency tests (float noise) | Integer-period shift, so a 0 shift is bit-exact. `test_unwrap_is_identity_in_range` at 1e-12 and the existing 1e-12 tests pin it | In scope |
| Taps uniformly scattered (player not following the beat) give a meaningless centre | Same as today: MAD and spread bound the result, and there is no NaN (`atan2(0,0)=0`). A consistency gate (low resultant length) is not added | Out of scope (flag) |
| Removing `nearest_beat_index` / `max_abs_delta_seconds` breaks unknown callers | Grep shows the only callers are `calibration_screen.cpp:113` and `offset_calibration_test.cpp`. The static check covers it | In scope |
| Changing `test_schedule` could look like weakening the AC "existing tests pass" | Only the `nearest_beat_index` lines change, because they pinned the buggy rule. Every other existing test function is unchanged and passes | In scope |
| The wizard stays stuck in `OUT OF RANGE` once the 32-sample cap is full | Back cancels and re-entering resets. Open Question 3 | Flag |
| A per-device offset would be the real fix for switching outputs | #70, not implemented | Out of scope |

---

## Open Questions

None of these block implementation. The plan uses the recommended default for each. **Owner decision** marks the ones that need the owner.

1. **Owner decision: delays above about 450 ms (aliasing).** A steady 120 BPM click cannot tell `D` from `D − 0.5 s`. So Option 1 can only *detect* out-of-range delays in a narrow band (420–450 ms, or a cluster below −50 ms). A 480 ms setup is still saved as about +0.02 s. AC3 ("a delay outside the supported range is reported") is therefore met only up to about one beat.
   **Recommendation:** accept and document it. 420 ms covers the owner's Bluetooth (about 222 ms) with about 200 ms of margin, and typical A2DP/HFP paths. If a > 450 ms device appears, a follow-up can add Option 2 (60 BPM, which supports up to about 900 ms) as an "extended range" mode. Doing that now would slow every calibration.
   **Correction (review of #74):** "a cluster below −50 ms" is only detected from about −80 to −50 ms. A net delay more than about 80 ms early aliases one beat later (−0.10 s looks like +0.40 s) and is saved wrong, the same periodic-click limit as above 450 ms. The owner kept the detection arc as is; documented in `docs/AUDIO_LATENCY.md` and pinned by `test_early_alias_documented_limit`.
2. **Window constants: `max_early = 0.05`, `max_late = 0.42`.** The issue gives "e.g. −0.1 s / +0.45 s". Those two overlap at 120 BPM (0.55 > 0.5), so a tap could belong to two beats.
   **Recommendation:** 0.05 / 0.42. The early limit applies to the *average* (single taps further out still line up), and "a few tens of ms" fits within 50 ms. 0.42 gives float and jitter margin above the 0.40 AC and a 30 ms detection band. If the owner prefers more early room (for example 0.08 / 0.40), it is a one-line config change, but the 400 ms AC would then sit exactly on the edge.
3. **What to do after `OUT OF RANGE`.** The plan keeps it non-sticky (more taps can bring it back) but adds no auto-reset or "press ENTER to retry".
   **Recommendation:** keep it this way. Back + re-enter already resets, and a retry action would be new Cabinet UI (a hint-bar change) that the AC does not ask for.
4. **Wrap-safe estimate beyond the literal Option 1 text.** The issue describes only the asymmetric matching. The plan adds the circular-centre unwrap because matching alone moves the "unpredictable mix" bug to the 450 ms edge, and it can save a wrong value silently (prototype: 0.43/0.46 → −0.195 s).
   **Recommendation:** include it (about 15 lines of pure code, fully unit-tested). Without it AC3 cannot be met reliably.

---

## Acceptance Criteria

- [ ] Synthetic hits at `beat + 0.28 s` and `beat + 0.40 s` give offsets of about −0.28 s and −0.40 s, in both the model test and through `CalibrationScreen` (AC1)
- [ ] `beat + 0.02 s` → about −0.02 s, and early taps of a few tens of ms still count. The existing `offset_calibration_test` (apart from the rewritten `nearest_beat_index` lines) and `calibration_screen_test` cases pass unchanged (AC2)
- [ ] A measured delay outside [−50, +420] ms shows `OUT OF RANGE` and the red notice, Confirm does not save, and one readable log line is written (AC3, within the alias limit of Open Question 1)
- [ ] `docs/AUDIO_LATENCY.md` "Wizard headroom" and the summary describe the new limit, the detection band and the alias limit (AC4)
- [ ] The music clock and input aging are unchanged. Only beat matching and estimation changed (AGENTS.md principle 1)
- [ ] The full sandboxed suite passes **51/51**, with no new warnings. `todo-stories.md` is untouched and unstaged
