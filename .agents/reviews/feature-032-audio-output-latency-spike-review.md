# Code Review: feature/032-audio-output-latency-spike

**Scope**: Branch `feature/032-audio-output-latency-spike` vs `main` (no commits on the branch; uncommitted changes to
`src/audio/audio_engine.cpp` and `docs/CROSS_PLATFORM_VERIFICATION.md`, plus untracked `docs/AUDIO_LATENCY.md` and the
plan/report under `.agents/`). `.agents/stories/todo-stories.md` is pre-existing and out of scope. GitHub issue #58
(spike), follow-ups #69, #70 and #71.
**Recommendation**: APPROVE (with three Low nits)

## Summary

Spike #58 asks why the best offset differs between Bluetooth and wired output and why OBS recordings drift. The branch
delivers `docs/AUDIO_LATENCY.md` (root cause, latency path, OBS sign derivation and workaround, clock granularity,
miniaudio API survey, options A–D, decision, measurements and owner procedure), one diagnostic log line in
`AudioEngine::init`, and a cross-link. All four acceptance criteria are covered, and the follow-ups (#69, #70 blocked by
#69, #71 with owner sign-off for any interpolation) are filed. I re-checked every `src/…:line` and `miniaudio.h:line`
citation against the tree and the vendored miniaudio 0.11.21, and they all hold. The code change is safe and builds
warning-free. Three Low nits remain, mostly about doc precision.

## Reference verification (re-done for this review)

| Claim in `docs/AUDIO_LATENCY.md` | Checked against | Verified |
|---|---|---|
| Clock = `ma_sound_get_cursor_in_pcm_frames`, song fully decoded | `sound_stream.cpp:55-62` (`MA_SOUND_FLAG_DECODE`), `:138-154` | Yes |
| Formula and sign | `music_clock.hpp:14-21`, `music_clock.cpp:53-60` | Yes |
| Offset storage and clamp ±3600 | `config.hpp:37-39`, `config_loader.cpp:203-210`, `:393` | Yes |
| Calibration forces offset 0, `offset = -mean(hit - beat)` after outlier rejection | `calibration_screen.cpp:63`, `:98-121`; `offset_calibration.cpp:62-120`; sign `offset_calibration.hpp:16-18` | Yes |
| Wizard is audio-only (no display latency) | `calibration_screen.cpp` render is text only, no visual beat cue | Yes |
| Input aging | `judgment_input.hpp:11-19`, `app.cpp:152` | Yes |
| Engine defaults: default device, low-latency profile | `ma_engine_init` copies no `performanceProfile` into a zeroed device config; `ma_performance_profile_low_latency = 0` (`miniaudio.h:4312`) | Yes |
| API lines 11240, 9006, 9059, 6985-7008, 11184, 6717, 6748-6787, 7496, 30242-30250, 29166, 29882, 30520, 20472, 20595 | `build/_deps/miniaudio-src/miniaudio.h` | Yes, all point at the cited symbol |
| PulseAudio: `internalPeriodSizeInFrames` is `tlength`, `periods × size` is `maxlength` | `miniaudio.h:30031-30033` (`tlength = maxlength / periods`), `:30639-30644` | Yes |
| Sink info callback copies only name/description; `pa_stream_get_latency` never loaded; no `AUTO_TIMING_UPDATE` on playback | `:29882-29902`; `grep -c pa_stream_get_latency` = 0; `:30520` | Yes |
| DirectSound reroutes without notification | `:6779-6782` (`* DirectSound` at `:6782`) | Yes |
| Correction to #58's technical notes (no device enumeration) | `audio_engine.cpp:24-25` uses `ma_engine_config_init()` defaults only | Yes |
| PRD backing for the diagnostic line | `PRD.md:275` "input/audio latency measurable via log output" | Yes |

The OBS sign derivation (`AUDIO_LATENCY.md:101-109`) follows from the cited clock and calibration code: with
`offset ≈ -(L + L_in)` the arrow reaches the receptor at cursor `t + L + L_in`, and the sink monitor carries the note's
sound at about `t + ε`.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`docs/AUDIO_LATENCY.md:111-117`: the OBS workaround value does not match the doc's own derivation.** Line 109
   derives the recorded lag as `L + L_in − ε`, where ε is the client buffer plus graph quantum, about 18.75 + 5.3 ≈ 24 ms
   as measured on this machine. The workaround still says to set Sync Offset to `+|offset_ms|`, which is `L + L_in`, so
   it overcorrects by about ε. Likewise, the wired bullet (lines 111-112) puts the lag at about 20 ms from the −22 ms
   offset. By the same formula it comes to about 0 ms. The `|…|` form also silently assumes a negative offset.
   *Recommendation:* present `−offset_ms − ε` (or "start at `−offset_ms`, minus roughly one client buffer, then trim
   by eye with a clap test") as a starting point. The Bluetooth conclusion is unaffected, since ε is small next to
   150–300 ms.

2. **`docs/AUDIO_LATENCY.md:202-203`: the config path is ambiguous.** The owner procedure says to read
   `data/config.json`, while the Measurements row (line 187) says `build/data/config.json`. The real location is
   `<executable dir>/data/config.json`, or the XDG dir, or `--data-dir` (`src/data/data_paths.cpp:10-29`).
   *Recommendation:* write `build/data/config.json` (dev build) or "the `config.json` named in the startup log".

3. **`src/audio/audio_engine.cpp:41,46`: small robustness and API nits in the diagnostic line.** The
   `ma_device_get_name` result is ignored. That is memory-safe, because miniaudio clears the buffer first
   (`miniaudio.h:42266-42268`), but a failure prints `''` with no hint. `device->pContext->backend` also reaches
   through the struct even though the public `ma_device_get_context()` exists (the `playback.internal*` fields have no
   getters, so direct access there is fine). *Recommendation:* print `<unknown>` when the call does not return
   `MA_SUCCESS`, and use `ma_device_get_context(device)->backend`. Optional, because #69 replaces this line with the
   `OutputDeviceInfo` struct.

**Noted, not a finding:**
- The logged name for a default device comes from the current default sink, not the stream's actual sink. This is
  documented (`AUDIO_LATENCY.md:58-61`) and carried into #69's technical notes.
- On PulseAudio, `ma_device_get_name` makes one blocking server round trip. It runs once at init on the context's own
  mainloop, separate from the device mainloop, so it does not race the audio thread.
- No new tests. This was planned: the change is a log statement only.
- The Bluetooth latency is an estimate, not a measurement. The owner accepted this, the doc labels it clearly and
  includes the owner procedure.

## Validation Results

All device-opening runs were done inside a `bwrap` sandbox with `/run/user/1000` (PipeWire/Pulse sockets) and
`/dev/snd` hidden, the network unshared and `PULSE_SERVER`/`PIPEWIRE_REMOTE` pointed at nonexistent paths. Before
running anything I confirmed inside the sandbox that `pactl`, `pw-cli` and `aplay -l` all failed to reach audio, so
miniaudio fell back to its Null backend and no sound could play.

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j`, grep for warnings/errors) | PASS (nothing to rebuild; no warnings or errors) |
| Warnings gate: `g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsyntax-only src/audio/audio_engine.cpp` with build includes | PASS (0 warnings) |
| Lint | n/a (no linter configured; the warnings gate above stands in) |
| Tests (`ctest --test-dir build --output-on-failure`, sandboxed) | PASS 38/38, none skipped |
| Diagnostic line (sandboxed `audio_test` / `music_clock_test` `-V`) | PASS: `Output device: 'NULL Playback Device' via Null, period 480 frames x 3 @ 48000 Hz (10 ms/period; …)`. Exercises the non-Pulse path |
| Headless smoke (`build/blaze-4k --headless --smoke-test 10`, scratch `--data-dir`, sandboxed) | PASS (exit 0, line logged once) |
| Real PulseAudio/Bluetooth output path | Not run here (no audible playback allowed). The implementer's null-sink run is recorded in the implementation report |

## What's Good

- Measured and estimated numbers are kept apart and labelled everywhere. The Bluetooth figure is never presented as
  measured.
- The doc corrects the wrong technical note on #58 instead of carrying it forward.
- Every claim cites `file:line`, and all the citations I checked are right. The miniaudio survey (what is and is not
  exposed in 0.11.21) is accurate and saves #69/#70 from rediscovering it.
- The decision follows AGENTS.md principle 1: `MusicClock` is unchanged and compensation stays in the offset.
  Interpolation is correctly sent to #71 behind owner sign-off.
- The single code change is diagnostic only, guarded against a null device and a zero sample rate, and runs once per
  init.
- The test runs that could open a device were routed to a null sink, and that run surfaced a real finding (default-sink
  name vs actual stream sink) that was then fed into #69.

## Recommendation

Approve. Fix nits 1 and 2 in the doc before merging if convenient (both are one-line wording changes). Nit 3 is
optional, because #69 replaces the log line. After merge, the owner should fill in the Bluetooth measurements with the
documented procedure.
