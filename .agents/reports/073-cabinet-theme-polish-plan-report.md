# Implementation Report

**Plan**: `.agents/plans/completed/073-cabinet-theme-polish-plan.md`
**Branch**: `feature/073-cabinet-theme-polish` (uncommitted; nothing committed or pushed)
**Status**: COMPLETE (Windows, macOS and the 2560x1440 captures are PENDING owner steps)

## Summary

Added the persisted `video.scanlines` toggle (default on, config file only) that `title_art::draw_scanlines` honours through a flag on `ThemeTextures` set from `AppConfig::scanlines`. Compared `--perf-report` and CPU/quad cost against the pre-theme build `a2ec06c`, captured screenshots of every screen, added a Cabinet-theme pass to `docs/CROSS_PLATFORM_VERIFICATION.md`, and kept `bitmap_font` documented as the fallback/debug text path.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Config field, load, save | `src/data/config.hpp`, `src/data/config_loader.cpp` | PASS |
| 2 | Config tests | `tests/config_persistence_test.cpp` | PASS |
| 3 | Overlay honours the toggle | `src/render/theme_textures.hpp`, `src/screens/title_art.cpp`, `src/app/app.*`, `src/main.cpp` | PASS |
| 4 | Overlay test (negative control run: fails with the early return removed) | `tests/title_screen_test.cpp` | PASS |
| 5 | Resolve `bitmap_font` (kept, documented) | `src/render/bitmap_font.hpp` | PASS |
| 6 | Perf comparison | docs | PASS (see deviations: CPU is higher in gameplay) |
| 7 | Screenshots | `docs/cabinet-theme/screenshots/` | PARTIAL: 1280x720 and 1920x1080 captured; 2560x1440 PENDING owner |
| 8 | Docs | `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md` | PASS |
| 9 | Full check | n/a | PASS |

## Validation Results

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Build | PASS | `scripts/check.sh` step 4, warnings as errors, host GCC |
| Lint + format | PASS | `scripts/check.sh` steps 1-2 (format fixed once with `scripts/format.sh`) |
| Tests | PASS (all, 53) | `scripts/check.sh` step 5, host |
| E2E default run | PASS | verify harness: scanlines visible, saved config has `"scanlines": true` |
| E2E scanlines off | PASS | `config.json` `{"video":{"scanlines":false}}`: overlay gone (pixel diff vs default 8466 px of the 1280x720 title), saved config keeps `false`, no config warning |
| Perf, pre-theme vs theme | PASS for frame time; CPU higher (see below) | 4 interleaved runs each, `--perf-report --no-vsync` |
| Windows / macOS | PENDING | owner checklist in `docs/CROSS_PLATFORM_VERIFICATION.md` |
| 2560x1440 screenshots | PENDING | host display is 1920x1080; an X11 window larger than the screen cannot be read back |

## Perf numbers (fixture pack, stub clock, Blaze Anthem BEGINNER 1, Mesa/AMD radeonsi)

| | Pre-theme `a2ec06c` | Cabinet theme |
|---|---|---|
| median frame | 16.65 ms (4/4 runs) | 16.65 ms (4/4) |
| p99 | 17.27, 17.34, 17.36, 19.08 ms | 17.16, 17.29, 17.80, 18.55 ms |
| max (first frame) | 148-176 ms | 250 ms (4/4) |
| quads / gameplay frame | 295.5 | 34.8 |
| draw calls / gameplay frame | 7.3 | 11.7 |
| CPU during gameplay (~5.9 s) | 0.25-0.29 s | 0.34-0.38 s |
| CPU whole run | 0.85-0.88 s | 1.63-2.11 s |

Frame time shows no regression. Gameplay draws far fewer quads but issues more draw calls, and CPU time in gameplay is about 0.09 s per 5.9 s higher; startup is heavier (theme load, font bake). The acceptance wording "gameplay is no more expensive" is met for quads and frame time, not for CPU time.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `README.md` | UPDATE | +2 |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | UPDATE | +95 |
| `src/app/app.cpp`, `src/app/app.hpp`, `src/main.cpp` | UPDATE | +1 each |
| `src/data/config.hpp` | UPDATE | +2 |
| `src/data/config_loader.cpp` | UPDATE | +4/-4 (formatter reflowed the video table) |
| `src/render/bitmap_font.hpp` | UPDATE | +7 |
| `src/render/theme_textures.hpp` | UPDATE | +7 |
| `src/screens/title_art.cpp` | UPDATE | +3 |
| `tests/config_persistence_test.cpp` | UPDATE | +23 |
| `tests/title_screen_test.cpp` | UPDATE | +31 |
| `docs/cabinet-theme/screenshots/*.png` | CREATE | 18 PNGs (5.7 MB) |
| `.agents/plans/073-cabinet-theme-polish-plan.md` | CREATE (archived) | |

## Deviations from Plan

- 2560x1440 screenshots not captured (display is 1920x1080); 1920x1080 (1.5x scale) captured instead, 2560x1440 listed as an owner step.
- `--no-vsync` still runs at about 60 fps (the loop is paced), so frame time cannot show cost; added CPU time from `/proc` and quads/draw calls per frame (a temporary probe in `GlQuadRenderer`, reverted) as the cost measures.
- Baseline run through a copy of the verify harness scripts inside the scratch worktree `/tmp/blaze4k-baseline` (removed afterwards), not by key-sending with `sendkey.py` directly. It opened the old UI on the owner's workspace, which the owner noticed and asked about.
- Fixture pack on the stub clock instead of a real song, to keep the runs silent.
- The overlay test covers `title_art::draw_scanlines` and `select_art::draw_scanlines`; `results_art::draw_scanlines` returns early on an uninitialised renderer, so it cannot be recorded headlessly.
- Extra screenshots: Attract and a scanlines-off title.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/config_persistence_test.cpp` | default on, round trip off, wrong-type warning, explicit false loads without warning |
| `tests/title_screen_test.cpp` | `test_scanlines_toggle`: one full-window quad when on, none when off (title and select entry points), back on |
