# Implementation Report

**Plan**: `.agents/plans/009-music-clock-plan.md`
**Branch**: `feature/009-music-clock`
**Status**: COMPLETE

## Summary

Implemented the timing subsystem (`src/timing/`) that derives gameplay time exclusively from the
audio stream's PCM frame cursor. `MusicClock` holds an injectable `std::function<SamplePosition()>`
source and a global offset, computing `time_seconds = frames / sample_rate + global_offset_seconds`
(the literal PRD formula; positive offset = clock later). Added additive raw accessors to
`SoundStream` (`get_position_frames()`, `get_sample_rate()`) and refactored `get_position_seconds()`
to share the frame cursor / monotonic clamp as a single source of truth. No config persistence or App
wiring was added (deferred to C4/C5).

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Expose raw sample position from `SoundStream` | `src/audio/sound_stream.hpp`, `src/audio/sound_stream.cpp` | ✅ |
| 2 | Create `MusicClock` header | `src/timing/music_clock.hpp` | ✅ |
| 3 | Create `MusicClock` implementation | `src/timing/music_clock.cpp` | ✅ |
| 4 | Register sources and test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 5 | Create deterministic + integration tests | `tests/music_clock_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure | ✅ (`cmake -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=...`) |
| Build (`cmake --build build -j16`) | ✅ zero warnings under `-Wall -Wextra -Wpedantic` |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 9/9 passed (8 prior + `music_clock_test`) |
| Explicit test (`./build/tests/music_clock_test`) | ✅ audio integration ran; `t=0.01875s` advanced from 0 |
| Forbidden deps (`rg -n "chrono|thread|SDL_GetPerformanceCounter|frame_dt" src/timing/`) | ✅ no matches (exit 1) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/music_clock.hpp` | CREATE | +56 |
| `src/timing/music_clock.cpp` | CREATE | +66 |
| `tests/music_clock_test.cpp` | CREATE | +174 |
| `src/audio/sound_stream.hpp` | UPDATE | +2 |
| `src/audio/sound_stream.cpp` | UPDATE | +12/-5 |
| `CMakeLists.txt` | UPDATE | +1 |
| `tests/CMakeLists.txt` | UPDATE | +10 |

## Deviations from Plan

- `SoundStream::get_position_seconds()` now computes `get_position_frames() / sample_rate_` instead of
  returning the clamped `last_position_` double directly. Behavior is equivalent within double rounding
  and preserves the single monotonic clamp (`last_position_`). Existing `audio_test` position/seek/stop
  assertions remain green. This is the plan's "optional but preferred" shared-source refactor.
- The header's no-forbidden-deps doc comment was reworded to avoid the literal tokens `chrono`/`thread`
  so the plan's architecture grep (`rg ... src/timing/`) returns no matches. No code change; comment only.
- No other deviations. `apply_offset` is `sample_seconds + offset_seconds` per the user's resolved
  decision (Blaze 4k sign, positive = later). Non-finite offsets reset to `0.0` (chosen documented behavior).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/music_clock_test.cpp` | 1. frame math + zero-rate guard; 2. positive/negative offset direction; 3. default/uncalibrated clock; 4. frame-hitch independence (1000 identical reads); 5. monotonic advance by exact frame delta; 6. non-finite offset rejection (NaN/Inf); 7. nanosecond conversion; 8. guarded audio integration via `SoundStream` adapter |

## End-to-End Verification

1. ✅ `./build/tests/music_clock_test` — audio integration sub-test ran (device present) and reported the
   clock advancing from 0 (`t=0.01875s`).
2. ✅ `ctest` → 9/9, no regression in the 8 prior tests.
3. ✅ Forbidden-dependency grep over `src/timing/` → no matches.
4. ✅ Offset direction/shift covered by test 2 (+0.050 / -0.050 shifts `time_seconds()` exactly).
