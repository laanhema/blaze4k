# Code Review: feature/015-metronome-sync-chart

**Scope**: Branch `feature/015-metronome-sync-chart` vs `main`, including uncommitted changes and untracked files (issue #15, [B7] Metronome sync-test chart).
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the permanent metronome sync-regression tool: `tests/fixtures/sync_test/metronome.sm` (240-beat / 120 BPM
fixture), `tests/sync_test_harness.hpp` (header-only fake-`SamplePosition` autoplay harness), `tests/metronome_sync_test.cpp`
(CTest executable), and the `tests/CMakeLists.txt` registration. The tool is genuinely device-free and deterministic:
it drives the real `MusicClock` via its injectable `Source` and the real `JudgmentEngine`, reads no wall-clock/frame-delta
input, and the fixture parses to 240 taps on exact quarter-note beats with balanced columns. Build is warning-clean under
`-Wall -Wextra -Wpedantic` and `ctest` is **15/15**. All three acceptance criteria are satisfied. Findings are test-strength
and invariant/doc issues; none is a defect in the shipped tool.

## Assessment of the flagraised deviation (harness measures delta from the clock, overlays engine verdict)

The deviation is **justified and does not reduce the test to harness self-math**:

- For **constant-offset drift** (case 3), the verdict asserted on every note (`delta.window`) is produced by the real
  engine path (`handle_step` → `classify_tap`), and the boundary sweep at `0.999·fw` vs `1.001·fw` genuinely pins the
  engine's inclusive `<=` boundary. This is real engine verification at sub-window resolution.
- For **progressive rate drift** (case 4), the numeric terminal delta necessarily comes from the harness because the
  engine intentionally emits no Tap event beyond Way Off; but engine-level detection is still asserted independently via
  `flagged > 0` and `report.misses == flagged`, both derived from engine-emitted `JudgmentKind::Miss` events. So AC2 is
  exercised at sub-window (engine classification) and super-window (engine miss flagging) scales.
- The sub-window boundary-flip assertions are meaningful; the only weak assertion is the analytic numeric equality (see
  Medium #1), not the engine wiring.

## Issues Found

### Critical

None.

### High Priority

None.

### Medium Priority

1. **`tests/metronome_sync_test.cpp:152-154` / `tests/sync_test_harness.hpp:75-77,120` — tautological "analytic" check.**
   `expected_drift_ms(note_seconds, rate_error, offset)` is `(note_seconds*rate_error + offset)*1000`, which is algebraically
   identical to the harness's own `delta_ms = (hit - note_seconds)*1000` (with `hit` computed from the same `note_seconds` and
   rate). The `approx(report.notes[i].delta_ms, expected, 0.05)` assertions therefore compare a value against itself and
   cannot catch a bug in the measurement path, contradicting the plan's "analysis is the reference, not a hardcoded tolerance"
   claim. A genuinely independent reference (e.g. expected delta derived from the fixture's BPM/beat math, or asserting the
   engine's emitted `event.delta_ms`) would make AC2 verification non-circular.

2. **`tests/sync_test_harness.hpp:111-130` — engine's own delta field is never asserted.** The harness reads
   `engine.events()` only to extract `event.window`; it ignores `JudgmentEvent::delta_ms` and recomputes `hit-note` itself.
   A regression in the engine's stored `delta_ms` (consumed by scoring/HUD downstream) would not be caught by this tool,
   despite it being the "judgment deltas reveal the drift" criterion. At minimum one case should assert the engine-emitted
   `delta_ms` for a judged note.

### Suggestions (Low)

3. **`tests/sync_test_harness.hpp:44-49` vs `:37-40` — inconsistent sample rate.** `music_time_at` hardcodes `48000`
   while `compute_frames`/`FakeSampleClock` use the `sample_rate` member; the two diverge if `sample_rate` is ever changed.
   Pass `sample_rate` (or the instance) into the static helper.
4. **`tests/sync_test_harness.hpp:66,68-69,82-84` — stale comments.** "one entry per judged tap" / "last judged note" no
   longer match the implementation, which now records every non-mine note (including `Miss`) and uses `back()`. Update the
   wording to match the documented deviation.
5. **`tests/metronome_sync_test.cpp:74-79` — weaker invariant than its comment.** The comment says the same-column gap
   "must exceed the engine's ±1.0 s search radius" so drift "can never select a neighbouring note", but the assertion is
   only `gap > 1.0` (equal to the radius). No-neighbour-selection requires `gap > 2×radius` (2.0 s); as written the guard
   permits selection of a neighbour for drift ≥ 1.0 s. Not reachable here (scored drift < Way Off), but the check does not
   prove the claim.
6. **`tests/metronome_sync_test.cpp:198-201` — determinism check omits `window`.** The bit-for-bit comparison covers
   `delta_ms`/`hit_seconds` and counters but not each note's engine verdict; include `window` for completeness.
7. **`tests/CMakeLists.txt:146-147` — double blank line** before the new `add_executable` block (cosmetic spacing).

## Validation Results

| Check | Status |
|-------|--------|
| Configure / Build (`cmake --build build -j16`) | PASS (no warnings/errors under `-Wall -Wextra -Wpedantic`) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (15/15; `metronome_sync_test` passed) |
| Tool direct run (`./build/tests/metronome_sync_test`) | PASS (all 7 sub-checks, exit 0) |
| Purity grep (SDL/GL/miniaudio/`<chrono>`/thread/AudioEngine) | PASS (no matches in harness/test) |
| Fixture structure (240 rows / 60 measures, exactly 1 tap each, columns balanced 60/60/60/60) | PASS |
| No production source changed (`git diff main --name-only` → `tests/CMakeLists.txt` only) | PASS |

## What's Good

- Correctly uses `MusicClock`'s injectable `Source` seam; no `AudioEngine`, window, or audio device, and no wall-clock or
  frame-delta input anywhere in the timing path (core principle 1 honored).
- Fixture is hand-maintained but structurally self-validated every run (`is_pure_beats`, counts, gap check), directly
  mitigating the "fixture drifts off-beat as it is edited" risk.
- Dual drift models (constant offset + progressive clock rate) with correct analytic values: 597.5 ms terminal for
  k=0.005, 0.1195 ms for 1 ppm, 167 flagged late notes — all independently verified correct.
- Header-only, reusable harness kept in `tests/`, registered via CTest (AC3), following the existing test/fixture/CMake idioms.
- Determinism and zero-device guarantees are asserted rather than assumed.

## Recommendation

Approve with nits. AC1/AC2/AC3 are met and the tool is correct and safe to keep. Before/while merging, the implementer
should address Medium #1 and #2 to make the AC2 verification non-tautological and to actually assert the engine's emitted
delta; the Low items are optional polish. No blocking changes required.