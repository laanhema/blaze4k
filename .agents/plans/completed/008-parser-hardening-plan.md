# Implementation Plan - Issue #8: [A8] Parser hardening: unit tests and fuzz against reference pack

## 1. Context & Objectives
Ensure the parser and library scanner are hardened against untrusted input.
Simfiles downloaded from the internet must never crash the game, cause infinite loops, or trigger unbounded memory allocations.
Build an automated fuzzing suite, harden MSD and Note parsing limits, and provide a reference simfile pack fixture verifying 100% of 4-panel songs load error-free.

## 2. Requirements & Acceptance Criteria
- **AC1**: Given the reference simfile pack, when the test suite runs, 100% of its 4-panel songs parse with zero crashes.
- **AC2**: Given deliberately malformed `.sm` / `.ssc` files (truncated, huge allocations, bad encodings, binary garbage), when fuzzed/parsed, every file is rejected gracefully (no crash, no unbounded allocation).
- **AC3**: Given known SM/SSC edge cases from StepMania source (escaped characters, comments inside tags, unclosed tags at EOF, empty measures, high-density row subdivisions), parsed output matches reference behavior.
- **AC4**: All parser tests pass under `ctest --test-dir build --output-on-failure`.

## 3. Hardening & Bounds Checks
1. **MSD Parser Bounds (`src/chart/msd_file.cpp`)**:
   - `kMaxFileSize`: 16MB (already present).
   - `kMaxTags`: 10,000 (already present).
   - Add `kMaxParamsPerTag = 1000` to prevent memory exhaustion from runaway colons.
   - Add `kMaxParamLength = 1024 * 1024` (1MB) to cap individual parameter size.
2. **Timing Data Hardening (`src/chart/timing_data.cpp`)**:
   - Add float validity checks (`!std::isnan`, `!std::isinf`).
   - Add bounds checks on beat (`beat >= 0.0` and `beat < 1e6`).
   - Cap maximum BPM and stop segments (max 5,000 segments each).
3. **Note Parser Hardening (`src/chart/note_parser.cpp`)**:
   - Add `kMaxMeasures = 10000` cap.
   - Add `kMaxRowsPerMeasure = 1024` cap.
   - Add `kMaxTotalNotes = 100000` cap.
   - Ensure unclosed hold/roll head durations are strictly positive (`len > 0`).
   - Handle leftover row buffer characters cleanly.

## 4. Reference Pack Fixture
Create `tests/fixtures/reference_pack/`:
- `Tundra Anthem/` (.sm with full difficulties, BPM changes, stops)
- `Northern Lights/` (.ssc with per-chart timing, 192nd subdivisions)
- `Glacier Groove/` (.sm with edge cases: escapes, comments inside tags, EOF without semicolon, empty measures)
- `Aurora Borealis/` (.sm + .ssc both present to test .ssc priority)

## 5. Fuzzing & Edge Case Test Suite (`tests/parser_hardening_test.cpp`)
- Binary garbage mutation tests (random bytes, null bytes, high ascii).
- Truncation tests at arbitrary byte boundaries.
- Boundary condition tests (16MB+ file, 10,000+ tags, 100,000+ notes, NaN/Inf).
- StepMania edge cases (escapes `\:`, `\;`, `\\`, comments in tags, missing headers).
- Full reference pack scan test asserting 100% error-free parsing.

## 6. Definition of Done
- 0 compiler warnings with `-Wall -Wextra -Wpedantic`.
- 100% CTest pass.
- Code review, PR, merged, issue status moved to Done.
