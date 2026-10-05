# Implementation Report

**Plan**: `.agents/plans/completed/049-remap-calibration-cabinet-plan.md`
**Branch**: `feature/049-remap-calibration-cabinet`
**Status**: COMPLETE (owner windowed visual check pending)

## Summary

The Input Remap and Calibration screens now draw in the Cabinet look through a new GL-free art module, `src/screens/setup_art.{hpp,cpp}` (pure reference-px layout plus thin, null-guarded draw helpers, the `options_art` / `select_art` / `results_art` pattern).

- **Both screens**: `bg_select`, `bar_top` with a runtime title ("REMAP INPUT" / "CALIBRATE OFFSET") in `options_art::kTitleStyle`, a per-state legend in `bar_hint` (built by `select_art::layout_hint_items`), and the scanlines drawn last.
- **Remap**: the binding table uses the options overlay's slanted rows (560 wide, 48 / 80 tall, pitch 58), grouped under `wheel_pack` KEYBOARD / PAD header rows, with RESET TO DEFAULTS (red) as the last row. The list scrolls 9 visible rows with `select_art::list_window`. The gold bar stays on the row being rebound while capturing. The model's message is a gold chip at the top right.
- **Calibration**: a gold phase word (GET READY / TAP ON THE BEAT / DONE), two `stat_panel` plates (SAMPLES, OFFSET; OFFSET reads "---" in steel until a usable result, and always without audio), and a centred red no-audio notice.

Only `render()`, includes, colour constants and class comments changed in the two screens. `update` / `enter` / `exit` / capture / the save path / the clock wiring are unchanged, and `input_remap_screen_test` and `calibration_screen_test` are unmodified and green. The resolved open questions use the plan's recommended defaults.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `setup_art` constants, legends, static_asserts, pure layout and labels; core source registration | `src/screens/setup_art.hpp`, `src/screens/setup_art.cpp`, `CMakeLists.txt` | ✅ |
| 2 | `setup_art` draw helpers (`draw_chrome`, `draw_hint_bar`, `draw_message_chip`, `draw_remap_table`, `draw_calibration`) | `src/screens/setup_art.{hpp,cpp}` | ✅ |
| 3 | `InputRemapScreen::render` uses `setup_art`; bitmap font and colour constants removed | `src/screens/input_remap_screen.{hpp,cpp}` | ✅ |
| 4 | `CalibrationScreen::render` uses `setup_art`; bitmap font and colour constants removed | `src/screens/calibration_screen.{hpp,cpp}` | ✅ |
| 5 | Dropped the #97 forward reference from the `draw_panel` comment | `src/screens/options_art.hpp` | ✅ |
| 6 | `setup_art_test` + registration | `tests/setup_art_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 7 | Full validation | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, host GCC 16.2.1, touched TUs rebuilt) | ✅ |
| Lint (zero compiler warnings in touched TUs; the whole build log has 0 `warning:` lines) | ✅ |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 49/49 passed |
| Static: no `draw_text` / `bitmap_font` in either screen | ✅ no hits |
| Static: no clock reads in `setup_art.*` | ✅ no hits |
| Static: behaviour files and the two screen tests untouched (`git diff --stat`) | ✅ empty |
| Static: `git diff -U0` hunks of both screens lie only in `render()`, includes, the colour block or the class comment | ✅ |
| E2E 1: `input_remap_screen_test`, `calibration_screen_test` (unmodified) and `setup_art_test` render walk (every remap row and RESET as the selection, message chip, capture; calibration CountIn / Sampling / Ready / no audio; 5 sizes; real and null services) | ✅ |
| E2E 2: headless app smoke (`--headless --smoke-test 5 --start-screen select`, sandboxed, scratch data dir) | ✅ `Blaze 4k shut down cleanly.`, exit 0. The ALSA `snd_func_refer` / `Unknown PCM dmix` lines come from the sandbox's empty `/dev/snd` (miniaudio falls back to its Null device), not from this change |
| E2E 3: windowed visual check | ⏳ owner-only (the agent must not launch the GUI) |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/setup_art.hpp` | CREATE | +260 |
| `src/screens/setup_art.cpp` | CREATE | +344 |
| `tests/setup_art_test.cpp` | CREATE | +654 |
| `src/screens/input_remap_screen.cpp` | UPDATE | +20/-86 |
| `src/screens/input_remap_screen.hpp` | UPDATE | +1/-0 |
| `src/screens/calibration_screen.cpp` | UPDATE | +28/-30 |
| `src/screens/calibration_screen.hpp` | UPDATE | +1/-1 |
| `src/screens/options_art.hpp` | UPDATE | +0/-1 |
| `CMakeLists.txt` | UPDATE | +1/-0 |
| `tests/CMakeLists.txt` | UPDATE | +18/-0 |

`.agents/stories/todo-stories.md` has unrelated uncommitted owner edits and was not touched.

## Deviations from Plan

1. **Legend piece counts.** The plan's table (and Task 6) says the five legends have 6 / 6 / 3 / 9 / 7 pieces. The browse and reset legends have 6 *items*, but the up/down arrow pair is two pieces, so `layout_hint_items` gives **7** pieces (the calibration rows, 9 and 7, already count pieces). The item lists match the plan exactly, and the test asserts the real piece counts **7 / 7 / 3 / 9 / 7** with the kinds and texts in order.
2. **Notice line height.** The test checks that each line box fits its band. The no-audio notice style (SairaBold 28) has a line box of about 44 px, a little taller than the plan's 40 px notice band. The band stays as pinned. The test asserts the intent instead: centred in the band, the line box clears the plates above (411) and the hint-bar rule below (666).
3. **Extra pure helper `remap_device_label(DeviceType) -> std::string_view`.** It is needed so the KEYBOARD / PAD header text does not allocate (`remap_device_name` returns `std::string`). The test pins it equal to `remap_device_name`.
4. **Text styles as public constants.** The remap, chip, plate, pending and notice styles (`kHeaderStyle`, `kKeyStyle`, `kResetStyle`, `kChipStyle`, `kPlatePendingStyle`, `kNoticeStyle`, …) are public `constexpr` in `setup_art.hpp`, so the tests check their fit and pre-baked atlases. `setup_art.hpp` therefore includes `render/ttf_font.hpp` (for `with_color`). It is GL-free.
5. **Value slant correction.** Centring a plate value on the slanted plate needs its cap centre, so one named constant, `kValueCapHalf = 12` (about half of SairaExtraBold 34's cap height), was added. Labels use `results_art::kStatLabelCapCentre`.
6. **Tasks 1 and 2 were written together** (one build after both). The build graph was complete at that point.
7. **Extra coverage in `setup_art_test`.** It also covers interleaved-device display rows, `GameAction::None` → "NONE", and a render with a long custom key binding to exercise the key-truncation path.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/setup_art_test.cpp` | `test_remap_display_rows` (defaults 27 rows / headers at 0 and 13 / reset 26, round trip over every binding, keyboard-only, interleaved, empty, clamping); `test_remap_layout` (9 visible rows, row rects, no overlap and the room budget for every selected slot, 1440p 2x, chip rect, list window); `test_remap_labels` (action / device / value views equal input_remap's helpers, capturing on rows 0 and 23); `test_calibration_layout` (plate rects, centring, bands, slant correction, samples text); `test_legends` (five legends: piece counts, kinds and texts, centred, no trailing gap, width); `test_text_fits` (titles vs the widest message chip, title band, phase words, every default binding and `<PRESS>` selected and not, RESET, KEYBOARD / PAD, plate values and labels, notice); `test_styles_prebaked`; `test_texture_names_exist`; `test_render_smoke` (both screens through every state, 5 sizes, real and null services) |
