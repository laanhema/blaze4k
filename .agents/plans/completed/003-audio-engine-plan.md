# Plan: Audio Engine with Sample-Exact Stream Position Query

## Summary

Implement the audio subsystem in `src/audio/` wrapping miniaudio:
1. `src/audio/audio_engine.hpp` and `src/audio/audio_engine.cpp`:
   - Initialize and manage the miniaudio `ma_engine` / `ma_context` / `ma_device`.
   - Manage master volume and provide resource lifetime management.
   - Support graceful fallback to null/silent device if audio hardware is unavailable.
2. `src/audio/sound_stream.hpp` and `src/audio/sound_stream.cpp`:
   - Load and decode audio files (WAV, MP3, OGG, FLAC) using miniaudio `ma_sound` or `ma_decoder`.
   - Provide playback controls: `play()`, `stop()`, `pause()`, `resume()`, `seek_seconds(double)`, `set_volume(float)`.
   - Provide sample-exact position query: `get_position_seconds()` computing `ma_sound_get_time_in_pcm_frames() / sample_rate` (or PCM cursor division).
   - Ensure monotonic position progression and graceful failure handling for missing/corrupted files.
3. Unit tests in `tests/audio_test.cpp`:
   - Generate synthetic test WAV files using standard PCM WAV header writing.
   - Test loading, duration query, playback, monotonic stream position advancement, pausing, seeking, volume, and graceful rejection of non-existent/corrupt files.
4. Integrate with `blaze4k_core` in `CMakeLists.txt` and tests.

## User Story

As a developer,
I want an audio wrapper that plays song files and exposes the exact stream position,
So that the gameplay clock can be driven by the audio hardware position instead of wall time.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/audio/`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #3 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/audio/audio_engine.hpp` | CREATE | Audio device/context management interface |
| `src/audio/audio_engine.cpp` | CREATE | Miniaudio engine implementation |
| `src/audio/sound_stream.hpp` | CREATE | Stream playback and sample-exact clock query interface |
| `src/audio/sound_stream.cpp` | CREATE | Miniaudio stream decoder and cursor tracking |
| `CMakeLists.txt` | UPDATE | Add audio sources to `blaze4k_core` |
| `tests/audio_test.cpp` | CREATE | Unit tests for sound stream playback, position tracking, and error handling |
| `tests/CMakeLists.txt` | UPDATE | Add `audio_test` target |

---

## Tasks

### Task 1: Create `AudioEngine`
- **Files**: `src/audio/audio_engine.hpp`, `src/audio/audio_engine.cpp`
- **Action**: CREATE
- **Implement**:
  - `AudioEngine` singleton / instance wrapping `ma_engine`.
  - `init()` initializing `ma_engine` with default playback device (or fallback to dummy/null backend on headless systems).
  - `shutdown()` cleanly uninitializing miniaudio resources.
  - Volume control and status query (`is_initialized()`).

### Task 2: Create `SoundStream`
- **Files**: `src/audio/sound_stream.hpp`, `src/audio/sound_stream.cpp`
- **Action**: CREATE
- **Implement**:
  - `load(const std::string& filepath)`: returns `bool`, handles missing or corrupt files gracefully with error logging.
  - `play()`, `stop()`, `pause()`, `resume()`, `seek_seconds(double seconds)`.
  - `get_position_seconds()`: returns `double` sample position ÷ sample rate. Ensures position is monotonic during playback.
  - `get_length_seconds()`, `is_playing()`, `set_volume(float vol)`.
  - Clean RAII destructor releasing `ma_sound`.

### Task 3: Update `CMakeLists.txt`
- **Files**: `CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Add `src/audio/audio_engine.cpp` and `src/audio/sound_stream.cpp` to `blaze4k_core`.

### Task 4: Unit tests in `tests/audio_test.cpp`
- **Files**: `tests/audio_test.cpp`, `tests/CMakeLists.txt`
- **Action**: CREATE / UPDATE
- **Implement**:
  - Create a valid 1-second 44.1kHz mono sine-wave WAV file programmatically in a temp directory.
  - Verify `SoundStream::load` succeeds on valid file, queries length ~1.0s.
  - Verify playback starts and `get_position_seconds()` progresses monotonically.
  - Verify seeking works accurately.
  - Verify non-existent file returns `false` without crashing.
  - Verify corrupt file returns `false` without crashing.

### Task 5: Validate and End-to-End Verification
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

---

## Validation

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- [ ] Supported audio formats (WAV, MP3, OGG) load and play
- [ ] `get_position_seconds()` returns sample position / sample rate monotonically
- [ ] Missing or corrupt file fails gracefully with error log, no crash
- [ ] Play, pause, stop, seek, and volume APIs are functional
- [ ] All unit tests pass
