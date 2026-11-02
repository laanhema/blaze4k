# Implementation Report

**Plan**: `.agents/plans/completed/074-sort-song-wheel-plan.md`
**Branch**: `feature/074-sort-song-wheel`
**Status**: COMPLETE

## Summary

Implemented deterministic alphabetical sorting for the song select wheel matching StepMania 5 `SongUtil::MakeSortString` semantics. Song packs are sorted alphabetically A–Z by name (case-insensitive, tie-broken by case-sensitive name then path). Songs within each pack are sorted alphabetically A–Z with transliterated title preference, subtitle tie-break, leading period stripping (`.59`), and non-alphanumeric sorting after Z (via `~`). Sorting is executed automatically upon scanning in `SongLibrary::scan_directory`, and pure helper functions `sort_songs` and `sort_song_library` allow direct unit testing on unsorted vectors.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Library sorting helpers and scan hook | `src/chart/song_library.hpp`, `src/chart/song_library.cpp` | PASS |
| 2 | Unit tests for unsorted input in `song_library_test` | `tests/song_library_test.cpp` | PASS |
| 3 | Update verification skill documentation | `.claude/skills/verify/features/song-select.md` | PASS |
| 4 | Tick `TODO.md` | `TODO.md` | PASS |

## Validation Results

| Check | Result | Command / environment |
|-------|--------|-----------------------|
| Type check / Build | PASS | `cmake --build build -j"$(nproc)"` — host |
| Lint | PASS | `scripts/check.sh` (step 1/5) — host |
| Format | PASS | `scripts/check.sh` (step 2/5) — host |
| Tests | PASS (53 passed) | `scripts/check.sh` (step 5/5) — host |
| Sanitizers (ASan/UBSan) | PASS (2 passed) | `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan -R "song_library|select_screen"` — host |
| E2E 1 (Headless Unit Check) | PASS | `ctest --test-dir build -R "song_library_test|select_screen_test" --output-on-failure` — host |
| E2E 2 (Visual Verification) | PASS | `.claude/skills/verify/scripts/b4k.sh` launch + select wheel navigation + gameplay transition + selection restore — host X11 DISPLAY :0 |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/chart/song_library.hpp` | UPDATE | +10/-0 |
| `src/chart/song_library.cpp` | UPDATE | +88/-0 |
| `tests/song_library_test.cpp` | UPDATE | +64/-0 |
| `.claude/skills/verify/features/song-select.md` | UPDATE | +1/-1 |
| `TODO.md` | UPDATE | +1/-1 (gitignored, local in-place edit) |

## Deviations from Plan

- `TODO.md` is gitignored per repository policy and was updated locally in place without being staged or committed.
- Applied `scripts/format.sh` (`.clang-format` 100-character line wrap) to `tests/song_library_test.cpp` after applying the prototype patch to satisfy `scripts/check.sh` formatting check.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/song_library_test.cpp` | Section 8: `Validate sorting of packs and songs (Issue #158)`: Unsorted pack collation (case-insensitive A-Z), out-of-order song sorting including case-insensitivity (`apple`), subtitle tie-break (`Banana` Part 1 vs Part 2), leading period strip (`.59`), transliterated title preference (`漢字` / `Cherry`), and non-alphanumeric trailing sort (`★Special`). |
