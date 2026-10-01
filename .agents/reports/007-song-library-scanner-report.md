# Implementation Report: Issue #7 — Song library scanner with pack grouping and art resolution

## Summary
Implemented the recursive song library scanner (`SongLibrary`) supporting `Songs/<Pack>/<Song>/` hierarchies, pack grouping, multi-tier banner/background/music art resolution (exact, case-insensitive, keyword fallback, and engine defaults), and resilient skipping of non-4-panel charts and corrupt files with readable logging.

## Changes Made
1. **Domain Models**:
   - `src/chart/song.hpp`: `Song` struct containing pack name, directory paths, simfile path, metadata, timing data, 4-panel chart list, resolved asset paths, and custom art flags.
   - `src/chart/song_pack.hpp`: `SongPack` struct grouping songs by pack name, with pack directory and pack banner path.
2. **Library Scanner**:
   - `src/chart/song_library.hpp` & `src/chart/song_library.cpp`:
     - `scan_directory`: Recursively traverses directories, detecting song folders containing `.ssc` and `.sm` files, prioritizing `.ssc` if both exist.
     - Pack grouping: Automatically associates songs with their parent pack name and pack banner.
     - Art resolution:
       - Exact path resolution relative to song folder.
       - Case-insensitive fallback matching (handling common Windows/Linux naming differences like `BANNER.PNG` vs `banner.png`).
       - Stem keyword matching for standard conventions (`*bn.*`, `*bg.*`, `*banner*`, `*background*`).
       - Engine fallback configuration (`set_fallback_banner`, `set_fallback_background`).
     - Graceful filtering: Skips songs without valid 4-panel charts or with corrupt simfiles cleanly with log output without crashing.
3. **Build System & Tests**:
   - Updated `CMakeLists.txt` adding `song_library.cpp` to `blaze4k_core`.
   - Added `tests/song_library_test.cpp` and registered target in `tests/CMakeLists.txt`.
   - Tests verify pack grouping, exact art matching, case-insensitive art matching, keyword fallback matching, engine default art, non-4-panel skipping, and corrupt simfile recovery. 100% CTest pass.
