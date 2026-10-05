# Code Review: feature/049-remap-calibration-cabinet

**Scope**: Branch `feature/049-remap-calibration-cabinet` vs `main` (no commits yet; uncommitted + untracked changes), GitHub issue #97. `.agents/stories/todo-stories.md` excluded (unrelated owner edit).
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the new GL-free art module `src/screens/setup_art.{hpp,cpp}`, the `render()` rewrites of `InputRemapScreen` and `CalibrationScreen`, the `options_art.hpp` comment trim, the CMake registrations, and the new `tests/setup_art_test.cpp`. The change follows the `options_art` / `select_art` / `results_art` pattern (sprite names, `draw_slice3` / `draw_stretch` usage, chip tinting, hint-bar placement, `stat_text_x`-style slant correction). It meets all four acceptance criteria. Only `render()`, includes, colour constants and class comments changed in the two screens, so input handling, capture, the calibration maths, the save path and Esc-back are untouched. The findings are four Low nits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/screens/setup_art.cpp:125` and `:146`: duplicated label tables.** `remap_action_label` and `remap_device_label` copy the switch in `input_remap.cpp:134` (`remap_action_name`) and `:155` (`remap_device_name`). The only thing that keeps them in sync is `test_remap_labels`. A new `GameAction` or a renamed label could drift between the chip message ("BOUND TO X") and the table.
   *Recommendation*: Keep one table. Move the `string_view` versions into `input_remap.cpp` and have the `std::string` versions return `std::string(remap_action_label(a))`.

2. **`src/screens/calibration_screen.cpp:160`: the "SSO-sized" comment overstates it.** The comment says both values are SSO-sized, which suggests no per-frame heap allocation. But on every Ready-state frame, `format_offset` (`options_menu.cpp:279`) builds a `std::ostringstream`, and that allocates. This is not a regression, because the old render allocated more.
   *Recommendation*: Either cache the formatted offset when `result_` changes, or change the comment so it only talks about the result strings.

3. **`src/screens/setup_art.hpp:153`: `kNoticeHeight` is smaller than the real line.** It is 40, but the notice style's line box is about 44 px. So the static_assert `kNoticeTop + kNoticeHeight <= kHintBarTop` pins a band that the drawn text is taller than. `test_text_fits` checks the real extents and they clear both neighbours, so nothing overlaps today.
   *Recommendation*: Set the band to 44, or note on the constant that the line box overflows it by about 2 px on each side.

4. **`src/screens/calibration_screen.cpp:173` / `src/screens/setup_art.hpp:83`: "ENTER SAVE" shows before saving is possible.** The audio legend shows "ENTER SAVE" during GET READY and TAP ON THE BEAT, but Confirm does nothing until `calib_.ready()`. The old footer had the same text, so this is pre-existing.
   *Recommendation*: Optionally use `kCalibrateNoAudioHint`'s item set (no ENTER SAVE) until `offset_ready`. The tap/cancel legend already exists.

**Noted, not a finding**:
- While capturing, the gold bar stays on the row being rebound. The old screen dropped the highlight during capture. This is a deliberate design choice in the plan, and the row shows `<PRESS>`.
- The device column became KEYBOARD / PAD `wheel_pack` header rows. These are display-only, and navigation still moves over the model's rows.
- The windowed visual check is still pending for the owner (agents may not launch the GUI).
- Pre-existing and outside this diff: with an empty `model.rows`, Confirm before any Down would `start_capture` on a row that does not exist. The default bindings make empty rows unreachable in practice.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, GCC, Release) | PASS (up to date, includes `setup_art_test`) |
| Warnings gate: `setup_art.cpp`, `input_remap_screen.cpp`, `calibration_screen.cpp` and `tests/setup_art_test.cpp` recompiled to `/dev/null` with the targets' own `flags.make` flags (`-O3 -std=c++20 -Wall -Wextra -Wpedantic`) | PASS (0 warnings) |
| Lint | N/A: no `.clang-format` or `clang-tidy` config in the repo, so the compiler warnings gate is the lint |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` (inside the sandbox, `/dev/snd` and `/run/user/$UID` were confirmed empty) | PASS 49/49 |
| `setup_art_test`, `input_remap_screen_test`, `calibration_screen_test` run directly in the same sandbox | PASS |
| Skipped / env-guarded tests | None skipped. Some fallbacks still run every assertion: (a) no GL context, so `TextRenderer` measures but bakes no atlases and `ThemeTextures` draws flat fallbacks, which means the render smoke covers layout and the draw-call paths but not pixels; (b) the calibration no-audio cases use the synthetic stub clock by design |

## What's Good

- The module is pure in reference pixels, with null-guarded draw helpers, so headless tests can pin the layout. Static_asserts encode the room budget (exactly 9 rows fit and a 10th does not), the plate centring and the vertical bands.
- Drawing reuses the overlay's row geometry, `name_x` / `value_right`, `list_window`, `layout_hint_items`, the chip convention (border tint matches the text colour) and the `stat_panel` slant correction, so there is little new visual vocabulary.
- The common path does not allocate when drawing remap text (`string_view` labels, truncation only on overflow), and long custom key names truncate against the action name instead of overlapping it.
- No clock reads and no audio includes in `setup_art`. Calibration timing is untouched (principle 1).
- The tests are thorough: display-row round trips (default, keyboard-only, interleaved, empty), text fit with the real fonts for every default binding and `<PRESS>` in both row states, worst-case `format_offset` widths, the pre-baked atlas check, manifest names, and a render walk over every state at 5 sizes with real and null services.
- The implementation report documents its deviations honestly (legend piece counts, notice band).

## Recommendation

Ready to merge after the owner's windowed visual check at 720p and 1440p. The four Low items are optional polish. #1 (one label table) gives the most for the least work.
