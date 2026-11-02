# Code Review: feature/073-cabinet-theme-polish

**Scope**: branch `feature/073-cabinet-theme-polish` vs merge base `65a0721` (main), uncommitted changes plus untracked `docs/cabinet-theme/screenshots/`, checked against issue #98
**Recommendation**: NEEDS CHANGES (one High: an acceptance criterion that no run shows as met)

## Summary

The code change is small and correct: `video.scanlines` is read, saved, carried through `AppConfig::scanlines` to `ThemeTextures`, and honoured in `title_art::draw_scanlines`, which every screen's scanlines call reaches (title, attract, select, options, calibration, input remap, results). Tests cover default, round trip, wrong type and the draw decision. `bitmap_font` is documented as fallback/debug. The open points are two acceptance criteria that the implementer disclosed as not fully met (perf, 2560x1440 screenshots).

Acceptance criteria:
1. **Persisted `video.scanlines` toggle + tests: met.** `src/data/config.hpp:29`, `config_loader.cpp:190,400`, `config_persistence_test.cpp` (default, round trip, wrong type, explicit false), `title_screen_test.cpp` `test_scanlines_toggle` via the headless recorder. Saved config keeps the value because `main.cpp:200` copies from `game_config`, which is the object that gets saved. Pixel-diff proof in the real window (`720p-10-title-scanlines-off.png` vs `720p-01-title.png`) is newer than the last source edit (13:37 vs 13:32).
2. **`--perf-report` no regression, gameplay no more expensive: NOT met / unverified** (see H1).
3. **Cross-platform doc with theme pass and screenshots at 1280x720 and 2560x1440: partly met.** Doc section exists (`docs/CROSS_PLATFORM_VERIFICATION.md:108-201`), Linux machine-checked, Windows/macOS owner steps listed with real preset names (`windows-msvc-release`, `macos-clang-release` exist in `CMakePresets.json`). 1280x720 screenshots exist for every screen (I opened `720p-07-gameplay.png` and `1080p-08-results.png`: layout renders correctly). 2560x1440 is missing (M1).
4. **No screen uses `bitmap_font` for player-facing text: met.** Verified: `SelectPlaceholderScreen` is not registered in `src/main.cpp`; the only live uses are the judgment pop fallback when the theme sprite is missing (`src/gameplay/judgment_animator.cpp:212-215`), the `TextRenderer` fallback and the cell-model helpers in `song_display_text.cpp`. It is documented in `src/render/bitmap_font.hpp` as fallback and debug.

No UI source change affects what a screen draws by default, so I did not launch the game (the caller forbade opening the window); I read the implementer's captures, which post-date the last source edit, and checked them.

## Issues Found

### Critical
None

### High Priority
- **H1** `docs/CROSS_PLATFORM_VERIFICATION.md:149` — The criterion "gameplay is no more expensive than before" is contradicted by the implementer's own measurement: gameplay CPU 0.25-0.29 s before vs 0.34-0.38 s after (about +30 %), draw calls per gameplay frame 7.3 -> 11.7, whole-run CPU 0.85-0.88 s -> 1.63-2.11 s; the doc itself says it "does **not** reproduce" the criterion in CPU terms (the run also used the fixture pack on the stub clock for about 5.9 s of gameplay, not `--perf-report` over a full song, and `--no-vsync` still paced at 60 fps so frame time cannot show cost). **Why:** only quads per frame (295.5 -> 34.8) and an unchanged 16.65 ms median frame meet the wording, so the criterion is not shown as met; slower GPUs/CPUs and a real full song are untested (unverified, not disproved). **Fix:** the owner decides on the issue: either accept the CPU trade-off and reword the criterion to "no frame-time regression and fewer quads", or cut the draw-call count (for example batch the sprite and text atlases so fewer texture switches flush per frame) and re-measure; in either case run `--perf-report` over a full real song before closing the issue (the Linux leg is not a substitute).

### Medium Priority
- **M1** `docs/CROSS_PLATFORM_VERIFICATION.md:166` — The criterion asks for a screenshot of each screen at 2560x1440; only 1280x720 and 1920x1080 were captured, and 2560x1440 is left as an owner step (line 197). **Why:** a literal criterion is unmet (the 1.5x scale is not the 2x scale that `title_screen_test` pins), though the cause is environmental: the host display is 1920x1080 and an X11 window larger than the screen cannot be read back. **Fix:** the owner captures `1440p-*.png` on a 1440p display (already in the checklist), or sets the criterion to 1920x1080; leave the issue open until one of the two is recorded.

### Suggestions (Low)
- **L1** `tests/title_screen_test.cpp:23` — `#include "screens/select_art.hpp"` is inserted before `screens/screen.hpp` and `screens/screen_manager.hpp`, breaking the alphabetical order of the surrounding block (clang-format has `SortIncludes: Never`, so the check does not catch it). **Why:** cosmetic. **Fix:** move it after `screens/screen_manager.hpp`, before `select_placeholder_screen.hpp`.

**Noted, not a finding**:
- Windows and macOS verification are owner steps by design (the criterion says "owner steps listed"); they are listed and marked not verified.
- Options overlay entry for the toggle is not added; the owner comment on the issue says config-file only is enough.
- Optional items in the technical notes (rotating results sunburst, 1x backgrounds) were not required and not done.
- `results_art::draw_scanlines` is not covered by the recorder test (it returns early on an uninitialised renderer, `src/screens/results_art.cpp:331`); it forwards to the tested `title_art::draw_scanlines`, and the real-window run covers it.
- The implementation report discloses that a baseline run opened the old UI on the owner's desktop; process matter, not a code finding.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` step 4 (warnings as errors, host GCC) in the user's `build/` |
| Warnings | NONE | the `-DBLAZE_WERROR=ON` build in `scripts/check.sh` fails on any project warning |
| Lint | PASS | `scripts/check.sh` steps 1-2 (lint, `format.sh --check`) |
| Tests | PASS | `scripts/check.sh` step 5: 100% of 53 passed, none skipped; tests run on the null audio device (no assertion skipped) |
| UI verification | SKIPPED | the caller forbade launching the game window; implementer captures (`docs/cabinet-theme/screenshots/`, 13:37-13:39, after the last source edit at 13:32) were opened and judged; the owner still checks 2560x1440 and Windows/macOS |

## What's Good

- One choke point (`title_art::draw_scanlines`) makes the toggle apply to every screen, and the flag survives `ThemeTextures::shutdown()/load()`.
- Config handling follows the existing tolerant pattern (`read_bool_field` warns by name and keeps the default) and the tests assert literal values, including a wrong-type case.
- The test for the toggle checks recorded quads on and off (with a negative control reported), not just that code runs.
- The doc reports an unfavourable perf result honestly instead of claiming the criterion.

## Recommendation

Resolve H1 with an owner decision (accept and reword, or reduce draw calls) and a full-song `--perf-report`; capture or waive 2560x1440 (M1); then `/fix-findings .agents/reviews/feature-073-cabinet-theme-polish-review.md` for L1.
