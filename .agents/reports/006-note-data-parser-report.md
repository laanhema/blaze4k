# Implementation Report: Issue #6 — 4-panel note data parsing (taps, holds, rolls, mines)

## Summary
Implemented note data parsing for 4-panel (`dance-single`) charts in both SM and SSC formats. The parser supports taps, hold heads/tails, roll heads/tails, and mines, while properly rejecting non-4-panel steps types (e.g. `dance-double`) and exotic timing (negative BPMs, negative stops/warps) gracefully.

## Changes Made
1. **Note and Chart Domain Model**:
   - `src/chart/note.hpp`: Defined `NoteType` (`Tap`, `HoldHead`, `RollHead`, `Mine`) and `Note` struct with exact nanosecond/second timestamps, beats, and hold/roll duration.
   - `src/chart/chart.hpp`: Defined `Chart` struct with steps type, difficulty, meter, timing data, note vector, and radar counts (taps, holds, rolls, mines).
2. **Note Data Parser**:
   - `src/chart/note_parser.hpp` & `src/chart/note_parser.cpp`:
     - Parses SM `#NOTES:` blocks and SSC `#NOTEDATA:` sections.
     - Strips comments and splits into measures by `,` or `;`.
     - Calculates beat offsets per measure based on row subdivisions.
     - Resolves second timestamps via `TimingData::beat_to_seconds`.
     - Validates and pairs hold/roll heads (`2` and `4`) with tails (`3`).
     - Gracefully filters out unsupported steps types (only `dance-single` supported).
     - Gracefully rejects exotic timing constructs (warps / negative BPMs).
3. **Simfile Parser Integration**:
   - `src/chart/simfile_parser.hpp` & `src/chart/simfile_parser.cpp`:
     - Populates `charts()` on parsed simfiles for both `.sm` and `.ssc` files.
     - Supports per-chart SSC timing data overrides if present.
4. **Timing Data Hardening**:
   - `src/chart/timing_data.hpp` & `src/chart/timing_data.cpp`:
     - Flags `has_exotic_timing()` when negative BPMs or warps (negative stops) are detected.
5. **Testing**:
   - `tests/note_parser_test.cpp`:
     - Comprehensive unit tests verifying taps, holds, rolls, mines, multi-difficulty extraction, graceful non-4-panel skipping, exotic timing rejection, and SSC block parsing.
     - Integrated into `tests/CMakeLists.txt` and passing 100%.
