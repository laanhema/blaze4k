# Plan: Add left margin to the difficulty name text in song select

## Summary

In the song select difficulty rows the name (`HARD`, `CHALLENGE`, an Edit chart's name) is drawn left-aligned at `row.x + kDiffNameX` with `kDiffNameX = 15` (`src/screens/select_art.hpp:67`, draw site `src/screens/select_art.cpp:588-591`). Measuring the baked row textures shows why it looks cramped: the coloured tab does not start at the row's content x. Its fill starts at **12.5 px** at the height of the name's cap top in a normal row and at **16.0 px** in the selected row (taller row, larger font, 2 px gold frame). So the first letter's ink clears the tab edge by **3.4 px** in a normal row and by **0.0 px** in the selected row: there it touches the gold frame. Because the selected row is 3.4 px worse at any shared offset, one offset cannot meet AC 2. This plan uses two offsets, the same shape the wheel already has (`kWheelSongTextX` / `kWheelSelectedTextX`): `kDiffNameX` 15 → **21** (+6) and a new `kDiffNameSelectedX` = **25** (+10). The gaps become 9.4 px (normal) and 10.0 px (selected) at the cap top. `kDiffNameBudget` drops 128 → **118**, so the name's right limit stays at x 143 in the selected row (139 in the others) and truncated Edit names still end inside the tab. The meter number, ticks, best % and all row art are untouched. Tests: a new real-font check pins both gaps, AC 2 and the right limit. This is presentation only: no clock, judgment or input code is touched.

## User Story

As a player browsing song select
I want the difficulty name to have a visible left margin inside its coloured tab
So that the name reads cleanly in every row, and most of all in the row I have selected

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/screens/select_art.hpp` (2 constants changed, 1 added, comments), `src/screens/select_art.cpp` (1 expression), `tests/select_art_test.cpp`, `TODO.md` |
| GitHub Issue | #126 ([TODO-33] Add left margin to the difficulty name text in song select) |
| PRD Phase | N/A |
| Branch (suggested) | `feature/059-diff-name-left-margin` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and current. Build with `cmake --build build -j` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | Run on `main` @ `01af2bf` with the sandboxed ctest command in Validation (2.87 s). The count stays **51**: no test executable is added |
| Sandbox requirement | n/a | Some tests open the real sound device. **Always** run ctest inside the `bwrap` prefix in Validation |
| Font measurements | scratch program linked against `build/libblaze4k_core.a`, `TextRenderer::measure` / `line_height` / `ascent` at 1280x720 (headless, no GL) | Reference px. See "Measured geometry" |
| Texture measurements | `assets/theme/cabinet/diff_row_*.png` and `diff_row_*_selected.png` (PIL, content box from `manifest.json`) | All five colours share the geometry. See "Measured geometry" |
| Mock measurement | `docs/cabinet-theme/reference/cabinet-v3-select.png` (1280x720, PIL) | The mock is not the target here, but it explains the cramping. See "Measured geometry" |
| Label survey | `SongLibrary::scan_directory` over `songs/` (owner's packs, gitignored): 1117 charts, 24 distinct row labels | One real label is newly truncated. See "Truncation impact" |
| Fixture pack | `tests/fixtures/reference_pack` | `Blaze Anthem` has CHALLENGE / HARD / MEDIUM / EASY / BEGINNER. No Edit chart. Edit rows need `songs/` |
| Off-limits file | `.agents/issues/todo-issues.md` | Unrelated, uncommitted owner edit. Do **not** stage, revert, edit or commit it |

### Forward references to #126

`grep -rn "#126\|TODO-33"` over `src/`, `tests/`, `docs/`, `.claude/`, `.agents/plans/` finds only `TODO.md:49` (the source line, already tagged `(#126)`, which Task 5 ticks). There are no placeholders in code.

### Measured geometry (reference px, relative to the row's content x)

**Name line box** (centred in the row by `centred_top`, `src/screens/select_art.cpp:61-64`):

| Row | Style | line height | ascent | cap height | cap top y | baseline y |
|---|---|---|---|---|---|---|
| normal (44 tall) | `kDiffName` 20 px | 31.48 | 22.70 | 13.76 | 15.2 | 29.0 |
| selected (52 tall) | `kDiffNameSelected` 22 px | 34.63 | 24.97 | 15.14 | 18.5 | 33.7 |

The first letter's ink starts 0.9 px (20 px) / 1.0 px (22 px) right of the draw x (left side bearing of B, E, M, H, N, D; C is 0.7).

**Baked tab, left side** (`diff_row_hard.png`, `diff_row_hard_selected.png`; beginner, easy and challenge checked and identical):

| Row | at | art outer left (outline / gold frame) | tab fill left |
|---|---|---|---|
| normal | cap top (y 15.2) | 11.5 | **12.5** |
| normal | mid (y 22) | 10.0 | 11.0 |
| normal | baseline (y 29.0) | 8.5 | 9.5 |
| selected | cap top (y 18.5) | 13.5 | **16.0** |
| selected | mid (y 26) | 12.0 | 14.0 |
| selected | baseline (y 33.7) | 10.5 | 12.5 |

The tab fill is 149.5 px wide in both (it ends at 161 / 164 at mid, as #122 measured). The issue's "~150 px wide" is right about the width. What it misses is that the tab **starts** 11–16 px into the content box, not at 0.

**Gap from the tab fill's left edge to the first letter's ink:**

| Row | at | today (x 15) | uniform +8 (23 / 23) | uniform +10 (25 / 25) | **plan (21 / 25)** |
|---|---|---|---|---|---|
| normal | cap top | 3.4 | 11.4 | 13.4 | **9.4** |
| selected | cap top | **0.0** | 8.0 | 10.0 | **10.0** |
| normal | mid | 4.9 | 12.9 | 14.9 | **10.9** |
| selected | mid | 2.0 | 10.0 | 12.0 | **12.0** |
| normal | baseline | 6.4 | 14.4 | 16.4 | **12.4** |
| selected | baseline | 3.5 | 11.5 | 13.5 | **13.5** |

With one shared offset the selected gap is always 3.4 px smaller than the normal one at the cap top, so AC 2 needs two offsets. The pair 21 / 25 is the only integer pair inside the issue's 6–10 px range where the selected gap is at least the normal gap at all three heights (+7 / +10 gives 10.4 vs 10.0 at the cap top).

**Mock:** in `cabinet-v3-select.png` the tab fill starts at row x + 1.5 (normal) / + 2 (selected) at mid, and the name ink sits 14.5 / 15 px inside it. The baked textures put the same art about 10 px (normal) / 12 px (selected) further right inside their content boxes, while `kDiffNameX = 15` was measured from the mock. That lost offset is the cramping. The plan restores about three quarters of the mock's gap, within the issue's range.

**Code-drawn Edit rows** (`draw_difficulty_row_art`, `src/screens/select_art.cpp:525-540`): the Edit tab is `skewed_quad({row.x, row.y, kEditTabWidth = 150, row.h}, 0.213)`, so it runs from 0 to 150 at mid (the mock's position), 11–14 px left of the baked tabs. Its left edge at the cap top is 1.5 / 1.6, so an Edit name has a 14.4 px gap today and 20.4 / 24.4 px after this change. Its right edge at the name's baseline is 148.5 (normal) / 148.4 (selected). This is the binding limit on the right.

**Right side:**

| Quantity | normal | selected |
|---|---|---|
| `CHALLENGE` width (widest standard name) | 99.1 | 107.2 |
| `BEGINNER` width | 88.5 | 95.7 |
| Name right limit today (`15 + 128`) | 143 | 143 |
| Name right limit, plan (`x + 118`) | 139 | **143** |
| `CHALLENGE` right end, plan | 120.1 | 132.2 |
| Edit tab right edge at the baseline | 148.5 | 148.4 |
| Baked tab right edge at the baseline | 159.0 | 162.0 |
| Left edge of the widest meter (`20` selected, centred at 190) | 176.9 | 175.1 |

The measured width includes 2 px of trailing tracking, so the ink ends about 2.6 px before the limit. With the limit at 143 the ink stays at least 8 px inside the Edit tab and 32 px before the meter number, exactly as today.

### Truncation impact (real labels, `songs/`)

| Label | selected width | today, selected (128) | plan, selected (118) | plan, normal (118) |
|---|---|---|---|---|
| `CHALLENGE` | 107.2 | fits | fits | fits |
| `Phrekwenci` | 112.9 | fits | fits | fits |
| `M. Emirzian` | 117.0 | fits | fits | fits |
| `M. Poveromo` (ITG3 / Dawn) | 126.9 | fits | **`M. Pover...`** | fits (117.4) |
| `K. Ward & Kyrandian` (ITG3 / Get Away) | 203.2 | `K. Ward & ...` | `K. Ward ...` | `K. Ward & ...` (unchanged) |
| `mDaWg & Hatena Zubon` (ITG3 / Bagpipe) | 230.9 | `mDaWg & ...` | `mDaWg &...` | `mDaWg & ...` (was `mDaWg & H...`) |

All other 18 labels are under 100 px. One label, `M. Poveromo`, goes from fitting to truncated, and only while its row is selected. Keeping it whole at x 25 would need a 127 px budget, which puts its last letter about 1 px past the Edit tab's right edge at the baseline. See Open Question 1.

A per-row budget (122 normal, 118 selected, sharing the 143 limit) was checked: no real label differs between 118 and 122 in the 20 px style, so the plan keeps one `kDiffNameBudget`.

---

## Patterns to Follow

### Naming: a text x per row state (the wheel does this already)
```cpp
// SOURCE: src/screens/select_art.hpp:85-88
// Wheel text x from the row's content x (manifest notes) and the right limit.
inline constexpr float kWheelSongTextX = 26.0f;
inline constexpr float kWheelSelectedTextX = 30.0f;
inline constexpr float kWheelPackTextX = 54.0f;

// SOURCE: src/screens/select_art.cpp:651-653
const float text_dx = art == WheelArt::Selected ? kWheelSelectedTextX
                      : art == WheelArt::Pack   ? kWheelPackTextX
                                                : kWheelSongTextX;
```

### Layout constants (reference px, measured-from comment, issue number for deviations)
```cpp
// SOURCE: src/screens/select_art.hpp:64-71
// Inside a difficulty row, from the row's content x (measured from the mock; the
// manifest notes say 24 / 183 / 216). The meter and ticks sit 16px right of the
// mock (#122): the slanted tab reaches x 167.5 at the cap top of a selected row.
inline constexpr float kDiffNameX = 15.0f;
inline constexpr float kDiffMeterCentreX = 190.0f;
inline constexpr float kDiffTickX = 210.0f;
inline constexpr float kDiffBestRight = 547.0f; // measured from mock: 17px in from the right edge
inline constexpr float kDiffNameBudget = 128.0f; // the tab is ~150px wide at mid-height
```

### The draw site (the only consumer of `kDiffNameX`)
```cpp
// SOURCE: src/screens/select_art.cpp:582-591
const int slot = static_cast<int>(i);
const bool selected = slot == selected_slot;
const Rect row = difficulty_row_rect(slot, selected_slot);
const theme::TextStyle style =
    with_color(selected ? theme::text::kDiffNameSelected : theme::text::kDiffName,
               difficulty_row_style(*rows[i].chart).colors.ink);
text->draw(renderer,
           text->truncate(difficulty_row_label(*rows[i].chart), style, L.px(kDiffNameBudget)),
           L.x(row.x + kDiffNameX), centred_top(*text, L, row.y, row.h, style), style,
           TextAlign::Left);
```

### Error handling
Not applicable. These are constexpr layout values. Draw helpers already no-op for null services (`src/screens/select_art.hpp:9-10`).

### Tests (plain executable, `TEST_CHECK` abort macro, section print, real fonts via `loaded_text()`, test-local art constants with provenance)
```cpp
// SOURCE: tests/select_art_test.cpp:613-640
void test_meter_clearance() {
    // The baked tab's right edge at the cap top (diff_row_hard{,_selected}.png; the
    // tab is slanted, so this is its widest point under the digits), and the first
    // tick's ink inset (diff_tick.png opaque x at mid-height).
    constexpr float kTabTopNormal = 163.5f;
    constexpr float kTabTopSelected = 167.5f;
    constexpr float kTickInkInset = 3.0f;
    constexpr float kMinClear = 4.0f;
    ...
    blaze4k::TextRenderer& text = loaded_text();
    const std::pair<theme::TextStyle, float> cases[] = {
        {theme::text::kDiffMeter, kTabTopNormal},
        {theme::text::kDiffMeterSelected, kTabTopSelected},
    };
    for (const auto& [style, tab_top] : cases) { ... }
    std::cout << "  - meter clearance ok.\n";
}
```

Existing budget checks that must keep passing unchanged: `tests/select_art_test.cpp:579-591` (a long Edit name truncates within `kDiffNameBudget` in both styles; every standard name fits it in the selected style) and `tests/select_screen_test.cpp:359-368` (every fixture chart's label, both styles). At 118 they hold: `CHALLENGE` selected is 107.2, and the long test name truncates to 111.5 / 114.5.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | UPDATE | `kDiffNameX` 15 → 21, new `kDiffNameSelectedX = 25`, `kDiffNameBudget` 128 → 118, comments |
| `src/screens/select_art.cpp` | UPDATE | The names loop picks the x by `selected` (one expression, `:590`) |
| `tests/select_art_test.cpp` | UPDATE | New `test_name_margin()` (gaps, AC 2, right limit, real fonts), header comment line |
| `TODO.md` | UPDATE | Tick line 49 (`(#126)`) |

No file is created. `tests/select_screen_test.cpp`, `src/render/theme.hpp`, `assets/` and `tests/CMakeLists.txt` are not changed.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Capture the "before" screenshots

- **File**: none (evidence only, in `/tmp/blaze4k-verify/126-before/evidence/`)
- **Action**: RUN, before editing any source file
- **Implement**: Follow the `verify` skill (`.claude/skills/verify/SKILL.md`, recipe `.claude/skills/verify/features/song-select.md`) with the unmodified build:
  ```bash
  B=.claude/skills/verify/scripts/b4k.sh
  R=/tmp/blaze4k-verify/126-before
  $B doctor
  $B launch --run $R -- --songs "$PWD/songs"
  $B keys $R Return && $B wait-screen $R Select
  $B shot $R 01-challenge-selected        # Anubis: CHALLENGE selected, HARD..BEGINNER unselected
  $B stop $R
  ```
  Read the PNG. If `songs/` is missing, launch without `-- --songs ...` and press `Down` once for `Blaze Anthem` (same five difficulties). If `shot` fails because the window is not viewable, follow the skill: do not move the owner's windows, report the missing "before" shot, and continue. AC 5 needs only the "after" shots.
- **Mirror**: `.claude/skills/verify/features/song-select.md`
- **Validate**: `ls /tmp/blaze4k-verify/126-before/evidence/01-challenge-selected.png`

### Task 2: Move the name constants

- **File**: `src/screens/select_art.hpp`
- **Action**: UPDATE
- **Implement**:
  - `:67` `kDiffNameX = 15.0f` → `21.0f`.
  - Add on the next line: `inline constexpr float kDiffNameSelectedX = 25.0f;`
  - `:71` `kDiffNameBudget = 128.0f` → `118.0f`, and replace its trailing comment so it stays truthful, for example: `// ends at x 143 in the selected row (25 + 118), inside the 150px Edit tab`.
  - Extend the comment block at `:64-66` in the file's terse style, for example: `// The name sits 6px right of the mock, 10px in the selected row (#126): the baked tab's fill starts at x 12.5 at the name's cap top, at 16 in the selected row.`
  - Do **not** touch `kDiffMeterCentreX`, `kDiffTickX`, `kDiffBestRight`, `kEditTabWidth`, or anything in `src/render/theme.hpp`.
- **Mirror**: `src/screens/select_art.hpp:85-88` (`kWheelSongTextX` / `kWheelSelectedTextX`)
- **Validate**: `cmake --build build -j`

### Task 3: Use the selected offset at the draw site

- **File**: `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**: In the names loop of `draw_difficulty_rows` (`:588-591`), change the x argument from `L.x(row.x + kDiffNameX)` to `L.x(row.x + (selected ? kDiffNameSelectedX : kDiffNameX))`. `selected` is already in scope (`:583`). Leave the `truncate(..., L.px(kDiffNameBudget))` call, the meter loop (`:593-604`) and the best % loop (`:605-616`) as they are.
- **Mirror**: `src/screens/select_art.cpp:651-653`
- **Validate**: `cmake --build build -j`

### Task 4: Pin the margin in the tests

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Add `test_name_margin()` right after `test_meter_clearance()` (`:613-640`) and call it from `main` right after `test_meter_clearance();` (`:1160`).
     - Test-local constants with a provenance comment, in the style of `test_meter_clearance`:
       - `kTabLeftNormal = 12.5f`, `kTabLeftSelected = 16.0f`: the baked tab fill's left edge at the name's cap top (`diff_row_hard{,_selected}.png`; the tab is slanted, so this is where it cuts closest to the first letter; in the selected row the fill starts inside the gold frame).
       - `kMinGap = 8.0f`.
       - `kNameRight = 143.0f`: the name's right limit before #126 (15 + 128).
       - `kMinClear = 4.0f`.
     - Left margin (AC 1):
       - `TEST_CHECK(art::kDiffNameX - kTabLeftNormal >= kMinGap);`
       - `TEST_CHECK(art::kDiffNameSelectedX - kTabLeftSelected >= kMinGap);`
     - Selected gap at least the normal gap (AC 2):
       - `TEST_CHECK(art::kDiffNameSelectedX - kTabLeftSelected >= art::kDiffNameX - kTabLeftNormal);`
     - Right limit unchanged, so names stay inside the tab (AC 3):
       - `TEST_CHECK(art::kDiffNameX + art::kDiffNameBudget <= kNameRight);`
       - `TEST_CHECK(art::kDiffNameSelectedX + art::kDiffNameBudget <= kNameRight);`
       - The code-drawn Edit tab's right edge at the name's baseline (the baseline is under 8 px below the row's centre in both rows): `TEST_CHECK(kNameRight + kMinClear <= art::kEditTabWidth - theme::skew::kRows * 8.0f);`
     - Real fonts, with `blaze4k::TextRenderer& text = loaded_text();`:
       - For each pair `(theme::text::kDiffName, art::kDiffNameX)` and `(theme::text::kDiffNameSelected, art::kDiffNameSelectedX)`, and each of `"BEGINNER", "EASY", "MEDIUM", "HARD", "CHALLENGE", "EDIT"`: `TEST_CHECK(x + text.measure(name, style) <= kNameRight);`
       - Clear of the meter number (AC 3): for `meter` 1..20, `TEST_CHECK(art::kDiffMeterCentreX - text.measure(std::to_string(meter), theme::text::kDiffMeterSelected) * 0.5f >= kNameRight + kMinClear);`
     - Print `"  - name margin ok.\n"`.
     - Expected values: gaps 8.5 and 9.0; right limits 139 and 143; Edit edge 148.3; `CHALLENGE` selected ends at 132.2; the widest meter starts at 175.1. On `main` the test cannot pass: `kDiffNameSelectedX` does not exist, and `kDiffNameX - kTabLeftNormal` is 2.5.
  2. Add a line to the header comment block (`:1-15`): `// #126: the difficulty name sits 6px right (10px in the selected row); gap to the baked tab's left edge and the unchanged right limit.`
  3. Do **not** edit `test_labels()` (`:564-593`) or `tests/select_screen_test.cpp`. They read `kDiffNameBudget` and pass at 118.
- **Mirror**: `tests/select_art_test.cpp:613-640`
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/select_art_test`

### Task 5: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:49`: `- [ ]` → `- [x]`. Change only that line. Do **not** touch `.agents/issues/todo-issues.md`.
- **Validate**: `git diff --stat -- TODO.md` shows 1 line changed.

---

## Validation

```bash
# Build (existing Release build dir)
cmake --build build -j

# Lint: no linter is configured. Gate on zero new compiler warnings in the touched TUs:
touch src/screens/select_art.cpp tests/select_art_test.cpp
cmake --build build -j 2>&1 | grep -i "warning" | grep "select_art" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open the real audio device)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **51/51 pass** (baseline 51/51 on `01af2bf`; no executable is added).

Static checks:

```bash
# Only the name constants and one draw expression change in src/
git diff -- src | grep -E '^[+-][^+-]' | grep -vE '^[+-]\s*//'
#   expect: kDiffNameX, kDiffNameSelectedX (added), kDiffNameBudget, and the L.x(...) line in select_art.cpp
# The meter, ticks and best % did not move (AC 4)
git diff -- src | grep -E '^[+-].*(kDiffMeterCentreX|kDiffTickX|kDiffBestRight) =' ; echo "exit $? (expect 1)"
# Timing / judgment / input / assets untouched
git diff --stat -- src/timing src/audio src/input src/gameplay src/render assets   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-issues   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable).** `select_art_test` pins both left gaps, the selected-vs-normal comparison, the unchanged right limit, real-font fit of every standard name at its own x, and clearance from the meter number. Its existing `test_ticks` and `test_meter_clearance` still pin the tick x values and the meter position (AC 4), and `test_render_smoke` walks the full select draw path (normal, selected and Edit rows) at six window sizes.

2. **`/verify` screenshots (AC 5).** Use the `verify` skill with the owner's packs, after the final build. The wheel wraps, so `Up` from the first song reaches the end of the last pack.
   ```bash
   B=.claude/skills/verify/scripts/b4k.sh
   R=/tmp/blaze4k-verify/126-after
   cmake --build build -j
   $B doctor
   $B launch --run $R -- --songs "$PWD/songs"
   $B keys $R Return && $B wait-screen $R Select
   $B shot $R 01-challenge-selected            # Anubis: CHALLENGE selected; HARD, MEDIUM, EASY, BEGINNER unselected
   $B keys $R Right Right Right Right
   $B shot $R 02-beginner-selected             # BEGINNER selected; CHALLENGE unselected
   $B keys $R Up Up Up Up Up
   $B shot $R 03-edit-unselected               # Virtual Emotion: CHALLENGE selected; BlueChaos, Phrekwenci, Rynker Edit rows
   $B keys $R Right
   $B shot $R 04-edit-selected                 # BlueChaos (Edit) selected
   $B keys $R $(printf 'Up %.0s' $(seq 65))
   $B keys $R Right
   $B shot $R 05-edit-truncated                # Dawn: "M. Pover..." selected (see Open Question 1)
   $B stop $R
   ```
   Check the song title in each shot before citing it. If `songs/` is missing, use the fixture pack (`Down` once for `Blaze Anthem`) for shots 01 and 02 and report that the Edit shots were not taken.

   Expected, comparing `01-challenge-selected` with the Task 1 "before" shot:
   - Every name starts further right inside its tab: 6 px in unselected rows, 10 px in the selected row.
   - In the selected row the name no longer touches the gold frame. Its gap to the tab's left edge looks the same as or slightly larger than in the unselected rows.
   - `CHALLENGE` and `BEGINNER` end well inside the tab, in both the selected and the unselected state.
   - The meter number, the ticks and the best % are in the same place as in the "before" shot.
   - Edit names (shots 03 to 05) start at the same x as the other names and end inside the Edit tab.

   Optional numeric check, when the PNG is 1280x720 (window px = reference px). In `01-challenge-selected`, along the selected row's mid line (y 398) the tab fill starts at x ≈ 72 and the name's ink at x ≈ 84 (74 before). Along the next row's mid line (y 456) the fill starts at x ≈ 55 and the ink at x ≈ 66 (60 before). Allow ±1 px.

   Read every PNG you cite. Run `stop` even after a failed attempt.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| One shared offset cannot satisfy AC 2: the selected row is 3.4 px worse at any shared value | Two offsets (21 / 25), measured from the baked art. The new test compares the two gaps | In scope |
| The lower budget truncates a real Edit name that fits today (`M. Poveromo`, selected row only) | The issue's technical note asks for the budget to drop with the shift. The right limit is kept at 143 so nothing leaves the tab. Raised as Open Question 1 | In scope (owner call) |
| A longer standard-looking label from another pack (passthrough labels are arbitrary) is truncated sooner | Same `truncate` path as today, 10 px less room in the selected row. The widest label in the owner's packs, `CHALLENGE`, has 10.8 px to spare | In scope |
| Real-font widths drift if the fonts change | The test measures at runtime with the shipped fonts. Only the tab geometry is a test-local constant with provenance | In scope |
| The test-local tab edges (12.5 / 16.0) go stale if the row textures are re-baked | Same approach and same risk as `test_meter_clearance` (#122). The comment names the source PNGs | In scope |
| The name's x now differs from the mock and the manifest notes | Intentional, per the owner's request. The header comment records the deviation and #126. The manifest is not edited | In scope |
| The code-drawn Edit row is drawn 11–14 px left of the baked rows (tab 0..150 at mid against 11..161 / 14..164), so Edit names get a wider left gap (20.4 / 24.4 px) and the tightest right limit | Not changed here: the issue says only the name moves. Aligning the Edit row with the baked art would make the gaps equal and free about 10 px for names. Raised as Open Question 3 | Out of scope (flag to owner) |
| Root cause: the baked row textures place their art about 10 px right of the mock inside the content box, which also caused #122 | Not changed here. This plan and #122 compensate in the text and meter positions | Out of scope (flag to owner) |
| The `/verify` window cannot be captured (owner on another workspace) | Follow the skill: do not move windows, retry later, and report a missing shot rather than claiming AC 5 | In scope |
| Timing path touched (principle 1) | Only presentation constants and one draw x change. The `git diff --stat` check on timing, audio, input and gameplay is in Validation | In scope |

---

## Open Questions

None blocks the work. The plan uses the recommended default for each.

1. **`M. Poveromo` is truncated to `M. Pover...` while its row is selected.** It is the only label in the owner's packs that fits today and not after the change.
   - **Default (planned): accept it.** The issue's technical note expects the budget to drop by about the shift, and AC 3 asks only that truncated Edit names stay inside the tab.
   - Alternative A: leave Edit rows at x 15 with a 128 px budget. No name changes, and Edit rows already have a 14.4 px gap. But Edit names would then start 6 / 10 px left of the other rows' names, and AC 1 ("every difficulty row") is not met as written.
   - Alternative B: accept it now and file the follow-up in question 3, after which the budget can go back to 128 or more.

2. **How far to shift?** The issue says about 6–10 px.
   - **Default (planned): +6 in unselected rows, +10 in the selected row.** It is the only integer pair in that range where the selected gap is at least the unselected gap at the cap top, mid-height and baseline. Each value is a one-line change if the owner wants it tighter or looser after seeing the screenshots, but the selected offset should stay at least 3.4 px above the other to keep AC 2.

3. **Follow-up issue for the Edit row art?** The code-drawn Edit row sits 11–14 px left of the baked rows. Recommendation: file a separate issue to align it. It is a visible art change, so it does not belong in this one.

---

## Acceptance Criteria

- [ ] The difficulty name starts further right in every row: +6 px unselected (`kDiffNameX` 21), +10 px selected (`kDiffNameSelectedX` 25), both inside the issue's 6–10 px range
- [ ] The selected row's gap to the tab's slanted left edge is at least the unselected rows' gap (9.0 vs 8.5 by draw x, 10.0 vs 9.4 by ink; tested)
- [ ] `CHALLENGE`, `BEGINNER` and truncated Edit names end at or before x 143, inside the tab and at least 4 px clear of the meter number (tested with the real fonts)
- [ ] The meter number, ticks and best % have not moved (`kDiffMeterCentreX`, `kDiffTickX`, `kDiffBestRight` unchanged; existing tests pass)
- [ ] `/verify` song select screenshots show the new margin on a selected and an unselected row
- [ ] Build has no new warnings. Tests pass 51/51
- [ ] `TODO.md:49` ticked. `.agents/issues/todo-issues.md` untouched
