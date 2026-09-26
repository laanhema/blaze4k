# Code Review: Issue #9 — [B1] Music-driven gameplay clock with global offset

**Scope**: Branch `feature/009-music-clock` vs `main`, including uncommitted/untracked work
(`src/timing/`, `tests/music_clock_test.cpp`, `CMakeLists.txt`, `src/audio/sound_stream.{hpp,cpp}`,
`tests/CMakeLists.txt`)
**Recommendation**: APPROVE WITH NITS

## Summary

`MusicClock` implements the pinned formula `time_seconds = frames / sample_rate + global_offset`
exactly, sources all time from an injectable PCM-frame provider (no wall-clock/frame-delta), and is
covered by deterministic unit tests plus a guarded audio integration test. `SoundStream` gains additive
raw accessors. The change is small, clean, warning-free, and all 9 tests pass.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority
None.

### Suggestions

- `src/audio/sound_stream.cpp:145-149` — on the monotonic-clamp path `get_position_frames()` returns
  `static_cast<uint64_t>(last_position_ * sample_rate_)` (truncated), so `get_position_seconds()` can
  report up to ~1 sample (≈23 µs @44.1 kHz) *below* the stored `last_position_`, a tiny non-monotonic
  rounding regression from the old `return last_position_`. Negligible for gameplay, but returning the
  clamp in seconds (or `llround`) would preserve the prior exactness.
- `src/timing/music_clock.cpp:22-28` — a non-finite offset resets to `0.0`, discarding a previously valid
  calibration rather than keeping the last finite value. The behavior is documented/tested, so this is
  intentional; preserving the prior value would be safer against a spurious NaN from config load.
- `src/timing/music_clock.cpp:7-8` — `std::move` is used but `<utility>` is not included directly; it
  compiles via `<functional>`'s transitive include. Explicitly including `<utility>` is more portable.
- `src/timing/music_clock.hpp:11` — `SamplePosition::sample_rate` defaults to `44100` while the engine
  runs at 48000; harmless today (no source ⇒ 0 frames ⇒ 0 s) but potentially misleading.
- `tests/music_clock_test.cpp:18-56` — `write_test_wav` is duplicated verbatim from `tests/audio_test.cpp`;
  a shared test helper would avoid drift.
- Note (informational): `MusicClock`'s production adapter inherits `SoundStream`'s "monotonic while
  playing" clamp. Backward `seek_seconds()` is unaffected (it resets `last_position_`), but any genuine
  backward cursor movement without an intervening seek is masked. This is pre-existing and acknowledged
  in the plan/open questions; worth revisiting when seek/loop behavior is specified.

## Requirement / AC Verification

| Acceptance Criterion | Status | Evidence |
|---|---|---|
| `time = pcm_frames / sample_rate + global_offset` | PASS | `src/timing/music_clock.cpp:42-60` |
| No wall-clock/frame-delta in timing path | PASS | `src/timing/` includes only `<cstdint>/<functional>/<cmath>/<iostream>`; tests 4–5 |
| Positive/negative offsets shift time correctly | PASS | `music_clock.cpp:49-51`; test 2 (`+0.050`/`-0.050`) |
| Offset 0 yields a functioning clock | PASS | Test 3; default `0.0` at `music_clock.hpp:53` |
| Registered in build + tests | PASS | `CMakeLists.txt:92`, `tests/CMakeLists.txt:82-91` |

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j16`, forced recompile of changed TUs) | PASS (0 warnings) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (9/9) |
| Audio integration sub-test (device present) | PASS (`t=0.01875s`, not skipped) |
| Forbidden-dep grep in `src/timing/` | PASS (no `chrono`/`thread`/`SDL_GetPerformanceCounter`/`frame_dt`) |

## What's Good

- Pure, hardware-independent clock with an injected `std::function<SamplePosition()>` source — exactly
  matches the "audio cursor is the only authority" principle; no caching or interpolation.
- Non-finite offset guard with a tagged `[MusicClock]` log mirrors the house error-handling pattern.
- Test 4 (1000 identical reads between frame changes) is a strong, honest demonstration that output
  depends only on the provider, not wall-clock.
- Additive `SoundStream` accessors keep `audio_test` green and share a single monotonic clamp.

## Recommendation

No blocking issues. Address the optional nits (rounding on the clamp return, `<utility>` include,
test-helper dedup) opportunistically. Ready to proceed to PR/merge.
