# Implementation Report

**Plan**: `.agents/plans/023-dimmed-background-fallback-plan.md`
**Branch**: `feature/023-dimmed-background`
**Status**: COMPLETE (uncommitted, per git policy)

## Summary

Renders the selected song's simfile background behind the note field during gameplay,
cover-fit and dimmed (`0.35` image alpha + `0.30` black scrim) so arrows stay readable. Songs
lacking art resolve to a **real committed asset** `assets/backgrounds/fallback.png` (dark vertical
gradient #1a2438 -> #05070f, 1024x576, 2.9 KiB) wired through `SongLibrary::set_fallback_background`
before the scan. The asset is discoverable at runtime via cwd-relative candidates plus an
executable-relative path, and CMake copies `assets/` next to the binary (POST_BUILD) so a fresh
`build/` run finds it. The demo path resolves the simfile's own bg tag and falls back to the same
asset. `BackgroundRenderer` renders the resolved path (song art or committed fallback) via
`Texture::from_file`; when the path is empty or the load fails/headless it draws only a dark solid +
scrim (no procedural gradient).

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | `BackgroundRenderer` (image load + cover-fit + dim/scrim) | `src/render/background_renderer.hpp/.cpp` | ✅ |
| 2 | `GameplayView` background integration | `src/gameplay/gameplay_view.hpp/.cpp` | ✅ |
| 3 | Pass resolved background from screen + demo, wire fallback asset | `src/screens/gameplay_screen.cpp`, `src/main.cpp` | ✅ |
| 3b | Asset-path candidate resolver | `src/data/data_paths.hpp/.cpp` | ✅ |
| 4 | Register source + test target + asset copy | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 5 | `background_test` | `tests/background_test.cpp` | ✅ |
| 5b | Fallback asset + committed PNG assertions | `tests/song_library_test.cpp` | ✅ |
| 6 | Full suite + warning budget | — | ✅ |
| 7 | Commit fallback asset | `assets/backgrounds/fallback.png` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ clean (0 warnings) |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 28/28 passed |
| Warning budget (`grep -i warning` on recompiled TUs) | ✅ NO WARNINGS |
| 2D-only (`rg "video|3D|glad" src/render/background_renderer.hpp`) | ✅ no matches |
| E2E 1 song with art (Glacier Groove) | ✅ exit 0; `background '<...>/bg.png'` logged |
| E2E 2 song without art (Aurora Borealis) | ✅ exit 0; committed fallback path logged |
| E2E build-dir discovery (run from `build/`) | ✅ resolves `assets/backgrounds/fallback.png` |
| E2E scan (`--songs tests/fixtures/reference_pack`) | ✅ 4 songs / 9 charts, exit 0 |

### Verbatim key output

```
$ ./build/blaze-4k --headless --smoke-test 30 \
    --gameplay-demo "tests/fixtures/reference_pack/Blaze Pack/Glacier Groove/Glacier Groove.sm"
[main] Fallback background: assets/backgrounds/fallback.png
[BackgroundRenderer] background 'tests/fixtures/reference_pack/Blaze Pack/Glacier Groove/bg.png'
[BackgroundRenderer] background texture unavailable (headless/no GL or decode failure); scrim-only fallback
[GameplayView] Loaded chart 'Medium' (meter 4): taps=4 holds=0 rolls=0 mines=0 total=4
Blaze 4k shut down cleanly.        (EXIT=0)

$ ./build/blaze-4k --headless --smoke-test 30 \
    --gameplay-demo "tests/fixtures/reference_pack/Blaze Pack/Aurora Borealis/Aurora Borealis.sm"
[main] Fallback background: assets/backgrounds/fallback.png
[BackgroundRenderer] background 'assets/backgrounds/fallback.png'
[BackgroundRenderer] background texture unavailable (headless/no GL or decode failure); scrim-only fallback
[GameplayView] Loaded chart 'Easy' (meter 2): taps=4 holds=0 rolls=0 mines=0 total=4
Blaze 4k shut down cleanly.        (EXIT=0)
```

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `assets/backgrounds/fallback.png` | CREATE | 1024x576 PNG, 2945 bytes |
| `src/render/background_renderer.hpp` | CREATE | +43 |
| `src/render/background_renderer.cpp` | CREATE | +99 |
| `tests/background_test.cpp` | CREATE | +141 |
| `src/data/data_paths.hpp` | UPDATE | +8 |
| `src/data/data_paths.cpp` | UPDATE | +14 |
| `src/gameplay/gameplay_view.hpp` | UPDATE | +4/-1 |
| `src/gameplay/gameplay_view.cpp` | UPDATE | +8/-1 (approx; net from git stat) |
| `src/screens/gameplay_screen.cpp` | UPDATE | +3/-1 |
| `src/main.cpp` | UPDATE | +36/-2 |
| `CMakeLists.txt` | UPDATE | +10 |
| `tests/CMakeLists.txt` | UPDATE | +16 |
| `tests/song_library_test.cpp` | UPDATE | +1 |

## Deviations from Plan

The user overrode the plan's open questions; these decisions win and drive the deviations:

1. **OQ1 overridden — real committed PNG, not a procedural gradient.** Added
   `assets/backgrounds/fallback.png` (generated with python3 PIL: vertical gradient #1a2438 ->
   #05070f, 1024x576). `BackgroundRenderer` renders whatever resolved path it is given
   (`song.resolved_background_path`, which the scanner fills with either song art or the committed
   fallback). No gradient texture is built; the `fallback_texture()` gradient accessor was dropped
   and `init()` was removed (nothing to build). The only safety path is a dark solid + scrim when
   the path is empty/load fails/headless.
2. **OQ4 overridden — `set_fallback_background` is wired.** `main` resolves the asset via
   `blaze4k::resolve_first_existing({cwd candidates..., exe_dir/assets/backgrounds/fallback.png})` and
   calls `library.set_fallback_background(...)` **before** `library.scan_directory(...)`.
   `set_fallback_banner` is untouched.
3. **New helper `blaze4k::resolve_first_existing`** in `src/data/data_paths.hpp/.cpp` (not in the plan's
   file list) to mirror the existing `data/judgment_constants.json` / `assets/data/...` candidate
   discovery pattern; used by `main` and directly unit-tested. This is the minimal shared seam for
   runtime-plus-test asset discovery.
4. **CMake POST_BUILD asset copy** added (not in the plan) so running the binary from `build/`
   finds `assets/backgrounds/fallback.png`.
5. **Demo parity** — `--gameplay-demo` resolves `metadata().background_path` relative to the simfile
   and falls back to the committed asset when absent.
6. **Test `BLAZE4K_ASSETS_DIR` compile definition** added for `background_test` so the committed asset
   is found from the out-of-tree test cwd (`build/tests`).
7. **`GameplayView::init` background load is just `background_.load(path)`** (no `init()` call),
   since there is no procedural texture to build.
8. OQ2 (`0.35` dim + `0.30` scrim), OQ3 (cover-fit), OQ5 (gameplay-only) implemented as recommended.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/background_test.cpp` | `cover_uv` equal aspect (full UV); tall 4:3-on-16:9 (crop vertical, v 0.125/0.875); wide 16:9-on-4:3 (crop horizontal, u 0.125/0.875); degenerate zero/negative dims -> full UV; headless load/render/shutdown safe no-ops; `resolve_first_existing` candidate order + empty; committed `fallback.png` exists, non-empty, is a regular file, decodes (1024x576) |
| `tests/song_library_test.cpp` | Extended Song 4 assertion: a song without art resolves to the fallback background path and that path exists on disk |

## Notes

- Work is left **uncommitted** on `feature/023-dimmed-background` per the git policy in the invoking
  request.
- The committed fallback is intentionally tiny/compressible (2.9 KiB) so it stays clone-friendly.
