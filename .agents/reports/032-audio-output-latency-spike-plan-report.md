# Implementation Report

**Plan**: `.agents/plans/completed/032-audio-output-latency-spike-plan.md`
**Branch**: `feature/032-audio-output-latency-spike`
**Status**: COMPLETE

## Summary

Spike for #58 (why the best offset differs between Bluetooth and wired output, and why OBS recordings
are out of sync). Delivered the investigation doc `docs/AUDIO_LATENCY.md` (root cause, latency
breakdown, OBS sign derivation and Sync Offset workaround, clock granularity, miniaudio API survey,
options A–D, decision, measurements, owner procedure), one diagnostic log line in
`AudioEngine::init`, a cross-link in `docs/CROSS_PLATFORM_VERIFICATION.md`, three follow-up issues
(#69, #70, #71) and a summary comment on #58. No timing, config or UI behaviour changed.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Create feature branch | — | ✅ |
| 2 | Diagnostic `[AudioEngine] Output device` log line | `src/audio/audio_engine.cpp` | ✅ |
| 3 | Investigation write-up (F1–F7) | `docs/AUDIO_LATENCY.md` | ✅ |
| 4 | Bluetooth measurement | `docs/AUDIO_LATENCY.md` (Measurements) | ✅ not measured; estimate + owner procedure documented |
| 5 | Cross-link | `docs/CROSS_PLATFORM_VERIFICATION.md` | ✅ |
| 6 | Full suite + smoke | — | ✅ |
| 7 | Follow-up issues F-A/F-B/F-C | GitHub #69, #70, #71 | ✅ |
| 8 | Summary comment on #58 | GitHub | ✅ |

## Validation Results

All runs that open a real audio device were routed to a temporary PipeWire null sink
(`PULSE_SINK=blaze4k_silent_test`) with the speaker sink muted as a second safeguard; routing was
verified via `pactl list short sink-inputs` (test stream on the null sink). Mute state, default sink
and module list were restored afterwards.

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j`, warning/error grep) | ✅ no warnings, no errors |
| Lint | n/a (no linter configured; warnings-as-lint gate above) |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 38/38 passed |
| Diagnostic line in `audio_test -V` | ✅ `[AudioEngine] Output device: 'Ryzen HD Audio Controller Speaker' via PulseAudio, period 900 frames x 4 @ 48000 Hz (18.75 ms/period; excludes OS mixer and Bluetooth latency)` |
| Headless smoke (`--headless --smoke-test 10`) | ✅ exit 0 |
| Doc references (`sed -n` on every `src/…:line` and `miniaudio.h:line`) | ✅ all point at the cited code |
| Issues (titles, labels, F-B blocked by F-A) | ✅ |
| Scope guard (`git diff --stat`, `todo-stories.md` still `??`) | ✅ |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `docs/AUDIO_LATENCY.md` | CREATE | +227 |
| `src/audio/audio_engine.cpp` | UPDATE | +15/-0 |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | +3/-1 |

## Deviations from Plan

- **Silent test runs.** The plan runs `ctest` directly; `audio_test` opens the real default device
  and plays a short 440 Hz tone. To avoid audible sound, every run that opens a device used a
  temporary null sink (`pactl load-module module-null-sink`, `PULSE_SINK=`) plus a muted speaker
  sink; both were restored. Consequence: the logged device name is the *default* sink, not the null
  sink the stream played on. This is a real finding (default-device name resolution vs actual stream
  sink) and was added to the doc and to #69's technical notes.
- **Granularity re-measured** with the plan's silent probe recipe (scratch only, deleted): min 388,
  mean 892.5, max 900 frames. The doc reports the range 876.5–892.5 across both runs.
- **Line ranges tightened** where the plan's ranges were slightly off after inspection:
  `music_clock.hpp:14-21` / sign `19-21`, `music_clock.cpp:53-60`, `offset_calibration.hpp:16-18`,
  `audio_engine.cpp:19-52` (after the edit), miniaudio reroute docs `6748-6787`, PulseAudio sink
  callback `29882`.
- **Bluetooth** (owner-resolved): not measured; documented as a typical 150–300 ms estimate plus the
  owner procedure. The −22 ms saved offset is recorded as calibrated on an unknown device.
- **F-C body** states explicitly that any interpolation needs owner sign-off (AGENTS.md principle 1).
- **F-B** Bluetooth figure labelled as an estimate in the issue body.
- **#58** received two comments: the spike summary (Task 8) and the implementation comment (skill
  Phase 6).

## Tests Written

None (as planned). The only code change is a log statement, verified through `audio_test` verbose
output. Test count stays 38.
