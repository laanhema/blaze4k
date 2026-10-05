# Implementation Report

**Plan**: `.agents/plans/completed/053-audio-period-clock-interpolation-plan.md`
**Branch**: `feature/053-audio-period-clock-interpolation`
**Status**: COMPLETE (agent scope). Owner hardware and listening checks are pending (⏳ below)
**GitHub Issue**: #81

## Summary

Implemented B + C from the #71 decision:

- **B.** `AudioEngine` now requests `periodSizeInFrames = 480` by default. The new config key `audio.period_size_frames` overrides it: `0` is the backend default, and any other value is clamped to 128–4096 with a warning. A negative value clamps to 128. The change applies on the next start. The `Output device` log line now shows the requested and the negotiated period.
- **C, anchor capture.** A miniaudio `onProcess` hook runs on the audio thread. It reads the injected monotonic clock (`SDL_GetTicksNS` from `main.cpp`, through a function pointer so `src/audio` stays SDL-free) before the clock sound's cursor. It publishes `(cursor, ns, device period)` through a lock-free seqlock of atomics (`AnchorSlot`).
- **C, burst grouping.** `CallbackGrouper` merges burst engine updates into one device-callback interval.
- **C, estimator.** `ClockInterpolator` runs on the game thread. Its output is at most one period ahead of the latest anchor, never below raw, never past the song length, and never decreasing. It resets on seek, stop, play, resume and load, and ignores pre-reset anchors.
- **Tap safety.** A Dekker-style detach handshake runs before `ma_sound_uninit`.
- **Timed pair.** `SamplePosition::timestamp_ns`, `TimedMusicTime` and `MusicClock::timed_time_seconds()` give one consistent pair. Gameplay and calibration age input against it with `aging_reference_ns`.
- **Assist ticks** stay on the raw cursor.
- **Stats.** Gameplay logs the measured device-callback interval when it shuts down.
- **Tool.** A committed, silent `clock_probe` tool, not registered with ctest.
- **Docs.** `AUDIO_LATENCY.md`, `CROSS_PLATFORM_VERIFICATION.md` and a README "Upgrading" note cover the change and the advice to recalibrate.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure interpolator | `src/timing/clock_interpolator.{hpp,cpp}`, `CMakeLists.txt` | ✅ |
| 2 | Grouper + seqlock + ClockTap | `src/audio/clock_anchor.{hpp,cpp}`, `CMakeLists.txt` | ✅ |
| 3 | Pure tests | `tests/clock_interpolation_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 4 | AudioEngine settings/period/onProcess/tap/null backend/stats | `src/audio/audio_engine.{hpp,cpp}` | ✅ |
| 5 | SoundStream interpolation | `src/audio/sound_stream.{hpp,cpp}` | ✅ |
| 6 | Null-backend integration test | `tests/clock_tap_test.cpp`, `tests/CMakeLists.txt`, `tests/test_wav_writer.hpp` | ✅ |
| 7 | MusicClock timed pair + aging helper + citation fix | `src/timing/music_clock.{hpp,cpp}`, `src/gameplay/judgment_input.hpp` | ✅ |
| 8 | Wire gameplay, calibration, metronome | `src/gameplay/gameplay_view.{hpp,cpp}`, `src/screens/calibration_screen.cpp`, `src/audio/metronome.cpp` | ✅ |
| 9 | Config key | `src/data/config.hpp`, `src/data/config_loader.cpp`, `tests/config_persistence_test.cpp` | ✅ |
| 10 | Boot wiring | `src/main.cpp` | ✅ |
| 11 | Music-clock tests | `tests/music_clock_test.cpp`, `tests/judgment_engine_test.cpp` | ✅ |
| 12 | Measurement tool | `tests/clock_probe.cpp`, `tests/CMakeLists.txt` | ✅ |
| 13 | Documentation | `docs/AUDIO_LATENCY.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md` | ✅ |
| 14 | Re-measure on the default sink | `docs/AUDIO_LATENCY.md` | ✅ (one run, per the invoking request) |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC 16.2.1) | ✅ |
| Lint: zero new warnings in touched TUs (`touch` + rebuild + grep) | ✅ `no new warnings` (0 `warning:` lines in the whole rebuild) |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 51/51 passed (baseline 49 + `clock_interpolation_test` + `clock_tap_test`) |
| Flake check: `clock_tap_test` ×5 sandboxed | ✅ 5/5 |
| `metronome_sync_test` | ✅ |
| Static: no SDL includes/calls in `src/audio`, `src/timing` | ✅ (narrowed check, see Deviations) |
| Static: `clock_interpolator.hpp` / `music_clock.hpp` include only `<cstdint>` / `<functional>` | ✅ |
| Static: no mutex/logging/allocation in `src/audio/clock_anchor.cpp` | ✅ |
| Static: `get_raw_position_seconds` used in `gameplay_view.cpp` (assist ticks) | ✅ line 357 |
| Static: `908-919` nowhere in `src`/`docs`; no "follow-up issue #81" | ✅ |
| Static: `todo-stories.md` not staged | ✅ 0 (still an unrelated unstaged change, untouched) |
| E2E 1: automated suite | ✅ |
| E2E 2: headless smoke (select screen, scratch data dir) | ✅ `Blaze 4k shut down cleanly.`, no `configure() after init` warning |
| E2E 2b: `{"audio":{"period_size_frames":64}}` | ✅ warning `audio.period_size_frames out of range; clamped`, log `requested period 128`, saved file has `128` |
| E2E 3: `clock_probe 480 --null` (sandboxed) | ✅ requested/negotiated 480, both residual lines printed |
| E2E 4: `clock_probe 480` on the real default sink (outside the sandbox, silent, one run) | ✅ interpolated rms **1.46 ms** (≤ 2 ms target), max 2.79 ms; raw step 480/480/480 |
| E2E 5a: wired full-song listening check + negotiated period | ⏳ owner |
| E2E 5b: Bluetooth full-song listening check (crackles?) | ⏳ owner |
| E2E 5c: feel at high speed mod (no ~18 ms stepping) | ⏳ owner |
| E2E 5d: recalibrate on each device | ⏳ owner |
| E2E 5e / optional TSan run | ⏳ owner (libtsan not installed here) |
| Windows / macOS negotiated period | ⏳ owner |

### Measurement (Task 14, recorded in `docs/AUDIO_LATENCY.md`)

`pactl get-default-sink` returned `bluez_output.88_C9_E8_25_D5_B1.1` both before and after the run. The default sink was the **WH-1000XM4 Bluetooth headset**, not the speaker sink measured in #71.

```
[AudioEngine] Output device: 'WH-1000XM4' via PulseAudio, requested period 480 frames (0 = backend default), period 1440 frames x 1 @ 48000 Hz (30 ms/period; excludes OS mixer and Bluetooth latency)
[clock_probe] raw step min / mean / max: 480 / 480.00 / 480 frames (299 steps)
[clock_probe] engine callback interval: min 480 / max 480 frames over 299 callbacks
[clock_probe] raw residual rms 3.26 ms / max 7.37 ms
[clock_probe] interpolated residual rms 1.46 ms / max 2.79 ms
[clock_probe] interpolated mean lead over raw: 5.12 ms
```

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/clock_interpolator.hpp` | CREATE | +64 |
| `src/timing/clock_interpolator.cpp` | CREATE | +43 |
| `src/audio/clock_anchor.hpp` | CREATE | +77 |
| `src/audio/clock_anchor.cpp` | CREATE | +88 |
| `tests/clock_interpolation_test.cpp` | CREATE | +432 |
| `tests/clock_tap_test.cpp` | CREATE | +198 |
| `tests/clock_probe.cpp` | CREATE | +234 |
| `CMakeLists.txt` | UPDATE | +2/-0 |
| `README.md` | UPDATE | +2/-0 |
| `docs/AUDIO_LATENCY.md` | UPDATE | +99/-21 |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | +14/-3 |
| `src/audio/audio_engine.cpp` | UPDATE | +113/-1 |
| `src/audio/audio_engine.hpp` | UPDATE | +55/-0 |
| `src/audio/metronome.cpp` | UPDATE | +6/-1 |
| `src/audio/sound_stream.cpp` | UPDATE | +102/-2 |
| `src/audio/sound_stream.hpp` | UPDATE | +32/-0 |
| `src/data/config.hpp` | UPDATE | +11/-0 |
| `src/data/config_loader.cpp` | UPDATE | +16/-0 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +19/-5 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +1/-0 |
| `src/gameplay/judgment_input.hpp` | UPDATE | +10/-1 |
| `src/main.cpp` | UPDATE | +14/-0 |
| `src/screens/calibration_screen.cpp` | UPDATE | +6/-3 |
| `src/timing/music_clock.cpp` | UPDATE | +8/-0 |
| `src/timing/music_clock.hpp` | UPDATE | +21/-3 |
| `tests/CMakeLists.txt` | UPDATE | +36/-0 |
| `tests/config_persistence_test.cpp` | UPDATE | +61/-0 |
| `tests/judgment_engine_test.cpp` | UPDATE | +7/-0 |
| `tests/music_clock_test.cpp` | UPDATE | +39/-0 |
| `tests/test_wav_writer.hpp` | UPDATE | +40/-0 |

## Deviations from Plan

1. **`tests/test_wav_writer.hpp` was extended** (not in the plan's file list). I added `write_silent_wav(path, seconds, rate, channels)`. The existing helper only writes a 44.1 kHz mono sine, and Task 6 needs a silent 48 kHz file. The existing function is unchanged.
2. **Task 14 ran once, not ×3 per period.** The invoking request allowed exactly one run on the real sink, so only `clock_probe 480` was run. `clock_probe 0` and the repeat runs are listed as owner items. The default sink was Bluetooth (WH-1000XM4), not the #71 speaker sink, so this is the first Bluetooth probe row. It is **not** the owner's full-song crackle check.
3. **The null-backend probe residuals are not "near zero"** as the plan expected. Measured raw rms 3.99 ms and interpolated rms 2.70 ms in one run. A second run of the default-init path in the sandbox, which fell back to Null, gave 2.90 / 0.05 ms. miniaudio's null device paces itself with its own thread and occasionally merges two periods (max group 960). It proves the path works, not precision. This is recorded in the docs.
4. **The static "no SDL" grep was narrowed** to includes/calls (`#include.*SDL|SDL_[A-Za-z]+\(`). The literal `grep -rn "SDL" src/audio src/timing` matches comments that document the injected `SDL_GetTicksNS` timebase, which the plan's design requires. The narrowed check finds no hits.
5. **The headless gameplay-demo smoke cannot show callback stats.** I ran `--gameplay-demo` once beyond the plan's E2E 2. It logs `clock interpolation on` and the new `Device callback interval (gameplay)` line, but with `0 callbacks`. The headless loop is not wall-paced: 20,000 frames run in a few ms, so playback ends before an audio callback group completes. The real `onProcess` → tap path is covered by `clock_tap_test`, and by `clock_probe` without `--null` in the sandbox, where the Null fallback gave 97 callbacks. The owner's full-song check (5a/5b) will show real gameplay stats.
6. **Small additions beyond the pinned API**, all minimal:
   - `SoundStream::clock_interpolation_enabled()`.
   - `AudioEngine::settings()`, `engine_sample_rate()` and `now_fn()`. `now_fn()` returns the snapshot taken at `init()`, so a late `configure()` never changes the audio thread's function mid-run.
   - `ClockInterpolator::reset_ns()`, used by the tests.
   - `CallbackGrouper::reset()`, called in `init()`.
   - `enable_clock_interpolation()` re-attaches when called again. It stays idempotent, and it can make a stream whose tap was replaced the active tap again.
7. **`clock_tap_test` bounds are loose** so a loaded host does not flake the test:
   - After `seek_seconds(0.5)` the reading must be between 24000 − one period and 24000 + 100 ms.
   - The resume reading must be between the paused value and paused + 100 ms.
   - Added beyond the plan: 50 attach/unload cycles of the **active** tap during playback (the real handshake), and a move-construction check.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/clock_interpolation_test.cpp` (new ctest) | 1. Grouper: steady 480; 128×2 burst → 256; 0-frame and 0-rate ignored without moving `last_update_ns`; early callback merges (≤ 960) and recovers; min/max/callbacks stats; reset. 2. Seqlock: empty read is false; round trip; 2-thread stress with 1e6 writes, invariant checked on every read. 3. Steady: error ≤ 1 frame, strictly increasing. 4. Late callback: no decrease, no jump > cap, back on the line. 5. Burst: grouped period never stalls, while a per-update cap of 128 does (stall count asserted). 6. Stall: frozen at anchor + cap, monotonic after resume. 7. Pause/seek: raw while paused, pre-reset anchor ignored, backward seek, no resume jump. 8. Song-end clamp; raw wins over an inexact length. 9. Resampling 44.1k/48k (cap 441). 10. Race window. 11. Clock skew. 12. Zero rates; overflow safety with `UINT64_MAX`. Every query checks out ≥ raw, out ≤ anchor + cap, and monotonic output |
| `tests/clock_tap_test.cpp` (new ctest) | Null-backend real-time run: `enable_clock_interpolation` is true and idempotent. 1.5 s of 1 ms polls: monotonic, ≥ raw, ≤ raw + period, non-zero monotonic timestamps, > 3× distinct values, mean lead > 0. Forward and backward seek. Pause freezes. Resume does not jump. Stop reads 0. A second stream replaces the tap; unloading the inactive tap is safe. 50 active-tap detach cycles. Move keeps the tap working. SKIP only if the null backend cannot init |
| `tests/music_clock_test.cpp` | `timed_time_seconds` makes one source call and applies the offset, with the source's timestamp. A 2-field `SamplePosition` gives `timestamp_ns == 0`. No source gives a zero pair. Consistent-pair aging (event 3 ms before T → m − 0.003). Fallback reference used when there is no timestamp |
| `tests/judgment_engine_test.cpp` | `aging_reference_ns(0, F) == F`, `aging_reference_ns(T, F) == T`, aging through the helper |
| `tests/config_persistence_test.cpp` | Default 480. Validate rejects 100, 4097 and −1, and accepts 0, 128 and 4096. Round trip of 256 and of 0 (no warning). Load: 64 → 128, −5 → 128, −5e6 → 128, 99999 → 4096 (all warn). `"abc"` and `300.5` keep 480 with a warning. null and missing keep 480 without a warning. 0, 128, 4096 and 960 are kept |
