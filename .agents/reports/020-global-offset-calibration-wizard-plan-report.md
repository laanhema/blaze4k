# Implementation Report

**Plan**: `.agents/plans/020-global-offset-calibration-wizard-plan.md`
**Branch**: `feature/020-offset-calibration-wizard`
**Issue**: #20 ([C5]) Global offset calibration wizard
**Status**: COMPLETE

## Summary

Added a guided tap-to-the-beat calibration wizard as a first-class
`ScreenId::Calibration`, reachable from the C4 options menu (`CALIBRATE OFFSET`
row → Confirm/Right). A pure `OffsetCalibration` model owns the analytic
120 BPM beat schedule and robust (MAD) offset estimation; a new `Metronome`
generates a 16-bit mono PCM click-track WAV in-memory, plays it through the
existing `SoundStream`, and exposes the stream as a `MusicClock::Source`. The
screen binds that clock exactly as `GameplayView` does and ages every panel press
through the **same** `music_time_for_event()` helper, so calibration measures the
identical music-clock + input-event-timestamp path as real gameplay. Confirm
writes `config.offset.global_offset_seconds` (applied by B1's existing
`gameplay_options_from_config` and persisted by `main`'s clean-exit
`save_config`); Back aborts through the manager's default navigation and never
writes the config.

## Resolved decisions (user overrides)

1. **OQ4 sign** — `new_offset = -mean(hit - beat)`, consistent with
   `music_clock`'s `time_seconds = frames/rate + offset` ("positive offset = clock
   later"). Pinned in `offset_calibration_test` (late → negative, early →
   positive).
2. **OQ1/OQ2 numerics** — `min_samples=8`, `max_samples=32`, wild cap
   `0.25 s`, MAD ×3 with a `0.05 s` floor, all `CalibrationConfig` constants.
3. **OQ3/OQ5 beat source** — analytic 120 BPM schedule with a 2 s lead-in and 64
   beats; in-memory generated click WAV through the existing `SoundStream`.
   Headless tests inject a fake `IAudioStream` + fake `MusicClock::Source`; the
   synthetic/degraded path refuses to save and logs
   `[Calibration] audio unavailable; offset not saved`.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure `OffsetCalibration` model | `src/timing/offset_calibration.{hpp,cpp}` | ✅ |
| 2 | `Metronome` + click-track synthesis | `src/audio/metronome.{hpp,cpp}` | ✅ |
| 3 | `ScreenId::Calibration` + back navigation | `src/screens/screen.hpp`, `screen_manager.cpp` | ✅ |
| 4 | Options-menu entry row | `src/screens/options_menu.{hpp,cpp}` | ✅ |
| 5 | `CalibrationScreen` | `src/screens/calibration_screen.{hpp,cpp}` | ✅ |
| 6 | Wire entry from `SelectScreen` | `src/screens/select_screen.cpp` | ✅ |
| 7 | Register wizard in `main` | `src/main.cpp` | ✅ |
| 8 | Register sources + test targets | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 9 | `offset_calibration_test` (pure) | `tests/offset_calibration_test.cpp` | ✅ |
| 10 | `calibration_screen_test` (headless) | `tests/calibration_screen_test.cpp` | ✅ |
| 11 | Extend `options_menu_test` / `screen_manager_test` | `tests/*_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure | ✅ `cmake -B build -DCMAKE_BUILD_TYPE=Release` |
| Build (`-Wall -Wextra -Wpedantic`) | ✅ no warnings/errors |
| Tests | ✅ **23/23** (`ctest --test-dir build --output-on-failure`) |
| Purity (`rg` for SDL/glad/miniaudio/chrono/std::time in `offset_calibration.*`) | ✅ no matches |
| E2E 1 — real binary headless boot + select logs | ✅ exit 0, `[ScreenManager] enter Select` |
| E2E 2 — config offset survives clean-exit save unchanged | ✅ `0.017` retained |
| E2E 3 — wizard with injected clock | ✅ `calibration_screen_test` |
| E2E 4 — pure calibration math | ✅ `offset_calibration_test` |
| E2E 5 — options row + select regression | ✅ `options_menu_test`, `select_screen_test` |
| E2E 6 — screen back navigation | ✅ `screen_manager_test` |
| E2E 7 — full regression | ✅ 23/23 |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/offset_calibration.hpp` | CREATE | +73 |
| `src/timing/offset_calibration.cpp` | CREATE | +117 |
| `src/audio/metronome.hpp` | CREATE | +61 |
| `src/audio/metronome.cpp` | CREATE | +160 |
| `src/screens/calibration_screen.hpp` | CREATE | +68 |
| `src/screens/calibration_screen.cpp` | CREATE | +189 |
| `tests/offset_calibration_test.cpp` | CREATE | +194 |
| `tests/calibration_screen_test.cpp` | CREATE | +371 |
| `src/screens/screen.hpp` | UPDATE | +2/-1 |
| `src/screens/screen_manager.hpp` | UPDATE | +2/-1 |
| `src/screens/screen_manager.cpp` | UPDATE | +5/-1 |
| `src/screens/options_menu.hpp` | UPDATE | +8 |
| `src/screens/options_menu.cpp` | UPDATE | +16 |
| `src/screens/select_screen.cpp` | UPDATE | +22 |
| `src/main.cpp` | UPDATE | +3 |
| `CMakeLists.txt` | UPDATE | +3 |
| `tests/CMakeLists.txt` | UPDATE | +20 |
| `tests/options_menu_test.cpp` | UPDATE | +32/-2 |
| `tests/screen_manager_test.cpp` | UPDATE | +14 |

No changes to `src/input/`, `src/gameplay/*` (judgment/scoring), `src/chart/`,
`src/timing/music_clock.*`, or `src/data/config*` field layout.

## Deviations from Plan

1. **`Metronome::start()` + `is_playing()` fallback** — the plan bound the clock
   to the owned stream whenever `prepare()` succeeded. On a device-less host
   `load()` can succeed while `play()` fails, which would leave the clock stuck at
   t=0. `CalibrationScreen::enter()` therefore treats `!metronome_.is_playing()`
   after `start()` as the audio-unavailable case and falls back to the synthetic
   clock. This strengthens the headless-safety requirement without changing the
   production path.
2. **Task-2 test coverage** — the plan listed no dedicated metronome test target,
   but the skill requires a test per new function. The click-track synthesis and
   `Metronome` clock source are covered by `calibration_screen_test`
   (`test_click_track_and_metronome`), which validates the RIFF/WAVE header, the
   stub fallback for an empty path, load/start/stop, and the frames-from-position
   clock source.
3. **Test-seam constructor semantics** — the plan's seam takes
   `(IAudioStream&, MusicClock::Source, CalibrationConfig)`. The `source` is now
   allowed to be empty; when it is, the supplied stream is handed to the
   metronome so a fake *failing* stream exercises the production synthetic
   no-audio path (Task 10 case 6) without injecting a source.

## Tests Written

| Test File | Test Cases |
|-----------|-----------|
| `tests/offset_calibration_test.cpp` | beat schedule + nearest index; zero bias; late→negative sign (OQ4); early→positive sign; wild rejection; MAD outlier prune; readiness at `min_samples` + cap at `max_samples`; degenerate empty/all-wild; determinism |
| `tests/calibration_screen_test.cpp` | measurement via `music_time_for_event` (aged timestamps); Confirm saves + transitions; B1 `gameplay_options_from_config` application; save/load round-trip; Back abort retains prior offset; synthetic no-audio refuses to save; zero/future timestamps safe; headless render + re-enter reset; click-track synthesis + metronome clock source |
| `tests/options_menu_test.cpp` (updated) | new clamp bound 0..4; `CALIBRATE OFFSET` name/value via `format_offset`; display-only seeding; adjust no-op; `options_menu_apply` does not clobber offset |
| `tests/screen_manager_test.cpp` (updated) | `ScreenId::Calibration` back_navigates + Back → Select |

## Acceptance Criteria

- [x] Options menu → `CALIBRATE OFFSET` → wizard plays a steady beat and prompts taps
- [x] Average hit delta computed from music-clock time + input event timestamps via `music_time_for_event` (Task 10 case 1)
- [x] Confirm writes `config.offset.global_offset_seconds`, applied via B1 and persisted on clean exit
- [x] Cancel/abort retains the previous offset unchanged
- [x] `ctest` → 23/23; existing tests green
- [x] `src/timing/offset_calibration.*` SDL/GL/audio/clock-free; zero new warnings
- [x] OQ1–OQ5 confirmed/decided by the user (see Resolved decisions)
