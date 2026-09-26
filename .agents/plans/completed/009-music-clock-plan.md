# Plan: Music-Driven Gameplay Clock with Global Offset

## Summary

Implement the timing subsystem under `src/timing/` that makes gameplay time derive **exclusively**
from the audio stream's PCM frame cursor — never wall-clock, never frame delta (PRD §6 pattern 1).

The design separates pure clock math from audio hardware so it can be unit-tested deterministically:

1. `MusicClock` holds a pluggable `SamplePosition{uint64_t frames, uint32_t sample_rate}` source and a
   `global_offset_seconds` value. `time_seconds() = frames / sample_rate + global_offset_seconds`.
2. Production wiring is an adapter lambda over a `SoundStream`; tests inject fake frame positions.
3. Additive accessors `SoundStream::get_position_frames()` / `get_sample_rate()` expose the raw
   sample data needed to compute `frames ÷ rate` explicitly (the existing `get_position_seconds()`
   already performs this division internally; the clock re-expresses it to match the AC literally).

No config persistence is added (that is Phase C / issue C4). The clock exposes
`set_global_offset_seconds()` / `global_offset_seconds()`; the future config and calibration wizard
(C5) will call these. `offset = 0` is the sane default (AC4).

## User Story

As a competitive player
I want gameplay time derived from the audio stream position plus a calibrated global offset
So that notes scroll and judge in perfect sync with the music — never drifting with frame timing.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/timing/`, `src/audio/` (additive accessors), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #9 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3, glad, miniaudio, nlohmann_json, stb already fetched |
| Baseline tests | 8/8 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 8" (0.13s) |
| Audio test | `tests/audio_test.cpp` | Uses `MA_SOUND_FLAG_DECODE`; position monotonic clamp proven; headless fallback handled |

**Start green, stay green:** 8 tests currently pass; this plan adds 1 new test target (`music_clock_test`) → 9 expected.

---

## Pinned Semantics

Authoritative formula for this project — from the issue AC and PRD §6 pattern 1 (Tundra Dance's own
locked spec), **not** copied from StepMania's sign convention:

```
gameplay_time_seconds = (pcm_cursor_frames / sample_rate) + global_offset_seconds
```

| Semantic | Value / Source | Notes |
|----------|----------------|-------|
| Position source | `ma_sound_get_cursor_in_pcm_frames` — `src/audio/sound_stream.cpp:135-137` | Already used by `SoundStream::get_position_seconds()` |
| Sample rate source | Sound data-format rate, fallback engine rate — `src/audio/sound_stream.cpp:69-74` | Sound is fully decoded (`MA_SOUND_FLAG_DECODE`, line 57) |
| Default offset | `0.0 s` (uncalibrated) | Issue AC4; StepMania/ITG defaults its own pref to `-0.008f` (`PrefsManager.cpp`) |
| Offset application | **Add**: `raw_sample_seconds + offset` | PRD §6 pattern 1 / issue AC literally state "plus global offset" |
| StepMania reference (for contrast) | `GetElapsedTimeFromBeat = GetElapsedTimeFromBeatNoOffset - m_fMusicRate * m_fGlobalOffsetSeconds` — `stepmania/stepmania:sr/TimingData.cpp` (via GitHub code search) | Tundra's stored value sign is **not** StepMania's; see Open Questions |
| Excluded inputs | No `<chrono>`, `<thread>`, `SDL_GetPerformanceCounter`, or frame-delta in `src/timing/` | Core principle 1 / PRD §11 quality indicator |

---

## Patterns to Follow

### Naming
```cpp
// SOURCE: src/audio/sound_stream.hpp:29-36
bool seek_seconds(double seconds);
[[nodiscard]] double get_position_seconds() const;
[[nodiscard]] double get_length_seconds() const;
[[nodiscard]] bool is_playing() const;
[[nodiscard]] bool is_loaded() const { return is_loaded_; }
```
Use `snake_case` methods, `get_*` for queries, `[[nodiscard]]` on pure getters, `_seconds` suffixes for units.

### Error Handling
```cpp
// SOURCE: src/audio/sound_stream.cpp:63-67
if (result != MA_SUCCESS) {
    std::cerr << "[SoundStream] Failed to load audio file '" << filepath
              << "' (error code: " << static_cast<int>(result) << ")\n";
    return false;
}
```
Tagged `std::cerr` log lines prefixed `[ModuleName]`; return `false`/safe value rather than throwing.

### Position Query (existing clock basis)
```cpp
// SOURCE: src/audio/sound_stream.cpp:130-145
double SoundStream::get_position_seconds() const {
    if (!is_loaded_ || !sound_) return 0.0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(sound_.get(), &cursor) == MA_SUCCESS && sample_rate_ > 0) {
        double pos = static_cast<double>(cursor) / static_cast<double>(sample_rate_);
        if (is_playing() && pos < last_position_) return last_position_; // monotonic while playing
        last_position_ = pos;
        return pos;
    }
    return last_position_;
}
```

### Tests
```cpp
// SOURCE: tests/audio_test.cpp:57-63
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)
```
Plain `int main()` test binaries, `TEST_CHECK` macro, `std::cerr` diagnostics, exit non-zero via `std::abort()`.

### Test target registration
```cmake
# SOURCE: tests/CMakeLists.txt:22-30
add_executable(audio_test
    audio_test.cpp
)
target_link_libraries(audio_test PRIVATE
    tundra_core
)
add_test(NAME audio_test COMMAND audio_test)
```

### Source registration
```cmake
# SOURCE: CMakeLists.txt:80-92
add_library(tundra_core STATIC
    src/app/window.cpp
    ...
    src/chart/song_library.cpp
)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/music_clock.hpp` | CREATE | `SamplePosition` struct + `MusicClock` interface (source injection, offset, time queries, pure helpers) |
| `src/timing/music_clock.cpp` | CREATE | Clock math implementation + safe guards |
| `src/audio/sound_stream.hpp` | UPDATE | Add `get_position_frames()` and `get_sample_rate()` accessors (additive; no behavior change) |
| `src/audio/sound_stream.cpp` | UPDATE | Implement the two accessors |
| `CMakeLists.txt` | UPDATE | Add `src/timing/music_clock.cpp` to `tundra_core` |
| `tests/music_clock_test.cpp` | CREATE | Deterministic clock-math + offset-sign + jitter-independence tests; guarded audio integration sub-test |
| `tests/CMakeLists.txt` | UPDATE | Register `music_clock_test` |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Expose raw sample position from `SoundStream`

- **Files**: `src/audio/sound_stream.hpp`, `src/audio/sound_stream.cpp`
- **Action**: UPDATE
- **Implement**:
  - `[[nodiscard]] uint64_t get_position_frames() const;` — returns the PCM frame cursor
    (`ma_sound_get_cursor_in_pcm_frames`), `0` when unloaded; applies the same monotonic clamp used
    by `get_position_seconds()` (keep `last_position_` semantics consistent — do **not** create a
    second competing clamp; reuse a single internal helper if cleaner).
  - `[[nodiscard]] uint32_t get_sample_rate() const { return sample_rate_; }`.
  - Refactor `get_position_seconds()` to call `get_position_frames()` internally so both share one
    source of truth (optional but preferred).
  - Purely additive; no existing callers' behavior changes.
- **Mirror**: `src/audio/sound_stream.cpp:130-145`
- **Validate**: `cmake --build build -j16` then `ctest --test-dir build --output-on-failure` (8/8 must stay green).

### Task 2: Create `src/timing/music_clock.hpp`

- **File**: `src/timing/music_clock.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace td {
  struct SamplePosition {
      uint64_t frames = 0;
      uint32_t sample_rate = 44100;
  };

  class MusicClock {
  public:
      using Source = std::function<SamplePosition()>;

      MusicClock() = default;
      explicit MusicClock(Source source);

      void set_source(Source source);            // injectable: fake in tests, SoundStream in prod
      void clear_source();
      [[nodiscard]] bool has_source() const;

      void set_global_offset_seconds(double offset);   // rejects non-finite -> 0.0
      [[nodiscard]] double global_offset_seconds() const;

      [[nodiscard]] SamplePosition sample_position() const;
      [[nodiscard]] double sample_time_seconds() const; // frames / rate
      [[nodiscard]] double time_seconds() const;        // frames / rate + offset
      [[nodiscard]] int64_t time_nanoseconds() const;   // round(time_seconds * 1e9)

      static double seconds_from_pcm(uint64_t frames, uint32_t sample_rate);
      static double apply_offset(double sample_seconds, double offset_seconds);
  };
  } // namespace td
  ```
  - `#include <cstdint>`, `<functional>`, `<cmath>` only — **no `<chrono>`/`<thread>`/SDL/miniaudio**.
  - Document the sign convention in a header comment: positive offset makes the clock read later.
  - Document the production binding pattern in a comment:
    `clock.set_source([&]{ return SamplePosition{s.get_position_frames(), s.get_sample_rate()}; });`
- **Mirror**: `src/audio/sound_stream.hpp:1-34` (interface style)
- **Validate**: `cmake --build build -j16`

### Task 3: Create `src/timing/music_clock.cpp`

- **File**: `src/timing/music_clock.cpp`
- **Action**: CREATE
- **Implement**:
  - `seconds_from_pcm`: return `0.0` if `sample_rate == 0`; otherwise
    `static_cast<double>(frames) / static_cast<double>(sample_rate)`.
  - `apply_offset`: `sample_seconds + offset_seconds`.
  - `sample_position()`: return stored source result, or `SamplePosition{}` when `source_` is empty.
  - `time_seconds()`: `apply_offset(sample_time_seconds(), global_offset_seconds_)`.
  - `set_global_offset_seconds`: `std::isfinite(offset) ? offset : 0.0`; log a `[MusicClock]` warning
    on rejection (mirroring the `std::cerr` house pattern).
  - `time_nanoseconds()`: `static_cast<int64_t>(std::llround(time_seconds() * 1e9))`.
  - No caching of time across calls; each `time_seconds()` re-reads the source (audio position is the
    only authority — no interpolation, no frame delta).
- **Mirror**: `src/audio/sound_stream.cpp:116-157`
- **Validate**: `cmake --build build -j16 -Werror` is **not** used; ensure zero warnings under existing `-Wall -Wextra -Wpedantic`.

### Task 4: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root `CMakeLists.txt`: add `src/timing/music_clock.cpp` to the `tundra_core` source list (after
    the `src/chart/` entries, line ~91).
  - `tests/CMakeLists.txt`: append a `music_clock_test` executable linking `tundra_core` and
    `add_test(NAME music_clock_test COMMAND music_clock_test)` (mirror the `audio_test` block).
- **Mirror**: `CMakeLists.txt:80-92`, `tests/CMakeLists.txt:22-30`
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`

### Task 5: Create `tests/music_clock_test.cpp`

- **File**: `tests/music_clock_test.cpp`
- **Action**: CREATE
- **Implement** (use the `TEST_CHECK` macro; deterministic, hardware-independent primary tests):
  1. **Frame math**: `seconds_from_pcm(44100, 44100) == 1.0 ± 1e-9`; `seconds_from_pcm(0, rate) == 0.0`;
     `seconds_from_pcm(100, 0) == 0.0` (guarded).
  2. **Offset applied**: with a fake static source `{48000 frames, 48000 Hz}` and offset `0.0`,
     `time_seconds() == 1.0`; with offset `+0.050`, `time_seconds() == 1.050`;
     with offset `-0.050`, `time_seconds() == 0.950`. This is the positive/negative direction AC.
  3. **Zero/uncalibrated default**: a default-constructed `MusicClock` (no source) returns
     `time_seconds() == 0.0` and `sample_time_seconds() == 0.0`; setting a source with offset `0` still
     functions (AC4).
  4. **Frame-hitch independence**: drive a mutable fake source; advance frames manually; assert the
     clock value depends **only** on the provider output and offset — calling `time_seconds()` many
     times between frame changes returns an identical value (proves no wall-clock/frame-delta input).
  5. **Monotonic advance**: advance the fake source from `1000 → 2000` frames and assert time strictly
     increases by `1000 / rate`.
  6. **Non-finite offset rejection**: `set_global_offset_seconds(NaN)` and `(Inf)` leave offset at `0.0`
     (or previous finite value per implementation choice — assert the chosen documented behavior).
  7. **Nanoseconds**: `time_nanoseconds()` for `1.0s + 0.5s offset == 1'500'000'000`.
  8. **Guarded audio integration (End-to-End)**: generate a short WAV in a temp dir (reuse the
     `write_test_wav` helper pattern from `tests/audio_test.cpp:14-55`), load via `SoundStream`, bind
     the clock with the lambda adapter, play, and assert `time_seconds()` advances from ~0. If
     `AudioEngine::instance().init()` returns `false` (headless), print a skip line and do not fail.
- **Mirror**: `tests/audio_test.cpp:57-142`
- **Validate**: `ctest --test-dir build --output-on-failure`

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 9/9: 8 existing + music_clock_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/music_clock_test
```

## End-to-End Verification

1. Build and run `./build/tests/music_clock_test` — the guarded integration sub-test reports either
   `time advanced from 0` (audio device present) or a `[music_clock_test] skipping audio integration
   (no audio device)` line (headless). Both are passes.
2. Run `ctest --test-dir build --output-on-failure` and confirm **9/9** pass; verify all 8 prior tests
   remain green (no regression from the `SoundStream` accessor addition).
3. Grep the new module for forbidden dependencies to enforce the architecture:
   `rg -n "chrono|thread|SDL_GetPerformanceCounter|frame_dt" src/timing/` → **no matches**.
4. Manually reason through / add a scratch assertion that changing global offset by `+20 ms` shifts
   `time_seconds()` by exactly `+0.020` (already covered by test 2).

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Offset sign is ambiguous versus StepMania/OpenITG (StepMania uses `- m_fMusicRate * m_fGlobalOffsetSeconds`, default `-0.008f`) | Pin Tundra's own formula `raw + offset` from PRD §6 / issue AC; document sign explicitly; surface in Open Questions for human confirmation | **In scope** — decision recorded in plan |
| Config module does not exist yet (C4), so there is no `config.json` to read an offset from | Clock owns a `double` with setter/getter and default `0.0`; C4 wires persistence, C5 wires calibration. No config code in this issue | **In scope** — setter only |
| Hardware-dependent test flakiness in CI/headless | Primary tests use injected fake `SamplePosition` sources (deterministic); audio integration test skips when `AudioEngine::init()` fails | **In scope** |
| `SoundStream` API change ripples to existing tests | Accessors are purely additive; run full 8-test suite after Task 1 | **In scope** |
| Double precision drift over a full song | PCM frame counts stay far below 2^53; seconds computed once per query from integer frames; negligible | **Out of scope** — documented only |
| Pause/seek semantics differ from a naive monotonic clock | Clock does not force monotonicity; it reflects the cursor, so pause freezes and seek jumps (correct for judgment after seek). SoundStream already clamps only during playback | **Out of scope** — documented only |

---

## Decisions

- **Clock is audio-source-agnostic**: `MusicClock` stores a `std::function<SamplePosition()>`, not a
  `SoundStream*`/miniaudio handle. This satisfies "unit-test the clock math independently of audio
  hardware" and keeps `src/timing/` free of platform includes. Production binding is a one-line lambda.
- **Explicit frames ÷ rate**: add raw accessors to `SoundStream` rather than reusing
  `get_position_seconds()`, so the AC is implemented literally and sample-exactness is preserved.
- **Offset stored with Tundra sign**: `time = raw + offset`; positive offset moves the clock later.
- **No monotonic enforcement / no caching in the clock**: the audio cursor is the only authority;
  caching or interpolation would reintroduce frame-timing logic.
- **No config persistence, no App integration** in this issue; downstream B3/B4 consume the clock.

---

## Open Questions

1. **Offset sign / stored-value convention (needs human decision).** PRD §6 pattern 1 and issue AC say
   `+ global offset`, but StepMania's authoritative code applies `- m_fMusicRate * m_fGlobalOffsetSeconds`
   and defaults the pref to `-0.008f`. Proposed default: implement `raw + offset` (positive = later),
   store `0.0`, and have C5 write Tundra-signed values. *Rationale:* the issue AC is explicit and this is
   a bespoke engine; importing StepMania's negative pref value would invert behavior. Confirm before C5.
2. **Where the offset value comes from pre-C4.** Proposed: `MusicClock::set_global_offset_seconds()`
   with default `0.0`, wired to `config.json` in C4. Confirm no minimal config loader is expected here.
3. **Pause semantics.** Proposed: clock freezes while paused (cursor stops) with no separate pause
   offset. Confirm this matches intended gameplay HUD behavior.
4. **Offset domain.** Proposed: the offset shifts the gameplay clock only; chart/note times remain in
   their parsed (`#OFFSET`-baked) domain, so `delta = gameplay_time - note_time`. Confirm against
   StepMania's note-time domain in B4.

---

## Acceptance Criteria

- [ ] `MusicClock::time_seconds()` returns `pcm_frames / sample_rate + global_offset_seconds`
- [ ] Framerate/vsync/frame hitches cannot affect the clock (no wall-clock or frame-delta in `src/timing/`)
- [ ] Positive and negative global offsets shift `time_seconds()` in the correct (documented) direction
- [ ] Offset `0` (uncalibrated) yields a functioning clock with sane defaults
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 9/9 pass (8 prior + `music_clock_test`)
- [ ] Follows existing module/naming/test/CMake patterns
