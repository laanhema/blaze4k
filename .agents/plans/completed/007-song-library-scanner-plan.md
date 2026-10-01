# Implementation Plan - Issue #7: [A7] Song library scanner with pack grouping and art resolution

## 1. Context & Objectives
Implement the recursive song library scanner for Blaze 4k.
Given a `Songs/<Pack>/<Song>/` directory hierarchy (or pack subfolder), the scanner recursively discovers simfiles, groups them into packs, resolves banner/background/music asset paths with case-insensitivity and fallback resolution, skips unsupported or corrupt charts gracefully with readable logs, and ensures 100% error-free loading for valid 4-panel songs.

## 2. Requirements & Acceptance Criteria
- **AC1**: Given a `Songs/<Pack>/<Song>/` folder structure, when scanned, every song with at least one 4-panel chart appears in the library grouped by pack.
- **AC2**: Given a song folder, when scanned, banner and background image paths from the simfile are resolved (with missing-art fallback).
- **AC3**: Given a corrupt or unsupported simfile in the tree, when scanned, it is skipped with a log line and scanning continues.
- **AC4**: Given reference pack fixtures, when scanned, 100% of 4-panel songs load error-free.

## 3. Architecture & Data Structures
- **Domain Models (`src/chart/song.hpp` & `src/chart/song_pack.hpp`)**:
  - `struct Song`:
    - `pack_name`, `song_dir`, `simfile_path`
    - `metadata` (`SongMetadata`)
    - `timing` (`TimingData`)
    - `charts` (`std::vector<Chart>`, containing valid 4-panel `dance-single` charts)
    - Resolved art paths: `banner_path`, `background_path`, `music_path`
    - Flags: `has_custom_banner`, `has_custom_background`
  - `struct SongPack`:
    - `name`, `pack_dir`, `banner_path`
    - `songs` (`std::vector<Song>`)
- **Library Scanner (`src/chart/song_library.hpp` / `src/chart/song_library.cpp`)**:
  - `bool scan_directory(const std::filesystem::path& root_path)`:
    - Recursively traverses filesystem using `std::filesystem::recursive_directory_iterator`.
    - Detects song folders containing `.ssc` or `.sm`.
    - Prefers `.ssc` if both exist in the same folder.
    - Determines pack name from folder hierarchy.
    - Resolves art with case-insensitive filesystem lookup.
    - Resolves fallback art (`*bn.*`, `*bg.*`, or configured fallback asset).
    - Filters out songs lacking 4-panel charts.
    - Catches exceptions/errors per simfile, logs warning, and continues.
  - Accessors:
    - `const std::vector<SongPack>& packs() const`
    - `size_t total_songs() const`
    - `size_t total_charts() const`
    - `const Song* find_song(std::string_view pack_name, std::string_view title) const`
    - `void clear()`

## 4. Testing Plan
- `tests/song_library_test.cpp`:
  - Build temporary directory fixture with `Songs/TestPack/ValidSong1/`, `Songs/TestPack/ValidSong2_SSC/`, `Songs/TestPack/CorruptSong/`, `Songs/TestPack/DoubleOnlySong/`.
  - Verify pack grouping: `TestPack` contains 2 songs.
  - Verify art resolution: exact matches, case-insensitive matches (`BANNER.PNG` vs `banner.png`), and auto-detected fallback matches (`song-bg.png`).
  - Verify resilience: corrupt simfile logged and skipped without crashing.
  - Verify unsupported chart skipping: doubles-only chart skipped without crashing.
  - Verify 100% of valid 4-panel songs parsed correctly.

## 5. Definition of Done
- Zero warnings with `-Wall -Wextra -Wpedantic`.
- 100% pass on all CTests.
- Implementation report, PR created, merged, issue marked Done.
