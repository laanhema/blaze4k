# Code Review: feature/074-sort-song-wheel

**Scope**: branch `feature/074-sort-song-wheel` (including uncommitted working-tree changes) against merge base with `main` @ `7c976d8`  
**Recommendation**: APPROVE (no findings)

## Summary

Reviewed deterministic alphabetical sorting for song packs and songs on the song select wheel implemented across `src/chart/song_library.{hpp,cpp}`, unit tests in `tests/song_library_test.cpp`, and updated verification documentation in `.claude/skills/verify/features/song-select.md`. The implementation follows StepMania 5 `SongUtil::MakeSortString` semantics (uppercase normalization, leading period removal for `.59`, and non-alphanumeric trailing collation via `~`), uses pure free helpers (`sort_song_library`, `sort_songs`) that make sorting testable without relying on OS directory ordering, and integrates sorting automatically into `SongLibrary::scan_directory`. All acceptance criteria from issue #158 are verified and met:

- **Packs are listed alphabetically on the song select wheel**: Met. `SongLibrary::scan_directory` invokes `sort()`, which sorts packs case-insensitively A–Z using `make_sort_string` (with case-sensitive and directory tie-breakers). `SelectScreen::rebuild` presents packs in library order.
- **Songs inside each pack are listed alphabetically A–Z by title**: Met. `sort_songs` orders songs using StepMania 5 `CompareSongPointersByTitle` precedence (transliterated title preferred, subtitle tie-break, raw title/subtitle tie-break, and `simfile_path` final tie-break).
- **The order does not depend on filesystem directory order**: Met. `tests/song_library_test.cpp` section 8 feeds unsorted packs (`Zeta Pack`, `Alpha Pack`, `mid Pack`) and songs out-of-order (`Zebra`, `apple`, `Banana Part 2`, `Banana Part 1`, `.59`, `★Special`, `漢字` with transliterated title `Cherry`) to `sort_song_library`, asserting exact sorted order; without sorting, the test fails immediately.
- **Returning to Select from Gameplay, Results, Calibration or Input remap still restores the highlighted song and chart**: Met. `SelectScreen::rebuild` restores cursor and chart selection by matching unique `simfile_path` and pack name; verified both by `select_screen_test` (`test_selection_preserved_on_reenter`) and by live driving via `.claude/skills/verify/scripts/b4k.sh`.
- **The song-select gotcha in `.claude/skills/verify/features/song-select.md` that says songs are listed in directory order is updated**: Met. Updated line 40 to state that packs and songs are listed alphabetically A–Z.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (step 4/5, `-DBLAZE_WERROR=ON`, GCC 16 threads) — host |
| Warnings | NONE | Recompiled changed files (`src/chart/song_library.cpp` and `tests/song_library_test.cpp`) with `-Wall -Wextra -Wpedantic -Werror -c -o /dev/null` — 0 warnings |
| Lint | PASS | `scripts/check.sh` (step 1/5 lint `scripts/lint.sh`, step 2/5 formatting check `scripts/format.sh --check`) — host |
| Tests | PASS | `scripts/check.sh` (step 5/5, 53/53 passed); `ctest --preset asan-ubsan` (55/55 passed, zero memory or UB errors) |
| UI verification | PASS | `.claude/skills/verify/scripts/b4k.sh` launch + select navigation + gameplay transition + selection restore (1280x720 window, X11 DISPLAY :0; captures in `/tmp/blaze4k-verify/review-074/evidence/01-title.png` through `06-select-restored.png`) |

## What's Good

- **Faithful adherence to reference behavior**: Collation logic strictly adheres to StepMania 5 `SongUtil::MakeSortString` and `CompareSongPointersByTitle` (AGENTS.md Principle 2), including stripping leading periods (`.59`), transliteration preference, and sorting symbols after `Z` (`~`).
- **Testability through pure functions**: Decoupling sort algorithms into free functions `sort_songs` and `sort_song_library` allows unit testing on synthetic vectors independently of host filesystem iteration behavior.
- **Rigorous tie-breaking**: Comparators define a complete strict weak ordering ending with immutable unique keys (`pack_dir` and `simfile_path`), avoiding unstable orderings.
- **Robust test cases**: Unit tests specifically cover case-insensitivity (`apple` vs `Banana`, `mid Pack` vs `Zeta Pack`), subtitle tie-breaking, numeric titles, transliterated foreign scripts, and non-alphanumeric titles.

## Recommendation

APPROVE. The change is complete, clean, thoroughly validated, and ready to be committed and merged.
