# Plan: Investigate Music-Clock Granularity — Cursor Advances in ~19 ms Callback Steps (#71)

## Summary

Issue #71 is a **spike**. The output is a set of measurements, cited documentation and an
**owner decision**. It does not ship gameplay code. The music clock
(`src/audio/sound_stream.cpp:138-153` → `ma_sound_get_cursor_in_pcm_frames`) advances once per
audio callback, about 18.75 ms on the dev machine. Input aging (`src/gameplay/judgment_input.hpp:11-19`)
pairs that stepped value with an SDL nanosecond reference taken at a different moment
(`src/app/app.cpp:152`), so judged hit times carry up to one callback of jitter. Arrows also move in
callback-sized jumps.

The implementer will:

1. Measure the callback step with a silent scratch probe on the current default Linux sink (agent),
   and write owner procedures for wired 3.5 mm, Bluetooth, Windows and macOS.
2. Document how OpenITG and StepMania 5 get the music position between hardware updates, with
   pinned-commit `file:line` citations. The research is already done and recorded in
   [Upstream Reference Findings](#upstream-reference-findings) below.
3. Evaluate the candidate approaches (A–E below) against AGENTS.md principle 1 in
   `docs/AUDIO_LATENCY.md`, with no recommendation presented as decided.
4. Post the options on #71 and ask the owner to decide. **No interpolation code is written in the
   repo.** The follow-up implementation issue is filed only after the owner approves a change.

Main finding from the research: neither reference engine uses a "mix cursor" like ours. Both ask the
audio driver for the **hardware playback position at query time** and pair it with a timestamp taken
in the same call. On some backends that position is itself extrapolated from the host clock (SM5
CoreAudio, SM5 PulseAudio via `PA_STREAM_INTERPOLATE_TIMING`). SM 5.0.12's PulseAudio driver behaves
exactly like Blaze today (it returns the write position, updated per callback) and carries an
"arrows stutter" comment. Later StepMania replaced it with interpolated stream time.

## User Story

As the owner of Blaze 4k
I want to know how coarse the music clock is on my devices, how ITG/StepMania solve this, and what
each fix would cost against the "music clock only" principle
So that I can make an informed decision before any interpolation code is written.

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT (spike: research, measurement, documentation and decision) |
| Complexity | MEDIUM |
| Systems Affected | `docs/AUDIO_LATENCY.md` only (plus GitHub comment and possibly a follow-up issue). No `src/` or `tests/` changes |
| GitHub Issue | #71 (related: #58, #69, #70) |
| Branch | `feature/036-music-clock-granularity-spike` (matches `feature/035-…` convention) |

---

## Environment Findings

| Tool / Fact | Version / Value | Notes |
|-------------|-----------------|-------|
| Repo HEAD | `main` @ `c22aa15` | Clean tree |
| CMake / GCC | 4.4.3 / 16.2.1 | `cmake --build build -j` is up to date |
| Baseline tests | **41/41 pass** (0.43 s) | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build`. **Start green, stay green:** this plan adds no test target, so the count stays at 41 |
| Audio stack | PipeWire 1.6.9 with pipewire-pulse | miniaudio picks the PulseAudio backend |
| Sinks available | Only `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink` (default). UCM split sinks: the 3.5 mm headphone sink only exists when a plug is inserted | The agent measures **only the current default sink**. Wired 3.5 mm and Bluetooth are owner steps |
| Bluetooth | WH-1000XM4 paired, not connected | The agent must not connect, route or select Bluetooth |
| Windows / macOS | No toolchain or host available (`docs/CROSS_PLATFORM_VERIFICATION.md:19`) | Owner steps, or recorded as "not measured" |
| miniaudio | `0.11.21` (`CMakeLists.txt:53`), header at `build/_deps/miniaudio-src/miniaudio.h` | All `miniaudio.h:N` citations below refer to this file |
| OpenITG source | `<scratchpad>/oitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Same commit pinned in `src/timing/judgment_constants.hpp` and plans 030/031/033. Re-fetch: `git clone -q --filter=blob:none --no-checkout https://github.com/openitg/openitg.git oitg && git -C oitg checkout -q f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` |
| StepMania 5.0.12 | `<scratchpad>/sm512` @ tag `v5.0.12` = `45e0787a7457c1b9071522463aa902d59ae3a2ee` | `git clone -q --filter=blob:none --no-checkout https://github.com/stepmania/stepmania.git sm512 && git -C sm512 fetch -q --depth 1 origin tag v5.0.12 && git -C sm512 checkout -q v5.0.12` |
| StepMania 5_1-new | `<scratchpad>/sm5` @ `825467bcd81c812b33ad684dc04dd151b2d5dec3` (2026-08-21) | `git clone -q --depth 1 --filter=blob:none https://github.com/stepmania/stepmania.git sm5`. **Cite the commit hash, not the branch**, because the branch moves |
| Off-limits | `.agents/stories/todo-stories.md` (untracked) | Do not modify, stage or revert it |

### Audio safety rules (binding for every probe)

- Probes play **only an all-zero buffer with `ma_sound_set_volume(…, 0)`**. They never play audible
  content.
- **Do not change system audio state:** no `pactl set-sink-mute`, no `pactl load-module
  module-null-sink`, no `PULSE_SINK=` rerouting, no `pactl set-default-sink`, no `wpctl` or
  `bluetoothctl` changes. (The current `docs/AUDIO_LATENCY.md:238-240` recommends a temporary null
  sink. Task 4 replaces that advice.)
- A probe that requests a small period makes PipeWire lower its graph quantum **while the probe is
  open**. That is transient and ends when the probe exits, but run probes only for a few seconds.
- Probes live in `<scratchpad>/probe/` and are **never committed**. A probe may compute a
  callback-anchored *estimate* for measurement only. That is analysis, not an implementation. No
  interpolation code goes into `src/`.
- `ctest` and the app run inside the bwrap sandbox (`--tmpfs /dev/snd`). Only the scratch probe opens
  the real device, because measuring real callback steps needs it.

---

## Preliminary Measurements (taken while planning, to be re-run in Task 2)

Probe `<scratchpad>/probe/gran2.c` (recipe in Task 2): default device, a 5 s all-zero
`ma_audio_buffer` → `ma_sound_init_from_data_source`, volume 0, cursor polled every 1 ms for 3 s.
`ma_engine_config.onProcess` (`miniaudio.h:11200`) records `(cursor, CLOCK_MONOTONIC)` once per
engine update. Residuals are measured against a least-squares line fitted to the cursor (the ideal
steady clock), excluding the first and last 0.3 s, with the mean bias removed (calibration absorbs
it).

| Requested `periodSizeInFrames` | Device reported | Cursor step min / mean / max (frames) | Raw cursor residual rms / max | Callback-anchored estimate residual rms / max | Estimate's mean lead |
|---|---|---|---|---|---|
| 0 (default) run 1 | 900 × 4 | 388 / 877.5 / 900 (18.3 ms) | 6.21 / 14.44 ms | 2.90 / 18.79 ms | +9.95 ms |
| 0 (default) run 2 | 900 × 4 | 388 / 888.8 / 900 (18.5 ms) | 6.24 / 14.55 ms | 2.87 / 6.15 ms | +9.93 ms |
| 480 (10 ms) | 1440 × 1 | 480 / 480.0 / 480 (10.0 ms) | 3.28 / 7.40 ms | 1.46 / 2.74 ms | +5.13 ms |
| 240 (5 ms) | 720 × 1 | 240 / 255.8 / 480 (5.3 ms) | 2.11 / 5.11 ms | 1.45 / 2.75 ms | +2.69 ms |
| 128 (2.7 ms) | 384 × 1 | 128 / 250.9 / 512 (5.2 ms) | 1.53 / 2.88 ms | 2.64 / 2.83 ms (before bias removal) | — |

Readings, all preliminary and from **one machine and one sink** (speaker):

- The default step reproduces #58 (mean 876–893, max 900).
- `ma_engine_config.periodSizeInFrames` really does shrink the step. It is passed to the device
  config at `miniaudio.h:74988-74989`. At 480 frames, steps were exactly 480.
- Below about 256 frames the step stops shrinking. The PipeWire graph quantum (256 @ 48 kHz, from
  #58) becomes the floor, and callbacks start to arrive in bursts (1133 callbacks but 576 distinct
  cursor values at 128).
- The callback-anchored estimate roughly halves the rms error, but at the default period it showed a
  **single 18.8 ms outlier**. The callback runs when PipeWire schedules it, not when its audio is
  consumed, so the anchor timestamp has its own jitter. Any interpolation would need clamping or
  smoothing (see approach C risks).
- The 128-frame row was taken before the bias-removal fix, so its estimate figures
  are not comparable. Task 2 re-runs everything.

---

## Upstream Reference Findings

This is the content for the new "How StepMania and OpenITG get the music position" section of
`docs/AUDIO_LATENCY.md` (AC 2). Every line was read while planning. The implementer re-verifies each
citation with `sed -n` (Task 3) before writing it into the doc.

### U1 — OpenITG (`f2c129fe`)

| Step | Citation | What it does |
|------|----------|--------------|
| Game loop reads the music position once per frame **with a timestamp** | `src/GameSoundManager.cpp:481-483` | `GetPositionSeconds(&m_bApproximate, &tm)` returns the position and a `RageTimer tm` |
| Position and timestamp taken together | `src/RageSound.cpp:742-756` | `pTimestamp->Touch()` inside a time-critical section, then `GetPositionSecondsInternal` |
| Position = **hardware frame now**, mapped to the source frame | `src/RageSound.cpp:688-731` | `SOUNDMAN->GetPosition(this)` (`src/RageSoundManager.cpp:92-96` → driver), clamped to be monotonic (`:712-728`), then `m_PositionMapping.Search` |
| Hardware→source map | `src/RageSound.cpp:512-516, 520-548`; `src/RageSoundPosMap.cpp:55-124` | The mixer records `(driver frameno, source position, frames)` blocks as it writes. `Search` maps a hardware frame **exactly inside a written block** (`:66-73`), otherwise to the nearest block edge and flags it approximate |
| ALSA driver position | `src/arch/Sound/ALSA9Helpers.cpp:392-405` | `last_cursor_pos - snd_pcm_delay()` after `snd_pcm_hwsync`: frames written minus frames still queued, i.e. **the frame at the DAC now**, sample-granular, read at query time |
| DirectSound driver position | `src/arch/Sound/DSoundHelpers.cpp:599-627` | Hardware play cursor from `GetCurrentPosition`, unwrapped against the write cursor, clamped monotonic |
| CoreAudio driver position | `src/arch/Sound/RageSoundDriver_CA.cpp:158-164` | `GetCurrentTime(time)` → `time.mSampleTime`, the device's own sample clock now |
| Frame-pacing correction | `src/GameSoundManager.cpp:395-428`, applied at `:523` | With steady vsync, subtract the extra scheduling delay from **both** the position and its timestamp (`fSeconds + fAdjust`, `tm + fAdjust`), "this won't adversely affect input timing" (`:408`) |
| Position stored with its timestamp | `src/GameState.cpp:795-804` | `m_LastBeatUpdate = timestamp; m_fMusicSeconds = fPositionSeconds` |
| Input aging (wall-clock extrapolation) | `src/Player.cpp:918-926` | `fCurrentMusicSeconds = m_fMusicSeconds + m_LastBeatUpdate.Ago() * rate`, then `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep * rate`. So OpenITG **already extrapolates the music time forward with the wall clock** from the last position read to "now" when judging |

**Key point:** OpenITG has no "between callbacks" problem, because its position is read from the
hardware at the moment of the query, not from the mixer's write cursor. Its timestamp is taken in the
same call, so `(position, timestamp)` is a consistent pair. Blaze's `(cursor, input_reference_ns_)`
pair is not: the cursor is stale by U(0, step) and the timestamp is "now".

### U2 — StepMania 5.0.12 (`45e0787a`)

| Step | Citation | What it does |
|------|----------|--------------|
| Same game-loop structure | `src/GameSoundManager.cpp:454-487` (frame adjustment), `:568-570` (read with `tm`), `:606` (`UpdateSongPosition(fSeconds + fAdjust, …, tm + fAdjust)`) | Unchanged from OpenITG |
| Position read with retry on preemption | `src/arch/Sound/RageSoundDriver_Generic_Software.cpp:490-523` | `pTimestamp->Touch(); GetPosition();` retried up to 3× while the pair took > 2 ms (`:504-510`), so position and timestamp stay consistent |
| Manager → driver | `src/RageSoundManager.cpp:95-100`; `src/RageSound.cpp:497-525` | Hardware frame → stream → source frame |
| ALSA | `src/arch/Sound/ALSA9Helpers.cpp:386-399` | Same `snd_pcm_delay` method as OpenITG |
| DirectSound | `src/arch/Sound/DSoundHelpers.cpp:552-590` | Hardware play cursor |
| WaveOut | `src/arch/Sound/RageSoundDriver_WaveOut.cpp:91-100` | `waveOutGetPosition(TIME_SAMPLES)` |
| WDM-KS | `src/arch/Sound/RageSoundDriver_WDMKS.cpp:1252-1263` | `KSPROPERTY_AUDIO_POSITION` play offset |
| **CoreAudio AudioUnit: host-clock position** | `src/arch/Sound/RageSoundDriver_AU.cpp:162, 214-217, 322-325` | `m_TimeScale = sampleRate / hostClockFrequency`. `GetPosition()` returns `m_TimeScale * AudioGetCurrentHostTime()`, and the render callback anchors each buffer at `m_TimeScale * inTimeStamp->mHostTime`. **The position between callbacks is the monotonic host clock**, anchored to the audio timestamps of each callback |
| **PulseAudio: write position, per callback** | `src/arch/Sound/RageSoundDriver_PulseAudio.cpp:297-300, 307-324` | `GetPosition()` returns `m_LastPosition`, which `StreamWriteCb` advances once per write callback. **This is the same behavior Blaze has now.** The comment at `:302-306` says "Something here is slow and causes arrows to stutter in gameplay" (added in commit `464f0f703bd2`, 2011, "PulseAudio has some issue which causes arrows to stutter.") |
| Input aging | `src/Player.cpp:1976-1978, 2140-2145` | Same `m_LastBeatUpdate.Ago()` forward extrapolation as OpenITG |

### U3 — StepMania 5_1-new (`825467bc`)

| Step | Citation | What it does |
|------|----------|--------------|
| **PulseAudio: interpolated stream time** | `src/arch/Sound/RageSoundDriver_PulseAudio.cpp:234-238, 309-333, 335-337` | The stream is opened with `PA_STREAM_INTERPOLATE_TIMING \| PA_STREAM_NOT_MONOTONIC \| PA_STREAM_AUTO_TIMING_UPDATE`, and `GetPosition()` is `pa_stream_get_time()` converted to frames. With `INTERPOLATE_TIMING`, libpulse **extrapolates the playback time from the system clock** between server timing updates. The most recent commit touching the file is `6dcb310c21b8` (2026-07-27, "Engine patches/bugfixes from ITGmania (2021-2024)") |
| Timestamp pairing changed | `src/arch/Sound/RageSoundDriver_Generic_Software.cpp:511-549` | The retry loop no longer `Touch()`es `pTimestamp`. **Note this as an upstream change, not as reference behavior**; it is a recent refactor and the 5.0.12 version is the established one |

### U4 — What this means for Blaze (for the doc)

- Reference engines read **where the hardware is now**, not where the mixer has written up to. Their
  granularity is the driver's position granularity (sample-accurate on ALSA `snd_pcm_delay`, the host
  clock on CoreAudio), not the callback size.
- On two backends, the reference "music position" between hardware updates is **host-clock
  extrapolation anchored to audio timestamps**: SM5 CoreAudio (U2) and SM5 5_1-new PulseAudio via
  libpulse (U3). The SM 5.0.12 PulseAudio driver, which did not do this, is the one that was reported
  as stuttering.
- Both engines already use wall-clock aging in the judgment path (`Player.cpp`), and Blaze copied
  that in `music_time_for_event`. What differs is that the reference engines pair the position with a
  timestamp taken **when the position was read**. Blaze pairs a callback-stale cursor with a
  timestamp from a different moment.
- Note for the doc: these are the reference implementations' *mechanisms*. Whether Blaze adopts any
  of them is the owner decision below.

### U5 — What miniaudio 0.11.21 offers for each approach

| Need | API / line | Note |
|------|-----------|------|
| Smaller device period | `ma_engine_config.periodSizeInFrames` / `periodSizeInMilliseconds` (`miniaudio.h:11190-11191`), forwarded to the device at `:74988-74989` | Measured to work (Preliminary Measurements). `performanceProfile` is **not** in `ma_engine_config`; it needs a caller-owned `ma_device` (`pDevice`, `:11181`; device config `:7044`) |
| PulseAudio default period | `miniaudio.h:30252-30276` | Low-latency default is **25 ms** on PulseAudio, because "buffers of < ~20ms result glitches when running through PipeWire" (`:30254-30258`). The server negotiated 900 frames here |
| Generic defaults | `MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_LOW_LATENCY 10` / `CONSERVATIVE 100` (`:12099-12105`); WASAPI uses them at `:22241-22244` | Windows WASAPI is expected around 10 ms (estimate, not measured) |
| Per-update hook on the audio thread | `ma_engine_config.onProcess` (`:11172, 11200`) | "Fired at the end of each call to `ma_engine_read_pcm_frames()`… from the audio thread". Where an anchor `(frames, monotonic ns)` could be captured |
| Hardware position query | — | **Not available.** miniaudio does not load `pa_stream_get_time` and opens Pulse streams with `START_CORKED \| ADJUST_LATENCY` only (`:30520`), no `INTERPOLATE_TIMING`/`AUTO_TIMING_UPDATE` (flag defines exist at `:28751-28753`, unused). Already recorded in `docs/AUDIO_LATENCY.md` ("What miniaudio 0.11.21 exposes") |
| Fixed-size callbacks | `ma_device_config.noFixedSizedCallback` (`:7048`) | Default is fixed size, so the engine sees period-sized chunks |

---

## Candidate Approaches (to evaluate; **no decision is made in this plan**)

Principle 1 (`AGENTS.md`): "gameplay is driven by the music clock (audio stream position), never
wall-clock or frame delta". Design pattern 1: "No frame-timing logic in the judgment path."

| ID | Approach | What changes | Principle 1 | Expected effect (from preliminary data) | Costs / risks |
|----|----------|--------------|-------------|-----------------------------------------|---------------|
| **A** | Status quo, document only | Nothing | Fully compliant | Jitter stays about ±9 ms (rms ≈ 6 ms), visual judder at about 53 Hz | No work. Fantastic window ±21.5 ms keeps losing ≈ a third of its width to clock noise |
| **B** | Smaller device period | `ma_engine_config.periodSizeInFrames` (for example 480 = 10 ms, or 256 ≈ 5.3 ms), possibly a config key | **Compliant.** Still a pure audio-position clock | 480: step 10 ms, rms 3.3 ms. 256 (graph quantum floor): rms about 2.1 ms | Underrun/crackle risk (miniaudio's own PipeWire warning, `miniaudio.h:30254-30258`), more wake-ups and CPU, Bluetooth sinks may force a larger quantum anyway. **Shifts output latency**, so existing saved offsets move by several ms and users should recalibrate. Needs a full-song underrun check on wired and Bluetooth |
| **C** | Callback-anchored interpolation | Capture `(cursor, monotonic ns)` in `onProcess` (atomics), then estimate `cursor + (now − t_cb) × rate`, clamped to `[cursor, cursor + one period]` and never decreasing | **Touches principle 1.** The monotonic clock fills in between audio updates. Bounded by one period and re-anchored to audio every callback, so it cannot drift. Same idea as SM5 CoreAudio (U2) and libpulse `INTERPOLATE_TIMING` (U3) | Default period: rms 6.2 → 2.9 ms. Smooth arrows | **Owner sign-off required.** Anchor jitter (one 18.8 ms outlier measured) needs clamping or smoothing. Thread-safety (audio-thread atomics). Must keep the existing monotonic guard (`sound_stream.cpp:146-148`). The anchor has to come from the same timebase as SDL event timestamps (`SDL_GetTicksNS`) |
| **D** | Judgment-only consistent pairing | Leave rendering on the raw cursor. Age input events against the **callback anchor** `(cursor_cb, t_cb)` instead of `(cursor_now, input_reference_ns_)`, extrapolating forward when an event is newer than the anchor | **Touches principle 1, more narrowly.** It extends the wall-clock aging that already exists in `music_time_for_event` (and OpenITG `Player.cpp:918-926`) to a consistent reference pair. Arrows are untouched | Judgment jitter close to C. No change in visual judder | Owner sign-off required (forward extrapolation past the last audio update). Same anchor-jitter risk as C. Two clocks (render vs judgment) that can disagree by up to one period |
| **E** | Query the backend's playback position | Per-backend native code: `pa_stream_get_time` + `INTERPOLATE_TIMING`, WASAPI `IAudioClock::GetPosition`, CoreAudio timestamps. This is what the reference engines' drivers do (U1, U2, U3) | Compliant in spirit: it *is* the audio position. But libpulse's interpolation is itself system-clock extrapolation | Best fidelity, and it would also include the client buffer in the clock | Not exposed by miniaudio 0.11.21 (U5). Needs a miniaudio patch or our own libpulse/WASAPI/CoreAudio code, which breaks the thin-wrapper and lean-scope principles. Same reasons option B of #58 was rejected for v1 |
| B + C / B + D | Combine a smaller period with interpolation | Both | As C/D | Smallest jitter | Sum of both risk sets |

Decision criteria to present to the owner: residual jitter vs. the ±21.5 ms Fantastic window,
principle-1 impact, cross-platform behavior, underrun risk, recalibration impact and code size.

---

## Patterns to Follow

### Docs style
`docs/AUDIO_LATENCY.md` (current file): H1 title, `##` sections, tables with `| Item | Value | Source |`,
code references in backticks as `path:line`, and **measured** / **estimate** labels on every number
(`docs/AUDIO_LATENCY.md:9-10`). Keep that labeling: every new number is marked **measured** (with
device, backend and date) or **estimate**.

### Citation style
```
// SOURCE: .agents/plans/completed/031-verify-judgment-timing-windows-plan.md:80
OpenITG commit: `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (github.com/openitg/openitg).
```
Upstream citations are written as `OpenITG src/RageSound.cpp:688-731 (@f2c129fe)`, and likewise
`SM 5.0.12` and `SM 5_1-new @825467bc`.

### Existing comment that already names the upstream
```cpp
// SOURCE: src/gameplay/judgment_input.hpp:7-10
// Reconstructs the music-clock time at which an SDL-timestamped input occurred,
// given the music time and SDL nanosecond reference sampled together. Mirrors
// OpenITG's `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep`
// (src/Player.cpp:908-919) but is a pure function: it reads no clock.
```
Note for the doc (not a code change): the citation there says `908-919`, while the computation is at
`Player.cpp:918-926` at `f2c129fe`. Record this as a doc finding. Do **not** edit the source
comment in this spike unless the owner asks.

### Issue comment style (decision request)
Mirror the #58 summary-comment pattern (`.agents/plans/completed/032-audio-output-latency-spike-plan.md`
Task 8): a short summary, a table of options, a link to the doc on the branch, and an explicit question.

### Tests
No new tests and no code changes. Validation is build + full suite (41/41) + citation verification.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `docs/AUDIO_LATENCY.md` | UPDATE | Rewrite "Clock granularity" (`:137-154`) with measurements, U1–U5 citations and the A–E evaluation. Add the measurement rows to "Measurements" (`:192-206`). Replace the null-sink advice (`:238-240`) with the volume-0 rule. Update the "Decision" (`:180-190`) and "Follow-up issues" (`:242-246`) bullets for #71 |
| GitHub #71 | COMMENT | Decision request listing options A–E |
| GitHub (new issue) | CREATE, **only after owner approval** | Follow-up implementation issue for the approved approach (AC 4) |

Not touched: `src/**`, `tests/**`, `CMakeLists.txt`, `.agents/stories/*`, `TODO.md`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Branch

- **Action**: `git switch -c feature/036-music-clock-granularity-spike`
- **Validate**: `git branch --show-current`

### Task 2: Measure the callback step (agent: current default sink only)

- **File**: `<scratchpad>/probe/gran2.c` (scratch, **never committed**)
- **Implement**: a C probe with `#define MINIAUDIO_IMPLEMENTATION` that
  1. takes an optional `periodSizeInFrames` argument and sets `ma_engine_config.periodSizeInFrames`
     and `onProcess`;
  2. prints backend, device name, rate and `internalPeriodSizeInFrames × internalPeriods`;
  3. plays a 5 s all-zero `ma_audio_buffer` (f32, 2 ch, engine rate) through
     `ma_sound_init_from_data_source` with `ma_sound_set_volume(…, 0)`;
  4. polls `ma_sound_get_cursor_in_pcm_frames` every 1 ms for 3 s, records `CLOCK_MONOTONIC` per
     poll, and in `onProcess` records `(cursor, CLOCK_MONOTONIC)`;
  5. prints step min/mean/max, the number of callbacks, and the bias-removed rms/max residual of
     (a) the raw cursor and (b) the callback-anchored estimate `max(cursor, cb_cursor + (now − cb_t) × rate)`
     against a least-squares line (skip the first/last 0.3 s).
- Build: `gcc -O1 -I build/_deps/miniaudio-src gran2.c -o gran2 -lm -lpthread -ldl`
- Run **3×** each at `0`, `480`, `256`, and record `pactl get-default-sink` and the date.
- Obey every rule in [Audio safety rules](#audio-safety-rules-binding-for-every-probe).
- **Validate**: the default-period mean step lands in about 870–900 frames, matching #58. If not,
  record the new value and investigate before writing the doc.

### Task 3: Verify every upstream citation

- **Action**: re-clone the three upstream trees at the pinned commits (Environment Findings). Run
  `sed -n 'A,Bp'` on every `file:line` in U1–U5 and confirm that the quoted behavior is at that range.
  Fix any off-by-a-few line numbers in the doc text (not here).
- **Validate**: every row in U1–U5 checked, with a note of any corrections.

### Task 4: Update `docs/AUDIO_LATENCY.md`

- **File**: `docs/AUDIO_LATENCY.md`
- **Action**: UPDATE
- **Implement**:
  1. **Clock granularity (`:137-154`)**: replace "This document does **not** claim how StepMania does
     it" with:
     - the measured step table from Task 2 (marked **measured**, with device, backend and date);
     - why the error happens: inconsistent `(cursor, reference_ns)` pairing, citing
       `src/app/app.cpp:152`, `src/gameplay/gameplay_view.cpp:137,156` and
       `src/gameplay/judgment_input.hpp:11-19`;
     - a new subsection **"How StepMania and OpenITG get the music position"** with U1–U4 as tables,
       every row carrying a pinned commit and `file:line`;
     - the miniaudio options (U5);
     - a new subsection **"Approaches evaluated against principle 1"** with the A–E table, the
       decision criteria, and a line `**Owner decision: pending** (requested on #71, <date>)`.
       Do **not** label any option as chosen or recommended-as-decided. Factual notes such as "B is
       the only option that needs no sign-off" are fine.
  2. **Measurements (`:192-206`)**: add the Task 2 rows (step per requested period, residuals).
  3. **Owner procedure**: add a subsection "Owner procedure: measuring the callback step" covering:
     - wired 3.5 mm: plug in (so the UCM headphone sink becomes default **by the owner's own
       action**), run `gran2 0` three times, and note `pactl get-default-sink`;
     - Bluetooth: connect the WH-1000XM4 as default, run `gran2 0` and `gran2 480` three times each.
       Watch for audible glitches in other apps during `480` and note the codec if known;
     - Windows / macOS: run `ctest --preset <os>-release -R audio_test -V` and copy the
       `[AudioEngine] Output device: … period N frames` line. Label it as a **proxy** (period ≈ upper
       bound of the step with fixed-size callbacks, `miniaudio.h:7048`). Optionally port `gran2.c` (it
       only needs a monotonic clock).
  4. **Reproducing the probes (`:224-240`)**: add the `gran2` recipe and **replace** the
     null-sink paragraph (`:238-240`) with: "Probes only play an all-zero buffer at volume 0. Do not
     mute or reroute sinks or change the default device."
  5. **Decision (`:180-190`) / Follow-up issues (`:242-246`)**: change the #71 bullet to point to the
     new section and say the decision is pending.
  6. Optional doc finding: `src/gameplay/judgment_input.hpp:10` cites `Player.cpp:908-919`, and the
     computation is at `:918-926` @ `f2c129fe`. Mention it, but do not change code.
- **Mirror**: `docs/AUDIO_LATENCY.md:38-50` (table style), `:192-206` (measurement table)
- **Validate**: every `src/…:N` reference opens to the cited code (`sed -n`). Every number is labeled
  **measured** or **estimate**. `grep -n "null-sink\|PULSE_SINK" docs/AUDIO_LATENCY.md` returns
  nothing.

### Task 5: Build and full suite (no code changed, so nothing should change)

- **Validate**:
  ```bash
  cmake --build build -j
  SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy bwrap --dev-bind / / --tmpfs /run/user/$(id -u) \
    --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
  ```
  Expect `100% tests passed out of 41`. `git diff --stat main` shows only `docs/AUDIO_LATENCY.md`.

### Task 6: Request the owner decision on #71 (AC 3)

- **Action**: write `<scratchpad>/issue71-decision-request.md`, then
  `gh issue comment 71 --body-file <scratchpad>/issue71-decision-request.md`. Contents:
  - measured step (Task 2) and the jitter consequence (3 bullets);
  - the reference-engine finding in 3 bullets (U4), with links to the doc section on the branch;
  - the A–E table (one line each: change, principle-1 impact, expected rms, main risk);
  - the explicit question: **"Owner decision needed: which option (A/B/C/D/E or a combination), and
    for C/D, do you approve wall-clock interpolation between audio callbacks under principle 1?"**
- **Do not** record a decision on the owner's behalf, and do not close #71. The spike AC
  "Owner decision recorded" stays open until the owner answers on the issue.
- **Validate**: `gh issue view 71 --json comments --jq '.comments[-1].body' | head -5`

### Task 7: Follow-up issue (AC 4), conditional

- **If the owner has already answered on #71 and approved a change**: file one implementation issue
  with `gh issue create` (labels `audio`, `timing`), titled after the approved option, using the
  #58/#67 body style (`**Type** · **Priority** · **Complexity**`, Description, Acceptance Criteria,
  Technical Notes, Dependencies, `Related: #71`). Include the measured baseline from Task 2 as the
  acceptance bar (for example "raw residual rms ≤ X ms on the default sink"), a full-song underrun
  check for B, and a metronome-sync regression run (`tests/metronome_sync_test.cpp`) for any option.
  Then record the decision and link in the doc's Decision section.
- **If the owner chose A or has not answered yet**: file nothing. Note in the PR description that AC 3
  ("Owner decision recorded") and AC 4 are pending owner input, and that the PR should **not** use
  `Closes #71` (use `Refs #71`).
- **Validate**: `gh issue list --search "in:body #71" --state open` (if an issue was filed).

---

## Validation

```bash
# Build (no code changes expected)
cmake --build build -j

# Lint: none configured in this repo (no clang-format/clang-tidy target)

# Tests (sandboxed: audio_test touches real hardware otherwise)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy bwrap --dev-bind / / --tmpfs /run/user/$(id -u) \
  --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Scope check
git diff --stat main   # only docs/AUDIO_LATENCY.md
```

## End-to-End Verification

1. **Measurement reproducibility (agent):** `./gran2 0` run three times on the default sink gives a
   mean step within ±5 % of the Task 2 doc value and prints `backend=PulseAudio … internalPeriod=900`
   (or whatever the default sink negotiates, recorded with the date). No sound is heard (volume 0,
   zero buffer), and `pactl get-default-sink` is unchanged before and after.
2. **Citation check:** for each upstream row in the doc, `sed -n` on the pinned checkout shows the
   quoted call (`snd_pcm_delay`, `AudioGetCurrentHostTime`, `m_LastPosition`,
   `PA_STREAM_INTERPOLATE_TIMING`, `m_LastBeatUpdate.Ago()`).
3. **Doc coherence:** "Clock granularity" no longer says the doc makes no claim about StepMania. It
   contains the A–E table, and the line `Owner decision: pending` (or the recorded decision with a link
   to the owner's comment).
4. **Issue state:** #71 has the decision-request comment. A follow-up issue exists **only** if the
   owner approved a change.
5. **No behavior change:** 41/41 tests pass, and `git diff main -- src tests CMakeLists.txt` is empty.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Implementer drifts into writing interpolation code "to try it" in `src/` | Explicitly forbidden. Probes stay in `<scratchpad>`. Scope check `git diff main -- src tests` must be empty | In scope (enforce) |
| Probe changes the user's audio setup | Audio safety rules: volume 0 + zero buffer, no mute, null sink, reroute or default-device change. Existing doc advice replaced | In scope (enforce) |
| Small-period probe briefly lowers the PipeWire quantum for the whole graph, which may cause glitches in other apps for a few seconds | Short runs (3 s). Mention it in the owner procedure | In scope (document) |
| Upstream line numbers shift (5_1-new is a moving branch) | Cite commit hashes, never branch names. Prefer 5.0.12 and OpenITG as the reference, and 5_1-new only for the PulseAudio change | In scope |
| The callback-anchored estimate has outliers (18.8 ms seen once) because the callback wake-up time ≠ consumption time | Report max as well as rms. List clamping/smoothing as an open design point for C/D. Do not oversell C | In scope (document) |
| Measurements from a single machine/sink are over-generalized | Label every number with device/backend/date. Bluetooth, 3.5 mm, Windows and macOS are owner steps or "not measured" | In scope |
| Option B changes output latency, so existing calibrations go stale | Record it in the B row and in any follow-up issue's ACs (recalibrate prompt / doc note) | Out of scope to fix here (document only) |
| Owner has not answered when the PR is ready | PR uses `Refs #71`, the issue stays open, and AC 3/4 are listed as pending in the PR body | In scope |
| `judgment_input.hpp:10` cites stale OpenITG lines | Note it in the doc only. Code comment unchanged unless the owner asks | Out of scope (flag) |

---

## Open Questions

1. **OWNER DECISION (blocking AC 3/4): which approach, if any?**
   A (status quo), B (smaller period, compliant), C (callback-anchored interpolation, needs principle-1
   sign-off), D (judgment-only consistent pairing, needs sign-off), E (backend position query, native
   code), or B+C / B+D. This plan **does not assume an answer**. The implementer posts the question
   (Task 6) and files a follow-up only on approval (Task 7).
2. **Does principle 1 allow a monotonic clock *bounded by one audio period and re-anchored every
   callback*?** The reference engines effectively do this (SM5 CoreAudio host time; libpulse
   `INTERPOLATE_TIMING` in SM5 5_1-new; wall-clock aging in OpenITG `Player.cpp`). If the owner
   approves C or D, AGENTS.md principle 1 / pattern 1 wording should be amended in the follow-up to
   record the exception precisely. Proposed default: none until the owner says so.
3. **Windows/macOS measurements.** No host is available to the agent. Proposed default: record "not
   measured" plus the `audio_test` period-log proxy procedure, and leave the AC checkbox for those
   platforms to the owner ("if available").
4. **Commit the probe as a tool?** Proposed default: **no**. Keep `gran2.c` in the scratchpad and
   document the recipe in the doc (as #58 did). Committing it would add a raw miniaudio C target and
   interpolation-estimate code to the repo before the owner's decision.
5. **Should the stale `Player.cpp:908-919` citation in `judgment_input.hpp` be fixed now?** Proposed
   default: no (docs-only spike). Fold it into the follow-up issue.

---

## Acceptance Criteria

- [ ] Callback step measured on Linux (current default sink by the agent; wired 3.5 mm and Bluetooth
      via the documented owner procedure) and documented in `docs/AUDIO_LATENCY.md`. Windows/macOS
      measured by the owner or explicitly marked "not measured" with the proxy procedure
- [ ] OpenITG and StepMania position semantics documented with pinned-commit `file:line` citations
      (U1–U4), all re-verified
- [ ] Approaches A–E evaluated against AGENTS.md principle 1 in the doc, with no decision claimed
- [ ] Decision request posted on #71. Owner decision recorded **by the owner** (pending otherwise)
- [ ] Follow-up implementation issue filed **only if** a change is approved
- [ ] No `src/`, `tests/` or build changes. Build OK, 41/41 tests pass
- [ ] No system audio state changed by any probe
