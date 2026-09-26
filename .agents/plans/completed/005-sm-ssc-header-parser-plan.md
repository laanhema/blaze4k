# Plan: SM/SSC Header and Timing Parser

## Summary

Implement the SM and SSC file parser for headers, song metadata, and timing data (BPM changes, stops, and offset) adhering to StepMania 5 reference semantics:
1. `src/chart/msd_file.hpp` & `src/chart/msd_file.cpp`: Robust, allocation-capped, bounds-checked MSD tag-value parser (`#TAG:VALUE;`) with comment stripping (`//`) and whitespace trimming. Untrusted input protection: max 16MB file size, max 10,000 tags.
2. `src/chart/timing_data.hpp` & `src/chart/timing_data.cpp`: Timing data structure storing offset, BPM segments (`beat=bpm`), and stop segments (`beat=seconds`). Implements exact `beat_to_seconds(double beat)` and `seconds_to_beat(double seconds)` conversion matching StepMania 5.
3. `src/chart/song_metadata.hpp`: Song metadata structure (title, subtitle, artist, translits, banner, background, cdtitle, music, offset, sample_start, sample_length).
4. `src/chart/simfile_parser.hpp` & `src/chart/simfile_parser.cpp`: High-level parser that reads `.sm` and `.ssc` files from disk or memory string, populating metadata and `TimingData`. Graceful failure on malformed input (error logging, no crashes).
5. Comprehensive unit tests in `tests/parser_test.cpp`:
   - Valid `.sm` header parsing
   - Valid `.ssc` header parsing
   - Multiple BPM changes and stops beat-to-time conversion fidelity
   - Roundtrip beat-to-seconds and seconds-to-beat consistency
   - Untrusted/malformed inputs: truncated tags, missing semicolons, huge buffers, NaN/garbage numbers without crashing.

## User Story

As a pad player,
I want the game to parse SM and SSC file headers (title, artist, banner, background, music, offset, BPMs, stops),
So that my existing simfile library loads without conversion.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM-HIGH |
| Systems Affected | `src/chart/`, `tests/` |
| GitHub Issue | #5 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/chart/msd_file.hpp` | CREATE | MSD tag/value lexer/parser interface |
| `src/chart/msd_file.cpp` | CREATE | Robust MSD parsing with comments and escape handling |
| `src/chart/timing_data.hpp` | CREATE | Timing model (BPMs, stops, beat <-> seconds conversion) |
| `src/chart/timing_data.cpp` | CREATE | Exact beat-to-time and time-to-beat conversion matching SM5 |
| `src/chart/song_metadata.hpp` | CREATE | Song metadata fields and path resolution |
| `src/chart/simfile_parser.hpp` | CREATE | SM/SSC parser interface |
| `src/chart/simfile_parser.cpp` | CREATE | Tag extraction and timing initialization |
| `CMakeLists.txt` | UPDATE | Add chart sources to `tundra_core` |
| `tests/parser_test.cpp` | CREATE | Unit tests for MSD, SM/SSC headers, timing math, and fuzz/hardening |
| `tests/CMakeLists.txt` | UPDATE | Add `parser_test` target |

---

## Tasks

### Task 1: MSD Parser (`src/chart/msd_file.hpp`, `.cpp`)
- Implement tokenization into `std::vector<MsdTag>` where each tag has `name` and `values` (parameters separated by `:` up to `;`).
- Strip single-line comments (`//...`).
- Bounds check buffer and cap max tags to 10,000 and max file size to 16MB.

### Task 2: TimingData (`src/chart/timing_data.hpp`, `.cpp`)
- Structures: `BpmSegment { double beat, double bpm }`, `StopSegment { double beat, double length_seconds }`.
- Method `add_bpm(double beat, double bpm)`, `add_stop(double beat, double seconds)`.
- Method `set_offset(double offset_seconds)`.
- Method `beat_to_seconds(double beat) const -> double`.
- Method `seconds_to_beat(double seconds) const -> double`.
- Handle stops correctly (time advances while beat stays constant).

### Task 3: SimfileParser (`src/chart/song_metadata.hpp`, `src/chart/simfile_parser.hpp`, `.cpp`)
- Parse `#TITLE:`, `#SUBTITLE:`, `#ARTIST:`, `#BANNER:`, `#BACKGROUND:`, `#MUSIC:`, `#OFFSET:`, `#BPMS:`, `#STOPS:`, `#SAMPLESTART:`, `#SAMPLELENGTH:`.
- In SSC, handle `#NOTEDATA:;` block transitions.
- Graceful error logging on parse errors without exceptions or aborts.

### Task 4: Unit tests in `tests/parser_test.cpp`
- Test basic MSD syntax.
- Test real-world SM snippet with BPM changes and stops.
- Test SSC snippet with metadata.
- Test beat <-> seconds conversion at multiple points (before stop, during stop, after stop, across BPM transitions).
- Test malformed inputs: unclosed tag, huge input, garbage numeric string.

### Task 5: Build and validate
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

---

## Validation

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Acceptance Criteria

- [ ] Valid `.sm` and `.ssc` headers parsed correctly (title, artist, banner, background, music, offset, BPMs, stops)
- [ ] Timing conversion beat→seconds and seconds→beat maintains mathematical fidelity
- [ ] Malformed header or corrupt input logs warning and never crashes
- [ ] 100% of unit tests pass
