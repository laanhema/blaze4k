# Plan: Show the Chart Name for Edit Difficulty Charts (#84)

## Summary

Song select and results show only the passthrough label `Edit` for Edit charts, so a song with several edits shows identical rows. StepMania 5 and OpenITG both show an Edit chart's **description** in place of the label (`StepsDisplay.cpp:199-202`, `DifficultyMeter.cpp:158,228-233`). Blaze 4k already parses that field into `Chart::description` for `.sm`. For `.ssc`, the parser currently writes both `#CHARTNAME` and `#DESCRIPTION` into the same variable, and the last one wins. This plan does four things:

1. Makes the SSC loader match SM5. With `#VERSION` ≥ 0.74 (the default when the tag is absent), `#DESCRIPTION` becomes the description and `#CHARTNAME` no longer overwrites it. Below 0.74, `#DESCRIPTION` is a chart name, so the description stays empty.
2. Adds one shared, code-point-safe display helper, `chart_display_label`. It returns the description for Edit charts, falls back to the passthrough label when the description is empty, and shortens long names to a cell budget with `...`.
3. Uses that helper on the select-screen difficulty rows and on the results difficulty line. Both sites go through small pure row/line builders, so tests can check them.
4. Leaves `difficulty_color`, the high-score keys and every non-Edit label exactly as they are.

## User Story

As a player with packs that contain custom Edit charts
I want song select and results to show each edit's own name (for example "JBEAN") instead of "Edit"
So that I can tell several edits of one song apart and know which one I played

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `chart/simfile_parser` (SSC `#VERSION`/`#CHARTNAME`/`#DESCRIPTION`), `render/bitmap_font` (new `truncate_to_cells`), `screens/song_display_text` (new `chart_display_label`), `screens/select_screen` (new `difficulty_row_text` + draw site), `screens/results_screen` (new `results_difficulty_line` + draw site), tests (`parser_test`, `bitmap_font_test`, `select_screen_test`, `results_screen_test`), `TODO.md` |
| GitHub Issue | #84 |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Use `cmake --build build -j$(nproc)`, which builds incrementally |
| C++ compiler | GCC 16.2.1 (`/usr/bin/g++`) | C++20 |
| Baseline tests | **41/41 pass** | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` printed "100% tests passed out of 41" (0.44 s) on `main` @ `7ff909c`. Start green, stay green |
| Sandbox requirement | — | `audio_test` opens the real audio device. **Always** run ctest through the `bwrap` command above (it hides `/dev/snd` and the user's PipeWire socket) |
| New test targets | none | All new cases go into existing executables. `tests/CMakeLists.txt` and the root `CMakeLists.txt` do not change, because no new `.cpp` is added |
| Edit name field | `src/chart/chart.hpp:79` (`Chart::description`), set in `src/chart/note_parser.cpp:85` | Today it is used only by `resolve_difficulty`/`is_beginner` (`chart.hpp:66-75,96-98`) |
| SSC chart-block parse | `src/chart/simfile_parser.cpp:118-160` | `CHARTNAME` and `DESCRIPTION` both assign `cur_desc` (line 136). `#VERSION` is only read in pass 1 (lines 72-73), and only to set `is_ssc_` |
| SM chart parse | `src/chart/simfile_parser.cpp:161-186` | The 2nd `#NOTES` field is the description (line 170). **No change** |
| Display sites | `src/screens/select_screen.cpp:675-677` (difficulty row), `src/screens/results_screen.cpp:153-157` (difficulty line) | Both concatenate `chart.difficulty` directly |
| Row geometry (select) | `select_screen.cpp:657-678` | Row x = `0.04*width`, highlight bar width = `0.42*width`, pixel 2.5 → 15 px per cell. The wheel starts at `0.52*width` (`:632`). At 1280 px the bar holds 35 cells. The fixed parts `"> "`, `"  [10]   "` and `"100.00%"` take 18 cells, which leaves **17 cells** for the name |
| Results line geometry | `results_screen.cpp:153-157` | Centered at `0.5*width`, pixel 2.0 → 12 px per cell |
| Font cell model | `src/render/bitmap_font.hpp:16-37`, `bitmap_font.cpp:204-213` | Each visible code point takes one `6*pixel` cell, and zero-width code points take none. Use `next_code_point`/`is_zero_width` from `src/render/unicode_text.hpp` |
| High-score key | `src/data/high_scores.cpp:144-157` | The key is filename + title + artist + steps_type + difficulty + meter + note-data hash. It does **not** use description, so the SSC precedence change cannot orphan stored scores |
| Fixture SSC files | `tests/fixtures/reference_pack/Blaze Pack/Northern Lights/Northern Lights.ssc:1,16,33` | `#VERSION:0.83;` and `#DESCRIPTION:Standard;`/`Expert;` with no `#CHARTNAME`, so they behave the same after the change. **Do not** add Edit charts to the reference pack (other tests count its charts); use inline fixtures |
| Real data (local `songs/`, gitignored) | ITG 3 `.sm` files | Named edits, for example `JBEAN` (Aliens in our Midst, Dance All Night), `mDaWg` (All That Matters), `mDaWg & Hatena Zubon` (Bagpipe: 20 cells, which is longer than the 17-cell budget at 1280 px and so exercises truncation) |

---

## Value Provenance (StepMania 5 / OpenITG semantics)

Upstream revisions read: **StepMania** `stepmania/stepmania` branch `5_1-new` @ `825467bcd81c812b33ad684dc04dd151b2d5dec3`. **OpenITG** `openitg/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`.

| Behaviour | Upstream | Blaze 4k decision |
|---|---|---|
| **Which field is an Edit chart's display name** | SM5 `src/StepsDisplay.cpp:199-202`: `if( params.pSteps && params.pSteps->IsAnEdit() ) sDisplayDescription = params.pSteps->GetDescription();`. OpenITG `src/DifficultyMeter.cpp:158`: `dc == DIFFICULTY_EDIT ? pSteps->GetDescription() : CString()`, shown at `:228-233` | Display **`Chart::description`** for Edit charts. `#CHARTNAME` is never the edit display name in either engine |
| What counts as an edit | SM5 `src/Steps.h:72`: `IsAnEdit() { return m_Difficulty == Difficulty_Edit; }`, i.e. the *resolved* difficulty after `Steps::TidyUpData` (`Steps.cpp:283-292`) | Use `resolve_difficulty(chart.difficulty, chart.description, chart.meter) == StepsDifficulty::Edit` (`chart.hpp:66-82`, which already mirrors TidyUpData) |
| SSC `#DESCRIPTION` vs `#CHARTNAME` | SM5 `src/NotesLoaderSSC.cpp:319-324` (`SetChartName` → `steps->SetChartName`), `:336-349` (`SetDescription`: if `song->m_fVersion < VERSION_CHART_NAME_TAG && !for_load_edit` → `SetChartName(name)`, else → `SetDescription(name)`). The handlers are registered at `:606,609`. `src/NotesLoaderSSC.h:32`: `VERSION_CHART_NAME_TAG = 0.74f` | Keep `#CHARTNAME` in its own slot so it no longer overwrites the description. With version ≥ 0.74, `#DESCRIPTION` sets the description. With version < 0.74, `#DESCRIPTION` is treated as a chart name and the description stays empty. Tag order no longer matters |
| SSC version value | SM5 `src/Song.cpp:78` (`m_fVersion = STEPFILE_VERSION_NUMBER`), `src/Song.h:25` (`0.83f`). `src/NotesLoaderSSC.cpp:74-78` (song `#VERSION`) and `:315-318` (steps `#VERSION`) both set `song->m_fVersion`. `src/RageUtil.cpp:1861-1869` (`StringToFloat` = `strtof`, non-finite → 0) | Default **0.83**. Every `#VERSION` tag, in file order, header or chart block, updates the current version for the chart blocks after it. Unparsable or non-finite → **0.0**, so a junk value counts as "old" just as `strtof` would make it |
| Edits loaded from `.edit` files (`for_load_edit`) | `NotesLoaderSSC.cpp:340` | Not applicable: Blaze 4k does not load `.edit` files |
| SM `#NOTES` 2nd field | SM5 `src/NotesLoaderSM.cpp:319-321` (`SetDescription`, `SetCredit` and `SetChartName` all get it). OpenITG `src/NotesLoaderSM.cpp:32` (`SetDescription`) | Already the description (`simfile_parser.cpp:170`). No change |
| Edit name length | SM5 `src/Steps.cpp:570-578` and `Steps.h:26` cap the stored edit description at 255 bytes (`MakeValidEditDescription`) | **Not mirrored.** We store the raw name and shorten only for display (by cells, code-point safe). Byte-capping would split UTF-8 and buys nothing for a display-only field |
| Empty edit name | SM5 would show an empty description | Per the issue AC, an empty name falls back to the passthrough label (`Edit`). This is issue-driven, not an SM parity value |

---

## Pinned Semantics

### `truncate_to_cells(std::string_view text, std::size_t max_cells) -> std::string` (render/bitmap_font)

- `cells(text)` is the number of non-zero-width code points, which is the same model as `text_width`.
- If `cells(text) <= max_cells`, return `text` unchanged, byte for byte.
- Otherwise, if `max_cells >= 3`, keep the longest prefix whose visible cells number `max_cells - 3` and append ASCII `"..."`, which takes exactly 3 cells.
- Otherwise, when `max_cells < 3`, keep a prefix with `max_cells` visible cells and add no ellipsis.
- The cut always lands on a code-point boundary as reported by `next_code_point`. A zero-width code point directly after the last kept visible code point stays (it attaches to that glyph). The cut is made at the start of the first visible code point that does not fit.
- Malformed UTF-8 is not rewritten: the kept prefix is the original bytes. Decoding is prefix-stable, so `cells(result) <= max_cells` holds even for malformed input, and a fuzz test asserts it.
- The function never throws and never reads past `text.size()`.

### `chart_display_label(const Chart& chart, std::size_t max_name_cells) -> std::string` (screens/song_display_text)

- If `resolve_difficulty(chart.difficulty, chart.description, chart.meter) == StepsDifficulty::Edit` **and** `!chart.description.empty()`, return `truncate_to_cells(chart.description, max_name_cells)`.
- Otherwise, return `chart.difficulty` unchanged, with no truncation. Non-Edit rows and the `Edit` fallback display exactly as before.
- There is no translit for chart names, so the text is drawn natively. `draw_text`/`text_width` already handle UTF-8, folds and placeholders (#77).

### `difficulty_row_text(const Chart& chart, bool selected, const std::string& best, float max_row_width, float pixel) -> std::string` (screens/select_screen)

- `prefix = selected ? "> " : "  "` and `suffix = "  [" + std::to_string(chart.meter) + "]   " + best`. These match today's format at `select_screen.cpp:675-676`.
- `cell_px = 6.0f * pixel`, `row_cells = floor(max_row_width / cell_px)`, `fixed = cells(prefix) + cells(suffix)`. Both are ASCII, so `cells` equals `size()`.
- `name_budget = max(kMinDifficultyLabelCells, row_cells - fixed)`, computed with signed ints to avoid underflow. `kMinDifficultyLabelCells = 9` is the width of `Challenge`, the longest standard label, so an edit name is never shortened below the width a standard label already takes.
- Return `prefix + chart_display_label(chart, name_budget) + suffix`.
- Call site: `max_row_width = width * 0.42f` (the highlight-bar width already used at `:672`), `pixel = 2.5f`.
- Invariant at 1280 px: for any Edit name with a 1–2-digit meter and any `best`, `text_width(row, 2.5f) <= 0.42f * 1280`. The row then stays inside the highlight bar and clear of the wheel at `0.52*width`.

### `results_difficulty_line(const Chart* chart, float max_line_width, float pixel) -> std::string` (screens/results_screen)

- If `chart == nullptr`, return `"UNKNOWN"` (unchanged).
- Otherwise, `suffix = " " + std::to_string(chart->meter)` and `name_budget = max(9, floor(max_line_width / (6*pixel)) - suffix.size())`. Return `chart_display_label(*chart, name_budget) + suffix`.
- Call site: `max_line_width = width * 0.9f`, `pixel = 2.0f` (the line's existing pixel size).

### Unchanged on purpose

- `difficulty_color(chart.difficulty)` stays keyed on the passthrough label (`select_screen.cpp:677`), so edits keep the neutral tint.
- High-score keys (`high_scores.cpp:144-157`), log lines and `PlayRequest` identity do not change.
- `is_beginner()` and `resolve_difficulty` keep reading `description`. For SSC, that is now the SM5-correct description rather than the last of CHARTNAME/DESCRIPTION. SM5 `TidyUpData` reads `GetDescription()` too (`Steps.cpp:283-284`).

---

## Patterns to Follow

### Draw-only display helper with a doc comment (extend this file)

```cpp
// SOURCE: src/screens/song_display_text.hpp:9-24
// Display text for song metadata. Draw-only: identity (high-score keys,
// library lookup) and log lines keep the raw native title.
...
[[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata);
```

### Code-point iteration (cell counting)

```cpp
// SOURCE: src/render/bitmap_font.cpp:204-213
float text_width(const std::string& text, float pixel) {
    std::size_t cells = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (!is_zero_width(next_code_point(text, pos))) {
            ++cells;
        }
    }
    return static_cast<float>(cells) * 6.0f * pixel;
}
```

### Pure, test-exposed screen helper declared in the screen header

```cpp
// SOURCE: src/screens/select_screen.hpp:25-29
// Pure difficulty tint for the select screen's chart rows, keyed on the
// passthrough simfile label (case-insensitive): ...
[[nodiscard]] Color difficulty_color(const std::string& difficulty);
```

### SSC chart-block tag dispatch (iequals chain, per-block reset on NOTEDATA)

```cpp
// SOURCE: src/chart/simfile_parser.cpp:127-138
if (iequals(tag.name, "NOTEDATA")) {
    cur_stepstype.clear();
    cur_desc.clear();
    ...
} else if (iequals(tag.name, "CHARTNAME") || iequals(tag.name, "DESCRIPTION")) {
    cur_desc = tag.value();
```

### Error handling (untrusted input: safe parse with fallback, never throw)

```cpp
// SOURCE: src/chart/simfile_parser.cpp:23-29
double parse_double_safe(std::string_view s, double fallback = 0.0) {
    try {
        return std::stod(std::string(s));
    } catch (...) {
        return fallback;
    }
}
```

### Tests (plain executable, `TEST_CHECK` abort macro, section prints)

```cpp
// SOURCE: tests/parser_test.cpp:112-125 (inline SSC fixture via parse_string)
blaze4k::SimfileParser ssc_parser;
TEST_CHECK(ssc_parser.parse_string(ssc_content, ".ssc"));
TEST_CHECK(ssc_parser.is_ssc());
...
std::cout << "  - SSC header format and version detection verified.\n";
```

```cpp
// SOURCE: tests/select_screen_test.cpp:420-500 (temp pack through the real
// SongLibrary -> SimfileParser -> SelectScreen::render path, headless renderer)
blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
manager.render(renderer, 1280, 720);
```

```cpp
// SOURCE: tests/results_screen_test.cpp:61-73 (hand-built Chart)
Chart make_chart() { Chart chart; chart.difficulty = "Hard"; chart.meter = 9; ... }
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/chart/simfile_parser.cpp` | UPDATE | SSC pass 2: track `ssc_version` (default 0.83, updated by every `#VERSION` in tag order). Give `#CHARTNAME` its own slot. `#DESCRIPTION` → description only when version ≥ 0.74 |
| `src/render/bitmap_font.hpp` / `.cpp` | UPDATE | Add `truncate_to_cells(std::string_view, std::size_t)` next to `text_width` |
| `src/screens/song_display_text.hpp` / `.cpp` | UPDATE | Add `chart_display_label(const Chart&, std::size_t)` and update the header comment to cover chart names |
| `src/screens/select_screen.hpp` / `.cpp` | UPDATE | Add `difficulty_row_text(...)` and `kMinDifficultyLabelCells`. Lines 675-677 use it |
| `src/screens/results_screen.hpp` / `.cpp` | UPDATE | Add `results_difficulty_line(...)`. Lines 153-157 use it |
| `tests/parser_test.cpp` | UPDATE | Named Edit chart in an inline `.sm` and an inline `.ssc` fixture, plus the SSC precedence/version cases |
| `tests/bitmap_font_test.cpp` | UPDATE | `truncate_to_cells` cases and fuzz, plus `chart_display_label` cases |
| `tests/select_screen_test.cpp` | UPDATE | `difficulty_row_text` cases (non-Edit unchanged, Edit name, long name fits the bar), plus a temp-pack case with `.sm` and `.ssc` named edits through the real library and render path |
| `tests/results_screen_test.cpp` | UPDATE | `results_difficulty_line` cases, plus a headless render smoke with an Edit chart |
| `TODO.md` | UPDATE | Tick line 42 (`(#84)`) |

Do **not** touch `.agents/stories/todo-stories.md`. It has an unrelated uncommitted change: do not revert, edit or stage it.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: SSC `#CHARTNAME` / `#DESCRIPTION` / `#VERSION` per SM5

- **File**: `src/chart/simfile_parser.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add `constexpr double kSscDefaultVersion = 0.83;` (SM5 `Song.h:25`) and `constexpr double kSscChartNameTagVersion = 0.74;` (SM5 `NotesLoaderSSC.h:32`) in the anonymous namespace, each with a citation comment.
  - Add `double parse_ssc_version(std::string_view)`, which mirrors `StringToFloat`: `parse_double_safe(s, 0.0)`, and if the result is not `std::isfinite`, return 0.0. Note: `std::stod` throws `out_of_range` on overflow, and the existing catch-all already maps that to the fallback 0.0.
  - In the SSC branch (lines 119-160), add `double ssc_version = kSscDefaultVersion;` before the loop. Add an `else if (iequals(tag.name, "VERSION")) ssc_version = parse_ssc_version(tag.value());` arm. Do **not** reset it on `NOTEDATA`, because SM5 stores it on the song (`NotesLoaderSSC.cpp:77,317`).
  - Split line 136. Add `std::string cur_chart_name;`, reset it on `NOTEDATA`. `CHARTNAME` → `cur_chart_name = tag.value();`. `DESCRIPTION` → `if (ssc_version < kSscChartNameTagVersion) cur_chart_name = tag.value(); else cur_desc = tag.value();`.
  - `cur_chart_name` is only parsed; nothing displays it. Add a comment citing `NotesLoaderSSC.cpp:319-349` and `StepsDisplay.cpp:199-202`, saying that SM5 shows the description, not the chart name, for edits. To avoid an unused-variable warning, either do not store it (a no-op arm with the comment) or keep the local. **Preferred:** a no-op `CHARTNAME` arm (`// Chart name: not the edit display name (SM5 StepsDisplay.cpp:199-202); deliberately not stored.`), and the `< 0.74` `DESCRIPTION` case is a no-op too. This avoids a dead field on `Chart` (YAGNI).
  - Leave the pass-1 `VERSION` → `is_ssc_` handling (lines 72-73) as is.
  - Add `#include <cmath>`.
- **Mirror**: `simfile_parser.cpp:127-138` (iequals chain), `:23-29` (safe parse)
- **Validate**: `cmake --build build -j$(nproc)`, then the sandboxed ctest command. `parser_test`, `song_library_test` and `parser_hardening_test` must stay green.

### Task 2: `truncate_to_cells` (code-point-safe shortening)

- **File**: `src/render/bitmap_font.hpp`, `src/render/bitmap_font.cpp`
- **Action**: UPDATE
- **Implement**: Per Pinned Semantics. Declare it after `text_width` with a doc comment (`[[nodiscard]] std::string truncate_to_cells(std::string_view text, std::size_t max_cells);`). In the implementation:
  - Count total cells first; if they fit, return `std::string(text)`.
  - Otherwise, walk the code points with `next_code_point`, recording `start = pos` before each decode. When a visible code point would exceed `keep = max_cells >= 3 ? max_cells - 3 : max_cells`, cut at `start`.
  - Append `"..."` only when `max_cells >= 3`.
- **Mirror**: `bitmap_font.cpp:204-213`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 3: `chart_display_label` shared helper

- **File**: `src/screens/song_display_text.hpp`, `src/screens/song_display_text.cpp`
- **Action**: UPDATE
- **Implement**: Per Pinned Semantics. Include `chart/chart.hpp`, `<cstddef>`, and `render/bitmap_font.hpp` (already included in the `.cpp`). It returns `std::string` by value, because the truncated result is a new string. Extend the header comment: "chart difficulty labels: Edit charts show their description (SM5 `StepsDisplay.cpp:199-202`, OpenITG `DifficultyMeter.cpp:158`); identity keeps the raw label".
- **Mirror**: `song_display_text.cpp:17-25`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 4: Select screen difficulty row

- **File**: `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Declare `inline constexpr std::size_t kMinDifficultyLabelCells = 9;` and `[[nodiscard]] std::string difficulty_row_text(const Chart& chart, bool selected, const std::string& best, float max_row_width, float pixel);` in the header, after `difficulty_color`. Add a doc comment. `Chart` is already forward-declared at `select_screen.hpp:15`, which is enough for a `const Chart&` parameter. The `.cpp` already sees the full type.
  - Implement per Pinned Semantics.
  - Replace lines 675-677 with `const std::string row = difficulty_row_text(chart, selected, best, width * 0.42f, 2.5f);` and keep `draw_text(renderer, row, diff_x, diff_y, 2.5f, difficulty_color(chart.difficulty));`.
  - Update the comment at `:653-655` to mention that Edit charts show their name.
- **Mirror**: `select_screen.cpp:100-117` (pure free function in the namespace), `select_screen.hpp:25-29`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 5: Results screen difficulty line

- **File**: `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: UPDATE
- **Implement**: Declare `[[nodiscard]] std::string results_difficulty_line(const Chart* chart, float max_line_width, float pixel);` with a doc comment. Replace lines 153-156 with `const std::string diff_line = results_difficulty_line(summary_.chart, width * 0.9f, 2.0f);`. Keep the draw call unchanged. Include `screens/song_display_text.hpp` (already included for the title) and `<algorithm>` if needed.
- **Mirror**: `select_screen.hpp:25-29`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 6: Parser tests (`.sm` and `.ssc` named Edit fixtures)

- **File**: `tests/parser_test.cpp`
- **Action**: UPDATE
- **Implement**: Add a new section before `main`'s final print, written in the existing inline style.
  - **SM**: `#NOTES:dance-single:JBEAN:Edit:10:0,0,0,0,0:` + 4 rows `;` → one chart with `difficulty == "Edit"`, `description == "JBEAN"` and `meter == 10`. Use the real-data shape from `Dance All Night.sm:30-34`.
  - **SSC, modern**: `#VERSION:0.83;` + `#NOTEDATA:;#STEPSTYPE:dance-single;#CHARTNAME:Chart Title;#DESCRIPTION:My Edit;#DIFFICULTY:Edit;#METER:12;#NOTES:...;` → `description == "My Edit"`.
  - **SSC, tag order reversed** (`#DESCRIPTION` before `#CHARTNAME`) → still `"My Edit"`. This is the regression for the old last-wins bug.
  - **SSC, CHARTNAME only** → `description.empty()`.
  - **SSC, no `#VERSION` at all** (parsed with the `.ssc` extension) → defaults to 0.83, so `#DESCRIPTION` is the description.
  - **SSC, `#VERSION:0.70;`** → `#DESCRIPTION:Old Name` gives `description.empty()` (pre-0.74 chart name).
  - **SSC, `#VERSION:garbage;`** → 0.0, so the description is empty, which matches `strtof`.
  - **SSC, two `NOTEDATA` blocks**: the second block's description must not leak from the first (the NOTEDATA reset).
- **Mirror**: `tests/parser_test.cpp:112-125`
- **Validate**: sandboxed ctest; `parser_test` passes

### Task 7: Truncation and label unit tests

- **File**: `tests/bitmap_font_test.cpp`
- **Action**: UPDATE
- **Implement**: Add `test_truncate_to_cells()` and `test_chart_display_label()`, both called from `main`.
  - **Truncation**:
    - A fitting string is returned unchanged (and `""` → `""`).
    - `truncate_to_cells("mDaWg & Hatena Zubon", 17) == "mDaWg & Hatena..."`, with `text_width(..., 1) == 17*6`.
    - Multi-byte: `"VerTex\xC2\xB3 Edit Name"` with a small budget never cuts inside `\xC2\xB3`.
    - A combining mark after the last kept glyph is kept.
    - `max_cells` 0, 1 and 2 → no ellipsis and at most that many cells.
    - Fuzz with 10,000 random byte strings and random budgets 0..40 (`std::mt19937{84}`): the result's cells are ≤ the budget, and the result is either the input or (when the budget is ≥ 3) ends with `"..."`.
  - **Labels**:
    - Edit + `"JBEAN"` → `"JBEAN"`.
    - Edit + empty → `"Edit"`.
    - Lowercase `"edit"` + empty → `"edit"` (passthrough).
    - `"Hard"` + description `"Some Author"` → `"Hard"`. Non-Edit charts never show the description.
    - `"Challenge"` → `"Challenge"`.
    - Invalid label `""` + description `"Edit"` resolves to Edit and shows `"Edit"` (SM5 `IsAnEdit` on the resolved difficulty).
    - Long Edit name + budget 10 → 10 cells ending in `"..."`.
- **Mirror**: `tests/bitmap_font_test.cpp:127-161`, `:163-189` (fuzz smoke)
- **Validate**: sandboxed ctest; `bitmap_font_test` passes

### Task 8: Select and results screen tests

- **File**: `tests/select_screen_test.cpp`, `tests/results_screen_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - **select, `difficulty_row_text`**:
    - A `"Hard"` meter-9 chart, selected, with best `"---"`, must equal exactly `"> Hard  [9]   ---"`; unselected, it must equal `"  Hard  [9]   ---"`. This shows the format is unchanged.
    - An Edit `"JBEAN"` chart gives `"> JBEAN  [10]   100.00%"`.
    - Edit + empty description gives `"> Edit  [10]   ---"`.
    - An Edit with a 60-char name at `max_row_width = 0.42f*1280`, pixel 2.5 gives `text_width(row, 2.5f) <= 0.42f*1280`, the row contains `"..."`, and the `"[10]"` and best substrings are intact.
    - At `0.42f*320`, the name budget clamps to 9 cells, with no crash and no underflow.
  - **select, real path**: a temp pack (mirror `test_special_character_titles`, `:420-500`) with one `.sm` song whose `#NOTES` has a named Edit plus a Hard chart, and one `.ssc` song with `#CHARTNAME` + `#DESCRIPTION` Edit charts (including a long UTF-8 name).
    - After `SongLibrary::scan_directory`, `chart_display_label` on the loaded charts returns the names.
    - Navigate with Right through the difficulties and render headlessly at 1280×720 and 640×480.
    - `make_sm` only writes an empty description, so add an optional description parameter or write the file inline. Prefer an inline string so existing callers are untouched.
  - **results**:
    - `results_difficulty_line(nullptr, ...) == "UNKNOWN"`.
    - A `"Hard"` meter-9 chart gives `"Hard 9"`, unchanged.
    - An Edit `"JBEAN"` meter-10 chart gives `"JBEAN 10"`.
    - Edit + empty gives `"Edit 10"`.
    - A long name at `0.9f*1280`, pixel 2.0 has width ≤ the limit.
    - Add a headless render smoke of `ResultsScreen` with an Edit chart summary (mirror `test_render_and_reenter`, `:289`).
- **Mirror**: `tests/select_screen_test.cpp:249-276,420-500`, `tests/results_screen_test.cpp:61-90,289-...`
- **Validate**: sandboxed ctest; all 41 tests pass

### Task 9: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Change line 42 from `- [ ]` to `- [x]` (the `(#84)` entry).
- **Validate**: `git diff TODO.md` shows a one-character change

---

## Validation

```bash
# Build (existing Release build dir, host g++)
cmake --build build -j$(nproc)

# Lint
# (no linter configured in this repo; the build must be warning-clean for touched files)

# Tests: MUST be sandboxed, because audio_test opens the real audio device
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: "100% tests passed out of 41". No new ctest targets are added, so the count stays at 41.

## End-to-End Verification

1. **Real-pack parse (headless, no window, no audio)**: build a scratch program in the scratchpad (not in the repo) that links `blaze4k_core`, scans the local `songs/` folder with `SongLibrary`, and prints `chart_display_label(chart, 17)` for every chart where `resolve_difficulty(...) == Edit`. Expected output includes:
   - `JBEAN` for *ITG3 / Aliens in our Midst* and *Dance All Night*
   - `mDaWg` for *All That Matters*
   - `mDaWg & Hatena...` for *Bagpipe*

   No Edit chart should print `Edit` unless its description really is empty. Alternatively, put a temporary `std::cerr` in a test and remove it afterwards. Do not commit the scratch program.
2. **Optional visual check (owner only)**: launch the game, open song select on *Bagpipe* and press Right to reach the Edit row. The row should show `mDaWg & Hatena...  [10]   ---` inside the highlight bar, with the neutral Edit tint. Play it, and the results screen should show `mDaWg & Hatena Zubon 10`. The agent must not launch the GUI binary itself, because it opens a window and plays audio.
3. Run the sandboxed ctest command above: 41/41.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Some SSC files (written by tools other than SM5) may put the edit name **only** in `#CHARTNAME` and leave `#DESCRIPTION` empty. They would then show `Edit` | This is faithful to SM5 `StepsDisplay`. The local packs are all `.sm`, so there is no real-data evidence yet. Raised as Open Question 1 | In scope: flag only |
| The SSC precedence change alters `Chart::description` for SSC files that have both tags, and that feeds `resolve_difficulty` | It only matters when `#DIFFICULTY` is unrecognized, and SM5 `TidyUpData` uses the same description (`Steps.cpp:283-284`). High-score keys do not include description (`high_scores.cpp:144-157`), so no scores are orphaned | In scope: covered by parser tests |
| Pre-0.74 SSC edits lose their name: `#DESCRIPTION` becomes the chart name, so they show `Edit` | Faithful to SM5 (`NotesLoaderSSC.cpp:340-343`). Files this old are rare (0.74 dates from the SM5 preview era) | In scope: test pins the behavior |
| Truncation splits a UTF-8 sequence or miscounts zero-width marks | Cut only at `next_code_point` boundaries, count with the same `is_zero_width` rule as `text_width`, and fuzz test it | In scope |
| `row_cells - fixed` underflows on narrow windows (minimum width 320, `config.hpp:20`) | Compute in signed `int` and clamp to `kMinDifficultyLabelCells`. A test runs at 320 px | In scope |
| At narrow widths (≤ ~800 px), even standard rows such as `Challenge [12] 100.00%` already overflow the highlight bar. Edit rows clamped to 9 cells overflow by the same amount | This layout limit already exists and Edit rows are never worse than `Challenge` rows. Not fixed here | Out of scope: note only |
| Results line: a very long name on a narrow window | The budget comes from `0.9*width`, with a floor of 9 cells | In scope |
| Pass 1 (`simfile_parser.cpp:69-116`) walks **all** tags, including SSC per-chart `#BPMS`/`#OFFSET`/`#STOPS`, so per-chart timing may leak into song-level timing | Unrelated to #84. Noticed while reading. Do not change it here; mention it in the PR as a possible follow-up | Out of scope: flag only |

---

## Open Questions

1. **Fall back to `#CHARTNAME` when an SSC Edit's `#DESCRIPTION` is empty?** SM5 and OpenITG show only the description for edits, so a chart name alone would display as `Edit`. **Proposed default: no** (principle 2, "Faithful, not novel"). We do not store the chart name. If the owner finds real SSC edits that carry their name only in `#CHARTNAME`, adding a `Chart::chart_name` field and a one-line fallback in `chart_display_label` would be a small, low-risk follow-up. Not blocking.
2. **Ellipsis style**: three ASCII dots `...` (3 cells) or a single cell? The font folds `…` (U+2026) to `.`, one cell (#77), so `…` would look like a single dot and could be misread. **Proposed default: ASCII `...`**. Not blocking.
3. **Results-line budget**: the plan allows the full name on results up to `0.9*width`, while select shortens to the bar width. Both use the same `chart_display_label` source, so the name is "the same", but on results it may be longer (less truncated) than on select. **Proposed default: accept**, because results has the room and the full name is more useful there. If the owner wants the two strings identical, pass the select budget instead. Not blocking.

---

## Acceptance Criteria

- [ ] In song select, an Edit chart's row shows its chart name (description), with meter and best % as today, instead of "Edit"
- [ ] The results screen difficulty line shows the same name for Edit charts
- [ ] An Edit chart with an empty name still shows "Edit". Non-Edit difficulties (Beginner…Challenge) display exactly as before (asserted with exact-string tests)
- [ ] A long name is shortened with `...` so the row stays inside the highlight bar at 1280 px and does not overlap the meter or best-% columns or the wheel
- [ ] Parser and select tests cover an `.sm` and an `.ssc` fixture with a named Edit chart
- [ ] SSC `#DESCRIPTION`/`#CHARTNAME` precedence matches SM5 (`NotesLoaderSSC.cpp:319-349`) and is independent of tag order
- [ ] `difficulty_color` stays keyed on the label, and high-score keys do not change
- [ ] All tasks completed. The build passes warning-clean, the sandboxed ctest passes 41/41, and the code follows existing patterns
- [ ] `.agents/stories/todo-stories.md` is untouched
