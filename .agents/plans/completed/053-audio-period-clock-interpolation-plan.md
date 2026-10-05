# Plan: Smaller audio period + callback-anchored music-clock interpolation (B + C from #71)

## Summary

Today the music clock is the raw `ma_sound` cursor. It moves once per audio callback (about 18.4 ms on the dev machine's default sink), and input aging pairs that stale cursor with a "now" timestamp taken at a different moment. This plan does what the owner approved on #71 (B + C):

- **B: a smaller period.** `AudioEngine` requests `ma_engine_config.periodSizeInFrames = 480` (10 ms @ 48 kHz) by default. A new config key, `audio.period_size_frames`, overrides it. `0` means the miniaudio/backend default, any other value is clamped to `[128, 4096]`, and a change applies on the next start. The `[AudioEngine] Output device` log line shows the requested and the negotiated period.
- **C: callback-anchored interpolation.** `ma_engine_config.onProcess` runs on the audio thread at the end of every engine read. There it reads an injected monotonic clock (`SDL_GetTicksNS` in production, a plain function pointer so `src/audio` stays SDL-free) and the registered clock sound's cursor. It publishes the anchor `(cursor, ns, device period)` through a lock-free seqlock.
- **Burst grouping.** A small grouper on the audio thread merges engine updates that arrive back to back within one device callback into a single group. That turns the per-update frame count into the device-callback interval required by AGENTS.md principle 1.
- **Game-thread estimator.** A pure `ClockInterpolator` in `src/timing` estimates `cursor + (now − anchor_ns) × rate`. It clamps the result to at most one device period ahead of the latest anchor, never lets it fall below the raw cursor or the song length, and never lets it decrease while playing. It resets on seek, stop, play, resume and load, and ignores anchors captured before the reset.
- **Consistent aging pair.** `SamplePosition` gains a `timestamp_ns` (the `now` used for the estimate). `MusicClock` exposes it as a `(seconds, ns)` pair, and gameplay and calibration age input events against that pair. This is the consistent pairing OpenITG/SM5 use.
- **Assist ticks stay on the raw cursor.** They are scheduled on engine time, which moves in lockstep with the raw cursor, not with the interpolated one.
- **Measurement.** A committed, non-ctest `clock_probe` tool reproduces the `gran2` measurement through the real `AudioEngine`/`SoundStream` path. Silent, using a `--null` backend mode for the sandbox.

## User Story

As a player
I want the music clock to advance smoothly and pair correctly with my input timestamps
So that arrows scroll without ~18 ms judder and my hits are judged with sub-2 ms clock error instead of up to ~15 ms

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | HIGH (audio-thread code, lock-free handoff, timing-critical semantics; ~10 files + 3 new) |
| Systems Affected | `src/audio/{audio_engine,sound_stream,metronome}.{hpp,cpp}`, new `src/audio/clock_anchor.{hpp,cpp}`, new `src/timing/clock_interpolator.{hpp,cpp}`, `src/timing/music_clock.{hpp,cpp}`, `src/gameplay/{gameplay_view.cpp,gameplay_view.hpp,judgment_input.hpp}`, `src/screens/calibration_screen.cpp`, `src/data/{config.hpp,config_loader.cpp}`, `src/main.cpp`, `CMakeLists.txt`, `tests/` (2 new ctest executables, 1 new tool, 3 updated tests), `docs/AUDIO_LATENCY.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md` |
| GitHub Issue | #81 (Implement smaller audio period + callback-anchored music-clock interpolation, B + C from #71) |
| Related | #71 (spike, decision), #69/#70 (device identity / per-device offset, **not implemented yet**) |
| Branch (suggested) | `feature/053-audio-period-clock-interpolation` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` configured (Release); `cmake --build build -j$(nproc)` is clean |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **49/49 pass** | `main` @ `9ff64bd`, sandboxed ctest (Validation), 0.69 s. The issue text says 41; the real baseline is 49. Target after this plan: **51/51** (2 new ctest executables) |
| Sandbox | `bwrap … --tmpfs /dev/snd …` | Inside the sandbox there is **no audio device**: `AudioEngine::init()` takes the silent fallback. All automated tests must pass there |
| miniaudio null backend | `ma_backend_null` (compiled in; `src/audio/miniaudio_impl.cpp` sets no `MA_NO_NULL`) | **Probed in the sandbox** (scratch `nullprobe2.c`): engine on a null-backend context, period 480 → `480 x 3 @ 48000`, `onProcess` fires ~102×/s, a silent sound's cursor advances in real time (47520 frames after 1 s). Usable for an agent-runnable integration test |
| `onProcess` frame count | `miniaudio.h:75273-75274` | Receives `framesRead`, which is **0 while the graph has nothing playing** (probed). The grouper must ignore 0-frame updates |
| ThreadSanitizer | **not available** | `g++ -fsanitize=thread` fails to link (`/usr/lib64/libtsan.so.2.0.0` missing); no clang. TSan is an owner-optional step (`sudo dnf install libtsan`). A seqlock prototype ran correctly in the scratchpad without TSan |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #81

- `docs/AUDIO_LATENCY.md:206-208`: the stale OpenITG citation in `src/gameplay/judgment_input.hpp:10` is "fixed in the follow-up issue #81" (Task 9).
- `docs/AUDIO_LATENCY.md:318, 355, 454` and the "Follow-up issues" list: update them to "implemented" (Task 13).
- `.agents/reviews/feature-036-music-clock-granularity-spike-review.md:21`: burst updates mean the cap must be the device-callback interval, not the engine update size. This is pinned below (CallbackGrouper).
- Stale line refs in the issue. The input reference is at `src/app/app.cpp:176` (the issue says `:152`). The gameplay clock binding is at `src/gameplay/gameplay_view.cpp:113-126`.

---

## Value Provenance

| Value / behavior | Source | Notes |
|---|---|---|
| `periodSizeInFrames` forwarded to the device | `build/_deps/miniaudio-src/miniaudio.h:74988-74989` | miniaudio 0.11.21 (`CMakeLists.txt` `GIT_TAG 0.11.21`) |
| `onProcess` fires at the end of every `ma_engine_read_pcm_frames`, on the audio thread | `miniaudio.h:11172` (typedef `void(*)(void*, float*, ma_uint64)`), `:11200`, call site `:75273-75274` | Receives `framesRead` |
| Cursor read returns a pending seek target immediately | `miniaudio.h:76823-76843` | After `ma_sound_seek_to_pcm_frame`, the raw cursor already reads the target. The interpolator reset uses the target as the floor |
| PulseAudio default 25 ms "because < ~20 ms glitches through PipeWire" | `miniaudio.h:30254-30258` | Reason for the explicit default and the override key |
| Step stops shrinking below the PipeWire graph quantum (256 @ 48 kHz), bursts at 128 | `docs/AUDIO_LATENCY.md` "Measured callback step" | Default must not go below 256. The grouper handles bursts |
| 480 frames: raw residual rms 3.26–3.27 ms, anchored rms 1.44–1.46 ms (max 2.9 ms) | `docs/AUDIO_LATENCY.md` table (gran2, 2026-10-03) | Target for the re-measure: interpolated rms ≤ 2 ms |
| On PulseAudio, `internalPeriodSizeInFrames` ≠ callback step (480 request → reported `1440 × 1`, step 480) | same table | So the clamp **cannot** use `internalPeriodSizeInFrames`. It uses the measured group size |
| OpenITG input aging | OpenITG `src/Player.cpp:918-926` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Verified in the #71 review (`.agents/reviews/feature-036-…-review.md:33`). Replaces `:908-919` at `judgment_input.hpp:10` |
| Consistent `(position, timestamp)` pair | OpenITG `src/RageSound.cpp:742-756`; SM5 `RageSoundDriver_Generic_Software.cpp:490-523` @ v5.0.12 | Basis for returning `timestamp_ns` with the position |
| Monotonic extrapolation anchored to audio timestamps | SM5 `RageSoundDriver_AU.cpp:214-217, 322-325` @ v5.0.12; SM 5_1-new `RageSoundDriver_PulseAudio.cpp:234-238` @ `825467bc` | Reference behavior for C |
| AGENTS.md principle 1 (amended in #71) | `AGENTS.md:13` | "…bounded to one device period ahead of the latest anchor (the device-callback interval, not the engine update size; engine updates that burst within one device callback collapse to the last one)…" |

---

## Pinned Semantics (the contract the tests pin)

### `ClockAnchor` (pure struct, `src/timing/clock_interpolator.hpp`)

```cpp
struct ClockAnchor {
    uint64_t cursor_frames = 0;        // clock sound's cursor after this engine update (source-rate frames)
    uint64_t timestamp_ns = 0;         // injected monotonic ns, read on the audio thread BEFORE the cursor
    uint32_t period_engine_frames = 0; // device-callback interval estimate (engine-rate frames)
};
```

The audio thread reads `now_ns` **before** the cursor. That ordering is what lets "ignore anchors with `timestamp_ns < reset_ns`" exclude every pre-seek cursor. The game thread takes `reset_ns` *after* the `ma_sound_seek_*`/start/stop call returns.

### `CallbackGrouper` (`src/audio/clock_anchor.hpp`, audio-thread state, pure, no miniaudio)

`uint32_t on_update(uint64_t frames, uint64_t now_ns, uint32_t engine_rate) noexcept` returns the current device-period estimate in engine frames.

- `frames == 0` or `engine_rate == 0`: ignored. Returns the current estimate unchanged and does not touch `last_update_ns`.
- `gap_ns = frames × 1e9 / engine_rate / 2` (half of this update's duration).
- First update, or `now_ns − last_update_ns > gap_ns`: start a new group (`prev_group = group` if one existed, `group = frames`). Otherwise the update is a burst in the same device callback: `group += frames`.
- `last_update_ns = now_ns`. Return `max(group, prev_group)`.

  `prev_group` covers the µs window right after the first update of a group. An early callback merged into the previous group only *loosens* the cap, to at most about 2 periods, for one interval. That is acceptable and tested.
- Also tracks `min_group`, `max_group` and `callbacks` (completed groups) for a diagnostic. These are `std::atomic<uint32_t/uint64_t>`, written relaxed on the audio thread and read on the game thread.

### `AnchorSlot` (`src/audio/clock_anchor.hpp`, seqlock, single writer)

- `void publish(const ClockAnchor&) noexcept` (audio thread, wait-free):
  `s = seq.load(relaxed); seq.store(s+1, relaxed); atomic_thread_fence(release); fields.store(relaxed)…; seq.store(s+2, release);`
- `bool try_read(ClockAnchor& out) const noexcept` (game thread):
  up to **8** attempts of `s1 = seq.load(acquire)`. Retry while `s1` is odd. Load the fields relaxed, `atomic_thread_fence(acquire)`, `s2 = seq.load(relaxed)`, and accept when `s1 == s2`. It returns `false` when no anchor was ever published (`s1 == 0`) or after 8 failed attempts. The caller then falls back to the raw cursor.
- All fields are `std::atomic<uint64_t>`/`<uint32_t>`, so there is no data race in the C++ memory-model sense (TSan-clean by construction). `static_assert(std::atomic<uint64_t>::is_always_lock_free)`.

### `ClockInterpolator` (`src/timing/clock_interpolator.{hpp,cpp}`, game thread, pure)

```cpp
struct InterpolationInput {
    uint64_t raw_cursor_frames = 0;
    bool playing = false;
    bool anchor_valid = false;
    ClockAnchor anchor;
    uint64_t now_ns = 0;
    uint32_t source_rate = 0;   // the sound's own rate (cursor units)
    uint32_t engine_rate = 0;   // device/engine rate (period units)
    uint64_t length_frames = 0; // 0 = unknown
};
class ClockInterpolator {
public:
    void reset(uint64_t now_ns, uint64_t floor_frames); // seek/stop/play/resume/load/unload
    [[nodiscard]] uint64_t update(const InterpolationInput& in);
    [[nodiscard]] uint64_t floor_frames() const;
};
```

`update` rules, in order:

1. `!playing`: `floor = raw`, return `raw`. Paused or ended means the raw cursor, which is today's behavior.
2. `est = raw`.
3. If `anchor_valid && anchor.timestamp_ns >= reset_ns && source_rate > 0 && engine_rate > 0`:
   - `elapsed = now_ns > anchor.timestamp_ns ? now_ns − anchor.timestamp_ns : 0`
   - `lead = floor(elapsed × source_rate / 1e9)`. Compute it in `long double` or split seconds/ns to avoid overflow.
   - `cap = floor(period_engine_frames × source_rate / engine_rate)`
   - `est = max(raw, anchor.cursor_frames + min(lead, cap))`. "Never below raw" covers the race window where the cursor has advanced inside an engine read before `onProcess` publishes.
4. If `length_frames > 0`: `est = max(raw, min(est, length_frames))`.
5. `out = max(est, floor)`, then `floor = out`, then return `out`. Never decreasing while playing.

Consequences pinned by tests:

- A late anchor never makes the clock go backwards. The floor holds the value until the estimate catches up.
- Nothing jumps more than one period ahead of the latest anchor.
- A stall or underrun freezes the clock at `anchor + cap`.
- Pre-reset anchors are ignored after seek/resume. Until a fresh anchor arrives the output is the raw cursor, so a backward seek works.

### `SamplePosition` / `MusicClock` (`src/timing/music_clock.hpp`)

- `struct SamplePosition { uint64_t frames = 0; uint32_t sample_rate = 48000; uint64_t timestamp_ns = 0; };`. Here `timestamp_ns` is the monotonic ns (SDL_GetTicksNS timebase) at which `frames` is the estimate, and `0` means unknown. Existing two-field aggregate inits keep compiling.
- New `struct TimedMusicTime { double seconds = 0.0; uint64_t timestamp_ns = 0; };` and `[[nodiscard]] TimedMusicTime timed_time_seconds() const;`. The method samples the source **once** and returns `frames/rate + offset` together with that sample's `timestamp_ns`. The header still includes only `<cstdint>` and `<functional>`.

### Input aging (`src/gameplay/judgment_input.hpp`)

- New `[[nodiscard]] inline uint64_t aging_reference_ns(uint64_t clock_timestamp_ns, uint64_t fallback_ns)`. It returns `clock_timestamp_ns != 0 ? clock_timestamp_ns : fallback_ns`.
- Gameplay and calibration call `const TimedMusicTime ref = clock_.timed_time_seconds();` and then `music_time_for_event(event.timestamp_ns, aging_reference_ns(ref.timestamp_ns, reference_ns), ref.seconds)`.
- Stub and injected sources report `timestamp_ns = 0`, so they keep using today's `App::input_reference_ns_`. Existing tests (`score_keeper_test`, `calibration_screen_test`) stay valid unchanged.
- The clock is sampled after `App::process_events()` has drained the events, so `ref.timestamp_ns` ≥ every event timestamp. A future event already maps to "no age" (`judgment_input.hpp:13`).

### `AudioEngine` (`src/audio/audio_engine.{hpp,cpp}`)

- `inline constexpr uint32_t kDefaultAudioPeriodFrames = 480;`
- `using MonotonicNowFn = uint64_t (*)();`
- `struct AudioEngineSettings { uint32_t period_size_frames = kDefaultAudioPeriodFrames; MonotonicNowFn now_ns = nullptr; bool use_null_backend = false; };`. `use_null_backend` is for headless tests and the probe only: a silent, real-time-paced miniaudio null device.
- `void configure(const AudioEngineSettings&)` must run **before** `init()`. If the engine is already initialized it logs `[AudioEngine] configure() after init; period applies on next init` and stores the settings. Then it calls `now_ns()` once on the calling thread, which primes SDL's lazy tick init before the audio thread exists.
- `init()` does the following:
  - Sets `config.periodSizeInFrames`, `config.onProcess = &on_process_thunk` and `config.pProcessUserData = this`.
  - With `use_null_backend`, it initializes an owned `ma_context` with `{ma_backend_null}` and sets `config.pContext`. The context is uninitialized after the engine in `shutdown()`.
  - The Output-device log line gains `requested period N frames (0 = backend default)` before the existing `period P frames x K` text. The existing text stays verbatim, because the owner procedures grep for `Output device`.
- Clock tap registration (one active tap):
  - `struct ClockTap { ma_sound* sound; AnchorSlot slot; };`, owned by a `SoundStream` through a `unique_ptr`, so its address is stable across moves.
  - `bool attach_clock_tap(ClockTap*)`. Returns false when the engine is not initialized or `now_ns == nullptr`. Replaces any previous tap.
  - `void detach_clock_tap(ClockTap*)`. Does nothing if this tap is not the active one. Otherwise it does `active_tap_.store(nullptr, seq_cst); while (in_process_.load(seq_cst)) std::this_thread::yield();`. After it returns, the audio thread can no longer touch that `ma_sound`. The handshake is Dekker-style under seq_cst: the audio side does `in_process_.store(true, seq_cst)` and then `active_tap_.load(seq_cst)`.
- `on_process(frames)` runs on the audio thread. It takes no locks, does no allocation and does no logging:

  ```
  in_process_ = true (seq_cst)
  if (frames > 0 && now_ns_) {
      ns = now_ns_()
      period = grouper_.on_update(frames, ns, engine_rate_)
      tap = active_tap_.load(seq_cst)
      if (tap) {
          read cursor via ma_sound_get_cursor_in_pcm_frames(tap->sound)
          tap->slot.publish({cursor, ns, period})
      }
  }
  in_process_ = false (release)
  ```
- `CallbackStats callback_stats() const` returns `{min, max, callbacks}`. `void log_callback_stats(const char* context) const` prints `[AudioEngine] Device callback interval (<context>): min A / max B frames over N callbacks @ R Hz` (game thread).
- `shutdown()` sets `active_tap_ = nullptr` before `ma_engine_uninit`. Tap owners still detach first.

### `SoundStream` (`src/audio/sound_stream.{hpp,cpp}`)

- `struct TimedFrames { uint64_t frames = 0; uint64_t timestamp_ns = 0; };`
- `bool enable_clock_interpolation()` can be called after `load()` and is idempotent. It creates the tap, attaches it, and caches `engine_rate` and `length_frames`. It returns false, and the stream stays raw-only, when the engine is not initialized or has no `now_ns`.
- `TimedFrames get_timed_position_frames() const`: with an attached tap, `now = now_ns()`, `try_read(anchor)`, read the raw cursor, then `interp_.update(...)`, returning `{out, now}`. Without a tap, `{raw_guarded, 0}`.
- `get_position_frames()` returns `get_timed_position_frames().frames`, and `get_position_seconds()` follows it. That means preview and metronome-override paths stay raw unless they opt in.
- `get_raw_position_frames()` is today's guarded raw implementation (`sound_stream.cpp:138-153`, moved verbatim). `get_raw_position_seconds()` is its seconds form.
- `play()`, `resume()`, `stop()`, `seek_seconds()`, `load()` and `unload()` call `interp_.reset(now_or_0, target_frame)` **after** the miniaudio call.
- `unload()` calls `detach_clock_tap` **before** `ma_sound_uninit`. The move ctor/assign moves `tap_` and `interp_` with `sound_`.

### Config (`audio.period_size_frames`)

- `int period_size_frames = kDefaultAudioPeriodFrames (480)` in `AudioSettings`. `config.hpp` stays pure, so it defines its own `kDefaultAudioPeriodFrames = 480`, `kMinAudioPeriodFrames = 128` and `kMaxAudioPeriodFrames = 4096`. A `static_assert` in `src/main.cpp` ties it to the audio constant.
- Loader rules:
  - Missing/null: default.
  - Wrong type or non-integer: warn, keep default (existing `read_int_field` behavior).
  - `0`: kept (backend default).
  - `1..127`: clamped to 128 with the warning `audio.period_size_frames out of range; clamped`.
  - Above 4096: clamped to 4096 with the same warning.
  - Negative: clamped to 128 (not to 0, so a typo never silently selects the backend default). Implementation: read with `read_int_field(…, -1'000'000, kMax, …)`, then `if (v != 0 && v < kMin) { warn; v = kMin; }`.
- `validate_game_config`: `v == 0 || (kMin <= v && v <= kMax)`.
- The writer emits it in `document["audio"]`.
- Applies at the next start (no live re-init).

---

## Patterns to Follow

### Naming / module purity

```cpp
// SOURCE: src/timing/music_clock.hpp:26-28
// This module must remain free of wall-clock/frame-timing inputs: it includes
// only <cstdint> and <functional>, with no platform or timing headers.
```

`clock_interpolator.hpp` follows the same rule (`<cstdint>` only). Time arrives as `uint64_t` ns parameters and the module never reads a clock.

### Engine init + diagnostic log

```cpp
// SOURCE: src/audio/audio_engine.cpp:24-31
ma_engine_config config = ma_engine_config_init();
ma_result result = ma_engine_init(&config, engine_.get());
if (result != MA_SUCCESS) {
    std::cerr << "[AudioEngine] Warning: Audio device initialization failed ("
              << static_cast<int>(result) << "). Running in silent fallback mode.\n";
    return false;
}
```

### Error handling (config: warn + clamp, never fail)

```cpp
// SOURCE: src/data/config_loader.cpp:97-100
if (value < static_cast<double>(lo) || value > static_cast<double>(hi)) {
    append_warning(warnings, std::string(section) + "." + key + " out of range; clamped");
}
target = static_cast<int>(std::clamp(value, static_cast<double>(lo), static_cast<double>(hi)));
```

### Clock source binding + input aging (sites to change)

```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:121-125, 137, 155-156
clock_.set_source([this] {
    return SamplePosition{audio_.get_position_frames(), audio_.get_sample_rate()};
});
...
const double reference_music = clock_.time_seconds();
...
judge_.handle_step(column, music_time_for_event(event.timestamp_ns, reference_ns, reference_music));
```

The same aging pattern is at `src/screens/calibration_screen.cpp:97, 108-109`. The metronome source is at `src/audio/metronome.cpp:191-205`.

### Tests (plain executable, `TEST_CHECK` abort macro, section prints)

```cpp
// SOURCE: tests/metronome_sync_test.cpp:11-17, tests/music_clock_test.cpp:22-34
#define TEST_CHECK(expr) \
    do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; std::abort(); } } while (0)
int main() { ... std::cout << "  - 2. config round-trip ok.\n"; }
```

Registration follows `tests/CMakeLists.txt:82-90` (`add_executable` + `target_link_libraries(... blaze4k_core)` + `add_test`). Silent WAV fixtures come from `tests/test_wav_writer.hpp`.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/clock_interpolator.hpp/.cpp` | CREATE | `ClockAnchor`, `InterpolationInput`, `ClockInterpolator` (pure) |
| `src/audio/clock_anchor.hpp/.cpp` | CREATE | `CallbackGrouper`, `AnchorSlot` seqlock, `ClockTap` |
| `src/audio/audio_engine.hpp/.cpp` | UPDATE | Settings/configure, period request, `onProcess` hook, tap attach/detach handshake, null backend, callback stats, log line |
| `src/audio/sound_stream.hpp/.cpp` | UPDATE | Tap ownership, interpolated timed position, raw accessor, resets, detach-before-uninit |
| `src/audio/metronome.cpp` | UPDATE | Enable interpolation on the owned stream; source carries `timestamp_ns` |
| `src/timing/music_clock.hpp/.cpp` | UPDATE | `SamplePosition::timestamp_ns`, `TimedMusicTime`, `timed_time_seconds()`; update the production-binding comment (`:23-24`) |
| `src/gameplay/judgment_input.hpp` | UPDATE | `aging_reference_ns`; fix OpenITG citation `:908-919` → `:918-926` @ `f2c129fe` |
| `src/gameplay/gameplay_view.cpp/.hpp` | UPDATE | Enable interpolation, timed binding, aging pair, assist ticks on the raw cursor, callback-stats log on shutdown |
| `src/screens/calibration_screen.cpp` | UPDATE | Aging pair |
| `src/data/config.hpp`, `src/data/config_loader.cpp` | UPDATE | `audio.period_size_frames` (read/validate/write) |
| `src/main.cpp` | UPDATE | `AudioEngine::instance().configure(...)` right after `app.init()` (`:258-262`), with an `SDL_GetTicksNS` thunk and a `static_assert` on the defaults |
| `CMakeLists.txt` | UPDATE | Add the 2 new `.cpp` files to `blaze4k_core` |
| `tests/clock_interpolation_test.cpp` | CREATE | Pure grouper/seqlock/interpolator tests (synthetic anchor sequences + 2-thread seqlock stress) |
| `tests/clock_tap_test.cpp` | CREATE | Null-backend integration (real `onProcess` → tap → `SoundStream`), sandbox-safe |
| `tests/clock_probe.cpp` | CREATE | `gran2`-equivalent measurement tool (**not** registered with ctest) |
| `tests/CMakeLists.txt` | UPDATE | Register the 2 tests and build `clock_probe` |
| `tests/music_clock_test.cpp`, `tests/config_persistence_test.cpp`, `tests/judgment_engine_test.cpp` | UPDATE | Timed pair, config key, `aging_reference_ns` |
| `docs/AUDIO_LATENCY.md` | UPDATE | Implemented design, measurements, per-device period table, recalibration advice, stale rows |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | Per-OS guidance: period override + recalibrate |
| `README.md` | UPDATE | Short "recalibrate after upgrading" note (the stand-in for release notes, see Open Question 3) |

---

## Tasks

Execute in order. Each task is atomic and verifiable. Build after every task with `cmake --build build -j$(nproc)`.

### Task 1: Pure interpolator (timing)

- **File**: `src/timing/clock_interpolator.hpp/.cpp`, `CMakeLists.txt` (`blaze4k_core` sources, next to `src/timing/music_clock.cpp`)
- **Action**: CREATE
- **Implement**: `ClockAnchor`, `InterpolationInput` and `ClockInterpolator` exactly as in Pinned Semantics. Do the overflow-safe lead math in `long double`; `elapsed` can be seconds after a stall. Add a header comment that cites AGENTS.md principle 1 and the #71 decision.
- **Mirror**: `src/timing/music_clock.hpp:14-28` (pure-module comment style)
- **Validate**: build

### Task 2: Grouper + seqlock (audio, pure)

- **File**: `src/audio/clock_anchor.hpp/.cpp`, `CMakeLists.txt`
- **Action**: CREATE
- **Implement**: `CallbackGrouper` and `AnchorSlot` as pinned. Add `struct ClockTap { ma_sound* sound = nullptr; AnchorSlot slot; };` with a forward-declared `ma_sound`. No miniaudio include in the header.
- **Validate**: build

### Task 3: Tests for Tasks 1–2

- **File**: `tests/clock_interpolation_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE
- **Implement**: Each case is a section with a `std::cout` line. Assert monotonic output, `out ≥ raw`, and `out ≤ anchor.cursor + cap` in every case.
  1. **Grouper.** Steady 480-frame updates every 10 ms give 480. Two 128-frame updates 20 µs apart, every 5.33 ms, give 256 (burst collapse). 0-frame updates are ignored. An early callback (4 ms instead of 10) merges and gives ≤ 960, then recovers to 480. The stats min/max/callbacks are right.
  2. **Seqlock.** Single-thread round trip. `try_read` is false before the first publish. 2-thread stress: the writer publishes 1e6 tuples with the invariant `timestamp_ns == cursor × 7 && period == cursor % 1000`; the reader checks the invariant on every successful read and counts successes > 0.
  3. **Steady.** 48 kHz source and engine, period 480, anchors every 10 ms on an ideal line, queries every 1 ms. Error vs the ideal line is ≤ 1 frame, and the clock never stalls (strictly increasing across queries 1 ms apart).
  4. **Late callback.** One anchor 8 ms late carrying the on-schedule cursor. No output decrease, and no single-query jump > cap. Back on the line after the next on-time anchor.
  5. **Burst.** 128-frame updates in pairs. With the grouper's period (256), the output keeps advancing through the whole 5.33 ms interval. Contrast: with a per-update cap of 128 it would stall for half the interval. Assert the stall does not happen.
  6. **Stall/underrun.** No anchors for 100 ms. The output freezes at `anchor + cap` and never decreases. After anchors resume it continues monotonically.
  7. **Pause/seek.**
     - `playing=false` returns raw.
     - `reset(now, target)` followed by a pre-reset anchor (ts < reset_ns) is ignored and the output is raw.
     - A backward seek is allowed through `reset`.
     - Resume after pause: the stale anchor is ignored, so there is no jump of `cap`.
  8. **Song end.** The output is clamped to `length_frames`, and `playing=false` at the end returns raw.
  9. **Resampling.** Source 44100, engine 48000, period 480 engine frames gives `cap = 441` source frames, and the lead uses 44100 Hz.
  10. **Race window.** Raw > anchor cursor gives output = raw.
  11. **Clock skew.** `now < anchor.ts` gives elapsed 0.
  12. **Zero rates.** These give raw.
- **Mirror**: `tests/music_clock_test.cpp` (structure), `tests/CMakeLists.txt:82-90` (registration)
- **Validate**: build, then the sandboxed ctest `-R clock_interpolation_test`

### Task 4: AudioEngine settings, period, onProcess, tap, null backend, stats

- **File**: `src/audio/audio_engine.hpp/.cpp`
- **Action**: UPDATE
- **Implement**: As pinned.
  - Keep `ma_engine_config_init()` and set only `periodSizeInFrames`, `onProcess`, `pProcessUserData` and, for the null backend, `pContext`.
  - Cache `engine_rate_ = ma_engine_get_sample_rate` after init.
  - Members: `std::atomic<ClockTap*> active_tap_{nullptr}`, `std::atomic<bool> in_process_{false}`, `CallbackGrouper grouper_`, `std::unique_ptr<ma_context> null_context_`.
  - Extend the log line (`audio_engine.cpp:47-51`) with `requested period N frames (0 = backend default)`.
  - Add a `static void on_process_thunk(void*, float*, ma_uint64)` declared in the .cpp, which needs a friend or public `on_process`. Use the miniaudio typedef signature exactly (`miniaudio.h:11172`).
- **Mirror**: `src/audio/audio_engine.cpp:19-55`
- **Validate**: build. `audio_test` still passes sandboxed (fallback path).

### Task 5: SoundStream interpolation

- **File**: `src/audio/sound_stream.hpp/.cpp`
- **Action**: UPDATE
- **Implement**: As pinned.
  - Move today's `get_position_frames` body to `get_raw_position_frames`, keeping the existing monotonic guard `:146-148`.
  - Add `std::unique_ptr<ClockTap> tap_`, `mutable ClockInterpolator interp_`, and cached `engine_rate_` / `length_frames_`.
  - `enable_clock_interpolation()` is idempotent.
  - Order in `get_timed_position_frames`: `now` first, then `try_read`, then the raw cursor, then `update`.
  - Resets after the miniaudio call in `play`/`resume`/`stop`/`seek_seconds`. `load`/`unload` reset to 0.
  - `unload()` detaches before `ma_sound_uninit`. The destructor goes through `unload()`, which already happens.
  - Update the move ctor/assign (`:18-38`) to move `tap_` and `interp_`.
  - `IAudioStream` (`sound_stream.hpp:13-24`) is unchanged.
- **Validate**: build. Sandboxed `audio_test`, `preview_player_test` and `metronome_sync_test` pass.

### Task 6: Null-backend integration test

- **File**: `tests/clock_tap_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE
- **Implement**:
  - Setup: `configure({480, steady_clock_ns, /*use_null_backend=*/true})` with a captureless `+[]{…}` lambda over `std::chrono::steady_clock`. Write a 3 s silent 48 kHz WAV to a temp dir with `test_wav_writer.hpp`, then `load`, `set_volume(0)`, `enable_clock_interpolation()` (must return **true**), `play`.
  - Poll every 1 ms for 1.5 s. Assert:
    - the interpolated value never decreases;
    - interpolated ≥ raw;
    - interpolated − raw ≤ `callback_stats().max + 1`;
    - `timestamp_ns` is non-zero and non-decreasing;
    - the interpolated value takes far more distinct values than the raw cursor (> 3×);
    - the mean lead over raw is > 0.
  - Then `seek_seconds(0.5)`: the next reading is within 0.5 s ± one period. `pause()`: two readings 20 ms apart are equal. `stop()`: 0. Load a second stream and attach it (replaces the tap). Then unload the first while the engine runs, which exercises the detach handshake (no crash, no hang).
  - Use loose bounds only, so a loaded host does not flake the test.
  - If the null backend cannot be initialized, print `SKIP` and return 0, but **fail** if it initialized and `enable_clock_interpolation()` returned false.
- **Mirror**: `tests/audio_test.cpp` (engine-singleton usage), `tests/test_wav_writer.hpp`
- **Validate**: sandboxed ctest `-R clock_tap_test`, run 5× in a row to check for flakiness

### Task 7: MusicClock timed pair + aging helper

- **File**: `src/timing/music_clock.hpp/.cpp`, `src/gameplay/judgment_input.hpp`
- **Action**: UPDATE
- **Implement**: `SamplePosition::timestamp_ns`, `TimedMusicTime` and `timed_time_seconds()` (one source call). `aging_reference_ns`. Fix the citation at `judgment_input.hpp:10` to `src/Player.cpp:918-926` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`.
- **Validate**: build

### Task 8: Wire gameplay, calibration, metronome

- **File**: `src/gameplay/gameplay_view.cpp/.hpp`, `src/screens/calibration_screen.cpp`, `src/audio/metronome.cpp`
- **Action**: UPDATE
- **Implement**:
  - **GameplayView.**
    - After `audio_.load(...)` succeeds (`gameplay_view.cpp:78`), call `audio_.enable_clock_interpolation()` and add `clock interpolation on|off` to the existing init summary log (`:95-108`).
    - `bind_clock_source` (`:121-125`) passes `{p.frames, rate, p.timestamp_ns}` from `get_timed_position_frames()`.
    - `handle_input_events` (`:137, 156`) uses the timed pair plus `aging_reference_ns`.
    - `schedule_assist_ticks` (`:346`) uses `audio_.get_raw_position_seconds()` instead of `clock_.sample_time_seconds()`. Extend the comment: engine time moves in lockstep with the raw cursor, not the interpolated one.
    - `shutdown()` (`:388`) calls `AudioEngine::instance().log_callback_stats("gameplay")` when audio was started.
  - **CalibrationScreen** (`:97, 108-109`): the timed pair, with `ctx.input_reference_ns` as the fallback.
  - **Metronome.**
    - After `stream().load` succeeds on the owned path (`metronome.cpp:173`), call `owned_.enable_clock_interpolation()`.
    - The owned `clock_source()` (`:203-205`) returns `timestamp_ns`.
    - The override path stays at `timestamp_ns = 0`.
- **Validate**: build. Sandboxed full ctest (calibration, score_keeper, gameplay_screen, perf_loop and metronome_sync must stay green).

### Task 9: Config key

- **File**: `src/data/config.hpp`, `src/data/config_loader.cpp`, `tests/config_persistence_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - As pinned: the read in `read_audio` (`config_loader.cpp:192-201`), the validate check near `:289-300`, the write at `:387-392`, and the `AudioSettings` field plus constants with a comment ("0 = miniaudio/backend default; raise this if you hear crackles or underruns; applies on next start").
  - Tests: default 480; round trip 256 and 0 (extend case 2 at `:118-160`); 64 → 128 with a warning; −5 → 128; 99999 → 4096; `"abc"` and `300.5` keep 480 with a warning; `validate_game_config` rejects 100 and accepts 0/128/4096.
- **Mirror**: `tests/config_persistence_test.cpp:196-206` (clamp case)
- **Validate**: sandboxed ctest `-R config_persistence_test`

### Task 10: Boot wiring

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Right after `app.init()` succeeds (`:258-262`) and **before** anything can init audio (the gameplay demo's `gameplay.init` at `:319`, `ui_sounds.init` at `:385`), call `blaze4k::AudioEngine::instance().configure({static_cast<uint32_t>(game_config.audio.period_size_frames), +[]() -> uint64_t { return SDL_GetTicksNS(); }})`.
  - Use a captureless lambda rather than `&SDL_GetTicksNS`, to avoid `SDLCALL` calling-convention mismatches on Windows.
  - Add `static_assert(blaze4k::kDefaultAudioPeriodFrames == static_cast<uint32_t>(blaze4k::kDefaultAudioPeriodFramesConfig))`, or whatever the two constant names end up being.
- **Validate**: build. Headless smoke (E2E step 2).

### Task 11: Music-clock tests

- **File**: `tests/music_clock_test.cpp`, `tests/judgment_engine_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `timed_time_seconds()` returns the offset-applied seconds plus the source's `timestamp_ns` from **one** source call (count the calls with a counter in the lambda).
  - A 2-field `SamplePosition` gives `timestamp_ns == 0`.
  - `aging_reference_ns(0, F) == F` and `aging_reference_ns(T, F) == T`.
  - A consistent-pair aging example: an event 3 ms before `T` with music `m` gives `m − 0.003`.
- **Validate**: sandboxed ctest

### Task 12: Measurement tool

- **File**: `tests/clock_probe.cpp`, `tests/CMakeLists.txt` (`add_executable(clock_probe …)` linked to `blaze4k_core`, **no** `add_test`)
- **Action**: CREATE
- **Implement**:
  - Usage: `clock_probe [period_frames=480] [--null] [--seconds 3]`.
  - Configure the engine (the null backend if `--null`), with a `steady_clock` now function. Write a 5 s all-zero WAV (engine rate, 2 ch) to the system temp dir, load it, `set_volume(0)`, enable interpolation and play.
  - Poll every 1 ms, recording `(now_ns, raw, interp)`.
  - Print:
    - the `[AudioEngine] Output device` line (already logged by init);
    - raw step min/mean/max;
    - the engine callback stats;
    - bias-removed rms/max residual of raw and interpolated against a least-squares line of the raw cursor over the run, skipping the first/last 0.3 s (same method as `gran2`, `docs/AUDIO_LATENCY.md` "Reproducing the probes");
    - the interpolated mean lead over raw.
  - Delete the temp file on exit.
  - A header comment says that it is silent (zero buffer, volume 0), that it briefly lowers the PipeWire quantum for other apps, and that it must run **outside** the bwrap sandbox to measure a real device.
- **Validate**: `bwrap … ./build/tests/clock_probe 480 --null` runs and prints both residual lines (null backend: near-zero residuals expected).

### Task 13: Documentation

- **File**: `docs/AUDIO_LATENCY.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md`
- **Action**: UPDATE
- **Implement**:
  - **`AUDIO_LATENCY.md`.**
    - "How the music clock works today" table (`:42-51`): update the Clock source, Input aging and Engine init rows to the new design.
    - Add a "Clock interpolation (#81, implemented)" subsection under Clock granularity. It covers the anchor capture, the grouper, the clamps, the timed pair, raw assist ticks, and the config key as **the fix for crackles/underruns** (raise it, e.g. 960, or set 0 for the backend default; restart to apply).
    - Recalibration advice: the smaller period plus the ~half-step lead move saved offsets by several ms (estimate ~5 ms at 480), so re-run the calibration wizard after upgrading.
    - Replace the "fixed in the follow-up issue #81" code finding (`:206-208`) with "fixed in #81".
    - Measurements: add a row for the `clock_probe` re-measure (Task 14) and a "Negotiated period per device" table with wired / Bluetooth rows marked **owner: not yet measured** until the owner fills them.
    - Owner procedure "measuring the callback step": point at `clock_probe` in place of the scratch `gran2`.
    - Decision/follow-up lines (`:318, 355, 454`): mark #81 implemented.
  - **`CROSS_PLATFORM_VERIFICATION.md`** "Per-OS audio backend guidance" (`:108-125`): one bullet on `audio.period_size_frames` (default 480; raise on crackles) and the recalibrate-after-change advice.
  - **`README.md`**: a one-paragraph "Upgrading" note after the settings sentence (`:44`): the music clock changed in this version, so re-run the offset calibration. Also say that `audio.period_size_frames` in `config.json` is the knob for crackles.
- **Validate**: `grep -n "#81" docs/AUDIO_LATENCY.md` (no "follow-up issue #81" remains); `grep -n "908-919" -r src docs` is empty

### Task 14: Measurement on the default sink (needs a real device)

- **Action**: MEASURE (no code)
- **Implement**:
  - **Outside the sandbox** (the sandbox has no device), on the current default sink only, without changing the default device or routing: `./build/tests/clock_probe 0` ×3 and `./build/tests/clock_probe 480` ×3. Record `pactl get-default-sink` before and after.
  - The probe is silent. If the agent session cannot open the real device unsandboxed, **leave this to the owner** and mark the doc row "owner: pending".
  - Expected at 480: raw step ≈ 480, interpolated rms ≤ 2 ms.
  - Record the results in `docs/AUDIO_LATENCY.md`.
  - If interpolated rms > 2 ms, do **not** add smoothing ad hoc. Record the result and raise it (Open Question 5).
- **Validate**: the doc table has the numbers, or an explicit "owner: pending"

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j$(nproc)

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
touch src/audio/audio_engine.cpp src/audio/sound_stream.cpp src/audio/clock_anchor.cpp src/audio/metronome.cpp \
      src/timing/clock_interpolator.cpp src/timing/music_clock.cpp src/gameplay/gameplay_view.cpp \
      src/screens/calibration_screen.cpp src/data/config_loader.cpp src/main.cpp
cmake --build build -j$(nproc) 2>&1 | grep -i "warning" | grep -E "audio_engine|sound_stream|clock_anchor|clock_interpolator|music_clock|metronome|gameplay_view|calibration_screen|config_loader|main\.cpp|clock_.*test|clock_probe" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Flake check for the real-time integration test
for i in 1 2 3 4 5; do bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build -R clock_tap_test --output-on-failure || break; done
```

Expected: **51/51 pass** (baseline 49/49 on `9ff64bd` + `clock_interpolation_test` + `clock_tap_test`). `metronome_sync_test` must stay green. Use the same bwrap prefix for any direct test-binary run.

Static checks:

```bash
# src/audio stays SDL-free; timing stays clock-free
grep -rn "SDL" src/audio src/timing                       # expect no hits
grep -n "#include" src/timing/clock_interpolator.hpp src/timing/music_clock.hpp   # only <cstdint>/<functional>
# No locks / allocation / logging on the audio thread
grep -n "mutex\|lock_guard\|std::cout\|std::cerr\|new \|make_unique" src/audio/clock_anchor.cpp   # expect none
# Assist ticks use the raw cursor
grep -n "get_raw_position_seconds" src/gameplay/gameplay_view.cpp
# Citation fixed
grep -rn "908-919" src docs                                # expect none
# Off-limits file untouched / unstaged
git diff --cached --name-only | grep -c todo-stories       # expect 0
```

Optional (owner, needs `libtsan`): `cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread && cmake --build build-tsan -j$(nproc)`, then the sandboxed ctest against `build-tsan` with `-R "clock_"`. The agent cannot run this here (libtsan is missing).

## End-to-End Verification

1. **Automated (agent, sandboxed):**
   - `clock_interpolation_test` pins every rule on synthetic anchor sequences: steady, late, burst, stall, pause/seek, song end, resampling and the race window, plus the seqlock under 2 threads.
   - `clock_tap_test` runs the real `onProcess` → seqlock → `SoundStream` path on miniaudio's null device in real time.
   - `metronome_sync_test` and the full suite stay green.
2. **Headless app smoke (agent, sandboxed, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --songs ./songs --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.`. With no device the engine takes the silent fallback. Confirm there is no crash and that no `configure() after init` warning appears (the init order is correct). Also run it with a scratch `config.json` holding `"audio":{"period_size_frames":64}` and check the clamp warning appears and the saved file has 128.
3. **Probe on the null backend (agent, sandboxed):** `./build/tests/clock_probe 480 --null` prints the requested/negotiated period (480) and both residual lines.
4. **Probe on the real default sink (needs a real device; agent only if unsandboxed device access is permitted, otherwise owner):** Task 14. Pass: raw step ≈ requested period, interpolated rms ≤ 2 ms at 480.
5. **Owner verification (real hardware, listening; the agent must not play audible audio):**
   - **a. Wired (speaker or 3.5 mm), default period.** Play one full song. There are no audible crackles or dropouts. The log has `[AudioEngine] Output device: … requested period 480 frames …` and, at the end of gameplay, `[AudioEngine] Device callback interval (gameplay): min/max … frames`. Record both in the "Negotiated period per device" table.
   - **b. Bluetooth (WH-1000XM4), default period.** Same check and record. If it crackles, set `audio.period_size_frames` to 960 (then 0), restart, and note which value is clean.
   - **c. Feel.** At a high speed mod (e.g. C600) the arrows scroll smoothly, with no ~18 ms stepping.
   - **d. Recalibrate.** Run the calibration wizard on each device. The offset moves by a few ms from the old value (expected). Then a full song judges on time.
   - **e. Optional TSan run** (Validation).

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Underruns or crackles at 480 frames (miniaudio's own PipeWire warning; Bluetooth) | Config override (`0` = backend default, `128–4096`), documented as the fix. Owner listening check on wired and Bluetooth (E2E 5a/5b) | In scope (owner verifies) |
| Bluetooth or another sink forces a bigger quantum than requested | The cap comes from the **measured** group size (CallbackGrouper), not the request or `internalPeriodSizeInFrames`, so it adapts automatically. Stats are logged per gameplay | In scope |
| Burst updates stall the clock (the review finding from #71) | Grouper collapses them; a burst test pins "no stall" | In scope |
| Late/early anchor jitter (the 18.8 ms outlier in #71) | Floor (no backward), cap (≤ one period ahead), raw floor. Pinned by tests. Smoothing only if the re-measure misses ≤ 2 ms (Open Question 5) | In scope |
| Use-after-free: the audio thread reads a `ma_sound` being uninitialized | Dekker handshake in `detach_clock_tap`, called before `ma_sound_uninit`; `shutdown()` clears the tap. Exercised in `clock_tap_test` | In scope |
| Seqlock torn read or memory-model mistakes | All fields atomic (no UB). Fences as pinned. 2-thread stress test. TSan is owner-optional (libtsan not installed here) | In scope (TSan: owner) |
| Pre-existing race: the game thread's `ma_sound_get_cursor_in_pcm_frames` reads the decoder cursor the audio thread writes (miniaudio-internal, non-atomic) | Unchanged by this plan (today's code does the same). A TSan run may flag it; document it and don't "fix" miniaudio | Out of scope (flag) |
| Assist ticks drift early if scheduled from the interpolated clock | `schedule_assist_ticks` uses `get_raw_position_seconds()` (static check) | In scope |
| Saved offsets shift by several ms (smaller period + ~half-step lead) | Recalibration advice in the docs and README. #70 (per-device offsets) is not implemented yet, so there is only the global offset | In scope (docs) |
| Audio initialized before `configure()` (singleton lazy init) | `configure()` placed right after `app.init()`, before any audio use. A warning is logged if late. Smoke checks that the warning is absent | In scope |
| Tests or tools that never call `configure()` | Default settings: period 480, `now_ns = nullptr`, so no interpolation and the raw cursor (today's behavior). Only `clock_tap_test` / `clock_probe` opt in | In scope |
| `clock_tap_test` flakes on a loaded host (real-time null device) | Loose bounds only, 5× repeat in Validation, SKIP only if the null backend itself cannot init | In scope |
| Calling `SDL_GetTicksNS` from the audio thread | `configure()` primes it on the main thread; it is a thread-safe monotonic read afterwards. The function pointer keeps `src/audio` SDL-free | In scope |
| Windows/macOS behavior of the 480 request (WASAPI shared ~10 ms, CoreAudio) is unmeasured | Owner cross-platform check (`docs/CROSS_PLATFORM_VERIFICATION.md`). The grouper adapts to whatever interval is negotiated | Out of scope (owner) |
| Device reroute re-negotiates the period mid-session (#69 not implemented) | The grouper re-measures continuously and the cap follows. No reroute handling is added here | Out of scope (flag) |

---

## Open Questions

None of these blocks implementation; the plan uses the recommended default for each. Questions 3 and 4 touch written deliverables or the house rule for probes. **Owner decision** marks the questions that need one.

1. **Default period: 480 or 256?**
   **Recommendation: 480.** It is the owner-approved target on #71, measured clean on the speaker sink, and it gives margin over the 256 PipeWire quantum. 256 halves the raw error again but sits exactly on the quantum, with more underrun risk. Not a contract change. Revisit after the owner's Bluetooth listening test.
2. **Config range: `0` or `128–4096`.**
   **Recommendation: as pinned.** 128 is allowed for experimenters, because bursts are handled. 4096 ≈ 85 ms is a generous crackle escape. Adding an `audio.*` key fits PRD `config.json` "video/audio/input/offset/gameplay options" (`.agents/PRDs/PRD.md:212`), so this is not a spec change.
3. **Owner decision: where the "recalibrate" advice goes.** The AC says "release notes / options help text", but the repo has **no release notes file** and the options menu has **no help-text area** (`src/screens/options_menu.cpp` has labels only).
   **Recommendation:** `docs/AUDIO_LATENCY.md` + a README "Upgrading" note + `docs/CROSS_PLATFORM_VERIFICATION.md`. No new UI text, since that would be a Cabinet layout change. The owner may prefer a release-notes file or an options hint instead.
4. **Owner decision (light): commit `clock_probe`.** #71 kept `gran2` scratch-only.
   **Recommendation: commit it** as a non-ctest tool under `tests/`. The owner needs a reproducible probe for the per-device measurements (wired, Bluetooth, Windows, macOS), and it measures the real `AudioEngine`/`SoundStream` path, not a re-implementation.
5. **Smoothing beyond clamps.** The plan ships clamp-only (floor + cap + raw floor), which meets the "no backward, ≤ one period forward" AC. A time-smoothing filter (DLL/PLL style) adds drift risk.
   **Recommendation:** only add one if the Task 14 re-measure misses rms ≤ 2 ms at 480. Raise it then rather than inventing a filter now.
6. **`use_null_backend` in production settings.** It is a small test/probe hook in `AudioEngineSettings`, never set by `main.cpp`. It keeps the integration test sandbox-runnable and is consistent with the thin-wrapper rule (no SDL, no new platform code). Accept.

---

## Acceptance Criteria

- [ ] The engine requests `periodSizeInFrames = 480` by default, and the `[AudioEngine] Output device` line shows the requested and negotiated period (plus the per-gameplay measured callback interval)
- [ ] `audio.period_size_frames` overrides it (0 = backend default, otherwise clamped to 128–4096). It is validated, clamped on load, round-tripped in `config_persistence_test`, and documented as the crackle/underrun fix
- [ ] Underrun/crackle check: a full song on wired **and** Bluetooth at the default period, with the negotiated period per device recorded in `docs/AUDIO_LATENCY.md` (**owner**, E2E 5a/5b)
- [ ] The music clock interpolates from an audio-thread `(cursor, ns)` anchor captured in `onProcess`. It is clamped to one device-callback interval (grouped bursts) ahead of the latest anchor, never below raw, and never decreasing (monotonic guard kept)
- [ ] Anchor jitter is clamped, pinned by synthetic tests: steady, late, burst, stall/underrun, pause/seek, song end
- [ ] Thread safety: a seqlock of atomics, no locks on the audio thread, a detach handshake, a 2-thread stress test. TSan is owner-optional
- [ ] The anchor uses the injected `SDL_GetTicksNS` timebase, and `src/audio` has no SDL include
- [ ] Input aging uses the consistent `(music time, timestamp_ns)` pair from the interpolated clock in gameplay and calibration
- [ ] Recalibration is advised in `docs/AUDIO_LATENCY.md` and in the README / cross-platform doc (Open Question 3)
- [ ] `metronome_sync_test` passes and the full sandboxed suite passes **51/51**, with no new warnings
- [ ] `clock_probe` re-measure on the default sink is recorded: raw step ≈ requested, interpolated rms ≤ 2 ms at 480 (**needs a real device**: agent if permitted unsandboxed, else owner)
- [ ] OpenITG citation fixed: `src/gameplay/judgment_input.hpp:10` reads `src/Player.cpp:918-926` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`
