# Implementation Report

**Plan**: `.agents/plans/completed/039-import-cabinet-theme-pack-plan.md`
**Branch**: `feature/039-import-cabinet-theme-pack`
**Status**: COMPLETE

## Summary

Moved the untracked Cabinet theme pack (`blaze4k-cabinet-theme/`, 80 files) into the tracked repo
layout: fonts + OFL licences to `assets/fonts/`, 65 PNGs + `manifest.json` to
`assets/theme/cabinet/`, `theme.hpp` to `src/render/`, and the pack README, implementation plan
and `reference/` mock-ups to `docs/cabinet-theme/`. All 80 files were verified byte-identical by
SHA-256 before the pack README intro was edited, and the emptied pack folder was removed
(empty-directory delete only). Added `tests/theme_test.cpp`, which compiles `theme.hpp`
(`static_assert`s) and checks the committed pack is complete. README gained the two OFL licence
lines and a Cabinet theme docs link; `docs/BUILDING.md` lists the new asset folders.
`CMakeLists.txt` is unchanged. The changes are staged with explicit paths and not committed.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Snapshot SHA-256 of the pack (80 files) | scratch only | ✅ |
| 2 | Move files into the repo layout; `sha256sum -c` 80/80 OK | `assets/fonts/`, `assets/theme/cabinet/`, `src/render/theme.hpp`, `docs/cabinet-theme/` | ✅ |
| 3 | Remove the emptied pack folder (`find -type f` empty, then `-empty -delete`) | `blaze4k-cabinet-theme/` | ✅ |
| 4 | Add pack/header test | `tests/theme_test.cpp` | ✅ |
| 5 | Register `theme_test` with `BLAZE4K_ASSETS_DIR` | `tests/CMakeLists.txt` | ✅ |
| 6 | Reword pack README intro/tree to repo paths | `docs/cabinet-theme/README.md` | ✅ |
| 7 | OFL licence lines + docs link; BUILDING asset list | `README.md`, `docs/BUILDING.md` | ✅ |
| 8 | Full validation, E2E, explicit-path staging | — | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`) | ✅ |
| Lint (no new warnings from `theme_test.cpp` / `theme.hpp`) | ✅ "no theme warnings" |
| Tests (sandboxed ctest, `build/`) | ✅ 100% of 42 passed |
| Checksums (80 moved files) | ✅ 80/80 OK |
| Pack layout sanity (6 fonts, 65 PNGs, manifest, theme.hpp, docs + 5 reference images, pack folder gone) | ✅ |
| E2E 1: forced relink copies assets (6 fonts, 65 PNGs, manifest `cmp` equal), `CMakeLists.txt` unchanged | ✅ |
| E2E 2: offline fresh-tree build + sandboxed ctest | ✅ 42/42 (see Deviations for the build-dir location) |
| Negative check (scratch): missing texture PNG and orphan PNG each make `theme_test` abort | ✅ |
| `.agents/stories/todo-stories.md` still ` M` (unstaged, untouched); `TODO.md` untouched | ✅ |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `assets/fonts/*.ttf` (4) | CREATE (move) | binary |
| `assets/fonts/OFL-Audiowide.txt`, `OFL-SairaCondensed.txt` | CREATE (move) | +93 each |
| `assets/theme/cabinet/*.png` (65) | CREATE (move) | binary |
| `assets/theme/cabinet/manifest.json` | CREATE (move) | moved unchanged |
| `src/render/theme.hpp` | CREATE (move) | +222 |
| `docs/cabinet-theme/README.md` | CREATE (move + intro edit) | +72 |
| `docs/cabinet-theme/IMPLEMENTATION_PLAN.md` | CREATE (move) | +171 |
| `docs/cabinet-theme/reference/*` (5 images) | CREATE (move) | binary |
| `tests/theme_test.cpp` | CREATE | +146 |
| `tests/CMakeLists.txt` | UPDATE | +17/-0 |
| `README.md` | UPDATE | +3/-0 |
| `docs/BUILDING.md` | UPDATE | +3/-2 |
| `blaze4k-cabinet-theme/` | DELETE (untracked, emptied) | — |

84 paths staged (80 moved + 4 new/edited).

## Deviations from Plan

1. **E2E 2 build-dir location.** The plan's literal command builds at `$S/build`, a sibling of
   the exported tree `$S/src`. That run built fine and copied 65 PNGs, but ctest gave 39/42:
   `parser_hardening_test`, `score_keeper_test` and `metronome_sync_test` failed with
   `Assertion failed at .../tests/parser_hardening_test.cpp:90: std::filesystem::exists(ref_pack_path)`
   (and the equivalent `exists(ref)` checks at `score_keeper_test.cpp:675` and
   `metronome_sync_test.cpp:48`). These tests find their tracked fixtures through cwd-relative
   paths (`tests/…`, `../tests/…`, `../../tests/…`), which only resolve when the build dir is
   inside the source tree. This is pre-existing and unrelated to this change; the fixtures are
   tracked and were present in the export. Re-ran with the AC's literal layout (`cmake -B build`
   inside the exported tree, same offline `FETCHCONTENT_SOURCE_DIR_*` overrides, `--unshare-net`):
   build OK, 65 PNGs and 6 fonts copied, **42/42 passed**, no theme warnings.
2. **Configure-time CMake deprecation warning** (`Update the VERSION argument <min> value…`) appears
   in the configure output; it is a pre-existing warning from a fetched dependency's
   `cmake_minimum_required`, not from this change.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/theme_test.cpp` | 4 compile-time `static_assert`s on `theme.hpp` (`hex` colour/alpha, `kFontFiles.size()`, reference resolution); `test_font_files_present` (4 TTFs exist, non-empty, sfnt magic `00 01 00 00`; both OFL texts exist); `test_manifest_textures` (63 entries, known `kind`, file exists, `probe_image_header` ok and equals `size_px`); `test_manifest_bitmap_fonts` (2 atlases, files exist and probe ok, glyphs cover `0-9 . % /` and space); `test_no_orphan_pngs` (every one of the 65 cabinet PNGs is referenced) |

## Follow-ups (flagged, out of scope)

- POST_BUILD asset copy only runs when `blaze-4k` relinks, so incremental builds can miss new assets (flag for #89).
- `.agents/stories/todo-stories.md` and issue bodies still mention `blaze4k-cabinet-theme/…` paths (file is off-limits here).
- Texture/mock-up licence: plan default followed (no extra note); owner to confirm.
- Tests using cwd-relative fixture lookup fail when the build dir is outside the source tree (Deviation 1); consider a `BLAZE4K_FIXTURES_DIR`-style compile definition later.
