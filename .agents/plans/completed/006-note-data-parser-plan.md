# Plan: 4-Panel Note Data Parser (Taps, Holds, Rolls, Mines)

## Summary

Implement the 4-panel (`dance-single`) note data parser and immutable note model under `src/chart/`:
1. `src/chart/note.hpp`:
   - `NoteType`: `Tap`, `HoldHead`, `RollHead`, `Mine`.
   - `Note`: column (0..3), beat, time_seconds, type, hold_length_beats, hold_end_time_seconds.
2. `src/chart/chart.hpp`:
   - `Chart`: steps_type ("dance-single"), description, difficulty label, meter (foot rating), notes vector, radar counts (taps, holds, rolls, mines), and chart-level `TimingData`.
3. `src/chart/note_parser.hpp` and `src/chart/note_parser.cpp`:
   - Measure subdivision: each measure has 4 beats, divided evenly by row count $N$. Row $r$ beat = $4M + 4(r/N)$.
   - Column parsing (0=Left, 1=Down, 2=Up, 3=Right):
     - `1`: Tap
     - `2`: Hold head (tracked per-column until matching `3` tail)
     - `3`: Hold/Roll tail (pairs with active head, sets hold length in beats and seconds)
     - `4`: Roll head (tracked until matching `3` tail)
     - `M`: Mine
   - Rejection of non-`dance-single` charts (e.g. `dance-double`, `pump-single`) with an informational log entry.
   - Rejection of exotic timing constructs (warps, negative BPMs, split timing) with a clear log entry.
4. Update `SimfileParser` to parse `#NOTES:` in `.sm` files and `#NOTEDATA:` in `.ssc` files, returning `std::vector<Chart>`.
5. Unit tests in `tests/note_parser_test.cpp`:
   - Parsing tap notes, hold heads/tails, rolls, and mines across different measure subdivisions (4ths, 8ths, 12ths, 16ths).
   - Multi-difficulty simfile extraction.
   - Graceful skip of `dance-double` charts.
   - Detection and graceful rejection of exotic timing.
6. Build and validate with CTest.

## User Story

As a pad player,
I want 4-panel note charts parsed into the internal note model — taps, holds, rolls, and mines —
So that every playable chart in my packs becomes game data.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM-HIGH |
| Systems Affected | `src/chart/`, `tests/` |
| GitHub Issue | #6 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/chart/note.hpp` | CREATE | Note model (NoteType, Note struct) |
| `src/chart/chart.hpp` | CREATE | Chart model (difficulty, meter, notes list, timing) |
| `src/chart/note_parser.hpp` | CREATE | 4-panel measure parser interface |
| `src/chart/note_parser.cpp` | CREATE | Note data measure/row parsing and hold-tail pairing |
| `src/chart/simfile_parser.hpp` | UPDATE | Add charts list to SimfileParser |
| `src/chart/simfile_parser.cpp` | UPDATE | Parse `#NOTES:` (.sm) and `#NOTEDATA:` (.ssc) |
| `CMakeLists.txt` | UPDATE | Add `src/chart/note_parser.cpp` to `tundra_core` |
| `tests/note_parser_test.cpp` | CREATE | Unit tests for taps, holds, rolls, mines, non-4-panel rejection |
| `tests/CMakeLists.txt` | UPDATE | Add `note_parser_test` target |

---

## Tasks

### Task 1: Create Note and Chart models (`src/chart/note.hpp`, `src/chart/chart.hpp`)
- `NoteType`: `Tap`, `HoldHead`, `RollHead`, `Mine`.
- `Note`: column (`int`), beat (`double`), time_seconds (`double`), type (`NoteType`), hold_length_beats (`double`), hold_end_time_seconds (`double`).
- `Chart`: metadata, difficulty label, meter, notes array, counts.

### Task 2: Implement NoteParser (`src/chart/note_parser.hpp`, `src/chart/note_parser.cpp`)
- Parse measure string: split by `,`, split each measure by rows.
- Calculate beat and time via `TimingData::beat_to_seconds()`.
- Match hold/roll heads (`2`, `4`) with tails (`3`) per column.
- Filter and reject non-`dance-single` steps types gracefully.

### Task 3: Integrate with `SimfileParser` (`src/chart/simfile_parser.hpp`, `.cpp`)
- In `.sm`: parse `#NOTES:` 6-parameter blocks.
- In `.ssc`: parse `#NOTEDATA:` sections with `#STEPSTYPE:`, `#DIFFICULTY:`, `#METER:`, `#NOTES:`.
- Store valid 4-panel charts in `SimfileParser::charts()`.

### Task 4: Unit tests in `tests/note_parser_test.cpp`
- Test full 4-panel chart with taps, holds, rolls, mines.
- Test multiple difficulty charts in single `.sm` and `.ssc`.
- Test graceful rejection of `dance-double`.
- Test exotic timing rejection (negative BPM, warps).

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

- [ ] Every tap, hold (head+tail), roll, and mine in 4-panel chart is parsed with correct beat, time, and column
- [ ] Multiple difficulties per file extracted with difficulty label and meter
- [ ] Non-4-panel charts rejected gracefully with log line
- [ ] Exotic timing charts rejected gracefully
- [ ] 100% of unit tests pass
