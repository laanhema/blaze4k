# Plan: Sort the song wheel alphabetically by pack and title (#158)

## Summary

The song select wheel currently presents packs and songs in arbitrary filesystem traversal order (`recursive_directory_iterator` order in `SongLibrary::scan_directory`, `src/chart/song_library.cpp:318-362`), causing song and pack ordering to vary between filesystems and operating systems. This plan introduces deterministic alphabetical sorting for the library: packs are sorted alphabetically A–Z by name, and songs within each pack are sorted alphabetically A–Z by title using StepMania 5 `SongUtil` semantics (`src/SongUtil.cpp:385-425,547-550`, `src/Song.h:204-214`). Primary ordering uses the transliterated title when available (falling back to native title), ties are broken by transliterated subtitle (falling back to native subtitle), and leading periods (`.59`) and non-alphanumeric characters (sorted to the end via `~`) mirror StepMania 5's `MakeSortString`. Sorting runs once after scanning in pure helper functions (`sort_song_library`, `sort_songs`), which enables isolated testing on unsorted vectors independent of the host filesystem. `SelectScreen::rebuild` already restores cursor position by stable `simfile_path` and pack name, which guarantees that returning to Select from Gameplay, Results, Calibration, or Input Remap preserves selection.

## User Story

As a player  
I want song packs and songs within each pack sorted alphabetically on the song wheel  
So that wheel navigation is predictable, consistent across all platforms, and easy to search.

## Metadata

| Field | Value |
|---|---|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/chart/song_library.{hpp,cpp}`, `tests/song_library_test.cpp`, `.claude/skills/verify/features/song-select.md`, `TODO.md` |
| GitHub Issue | #158 ([TODO-46] Sort the song wheel alphabetically by pack and title) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/074-sort-song-wheel-prototype.patch` |
| Plan sequence | `074`. The running sequence in `.agents/plans/completed/` ends at `073` (#98, cabinet theme polish). |

---

## Environment Findings

Planned on `main` @ `7c976d8`, working tree clean (with prototype patch saved).

| Fact / Tool | Value | Notes |
|---|---|---|
| Baseline build & tests | `scripts/check.sh` passed, 53 of 53 tests | Linux GCC; silent null audio device |
| StepMania 5 reference | `stepmania/stepmania` @ `825467bcd8` (`src/SongUtil.cpp:385-425,547-550`, `src/Song.h:204-214`) | Upstream uses `GetTranslitMainTitle`, `GetTranslitSubTitle`, `MakeSortString`, and group comparison |
| Selection restoration | `SelectScreen::rebuild` (`src/screens/select_screen.cpp:189-216`) | Re-identifies song by `simfile_path` and pack by name; sorting does not break restoration |
| Reference fixture | `tests/fixtures/reference_pack/Blaze Pack` | Four songs already happen to be in alphabetical order on disk; sorting must be proved with dedicated unsorted unit tests |
| Forward references to `#158` | `.agents/plans/completed/072-open-close-packs-song-wheel-plan.md:260`, `.agents/reviews/feature-072-open-close-packs-song-wheel-review-r1.md`, `TODO.md:64` | Verified |

---

## Reference Behaviour (StepMania 5)

StepMania 5's sorting logic is implemented in `src/SongUtil.cpp` and `src/Song.h`:

- **Transliteration precedence**: `Song::GetTranslitMainTitle()` (`src/Song.h:204-207`) returns `m_sMainTitleTranslit.empty() ? m_sMainTitle : m_sMainTitleTranslit`. Transliterations are preferred so foreign-script titles sort in Romanized A–Z order on Latin song wheels.
- **Subtitle tie-break**: `Song::GetTranslitSubTitle()` (`src/Song.h:211-214`) returns `m_sSubTitleTranslit.empty() ? m_sSubTitle : m_sSubTitleTranslit`. If two songs share a main title, `CompareSongPointersByTitle` compares subtitle sort strings (`src/SongUtil.cpp:391-396`).
- **`MakeSortString`**: `SongUtil::MakeSortString` (`src/SongUtil.cpp:371-385`) uppercases the string, strips a leading `.` (so `.59` sorts under `59`), and prepends `~` (`char(126)`) if the leading character is not alphanumeric (`0-9`, `A-Z`), placing non-alphanumeric titles at the end after `Z`.
- **Group and title sort**: `CompareSongPointersByGroupAndTitle` (`src/SongUtil.cpp:535-546`) compares pack/group names first, then delegates to `CompareSongPointersByTitle`.
- **Deterministic tie-breaker**: When title sort strings match, StepMania compares the unique `SongFilePath` (`src/SongUtil.cpp:400-403`). In Blaze 4k, `simfile_path` serves this exact role.

### Value Provenance (StepMania 5 @ `825467bcd81c812b33ad684dc04dd151b2d5dec3`)

| Behaviour | Source | Line(s) |
|---|---|---|
| Transliterated main title preference | `src/Song.h` | 204–207 |
| Transliterated subtitle preference | `src/Song.h` | 211–214 |
| `MakeSortString` (uppercase, leading '.', '~' for non-alnum) | `src/SongUtil.cpp` | 371–385 |
| Primary title sort with subtitle tie-break | `src/SongUtil.cpp` | 387–396 |
| File path tie-breaker | `src/SongUtil.cpp` | 400–403 |
| Group comparison | `src/SongUtil.cpp` | 535–546 |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Implement sorting as pure free functions `sort_songs(std::vector<Song>&)` and `sort_song_library(std::vector<SongPack>&)` in `src/chart/song_library.{hpp,cpp}`, with a `SongLibrary::sort()` member. | Enables testing unsorted vectors directly without relying on OS directory iteration order (AC3). |
| D2 | Call `sort()` automatically in `SongLibrary::scan_directory` before returning `true` (both in the single-song root path and the multi-pack directory path). | Guarantees all scanned libraries are sorted on disk scan, matching the requirement that the wheel presents sorted packs and songs. |
| D3 | Packs sorted case-insensitively A–Z using `make_sort_string(name)`. Ties broken by case-sensitive name, then `pack_dir`. | Natural A–Z ordering for pack folders; deterministic across filesystems. |
| D4 | Songs sorted using StepMania 5 `SongUtil::MakeSortString` rules: case-insensitive uppercase, leading period stripped (`.59`), non-alphanumeric prepended with `~` so symbols sort after Z. | Adheres to AGENTS.md Principle 2 ("Faithful, not novel"). |
| D5 | Song sort key precedence: `title_translit` (fallback to `title`), tie-break by `subtitle_translit` (fallback to `subtitle`), tie-break by case-sensitive title, subtitle, and finally `simfile_path`. | Matches StepMania 5 `CompareSongPointersByTitle` and provides strict weak ordering. |
| D6 | No changes required in `SelectScreen::rebuild`. | Audited: `SelectScreen::rebuild` (`src/screens/select_screen.cpp:189-216`) re-finds the selected song by `simfile_path` equality and open pack by name, independent of pack or song index. |

---

## Patterns to Follow

### Pure helper beside existing chart sort
```cpp
// SOURCE: src/chart/song_library.cpp:61-68
void sort_charts_hardest_first(std::vector<Chart>& charts) {
    std::stable_sort(charts.begin(), charts.end(), [](const Chart& a, const Chart& b) {
        if (a.meter != b.meter) {
            return a.meter > b.meter;
        }
        return difficulty_rank(a.difficulty) > difficulty_rank(b.difficulty);
    });
}
```

### Case-insensitive comparison helpers in `song_library.cpp`
```cpp
// SOURCE: src/chart/song_library.cpp:11-20
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}
```

### Selection preservation by stable key
```cpp
// SOURCE: src/screens/select_screen.cpp:189-206
if (!previous_song.empty()) {
    for (std::size_t i = 0; i < songs_.size(); ++i) {
        const Song* song = songs_[i].song;
        if (song == nullptr || song->simfile_path != previous_song) {
            continue;
        }
        open_pack_ = songs_[i].pack_index;
        rebuild_rows();
        cursor_row_ = std::max(0, song_row_[i]);
        if (!song->charts.empty()) {
            selected_chart_ =
                std::clamp(previous_chart, 0, static_cast<int>(song->charts.size()) - 1);
        }
        update_persisted();
        return;
    }
}
```

---

## Files to Change

| File | Action | Covered by Prototype Patch? | Purpose |
|---|---|---|---|
| `src/chart/song_library.hpp` | UPDATE | Yes | Declare `sort_songs`, `sort_song_library`, and `SongLibrary::sort` |
| `src/chart/song_library.cpp` | UPDATE | Yes | Implement `make_sort_string`, comparators, sort functions, and invoke `sort()` in `scan_directory` |
| `tests/song_library_test.cpp` | UPDATE | Yes | Add comprehensive test section 8 asserting sort order on unsorted input |
| `.claude/skills/verify/features/song-select.md` | UPDATE | Yes | Update gotcha note regarding song wheel directory vs alphabetical order |
| `TODO.md` | UPDATE | No (gitignored) | Tick off line 64: `[x] Sort the song wheel... (#158)` |

---

## Tasks

Execute in order. Tasks 1, 2, and 3 are covered by the prototype patch (`.agents/plans/074-sort-song-wheel-prototype.patch`), which `/implement` applies before Task 1.

### Task 1: Library sorting helpers and scan hook

- **File**: `src/chart/song_library.hpp`, `src/chart/song_library.cpp`
- **Action**: UPDATE
- **Implement**:
  1. In `src/chart/song_library.hpp`, declare `void sort_songs(std::vector<Song>& songs);` and `void sort_song_library(std::vector<SongPack>& packs);` under namespace `blaze4k`. Add `void sort();` to `SongLibrary`.
  2. In `src/chart/song_library.cpp`:
     - Implement `std::string make_sort_string(std::string_view text)` matching SM5: convert to uppercase, strip leading `.`, prepend `~` to non-alphanumeric strings.
     - Implement `bool compare_packs_by_name(const SongPack& a, const SongPack& b)`: compare `make_sort_string`, tie-break by name, then `pack_dir`.
     - Implement `bool compare_songs_by_title(const Song& a, const Song& b)`: compare `make_sort_string` of title (transliterated preferred), tie-break by subtitle (transliterated preferred), tie-break by case-sensitive title, subtitle, and `simfile_path`.
     - Implement `sort_songs` using `std::stable_sort` with `compare_songs_by_title`.
     - Implement `sort_song_library` using `std::stable_sort` on packs, then `sort_songs` on each pack's songs.
     - Implement `SongLibrary::sort()` delegating to `sort_song_library(packs_)`.
     - In `SongLibrary::scan_directory`, call `sort();` before returning `true` in both the `root_is_song` path (`line 309`) and the directory scan path (`line 389`).
- **Mirror**: `src/chart/song_library.cpp:61-68` (`sort_charts_hardest_first`).
- **Validate**: `cmake --build build -j"$(nproc)"`

### Task 2: Unit tests for unsorted input in `song_library_test`

- **File**: `tests/song_library_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Add test block 8 verifying `sort_song_library`:
     - Provide packs in unsorted order: `"Zeta Pack"`, `"Alpha Pack"`, `"mid Pack"`. Assert result is `"Alpha Pack"`, `"mid Pack"`, `"Zeta Pack"`.
     - In `"Alpha Pack"`, provide songs out of order:
       - `"Zebra"`
       - `"apple"` (lowercase, verifies case-insensitivity)
       - `"Banana"` with subtitle `"Part 2"`
       - `"Banana"` with subtitle `"Part 1"` (verifies subtitle tie-break)
       - `".59"` (verifies leading period strip, sorts before `"apple"`)
       - `"★Special"` (verifies non-alphanumeric sorts after Z)
       - `"漢字"` with `title_translit = "Cherry"` (verifies transliteration precedence over native title)
     - Call `sort_song_library(packs)` and assert exact expected ordering:
       1. `".59"`
       2. `"apple"`
       3. `"Banana" / "Part 1"`
       4. `"Banana" / "Part 2"`
       5. `"漢字" / "Cherry"`
       6. `"Zebra"`
       7. `"★Special"`
- **Mirror**: `tests/song_library_test.cpp:148-207`
- **Validate**: `ctest --test-dir build -R song_library_test --output-on-failure`

### Task 3: Update verification skill documentation

- **File**: `.claude/skills/verify/features/song-select.md`
- **Action**: UPDATE
- **Implement**:
  - Update line 40 gotcha from:
    `- Songs and packs are listed in directory order and are not sorted. The fixture order above is what this machine's filesystem returns. On another filesystem, read the wheel in the screenshot before counting Down presses.`
    to:
    `- Packs and songs are listed alphabetically A–Z (packs by name, songs by title with StepMania 5 sort semantics). The order does not depend on filesystem directory order.`
- **Mirror**: `.claude/skills/verify/features/song-select.md:37-45`
- **Validate**: `grep -n "alphabetically A–Z" .claude/skills/verify/features/song-select.md`

### Task 4: Tick `TODO.md`

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Update line 64 from `- [ ] Sort the song wheel... (#158)` to `- [x] Sort the song wheel... (#158)`. Note: `TODO.md` is gitignored and does not appear in commits or patches.
- **Mirror**: `TODO.md:61-67`
- **Validate**: `grep -n "Sort the song wheel" TODO.md`

---

## Validation

```bash
# Lint, format check, compile with warnings as errors, run all tests (CI check)
scripts/check.sh

# Run specific song library tests
ctest --test-dir build -R "song_library|select_screen" --output-on-failure

# AddressSanitizer and UndefinedBehaviorSanitizer run
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan -R "song_library|select_screen"
```

Start green (53 of 53 passed), stay green.

## End-to-End Verification

1. **Headless Unit Check**: Run `ctest --test-dir build -R "song_library_test|select_screen_test" --output-on-failure`. Verify that unsorted song packs and song titles sort into exact alphabetical order, and that `test_selection_preserved_on_reenter` in `select_screen_test` passes, confirming selection is preserved after transitions.
2. **Visual Verification (`/verify`)**:
   - Launch Select screen using `$B launch` with fixture songs.
   - Open pack on Select screen (`ENTER`).
   - Observe that songs appear in alphabetical order on the wheel:
     - `Aurora Borealis`
     - `Blaze Anthem`
     - `Glacier Groove`
     - `Northern Lights`
   - Select `Blaze Anthem`, press `ENTER` to start Gameplay, press `Escape`/`Back` to return to Select.
   - Verify that `Blaze Anthem` remains highlighted and its pack remains open.

---

## Risks

| Risk | Mitigation | In-scope / Out-of-scope |
|---|---|---|
| Non-Latin titles with no transliteration tag sorting in raw UTF-8 byte order | StepMania 5 prepends `~` to non-ASCII/non-alphanumeric characters, placing untransliterated scripts after Z. Titles with `#TITLETRANSLIT` sort by Romanized transliteration. | In scope. |
| Existing tests relying on pre-sort directory order | Audited all 53 tests in `tests/`: no test relied on song vector index order; `SlideFixture` songs were already named `"Song 1"` through `"Song 6"` and `"Pack One"` / `"Pack Two"`. | In scope. |
| Selection lost on re-entering Select screen | `SelectScreen::rebuild` re-finds songs by `simfile_path` and packs by name; `test_selection_preserved_on_reenter` confirms this behavior remains intact. | In scope. |

---

## Open Questions

1. **Should pack names strip leading dots and push non-alphanumeric characters to the end like songs?**
   - *Proposed default*: Yes. Using `make_sort_string` for both packs and songs keeps collation consistent across the entire wheel (e.g. `.Favorites` sorts under `F`, and `★Community Packs` sorts at the end after `Z`).
2. **Case collision handling (e.g., "pack" vs "Pack", or "Song" vs "song")**:
   - *Proposed default*: Primary sort is case-insensitive (A == a). Ties are broken deterministically by case-sensitive string comparison (`<`), and finally by `pack_dir` or `simfile_path`. This prevents non-deterministic ordering between runs while keeping natural case-insensitive grouping.

---

## Acceptance Criteria

- [ ] Packs are listed alphabetically on the song select wheel
- [ ] Songs inside each pack are listed alphabetically A–Z by title
- [ ] The order does not depend on filesystem directory order: a `song_library_test` case sorts songs and packs given in unsorted order, and fails without the sort
- [ ] Returning to Select from Gameplay, Results, Calibration or Input remap still restores the highlighted song and chart
- [ ] The song-select gotcha in `.claude/skills/verify/features/song-select.md` that says songs are listed in directory order is updated
- [ ] All tasks completed; `scripts/check.sh` passes with zero warnings and all tests green
- [ ] Prototype patch `.agents/plans/074-sort-song-wheel-prototype.patch` applies cleanly via `git apply --check`
