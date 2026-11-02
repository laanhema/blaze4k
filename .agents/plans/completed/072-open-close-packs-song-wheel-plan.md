# Plan: Open and close packs on the song wheel (#166)

## Summary

Today the Select wheel lists every selectable song under an inline pack header that the cursor skips (`build_wheel_rows`, `src/screens/select_art.cpp:143`; `SelectScreen` navigates a song index, `src/screens/select_screen.cpp:279`). This plan makes the wheel's cursor a row cursor over the *visible* display rows. Pack headers are always visible and selectable. Songs are visible only for the one open pack, which is StepMania's behaviour (verified against the StepMania source, see Reference Behaviour). Confirm on a header toggles it: it closes the open pack, or opens this one and closes the other. Confirm on a song plays it as before. The open pack and the cursor target are remembered by name, so re-entering Select from Gameplay, Results, Calibration or Input remap keeps the highlighted song and keeps its pack open. A selected header is drawn with the gold selected bar and a code-drawn open/closed marker; the info panel shows nothing for a header (no song).

## User Story

As a player with many song packs
I want to collapse a pack to its header and expand it again
So that the song wheel stays short and I can reach another pack quickly

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY (feature on the existing wheel) |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/select_screen.{hpp,cpp}`, `src/screens/select_art.{hpp,cpp}`, `tests/select_screen_test.cpp`, `tests/select_art_test.cpp`, `.claude/skills/verify/features/song-select.md`, `TODO.md` |
| GitHub Issue | #166 ([TODO-54] Open and close packs on the song wheel) |
| PRD Phase | N/A |
| Prototype Patch | N/A (the change touches the cursor model of one screen; a slice would not prove more than the tests do) |
| Plan sequence | `072`. The running sequence in `.agents/plans/completed/` ends at `071` (#148, TODO-45); TODO ids do not map to it. |

---

## Environment Findings

Planned on `main` @ `675a6c9`, clean tree.

| Fact | Value | Notes |
|------|-------|-------|
| Baseline | `cmake --build build` then `ctest --test-dir build`: 100% passed, 53 of 53 | Linux, GCC; tests use the silent null audio device |
| Forward references to `#166` / TODO-54 | `.agents/issues/todo-issues.md:2311`, `TODO.md:67` only | No code placeholders to pick up |
| StepMania source | Fetched `src/WheelBase.cpp` and `src/MusicWheel.cpp` from `stepmania/stepmania` @ `825467bcd8` via `gh api` | Upstream matched the local copy at `/tmp/opencode/MusicWheel.cpp` for `SetOpenSection` |
| Issue-stated numbers | "the window logic moves two rows when it crosses a header" (issue comment) | True today (`select_art.hpp:187`, test `select_screen_test.cpp:1000`). After this change a one-step move moves the window by at most one row, except wrap (snap) and a toggle (snap). |
| Library facts | `SelectScreen::rebuild` runs on every `enter()` (`select_screen.cpp:242`); the screen object persists across transitions (`ScreenManager::add_screen`), so members survive | Open pack and cursor can live in members; names guard against a changed library |

## Reference Behaviour (StepMania 5, cite this on the issue)

- `WheelBase::Select()` (`src/WheelBase.cpp`, `case WheelItemDataType_Section`): `if (m_sExpandedSectionName == sThisItemSectionName) SetOpenSection("") /* collapse */ else SetOpenSection(sThisItemSectionName) /* expand */`, then returns `false` ("Opening/closing sections cannot end the screen").
- `MusicWheel::SetOpenSection(group)` (`src/MusicWheel.cpp:1439`): stores a single `m_sExpandedSectionName`; songs are skipped when `d.m_sText != group` (line 1464-1467). So **exactly one section is open at a time**, and with `group == ""` only headers remain. Section items stay in the list, so the selection remains on the header after a toggle (the "select the previously selected item" loop, line 1512-1525, finds the same section item).
- `MusicWheel::BeginScreen` (line ~200): when a previous song exists the wheel opens that song's section; with none it calls `SetOpenSection("")` (all closed).

### Value Provenance (StepMania 5, pinned at `stepmania/stepmania` commit `825467bcd81c812b33ad684dc04dd151b2d5dec3`)

| Behaviour | Source | Line(s) |
|-----------|--------|---------|
| Confirm on a section toggles it; returns false (cannot end the screen) | `src/WheelBase.cpp`, `WheelBase::Select`, `case WheelItemDataType_Section` | 288-306 |
| One expanded section name; songs outside it are skipped | `src/MusicWheel.cpp`, `MusicWheel::SetOpenSection` | 1439-1467 |
| Selection stays on the previously selected item (the header) after a toggle | `src/MusicWheel.cpp`, same function | 1512-1525 |
| No previous song on entry: all sections closed | `src/MusicWheel.cpp`, `MusicWheel::BeginScreen` | 210 |

Blaze 4k decision: copy single-open-pack and cursor-stays-on-header exactly. First entry differs on purpose (Open Question 1).

---

## Decisions

| # | Decision | Reason |
|---|----------|--------|
| D1 | One open pack at a time (`open_pack_` index, -1 = none). | Matches StepMania (AC4) and keeps the wheel short, the stated goal. |
| D2 | `SelectScreen` gets a row cursor `cursor_row_` into `wheel_rows_` (visible rows only). `selected_song()` is `nullptr` when the cursor is on a header; `selected_song_index()` returns -1 there. | The issue's own suggestion ("a row-level cursor"). Keeps `list_window`, the slide and the draw path on one row index. |
| D3 | `build_wheel_rows(song_packs, open_pack)` emits a header per pack and the songs of `open_pack` only. `WheelRows::song_row[i]` is -1 for a hidden song. | Pure, testable in `select_art_test`. Header rows and their `pack_index` are unchanged for the draw path. |
| D4 | Persist by name: `open_pack_name_` (empty = none) and, when the cursor is on a header, `cursor_pack_name_`; a song cursor persists by `simfile_path` as today. | `rebuild()` clears and refills `songs_` on each `enter()`. Names survive a re-scan; indexes do not. |
| D5 | A toggle snaps the wheel (no slide): `scroll_start_ = 0`, `scroll_elapsed_ = 0`. The cursor is re-found on the same header's new row. | The row set changed; a slide from the old positions would show wrong rows. `wheel_scroll_start` already snaps for large jumps. |
| D6 | Selected header art: `WheelArt::SelectedPack`, drawn with the existing `wheel_row_selected` bar, label in `kWheelSelected`, text at `kWheelPackTextX`. Every header (selected or not) gets a marker triangle left of its text: right-pointing when closed, down-pointing when open. The triangle is a pure helper in `select_art` built like `hint_arrow_quads` (`select_art.cpp:355`), drawn with `draw_solid`. | No new texture, no dependency on the symbol font's glyph set; `kWheelPackTextX = 54` (`select_art.hpp:104`) already leaves room left of the label. |
| D7 | On a header the left info panel shows the banner fallback and no song info or difficulty rows; the preview stops. | `render()` and `request_preview_for_selected()` already handle `song == nullptr` (`select_screen.cpp:228-233`, `594`, `606`). No pack-banner feature (scope). |
| D8 | Left/Right (chart move) do nothing on a header; the held-direction repeat calls the same `move_row`. Confirm is not repeated. | `move_chart` already returns for `chart_count() <= 1`; `chart_count()` is 0 with no song. |

---

## Patterns to Follow

### Pure row builder + test
```cpp
// SOURCE: src/screens/select_art.cpp:143-156
WheelRows build_wheel_rows(std::span<const int> song_packs) {
    WheelRows out;
    ...
        if (i == 0 || song_packs[i - 1] != pack) {
            out.rows.push_back(WheelRow{WheelRow::Kind::Pack, pack, -1});
        }
        out.song_row.push_back(static_cast<int>(out.rows.size()));
        out.rows.push_back(WheelRow{WheelRow::Kind::Song, pack, static_cast<int>(i)});
```
Test: `tests/select_art_test.cpp:253-282` (`test_build_wheel_rows`, `TEST_CHECK` with hand-written expected rows).

### Preserve selection across `enter()` by stable key
```cpp
// SOURCE: src/screens/select_screen.cpp:97-108, 142-155
std::string previous_key;  // previous->simfile_path
...
for (std::size_t i = 0; i < songs_.size(); ++i) {
    if (song != nullptr && song->simfile_path == previous_key) { selected_song_ = ...; break; }
```
Test: `test_selection_preserved_on_reenter`, `tests/select_screen_test.cpp:438-453`.

### Slide only when the window moved
```cpp
// SOURCE: src/screens/select_screen.cpp:279-298
const int old_first = select_art::list_window(selected_wheel_row(), rows, select_art::kWheelVisibleRows).first;
...
if (new_first != old_first) { scroll_start_ = select_art::wheel_scroll_start(wheel_scroll_offset(), new_first - old_first); scroll_elapsed_ = 0.0; }
```

### Code-drawn arrow geometry as a pure helper
`ArrowQuads hint_arrow_quads(HintArrow, Vec2)` (`src/screens/select_art.cpp:355-390`, header `select_art.hpp:315`), tested at `tests/select_art_test.cpp:794-802`; drawn through `draw_solid` (`select_art.cpp:79`).

### Test fixtures
`SlideFixture` / `make_slide_library` (two packs "Pack One" and "Pack Two", 6 songs each; `tests/select_screen_test.cpp:840-880`), `pack_of` (line 857). `make_library` (one pack, 3 selectable songs; line 125). A decision made in a draw call is checked via `GlQuadRenderer::recorded_quads` or a pure helper (AGENTS.md).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | UPDATE | `build_wheel_rows(song_packs, open_pack)`; `song_row` -1 for hidden songs; `WheelArt::SelectedPack`; `WheelRowView::open` (marker state); `wheel_marker_quad` declaration; fix the "two rows" comments at `:167-168` and `:187-188` |
| `src/screens/select_art.cpp` | UPDATE | Implement the above; draw the marker; text pass for `SelectedPack` |
| `src/screens/select_screen.hpp` | UPDATE | Row cursor, open-pack state, `toggle_pack`, test accessors (`on_pack_header`, `open_pack_name`, `selected_wheel_row` public or a `cursor_row()`), remove `selected_song_`/`song_row_index_` as cursor state |
| `src/screens/select_screen.cpp` | UPDATE | `rebuild`, `move_song` -> `move_row`, Confirm on a header, render of headers, first-entry default |
| `tests/select_art_test.cpp` | UPDATE | Cases for `build_wheel_rows` with an open pack, none open, hidden `song_row`; marker helper |
| `tests/select_screen_test.cpp` | UPDATE | Adapt tests that assume every song is reachable; add the new behaviour tests (Task 6) |
| `.claude/skills/verify/features/song-select.md` | UPDATE | New behaviour: headers selectable, Confirm toggles, one pack open; the fixture has one pack so say how to get two |
| `TODO.md` | UPDATE | Tick line 67 (gitignored, not in the diff) |

---

## Tasks

Execute in order. Tasks 1 and 2 change a signature used by both files, so build after Task 2 (not between them); build after every later task with `cmake --build build -j"$(nproc)"`.

### Task 1: Pure layer in `select_art` (open pack, header selection art, marker)

- **File**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  1. `build_wheel_rows(std::span<const int> song_packs, int open_pack)`: unchanged header logic (a header before the first song of each pack in `song_packs`); a `Song` row and its `song_row` entry only when `pack == open_pack`; otherwise `song_row[i] = -1`. `open_pack = -1` means all closed. Update the struct comment ("Navigation moves over songs only" is no longer true). Update the callers in `select_screen.cpp` in Task 2 (the one existing call is the only one; `select_art_test.cpp:256,273,277` are updated in Task 5).
  2. `WheelArt` gains `SelectedPack`. `draw_wheel_row_art` draws it exactly like `Selected` (`wheel_row_selected`). `draw_wheel`: the "selected bar does not slide" rule and the final text pass treat `Selected` and `SelectedPack` alike; text style `kWheelSelected`; text x `kWheelPackTextX`; no subtitle. Two places in `draw_wheel` compare against `WheelArt::Selected` (the art loop at `select_art.cpp:735-748` and `draw_text_pass`); each must include `SelectedPack`.
  3. `WheelRowView` gains `bool open = false;` (meaningful for `Pack` and `SelectedPack` only). Add `[[nodiscard]] std::array<Vec2, 4> wheel_marker_quad(const Rect& row, bool open)`: a small solid triangle (as a degenerate quad, like `hint_arrow_quads`' heads) centred at `row.x + <constant>` and vertically centred; right-pointing when `!open`, down-pointing when `open`. The x constant goes next to `kWheelPackTextX` as a named `inline constexpr`, small enough to clear the label at x 54. `draw_wheel` draws it for Pack and SelectedPack rows that are `drawn(view)`, using `draw_solid` and a text-ink colour from `theme::color` (reuse `kPackInk` / `kSelectedInk`, do not add a colour).
  4. Fix the stale comments: `select_art.hpp:187-188` ("one song step moves the window 2 rows when it crosses a pack header") now reads that a step moves it at most one row; the behaviour of `wheel_scroll_start` itself does not change.
- **Mirror**: `select_art.cpp:143-156`, `:355-390`; `draw_wheel` `select_art.cpp:659-757`.
- **Validate**: `cmake --build build -j"$(nproc)" 2>&1 | tail -5` (the build breaks until Task 2 updates the `select_screen.cpp` call; do Tasks 1 and 2 together before building).

### Task 2: Row cursor and open pack in `SelectScreen`

- **File**: `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  1. State: replace `int selected_song_` and `std::vector<int> song_row_index_` with `int cursor_row_ = 0;` and `int open_pack_ = -1;` plus persisted `std::string open_pack_name_`, `std::string cursor_pack_name_`, `bool wheel_restored_ = false;`. Keep `WheelEntry::pack_index`. Keep `songs_`, `pack_labels_`, `wheel_rows_`. Add `std::vector<std::string> pack_names_` only if needed to map a pack index to `packs[i].name` (it is `ctx.library->packs()[i].name`, available in `rebuild`).
  2. A private `rebuild_rows()` that builds `song_packs` (as `rebuild` does at `select_screen.cpp:133-137`), calls `build_wheel_rows(song_packs, open_pack_)` and stores `wheel_rows_` and a `song_row_` vector. `rebuild()` and `toggle_pack()` both call it.
  3. `rebuild()` (`select_screen.cpp:97-156`): before clearing, capture `previous_key` (the song under the cursor, by `simfile_path`), `previous_header` (`cursor_pack_name_` when the cursor is on a header), `previous_open` (`open_pack_name_`). After refilling `songs_`, resolve in this order: (a) the previous song is found: `open_pack_` = its pack, cursor on its song row, chart clamp as today; (b) else a previous header or open pack name matches a pack that has songs: `open_pack_` from `previous_open` (or -1 when it was empty and `wheel_restored_`), cursor on that header; (c) first entry (`!wheel_restored_`): per Open Question 1, open the pack of `songs_[0]` and put the cursor on song 0. Set `wheel_restored_ = true`, and update `open_pack_name_` / `cursor_pack_name_` whenever the cursor or the open pack changes (`move_row`, `toggle_pack`, `rebuild`), so the next `enter()` can read them. The empty-library path (`ctx.library == nullptr`, no songs) leaves `wheel_rows_` empty and `cursor_row_` 0.
  4. `selected_wheel_row()` returns `cursor_row_` (clamped to the row count); `selected_song()` returns `songs_[wheel_rows_[cursor_row_].song_index].song` when the row is a Song row, else `nullptr` (and `nullptr` when `wheel_rows_` is empty). `selected_song_index()` returns that `song_index` or -1. Add accessors for tests: `bool on_pack_header() const`, `const std::string& open_pack_name() const`, `int selected_wheel_row() const` (make it public), and a `selected_pack_name()` for the header row's pack. `song_count()` stays the count of selectable songs (all packs).
  5. `move_song(int delta)` becomes `move_row(int delta)`: wrap over `wheel_rows_.size()`, set `selected_chart_ = 0`, slide exactly as before, call `request_preview_for_selected()` (stops the preview on a header). `apply_navigation` and the Up/Down cases (`select_screen.cpp:308-325`, `476-481`) call it. Keep the early return for zero rows.
  6. `toggle_pack()`: pack = the cursor header's `pack_index`; `open_pack_ = (open_pack_ == pack) ? -1 : pack`; `rebuild_rows()`; set `cursor_row_` to the row of that pack's header (linear scan for `Kind::Pack && pack_index == pack`); `scroll_start_ = 0; scroll_elapsed_ = 0` (D5); `selected_chart_ = 0`; `request_preview_for_selected()`; update the persisted names.
  7. Confirm (`select_screen.cpp:488-502`): when `on_pack_header()`, call `toggle_pack()` and `break` (never builds a `PlayRequest`, never transitions); otherwise unchanged.
  8. `render()` (`:627-666`): for a `Pack` row, `selected = r == selected_row`; `view.art = selected ? SelectedPack : Pack`; `view.open = row.pack_index == open_pack_`; label as today. Guard `if (!songs_.empty())` stays. The window, `list_window(selected_row, rows, ...)`, and the `selected_row - window.first` slot stay as written.
  9. Update the class comment at `select_screen.hpp:31-37` ("pack header rows inline") to say the headers are selectable and open/close.
- **Mirror**: `select_screen.cpp:97-156` (key-based restore), `:279-298` (slide), `:488-502` (Confirm).
- **Validate**: `cmake --build build -j"$(nproc)" 2>&1 | grep -E "error|warning" ; echo build-rc=$?` then `ctest --test-dir build -R "select" --output-on-failure` (expect failures in tests that assume every song is reachable; Task 6 fixes them).

### Task 3: First-entry and degenerate cases

- **File**: `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**: Make sure these hold (they are test cases in Task 6): empty library (no rows, Confirm/Up/Down no-ops, no crash); a library whose only songs are `SELECTABLE:NO` has no rows; a single pack with the cursor on its header and Confirm closes it, leaving one row, and a second Confirm reopens; the cursor index never exceeds `wheel_rows_.size() - 1` after a toggle; a persisted name that no longer exists falls through to case (c).
- **Validate**: part of Task 6's tests.

### Task 4: `select_art_test` cases

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**: Update `test_build_wheel_rows` (`:253`) for the two-argument form. Cases, with expected rows written out by hand (packs `{0,0,1,1,1}`): `open_pack = 0` gives rows `[P0, S0, S1, P1]` with `song_row = {1, 2, -1, -1, -1}`; `open_pack = 1` gives `[P0, P1, S2, S3, S4]` with `song_row = {-1, -1, 2, 3, 4}`; `open_pack = -1` gives `[P0, P1]`, all `-1`; an `open_pack` that is not in the list gives headers only; empty input gives nothing. Add a `wheel_marker_quad` test (closed points right: one vertex to the right of the other two, same y spread; open points down) with coordinates computed by hand from the named constant, not by calling the helper's own formula. Add a recorded-quads check for the marker and the selected header, modelled on `test_edit_row_recorded_quads` (`tests/select_art_test.cpp:990-1005`): draw a `SelectedPack` row through `draw_wheel` into a `GlQuadRenderer` and assert the marker quad's vertices against hand-computed coordinates.
- **Mirror**: `tests/select_art_test.cpp:253-282`, `:794-802`.
- **Validate**: `ctest --test-dir build -R select_art --output-on-failure`

### Task 5: Adapt existing `select_screen_test` cases

- **File**: `tests/select_screen_test.cpp`
- **Action**: UPDATE
- **Implement**: These tests assume Down visits every song; with one open pack they must be reviewed one by one:
  - `:156-192` `navigate_to_chart_count` loops `song_count()` Downs and `:167-186` wrap checks use song indexes (`make_library` has one pack of 3 songs, so the pack is open on entry; rows = 1 header + 3 songs; wrap now passes through the header). Loop `wheel_row_count()` times instead and expect the wrap to include a header stop.
  - `:225-253` (key-repeat) uses `selected_song_index()` of consecutive songs: still valid inside the one pack when the cursor starts on song 0; re-check the expected indexes after the header stop.
  - `:438-453` `test_selection_preserved_on_reenter`: keep, it now also asserts the pack stays open (add `open_pack_name()` equality).
  - `:630-632`, `:345`, `:502`, `:594` use `song_count()` and a before/after song index; re-run and fix expectations.
  - `:896-950` `test_wheel_slide`: with Pack One open (6 songs + 2 headers = 8 rows) the row count and window numbers change (it asserts `wheel_row_count() == 14`, `wheel_first_row()` values, wrap to song 11). Recompute by hand from `list_window` (visible 7, window centred on the cursor): do not derive them by calling the code.
  - `:983-1018` `test_wheel_skips_pack_rows` encodes the old behaviour (skip header, 2-row slide). Replace it with Task 6's tests.
- **Validate**: `ctest --test-dir build -R select_screen --output-on-failure`

### Task 6: New behaviour tests in `select_screen_test`

- **File**: `tests/select_screen_test.cpp`
- **Action**: UPDATE
- **Implement**: Add to `main()` (same inline `TEST_CHECK` style, one `std::cout` line per case), using `SlideFixture` (two packs of 6):
  1. **Cursor lands on a header and Confirm toggles**: from song 0 press Up: the cursor is on Pack One's header (`on_pack_header()`, `selected_song() == nullptr`, `selected_song_index() == -1`, `wheel_row_count() == 8`). Confirm: `open_pack_name()` is empty, `wheel_row_count() == 2`, cursor still on the header, `manager.active_id() == ScreenId::Select` (no transition), `request.song == nullptr`. Confirm again: reopened, `wheel_row_count() == 8`.
  2. **Closed pack is skipped by Up/Down**: with Pack One closed (two rows), Down moves header -> header and Up back; neither stops on a hidden song; `selected_song_index()` stays -1.
  3. **One pack open at a time (StepMania)**: close Pack One, move to Pack Two's header, Confirm: `open_pack_name()` is Pack Two, `wheel_row_count() == 8`, and Pack One's songs are gone (check by walking Down once around the wheel and asserting every song's `pack_of` equals Pack Two's).
  4. **Selection survives re-entry**: highlight a Pack Two song (open Pack Two, Down once), `manager.start(ScreenId::Select)` is not enough (it resets state): go through Confirm -> Gameplay (needs the `PlayRequest` the existing test `:438-453` builds) -> Back, as in `test_selection_preserved_on_reenter`, and assert the same song and `open_pack_name()`. Add the same for Calibration and Input remap by reusing the existing options-overlay return tests (`reopen_options_on_enter_`) with the cursor on a Pack Two song; assert the song is unchanged.
  5. **Header persists**: leave Select with the cursor on a header (Back -> Title, then Confirm on Title to return, using the existing title/select flow in the test file) and assert the cursor is on the same header on return.
  6. **First entry**: a fresh screen opens the first pack with the cursor on song 0 (Open Question 1 default); update the assertion if the owner picks another default.
  7. **Slide**: a Down inside an open pack moves the window by at most one row; a toggle leaves `wheel_scroll_offset() == 0`; a wrap snaps.
  8. **Render smoke**: with `loaded_theme()` / `loaded_text()` (as `:1012-1018`), render with the cursor on an open header, on a closed header, and on a song, at 1280x720; no crash. The quad-level assertions live in Task 4's recorded-quads test.
  9. **Empty library** still shows "NO SONGS FOUND" and ignores Confirm (existing test, `tests/select_screen_test.cpp:497-503`).
- **Mirror**: `tests/select_screen_test.cpp:438-453`, `:983-1018`.
- **Validate**: `ctest --test-dir build -R select_screen --output-on-failure`

### Task 7: Verification skill doc

- **File**: `.claude/skills/verify/features/song-select.md`
- **Action**: UPDATE
- **Implement**: Rewrite the wheel bullets (`:7`, `:29`, `:36-38`): the pack header is a selectable row; Confirm on it opens or closes the pack; only one pack is open; `wheel-move` skips hidden songs; the fixture has one pack (`BLAZE PACK`), so to see the one-open rule copy the fixture pack to a second directory name under a scratch `--songs` folder. State the first-entry position (Open Question 1 default).
- **Validate**: `grep -n "header" .claude/skills/verify/features/song-select.md`

### Task 8: Tick `TODO.md`

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Change line 67 `- [ ]` to `- [x]` for the "Open and close packs" item (gitignored; edit in place).
- **Validate**: `grep -n "Open and close packs" TODO.md`

---

## Validation

```bash
# Build with warnings as errors, lint, formatting, all tests (CI runs this)
scripts/check.sh

# Fast loop while developing
cmake --build build -j"$(nproc)" && ctest --test-dir build -R "select" --output-on-failure

# Sanitizers (CI job `sanitizer`)
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan -R select
```

Start green (53 of 53 at planning time), stay green.

## End-to-End Verification

Run through the `/verify` skill (`.claude/skills/verify/`, it launches the real window on the local X11 desktop; do not launch the game by hand from a probe). Needs two packs: copy the fixture pack folder to a second name inside a scratch songs directory (see `features/song-select.md`, Preconditions) and pass it with `--songs`.

1. Open Select: the first pack is open with its first song highlighted (Open Question 1 default); the other pack shows as a header only. Shot: `01-select`.
2. Up from the first song: the cursor is on the first pack's header (gold bar, down-pointing marker, left panel without song info).
3. Confirm on it: the pack collapses to its header, the cursor stays on it, the marker points right; the log shows no `[GameplayScreen] started`.
4. Down to the other pack's header, Confirm: it opens, the first pack stays collapsed (one open pack).
5. Down onto a song, Confirm: Gameplay starts for that song; Back from Gameplay returns to Select with the same song highlighted and its pack open.
6. Open options (Tab) -> Calibrate -> Back: the highlighted song is unchanged.

## Risks

| Risk | Mitigation (scope) |
|------|--------------------|
| Many existing `select_screen_test` cases assume Down visits every song (Task 5) | Review each; recompute window numbers by hand from `list_window`, not from the code. In scope. |
| Persisting by name when two packs share a display name | Pack names are directory names, unique under one `songs/` root; falls through to the first-entry case if not found. In scope. |
| `selected_song()` is now `nullptr` on a header: any code that dereferences it | Audited in this plan: `request_preview_for_selected`, `render`, `chart_count`, `selected_chart`, Confirm all test for null (`select_screen.cpp:158-178, 228, 489-494, 594, 606`). At planning time no file outside `select_screen.{hpp,cpp}` and the tests calls `selected_song()`. In scope. |
| Held Up/Down repeat now also stops on headers, slightly more key presses per pack | Intended (headers are rows). Only flagged. |
| The hint bar still reads "ENTER PLAY" (`kSelectHintItems`, `select_art.cpp:47-58`) and is wrong on a header | Out of scope for the first change; flagged as a follow-up. A header-specific hint is a layout change to a measured bar. |
| Windows/macOS | Pure C++ logic and code-drawn geometry only; no platform code. Out of scope. |
| Sorting of packs/songs is still filesystem order (#158) | Out of scope; open/close works on whatever order `scan_directory` returns. |

## Open Questions

1. **What does the wheel show on a first entry (no prior selection)?** Proposed default: the first pack open and its first song highlighted (today's behaviour, keeps the verify steps and AC3 "the pack that holds the highlighted song is open"). StepMania opens nothing on first entry (`SetOpenSection("")`), which would show only headers. Owner may prefer that for a big library; it is a one-line change in `rebuild()` case (c).
2. **Selected-header look.** Proposed: gold selected bar + dark label + triangle marker (D6), using existing art. If the owner wants a distinct pack-selected texture, that is an asset task, not part of this plan.
3. **Info panel on a header.** Proposed: empty (D7). StepMania shows the group banner; this repo has no pack-banner loader on the select screen (the test fixture writes a `Test Pack/banner.png`, but no screen code reads it). Out of scope unless the owner asks.

## Acceptance Criteria

- [ ] The cursor lands on a pack header; Confirm closes an open pack and opens a closed one, without leaving Select
- [ ] A closed pack shows only its header; Up/Down never stop on its hidden songs
- [ ] The pack holding the highlighted song is open on entering Select; the highlighted song survives Gameplay, Results, Calibration and Input remap
- [ ] Opening a pack closes the previously open one (StepMania `SetOpenSection`), cited on #166 with `src/WheelBase.cpp` and `src/MusicWheel.cpp:1439-1467`
- [ ] All tasks completed; `scripts/check.sh` passes; sanitizer run of the select tests passes
- [ ] Follows existing patterns (pure helper + hand-computed expectations, `TEST_CHECK` style)
