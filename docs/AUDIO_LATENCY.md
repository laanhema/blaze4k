# Audio Output Latency: Bluetooth vs Wired vs OBS

Investigation record for issue #58 (`TODO.md:33`). It answers three questions:

1. Why does the best global offset change between Bluetooth and wired (3.5 mm) headphones?
2. Why are OBS recordings out of sync when play itself feels right?
3. What fix removes the need to recalibrate after switching output devices?

Numbers marked **measured** come from the Linux dev machine (see [Measurements](#measurements)).
Numbers marked **estimate** were not measured here and must not be treated as verified.

## Summary

- **Root cause.** The music clock is the miniaudio *mix cursor*
  (`ma_sound_get_cursor_in_pcm_frames`), plus one global offset. The cursor counts frames the engine
  has written into the audio callback buffer, not frames the player has heard. Everything after that
  point (client buffer, PipeWire graph, ALSA or Bluetooth sink, codec, radio, headset jitter buffer)
  is invisible to the clock. The calibration wizard folds all of it into the single
  `offset.global_offset_seconds`. The owner's wizard calibrations (**measured**) are −0.023 s on
  wired/speaker and about −0.222 s on WH-1000XM4 Bluetooth, so Bluetooth adds about 200 ms over
  wired. One number cannot cover both, so switching devices moves the optimum.
- **OBS.** OBS "Desktop Audio" taps the PipeWire sink monitor, which sits *before* the Bluetooth
  stages. With a Bluetooth-calibrated offset, the recorded video lags the recorded audio by about the
  Bluetooth latency. In game, the player hears the audio equally late, so it feels in sync.
  Workaround: OBS *Sync Offset* (see [below](#why-obs-recordings-are-out-of-sync)).
- **Decision.** Per-output-device saved offsets keyed by a stable backend device ID, falling back to
  the global offset (option A). Querying OS latency at runtime is rejected for v1 (option B).
- **Secondary finding (measured).** The cursor advances once per audio callback, about 18.75 ms at a
  time here. That adds up to about ±9 ms of judgment jitter and a small per-device bias. Spike #71
  measured it, compared it with the reference engines, and the owner approved a smaller period plus
  callback-anchored interpolation (B + C, see [Clock granularity](#clock-granularity)).
- **Wizard headroom (code finding).** The Bluetooth calibration is within 28 ms of the wizard's
  250 ms tap limit. A slower Bluetooth path saves a wrong, positive offset (see
  [Wizard headroom](#wizard-headroom)).
- **Correction to the #58 technical notes.** `src/audio/audio_engine.cpp` has **no** device
  enumeration or selection. `AudioEngine::init` calls `ma_engine_init` with
  `ma_engine_config_init()` defaults, so it always opens the OS default playback device. Follow-up
  #69 adds device identity.

## How the music clock works today

| Step | Code | Effect |
| ---- | ---- | ------ |
| Song audio is fully decoded on load | `src/audio/sound_stream.cpp:55-62` (`MA_SOUND_FLAG_DECODE`) | No streaming-decoder delay. The cursor is a read position in an in-memory PCM buffer |
| Clock source | `src/audio/sound_stream.cpp:138-154` → `ma_sound_get_cursor_in_pcm_frames` | Frames the engine has pulled into the current device callback. The cursor moves when miniaudio *writes* audio, not when it is *heard* |
| Gameplay binding | `src/gameplay/gameplay_view.cpp:111-124`, offset applied at `:70` | `SamplePosition{audio_.get_position_frames(), audio_.get_sample_rate()}` |
| Formula | `src/timing/music_clock.hpp:14-21`, `src/timing/music_clock.cpp:53-60` | `time = frames / rate + global_offset_seconds`. Positive offset = clock reads later |
| Offset storage | `src/data/config.hpp:37-39`, loader `src/data/config_loader.cpp:203-210`, writer `:393` | One `offset.global_offset_seconds`, clamped to ±3600 s |
| Calibration | `src/screens/calibration_screen.cpp:63` (clock offset forced to 0), `:98-121`; math `src/timing/offset_calibration.cpp:62-120`, sign `src/timing/offset_calibration.hpp:16-18` | The player taps to clicks played through the *same* output path. `offset = -mean(hit - beat)` (after outlier rejection), so it absorbs **output latency + input latency + player bias**. It does not include display latency, because the wizard is audio-only |
| Input aging | `src/gameplay/judgment_input.hpp:11-19`, reference taken at `src/app/app.cpp:152` | `hit = reference_music - (reference_ns - event_ns)`. `reference_music` is read once per frame from the stepped cursor |
| Engine init | `src/audio/audio_engine.cpp:19-52` | `ma_engine_config_init()` defaults: OS default playback device, low-latency performance profile. No device ID, no notification callback. Since #58 it logs the device name, backend and client buffer (diagnostic only) |

The diagnostic line looks like this (measured, dev machine):

```
[AudioEngine] Output device: 'Ryzen HD Audio Controller Speaker' via PulseAudio, period 900 frames x 4 @ 48000 Hz (18.75 ms/period; excludes OS mixer and Bluetooth latency)
```

Read `period × periods` with care on PulseAudio: miniaudio reports `maxlength` split into periods
(`miniaudio.h:30630-30644`), and the actually queued target (`tlength`) is one period
(`miniaudio.h:30028-30035`). The 75 ms total is **not** the output latency.

With a default device, the logged name is resolved at call time from the server's *current*
default sink. It can differ from the sink the stream really plays on (for example after a
per-stream sink override or a stream-restore rule). This was observed while running the tests against
a null sink: the stream played on the null sink, but the log still named the speaker.

## Where the latency hides

Linux/PipeWire path on the dev machine:

```
decoded PCM ──cursor──▶ ma_engine mix ──▶ PA client buffer (tlength ≈ 18.75 ms, measured)
   ──▶ PipeWire graph (quantum 256 = 5.3 ms, measured)  ──▶ sink
        ├─ wired / speaker: ALSA period/buffer (a few ms) ──▶ DAC ──▶ ear        ≈ 25–40 ms total (estimate)
        └─ Bluetooth: bluez5 sink ──▶ codec encode (SBC/AAC/LDAC) ──▶ radio
                       ──▶ headset jitter buffer + decode ──▶ ear            ≈ 200 ms more than wired (measured)
OBS "Desktop Audio" taps the sink monitor here ─┘ (before the bluez5/headset stages)
```

- The wired total is an **estimate** built from the measured client buffer and graph quantum plus a
  typical ALSA buffer.
- The Bluetooth figure comes from the owner's wizard calibrations on the WH-1000XM4: about −0.222 s
  vs −0.023 s wired, so `L_bt − L_wired ≈ 200 ms` (**measured**, see [Measurements](#measurements)).
  That is inside the commonly reported 150–300 ms A2DP range. The codec in use was not recorded.
- The headset-side buffer is invisible to every OS API. Even a perfect OS latency query leaves a
  headset-specific error.
- Codec and profile changes alter the latency of the *same* device: A2DP vs HFP (when a mic is
  active), SBC vs AAC vs LDAC, and LDAC quality modes.

## Why the best offset changes per device

Let `L` be the output latency from cursor to ear and `L_in` the input latency plus player bias.
Calibration stores `offset ≈ -(L + L_in)` (derivation below). `L_in` stays about the same when the
player switches headphones, but `L` grows by `L_bt − L_wired` on Bluetooth. So the optimal offset
moves by that amount: about 200 ms on the owner's WH-1000XM4 (measured: −0.023 s wired vs about
−0.222 s Bluetooth). That is far wider than the ±21.5 ms Fantastic window, so a stale offset is obvious immediately.

A smaller second contributor is the callback size (see [Clock granularity](#clock-granularity)):
Bluetooth sinks often run larger buffers, which shifts the mean bias by a few ms.

### Wizard headroom

The wizard assigns each tap to the nearest beat (`src/screens/calibration_screen.cpp:111-112`). Beats
are 0.5 s apart, and taps more than 0.25 s from their beat are rejected
(`src/timing/offset_calibration.hpp:25`). So it can only measure a total delay `L + L_in` below
250 ms. The WH-1000XM4 calibration (about 222 ms) is within 28 ms of that limit. A slower path, for
example another codec or the HFP profile, makes taps land nearer the *next* beat. Those taps are
accepted with a negative delta, and the wizard saves a **positive** offset of about `0.5 − (L + L_in)`
seconds, which is wrong. Not fixed in this spike.

## Why OBS recordings are out of sync

Sign convention: `src/timing/music_clock.hpp:19-21` (positive offset = clock reads later) and
`src/timing/offset_calibration.hpp:16-18` (`new_offset = -mean(hit - beat)`).

1. **Calibration.** The clock offset is 0 during the wizard. The click for beat `b` leaves the mixer
   at cursor `b` and is heard at `b + L`. The tap is timestamped at about `b + L + L_in`. The wizard
   stores `offset ≈ -(L + L_in)`.
2. **Gameplay.** The note at time `t` reaches the receptor when `clock = t`, that is when
   `cursor + offset = t`, so `cursor = t + L + L_in`. The player hears that note's audio at cursor
   time `t + L`. Arrow and sound line up *for the player*, who also needs `L_in` to react.
3. **Recording.** OBS captures audio from the sink monitor at about `cursor + ε` (client buffer and
   graph quantum, upstream of Bluetooth) and captures video at render time. In the file, the arrow
   reaches the receptor about `L + L_in − ε` **after** its sound.
   - Bluetooth: about `222 − 24 ≈ 198 ms` with the owner's WH-1000XM4 calibration (−0.222 s). Very
     visible.
   - Wired: about `L_wired + L_in − ε`. The saved −22 ms offset gives `L + L_in ≈ 22 ms`, and
     `ε ≈ 24 ms` on this machine, so the lag is around 0 ms and not noticeable.

**Workaround (no code change).** In OBS, open *Edit → Advanced Audio Properties* and set *Sync
Offset* on the Desktop Audio (or application audio capture) source to about `−offset_ms − ε`, where
`offset_ms` is the active `global_offset_seconds × 1000` and ε is roughly one client buffer plus the
graph quantum (about 24 ms here; see [Measurements](#measurements)). For example, the WH-1000XM4
calibration `-0.222` → about `222 − 24 ≈ 198 ms`. This delays the recorded audio to match the video.
Treat it as a starting point and trim by eye with a clap test. If the result is zero or negative, no Sync Offset is needed.
Alternatively, record while using wired output and its calibration.

## Clock granularity

Spike #71. The cursor (`src/audio/sound_stream.cpp:138-153` → `ma_sound_get_cursor_in_pcm_frames`)
steps once per audio callback, about 18.2–18.4 ms at a time with the default period on the dev
machine.

- **Judgment.** `reference_music` is up to one callback **behind** the true position, so aged hit
  times read early by `U(0, step)`. The mean bias is about `step / 2` (around 9 ms), which calibration
  absorbs. The jitter of up to ±9 ms around that mean (rms 6.2 ms, measured below) is **not**
  absorbed, and it is a large share of the ±21.5 ms Fantastic window.
- **Per device.** The callback size depends on device and backend, so this is a second, smaller
  reason the optimal offset shifts per device.
- **Visual.** Arrows advance in about 18 ms jumps (about 53 Hz), roughly one 60 Hz frame, which can
  show as judder at high speed mods.

### Measured callback step

**Measured** 2026-10-03, dev machine, default sink `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink`
("Ryzen HD Audio Controller Speaker"), miniaudio PulseAudio backend on pipewire-pulse 1.6.9,
48000 Hz. Silent probe `gran2` (recipe in [Reproducing the probes](#reproducing-the-probes-silent-scratch-only)),
three runs per row, 3 s each. `pactl get-default-sink` was the same before and after.

Residuals are measured against a least-squares line fitted to the cursor over the run (the ideal
steady clock), excluding the first and last 0.3 s, with the mean bias removed (calibration absorbs
it). They show how **uneven** the clock is, not how far it is from the DAC. The "anchored estimate"
is `max(cursor, cb_cursor + (now − cb_time) × rate)`, where `(cb_cursor, cb_time)` is captured in
`ma_engine_config.onProcess` at each engine update. It is a measurement aid in the scratch probe only.

| Requested `periodSizeInFrames` | Device reported | Cursor step min / mean / max (frames) | Engine updates in 3 s | Raw cursor residual rms / max | Anchored estimate residual rms / max | Estimate's mean lead over raw |
| --- | --- | --- | --- | --- | --- | --- |
| 0 (default) | 900 × 4 | 388 / 872.6–883.1 / 900 (18.18–18.40 ms) | 159–161 | 6.20 / 14.37–14.60 ms | 2.83–2.86 / 5.74–5.87 ms | +9.88–9.94 ms |
| 480 (10 ms) | 1440 × 1 | 480 / 480.0 / 480 (10.00 ms) | 302 | 3.26–3.27 / 7.46–7.64 ms | 1.44–1.46 / 2.80–2.91 ms | +5.13–5.19 ms |
| 256 (5.3 ms) | 768 × 1 | 256 / 256.0 / 256 (5.33 ms) | 567 | 1.54 / 2.81–3.18 ms | 0.08–0.13 / 0.26–0.57 ms | +2.63–2.66 ms |
| 128 (2.7 ms), one planning run | 384 × 1 | 128 / 250.9 / 512 (5.2 ms) | 1133 (576 distinct cursor values) | 1.53 / 2.88 ms | not comparable (taken before the bias-removal fix) | — |

Readings (all **measured** on this one machine and sink):

- The default step reproduces #58 (mean 876–893, max 900 there).
- `ma_engine_config.periodSizeInFrames` really shrinks the step. It is forwarded to the device config
  at `miniaudio.h:74988-74989`. At 480 and 256 frames every step was exactly the requested size.
- Below 256 frames the step stops shrinking. The PipeWire graph quantum (256 @ 48 kHz, #58) is the
  floor, and callbacks arrive in bursts (1133 updates but 576 distinct cursor values at 128).
- The anchored estimate halves the rms error at 480 and the default period. At 256 (period = graph
  quantum) it is almost exact, which suggests the updates then run in lockstep with the graph
  (interpretation, not verified).
- The anchor has its own jitter: the callback runs when PipeWire schedules it, not when its audio is
  consumed. One of two default-period runs during planning showed a single **18.79 ms** estimate
  outlier. The three runs above stayed under 5.9 ms. Any interpolation needs outlier clamping or
  smoothing.
- The estimate leads the raw cursor by about half a step on average. Switching the clock to it moves
  the mean bias, so saved offsets shift by about that amount (**estimate**: ~10 ms at the default
  period, ~5 ms at 480).
- Wired 3.5 mm, Bluetooth, Windows and macOS: **not measured**. See
  [Owner procedure: measuring the callback step](#owner-procedure-measuring-the-callback-step).

### Why the error happens

The judgment path pairs two values from different moments:

| Value | Where | When it was true |
| --- | --- | --- |
| `input_reference_ns_ = SDL_GetTicksNS()` | `src/app/app.cpp:152` | After the frame's events are drained ("now") |
| `reference_music = clock_.time_seconds()` | `src/gameplay/gameplay_view.cpp:137` | At the last audio callback, so stale by `U(0, step)` |
| `music_time_for_event(event.timestamp_ns, reference_ns, reference_music)` | `src/gameplay/gameplay_view.cpp:156`, `src/gameplay/judgment_input.hpp:11-19` | Ages the event against the pair as if both were taken together |

The aging itself mirrors OpenITG (below). The difference is that OpenITG's pair is consistent.

Code finding: the comment at `src/gameplay/judgment_input.hpp:10` cites OpenITG
`src/Player.cpp:908-919`, but at `f2c129fe` the computation is at `:918-926`. Left unchanged in this
docs-only spike, and fixed in the follow-up issue #81.

### How StepMania and OpenITG get the music position

Pinned sources: OpenITG `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (github.com/openitg/openitg,
the same commit as `src/timing/judgment_constants.hpp`); StepMania 5.0.12 tag `v5.0.12` =
`45e0787a7457c1b9071522463aa902d59ae3a2ee`; StepMania `5_1-new` @
`825467bcd81c812b33ad684dc04dd151b2d5dec3` (2026-08-21; cite the commit, the branch moves). Every
row was re-read with `sed -n` on 2026-10-03.

**OpenITG @f2c129fe**

| Step | Citation | What it does |
| --- | --- | --- |
| Game loop reads the position once per frame **with a timestamp** | `src/GameSoundManager.cpp:481-483` | `GetPositionSeconds(&m_bApproximate, &tm)` returns the position and a `RageTimer tm` |
| Position and timestamp taken together | `src/RageSound.cpp:742-756` | `pTimestamp->Touch()` inside a time-critical section, then `GetPositionSecondsInternal` |
| Position = **hardware frame now**, mapped to the source frame | `src/RageSound.cpp:688-731` | `SOUNDMAN->GetPosition(this)` (`src/RageSoundManager.cpp:92-96` → driver), clamped monotonic (`:712-728`), then `m_PositionMapping.Search` |
| Hardware → source map | `src/RageSound.cpp:512-516, 520-548`; `src/RageSoundPosMap.cpp:55-124` | The mixer records `(driver frameno, source position, frames)` blocks as it writes. `Search` maps a hardware frame exactly inside a written block (`:66-73`), otherwise to the nearest block edge, flagged approximate |
| ALSA position | `src/arch/Sound/ALSA9Helpers.cpp:392-405` | `last_cursor_pos - snd_pcm_delay()` after `snd_pcm_hwsync`: frames written minus frames still queued, i.e. the frame at the DAC now, read at query time |
| DirectSound position | `src/arch/Sound/DSoundHelpers.cpp:599-627` | Hardware play cursor from `GetCurrentPosition`, unwrapped against the write cursor, clamped monotonic |
| CoreAudio position | `src/arch/Sound/RageSoundDriver_CA.cpp:158-164` | `GetCurrentTime(time)` → `time.mSampleTime`, the device's own sample clock now |
| Frame-pacing correction | `src/GameSoundManager.cpp:395-428`, applied at `:523` | With steady vsync, the extra scheduling delay is subtracted from **both** the position and its timestamp (`fSeconds + fAdjust`, `tm + fAdjust`): "this won't adversely affect input timing" (`:408`) |
| Position stored with its timestamp | `src/GameState.cpp:795-804` | `m_LastBeatUpdate = timestamp; m_fMusicSeconds = fPositionSeconds` |
| Input aging (wall-clock extrapolation) | `src/Player.cpp:918-926` | `fCurrentMusicSeconds = m_fMusicSeconds + m_LastBeatUpdate.Ago() * rate`, then `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep * rate`. OpenITG already extrapolates the music time forward with the wall clock from the last position read when judging |

OpenITG has no "between callbacks" problem: its position is read from the hardware at query time, not
from the mixer's write cursor, and its timestamp is taken in the same call.

**StepMania 5.0.12 @45e0787a**

| Step | Citation | What it does |
| --- | --- | --- |
| Same game-loop structure | `src/GameSoundManager.cpp:454-487` (frame adjustment), `:568-570` (read with `tm`), `:606` (`UpdateSongPosition(fSeconds + fAdjust, …, tm + fAdjust)`) | Unchanged from OpenITG |
| Position read with retry on preemption | `src/arch/Sound/RageSoundDriver_Generic_Software.cpp:490-523` | `pTimestamp->Touch(); GetPosition();` retried up to 3× while the pair took > 2 ms (`:504-510`), so position and timestamp stay consistent |
| Manager → driver → source frame | `src/RageSoundManager.cpp:95-100`; `src/RageSound.cpp:497-525` | Hardware frame → stream → source frame |
| ALSA | `src/arch/Sound/ALSA9Helpers.cpp:386-399` | Same `snd_pcm_delay` method as OpenITG |
| DirectSound | `src/arch/Sound/DSoundHelpers.cpp:552-590` | Hardware play cursor |
| WaveOut | `src/arch/Sound/RageSoundDriver_WaveOut.cpp:91-100` | `waveOutGetPosition(TIME_SAMPLES)` |
| WDM-KS | `src/arch/Sound/RageSoundDriver_WDMKS.cpp:1252-1263` | `KSPROPERTY_AUDIO_POSITION` play offset |
| **CoreAudio AudioUnit: host-clock position** | `src/arch/Sound/RageSoundDriver_AU.cpp:162, 214-217, 322-325` | `m_TimeScale = sampleRate / hostClockFrequency`. `GetPosition()` returns `m_TimeScale * AudioGetCurrentHostTime()`, and the render callback anchors each buffer at `m_TimeScale * inTimeStamp->mHostTime`. Between callbacks the position **is the monotonic host clock**, anchored to each callback's audio timestamp |
| **PulseAudio: write position, per callback** | `src/arch/Sound/RageSoundDriver_PulseAudio.cpp:297-300, 307-324` | `GetPosition()` returns `m_LastPosition`, which `StreamWriteCb` advances once per write callback. **This is what Blaze does today.** The comment at `:302-306` says "Something here is slow and causes arrows to stutter in gameplay" (commit `464f0f703bd2`, 2011, "PulseAudio has some issue which causes arrows to stutter.") |
| Input aging | `src/Player.cpp:1976-1978, 2140-2145` | Same `m_LastBeatUpdate.Ago()` forward extrapolation as OpenITG |

**StepMania 5_1-new @825467bc**

| Step | Citation | What it does |
| --- | --- | --- |
| **PulseAudio: interpolated stream time** | `src/arch/Sound/RageSoundDriver_PulseAudio.cpp:234-238, 309-333, 335-337` | The stream is opened with `PA_STREAM_INTERPOLATE_TIMING \| PA_STREAM_NOT_MONOTONIC \| PA_STREAM_AUTO_TIMING_UPDATE`, and `GetPosition()` is `pa_stream_get_time()` converted to frames. With `INTERPOLATE_TIMING`, libpulse extrapolates the playback time from the system clock between server timing updates. Last commit touching the file: `6dcb310c21b8` (2026-07-27, "Engine patches/bugfixes from ITGmania (2021-2024)") |
| Timestamp pairing changed | `src/arch/Sound/RageSoundDriver_Generic_Software.cpp:511-549` | The retry loop no longer `Touch()`es `pTimestamp`. A recent upstream change, not the established reference behavior (5.0.12 above is) |

**What this means for Blaze**

- The reference engines read **where the hardware is now**, not where the mixer has written up to.
  Their granularity is the driver's position granularity (sample-accurate on ALSA `snd_pcm_delay`,
  the host clock on CoreAudio), not the callback size.
- On two backends the reference position between hardware updates is **monotonic-clock
  extrapolation anchored to audio timestamps**: SM5 CoreAudio and SM 5_1-new PulseAudio (via
  libpulse). The SM 5.0.12 PulseAudio driver, which did not do this, is the one reported as
  stuttering.
- Both engines already use wall-clock aging in the judgment path (`Player.cpp`), and Blaze copied that
  in `music_time_for_event`. The difference is that they pair the position with a timestamp taken
  **when the position was read**. Blaze pairs a callback-stale cursor with a timestamp from a
  different moment.

### What miniaudio 0.11.21 offers for the clock

| Need | API / line | Note |
| --- | --- | --- |
| Smaller device period | `ma_engine_config.periodSizeInFrames` / `periodSizeInMilliseconds` (`miniaudio.h:11190-11191`), forwarded to the device at `:74988-74989` | Works (measured above). `performanceProfile` is **not** in `ma_engine_config`; it needs a caller-owned `ma_device` (`pDevice`, `:11181`; device config `:7044`) |
| PulseAudio default period | `miniaudio.h:30252-30276` | Low-latency default is **25 ms** on PulseAudio, because "buffers of < ~20ms result glitches when running through PipeWire" (`:30254-30258`). The server negotiated 900 frames here |
| Generic defaults | `MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_LOW_LATENCY 10` / `CONSERVATIVE 100` (`:12099-12105`); WASAPI uses them at `:22241-22244` | Windows WASAPI is expected around 10 ms (**estimate**, not measured) |
| Per-update hook on the audio thread | `ma_engine_config.onProcess` (`:11172, 11200`) | "Fired at the end of each call to `ma_engine_read_pcm_frames()`… from the audio thread". Where an anchor `(frames, monotonic ns)` can be captured |
| Hardware position query | — | **Not available.** miniaudio does not load `pa_stream_get_time` and opens Pulse streams with `START_CORKED \| ADJUST_LATENCY` only (`:30520`), no `INTERPOLATE_TIMING`/`AUTO_TIMING_UPDATE` (defines at `:28751-28753`, unused) |
| Fixed-size callbacks | `ma_device_config.noFixedSizedCallback` (`:7048`) | Default is fixed size, so the engine sees period-sized chunks |

### Approaches evaluated against principle 1

These letters are specific to #71 and unrelated to the #58 options in
[Solutions evaluated](#solutions-evaluated). Principle 1 (`AGENTS.md`, as worded before #71): "gameplay
is driven by the music clock (audio stream position), never wall-clock or frame delta". Design pattern
1: "No frame-timing logic in the judgment path." Expected effects come from the measurements above
(dev machine, speaker sink).

| ID | Approach | What changes | Principle 1 | Expected effect | Costs / risks |
| --- | --- | --- | --- | --- | --- |
| **A** | Status quo, document only | Nothing | Fully compliant | Raw rms 6.2 ms, max ±14.6 ms, visual judder at about 53 Hz | No work. The ±21.5 ms Fantastic window keeps losing a large share to clock noise |
| **B** | Smaller device period | `ma_engine_config.periodSizeInFrames` (e.g. 480 = 10 ms, or 256 ≈ 5.3 ms), with a config override | Compliant. Still a pure audio-position clock | 480: step 10 ms, raw rms 3.3 ms. 256 (graph quantum floor): raw rms 1.5 ms | Underrun/crackle risk (miniaudio's own PipeWire warning, `miniaudio.h:30254-30258`), more wake-ups and CPU, Bluetooth sinks may force a larger quantum anyway. **Shifts output latency**, so saved offsets move by several ms and users should recalibrate. Needs a full-song underrun check on wired and Bluetooth |
| **C** | Callback-anchored interpolation | Capture `(cursor, monotonic ns)` per audio update, then estimate `cursor + (now − t_anchor) × rate`, clamped to at most one period ahead and never decreasing | **Touches principle 1.** A monotonic clock fills the gap between audio updates. Bounded by one period and re-anchored at every update, so it cannot drift. Same idea as SM5 CoreAudio and libpulse `INTERPOLATE_TIMING` (above) | Default period: rms 6.2 → 2.9 ms; at 480: 3.3 → 1.5 ms. Smooth arrows | Anchor jitter (one 18.8 ms outlier measured) needs clamping or smoothing. Thread safety (audio-thread anchor). Keep the monotonic guard (`src/audio/sound_stream.cpp:146-148`). The anchor must use the SDL event timebase (`SDL_GetTicksNS`). Mean bias moves by about half a step, so recalibration is advised |
| **D** | Judgment-only consistent pairing | Rendering stays on the raw cursor. Input events are aged against the callback anchor `(cursor_cb, t_cb)` instead of `(cursor_now, input_reference_ns_)` | Touches principle 1, more narrowly: extends the existing wall-clock aging (`music_time_for_event`, OpenITG `Player.cpp:918-926`) to a consistent pair | Judgment jitter close to C. No change in visual judder | Same anchor-jitter risk as C. Two clocks (render vs judgment) that can disagree by up to one period |
| **E** | Query the backend's playback position | Per-backend native code: `pa_stream_get_time` + `INTERPOLATE_TIMING`, WASAPI `IAudioClock::GetPosition`, CoreAudio timestamps. What the reference drivers do | Compliant in spirit (it *is* the audio position), though libpulse's interpolation is itself system-clock extrapolation | Best fidelity, and the clock would also include the client buffer | Not exposed by miniaudio 0.11.21. Needs a miniaudio patch or our own libpulse/WASAPI/CoreAudio code, which breaks the thin-wrapper and lean-scope principles (same reasons #58 option B was rejected for v1) |
| B + C / B + D | Smaller period plus interpolation | Both | As C / D | Smallest jitter (480 + anchored estimate: rms 1.5 ms, max < 3 ms) | Sum of both risk sets |

Decision criteria: residual jitter vs. the ±21.5 ms Fantastic window, principle-1 impact,
cross-platform behavior, underrun risk, recalibration impact and code size.

**Owner decision (2026-10-03): B + C approved.**

- **B:** a smaller audio period via `ma_engine_config.periodSizeInFrames` (target about 480 frames /
  10 ms), with a safe default and a config override for systems that crackle or underrun.
- **C:** callback-anchored interpolation. Capture `(cursor, monotonic ns in the SDL_GetTicksNS
  timebase)` at each audio update. Between updates, estimate `cursor + elapsed × rate`, clamped to at
  most one period ahead, re-anchored at every update and never decreasing. Anchor outliers are
  smoothed or clamped.
- **Rationale (owner, paraphrased):** the owner wants absolute precision, butter-smooth arrow
  scrolling and audio perfectly in sync. The reference engines (SM5 CoreAudio host clock, SM5
  5_1-new PulseAudio via libpulse `INTERPOLATE_TIMING`) fill the gaps with a monotonic clock anchored
  to audio updates, and AGENTS.md principle 2 (faithful to the reference) supports doing the same.
  Principle 1 was amended in `AGENTS.md` to record this bounded exception.
- **Not chosen:** A and D. **E** is noted as a possible future upgrade and is not approved now.
- Implementation: follow-up issue #81. Decision comment on #71: <https://github.com/laanhema/blaze4k/issues/71#issuecomment-5968315939>.

## What miniaudio 0.11.21 exposes

Line numbers refer to `build/_deps/miniaudio-src/miniaudio.h` (`GIT_TAG 0.11.21`, `CMakeLists.txt`).

| API / field | Line | Usable for |
| ----------- | ---- | ---------- |
| `ma_engine_get_device(ma_engine*)` | 11240 | Reaching the `ma_device` behind the engine |
| `ma_device.playback.internalSampleRate / internalPeriodSizeInFrames / internalPeriods` | 7744-7755 | **Client buffer only.** On PulseAudio `periods × size` is `maxlength`, not the queued `tlength` |
| `ma_device_get_info` / `ma_device_get_name(dev, ma_device_type_playback, …)` | 9006, 9059 (impl 42232-42291) | Display name plus `ma_device_info.id`. With a default device it resolves the **current** default device at call time |
| `ma_device_id` union | 6985-7008 | Stable keys: `pulse[256]` (sink name, e.g. `bluez_output.<MAC>.1`), `wasapi[64]` (endpoint ID), `coreaudio[256]` (device UID), `alsa[256]` |
| `ma_engine_config.notificationCallback`, `ma_device_notification_type_rerouted` | 11184, 6717; docs 6748-6787 | Learning that the default device changed (PulseAudio via `pa_stream_set_moved_callback`, 7496 and `ma_device_on_rerouted__pulse` 30242-30250). **Never fired by DirectSound** |
| `ma_get_backend_name(ma_backend)` | — | Backend label for logs and key prefix |
| PulseAudio sink latency | `ma_pa_sink_info.latency` 29166 | **Not exposed.** `ma_context_get_device_info_sink_callback__pulse` (29882) copies only name and description. `pa_stream_get_latency` is never loaded, and playback streams are created without `AUTO_TIMING_UPDATE` (30520) |
| WASAPI `GetStreamLatency` | 20472, 20595 (vtable only) | Internal. It only reports the shared-engine buffer anyway, not Bluetooth |

## Solutions evaluated

| Option | How | Pros | Cons | Verdict |
| ------ | --- | ---- | ---- | ------- |
| **A. Per-output-device saved offsets** | Key = `backend:device-id` from `ma_device_get_info(…).id`, display name kept for UI. The wizard saves to the active device's entry. Gameplay resolves the offset at song start, falling back to `global_offset_seconds` | Captures *everything*, including the headset buffer and the player's input latency and bias, because it is measured the same way as today. Pure config/timing change. Keeps all compensation in the offset (AGENTS.md principle 1). Cross-platform via the miniaudio ID union | First use of a new device needs one calibration. Codec/profile changes on the same headset share a key. On non-UCM Linux, speakers and the 3.5 mm jack can be one sink (same key). Default-device resolution can race a Bluetooth connect/disconnect | **Chosen** |
| B. Query OS latency at runtime | PulseAudio `pa_stream_get_latency` (needs timing updates and our own libpulse loading or a miniaudio patch); WASAPI `IAudioClock`/`GetStreamLatency`; CoreAudio device latency + safety offset + stream latency | No per-device calibration. Could follow mid-song changes | Not in miniaudio 0.11.21's API, so it needs per-backend native code (breaks the thin platform wrapper and lean scope). Cannot see the headset buffer. Bluetooth reports are often inaccurate. Values fluctuate, so feeding them into the clock live adds jitter | **Rejected for v1.** Possible later as a seed for unseen devices (A+B hybrid) |
| C. Separate visual delay / "recording mode" | A second offset that shifts only the note field | Could make recordings line up | Recordings are a niche case and OBS already has Sync Offset. Adds a second timing knob outside the ITG-faithful scope | **Rejected.** OBS workaround documented instead |
| D. Assume the lowest-latency device | — | — | Does nothing for Bluetooth | **Rejected** |

## Decision

- **Option A**, split into two issues so each piece is small and testable:
  - #69 exposes the active output device identity (backend, ID, name, client buffer) and handles
    reroute notifications.
  - #70 adds per-device offsets to the config, resolves the offset at song start, saves calibrations
    per device (and also updates `global_offset_seconds`, so the latest calibration is the fallback
    for unseen devices) and shows the device in the options menu. Blocked by #69.
- `MusicClock` stays unchanged for #58. Compensation for output latency stays in the offset only.
- The OBS desync is handled by documentation (Sync Offset), not code.
- Clock granularity is a separate spike (#71). Owner decision 2026-10-03: B + C (smaller period plus
  callback-anchored interpolation), to be implemented in #81. See
  [Approaches evaluated against principle 1](#approaches-evaluated-against-principle-1).

## Measurements

Dev machine, Linux/Fedora, PipeWire 1.6.9 with pipewire-pulse, miniaudio picks the PulseAudio
backend.

| Item | Value | Source |
| ---- | ----- | ------ |
| Default sink | `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink` ("Ryzen HD Audio Controller Speaker"). UCM split sinks, so speaker and headphone jack are separate sinks | `pactl get-default-sink` (measured) |
| miniaudio device | PulseAudio, 48000 Hz, period 900 frames × 4 (18.75 ms/period) | `[AudioEngine] Output device` log line (measured) |
| PipeWire graph | driver quantum 256 @ 48 kHz (5.33 ms); the probe stream requests `node.latency = 300/48000` | `pw-top -b` (measured) |
| Cursor step | min 388, mean 876.5–892.5, max 900 frames (≈ 18.3–18.75 ms) | silent probe, two runs (measured, #58) |
| Cursor step, default period (#71) | min 388, mean 872.6–883.1, max 900 frames (18.18–18.40 ms); raw residual rms 6.20 ms, max 14.37–14.60 ms | `gran2 0`, three runs, 2026-10-03 (measured) |
| Cursor step, `periodSizeInFrames = 480` (#71) | device 1440 × 1; every step 480 frames (10.00 ms); raw residual rms 3.26–3.27 ms, max 7.46–7.64 ms | `gran2 480`, three runs, 2026-10-03 (measured) |
| Cursor step, `periodSizeInFrames = 256` (#71) | device 768 × 1; every step 256 frames (5.33 ms); raw residual rms 1.54 ms, max 2.81–3.18 ms | `gran2 256`, three runs, 2026-10-03 (measured) |
| Callback-anchored estimate (#71) | residual rms 2.83–2.86 ms (default), 1.44–1.46 ms (480), 0.08–0.13 ms (256); one 18.79 ms outlier in a planning run at the default period | `gran2`, same runs (measured; scratch probe only) |
| Callback step: wired 3.5 mm, Bluetooth, Windows, macOS | **not measured** | See [owner procedure](#owner-procedure-measuring-the-callback-step) |
| `pactl` latency fields | `Latency: 0 usec` | pipewire-pulse does not fill these; use `pw-top`/`pw-dump` |
| Saved offset | `global_offset_seconds = -0.02204` (−22 ms) in `build/data/config.json` | Wired/speaker calibration; the owner reports −0.023 s on both (measured) |
| Wired end-to-end latency | ≈ 25–40 ms | **Estimate** from client buffer + quantum + typical ALSA buffer |
| Bluetooth (WH-1000XM4) calibration | about −0.222 s (wizard), so `L_bt − L_wired ≈ 200 ms` | Owner (measured); codec not recorded |

### Owner procedure: measuring Bluetooth

The headset was paired but not connected during this spike, and no audio was routed to it. The owner
has since done step 3 (see the table above). Steps 1–2 would add the Bluetooth client buffer and
PipeWire-reported latency. To run them, with the headset connected **as the default sink**
(`pactl get-default-sink` shows a `bluez_output.` prefix):

1. Run `ctest --test-dir build -R audio_test -V | grep "Output device"` to get the Bluetooth client
   buffer. (`audio_test` plays a short test tone.)
2. While it runs (or while the game runs), capture the graph quantum and reported latency:
   `timeout 4 pw-top -b -n 3`, and
   `pw-dump | python3 -c 'import json,sys; [print(o["info"]["props"].get("node.name"), o["info"]["props"].get("node.latency"), o["info"].get("params",{}).get("Latency"), o["info"].get("params",{}).get("ProcessLatency")) for o in json.load(sys.stdin) if o.get("type")=="PipeWire:Interface:Node" and str(o.get("info",{}).get("props",{}).get("node.name","")).startswith("bluez_output.")]'`.
3. Run the calibration wizard on Bluetooth and on wired 3.5 mm, and note `global_offset_seconds` in
   `build/data/config.json` (dev build; otherwise `<executable dir>/data/`, the XDG data dir or
   `--data-dir`, see `src/data/data_paths.cpp:10-29`) after each. The difference is the measured `L_bt − L_wired`.

### Owner procedure: measuring the callback step

**Not measured** by the agent: the spike only measured the current default sink and must not change
the default device or routing. The owner runs these steps on their own setup. Build `gran2` first (see
[Reproducing the probes](#reproducing-the-probes-silent-scratch-only)). Each run is about 3 s and
silent (all-zero buffer at volume 0). A small requested period briefly lowers the PipeWire graph
quantum for every app while the probe runs, so other apps may glitch for those few seconds.

1. **Wired 3.5 mm (Linux).** Plug the headphones in. With UCM split sinks the headphone sink only
   exists while a plug is inserted. If it does not become the default automatically, select it
   yourself. Then:
   - `pactl get-default-sink` (note the name);
   - `./gran2 0` three times, then `./gran2 480` three times;
   - record the `internalPeriod` line, the step min/mean/max and both residual lines in
     [Measurements](#measurements).
2. **Bluetooth (Linux, WH-1000XM4).** Connect the headset and make it the default sink
   (`pactl get-default-sink` shows a `bluez_output.` prefix). Note the codec if known
   (`pactl list sinks | grep -E 'Name:|api.bluez5.codec'`). Run `./gran2 0` and `./gran2 480` three
   times each. Listen for glitches in other apps during the `480` runs, and note whether the
   reported device period stays at the requested size or is forced larger.
3. **Windows / macOS.** No host or toolchain is available to the agent. Build with the
   `windows-msvc-release` or `macos-clang-release` preset, then run
   `ctest --preset <os>-release -R audio_test -V` and copy the
   `[AudioEngine] Output device: … period N frames` line. This is a **proxy**: with fixed-size
   callbacks (`miniaudio.h:7048`) the period is about the upper bound of the cursor step. For the real
   step, port `gran2.c` (it only needs a monotonic clock in place of `clock_gettime`).

### Reproducing the probes (silent, scratch only)

Build with `gcc -O1 -I build/_deps/miniaudio-src probe.c -o probe -lm -lpthread -ldl`. Neither makes
any sound.

- **Device probe:** `#define MINIAUDIO_IMPLEMENTATION`, `ma_engine_init` with the default config,
  `ma_engine_get_device`, then print `ma_get_backend_name(d->pContext->backend)`,
  `ma_device_get_name(d, ma_device_type_playback, …)` and
  `d->playback.internalSampleRate/internalPeriodSizeInFrames/internalPeriods`. Run
  `pw-top -b -n 3` while the engine outputs silence.
- **Granularity probe:** a 5 s all-zero `ma_audio_buffer` (f32, 2 ch, engine rate) →
  `ma_sound_init_from_data_source`, volume 0, `ma_sound_start`, then poll
  `ma_sound_get_cursor_in_pcm_frames` every 1 ms for 2 s and print min, mean and max step.
- **`gran2` (callback step and residuals, #71):** `gran2 [periodSizeInFrames]`. Sets
  `ma_engine_config.periodSizeInFrames` (0 = default) and `onProcess`, prints backend, device name,
  rate and `internalPeriodSizeInFrames × internalPeriods`, then plays a 5 s all-zero
  `ma_audio_buffer` (f32, 2 ch, engine rate) via `ma_sound_init_from_data_source` with
  `ma_sound_set_volume(…, 0)`. It polls `ma_sound_get_cursor_in_pcm_frames` every 1 ms for 3 s with a
  `CLOCK_MONOTONIC` timestamp per poll, and `onProcess` records `(cursor, CLOCK_MONOTONIC)` per engine
  update. It prints step min/mean/max, the number of updates, and the bias-removed rms/max residual of
  (a) the raw cursor and (b) `max(cursor, cb_cursor + (now − cb_time) × rate)` against a
  least-squares line, skipping the first and last 0.3 s. The probe is not committed.

Probes only play an all-zero buffer at volume 0. Do not mute or reroute sinks or change the default
device.

## Follow-up issues

- #69 — Expose the active audio output device (backend, ID, name, buffer) and handle reroutes
- #70 — Per-output-device global offset (remember calibration per headphones/speakers), blocked by #69
- #71 — Investigate music-clock granularity (decoder cursor advances in ~19 ms audio-callback steps).
  Spike done; owner decision B + C (2026-10-03)
- #81 — Implement smaller audio period + callback-anchored music-clock interpolation (B + C from
  #71)
