# Implementation Report

**Plan**: `.agents/plans/completed/046-gameplay-hud-cabinet-plan.md`
**Branch**: `feature/046-gameplay-hud-cabinet`
**Status**: COMPLETE (owner windowed visual check pending)

## Summary

Restyled the gameplay HUD to the Cabinet look (#93). The live percent, the top-centre combo, the eight per-window judgment chips and their palette (`judgment_color`, `percent_text_rect`) are gone. The life bar is now `life_frame` (9-slice) at `theme::layout::kLifeBar`, with `life_fill` / `life_fill_danger` (below `theme::color::kLifeDangerThreshold`) cropped inside the 32 px track and `life_stripes` tiled bottom-aligned over the fill. The slide-then-shrink field-clearance clamp is kept. A `diff_badge` plate tinted with `theme::difficulty` shows e.g. "HARD 8" in the ink colour, left-aligned 18 px into the plate. The plate grows up to 300 ref px for long Edit names and then truncates. It hides when it cannot clear the field. The judgment pop draws the baked `judgment_<kind>` sprite with the unchanged scale/fade curves, and falls back to the bitmap label when the theme or the manifest entry is missing. A persistent "N COMBO" line (`kComboNumber` + `kComboLabel`, sheared by `kComboGroupShear`) sits at `kComboTop` from a combo of 4 (OpenITG `ShowComboAt=4`). Milestones and the final combo flash the number gold → white (colour only, so no atlas re-bake happens mid-song).

ScoreKeeper, LifeKeeper, the judgment engine, the note field, receptors, the Cel noteskin and the background dim are untouched. Nothing new reads the music clock, and all animation stays on the fixed `dt`.

All plan defaults for the open questions were adopted, as the invoking request specified.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Judgment animator: sprites, combo line, pure helpers | `src/gameplay/judgment_animator.{hpp,cpp}` | ✅ |
| 2 | HUD renderer: remove percent/chips, chrome life bar, badge | `src/gameplay/hud_renderer.{hpp,cpp}` | ✅ |
| 3 | GameplayView wiring and draw order | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| 4 | GameplayScreen + `--gameplay-demo` harness | `src/screens/gameplay_screen.{hpp,cpp}`, `src/main.cpp` | ✅ |
| 5 | `hud_renderer_test` | `tests/hud_renderer_test.cpp` | ✅ |
| 6 | `judgment_animator_test` (+ asset roots) | `tests/judgment_animator_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 7 | `gameplay_screen_test` (+ asset roots) | `tests/gameplay_screen_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 8 | README line 50 + full validation | `README.md` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8`) | ✅ (rc 0) |
| Lint: no new warnings in touched TUs (clean rebuild of all touched TUs) | ✅ "no new warnings" (0 warning lines in the whole build log) |
| Tests (sandboxed bwrap ctest, `-j8`) | ✅ 48/48 passed |
| Static: `percent_text_rect\|judgment_color\|kLifeBarMinInset` gone | ✅ no hits |
| Static: `draw_chip` | ✅ (narrowed, see Deviations) no hits in `src/gameplay` or tests |
| Static: `format_percent` only in results/select outside hud_renderer | ✅ `results_screen.cpp`, `select_screen.cpp`, `select_art.hpp` (comment) |
| Static: no clock reads in HUD/animator | ✅ no hits |
| E2E 1: headless `--gameplay-demo` Anubis, 30 frames | ✅ `[GameplayView] Loaded chart 'Hard' (meter 8)`, final score line, `Blaze 4k shut down cleanly.` (the only error lines are ALSA ones caused by the sandbox's tmpfs `/dev/snd`) |
| E2E 2: headless shell `--start-screen select`, 5 frames | ✅ exits cleanly |
| E2E 3: windowed visual check vs `cabinet-v3-gameplay.png` at 1280x720 and 2560x1440 | ⏳ owner-only (needs a display; not run) |

Note: headless runs have no GL context, so `GameplayView::render` returns early and the HUD draw calls do not execute in E2E 1. Geometry is pinned by the pure-layout unit tests. The draw paths run headless only up to the uninitialised-renderer guard, and their real pixels need the owner's windowed check.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/hud_renderer.hpp` | UPDATE | +68/-35 |
| `src/gameplay/hud_renderer.cpp` | UPDATE | +106/-155 |
| `src/gameplay/judgment_animator.hpp` | UPDATE | +65/-15 |
| `src/gameplay/judgment_animator.cpp` | UPDATE | +104/-18 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +14/-1 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +11/-6 |
| `src/screens/gameplay_screen.hpp` | UPDATE | +10/-0 |
| `src/screens/gameplay_screen.cpp` | UPDATE | +12/-2 |
| `src/main.cpp` | UPDATE | +3/-1 |
| `tests/hud_renderer_test.cpp` | UPDATE | +168/-97 |
| `tests/judgment_animator_test.cpp` | UPDATE | +155/-13 |
| `tests/gameplay_screen_test.cpp` | UPDATE | +102/-0 |
| `tests/CMakeLists.txt` | UPDATE | +14/-0 |
| `README.md` | UPDATE | +1/-1 |

`.agents/stories/todo-stories.md` was left as the owner had it (pre-existing, unrelated uncommitted edit; not touched or staged).

## Deviations from Plan

1. **`judgment_animator.cpp` still includes `gameplay/hud_renderer.hpp`.** The plan said to drop that include ("existed only for the palette"), but the pinned combo-line semantics draw `format_combo(live_combo_)`, which lives in that header. The include is kept for `format_combo`, and the palette dependency is gone.
2. **Static `draw_chip` grep narrowed.** The plan's `grep -nE "...|draw_chip|..." src tests -r` also matches `select_art::draw_chips`, the select screen's speed/scroll chips, which are unrelated and must stay. The narrow variant (no `draw_chip` in `src/gameplay` or tests) has no hits.
3. **Small additions for testability** (no behaviour change): a `JudgmentAnimator::popup_sprite()` accessor, a NaN-life case in `test_clamp`, and a `test_judgment_pop_rect` unit test in `judgment_animator_test`. `layout_diff_badge` treats a non-finite text width as the maximum plate width.
4. **`gameplay_screen.cpp` adds `#include <string>`** for `std::to_string` (hygiene).
5. **E2E 3 (windowed visual check) is owner-only** and pending.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/hud_renderer_test.cpp` | `test_life_bar_matches_mock_720p`, `test_life_bar_scales` (2560x1440, 1920x1080, 3440x1440, 640x480, 1024x768), `test_bottom_up_fill`, `test_clamp` (incl. NaN), `test_danger`, `test_track_inside_frame`, `test_no_field_overlap_common_sizes` (bar + badge), `test_below_diff_badge`, `test_judgment_pop_clear` (peak scale, 4 sizes), `test_narrow_clamp` (500x400, 400x400), `test_diff_badge_layout`, `test_combo_line_layout`, `test_degenerate` |
| `tests/judgment_animator_test.cpp` | `test_sprite_mapping` (exact names + real manifest kind/content 888x132), `test_judgment_pop_rect`, `test_combo_visibility`, `test_combo_number_color`, rewritten `test_headless_render_and_reset` (null + real headless theme/fonts); label/curve/consume/milestone tests unchanged |
| `tests/gameplay_screen_test.cpp` | `test_difficulty_badge_for` (Hard, Challenge, named Edit, unnamed Edit, empty label → BEGINNER), `test_enter_sets_badge_and_renders_headless`; `test_end_delay_before_results` unchanged |
