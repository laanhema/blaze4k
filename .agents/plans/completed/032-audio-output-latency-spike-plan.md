# Plan: Investigate Audio Output Latency — Bluetooth vs Wired vs OBS (#58)

## Summary

Issue #58 is a **spike**. It asks why the best global offset differs between Bluetooth and wired
headphones, why OBS recordings are out of sync while play feels fine, and what fix would remove the
need to recalibrate after switching output devices. Most of the investigation was done while writing
this plan. The code reading and the silent miniaudio probes on the dev machine are recorded in the
**Findings** section below. Short version:

- **Root cause (confirmed in code):** the music clock is `ma_sound_get_cursor_in_pcm_frames()`
  (`src/audio/sound_stream.cpp:138-154`), plus one global offset (`src/timing/music_clock.cpp:58-61`).
  That cursor counts the frames the miniaudio engine has *mixed into the device callback buffer*. It
  does not include anything downstream: the miniaudio/PulseAudio client buffer, the PipeWire graph,
  the ALSA or bluez5 sink, the Bluetooth codec and radio, or the headphone's own jitter buffer. The
  calibration wizard (`src/screens/calibration_screen.cpp:98-121`) folds all of that into
  `global_offset_seconds = -mean(hit - beat)`. One number has to cover a latency that changes a lot
  per device (wired is about 25–40 ms end to end on this machine; A2DP Bluetooth is typically
  150–300 ms), so changing devices moves the optimum.
- **OBS (derived from the clock formula):** with a Bluetooth-calibrated offset `≈ -L_bt`, an arrow
  reaches the receptor when `cursor = note_time + L_bt`. OBS records "Desktop Audio" from the PipeWire
  sink monitor, which sits upstream of the bluez5 encoder and the headphones, at roughly cursor time.
  So in the recording the video lags the audio by about `L_bt`. In game the player hears the audio
  `L_bt` late too, so the arrows feel in sync. The recording has no such delay.
- **miniaudio 0.11.21 cannot report the real output latency.** Its public API only exposes the client
  buffer (`ma_device.playback.internalPeriodSizeInFrames`/`internalPeriods`, `miniaudio.h:7754-7755`).
  The PulseAudio backend *fetches* `pa_sink_info.latency` but throws it away
  (`miniaudio.h:29166`, callback at `ma_context_get_device_info_sink_callback__pulse`). It never loads
  `pa_stream_get_latency`. No OS API can see the headphone-side buffer anyway.
- **Secondary finding (measured):** the cursor advances in steps of one audio callback (mean 876, max
  900 frames, which is **about 18.3–18.75 ms at 48 kHz** here). So `reference_music` in
  `music_time_for_event` (`src/gameplay/judgment_input.hpp:11-19`) jitters by up to one callback. This
  adds a device-dependent mean bias of about half a callback (also folded into the offset) and up to
  ±9 ms of judgment jitter, which is large next to the ±21.5 ms Fantastic window.

**Recommended approach: per-output-device saved offsets**, keyed by a stable backend device ID and
falling back to the global offset. OS latency queries were evaluated and rejected for v1 (see
Solutions). Spike deliverables: `docs/AUDIO_LATENCY.md` (the investigation doc), a **single
diagnostic log line** in `AudioEngine::init` (justified by PRD:275, "input/audio latency measurable
via log output"), a one-line cross-link in `docs/CROSS_PLATFORM_VERIFICATION.md`, **three follow-up
issues** filed with `gh issue create`, and a summary comment on #58.

## User Story

As a player who switches between Bluetooth and wired headphones (and sometimes records with OBS)
I want to know why my offset changes and have a planned fix that remembers the offset per device
So that I don't have to recalibrate every time I change headphones, and I know how to line up my recordings

## Metadata

| Field | Value |
|-------|-------|
| Type | SPIKE (investigation doc + one diagnostic log line + follow-up issues) |
| Complexity | LOW (code) / MEDIUM (write-up) |
| Systems Affected | `docs/` (new `AUDIO_LATENCY.md`, cross-link in `CROSS_PLATFORM_VERIFICATION.md`), `src/audio/audio_engine.cpp` (one log statement), GitHub issues (3 new + 1 comment on #58) |
| GitHub Issue | #58 (`[TODO-5]`, labels `audio`, `timing`, `spike`) |
| Branch | `feature/032-audio-output-latency-spike` |
| miniaudio | 0.11.21 (`CMakeLists.txt:49-55`, `GIT_TAG 0.11.21`), header at `build/_deps/miniaudio-src/miniaudio.h` |

---

## Environment Findings

| Tool / Fact | Version / Value | Notes |
|-------------|-----------------|-------|
| CMake / GCC | 4.4.3 / 16.2.1 | `build/` already configured. `cmake --build build -j` is up to date at `2dbd609` |
| Baseline tests | **38/38 pass** | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build`, 0.52 s, `main` @ `2dbd609`. Note: `audio_test` initializes a real miniaudio device, because miniaudio ignores `SDL_AUDIO_DRIVER`. It plays only silence or test WAVs at test volume, as before |
| Audio stack | PipeWire 1.6.9 with pipewire-pulse (`pactl info`: "PulseAudio (on PipeWire 1.6.9)") | miniaudio picks the **PulseAudio** backend |
| Default sink | `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink` ("Ryzen HD Audio Controller Speaker") | UCM split sinks, so speakers and headphones are separate sinks on this machine |
| miniaudio device (probe) | `backend=PulseAudio`, `internalSampleRate=48000`, `internalPeriodSizeInFrames=900`, `internalPeriods=4` | `900×4 = 75 ms` is PA `maxlength`. The real queued client latency is about `tlength = 1 period = 18.75 ms` (`miniaudio.h:30028-30035, 30636-30644`) |
| PipeWire graph (`pw-top -b`) | driver quantum **256 @ 48 kHz (5.33 ms)**. The probe stream node requests `node.latency = 300/48000` | `clock.quantum` default is 1024. The graph runs at 256 because a client requests it |
| Cursor granularity (probe) | `ma_sound_get_cursor_in_pcm_frames` steps: min 388, **mean 876.5, max 900 frames (≈18.3 ms)** | Measured over 2 s with a silent `ma_sound` (volume 0) polled every 1 ms |
| `pactl` latency fields | `Latency: 0 usec`, `Buffer/Sink Latency: 0 usec` | pipewire-pulse does not fill these for `pactl list`. Use `pw-top`/`pw-dump` to read latency |
| Saved offset (`build/data/config.json`) | `global_offset_seconds = -0.02204` (−22 ms) | Probably a wired/speaker calibration. Provenance unknown (Open Question 1) |
| Bluetooth | `WH-1000XM4` (88:C9:E8:25:D5:B1) **paired, not connected** | The agent must **not** connect or route Bluetooth audio. Bluetooth measurements are an owner step (Task 4) |
| Off-limits | `.agents/stories/todo-stories.md` (untracked) | Do not modify, stage or revert it. Do **not** use `/create-story` or `/update-stories` (both edit or parse the story files). File follow-ups with `gh issue create`, as #67 was |

**Start green, stay green:** 38 tests pass now. This plan adds **no new test target**. The only code
change is one log statement, so the count stays at 38.

### Reproducing the probes (scratch only, never in the repo)

Both probes live in `<scratchpad>/probe/`. Build each with
`gcc -O1 -I build/_deps/miniaudio-src X.c -o X -lm -lpthread -ldl`. Neither makes any sound.

- `probe.c`: `#define MINIAUDIO_IMPLEMENTATION`, then `ma_engine_init` on the default config and
  `ma_engine_get_device`. It prints `ma_get_backend_name(d->pContext->backend)`, the
  `ma_device_get_name(d, ma_device_type_playback, …)` result, and
  `d->playback.internalSampleRate/internalPeriodSizeInFrames/internalPeriods`. Then it runs
  `pw-top -b -n 3` while the engine outputs silence.
- `gran.c`: builds a 5 s all-zero `ma_audio_buffer` (f32, 2 ch, engine rate), runs
  `ma_sound_init_from_data_source` with volume 0 and `ma_sound_start`, then polls
  `ma_sound_get_cursor_in_pcm_frames` every 1 ms for 2 s and prints min, mean and max step.

---

## Findings (the content of `docs/AUDIO_LATENCY.md`)

### F1 — What the clock measures today

| Step | Code | Effect |
|------|------|--------|
| Song audio loaded fully decoded | `src/audio/sound_stream.cpp:55-62` (`MA_SOUND_FLAG_DECODE`) | No streaming-decoder delay. The cursor is the read position in an in-memory PCM buffer |
| Clock source | `src/audio/sound_stream.cpp:138-154` → `ma_sound_get_cursor_in_pcm_frames` | Frames the engine has **pulled into the current device callback**. The cursor moves when miniaudio *writes* audio, not when it is *heard* |
| Gameplay binding | `src/gameplay/gameplay_view.cpp:111-124`, offset at `:70` | `SamplePosition{audio_.get_position_frames(), audio_.get_sample_rate()}` |
| Formula | `src/timing/music_clock.hpp:14-26`, `.cpp:53-61` | `time = frames / rate + global_offset_seconds`. Positive offset = clock reads later |
| Offset storage | `src/data/config.hpp:37-39`, loader `src/data/config_loader.cpp:203-210, 393` | One `offset.global_offset_seconds`, clamped to ±3600 |
| Calibration | `src/screens/calibration_screen.cpp:63, 98-121`, math `src/timing/offset_calibration.cpp:62-120` | The player taps to clicks played through the *same* device path. `offset = -mean(hit - beat)`, so it absorbs **audio-output latency + input latency + player bias** (not display latency, because the wizard is audio-only) |
| Input aging | `src/gameplay/judgment_input.hpp:11-19`, `src/app/app.cpp:152` | `hit = reference_music - (reference_ns - event_ns)`. `reference_music` is read once per frame from the stepped cursor |
| Engine init | `src/audio/audio_engine.cpp:19-37` | `ma_engine_config_init()` defaults: default playback device, default (low-latency) performance profile. No device ID, no notification callback, and nothing about the device is logged except rate and channels |

Note: the issue's technical-notes comment says "Device enumeration/selection in
`src/audio/audio_engine.cpp`". **That code does not exist.** The engine always opens the OS default
device. The doc must say this, and follow-up F-A adds it.

### F2 — Where the unmeasured latency hides (Linux/PipeWire path)

```
decoded PCM ──cursor──▶ ma_engine mix ──▶ PA client buffer (tlength ≈ 18.75 ms here)
   ──▶ PipeWire graph (quantum 256 = 5.3 ms here) ──▶ sink
        ├─ wired / speaker: ALSA period/buffer (a few ms) ──▶ DAC ──▶ ear          ≈ 25–40 ms total
        └─ Bluetooth: bluez5 sink ──▶ codec encode (SBC/AAC/LDAC) ──▶ radio
                       ──▶ headphone jitter buffer + decode ──▶ ear              ≈ 150–300 ms typical
OBS "Desktop Audio" taps the sink monitor here ─┘ (before the bluez5/headphone stages)
```

- The wired total is an estimate from the measured parts (client buffer + quantum + ALSA). The
  Bluetooth range is the commonly reported A2DP figure, **not measured here**. The doc must label it
  as such until Task 4 (owner measurement) provides real numbers.
- The headphone-side buffer is **invisible to every OS API**, so even a perfect OS latency query
  would leave headset-specific error.
- Codec and profile changes (A2DP vs HFP when a mic is active, SBC vs AAC vs LDAC) change the latency
  of the *same* device.

### F3 — Why OBS is out of sync while play feels right

Derivation from `music_clock.hpp` and `offset_calibration.hpp:16-20`. Let `L` be the output latency
from cursor to ear:

1. Calibration: the click for beat `b` is heard at cursor time `b + L`, so taps land at about
   `b + L + L_in`. The wizard stores `offset ≈ -(L + L_in)`.
2. Gameplay: the note at `t` reaches the receptor when `clock = t`, that is when
   `cursor = t + L + L_in`. The player hears that note's audio at `cursor = t + L`. Arrow and sound
   line up for the player.
3. OBS captures audio at about `cursor + client buffer` (sink monitor, upstream of Bluetooth) and
   video at render time. In the file, the arrow reaches the receptor about `L_bt + L_in − ε` **after**
   the sound. With Bluetooth that is roughly 150–300 ms (very visible). With wired it is about
   `L_wired + L_in` (small, and the saved −22 ms suggests about 20 ms), which is hard to notice.
4. **Workaround to document now:** in OBS, open *Advanced Audio Properties*, then *Sync Offset* on
   the Desktop Audio (or application-capture) source, and set it to `+|offset_ms|`. That delays the
   audio to match the video. Another option is to record while calibrated for wired output.

### F4 — Clock granularity (secondary, measured)

The cursor steps about 900 frames (18.75 ms) at a time on this machine. Consequences:

- Judgment: `reference_music` is up to one callback **behind** the true position, so aged hit times
  read early by `U(0, step)`. The mean bias is about step/2 (around 9 ms), which calibration absorbs.
  The jitter of up to ±9 ms is **not** absorbed, and it is a large share of the ±21.5 ms Fantastic
  window.
- The callback size depends on device and backend (Bluetooth sinks often run larger quanta). That is a
  second, smaller reason the optimal offset shifts per device.
- Visual: arrows advance in 18.75 ms jumps, which is about one 60 Hz frame, so there is visible judder
  at high speed mods.
- This is a *separate* follow-up (F-C). Fixing it means interpolating between callbacks, which touches
  AGENTS.md principle 1 (music-driven clock), so it needs an owner decision and a reference check
  against StepMania's sound position code (the implementer of F-C must cite `file:line`; this plan
  does **not** claim how StepMania does it).

### F5 — What miniaudio 0.11.21 exposes (cited from `build/_deps/miniaudio-src/miniaudio.h`)

| API / field | Line | Usable for |
|-------------|------|------------|
| `ma_engine_get_device(ma_engine*)` | 11240 | Reach the `ma_device` behind the engine |
| `ma_device.playback.internalPeriodSizeInFrames / internalPeriods / internalSampleRate` | 7744-7755 | **Client buffer only** (and for PulseAudio, `periods×size` = `maxlength`, not the queued `tlength`) |
| `ma_device_get_info` / `ma_device_get_name(dev, ma_device_type_playback, …)` | 9006, 9059 (impl 42232-42291) | Device display name, plus `ma_device_info.id` (backend-specific, see below). With a default device (`pID == NULL`) it resolves the **current default** device at call time |
| `ma_device_id` union | 6985-7008 | Stable keys: `pulse[256]` (sink name, for example `bluez_output.88_C9_E8_25_D5_B1.1`), `wasapi[64]` (endpoint ID string), `coreaudio[256]` (device UID), `alsa[256]` |
| `ma_engine_config.notificationCallback` / `ma_device_notification_type_rerouted` | 11184, 6717; docs 6750-6787 | Learn that the default device changed (PulseAudio via `pa_stream_set_moved_callback`, line 7496/30242-30250). **Not fired by DirectSound** |
| `ma_get_backend_name(ma_backend)` | (used by the probe) | Backend label for logs and key prefix |
| PulseAudio sink latency | `ma_pa_sink_info.latency` (29166), discarded in `ma_context_get_device_info_sink_callback__pulse` | **Not exposed.** `pa_stream_get_latency` is not loaded, and streams are created without `AUTO_TIMING_UPDATE` (30520) |
| WASAPI `GetStreamLatency` | 20472-20595 (vtable only) | Internal. It reports only the shared-engine buffer anyway, not Bluetooth |

### F6 — Solutions evaluated

| Option | How | Pros | Cons | Verdict |
|--------|-----|------|------|---------|
| **A. Per-output-device saved offsets** | Key = `backend:device-id` from `ma_device_get_info(…).id`, with the display name kept for UI. The calibration wizard saves to the active device's entry. Gameplay resolves the offset at song start, falling back to `global_offset_seconds` | Captures *everything*, including the headphone-side buffer and the player's input/bias, because it is measured the same way the current offset is. Pure config/timing change. Fits "offset is the only compensation" (issue comment, AGENTS.md principle 1). Cross-platform via the miniaudio ID union | The first use of a new device needs one calibration. Codec/profile changes on the same headset are not distinguished. On non-UCM Linux, speakers and wired headphones can share one sink (same key). Default-device resolution can race a Bluetooth connect/disconnect | **Chosen** |
| B. Query OS latency at runtime | PulseAudio: `pa_stream_get_latency` (needs timing updates and our own libpulse symbol loading or a miniaudio patch). WASAPI: `IAudioClock`/`GetStreamLatency`. CoreAudio: device latency + safety offset + stream latency properties | No calibration per device. Could follow mid-song changes | Not in miniaudio 0.11.21's API, so it needs per-backend native code (breaks "thin platform wrapper" and the lean scope). Cannot see the headset buffer. Bluetooth reports are often inaccurate. Values fluctuate, so feeding them into the clock live adds jitter | **Rejected for v1.** Could be a seed for unseen devices later (A+B hybrid), noted as future work |
| C. Separate visual delay / "recording mode" | A second offset that shifts only the note field (similar to StepMania 5's visual-delay preference; verify before citing) | Could make recordings line up | Recordings are a niche case. OBS already has Sync Offset. It adds a second timing knob, which is outside the ITG-faithful scope | **Rejected.** Document the OBS Sync Offset workaround instead |
| D. Pick the latency of the lowest-latency device automatically | — | — | Does nothing for Bluetooth | Rejected |

### F7 — Follow-up issues (filed in Task 7)

- **F-A** Expose the active output device identity (backend, ID, name, client buffer) from
  `AudioEngine`, and react to reroute notifications.
- **F-B** Per-output-device offsets: config schema, resolution at song start, calibration saves per
  device, options menu shows the device. Blocked by F-A.
- **F-C** Reduce music-clock granularity (the cursor steps once per audio callback, about 18.75 ms).
  This is a spike or owner decision because it touches timing principle 1.

---

## Patterns to Follow

### Log style (prefix tag, `std::cout` for info, `std::cerr` for warnings)
```cpp
// SOURCE: src/audio/audio_engine.cpp:26-35
std::cerr << "[AudioEngine] Warning: Audio device initialization failed ("
          << static_cast<int>(result) << "). Running in silent fallback mode.\n";
...
std::cout << "[AudioEngine] Initialized successfully. Sample rate: "
          << ma_engine_get_sample_rate(engine_.get()) << " Hz, Channels: "
          << ma_engine_get_channels(engine_.get()) << "\n";
```

### Docs style
`docs/CROSS_PLATFORM_VERIFICATION.md`: H1 title, short `##` sections, bullet lists, and code
references in backticks (`docs/CROSS_PLATFORM_VERIFICATION.md:108-123` is the existing "Per-OS audio
backend guidance" section the new doc extends).

### Follow-up issue body style
```
// SOURCE: gh issue view 67 (filed by the #57 spike)
**Type**: Bug · **Priority**: Low · **Complexity**: Medium

### Description
...
### Acceptance Criteria
- [ ] ...
### Technical Notes
- ...
Related: #57
```

### Tests
No new tests. The change is a log statement, verified through `audio_test` output
(`tests/audio_test.cpp:25-28` calls `AudioEngine::init()` and prints the result).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `docs/AUDIO_LATENCY.md` | CREATE | The spike write-up: F1–F7 above, measurements, OBS workaround, decision, follow-up links (AC 1–3) |
| `src/audio/audio_engine.cpp` | UPDATE | One diagnostic log statement after successful init: backend, device name, period × periods @ rate, and that it excludes OS/Bluetooth latency (PRD:275) |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | Extend the "re-run the offset calibration wizard" sentence (`:122-123`) with a link to `AUDIO_LATENCY.md` |
| GitHub | CREATE ×3, COMMENT ×1 | Follow-up issues F-A, F-B, F-C (AC 4) and a summary comment on #58 |

Not touched: `src/timing/*`, `src/data/*`, `src/screens/*`, tests, `.agents/stories/*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Branch

- **Action**: `git switch -c feature/032-audio-output-latency-spike` from `main` @ `2dbd609` or later.
  Confirm `git status --short` shows only `?? .agents/stories/todo-stories.md` (plus this plan if it
  is not yet committed).
- **Validate**: `git branch --show-current`

### Task 2: Diagnostic log line in `AudioEngine::init`

- **File**: `src/audio/audio_engine.cpp`
- **Action**: UPDATE (lines 32-36 only)
- **Implement**: after the existing "Initialized successfully" line, add:
  ```cpp
  if (ma_device* device = ma_engine_get_device(engine_.get()); device != nullptr) {
      char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
      ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), nullptr);
      const ma_uint32 rate = device->playback.internalSampleRate;
      const ma_uint32 period = device->playback.internalPeriodSizeInFrames;
      const double period_ms = rate > 0 ? 1000.0 * period / rate : 0.0;
      std::cout << "[AudioEngine] Output device: '" << name << "' via "
                << ma_get_backend_name(device->pContext->backend) << ", period " << period
                << " frames x " << device->playback.internalPeriods << " @ " << rate
                << " Hz (" << period_ms << " ms/period; excludes OS mixer and Bluetooth latency)\n";
  }
  ```
  If the name lookup fails, `name` stays empty, which is fine. Do not add a notification callback,
  device selection, or any timing change. Those belong to F-A and F-B.
- **Why it is allowed in a spike**: PRD:275 requires audio latency to be "measurable via log output".
  The owner's Bluetooth measurement (Task 4) needs to see which device and buffer were active.
- **Mirror**: `src/audio/audio_engine.cpp:33-35`
- **Validate**: `cmake --build build -j 2>&1 | grep -E "audio_engine.*(warning|error)"` (expect no
  output), then `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build -R audio_test -V | grep "\[AudioEngine\] Output device"`
  (expect one line naming the PulseAudio backend and a period of about 900 × 4 @ 48000).

### Task 3: Write `docs/AUDIO_LATENCY.md`

- **File**: `docs/AUDIO_LATENCY.md`
- **Action**: CREATE
- **Implement**: turn Findings F1–F7 into the doc. Suggested sections: *Summary* → *How the music
  clock works today* (F1 table with `file:line`) → *Where the latency hides* (F2 diagram) → *Why the
  best offset changes per device* → *Why OBS recordings drift* (F3 derivation and the Sync Offset
  workaround) → *Clock granularity* (F4) → *What miniaudio exposes* (F5) → *Solutions evaluated*
  (F6 table) → *Decision* → *Measurements* (Environment Findings rows and probe recipe, plus the
  Bluetooth row from Task 4 or an explicit "not measured" note) → *Follow-up issues* (fill in the
  numbers after Task 7). Keep the sign convention consistent with `music_clock.hpp:19-22` and
  `offset_calibration.hpp:16-20`. Correct the issue comment's claim about device enumeration (F1
  note). Mark every non-measured number (the Bluetooth 150–300 ms range, the wired 25–40 ms total) as
  an estimate.
- **Mirror**: `docs/CROSS_PLATFORM_VERIFICATION.md` (tone, headings)
- **Validate**: `grep -c "file\|src/" docs/AUDIO_LATENCY.md` (non-zero), then manually check that
  every `src/…:line` reference still points at the cited code (`sed -n` each one).

### Task 4: Bluetooth measurement (owner step, optional for the agent)

- **Action**: the agent must **not** connect, pair or route Bluetooth audio. If the owner has the
  WH-1000XM4 connected *as the default sink* when this runs (check `pactl get-default-sink` for a
  `bluez_output.` prefix), do these silent read-only steps:
  1. `SDL_AUDIO_DRIVER=dummy ctest --test-dir build -R audio_test -V | grep "Output device"` to get the
     Bluetooth client buffer.
  2. `timeout 4 pw-top -b -n 3` while that runs, plus
     `pw-dump | python3 -c '…print node.name, node.latency, params.Latency, params.ProcessLatency for bluez_output.* nodes…'`
     to get the quantum and reported latency.
  3. Ask the owner for two offsets: the wizard result on Bluetooth and on wired (or read
     `global_offset_seconds` from `data/config.json` after each run).
  Otherwise, record in the doc's Measurements section: "Bluetooth: not measured on the dev machine;
  owner reports a noticeably larger optimal offset (TODO.md:33); typical A2DP 150–300 ms". Then list
  the three steps above as the reproduction procedure.
- **Validate**: the Measurements section has either real Bluetooth numbers or the explicit
  not-measured note plus the procedure.

### Task 5: Cross-link from the cross-platform doc

- **File**: `docs/CROSS_PLATFORM_VERIFICATION.md`
- **Action**: UPDATE `:122-123`. Keep the sentence and append: "See `docs/AUDIO_LATENCY.md` for why
  (Bluetooth adds latency the game clock cannot see) and for the OBS recording sync workaround."
- **Validate**: `grep -n AUDIO_LATENCY docs/CROSS_PLATFORM_VERIFICATION.md`

### Task 6: Full suite and smoke

- **Validate**: see Validation. Expect **38/38** and a headless smoke run that exits 0.

### Task 7: File the follow-up issues (AC 4)

- **Duplicate check first**: `gh issue list --state all --search "per-device offset OR output device OR clock granularity OR audio callback in:title"`
  must return nothing relevant.
- **How**: write each body to `<scratchpad>/issue-F*.md` and run `gh issue create --title … --label … --body-file …`.
  Use the repo style (`**Type**: … · **Priority**: … · **Complexity**: …`, Description, Acceptance
  Criteria, Technical Notes, "Related: #58") and put everything in the body, no separate comment (as
  in #67). Do **not** use `/create-story` or `/update-stories`, and do **not** touch
  `.agents/stories/*`. Do not add the issues to the project board unless the owner asks. File F-A
  first, so that F-B can say "Blocked by #<F-A>".

**F-A** — title: `Expose the active audio output device (backend, ID, name, buffer) and handle reroutes`
labels: `audio`, `technical`
```
**Type**: Technical · **Priority**: Medium · **Complexity**: Medium

### Description

`AudioEngine` always opens the OS default device and knows nothing about it beyond sample rate and
channels (`src/audio/audio_engine.cpp:19-37`). The per-device offset work (#<F-B>) needs a stable
identity for the device that is actually playing, and needs to know when it changes (for example a
Bluetooth headset connecting mid-session). See docs/AUDIO_LATENCY.md (#58).

### Acceptance Criteria

- [ ] `AudioEngine` exposes an `OutputDeviceInfo { std::string key; std::string name; std::string backend; uint32_t period_frames; uint32_t periods; uint32_t sample_rate; }` for the active playback device, where `key = backend + ":" + backend-specific id` (PulseAudio sink name, WASAPI endpoint id, CoreAudio UID, ALSA name) taken from `ma_device_get_info(...).id`
- [ ] The key is printable/JSON-safe (WASAPI's wchar id is converted to UTF-8) and is empty, not garbage, when the backend cannot report an id (null backend / silent fallback)
- [ ] A miniaudio `notificationCallback` is registered via `ma_engine_config`. On `ma_device_notification_type_rerouted` it marks the device info stale (atomic flag, no work on the audio thread), and the main thread re-queries it on the next `output_device()` call and logs the new device
- [ ] Unit test for the pure id → key formatting (no real device). The existing `audio_test` still passes headless
- [ ] No change to the music clock, offsets or timing paths

### Technical Notes

- miniaudio 0.11.21: `ma_engine_get_device` (miniaudio.h:11240), `ma_device_get_info`/`ma_device_get_name` (9006/9059), `ma_device_id` union (6985-7008), reroute docs (6750-6787). DirectSound never fires rerouted. With a default device (`pID == NULL`), get_info resolves the *current* default device, which can differ briefly from the device the stream is on (PulseAudio stream-restore). Verify on PipeWire by connecting/disconnecting a Bluetooth headset.
- Do not call `ma_device_get_info` from the notification callback (it does a blocking server round-trip on PulseAudio).
- The diagnostic log added in #58 (`[AudioEngine] Output device: ...`) can be replaced by logging this struct.

### Dependencies

- Blocked by: None
- Blocks: #<F-B>

Related: #58
```

**F-B** — title: `Per-output-device global offset (remember calibration per headphones/speakers)`
labels: `timing`, `audio`, `data`, `enhancement`
```
**Type**: Enhancement · **Priority**: Medium · **Complexity**: Medium

### Description

The best global offset depends on the output device. Bluetooth adds about 150–300 ms of latency that
the music clock (the miniaudio decoder cursor) cannot see, against about 25–40 ms for wired (see
docs/AUDIO_LATENCY.md, #58). Today there is one `offset.global_offset_seconds`, so switching
headphones means recalibrating. Store one offset per output device and select it automatically.

### Acceptance Criteria

- [ ] Config gains `offset.devices`: a map `device key → { "name": string, "offset_seconds": number }` (keys from #<F-A>). It is additive: `global_offset_seconds` stays and is the fallback for unknown devices, old configs load unchanged, and malformed entries are dropped with a warning (same clamping as `global_offset_seconds`, ±3600, finite only)
- [ ] A pure resolver `resolve_offset(const OffsetSettings&, std::string_view device_key) -> double` returns the device entry when present and the global offset otherwise, and has unit tests (hit, miss, empty key)
- [ ] The calibration wizard's Confirm saves the result to the active device's entry (creating it with the device name) **and** to `global_offset_seconds`, so the latest calibration is the default for new devices
- [ ] Gameplay resolves the offset once at song start (`GameplayView` load). The offset is never changed mid-song, even if the device reroutes
- [ ] The options menu's offset row shows the active device name next to the value that will be used
- [ ] Config round-trip test covers `offset.devices`
- [ ] Manual: calibrate on wired, then on Bluetooth. Switching devices between songs picks the right offset with no recalibration

### Technical Notes

- Current code: `src/data/config.hpp:37-39`, `src/data/config_loader.cpp:203-210, 393`, `src/gameplay/gameplay_options.cpp:20`, `src/gameplay/gameplay_view.cpp:70`, `src/screens/calibration_screen.cpp:121`, `src/screens/options_menu.cpp:156`.
- Keep `MusicClock` unchanged. Compensation stays in the offset only (AGENTS.md principle 1, #58 issue notes).
- Known limits, to be documented and not solved: the same headset on a different codec/profile (A2DP vs HFP, SBC vs LDAC) shares a key, and on non-UCM Linux, speakers and the 3.5 mm jack can be one sink (same key).
- `kConfigVersion` can stay 1 because the change is additive. Decide in the plan.

### Dependencies

- Blocked by: #<F-A>
- Blocks: None

Related: #58
```

**F-C** — title: `Investigate music-clock granularity: decoder cursor advances in ~19 ms audio-callback steps`
labels: `timing`, `audio`, `spike`
```
**Type**: Spike · **Priority**: Medium · **Complexity**: Medium

### Description

Measured during #58: `ma_sound_get_cursor_in_pcm_frames` (the music clock source,
`src/audio/sound_stream.cpp:138-154`) advances once per audio callback (mean 876 / max 900 frames,
about 18.3–18.75 ms at 48 kHz, PipeWire/PulseAudio backend). `music_time_for_event`
(`src/gameplay/judgment_input.hpp:11-19`) pairs that stepped value with an SDL nanosecond reference, so
aged hit times have up to one callback (about ±9 ms around the mean) of jitter, a large share of the
±21.5 ms Fantastic window. Arrows also move in about 19 ms jumps. The step size differs per
device/backend, which shifts the optimal offset a little too.

### Acceptance Criteria

- [ ] Callback step size measured on Linux (wired + Bluetooth), and on Windows/macOS if available, and documented in docs/AUDIO_LATENCY.md
- [ ] How StepMania/OpenITG compute the music position between hardware updates is documented with exact `file:line` citations
- [ ] At least one approach evaluated against AGENTS.md principle 1 (music-driven clock): for example interpolating between callbacks using the frame count captured in the data callback plus a monotonic timestamp, or a smaller period via `ma_engine_config.periodSizeInFrames`/`performanceProfile`. Owner decision recorded
- [ ] Follow-up implementation issue filed if a change is approved

### Technical Notes

- Probe recipe (silent): a 5 s zero `ma_audio_buffer` → `ma_sound_init_from_data_source`, volume 0, poll the cursor every 1 ms. See docs/AUDIO_LATENCY.md.
- Any interpolation uses wall-clock between audio updates and needs explicit owner sign-off against principle 1.

### Dependencies

- Blocked by: None
- Blocks: None

Related: #58
```

- **Validate**: `gh issue view <n> --json title,labels` for each one. Then edit the doc's *Follow-up
  issues* section and the `#<F-A>`/`#<F-B>` placeholders in the F-A and F-B bodies
  (`gh issue edit <n> --body-file …`) with the real numbers.

### Task 8: Summary comment on #58

- **Action**: `gh issue comment 58 --body-file <scratchpad>/issue58-summary.md`. The comment holds the
  root cause (3 bullets), the OBS explanation and workaround, the decision (option A) with a link to
  `docs/AUDIO_LATENCY.md` on the branch, the follow-up issue numbers, and the correction about
  `audio_engine.cpp` having no device enumeration. Do not close #58 here. The PR (`Closes #58`)
  closes it.
- **Validate**: `gh issue view 58 --json comments --jq '.comments[-1].body' | head -5`

---

## Validation

```bash
# Build (warnings in touched files = lint gate; no linter configured)
cmake --build build -j 2>&1 | grep -E "warning|error"; cmake --build build -j

# Tests: expect 38/38 (headless, no window; audio_test opens the default device and outputs test audio as before)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# The new diagnostic line appears
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build -R audio_test -V | grep "\[AudioEngine\] Output device"

# Smoke
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10; echo "exit=$?"

# Scope guard
git diff --stat            # expect only src/audio/audio_engine.cpp, docs/AUDIO_LATENCY.md, docs/CROSS_PLATFORM_VERIFICATION.md (+ this plan)
git status --short .agents/stories/todo-stories.md   # still "??", untouched
```

## End-to-End Verification

1. **Diagnostic (automated, headless):** the `audio_test` verbose output contains
   `[AudioEngine] Output device: 'Ryzen HD Audio Controller Speaker' via PulseAudio, period 900 frames x 4 @ 48000 Hz (18.75 ms/period; …)`
   (or whatever the current default sink is). That matches the probe numbers in Environment Findings.
2. **Doc accuracy:** each `src/…:line` and `miniaudio.h:line` reference in `docs/AUDIO_LATENCY.md`
   opens to the cited code (`sed -n`). The sign derivation in F3 agrees with
   `music_clock.hpp:19-22`/`offset_calibration.hpp:16-20`.
3. **Issues:** three new issues exist with the specified titles and labels. F-B references F-A as a
   blocker. #58 has the summary comment.
4. **Owner check (interactive; the agent does not run it):** with Bluetooth connected, launch the game
   normally. The log names the `bluez_output…` sink. In OBS, set Desktop Audio *Sync Offset* to
   `+|offset_ms|` and confirm a short recording lines up.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Bluetooth latency is not measured on the dev machine (headset paired but not connected, and the agent must not route audio) | The doc labels the range as typical or estimated. Task 4 gives the owner a silent procedure. AC 1 is satisfied by the mechanism and the measured wired-side parts. The real numbers can be added later | In scope (flag) |
| `ma_device_get_name` makes a blocking PulseAudio round-trip in `init()` | Init runs once, lazily on the first `SoundStream::load`. The cost is milliseconds | In scope |
| The log line exposes a device name in logs | Local stdout only, and the game is offline | Accepted |
| PulseAudio `periods × period` (75 ms) is misread as the real latency | The doc explains that `maxlength` and `tlength` differ (`miniaudio.h:30028-30035`). The log says "ms/period", not "latency" | In scope |
| Default-device resolution races a Bluetooth connect, so a wrong key is used | Called out in F-A's technical notes for verification. F-B only resolves at song start | Out of scope (follow-up) |
| Same headset, different codec/profile shares one key. On non-UCM Linux, speaker and jack share one sink | Documented as a known limit in F-B. Recalibration fixes it | Out of scope (documented) |
| Clock granularity (F4) fix conflicts with principle 1 | Filed as a spike (F-C) that needs an owner decision. Nothing changes in this branch | Out of scope (follow-up) |
| `audio_test` makes sound on the owner's machine | Existing behavior, unchanged by this plan. The probes in this plan are silent | Out of scope |
| Issue comment on #58 says device enumeration exists in `audio_engine.cpp` | The doc and the #58 summary comment correct it | In scope |

---

## Decisions

- **D1 — Option A (per-device saved offsets) is the chosen approach.** It measures what the player
  actually hears, including headphone buffers that no OS API can see. It reuses the existing
  calibration math, and it keeps all compensation in the offset (issue comment, AGENTS.md principle 1).
  Option B is rejected for v1 because miniaudio 0.11.21 does not expose sink or stream latency
  (F5), and native per-backend code breaks the thin-wrapper and lean-scope principles.
- **D2 — Split A into F-A (device identity + reroute) and F-B (offset model/UI)**, so each piece is
  small and testable. F-C is a separate spike.
- **D3 — The only code change is one log statement.** No timing, config or UI behavior changes in a
  spike.
- **D4 — File with `gh issue create`, not the story skills**, because `.agents/stories/todo-stories.md`
  is untracked and off-limits (same as the #57 → #67 precedent). The full issue bodies are also kept
  in this plan for traceability.
- **D5 — The OBS fix is documentation only** (OBS Sync Offset). No "recording mode" (option C).

## Open Questions

1. **Bluetooth numbers.** Can the owner provide the wizard's offset on the WH-1000XM4 and on wired
   3.5 mm, and was the current −22 ms in `build/data/config.json` calibrated on speakers or wired?
   *Proposed default:* ship the doc with the not-measured note and procedure (Task 4), and the owner
   adds the numbers later.
2. **Should F-C be filed now, or folded into the doc only?** It is a real finding about the clock
   (about ±9 ms judgment jitter), but outside the issue's literal question. *Proposed default:* file it
   as a Medium spike, since it explains part of the per-device shift and affects judgment fairness.
3. **Calibration write target (F-B).** Should Confirm also overwrite `global_offset_seconds`?
   *Proposed default:* yes (latest calibration becomes the fallback for unseen devices). The owner can
   change this in F-B's plan.
4. **Project board.** Should the follow-ups go on the GitHub Project board (Backlog)? *Proposed
   default:* no, unless the owner asks (same as the #67 precedent).

---

## Acceptance Criteria

- [ ] `docs/AUDIO_LATENCY.md` documents the root cause: the clock is the decoder/mix cursor and
  excludes device, PipeWire, Bluetooth and headset latency. It gives per-device magnitude (measured
  wired-side parts, Bluetooth measured or explicitly estimated) (issue AC 1)
- [ ] The doc explains the OBS desync with the sign derivation and the Sync Offset workaround (issue AC 2)
- [ ] The doc evaluates options A–D with tradeoffs and records the decision (issue AC 3)
- [ ] F-A, F-B and F-C are filed with the specified titles, labels and bodies, and linked from the doc
  and from a summary comment on #58 (issue AC 4)
- [ ] Diagnostic log line added. Build has no new warnings. 38/38 tests pass. Headless smoke exits 0
- [ ] `.agents/stories/todo-stories.md` untouched
