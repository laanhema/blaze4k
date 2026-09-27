# Plan: Global Offset Calibration Wizard (C5)

## Summary

Add a guided **tap-to-the-beat calibration wizard** to the arcade shell, entered from the C4
options menu. A new `ScreenId::Calibration` screen plays a steady, generated metronome click through
the existing `SoundStream`, binds a `MusicClock` to that stream exactly as `GameplayView` does, and
collects panel presses. Each press is aged from its SDL nanosecond timestamp to music-clock time via
the **same** `music_time_for_event()` helper gameplay uses, then converted to a delta against the
nearest metronome beat. After enough samples a **pure, headless-testable** `OffsetCalibration` model
averages the inlier deltas (with robust outlier rejection) and derives the global offset. On Confirm
the offset is written to `GameConfig::offset.global_offset_seconds` — the value B1's
`gameplay_options_from_config` already feeds into every `GameplayView` clock — and `main`'s existing
clean-exit `save_config` persists it. Back/abort never touches the config, so the previous offset is
retained unchanged.

The design keeps the pattern that worked for C4: a pure state/math module (`src/timing/`) plus a thin
screen that owns real audio/clock and is constructed with an injectable `IAudioStream` + clock
source for headless tests. No new `GameAction` and no `src/input/` changes are needed — the wizard
taps use the existing panel actions and Confirm/Back.

## User Story

As a pad player
I want a guided tap-to-the-beat calibration wizard that computes and saves my global audio offset
So that my hits register on-time despite pad/OS/audio latency.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/timing/` (new pure offset model), `src/audio/` (new metronome/click source), `src/screens/` (new calibration screen + options-menu entry), `src/screens/screen_manager.cpp` + `screen.hpp` (new `ScreenId::Calibration`), `src/main.cpp` (register screen), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #20 ([C5]) |
| PRD refs | §7.5 Calibration wizard, §5 story 4, §12 Phase C, §6 pattern 1 (music-driven clock), §14 (latency risk) |
| Depends on | C4 (#19, options menu), B1 (#9, music clock + offset), C1 (#16 screens), C2 (#17 config) — all merged |
| Blocks | — |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` (no `-Werror`) |
| Cores | 16 | `-j16` safe |
| Baseline tests | **21/21 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 21" (0.44 s), run this session |
| Build | **green** | `cmake --build build -j16` up to date, no warnings observed |
| Music clock | `src/timing/music_clock.hpp:14-26,49-60` | `time_seconds() = frames/rate + offset`; **positive offset = clock reads later**; pure (`<cstdint>`+`<functional>`) |
| Global offset (B1) | `src/gameplay/gameplay_options.cpp:20`, `gameplay_view.cpp:53` | `config.offset.global_offset_seconds` → `GameplayOptions.global_offset_seconds` → `clock_.set_global_offset_seconds` |
| Config offset field | `src/data/config.hpp:37-39,65`, `config_loader.cpp:203-210` | `OffsetSettings{global_offset_seconds=0.0}`, loader clamps `[-3600, 3600]`, `validate_game_config` requires finite |
| Gameplay input path | `src/gameplay/gameplay_view.cpp:101-131`, `judgment_input.hpp:11-18` | `reference_music = clock_.time_seconds()`; `hit = music_time_for_event(event.timestamp_ns, reference_ns, reference_music)`; **must be reused verbatim** |
| Input events / actions | `src/input/input_event.hpp:53-60`, `input_manager.cpp:40-82` | `timestamp_ns` from SDL (`event.key.timestamp` / `event.gbutton.timestamp`); panel actions `Left/Down/Up/Right`, `Confirm`, `Back` all mapped |
| Screen interface / manager | `src/screens/screen.hpp:22,30-46,51-72`, `screen_manager.cpp:19-28,132-159` | `enter/update/render/exit`, `handle_back`/`back_consumed`, `default_back_navigates`, `screen_id_name` switch |
| Options menu seam | `src/screens/options_menu.hpp:15-23`, `options_menu.cpp:216-246`, `select_screen.cpp:242-279` | `OptionsRow` enum + `options_row_name`/`options_row_value_text` are the documented C5 extension point; Select intercepts modal presses |
| Screen context | `src/screens/screen.hpp:30-46` | `config*`, `manager*`, `input_reference_ns` set by main each tick; **no data-dir path in context** (main passes it to the screen ctor) |
| Audio streams | `src/audio/sound_stream.hpp:14-61`, `preview_player.hpp:20-48` | `IAudioStream` test seam + `SoundStream` (file-based, `get_position_frames()`/`get_sample_rate()`); `PreviewPlayer(IAudioStream&)` is the injection precedent |
| WAV generation precedent | `tests/test_wav_writer.hpp:10-50` | 16-bit mono 44.1 kHz PCM writer (test-only; a production analogue is needed) |
| Headless clock precedent | `gameplay_view.cpp:86-99,138-150`; `tests/sync_test_harness.hpp:86-97` | synthetic frames advanced by `fixed_dt`, or injected fake `MusicClock::Source` |
| Test idiom / registration | `tests/options_menu_test.cpp:10-17`, `tests/CMakeLists.txt:186-194` | `TEST_CHECK` + one `add_executable`/`target_link_libraries(... tundra_core)`/`add_test` block per target |

**Start green, stay green:** 21 tests pass; this plan adds **2** targets
(`offset_calibration_test`, `calibration_screen_test`) and extends `options_menu_test` /
`screen_manager_test` in place → **23 expected**. No changes to `src/timing/judgment_constants.*`,
`src/gameplay/*` judgment/scoring, `src/input/*`, or `src/data/config*` field layout.

---

## Pinned Semantics

Authority: **PRD §6 pattern 1 / §7.5** for the music-clock rule; **`music_clock.hpp:14-26`** for the
offset sign; **`gameplay_view.cpp:101-131` + `judgment_input.hpp`** for the exact measurement path;
**C2 `OffsetSettings`** for persistence. No judgment/scoring constants are introduced.

### Measurement path (must match gameplay)

For every accepted panel press:

```
reference_music = clock_.time_seconds();                    // sampled once per update, like GameplayView
hit_music       = music_time_for_event(event.timestamp_ns,   // SAME helper as gameplay
                                       ctx.input_reference_ns,
                                       reference_music);
delta           = hit_music - beat_music;                    // sign: positive = late/after the beat
```

- `beat_music` comes from the analytic metronome schedule (below), **not** a chart.
- The wizard's `MusicClock` runs with `global_offset_seconds = 0` during measurement, so
  `delta` is the raw latency being calibrated. (Keeping the current offset would give
  `delta = latency + current_offset` and the same final answer; 0 is chosen for clarity — OQ4.)
- Only pressed panel events (`Left/Down/Up/Right`) are samples; releases, `Confirm`, `Back`, and
  menu actions are ignored for sampling.

### Offset sign convention (derived, then flagged)

Because `time_seconds() = sample_time + offset` and the note/beat times are fixed chart/schedule
times, a **late** player (delta > 0) needs the clock to read *earlier* at tap time:

```
new_offset_seconds = -mean_delta_seconds
```

Cross-checked against `tests/sync_test_harness.hpp:86-97`, where `delta_ms == offset*1000` for an
on-time player: an opponent offset shifts the measured delta by exactly the offset, confirming the
inverse relationship. The PRD/issue do not state the sign; **flagged OQ4** with this proposed default.

### Beat schedule (steady metronome)

Analytic, constant-BPM schedule in music-clock seconds:

```
beat_time(n)        = lead_in_seconds + n * beat_period_seconds
beat_period_seconds = 60 / bpm             // default bpm = 120 -> 0.5 s
nearest_beat_index(t) = clamp(round((t - lead_in) / period), 0, max_beats-1)
```

Rationale: PRD §7.5 only says "a steady beat"; a constant schedule needs no chart/simfile and cannot
drift. Defaults in **OQ3**. A tap whose `|delta|` exceeds the acceptance cap is rejected as a wild
mistap (`rejected_wild`).

### Sample policy and robust averaging

- Accumulate accepted samples up to `max_samples`; the result is "ready" at `min_samples`.
- Outlier rejection (robust, so a few mistaps do not skew the result): compute the median delta and
  the median absolute deviation (`MAD`); keep samples with
  `|delta - median| <= max(mad_multiplier * MAD, mad_floor_seconds)`.
- Result = mean and stddev of the retained (inlier) deltas. Defaults (**OQ1/OQ2**).

### Confirm / cancel semantics

- **Confirm** (only once `min_samples` are collected) writes
  `ctx.config->offset.global_offset_seconds = result.offset_seconds` and transitions to Select.
  Persistence is `main`'s existing clean-exit `save_config` (`main.cpp:344-347`).
- **Back / abort** transitions to Select via the manager's default back navigation and never writes
  the config → the previous offset is retained (AC4). No `handle_back` override is needed; the
  screen's `exit()` must not write.
- The saved value is applied to the gameplay clock by the **existing B1 path**
  (`gameplay_options_from_config` → `MusicClock::set_global_offset_seconds`); no new wiring.

### Headless beat and testability

- Production beat = a **generated 16-bit PCM click WAV** written once to
  `<data-dir>/calibration_click.wav`, loaded through the existing `SoundStream`, and used as the
  `MusicClock` source. A new `Metronome` class owns this and exposes `MusicClock::Source`.
- Headless / no audio device: `Metronome::prepare()` fails to load, the screen binds a **synthetic
  frames source advanced by `fixed_dt`** (mirrors `GameplayView`'s stub) and refuses to save a
  meaningless offset (logs `[Calibration] audio unavailable; offset not saved`).
- Tests never touch a device: inject a fake `IAudioStream` + a fake `MusicClock::Source` (the
  `PreviewPlayer(IAudioStream&)` and `sync_test_harness.hpp` precedents) and drive synthetic
  `InputEvent` timestamps. The math lives in the pure `OffsetCalibration` model. **OQ5**.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Offset applied to the gameplay clock via `GameplayOptions.global_offset_seconds` | `gameplay_options.cpp:20`, `gameplay_view.cpp:53` | Sourced |
| Positive offset = clock reads later | `music_clock.hpp:19-21` | Sourced |
| Measurement uses `music_time_for_event` + `clock_.time_seconds()` | `gameplay_view.cpp:110-129`, `judgment_input.hpp:11-18` | Sourced |
| Config field name / range / finite validation | `config.hpp:37-39`, `config_loader.cpp:203-210,290` | Sourced |
| `delta_ms == offset*1000` (inverse relation) | `tests/sync_test_harness.hpp:86-97` | Sourced |
| `new_offset = -mean_delta` sign | Derived from the two rows above; not stated in PRD/issue | **Flagged OQ4** |
| `min_samples=8`, `max_samples=32`, `max_abs_delta=0.25 s` | UI/statistics choice; no OpenITG source | **Flagged OQ1** |
| `mad_multiplier=3.0`, `mad_floor=0.05 s` | Robust-statistics choice | **Flagged OQ2** |
| `bpm=120`, `lead_in=2 s`, `max_beats=64` | Steady-beat choice | **Flagged OQ3** |
| Generated click WAV through `SoundStream` | No bundled asset; mirrors `tests/test_wav_writer.hpp` | **Flagged OQ5** |

No judgment windows, DP weights, grade boundaries, or life deltas are introduced or changed.

---

## Patterns to Follow

### Exact gameplay measurement path (reuse, do not reimplement)
```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:110-129
const double reference_music = clock_.time_seconds();
judge_.handle_step(column,
                   music_time_for_event(event.timestamp_ns, reference_ns, reference_music));
```

### Pure timing module (no SDL/GL/audio/clock)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:1-19, src/timing/music_clock.hpp:26-27
// includes only standard-library headers; loading/platform lives elsewhere
```

### Screen lifecycle + headless event loop
```cpp
// SOURCE: src/screens/select_screen.cpp:233-317, title_screen.cpp:37-74
void SelectScreen::update(ctx, fixed_dt, events) {
    for (const InputEvent& e : events) {
        if (!e.pressed) continue;
        switch (e.action) { /* ... */ }
    }
}
```

### Injectable audio stream seam
```cpp
// SOURCE: src/audio/preview_player.hpp:20-27, tests/preview_player_test.cpp:23-63
explicit PreviewPlayer(IAudioStream& stream);   // caller owns; tests pass a FakeAudioStream
```

### Fake clock source (headless)
```cpp
// SOURCE: tests/sync_test_harness.hpp:94-97
MusicClock clock([&fake] { return SamplePosition{fake.frames, fake.sample_rate}; });
```

### Options menu extension seam
```cpp
// SOURCE: src/screens/options_menu.cpp:216-246, select_screen.cpp:250-271
case OptionsRow::Fail: menu.fail_enabled = !menu.fail_enabled; break;
```

### Test idiom + registration
```cpp
// SOURCE: tests/options_menu_test.cpp:10-17, tests/CMakeLists.txt:186-194
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/offset_calibration.hpp` | CREATE | Pure `CalibrationConfig`/`CalibrationSample`/`CalibrationResult` + `OffsetCalibration` (schedule + robust average; no SDL/GL/audio) |
| `src/timing/offset_calibration.cpp` | CREATE | Beat-schedule math, wild rejection, MAD outlier prune, mean/stddev, offset sign |
| `src/audio/metronome.hpp` | CREATE | `MetronomeConfig`, `Metronome` (owns `SoundStream`, exposes `MusicClock::Source`), pure `write_click_track` decl |
| `src/audio/metronome.cpp` | CREATE | Click-WAV synthesis (16-bit mono PCM) + optional `IAudioStream` seam |
| `src/screens/calibration_screen.hpp` | CREATE | `CalibrationPhase`, `CalibrationScreen` + test accessors/test-seam ctor |
| `src/screens/calibration_screen.cpp` | CREATE | Lifecycle, beat/clock binding, tap sampling, Confirm-save / abort, render |
| `src/screens/screen.hpp` | UPDATE | Add `ScreenId::Calibration` |
| `src/screens/screen_manager.cpp` | UPDATE | `screen_id_name` case; Calibration in `default_back_navigates`; `handle_back` branch → Select |
| `src/screens/options_menu.hpp` | UPDATE | Add `OptionsRow::CalibrateOffset` + display-only `offset_seconds` field + `format_offset` helper |
| `src/screens/options_menu.cpp` | UPDATE | Row name/value text; seed `offset_seconds` from config; no-op adjust for the action row |
| `src/screens/select_screen.cpp` | UPDATE | Intercept the Calibrate row on Confirm/Right → `transition_to(ScreenId::Calibration)`; close overlay |
| `src/main.cpp` | UPDATE | Register `CalibrationScreen(paths.data_dir / "calibration_click.wav")` |
| `CMakeLists.txt` | UPDATE | Add the 3 new `.cpp` files to `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `offset_calibration_test` and `calibration_screen_test` |
| `tests/offset_calibration_test.cpp` | CREATE | Pure math: schedule, sign, wild/outlier rejection, readiness, determinism |
| `tests/calibration_screen_test.cpp` | CREATE | Headless screen integration with fake stream/clock: sampling path, Confirm-save, abort-retains, B1 apply, config round-trip |
| `tests/options_menu_test.cpp` | UPDATE | New row name/value, seeded offset display, adjust no-op, clamp to new `kOptionsRowCount` |
| `tests/screen_manager_test.cpp` | UPDATE | Back from `ScreenId::Calibration` → Select (new default-navigation member) |

Not modified: `src/input/*` (no new action), `src/data/config.hpp` / `config_loader.*` (offset field
already exists), `src/timing/music_clock.*`, `src/gameplay/*` (B1 path already consumes the offset),
`src/render/*`, `src/chart/*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure `OffsetCalibration` model

- **Files**: `src/timing/offset_calibration.hpp`, `src/timing/offset_calibration.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  // offset_calibration.hpp  (pure: <cstddef> + <vector> only)
  namespace td {
  struct CalibrationConfig {
      double lead_in_seconds = 2.0;
      double beat_period_seconds = 0.5;       // 120 BPM (OQ3)
      int max_beats = 64;
      int min_samples = 8;                    // OQ1
      int max_samples = 32;                   // OQ1
      double max_abs_delta_seconds = 0.25;    // OQ1
      double mad_multiplier = 3.0;            // OQ2
      double mad_floor_seconds = 0.05;        // OQ2
      [[nodiscard]] double beat_time(int index) const;
      [[nodiscard]] int nearest_beat_index(double music_seconds) const;
  };
  struct CalibrationSample { double beat_seconds = 0.0; double hit_seconds = 0.0; };
  struct CalibrationResult {
      bool ready = false; int accepted = 0; int rejected_wild = 0; int rejected_outlier = 0;
      double mean_delta_seconds = 0.0;  // mean(hit - beat); positive = late
      double offset_seconds = 0.0;      // -mean_delta (Tundra sign; OQ4)
      double spread_seconds = 0.0;      // stddev of inliers
  };
  class OffsetCalibration {
  public:
      explicit OffsetCalibration(CalibrationConfig config = {});
      void reset();
      bool add_sample(double beat_seconds, double hit_seconds); // false = wild-rejected
      [[nodiscard]] const CalibrationConfig& config() const;
      [[nodiscard]] int sample_count() const;
      [[nodiscard]] bool ready() const;
      [[nodiscard]] CalibrationResult result() const;           // pure, recomputed
  private:
      CalibrationConfig config_;
      std::vector<CalibrationSample> samples_;
      int rejected_wild_ = 0;
  };
  } // namespace td
  ```
  `beat_time(n) = lead_in + n*period`; `nearest_beat_index(t) = clamp(round((t-lead_in)/period), 0, max_beats-1)`.
  `add_sample`: `delta = hit - beat`; reject when `|delta| > max_abs_delta_seconds` (bump
  `rejected_wild_`, return false); cap total at `max_samples`; `ready() = sample_count() >= min_samples`.
  `result()`: compute deltas → median → `MAD` → `threshold = max(mad_multiplier*MAD, mad_floor)` →
  inliers `|d-median| <= threshold` → `mean_delta`, `offset = -mean_delta`, `spread`; `rejected_outlier`
  = median-eligible count − inliers; empty/inlier-empty → `ready=false`, zeros (no NaN). Never throws.
- **Mirror**: `src/timing/judgment_constants.hpp:1-19` (pure header), `src/timing/music_clock.cpp` (small pure impl).
- **Validate**: `cmake --build build -j16` (after Task 8 registers the `.cpp`).

### Task 2: `Metronome` (beat + click track)

- **Files**: `src/audio/metronome.hpp`, `src/audio/metronome.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  // metronome.hpp  (uses audio/sound_stream.hpp + timing/music_clock.hpp; no miniaudio in header)
  struct MetronomeConfig {
      double bpm = 120.0; double lead_in_seconds = 2.0; int beats = 64;
      double click_frequency_hz = 1000.0; double click_duration_seconds = 0.04;
      double click_amplitude = 0.7; double sample_rate = 44100.0;
      [[nodiscard]] double beat_period_seconds() const { return 60.0 / bpm; }
  };
  [[nodiscard]] bool write_click_track(const std::filesystem::path& path, const MetronomeConfig&);
  class Metronome {
  public:
      Metronome();
      explicit Metronome(IAudioStream& stream);          // test seam (mirrors PreviewPlayer)
      bool prepare(const std::filesystem::path& wav_path, const MetronomeConfig& = {});
      void start(); void stop();
      [[nodiscard]] MusicClock::Source clock_source();
      [[nodiscard]] const MetronomeConfig& config() const;
      [[nodiscard]] bool is_playing() const;
      [[nodiscard]] bool using_stub() const { return using_stub_; }
  private:
      IAudioStream& stream();
      SoundStream owned_; IAudioStream* override_ = nullptr;
      MetronomeConfig config_; bool prepared_ = false; bool playing_ = false; bool using_stub_ = false;
  };
  ```
  `write_click_track`: 16-bit mono PCM WAV with a short windowed sine click at each
  `beat_time(n)`; mirror `tests/test_wav_writer.hpp:10-50` header layout, but with per-beat clicks.
  `prepare`: create parent dirs, (re)generate the WAV if missing, then `stream().load(path)`;
  on failure set `using_stub_ = true` and return false. `start`: `playing_ = stream().play()`.
  `clock_source()`: production returns `SamplePosition{owned_.get_position_frames(), owned_.get_sample_rate()}`;
  with an override, `SamplePosition{pos_frames_from_seconds(override_->get_position_seconds()), rate}`.
- **Mirror**: `src/audio/sound_stream.hpp:14-61`, `src/audio/preview_player.hpp:20-48`.
- **Validate**: `cmake --build build -j16`.

### Task 3: Add `ScreenId::Calibration` and back navigation

- **Files**: `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`
- **Action**: UPDATE
- **Implement**: add `Calibration` to `enum class ScreenId` (after `Select` is fine; keep `Results`).
  In `screen_id_name` add `case ScreenId::Calibration: return "Calibration";` (required to avoid
  `-Wswitch`). In `default_back_navigates` include `id == ScreenId::Calibration`. In
  `ScreenManager::handle_back()` add `else if (active_id_ == ScreenId::Calibration) { transition_to(ScreenId::Select); }`.
  Update the header's comment to mention Calibration → Select (abort).
- **Mirror**: `src/screens/screen_manager.cpp:13-15,19-28,132-148`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` still passes (Task 9 adds the new case).

### Task 4: Options menu entry row

- **Files**: `src/screens/options_menu.hpp`, `src/screens/options_menu.cpp`
- **Action**: UPDATE
- **Implement**: append `CalibrateOffset` to `enum class OptionsRow` **after `Fail`** (keeps existing
  row indices 0–3 stable), so `kOptionsRowCount == 5`. Add `double offset_seconds = 0.0;` to
  `OptionsMenu` (**display-only**; `options_menu_apply` must NOT write it back). Seed it in
  `options_menu_from_config`: `menu.offset_seconds = config.offset.global_offset_seconds;`. Add
  `[[nodiscard]] std::string format_offset(double seconds);` (e.g. `"+0.023 s"` / `"-0.011 s"`).
  `options_row_name` → `"CALIBRATE OFFSET"`; `options_row_value_text` → `format_offset(menu.offset_seconds)`.
  `options_menu_adjust` for `CalibrateOffset` is a documented **no-op** (activation is handled by
  `SelectScreen`), so existing callers/tests keep compiling; keep the `Count` case.
- **Mirror**: `src/screens/options_menu.cpp:155-246`; C4's "extension point" comment (`options_menu.hpp:12-15`).
- **Validate**: `cmake --build build -j16`.

### Task 5: `CalibrationScreen` (pure-math host + real streams)

- **Files**: `src/screens/calibration_screen.hpp`, `src/screens/calibration_screen.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  enum class CalibrationPhase : int { CountIn = 0, Sampling, Ready };
  class CalibrationScreen : public Screen {
  public:
      CalibrationScreen();                                            // production
      explicit CalibrationScreen(std::filesystem::path click_wav_path);
      CalibrationScreen(IAudioStream& stream, MusicClock::Source source,
                        CalibrationConfig config = {});                // test seam
      [[nodiscard]] ScreenId id() const override { return ScreenId::Calibration; }
      void enter(ScreenContext&) override;
      void update(ScreenContext&, double fixed_dt, const std::vector<InputEvent>&) override;
      void render(ScreenContext&, GlQuadRenderer&, int w, int h) override;
      void exit(ScreenContext&) override;
      [[nodiscard]] CalibrationPhase phase() const { return phase_; }
      [[nodiscard]] int sample_count() const { return calib_.sample_count(); }
      [[nodiscard]] const CalibrationResult& result() const { return result_; }
      [[nodiscard]] bool audio_available() const { return !synthetic_; }
  private:
      MusicClock clock_; Metronome metronome_; OffsetCalibration calib_;
      CalibrationConfig config_{}; CalibrationResult result_{};
      std::filesystem::path click_path_; MusicClock::Source injected_source_{};
      bool source_injected_ = false; bool synthetic_ = false; bool saved_ = false;
      CalibrationPhase phase_ = CalibrationPhase::CountIn;
      double stub_frames_ = 0.0; uint32_t stub_rate_ = 44100;
  };
  ```
  - `enter`: `calib_ = OffsetCalibration(config_); clock_.set_global_offset_seconds(0.0); saved_ = false;
    phase_ = CountIn;` If `source_injected_` → `clock_.set_source(injected_source_)`, else
    `metronome_ = Metronome[...]`; `metronome_.prepare(click_path_, metronome-config)`; on success
    `metronome_.start(); clock_.set_source(metronome_.clock_source()); synthetic_ = false;` on failure
    `synthetic_ = true; clock_.set_source([this]{ return SamplePosition{(uint64_t)stub_frames_, stub_rate_}; });`
  - `update`: if `synthetic_` → `stub_frames_ += fixed_dt * stub_rate_`; else `metronome_.start()` is
    idempotent (already started). `const double reference_music = clock_.time_seconds();`. Promotion:
    if `phase_ == CountIn && reference_music >= config_.lead_in_seconds` → `Sampling`. For each pressed
    event: if panel action and `phase_ != CountIn` → compute `hit = music_time_for_event(event.timestamp_ns,
    ctx.input_reference_ns, reference_music)`, `idx = config_.nearest_beat_index(hit)`,
    `calib_.add_sample(config_.beat_time(idx), hit)`. If `event.action == Confirm && calib_.ready()`:
    if `!synthetic_ && ctx.config != nullptr` → `ctx.config->offset.global_offset_seconds =
    calib_.result().offset_seconds; saved_ = true; ctx.manager->transition_to(ScreenId::Select);`
    else log `[Calibration] audio unavailable; offset not saved`. Finally
    `result_ = calib_.result(); if (calib_.ready()) phase_ = Ready;`. **Back is not handled here**
    (the manager's default abort → Select).
  - `exit`: `metronome_.stop(); clock_.clear_source();` (no config write → abort retains offset).
  - `render`: dim background; title "CALIBRATE OFFSET"; phase text ("GET READY"/"TAP ON THE BEAT"/
    "DONE"); progress `sample_count()/min`; live computed offset when ready; footer
    `"[ANY ARROW] TAP   [ENTER] SAVE   [BACK] CANCEL"`. Headless renderer is a no-op
    (`draw_text` already guarded).
- **Mirror**: `src/screens/select_screen.cpp:201-329` (lifecycle/event loop/render), `gameplay_view.cpp:86-99` (stub clock).
- **Validate**: `cmake --build build -j16`.

### Task 6: Wire the entry from `SelectScreen`

- **File**: `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**: inside the `if (options_open_)` modal loop, **before** the existing `switch`, intercept
  the calibration row:
  ```cpp
  if (options_.row == static_cast<int>(OptionsRow::CalibrateOffset)) {
      if (event.action == GameAction::Up)   { options_menu_move_row(options_, -1); continue; }
      if (event.action == GameAction::Down) { options_menu_move_row(options_, +1); continue; }
      if (event.action == GameAction::Confirm || event.action == GameAction::Right) {
          options_open_ = false;
          if (ctx.manager != nullptr) { ctx.manager->transition_to(ScreenId::Calibration); }
          continue;
      }
      continue; // Left and unrelated actions: no-op
  }
  ```
  Leave the rest of the modal dispatch unchanged. `enter` already re-seeds `options_` from config,
  so a wizard-computed offset is shown next time Options opens.
- **Mirror**: `src/screens/select_screen.cpp:242-279`.
- **Validate**: `cmake --build build -j16`.

### Task 7: Register the wizard in `main`

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**: after `shell->add_screen(std::make_unique<td::GameplayScreen>());` add
  `shell->add_screen(std::make_unique<td::CalibrationScreen>(paths.data_dir / "calibration_click.wav"));`
  and `#include "screens/calibration_screen.hpp"`. Both `paths` and the include list are already in scope
  (`main.cpp:149-152,18-23`). No other changes: `ctx.config`, `ctx.input_reference_ns`, and the clean-exit
  `save_config` already cover persistence/application.
- **Mirror**: `src/main.cpp:286-311,341-351`.
- **Validate**: `cmake --build build -j16`.

### Task 8: Register sources and test targets

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/timing/offset_calibration.cpp`, `src/audio/metronome.cpp`, and
  `src/screens/calibration_screen.cpp` to the `tundra_core` list (`CMakeLists.txt:80-120`); append
  `offset_calibration_test` and `calibration_screen_test` blocks mirroring `tests/CMakeLists.txt:186-194`.
- **Mirror**: `CMakeLists.txt:80-120`, `tests/CMakeLists.txt:186-194`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 9: `offset_calibration_test` (pure)

- **File**: `tests/offset_calibration_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no SDL/GL/audio):
  1. **Schedule** — `beat_time(0)==lead_in`, `beat_time(3)==lead_in+1.5`; `nearest_beat_index`
     returns 0 for `lead_in`, the right index between beats, clamps at 0 and `max_beats-1`.
  2. **Zero bias** — samples exactly on beats → `mean_delta==0`, `offset==0`, `ready` once `min_samples`.
  3. **Late bias sign** — all samples `beat+0.030` → `mean_delta==+0.030`, `offset==-0.030`.
  4. **Early bias sign** — all samples `beat-0.020` → `offset==+0.020`.
  5. **Wild rejection** — a tap `0.5 s` off → `add_sample` returns false, `rejected_wild` increments,
     accepted count unchanged.
  6. **Outlier prune** — 10 inliers at `+0.020` + 1 at `+0.150` → `mean_delta ≈ +0.020`,
     `rejected_outlier >= 1`, `spread` small.
  7. **Readiness / cap** — not ready below `min_samples`; samples stop at `max_samples`.
  8. **Degenerate** — no samples → `ready==false`, zeros; all-wild → ready false, no crash.
  9. **Determinism** — identical inputs → identical `CalibrationResult`.
- **Mirror**: `tests/options_menu_test.cpp:10-17,253-265`.
- **Validate**: `./build/tests/offset_calibration_test` → 0.

### Task 10: `calibration_screen_test` (headless integration)

- **File**: `tests/calibration_screen_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; fake `IAudioStream` copied from `tests/preview_player_test.cpp:23-63`;
  fake `MusicClock::Source` over a `frames/sample_rate` struct):
  1. **Measure through the gameplay path** — construct with fake stream + fake source; `enter` with a
     `ScreenContext` carrying `config` and a real `ScreenManager` (Calibration + Select registered) and
     `input_reference_ns`. Past lead-in, feed synthetic panel `InputEvent`s whose `timestamp_ns` and
     `reference_music` produce a known delta; assert `sample_count`, and that the sample delta equals
     `music_time_for_event(...)` called directly (proves identical path).
  2. **Confirm saves + transitions** — after `min_samples`, press Confirm → `config.offset.
     global_offset_seconds == result.offset_seconds`, `manager.active_id()==ScreenId::Select`.
  3. **B1 application** — `gameplay_options_from_config(config).global_offset_seconds == computed`;
     a `MusicClock` set to that offset reads `sample_time + offset`.
  4. **Persistence round-trip** — `save_config`/`load_config` in a temp dir, assert the offset survives
     (real C2 path, AC3).
  5. **Abort retains** — seed `config.offset.global_offset_seconds = 0.123`, enter, collect some taps,
     send Back through `manager.update` → active is Select and the config value is still exactly
     `0.123` (AC4).
  6. **Synthetic (no audio) refuses to save** — construct via the production/default path with a fake
     failing stream (`load_result=false`), collect, Confirm → config unchanged, warning path taken.
  7. **Zero/garbage timestamps** — `timestamp_ns == 0` or `>= reference_ns` does not crash and does not
     produce a bogus sample.
  8. **Render/exit** — uninitialized `GlQuadRenderer` render is a no-op; re-enter resets state.
- **Mirror**: `tests/preview_player_test.cpp:23-63`, `tests/select_screen_test.cpp` (manager wiring), `tests/config_persistence_test.cpp:106-146`.
- **Validate**: `./build/tests/calibration_screen_test` → 0.

### Task 11: Extend `options_menu_test` and `screen_manager_test`

- **Files**: `tests/options_menu_test.cpp`, `tests/screen_manager_test.cpp`
- **Action**: UPDATE
- **Implement**: `options_menu_test`: row-nav clamp now `0..4`; `options_row_name(CalibrateOffset) ==
  "CALIBRATE OFFSET"`; `options_row_value_text` reflects `offset_seconds` via `format_offset` (e.g.
  seed `config.offset.global_offset_seconds = 0.023` → contains `"+0.023"`); `options_menu_adjust`
  on the row leaves the menu unchanged; `options_menu_apply` does **not** modify `config.offset`.
  `screen_manager_test`: register a spy for `ScreenId::Calibration`, `start` it, press Back →
  active becomes `ScreenId::Select`; `back_navigates()` is true on Calibration.
- **Mirror**: `tests/options_menu_test.cpp:69-84,172-205`; `tests/screen_manager_test.cpp:112-160`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect **23/23**).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 23/23: 21 existing + offset_calibration_test + calibration_screen_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/offset_calibration_test
./build/tests/calibration_screen_test
./build/tests/options_menu_test
./build/tests/screen_manager_test

# Purity: the offset model must stay free of SDL/GL/audio/clock/wall-clock
rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time" src/timing/offset_calibration.*
# expected: no matches

# Warning budget (no -Wswitch on ScreenId / OptionsRow)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless, non-blocking (no window/GL/audio device required), and use `--data-dir` so the
developer's real `data/` is untouched.

1. **Real binary boots and registers the wizard** (no regression):
   ```bash
   rm -rf /tmp/td-e2e-cal
   ./build/tundra-dance --headless --smoke-test 30 \
     --start-screen select --songs tests/fixtures/reference_pack --data-dir /tmp/td-e2e-cal
   # exit 0; logs "[SongLibrary] ...", "[SelectScreen] library: 4 songs, 6 charts",
   # "[ScreenManager] enter Select"; no crash; no audio device needed
   ```
2. **Config offset survives the real clean-exit save** (AC3/AC4 at the binary level):
   ```bash
   printf '{"version":1,"offset":{"global_offset_seconds":0.017}}\n' > /tmp/td-e2e-cal/config.json
   ./build/tundra-dance --headless --smoke-test 5 --start-screen select --data-dir /tmp/td-e2e-cal
   # exit 0; /tmp/td-e2e-cal/config.json still contains 0.017 (no wizard run -> unchanged)
   ```
3. **Wizard end-to-end with an injected clock (no device)** — measures, saves, applies (AC1/AC2/AC3):
   ```bash
   ./build/tests/calibration_screen_test
   # fake IAudioStream + fake MusicClock::Source; synthetic taps aged via music_time_for_event;
   # after min_samples Confirm writes config.offset.global_offset_seconds;
   # save_config/load_config round-trips; gameplay_options_from_config returns the offset (B1);
   # Back leaves the seeded offset untouched
   ```
4. **Pure calibration math**:
   ```bash
   ./build/tests/offset_calibration_test
   # beat schedule, sign convention (late -> negative offset), wild + MAD outlier rejection,
   # readiness at min_samples, cap at max_samples, determinism
   ```
5. **Options-menu entry row** (AC1 reachability):
   ```bash
   ./build/tests/options_menu_test   # "CALIBRATE OFFSET" row, seeded offset display, adjust no-op
   ./build/tests/select_screen_test  # existing modal tests still green (no regression)
   ```
6. **Screen back navigation** (AC4 shell behavior):
   ```bash
   ./build/tests/screen_manager_test   # ScreenId::Calibration + Back -> Select; back_navigates() true
   ```
7. **Regression**:
   ```bash
   ctest --test-dir build --output-on-failure   # 23/23; music_clock_test/metronome_sync_test/
   # judgment_engine_test/life_keeper_test/config_persistence_test unchanged
   ```
8. `git status` shows only new files under `src/timing/`, `src/audio/`, `src/screens/`, the CMake files,
   and the test files — edits limited to `screen.hpp`, `screen_manager.cpp`, `options_menu.*`,
   `select_screen.cpp`, `main.cpp`. No `src/input/`, `src/gameplay/`, or `src/chart/` changes.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Wizard drifts from the gameplay measurement path, making calibration meaningless | Reuse `music_time_for_event()` + `clock_.time_seconds()` verbatim; Task 10 case 1 asserts equality against the helper called directly | **In scope** |
| Wrong offset sign silently doubles the latency error | Derive from `music_clock.hpp` + `sync_test_harness` (`delta == offset`); explicit sign tests (Task 9 cases 3–4); flagged **OQ4** | **In scope** — flagged |
| A few mistaps skew the average | Wild rejection (`max_abs_delta`) + MAD outlier prune; Task 9 cases 5–6 | **In scope** |
| No audio device headless → saving a meaningless offset | Synthetic fallback refuses to write config and logs a warning; Task 10 case 6 | **In scope** |
| Generated click WAV pollutes the user data dir / fails to write | Write to `<data-dir>/calibration_click.wav`; on failure fall back to synthetic (no save); bounded file size (`beats × period`) | **In scope** — flagged OQ5 |
| Adding `ScreenId::Calibration` causes a `-Wswitch` warning or a missed navigation branch | Add the `screen_id_name` case and `default_back_navigates`/`handle_back` branch in one task; warning budget check in Validation | **In scope** |
| `options_menu_apply` clobbers a wizard-written offset with the menu's stale copy | `offset_seconds` is **display-only** in `OptionsMenu`; `options_menu_apply` never writes `config.offset`; asserted in Task 11 | **In scope** |
| Mid-run wizard-computed offset is lost on a hard kill before exit | Same clean-exit persistence model as C4; AC only requires it written to config (and it is, immediately) | **Out of scope** — flagged |
| Count-in/attract interference: idle-attract fires during the wizard | Attract policy only triggers on Title/Select; Calibration is neither, so it cannot fire | **In scope** — by construction |
| Beat-schedule/`min_samples` values are not sourced from OpenITG | One `CalibrationConfig` struct; changing them touches only `offset_calibration.*` + its test; flagged **OQ1/OQ3** | **Flagged** |
| Calibrating against a WAV click vs in-game music may differ (mixer path) | Same `SoundStream`/miniaudio path as gameplay; document the residual difference; flag for a future "calibrate on real song" mode | **Flagged OQ5** |

---

## Decisions

- **New `ScreenId::Calibration`, not an inline Select sub-state.** The wizard owns its own audio/clock
  lifecycle and multi-phase UI; a dedicated screen matches the C1 state machine and keeps Select the
  owner of the options overlay. Back is the manager's default (Calibration → Select) so abort is
  inherently config-silent.
- **Entry row in the C4 options menu, activated by Confirm/Right.** Row enum + name/value text are the
  documented C5 seam; `options_menu_adjust` stays `void` and becomes a no-op for the action row, so
  existing tests compile unchanged.
- **Pure `OffsetCalibration` in `src/timing/`.** Keeps the statistical/sign logic out of the screen and
  free of SDL/GL/audio, so the hard parts are unit-tested headless (AGENTS.md principles 1 & 3).
- **Reuse `music_time_for_event` rather than reimplement aging.** The AC explicitly requires the same
  path as gameplay; the header is already pure and included by `GameplayView`.
- **Generated click WAV through `SoundStream`.** No bundled asset, no binary blob in the repo, and the
  clock stays bound to a real stream position exactly as gameplay. `Metronome(IAudioStream&)` is the
  test seam.
- **Immediate config write on Confirm; clean-exit persistence.** Reuses B1's
  `gameplay_options_from_config` and `main`'s existing `save_config`; no new context field or save call.
- **Synthetic fallback never saves.** Prevents recording a meaningless offset when no audio device is
  present, while keeping the wizard constructible/updatable headless.

---

## Open Questions

1. **Non-blocking — sample count.** PRD/issue do not specify. Proposed `min_samples = 8`,
   `max_samples = 32`. Rationale: enough samples to average out human jitter, few enough to keep the
   wizard under ~20 s at 120 BPM; plus a hard cap so the screen always terminates. Confirm or supply a
   target (OpenITG/ITG-style wizards typically use ~16 taps).
2. **Non-blocking — outlier rejection.** PRD/issue silent. Proposed robust MAD pruning
   (`|d - median| <= max(3·MAD, 0.05 s)`) plus a per-tap wild cap (`|delta| > 0.25 s` rejected).
   Rationale: robust to a few gross mistaps without discarding legitimate variance; no OpenITG source
   exists for this (calibration is not an OpenITG feature). Confirm the policy/thresholds.
3. **Non-blocking — target beat source and defaults.** Proposed an analytic 120 BPM schedule with a
   2 s lead-in and 64 max beats, rather than a simfile/chart. Rationale: "steady beat" is constant by
   definition; avoids a library/asset dependency. Confirm BPM/lead-in, or specify reusing a bundled
   metronome chart.
4. **Non-blocking — offset sign convention.** The issue/PRD do not state it. Derived proposal:
   `new_offset = -mean(hit - beat)` (positive offset = clock reads later, per `music_clock.hpp`; a late
   player needs a negative offset). Cross-checked against `sync_test_harness`. Confirm this is the
   intended Tundra convention before pinning tests.
5. **Non-blocking — headless beat production.** Proposed a generated click WAV played through the
   existing `SoundStream`, with a synthetic `fixed_dt`-advanced fallback that refuses to save. Tests
   inject a fake `IAudioStream` + fake `MusicClock::Source` and never open a device. Alternative: a
   committed `assets/metronome.wav`, or calibrate against a real song via a preview-style player.
   Confirm the preferred beat source and whether calibrating to in-game music (mixer path) is wanted.
6. **Non-blocking — save timing.** The AC says "written to config.json"; proposed writing
   `ctx.config` on Confirm and relying on the existing clean-exit save (same as C4). Say if an
   immediate save-on-confirm (needing the config path in `ScreenContext`) is required.

---

## Acceptance Criteria

- [ ] Starting from the options menu (`CALIBRATE OFFSET` row → Confirm/Right), the wizard plays a
      steady beat and prompts the user to tap (Tasks 2/4/5/6; OQ5)
- [ ] After enough samples the average hit delta is computed from music-clock time and input event
      timestamps via the exact gameplay path (`music_time_for_event`) (Tasks 1/5; Task 10 case 1)
- [ ] On Confirm the offset is written to `config.json` and applied to the gameplay clock through B1's
      existing `gameplay_options_from_config` path (Tasks 5/7; Task 10 cases 2–4; E2E 2–3)
- [ ] On cancel/abort the previous offset is retained unchanged (Task 5 `exit`; Task 10 case 5; E2E 2)
- [ ] `ctest --test-dir build --output-on-failure` → **23/23**; miss/`metronome_sync_test`/
      `music_clock_test`/`config_persistence_test` and `--gameplay-demo` stay green (Task 11; E2E 7)
- [ ] `src/timing/offset_calibration.*` stays SDL/GL/audio/clock-free; zero new warnings under
      `-Wall -Wextra -Wpedantic` (Validation)
- [ ] Open Questions OQ1–OQ5 confirmed or defaults accepted (sample count, outlier rejection, beat
      source, offset sign, headless beat)
