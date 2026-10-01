# Code Review: feature/012-judgment-engine-event-log (Issue #12)

**Scope**: Branch `feature/012-judgment-engine-event-log` vs `main`, including uncommitted and untracked changes (no commits on the branch yet). New files: `src/gameplay/judgment.hpp`, `judgment_input.hpp`, `judgment_engine.{hpp,cpp}`, `tests/judgment_engine_test.cpp`; modified: `gameplay_view.{hpp,cpp}`, `app.{hpp,cpp}`, `main.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`.
**Recommendation**: APPROVE WITH NITS

## Summary

B4 implements a pure, event-sourced judgment engine: tap/mine classification off the reused B2 `JudgmentConstants`, deterministic Miss/AvoidedMine expiry, an analytic (non-frame-delta) hold/roll life model, roll re-hit refresh, and held-over-mine crossing — all emitted as immutable `JudgmentEvent`s. The judgment core is correctly time-parameterized and platform-free (no SDL/GL/`<chrono>`/frame-delta), integration into `GameplayView`/`App`/`main` follows existing patterns, and the new test target is green (12/12). The pinned OpenITG semantics were spot-checked against upstream `f2c129fe` and match in all major paths. Findings are limited to edge-case logic and test-strength issues; nothing blocks merge.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

- **`tests/judgment_engine_test.cpp:321-344` — the frame-rate-independence test does not exercise the analytic decay it claims to validate.** The test holds the button throughout (`held_col(0)` on every update), so `held_now` is always true and `life` is pinned at `1.0`; the `1 - (t - satisfied)/window` branch is never evaluated. The test would still pass if the decay branch accumulated frame deltas (e.g. `life -= step/window`). Add a coarse-vs-fine comparison on the *release/decay* path (and a roll re-hit decay case) to actually pin invariance.
- **`src/gameplay/judgment_engine.cpp:267-284` — a hold held through its tail can be judged `HoldNg` if a single update gap exceeds `hold_ok` (0.32 s).** `held_now` requires `music_time <= hold_end_time_seconds`, so on the first update past the tail `held_now` is false, and `life` is decayed from the previous satisfied time; a gap > 0.32 s (e.g. a main-loop/audio stall) clamps life to 0 and emits NG even though `held_columns[col]` is still true. OpenITG keeps `fLife = 1` while the button is down and returns OK, so this is a real divergence. Not reachable at the 60 Hz fixed step, but it is a frame-rate-dependent outcome in a module whose contract is frame-rate independence. Consider using raw `held_columns[col]` (not `&& music_time <= end`) for the life term, or treating `held` at/after the tail as satisfied.

### Suggestions

- **`src/gameplay/judgment_engine.cpp:111,271-279` — hold life decays from an early head hit rather than from note start.** When a hold head is hit early (up to 0.18 s) and then released, `hold_satisfied_time` is seeded with the early `hit_time`, so Blaze 4k has already decayed by `(note_start - hit_time)` at note start, whereas OpenITG sets `fLife = 1` at the head step and only begins decaying once the hold row is reached. Practical impact is small (a head hit normally means the button is held); clamping the seed to `note.time_seconds` would be exact.
- **`src/gameplay/judgment_input.hpp:11` + `src/gameplay/gameplay_view.cpp:102` + `src/app/app.cpp:116` — the ns reference and the music reference are not sampled together.** `input_reference_ns_` is captured at the end of `App::process_events()`, but `reference_music` is sampled later inside `handle_input_events()`, so each input is systematically over-aged by the poll→update latency. This matches the ratified "capture once" decision and is far below one window, but the plan's claim that they are sampled "together" is not literally true; sampling the music clock at the same point as `SDL_GetTicksNS()` would remove the bias.
- **`src/gameplay/judgment_engine.cpp:355` — `is_note_judged` is false for a hold/roll whose head was hit but tail is pending.** `complete` only flips on OK/NG, so the B5 handoff sees a hit hold head as "not judged". Harmless for B4 (unused in production) but worth clarifying before B5 consumes it.
- **`src/gameplay/judgment.hpp:3` — unused `<cstdint>` include.** Trivial.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j16`) | PASS |
| Warnings (`-Wall -Wextra -Wpedantic`, forced recompile of new/changed sources) | PASS (no warning/error lines) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS 12/12 |
| `./build/tests/judgment_engine_test` (15 sub-checks) | PASS |
| Headless smoke (`--headless --gameplay-demo … --smoke-test 120`) | PASS (exit 0, `[GameplayView] Session judgment events: 0`, no GL) |
| Purity grep (SDL/GL/`ma_`/`chrono`/`fixed_dt`/`delta_time`/`frame` in judgment core) | PASS (only comment text matches; no includes/usages) |

## What's Good

- The judgment core is genuinely pure: `judgment.hpp`, `judgment_input.hpp`, `judgment_engine.{hpp,cpp}` include no SDL/GL/audio/`<chrono>` and take absolute music time — the only wall-clock read (`SDL_GetTicksNS`) lives in `App` and is used solely to age inputs.
- B2 constants are reused, not redefined: `classify_tap`, `windows.hit_mine`, `windows.way_off`, `windows.hold_ok`, `windows.hold_roll`. `JudgmentEvent {column, note_time_seconds, hit_time_seconds, delta_ms, window}` matches the issue schema (plus `kind`/`hold`/`note_type`/`note_index` for the B5 handoff).
- Semantics spot-checked against upstream `f2c129fe` and consistent: step-beyond-way-off → no event (not Miss); mine uses its own 0.070 window; untouched mine → AvoidedMine; missed hold head never yields OK/NG (`Player.cpp:519,583,587`); roll re-hit resets life with no score effect; held-over-mine triggers HitMine.
- Deterministic Miss/AvoidedMine `hit_time = note_time + way_off`, append-only log with a correct drain suffix, and the OpenITG hidden-note rule (`score >= Great`, hit mines) reused as the sole visual contract — no judgment sprites (correctly deferred to D2).
- B4 boundary respected: no combo/life/DP/percent/grade state in the engine.

## Recommendation

Approve with nits. Address the Medium test gap (add a decay-path frame-rate-independence case) and the >`hold_ok` stall false-NG before or alongside B5, since both concern the same analytic hold model that later stages will build on.
