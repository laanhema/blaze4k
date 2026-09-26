# Implementation Report

**Plan**: `.agents/plans/018-song-select-wheel-banners-difficulties-preview-plan.md`
**Branch**: `feature/018-song-select-wheel`
**Status**: COMPLETE

## Summary

Replaced the C1 `SelectPlaceholderScreen` registration with a real `SelectScreen`: a flat,
pack-grouped wheel over the scanned `SongLibrary` with banner art, artist/BPM, passthrough
difficulty labels + foot ratings, and best grade per chart (via C2's `make_chart_key` /
`find_high_score`). Highlighting a song arms a delayed, looping `PreviewPlayer` (using
`SoundStream`); navigation cancels it. Confirm publishes a `PlayRequest` and transitions to a
new thin `GameplayScreen` that hosts the existing `GameplayView`; Back aborts Gameplay → Select.
Banners decode via a vendored `stb_image` TU behind `Texture::from_file` + `TextureCache`.
`main.cpp` gains `--songs` / `--start-screen`, scans the library, and wires the extended
`ScreenContext`. All new logic is headless-safe (no window/GL/audio device required).

Resolved open questions per the invoking request: OQ1 thin GameplayScreen (Results deferred to
C7), OQ2 vendored stb_image + 16 MiB / 4096 px caps + placeholder fallback, OQ9 Back = Gameplay
→ Select; all other OQs use the plan's proposed defaults.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Extract `GameplayOptions` + config mapping | `src/gameplay/gameplay_options.{hpp,cpp}`, `gameplay_view.hpp` | ✅ |
| 2 | `Texture::from_file` + stb_image TU | `src/render/texture.{hpp,cpp}`, `stb_image_impl.cpp` | ✅ |
| 3 | `TextureCache` | `src/render/texture_cache.{hpp,cpp}` | ✅ |
| 4 | `PreviewPlayer` | `src/audio/preview_player.{hpp,cpp}` | ✅ |
| 5 | `PlayRequest` + `ScreenContext` extension | `src/screens/play_request.hpp`, `screen.hpp` | ✅ |
| 6 | `SelectScreen` | `src/screens/select_screen.{hpp,cpp}` | ✅ |
| 7 | `GameplayScreen` | `src/screens/gameplay_screen.{hpp,cpp}` | ✅ |
| 8 | Back semantics for Gameplay | `src/screens/screen_manager.{hpp,cpp}` | ✅ |
| 9 | `main.cpp` scan + wiring + CLI | `src/main.cpp` | ✅ |
| 10 | Source + test registration | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 11 | `preview_player_test` | `tests/preview_player_test.cpp` | ✅ |
| 12 | `select_screen_test` | `tests/select_screen_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ |
| Warnings under `-Wall -Wextra -Wpedantic` (forced recompile of all changed TUs) | ✅ zero warnings/errors |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ **19/19 passed** |
| `./build/tests/select_screen_test` | ✅ exit 0 |
| `./build/tests/preview_player_test` | ✅ exit 0 |
| Purity `rg "SDL_\|glad\|miniaudio" src/screens/select_screen.* src/gameplay/gameplay_options.*` | ✅ no matches |
| `rg "STB_IMAGE_IMPLEMENTATION" src/render` | ✅ exactly 1 (stb_image_impl.cpp) |

### End-to-End Verification

| # | Step | Result |
|---|------|--------|
| 1 | `--headless --smoke-test 30 --start-screen select --songs tests/fixtures/reference_pack` | ✅ exit 0; `[SongLibrary] scanned '...': 4 songs, 9 charts`, `[SelectScreen] library: 4 songs, 9 charts`, `[ScreenManager] enter Select` |
| 2 | Missing songs dir (`--songs /tmp/does-not-exist`) | ✅ exit 0; `[SongLibrary] Directory does not exist`, `[SelectScreen] library: 0 songs, 0 charts` |
| 3 | Wheel/difficulty/grade/Confirm→Gameplay synthetic input | ✅ `select_screen_test` asserts Up/Down wrap, Left/Right clamp, star/letter labels, seeded best-grade, options derivation, Confirm→Gameplay `is_ready()`, Back→Select |
| 4 | Preview scheduling | ✅ `preview_player_test` asserts delayed load attempt, cancel-on-nav timer reset, stop, empty path, volume clamp (missing files never crash) |
| 5 | Preview uses `sample_start` | ✅ `select_screen_test` Alpha fixture `SAMPLESTART:30.0` → `preview().start_seconds() == 30.0`, `requested_path()` non-empty |
| 6 | Regression: full suite + `--gameplay-demo` | ✅ 19/19 incl. unchanged timing/judgment/scoring; demo exits 0 |
| 7 | `git status` scope | ✅ only the planned files; no changes under `src/timing/`, `src/chart/`, or judgment/score/note-field code |

## Files Changed

**Created (12):**
`src/screens/select_screen.{hpp,cpp}`, `src/screens/gameplay_screen.{hpp,cpp}`,
`src/screens/play_request.hpp`, `src/audio/preview_player.{hpp,cpp}`,
`src/render/texture_cache.{hpp,cpp}`, `src/render/stb_image_impl.cpp`,
`src/gameplay/gameplay_options.{hpp,cpp}`, `tests/select_screen_test.cpp`,
`tests/preview_player_test.cpp`.

**Updated (9):**
`CMakeLists.txt`, `tests/CMakeLists.txt`, `src/main.cpp`, `src/render/texture.{hpp,cpp}`,
`src/gameplay/gameplay_view.hpp`, `src/screens/screen.hpp`, `src/screens/screen_manager.{hpp,cpp}`.

Total: ~1,220 new lines (incl. tests) + 174 inserted / 13 removed across tracked edits.

## Deviations from Plan

1. **`gameplay_options.hpp` include**: The plan said it would include `chart/timing_data.hpp` +
   `gameplay/speed_mod.hpp`, but `ScrollDirection` actually lives in `gameplay/note_field.hpp`
   (not `speed_mod.hpp`). It includes `gameplay/note_field.hpp` instead, which also pulls in the
   timing/speed headers. No behavioral difference; still pure (no SDL/GL/audio).
2. **`kDefaultConstants`**: No such symbol exists; `GameplayScreen::enter` falls back to
   `JudgmentConstants::compiled_defaults()` when `ctx.constants == nullptr` (the plan's intended
   default).
3. **`format_bpm_range` empty → `"?"` is unreachable via the public API**: `TimingData`'s
   constructor/`clear()` always seeds a `{0.0, 120.0}` segment, so `bpms()` is never empty. The
   `"?"` branch remains as defensive code; the test instead asserts the default → `"120"`, single
   → `"140"`, and multi-segment → `"128-175"` cases.
4. **E2E chart count is 9, not the plan's 6**: scanning `tests/fixtures/reference_pack` yields
   4 songs / 9 charts (Tundra Anthem 5, Northern Lights 2, Aurora Borealis .ssc 1, Glacier Groove
   1). The observation log therefore reads `4 songs, 9 charts`; exit status and the logged events
   are as specified. The plan's expected `6` was an incorrect fixture census.
5. **Minor**: `PreviewPlayer` exposes `start_seconds()` / `length_seconds()` accessors beyond the
   plan's listed API so the test can assert the `sample_start` value flows through. `SelectScreen`
   reset `selected_chart_` to 0 on song change (plan reset it only on `enter`); this is a
   determinism choice and not asserted otherwise.
6. **`select_screen_test` order-independence**: the scanner's directory iteration order is
   unspecified, so the test locates the 3-chart and 1-chart songs by navigating the wheel rather
   than assuming fixed indices.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/preview_player_test.cpp` | delay scheduling + missing-file failure; re-request resets timer; stop cancels; empty path / non-positive values; volume clamp no-crash |
| `tests/select_screen_test.cpp` | wheel load + `SELECTABLE:NO` filtering + preview `sample_start`; Up/Down wrap; Left/Right clamp + single-chart no-op; best-grade lookup + star/letter labels; BPM formatting; config→options derivation; Confirm→Gameplay handoff + Back abort; empty-library safety + render smoke |

## Acceptance Criteria

- [x] Entering Select lists the library as a pack-grouped, scrollable wheel with banner art/artist/BPM
- [x] Highlight change requests a delayed preview and cancels on navigation
- [x] Passthrough difficulty labels + foot ratings + best grade per chart
- [x] Navigable with keyboard and pad (shared `Left/Down/Up/Right` + `Confirm`/`Back` actions)
- [x] Confirm launches gameplay with the chosen chart and current options
- [x] Corrupt/missing library, missing art, and missing audio never crash
- [x] `ctest` → 19/19; `--gameplay-demo` and prior tests stay green
- [x] Zero new warnings; no timing/judgment/scoring/note-field changes; select/options stay SDL/GL/audio-free
