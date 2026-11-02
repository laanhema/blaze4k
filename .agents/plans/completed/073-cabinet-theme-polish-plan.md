# Plan: Polish the Cabinet theme and verify it cross-platform

## Summary

Finish the Cabinet theme (issue #98). Add a persisted `video.scanlines` toggle (default on) that
`title_art::draw_scanlines` (the single definition every screen's scanlines call forwards to)
honours; compare `--perf-report` over a full song against a pre-theme baseline build; add a
Cabinet-theme pass with 1280x720 and 2560x1440 screenshots to `docs/CROSS_PLATFORM_VERIFICATION.md`;
and resolve `bitmap_font` by keeping it, documented as the fallback (and test-only placeholder)
text path, because deleting it would break the TrueType and judgment-pop fallbacks.

## User Story

As a player
I want to switch the scanlines overlay off from the config file
So that I can have a cleaner picture, and the owner can trust the theme on all three platforms.

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | MEDIUM (small code, but a baseline build, perf runs and screenshots) |
| Systems Affected | data (config), app, render (theme), screens (title_art), docs, tests |
| GitHub Issue | #98 |
| PRD Phase | N/A (source: `docs/cabinet-theme/IMPLEMENTATION_PLAN.md` 3.8 polish) |
| Prototype Patch | N/A |

## Environment Findings

| Probe | Result |
|-------|--------|
| Host build | `build/` Release, GCC; `ctest --test-dir build` -> 100% passed out of 53 (green baseline) |
| Scanline call sites | `title_art::draw_scanlines` (`src/screens/title_art.cpp:84`) is the one definition; `select_art`, `results_art` and `setup_art` forward to it; screens call it from `title_screen.cpp`, `attract_screen.cpp`, `select_screen.cpp`, `results_screen.cpp`, `calibration_screen.cpp`, `input_remap_screen.cpp`. Gameplay draws none. |
| `bitmap_font` users | `src/gameplay/judgment_animator.cpp:213` (fallback when the theme/manifest entry is missing), `src/render/ttf_font.cpp` (fallback when a TTF face is missing), `src/screens/song_display_text.cpp` (`font_covers_text`, `truncate_to_cells` helpers), `src/screens/select_placeholder_screen.cpp` (not registered in `src/main.cpp`; only tests use it) |
| Headless draw check | with `GlQuadRenderer` uninitialized, a loaded `ThemeTextures` has no GL textures, so `draw_tiled` records one flat full-rect quad (`draw_flat`, `src/render/theme_textures.cpp:1028`); `recorded_quads()` counts it. So the toggle is testable without GL. |
| Pre-theme baseline | `a2ec06c` (merge of #88, before `bfc90e4` #89 started loading the theme). Build in a scratch git worktree outside the repo. |
| Verify harness | `.claude/skills/verify/scripts/b4k.sh` hardcodes `build/blaze-4k`; it opens a floating 1280x720 window. 2560x1440 needs `video.width/height` in the run's config plus an `i3-msg` resize of the harness window only. |

## Patterns to Follow

### Naming / config fields
```
// SOURCE: src/data/config.hpp:22-27
struct VideoSettings {
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool fullscreen = false;
};
```

### Config load / save
```
// SOURCE: src/data/config_loader.cpp:188-189 and 396-401
read_bool_field(*node, "vsync", target.vsync, warnings, "video");
read_bool_field(*node, "fullscreen", target.fullscreen, warnings, "video");
...
{"fullscreen", config.video.fullscreen},
```

### Tests
```
// SOURCE: tests/config_persistence_test.cpp:75-76 and 136-137,159-160
TEST_CHECK(defaults.video.vsync);
config.video.fullscreen = true;
TEST_CHECK(loaded.video.fullscreen);
```

### Headless recorder test
```
// SOURCE: tests/select_art_test.cpp:884-905
blaze4k::GlQuadRenderer renderer;   // uninitialized: draws are recorded
renderer.clear_recorded();
...
const auto quads = renderer.recorded_quads();
TEST_CHECK(quads.size() == 2);
```

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/data/config.hpp` | UPDATE | `VideoSettings::scanlines = true` |
| `src/data/config_loader.cpp` | UPDATE | read and save `video.scanlines` |
| `tests/config_persistence_test.cpp` | UPDATE | default, round-trip, bad-type warning |
| `src/render/theme_textures.hpp/.cpp` | UPDATE | `set_scanlines_enabled` / `scanlines_enabled` on `ThemeTextures` (default true) |
| `src/app/app.hpp`, `src/app/app.cpp` | UPDATE | `AppConfig::scanlines`; `App::init` pushes it into `theme_textures_` |
| `src/main.cpp` | UPDATE | copy `game_config.video.scanlines` into `config.scanlines` |
| `src/screens/title_art.cpp` | UPDATE | `draw_scanlines` returns early when disabled |
| `tests/title_screen_test.cpp` | UPDATE | recorder test: enabled draws the overlay, disabled draws nothing |
| `src/render/bitmap_font.hpp` | UPDATE | header comment: fallback text path, not used by any themed player-facing text |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | Cabinet-theme pass, perf numbers, screenshot index, `bitmap_font` and `video.scanlines` notes |
| `docs/cabinet-theme/screenshots/*.png` | CREATE | 1280x720 and 2560x1440 screenshots of each screen |
| `README.md` | UPDATE | one line about the `video.scanlines` config key (settings are listed there) |

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Config field, load, save

- **File**: `src/data/config.hpp`, `src/data/config_loader.cpp`
- **Action**: UPDATE
- **Implement**: add `bool scanlines = true;` to `VideoSettings` (comment: Cabinet scanlines overlay, config-file only). Add `read_bool_field(*node, "scanlines", target.scanlines, warnings, "video");` in `read_video` and `{"scanlines", config.video.scanlines}` in `save_config`.
- **Mirror**: `src/data/config_loader.cpp:188-189,400`
- **Validate**: `cmake --build build -j"$(nproc)"`

### Task 2: Config tests

- **File**: `tests/config_persistence_test.cpp`
- **Action**: UPDATE
- **Implement**: assert the default is true (section 1); set it false in the round-trip (section 2) and assert it loads false; add a case where `"video": {"scanlines": "yes"}` keeps the default and the load message names `video.scanlines` (mirror the existing wrong-type cases in the file). Hard-code literal expected values.
- **Mirror**: `tests/config_persistence_test.cpp:75,136,159`
- **Validate**: `ctest --test-dir build --output-on-failure -R config_persistence`

### Task 3: Make the overlay honour the toggle

- **File**: `src/render/theme_textures.hpp`, `src/render/theme_textures.cpp`, `src/screens/title_art.cpp`, `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**: `ThemeTextures` gets a `bool scanlines_enabled_ = true;` with setter and getter; `shutdown()` leaves it alone. `title_art::draw_scanlines` returns before `draw_tiled` when `!theme.scanlines_enabled()`. `AppConfig` gets `bool scanlines = true;`; `App::init` calls `theme_textures_.set_scanlines_enabled(config_.scanlines)` next to `theme_textures_.load(...)`; `main.cpp` sets `config.scanlines = game_config.video.scanlines;` beside the other video copies.
- **Mirror**: `src/main.cpp:198-199`, `src/app/app.cpp:52`
- **Validate**: `cmake --build build -j"$(nproc)"`

### Task 4: Overlay test

- **File**: `tests/title_screen_test.cpp`
- **Action**: UPDATE
- **Implement**: add `test_scanlines_toggle` and call it from `main`. Load a `ThemeTextures` from the real theme dir (headless: no GL), uninitialized renderer. Enabled: `title_art::draw_scanlines(theme, renderer, 1280, 720, 1.0f)` records exactly one quad whose corners span {0,0}-{1280,720}. Disabled: zero quads. Re-enabled: one again. Also assert `select_art::draw_scanlines` and `results_art::draw_scanlines` record zero when disabled (they forward).
- **Mirror**: `tests/select_art_test.cpp:884-905`
- **Validate**: `ctest --test-dir build --output-on-failure -R title_screen`. To prove it fails on broken code, temporarily remove the early return from `draw_scanlines`, rebuild, see the test fail, restore it.

### Task 5: Resolve `bitmap_font`

- **File**: `src/render/bitmap_font.hpp`
- **Action**: UPDATE
- **Implement**: no deletion (`ttf_font.cpp` fallback, `judgment_animator.cpp` fallback and `song_display_text.cpp` helpers use it; `SelectPlaceholderScreen` is test-only and not registered in `src/main.cpp`). Extend the header comment: this is the fallback and debug text path, drawn only when a theme asset or TTF face is missing; themed screens draw text through `TextRenderer`. Re-grep to confirm no registered screen calls `draw_text` / `draw_text_centered` directly: `grep -rn "draw_text" src --include=*.cpp`.
- **Validate**: `scripts/check.sh` (later); comment only here, `cmake --build build -j"$(nproc)"`

### Task 6: Perf comparison

- **File**: none (CHECK); results go into the docs in Task 8
- **Action**: CHECK
- **Implement**: `git worktree add /tmp/blaze4k-baseline a2ec06c`, configure and build Release there (`cmake -B build -DCMAKE_BUILD_TYPE=Release`, same generator; if FetchContent needs the network and fails, point `FETCHCONTENT_SOURCE_DIR_*` at `build/_deps/*-src`). Run the same scripted path on both binaries: play one full chart (fixture pack on the stub clock, or one `songs/` chart with real audio, same chart both times), reaching Results, then quit cleanly, with `--perf-report --no-vsync` so the numbers show CPU/GPU cost, not the vsync period. Drive the new build with the `verify` skill (`launch ... -- --perf-report --no-vsync`, keys, `stop`); drive the baseline binary the same way by running it from its own build dir with a scratch `--data-dir` and `xdotool`-free key sending via `.claude/skills/verify/scripts/sendkey.py` against its window id. Run each three times; record the median row. Also compare the gameplay quad count per frame if cheap to get (`perf_loop_test` output). Remove the worktree afterwards.
- **Validate**: gameplay `p99` and `median` after <= before (plus noise of 10%); if after is clearly worse, stop and report rather than hide it.

### Task 7: Screenshots

- **File**: `docs/cabinet-theme/screenshots/{720p,1440p}-<screen>.png`
- **Action**: CREATE
- **Implement**: with the `verify` skill (`features/*.md` recipes), capture Title, Attract (`-- --attract-timeout 3`), Select, Options overlay, Gameplay, Results, Calibration and Input Remap at 1280x720 (default window) and at 2560x1440 (write `{"video":{"width":2560,"height":1440}}` to `RUN/data/config.json` before launch, then `i3-msg "[id=<wid>] resize set 2560 1440"` on the harness window only). Read each PNG to confirm the screen and size (`identify`). Keep the PNGs small enough to commit (optimize with ImageMagick `-strip` if > 1 MB). Also capture Select with `video.scanlines` false once, to prove the toggle in the real window (log + shot). Use `$B stop RUN` after every attempt. Never move the owner's windows.
- **Validate**: `identify docs/cabinet-theme/screenshots/*.png` shows the expected sizes.

### Task 8: Docs

- **File**: `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md`
- **Action**: UPDATE
- **Implement**: add a "Cabinet theme pass (#98)" section: Linux machine-checked rows (tests, `--perf-report` before/after table with host, GPU, resolution, chart; screenshot index; `video.scanlines` proof; `bitmap_font` decision) and unchecked owner steps for Windows and macOS (build presets, launch, visit each screen at 1280x720 and 2560x1440, compare against the Linux screenshots, scanlines toggle, perf report) marked PENDING owner. Cite code by file and symbol name only; no test counts (`doc-test-count`) and no line numbers (`doc-line-citation`). README: one line that `config.json` `video.scanlines` turns the overlay off.
- **Validate**: `scripts/lint.sh`

### Task 9: Full check

- **File**: none
- **Action**: CHECK
- **Implement**: `scripts/check.sh`; `scripts/format.sh --check`.
- **Validate**: exits 0.

## Validation

```bash
# Build
cmake --build build -j"$(nproc)"

# Lint + warnings-as-errors build + all tests
scripts/check.sh

# Tests
ctest --test-dir build --output-on-failure
```

## End-to-End Verification

1. Build `build/` Release, `doctor` via `.claude/skills/verify/scripts/b4k.sh doctor`.
2. `launch --run /tmp/blaze4k-verify/theme-polish` with default config: Title screenshot shows scanlines. `stop`, then check `RUN/evidence/config.json` contains `"scanlines": true`.
3. Pre-seed `RUN/data/config.json` with `{"video":{"scanlines":false}}`, launch, screenshot Select: no scanline texture; the `game.log` has no config warning. Stop and confirm the saved config keeps `"scanlines": false`.
4. Run Task 6's perf comparison and Task 7's screenshots.

## Risks

| Risk | Mitigation |
|------|------------|
| Baseline worktree cannot fetch dependencies offline | Reuse `build/_deps/*-src` through `FETCHCONTENT_SOURCE_DIR_*`; in scope |
| Perf run on a window the owner can cover; harness hardcodes `build/blaze-4k` | Run the baseline binary directly with its own data dir; keep windows unmoved; in scope |
| Fixture songs play on a stub clock, so frame costs include no audio | State the clock source in the doc; use an owner `songs/` chart for the perf runs if available; in scope |
| 2560x1440 capture depends on i3 resize and the display size | If the screen cannot hold 2560x1440, report the missing shots as such; flag, do not fake |
| Keeping `bitmap_font` leaves a placeholder screen in the tree | Out of scope; flagged in the doc as test-only |
| Windows/macOS cannot be run here | Listed as owner steps, status PENDING |

## Acceptance Criteria

- [ ] `video.scanlines` (default on) persists, toggles the overlay on every screen, and is covered by config and overlay tests
- [ ] `--perf-report` over a full song shows no regression versus the pre-theme baseline; numbers are in the doc
- [ ] `docs/CROSS_PLATFORM_VERIFICATION.md` has the Cabinet-theme pass, owner steps, and screenshots at both sizes
- [ ] `bitmap_font` is documented as fallback/debug-only (no themed player-facing screen uses it)
- [ ] `scripts/check.sh` passes
- [ ] Follows existing patterns
