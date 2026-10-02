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
  `offset.global_offset_seconds`. Wired output adds roughly 25–40 ms end to end (**estimate**,
  built from measured parts); A2DP Bluetooth typically adds 150–300 ms (**estimate**, not measured
  here). One number cannot cover both, so switching devices moves the optimum.
- **OBS.** OBS "Desktop Audio" taps the PipeWire sink monitor, which sits *before* the Bluetooth
  stages. With a Bluetooth-calibrated offset, the recorded video lags the recorded audio by about the
  Bluetooth latency. In game, the player hears the audio equally late, so it feels in sync.
  Workaround: OBS *Sync Offset* (see [below](#why-obs-recordings-are-out-of-sync)).
- **Decision.** Per-output-device saved offsets keyed by a stable backend device ID, falling back to
  the global offset (option A). Querying OS latency at runtime is rejected for v1 (option B).
- **Secondary finding (measured).** The cursor advances once per audio callback, about 18.75 ms at a
  time here. That adds up to about ±9 ms of judgment jitter and a small per-device bias.
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
default sink. It can differ from the sink the stream really plays on (for example after
`PULSE_SINK=` overrides or a stream-restore rule). This was observed while running the tests against
a null sink: the stream played on the null sink, but the log still named the speaker.

## Where the latency hides

Linux/PipeWire path on the dev machine:

```
decoded PCM ──cursor──▶ ma_engine mix ──▶ PA client buffer (tlength ≈ 18.75 ms, measured)
   ──▶ PipeWire graph (quantum 256 = 5.3 ms, measured)  ──▶ sink
        ├─ wired / speaker: ALSA period/buffer (a few ms) ──▶ DAC ──▶ ear        ≈ 25–40 ms total (estimate)
        └─ Bluetooth: bluez5 sink ──▶ codec encode (SBC/AAC/LDAC) ──▶ radio
                       ──▶ headset jitter buffer + decode ──▶ ear            ≈ 150–300 ms typical (estimate)
OBS "Desktop Audio" taps the sink monitor here ─┘ (before the bluez5/headset stages)
```

- The wired total is an **estimate** built from the measured client buffer and graph quantum plus a
  typical ALSA buffer.
- The Bluetooth range is the commonly reported A2DP figure. It is an **estimate, not measured on the
  dev machine** (see [Measurements](#measurements) for the owner procedure).
- The headset-side buffer is invisible to every OS API. Even a perfect OS latency query leaves a
  headset-specific error.
- Codec and profile changes alter the latency of the *same* device: A2DP vs HFP (when a mic is
  active), SBC vs AAC vs LDAC, and LDAC quality modes.

## Why the best offset changes per device

Let `L` be the output latency from cursor to ear and `L_in` the input latency plus player bias.
Calibration stores `offset ≈ -(L + L_in)` (derivation below). `L_in` stays about the same when the
player switches headphones, but `L` jumps from about 25–40 ms to about 150–300 ms (both estimates).
So the optimal offset moves by roughly `L_bt − L_wired`, which is on the order of 100–250 ms. That
is far wider than the ±21.5 ms Fantastic window, so a stale offset is obvious immediately.

A smaller second contributor is the callback size (see [Clock granularity](#clock-granularity)):
Bluetooth sinks often run larger buffers, which shifts the mean bias by a few ms.

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
   - Bluetooth: about 150–300 ms (estimate). Very visible.
   - Wired: about `L_wired + L_in − ε`. The saved −22 ms offset gives `L + L_in ≈ 22 ms`, and
     `ε ≈ 24 ms` on this machine, so the lag is around 0 ms and not noticeable.

**Workaround (no code change).** In OBS, open *Edit → Advanced Audio Properties* and set *Sync
Offset* on the Desktop Audio (or application audio capture) source to about `−offset_ms − ε`, where
`offset_ms` is the active `global_offset_seconds × 1000` and ε is roughly one client buffer plus the
graph quantum (about 24 ms here; see [Measurements](#measurements)). For example `-0.180` → about
`180 − 24 ≈ 156 ms`. This delays the recorded audio to match the video. Treat it as a starting point
and trim by eye with a clap test. If the result is zero or negative, no Sync Offset is needed.
Alternatively, record while using wired output and its calibration.

## Clock granularity

The cursor steps once per audio callback. Measured on the dev machine: min 388, mean 876–893, max
900 frames per step, about 18.3–18.75 ms at 48 kHz.

- **Judgment.** `reference_music` is up to one callback **behind** the true position, so aged hit
  times read early by `U(0, step)`. The mean bias is about `step / 2` (around 9 ms), which calibration
  absorbs. The jitter of up to ±9 ms around that mean is **not** absorbed, and it is a large share of
  the ±21.5 ms Fantastic window.
- **Per device.** The callback size depends on device and backend, so this is a second, smaller
  reason the optimal offset shifts per device.
- **Visual.** Arrows advance in about 18.75 ms jumps, roughly one 60 Hz frame, which can show as
  judder at high speed mods.

Fixing this means interpolating between callbacks or shrinking the period. Interpolation uses
wall-clock time between audio updates, which touches AGENTS.md principle 1 (music-driven clock), so
it needs an explicit owner decision and a reference check against StepMania/OpenITG's sound position
code. This document does **not** claim how StepMania does it. Tracked as spike #71.

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
- `MusicClock` stays unchanged. Compensation stays in the offset only.
- The OBS desync is handled by documentation (Sync Offset), not code.
- Clock granularity is a separate spike (#71) that needs owner sign-off.

## Measurements

Dev machine, Linux/Fedora, PipeWire 1.6.9 with pipewire-pulse, miniaudio picks the PulseAudio
backend.

| Item | Value | Source |
| ---- | ----- | ------ |
| Default sink | `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink` ("Ryzen HD Audio Controller Speaker"). UCM split sinks, so speaker and headphone jack are separate sinks | `pactl get-default-sink` (measured) |
| miniaudio device | PulseAudio, 48000 Hz, period 900 frames × 4 (18.75 ms/period) | `[AudioEngine] Output device` log line (measured) |
| PipeWire graph | driver quantum 256 @ 48 kHz (5.33 ms); the probe stream requests `node.latency = 300/48000` | `pw-top -b` (measured) |
| Cursor step | min 388, mean 876.5–892.5, max 900 frames (≈ 18.3–18.75 ms) | silent probe, two runs (measured) |
| `pactl` latency fields | `Latency: 0 usec` | pipewire-pulse does not fill these; use `pw-top`/`pw-dump` |
| Saved offset | `global_offset_seconds = -0.02204` (−22 ms) in `build/data/config.json` | The device it was calibrated on is **unknown** (owner to confirm: speaker or wired) |
| Wired end-to-end latency | ≈ 25–40 ms | **Estimate** from client buffer + quantum + typical ALSA buffer |
| Bluetooth (WH-1000XM4) | **Not measured on the dev machine.** The owner reports a noticeably larger optimal offset (`TODO.md:33`). Typical A2DP: 150–300 ms | **Estimate** |

### Owner procedure: measuring Bluetooth

The headset was paired but not connected during this spike, and no audio was routed to it. To fill in
real numbers, with the headset connected **as the default sink** (`pactl get-default-sink` shows a
`bluez_output.` prefix):

1. Run `ctest --test-dir build -R audio_test -V | grep "Output device"` to get the Bluetooth client
   buffer. (`audio_test` plays a short test tone.)
2. While it runs (or while the game runs), capture the graph quantum and reported latency:
   `timeout 4 pw-top -b -n 3`, and
   `pw-dump | python3 -c 'import json,sys; [print(o["info"]["props"].get("node.name"), o["info"]["props"].get("node.latency"), o["info"].get("params",{}).get("Latency"), o["info"].get("params",{}).get("ProcessLatency")) for o in json.load(sys.stdin) if o.get("type")=="PipeWire:Interface:Node" and str(o.get("info",{}).get("props",{}).get("node.name","")).startswith("bluez_output.")]'`.
3. Run the calibration wizard on Bluetooth and on wired 3.5 mm, and note `global_offset_seconds` in
   `build/data/config.json` (dev build; otherwise `<executable dir>/data/`, the XDG data dir or
   `--data-dir`, see `src/data/data_paths.cpp:10-29`) after each. The difference is the measured `L_bt − L_wired`.

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

To run anything that opens a real device without sound, route it to a temporary null sink:
`pactl load-module module-null-sink sink_name=silent` and run with `PULSE_SINK=silent`, then
`pactl unload-module <id>`.

## Follow-up issues

- #69 — Expose the active audio output device (backend, ID, name, buffer) and handle reroutes
- #70 — Per-output-device global offset (remember calibration per headphones/speakers), blocked by #69
- #71 — Investigate music-clock granularity (decoder cursor advances in ~19 ms audio-callback steps)
