# Implementation Report

**Plan**: `.agents/plans/024-noteskin-animations-ui-sounds-plan.md`
**Branch**: `feature/024-noteskin-animations-ui-sounds`
**Status**: COMPLETE

## Summary

Delivered all of Phase D2 (#24) in one branch/PR, per the confirmed decisions: a procedural,
direction-aware noteskin (no committed PNGs), judgment + combo pop animations driven directly by the
existing B4 event log, and synthesized menu UI sounds (Move/Confirm/Back on Title/Attract/Select/
Results only). The judgment/scoring/timing path is untouched; `fixed_dt` drives only pop fades.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| A1 | Pure note-art rasterizers | `src/render/note_art.{hpp,cpp}` | ✅ |
| A2 | `NoteSkin` art textures + per-column direction | `src/gameplay/noteskin.{hpp,cpp}` | ✅ |
| A3 | Note field renderer uses the art | `src/gameplay/note_field_renderer.cpp` | ✅ |
| A4 | `note_art_test` | `tests/note_art_test.cpp` | ✅ |
| B1 | Shared `judgment_color` accessor | `src/gameplay/hud_renderer.{hpp,cpp}` | ✅ |
| B2 | `JudgmentAnimator` | `src/gameplay/judgment_animator.{hpp,cpp}` | ✅ |
| B3 | `GameplayView` animator integration | `src/gameplay/gameplay_view.{hpp,cpp}` | ✅ |
| B4 | `judgment_animator_test` | `tests/judgment_animator_test.cpp` | ✅ |
| C1 | `UiSoundPlayer` + WAV synth | `src/audio/ui_sounds.{hpp,cpp}` | ✅ |
| C2 | Shell UI-sound triggers | `src/screens/screen.hpp`, `src/screens/screen_manager.cpp` | ✅ |
| C3 | Wire player in `main` | `src/main.cpp` | ✅ |
| C4 | `ui_sounds_test` | `tests/ui_sounds_test.cpp` | ✅ |
| D1 | Register sources + tests | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| D2 | Full suite + warning budget | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ no errors |
| Warnings (`-Wall -Wextra -Wpedantic`) | ✅ none |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 31/31 |
| `note_art_test` / `judgment_animator_test` / `ui_sounds_test` | ✅ |
| `screen_manager_test` / `note_field_test` unchanged | ✅ |
| Forbidden includes (`glad`/`SDL` in pure modules) | ✅ none |
| Judgment/scoring path diff | ✅ none |
| E2E `--gameplay-demo` smoke (exit 0) | ✅ |
| E2E shell `--headless --smoke-test` (UI sounds wired, exit 0) | ✅ |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/note_art.hpp` | CREATE | +23 |
| `src/render/note_art.cpp` | CREATE | +173 |
| `src/gameplay/judgment_animator.hpp` | CREATE | +68 |
| `src/gameplay/judgment_animator.cpp` | CREATE | +145 |
| `src/audio/ui_sounds.hpp` | CREATE | +46 |
| `src/audio/ui_sounds.cpp` | CREATE | +197 |
| `tests/note_art_test.cpp` | CREATE | +129 |
| `tests/judgment_animator_test.cpp` | CREATE | +176 |
| `tests/ui_sounds_test.cpp` | CREATE | +204 |
| `src/gameplay/noteskin.hpp` | UPDATE | +33/-… |
| `src/gameplay/noteskin.cpp` | UPDATE | +95/-… |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | +11/-… |
| `src/gameplay/hud_renderer.hpp` | UPDATE | +6 |
| `src/gameplay/hud_renderer.cpp` | UPDATE | +55/-… |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +3 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +13 |
| `src/screens/screen.hpp` | UPDATE | +5 |
| `src/screens/screen_manager.cpp` | UPDATE | +27 |
| `src/main.cpp` | UPDATE | +5 |
| `CMakeLists.txt` | UPDATE | +3 |
| `tests/CMakeLists.txt` | UPDATE | +30 |

## Deviations from Plan

1. **Direction via per-column textures, not `arrow_uv`.** A `UVRect` maps a fixed corner scheme
   (gl_quad_renderer.cpp:194-211), so it can only express axis-aligned flips/180° — a true 90°
   rotation is unrepresentable. `NoteSkin` therefore bakes direction into `std::array<Texture,4>` per
   category and exposes `head_texture(NoteType, int column)` / `receptor_texture(int column)`. The
   plan's `arrow_uv(int column)` was dropped as impossible-by-construction; the fix is documented in
   `noteskin.hpp` and covered by the direction-coverage test.
2. **`NoteSkin` `head_texture` takes `column`** (needed for direction); `body_texture()` added;
   `receptor_texture()` no-arg replaced by the column overload. `style_for`/`column_tint`/
   `quad_texture` retained (note_field_test source-compat).
3. **OQ2 "plus final combo"** implemented explicitly: `JudgmentAnimator::celebrate(int)` arms a
   pop for the final non-round combo, called once from `GameplayView` when `score_.is_complete()`.
4. **`ui_sounds_test` degenerate-dir case uses an empty path** (returns false) rather than a
   syntactically nonexistent path, because a nonexistent directory is created and, with a working
   audio device, would legitimately succeed.
5. **`hud_renderer.hpp` gained `#include "render/geometry.hpp"`** so the new `Color` return type
   compiles (score_keeper.hpp does not pull geometry).
6. Bitmap font retained (OQ6); no `stb_truetype`. Single PR (OQ5). Mask size 64px; the rasterizer
   uses symmetric 3×3 supersampling so 90° rotations preserve coverage exactly.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/note_art_test.cpp` | buffer shape/white-RGB/coverage, hollow receptor, distinct tap/hold/roll/mine/body masks, 4-way rotation changes mask & preserves coverage, size<=0 empty |
| `tests/judgment_animator_test.cpp` | full label mapping, color mapping, pop curves (incl. degenerate duration), consume/last-wins/stat-only, milestone dedupe, final-combo celebrate, headless render+reset |
| `tests/ui_sounds_test.cpp` | WAV RIFF/WAVE/16-bit/mono/nonzero + distinct sizes, empty path rejected, menu Move/Confirm/Back triggers, silence on Gameplay/Calibration/InputRemap, null sink, unavailable player no-op |
