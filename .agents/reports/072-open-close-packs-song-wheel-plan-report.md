# Implementation Report

**Plan**: `.agents/plans/completed/072-open-close-packs-song-wheel-plan.md`
**Branch**: `feature/072-open-close-packs-song-wheel`
**Status**: COMPLETE

## Summary

The Select wheel cursor is now a row cursor over the visible display rows. Pack headers are always visible and selectable; only the one open pack shows its songs (StepMania `SetOpenSection`). Confirm on a header closes the open pack or opens this one and closes the other, without leaving Select. The open pack and the cursor target (song by simfile path, or header by pack name) are remembered by name, so re-entry from Gameplay, Calibration, Input remap or Title restores them. First entry opens the first pack with song 0 highlighted. The selected header draws the gold bar plus a code-drawn triangle (down = open, right = closed); the info panel is empty on a header.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Pure layer: `build_wheel_rows(song_packs, open_pack)`, `WheelArt::SelectedPack`, `WheelRowView::open`, `wheel_marker_quad`, stale comments | `src/screens/select_art.{hpp,cpp}` | PASS |
| 2 | Row cursor, open pack, `toggle_pack`, `move_row`, restore by name, Confirm on a header, render | `src/screens/select_screen.{hpp,cpp}` | PASS |
| 3 | First-entry and degenerate cases (empty / all-hidden library, single pack, vanished names) | `src/screens/select_screen.cpp` | PASS |
| 4 | `select_art_test` cases (rows, marker geometry, recorded quads, render smoke) | `tests/select_art_test.cpp` | PASS |
| 5 | Adapt existing `select_screen_test` cases | `tests/select_screen_test.cpp` | PASS |
| 6 | New behaviour tests | `tests/select_screen_test.cpp` | PASS |
| 7 | Verification skill doc | `.claude/skills/verify/features/song-select.md` | PASS |
| 8 | Tick `TODO.md` line 67 (gitignored) | `TODO.md` | PASS |

## Validation Results

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Type check (build, warnings as errors) | PASS | `scripts/check.sh` (step 4) - host |
| Lint | PASS | `scripts/check.sh` (steps 1, 2: lint + clang-format) - host |
| Tests | PASS (53 passed) | `scripts/check.sh` (step 5) / `ctest --test-dir build` - host |
| Sanitizer (select tests) | PASS (2 of 2) | `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan -R select` - host |
| E2E 1-6 via `/verify` | PASS | `b4k.sh launch --run /tmp/blaze4k-verify/072-packs -- --songs /tmp/blaze4k-verify/songs072` (two copies of the fixture pack); shots `01`..`08` in `/tmp/blaze4k-verify/072-packs/evidence/` |

E2E detail: first entry opened the first pack with song 0 highlighted; Up gave the gold header bar with a down marker and an empty left panel; Confirm collapsed the pack, marker pointed right, `[GameplayScreen] started` count stayed 0; Down + Confirm on the other header opened it and kept the first collapsed; Confirm on a song reached Gameplay and Back restored the same song with its pack open; Tab -> Calibrate -> Back -> Back left the same song highlighted and the pack open.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/screens/select_art.hpp` | UPDATE | +27/-8 |
| `src/screens/select_art.cpp` | UPDATE | +47/-14 |
| `src/screens/select_screen.hpp` | UPDATE | +30/-7 |
| `src/screens/select_screen.cpp` | UPDATE | +163/-50 |
| `tests/select_art_test.cpp` | UPDATE | +130/-26 |
| `tests/select_screen_test.cpp` | UPDATE | +358/-35 |
| `.claude/skills/verify/features/song-select.md` | UPDATE | +7/-4 |
| `TODO.md` | UPDATE (gitignored, not in the diff) | line 67 ticked |

## Deviations from Plan

1. **Marker only on the selected header.** `assets/theme/cabinet/wheel_pack.png` already bakes a down caret into every unselected header (`manifest.json` note "folder caret baked in"; `docs/cabinet-theme/README.md`). The first E2E run showed the plan's code-drawn marker doubling it. The marker is now drawn only for the selected header (`SelectedPack`), sized and placed like the baked caret (`kWheelMarkerX = 36`, `kWheelMarkerHalf = 9`; plan said "every header"). An unselected header keeps the baked down caret for open and closed alike.
2. `SelectScreen` also stores `pack_names_`, `song_row_`, `cursor_song_key_` (the song key is persisted on each cursor change instead of being read from the old `songs_` pointer in `rebuild()`, so a changed library cannot dereference stale pointers). Plan allowed `pack_names_`; the rest is an implementation detail of D4.
3. Plan test item 7 "a wrap snaps" cannot be shown in the 8-row slide fixture (wrap moves the window one row, so it slides one pitch); `test_wheel_slide` asserts that, and a new `test_wheel_wrap_snaps` (one pack of 10 songs, 11 rows) asserts the snap.
4. `test_render_smoke` in `select_art_test` was adapted beyond the plan's list: first pack is open on entry in filesystem order, so it asserts order-independently and walks both packs and the closed state.
5. Results screen re-entry is not driven separately: it goes through the same `enter()` -> `rebuild()` path as the Gameplay return, which is tested.
6. A sanitizer run that overlapped my edits linked a stale `select_art_test` (concurrent compile read the file before the edit); after touching the sources and rebuilding, it passed. No code change.

Not done (per the owner's decisions): hint bar stays "ENTER PLAY" on a header (out of scope); no pack banner (info panel empty on a header).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/select_art_test.cpp` | `test_build_wheel_rows` (open pack 0 / 1 / none / unknown / empty / single), `test_wheel_marker` (hand-computed geometry), `test_wheel_header_recorded_quads` (selected header marker, open and closed, via `GlQuadRenderer::recorded_quads`), `test_render_smoke` (headers, toggles, both packs) |
| `tests/select_screen_test.cpp` | adapted: song navigation (header stop), chart navigation lap, special-character titles, `test_selection_preserved_on_reenter` (+ open pack), `test_wheel_slide` (8-row numbers recomputed by hand); new: `test_pack_header_toggle`, `test_pack_toggle_snaps_slide`, `test_wheel_wrap_snaps`, `test_all_hidden_library`, `test_pack_state_survives_reenter` (Gameplay, Calibration, Input remap, Title; header persistence; vanished library fallback), `test_pack_header_render`; `test_wheel_skips_pack_rows` removed (old behaviour) |

## Later changes (after the first review)

The owner requested four changes after this report was written. The code, `docs/cabinet-theme/README.md` and `.claude/skills/verify/features/song-select.md` follow them; where the text above says otherwise, the text below wins.

1. **Marker on every pack header.** The open/closed marker is drawn on every pack header (down = open, right = closed), selected or not. The baked caret was removed from `assets/theme/cabinet/wheel_pack.png` (recipe in `docs/cabinet-theme/README.md`, note in `assets/theme/cabinet/manifest.json`). The Input Remap headers draw the same marker through `draw_pack_marker` in `src/screens/setup_art.cpp`.
   Supersedes: Deviation 1 ("Marker only on the selected header", and "An unselected header keeps the baked down caret for open and closed alike"). The plan's "every header" is the behaviour again.
2. **Hint bar word.** The word after ENTER is OPEN on a closed pack header, CLOSE on an open pack header, and PLAY on a song (`ConfirmHint`, `confirm_hint_word` and `select_hint_items` in `src/screens/select_art.cpp`; `SelectScreen::confirm_hint`).
   Supersedes: "Not done" ("hint bar stays 'ENTER PLAY' on a header").
3. **First entry opens with every pack closed.** The cursor is on the first pack header. This is what StepMania's `MusicWheel::BeginScreen` does when no previous song is found: it calls `SetOpenSection("")` (`src/MusicWheel.cpp:138-210` at `825467bc`). Re-entry (after Gameplay, Results, Calibration, Input Remap or Title) still restores the remembered song or header and its open pack.
   Supersedes: the E2E detail line "first entry opened the first pack with song 0 highlighted", and the `test_render_smoke` note in Deviation 4 ("first pack is open on entry").
4. **Banner on a selected pack header.** A selected pack header shows its pack's banner (`SongPack::banner_path`), else the library fallback banner, chosen by `SelectScreen::cursor_banner_path`. The rest of the info panel stays empty. A test covers the choice of path; the draw itself was checked in the real window.
   Supersedes: "Not done" ("no pack banner (info panel empty on a header)").
