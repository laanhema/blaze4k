# Plan: Add right margin to the best % text in the song select difficulty rows

## Summary

In the song select difficulty rows the best % (`94.18%`, `100.00%`, or `---`) is drawn right-aligned at `row.x + kDiffBestRight` with `kDiffBestRight = 547` (`src/screens/select_art.hpp:73`, draw site `src/screens/select_art.cpp:634`). The value was measured from the mock, where the row's outline sits at x 563. The baked row textures put the outline further in: its inner edge is at x **553** at mid-height in an unselected row and at x **550** in the selected row (2 px gold frame). The outline is slanted, so it is closer still at the text's baseline. Today the text's right edge is 6.0 px (mid) / 4.5 px (baseline) from the outline in an unselected row and **3.0 px / 1.4 px** from the gold frame in the selected row. A screenshot of today's build confirms it: 7 px and 4 px.

This plan uses two offsets, the same shape the name got in #126 (`kDiffNameX` / `kDiffNameSelectedX`): `kDiffBestRight` 547 → **537** (−10) and a new `kDiffBestSelectedRight` = **533** (−14). The gaps become 16.0 / 14.5 px (unselected, mid / baseline) and 17.0 / 15.4 px (selected). One shared offset cannot work: the selected row is 3 px tighter at any shared value, and no single integer keeps both rows inside the issue's 14–18 px. A new pure helper, `difficulty_best_right(row, selected)`, picks the offset so a test can observe which row gets which (the lesson of #126's review). The widest best, `100.00%`, still starts 69.5 px after the last tick (81.2 px in unselected rows). The name, meter number, ticks and all row art are untouched. Tests: a new real-font check pins both gaps, the selected-vs-unselected comparison and the room after the last tick. This is presentation only: no clock, judgment or input code is touched.

## User Story

As a player browsing song select
I want my best % to have a visible right margin inside each difficulty row
So that the number reads cleanly and does not crowd the row's outline, least of all the gold frame of the row I have selected

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/screens/select_art.hpp` (1 constant changed, 1 added, 1 helper declared, comments), `src/screens/select_art.cpp` (1 new pure function, 1 draw expression), `tests/select_art_test.cpp`, `.claude/skills/verify/features/song-select.md` (one recipe step), `TODO.md` |
| GitHub Issue | #131 ([TODO-36] Add right margin to the best % text in the song select difficulty rows) |
| PRD Phase | N/A |
| Branch (suggested) | `feature/062-diff-best-right-margin` |
| Plan sequence | `062`. `061` was #130. `060` was used by #127 (branch `feature/060-stat-label-left-margin`) with no plan file in the repo |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and current. Build with `cmake --build build -j` |
| C++ compiler | GCC 16.2.1 | C++20, `-O3 -Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | Run on `main` @ `4dabf4b` with the sandboxed ctest command in Validation (3.16 s). The count stays **51**: no test executable is added |
| Sandbox requirement | n/a | Tests open the real sound device. **Always** run ctest and any test binary inside the `bwrap` prefix in Validation. Never change system audio state |
| Font measurements | scratch program linked against `build/libblaze4k_core.a` with `select_art_test`'s flags (`TextRenderer::measure` / `line_height` / `ascent` at 1280x720, headless, no GL), plus glyph ink bounds from the same TTFs with PIL/FreeType (advances agree with `measure` to 0.01 px) | Reference px. See "Measured geometry" |
| Texture measurements | `assets/theme/cabinet/diff_row_*.png` and `diff_row_*_selected.png` (PIL 12.3.0, content box from `manifest.json`) | All five colours agree. See "Measured geometry" |
| Mock measurement | `docs/cabinet-theme/reference/cabinet-v3-select.png` (1280x720, PIL) | See "Mock against the game" |
| Earlier screenshot | `/tmp/blaze4k-verify/130-after/evidence/01-edit-unselected.png` (1280x720, taken on today's `main`; #130 did not move the best %) | Shows today's gaps. `/tmp` may be cleared, so Task 1 takes fresh shots |
| `/verify` and best scores | `b4k.sh launch` gives each run an empty data dir, so every row shows `---` | A `RUN/data/scores.json` written **before** `launch` is loaded by the game (`launch` only does `mkdir -p RUN/data`; `src/main.cpp:203` loads `<data-dir>/scores.json`). The seed file in Task 1 was loaded headlessly with `load_high_scores`: 4 records, `100.00%`, `100.00%`, `88.88%`, `7.31%`. The seeded launch itself was not run (planning opens no windows). See Open Question 3 |
| Fixture chart keys | `make_chart_key` over `tests/fixtures/reference_pack` (scratch program) | `Blaze Anthem`: Challenge `f6aa45043607306c`, Hard `96dd02b27954ac3b`, Medium `1a180a10c0d25c92`, Easy `221ef40c4dab1693`, Beginner `c2b4b1ee2666e104`. The key hashes the simfile name, title, artist and notes, not the path |
| Edit charts for `/verify` | `songs/` (owner's packs, gitignored): `Virtual Emotion` has Edit rows `BlueChaos`, `Phrekwenci`, `Rynker` | Reached with `Up` x5 from the first song. The fixture pack has no Edit chart |
| `TODO.md` | gitignored (`.gitignore:27`) | Ticked locally only, as in earlier plans |
| Off-limits file | `.agents/issues/todo-issues.md` | Unrelated, uncommitted owner edit. Do **not** stage, revert, edit or commit it |

### Forward references to #131

`grep -rn "#131\|TODO-36"` over `src/`, `tests/`, `docs/`, `scripts/`, `.claude/`, `.agents/plans`, `.agents/reports`, `.agents/reviews` finds only `TODO.md:52` (the source line, already tagged `(#131)`, which Task 5 ticks). There are no placeholders in code.

Two earlier documents shape this work without the number:

- `.agents/reviews/feature-059-diff-name-left-margin-review.md:25` (finding L1 of #126): choosing an offset by row state inside the GL draw call cannot be tested, so the choice was moved into the pure helper `difficulty_name_x`. This plan starts with the helper.
- `.agents/plans/completed/059-diff-name-left-margin-plan.md:366` (Risk row): the baked row textures place their art about 10 px inside the mock's position. That is the cause here too.

### Measured geometry (reference px, relative to the row's content x and y)

**Best % line box** (centred in the row by `centred_top`, `src/screens/select_art.cpp:60-64`):

| Row | Style | line height | ascent | cap height | cap top y | mid y | baseline y |
|---|---|---|---|---|---|---|---|
| unselected (44 tall) | `kDiffBest`, SairaBold 20 px | 31.48 | 22.70 | 13.94 | 15.0 | 22 | 28.96 |
| selected (52 tall) | `kDiffBestSelected`, SairaExtraBold 22 px | 34.63 | 24.97 | 15.31 | 18.3 | 26 | 33.66 |

**Text widths and ink:**

| Text | `kDiffBest` width | `kDiffBestSelected` width |
|---|---|---|
| `100.00%` (widest: the only five-digit value `format_percent` returns) | 63.82 | 71.48 |
| `88.88%` (widest four-digit value) | 58.42 | 65.34 |
| `0.00%` | 48.08 | 53.92 |
| `---` | 17.82 | 19.67 |

The ink ends 0.9 px left of the draw x in both styles (right side bearing: `%` 0.87 / 0.90, `-` 0.94 / 0.98). The rightmost ink of `%` is its lower ring, between the baseline and about 6 px above it. The hyphens of `---` sit 4.6–6.9 px above the baseline (4.8–7.7 selected), just below mid-height.

**Baked row, right side** (`diff_row_hard.png`, `diff_row_hard_selected.png`; beginner, easy, medium and challenge checked and identical):

| Row | at | outline inner edge | art outer edge | outline |
|---|---|---|---|---|
| unselected | cap top (y 15.0) | 554.5 | 555.5 | 1 px `#27325A` ring |
| unselected | mid (y 22) | **553.0** | 554.0 | |
| unselected | baseline (y 28.96) | **551.5** | 552.5 | |
| selected | cap top (y 18.3) | 551.5 | 553.5 | 2 px gold ring (`#FFD633`), glow outside it |
| selected | mid (y 26) | **550.0** | 552.0 | |
| selected | baseline (y 33.66) | **548.5** | 550.5 | |

The same edges come out of the code: `edit_row_rects(row, selected).inner` ends at 553 (564 − `kEditInsetX` 10 − `kEditBorder` 1) and 550 (564 − 12 − 2), and `skewed_quad(..., theme::skew::kRows)` moves the edge 0.213 px left per px below the row's centre: 551.52 and 548.37 at the two baselines. `test_edit_row_geometry` (`tests/select_art_test.cpp:820-977`) holds these rects to all ten PNGs within 1 px (worst frame error 0.34 px, `.agents/reports/061-edit-diff-row-match-baked-plan-report.md`). So the code-drawn Edit row has the same outline, and the new test can take the outline from `edit_row_rects` with no hard-coded art numbers.

**Gap from the text's right edge (the draw x) to the outline's inner edge** (computed with the game's own helpers in a scratch program; the ink gap is 0.9 px larger):

| Row | at | today (547 / 547) | shared 537 | 537 / 534 | **plan (537 / 533)** |
|---|---|---|---|---|---|
| unselected | mid | 6.0 | 16.0 | 16.0 | **16.0** |
| unselected | baseline | 4.5 | 14.5 | 14.5 | **14.5** |
| selected | mid | **3.0** | 13.0 | 16.0 | **17.0** |
| selected | baseline | **1.4** | 11.4 | 14.4 | **15.4** |

- With one shared offset the selected gap is 3 px smaller at mid-height and 3.15 px smaller at the baseline (the selected text's baseline is 7.66 px below the row's centre against 6.96 px). 14–18 px at both heights needs 535–537 in an unselected row and 532–534 in the selected row, so no shared integer fits. AC 1 and AC 2 need two offsets.
- 537 / 533 is the pair with the smallest move where both rows are inside 14–18 px at mid-height and at the baseline **and** the selected gap is at least the unselected gap at both heights. 537 / 534 gives equal gaps at mid-height but leaves the selected row 0.15 px short at the baseline. See Open Question 1.
- At the cap top the gaps are 17.5 and 18.6 px, but no best % has ink at its right edge there (the `%` reaches its right edge only in the lower ring), so the cap top is not pinned.

**Room after the last tick** (the tick box ends at x 392: `kDiffTickX` 210 + 9 × 18 + `kTickWidth` 20):

| Row | `100.00%` starts today | `100.00%` starts, plan | room after the last tick, plan |
|---|---|---|---|
| unselected | 483.2 | 473.2 | **81.2** (91.2 today) |
| selected | 475.5 | 461.5 | **69.5** (83.5 today) |

### Today's gap in the running game (evidence)

`/tmp/blaze4k-verify/130-after/evidence/01-edit-unselected.png` (1280x720, `Virtual Emotion`, every best is `---`), measured with the script in End-to-End Verification:

| Row | Scanline | Text ink | Outline starts | Gap | Model (this plan's numbers) |
|---|---|---|---|---|---|
| `CHALLENGE`, selected (row x 58, y 372..424) | y 398 | 586..604 | gold at 608 | **4 px** | ink ends 58 + 547 − 0.98 = 604.0; frame 58 + 550 = 608 |
| `BlueChaos` (Edit), unselected (row x 44, y 434..478) | y 457 | 574..590 | ring at 597 | **7 px** | ink ends 44 + 547 − 0.94 = 590.1; ring 44 + 553 = 597 |

The model matches the game to the pixel. A `%` ends lower, where the slanted outline is closer: about 2–3 px (selected) and 5–6 px (unselected) today.

### Issue-stated numbers, re-measured

| Issue note | Re-measured | Verdict |
|---|---|---|
| `kDiffBestRight = 547` in `select_art.hpp`; drawn right-aligned in the "bests" loop at `row.x + kDiffBestRight`; `kDiffBest` 20 px, `kDiffBestSelected` 22 px gold | `select_art.hpp:73`; `select_art.cpp:625-636` (the x at `:634`); `theme.hpp:137,140` (SairaBold 20 steel, SairaExtraBold 22 gold) | Right |
| "17px in from the right edge" means the edge of the 564 px row rect | 564 − 547 = 17. In the mock the outline's inner edge is at 563 at mid-height | Right |
| Unselected: the baked body ends at x 553.5 at mid-height (6.5 px gap) | The 1 px ring spans 553.0..554.0. Its inner edge is 553: 6.0 px from the draw x | 553.5 is the middle of the ring. The visible gap ends at 553 |
| Selected: 551.5 at mid-height (4.5 px gap to the gold frame) | The 2 px gold ring spans 550.0..552.0. Its inner edge is 550: **3.0 px** from the draw x | The gold frame starts 1.5 px earlier than stated. The selected row is 3 px tighter than the others, not 2 |
| The right edge is slanted, from 558 at the top to 549 at the bottom of an unselected row | Outer edge 558.5 on the first row, 549.5 on the last | Right within 0.5 px (as #130 found) |
| "A value of about 537–541 gives a 14–18 px gap in unselected rows" | With the inner edge at 553: 537 → 16, 541 → 12 at mid-height (14.5 and 10.5 at the baseline). 14–18 at both heights needs 535–537 | Only the low end of the range reaches 14 px. 537 is inside it |
| The last tick ends at x 392 | 392 (`tick_rect`, `select_art.cpp:224-228`) | Right |
| `tests/select_art_test.cpp` (~line 611) requires `tick_rect(row, false, 9).x + 20 < row.x + kDiffBestRight - 60`; it holds at 537 | The line is `:615`. At 537: 436 < 521 | Right. The check stays as it is |
| The selected row "may need its own offset, as the name got in #126 (`kDiffNameSelectedX`)" | It does (table above). #126's review also moved the choice into a pure helper, `difficulty_name_x` (`select_art.cpp:230-232`) | Right. This plan mirrors both |
| #130 "moves the Edit row's right edge in to match the baked rows. Either order works" | #130 is merged (`4dabf4b`). The Edit row's ring is now the baked ring | The gap is the same in baked and Edit rows |
| Not in the issue | A fresh `/verify` run shows `---` in every row, so a screenshot cannot show a `%` (AC 3, AC 5) without saved scores | Task 1 and End-to-End Verification seed `RUN/data/scores.json` |

### Mock against the game

`cabinet-v3-select.png` has `HARD` selected (slot 3). Row-relative x; the mock's best % is drawn italic.

| Quantity | Mock | Game today | Plan |
|---|---|---|---|
| Outline inner edge at mid-height, unselected / selected | 563 / 563 | 553 / 550 | 553 / 550 (art unchanged) |
| Best % ink right edge, unselected / selected | 547 / 546 | 546.1 / 546.1 | 536.1 / 532.1 |
| Tightest ink-to-outline gap (in the `%`'s lower ring), unselected / selected | **15 / 15** | 5.4 / 2.3 | **15.4 / 16.3** |
| Ink-to-outline gap at the mid-height scanline, unselected / selected | 20 / 21 | not comparable (the mock's text is italic) | not comparable |

The baked textures put the outline 10 px (unselected) and 13 px (selected) left of the mock's, while `kDiffBestRight` stayed at the mock's x. Moving the text by 10 px restores the mock's 15 px in unselected rows. The selected row gets 14 px, 1 px more than the mock's gap, so that AC 2 holds at every height (Open Question 1).

---

## Patterns to Follow

### Naming: one offset per row state
```cpp
// SOURCE: src/screens/select_art.hpp:64-73
// Inside a difficulty row, from the row's content x (measured from the mock; the
// manifest notes say 24 / 183 / 216). The meter and ticks sit 16px right of the
// mock (#122): the slanted tab reaches x 167.5 at the cap top of a selected row.
// The name sits 6px right of the mock, 10px in the selected row (#126): the baked
// tab's fill starts at x 12.5 at the name's cap top, at 16 in the selected row.
inline constexpr float kDiffNameX = 21.0f;
inline constexpr float kDiffNameSelectedX = 25.0f;
inline constexpr float kDiffMeterCentreX = 190.0f;
inline constexpr float kDiffTickX = 210.0f;
inline constexpr float kDiffBestRight = 547.0f; // measured from mock: 17px in from the right edge
```
The `Selected` word goes before the unit: `kDiffNameSelectedX`, `kTickSelectedHeight`, `kEditSelectedInsetX`. So the new constant is `kDiffBestSelectedRight`.

### Pure layout helper that picks the offset (GL-free, so the test pins it headless)
```cpp
// SOURCE: src/screens/select_art.hpp:225-226
// Left edge of the difficulty name in `row`: row.x + 21 (25 selected).
[[nodiscard]] float difficulty_name_x(const Rect& row, bool selected);

// SOURCE: src/screens/select_art.cpp:230-232
float difficulty_name_x(const Rect& row, bool selected) {
    return row.x + (selected ? kDiffNameSelectedX : kDiffNameX);
}
```

### The draw site being changed
```cpp
// SOURCE: src/screens/select_art.cpp:625-636
for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].chart == nullptr) {
        continue;
    }
    const int slot = static_cast<int>(i);
    const bool selected = slot == selected_slot;
    const Rect row = difficulty_row_rect(slot, selected_slot);
    const theme::TextStyle& style =
        selected ? theme::text::kDiffBestSelected : theme::text::kDiffBest;
    text->draw(renderer, rows[i].best, L.x(row.x + kDiffBestRight),
               centred_top(*text, L, row.y, row.h, style), style, TextAlign::Right);
}
```
The names loop above it already calls its helper: `L.x(difficulty_name_x(row, selected))` (`:610`).

### Error handling
Not applicable. These are constexpr layout values and a pure function. Draw helpers already no-op for null services (`src/screens/select_art.hpp:9-10`).

### Tests (plain executable, `TEST_CHECK` abort macro, section print, real fonts via `loaded_text()`)
```cpp
// SOURCE: tests/select_art_test.cpp:651-672
void test_name_margin() {
    ...
    // The draw site's x: each row state gets its own offset (row x 44, 58 selected).
    const Rect row = art::difficulty_row_rect(0, 3);
    const Rect sel = art::difficulty_row_rect(3, 3);
    TEST_CHECK(art::difficulty_name_x(row, false) == 65.0f);
    TEST_CHECK(art::difficulty_name_x(sel, true) == 83.0f);
    TEST_CHECK(art::difficulty_name_x(row, false) - row.x == art::kDiffNameX);
    TEST_CHECK(art::difficulty_name_x(sel, true) - sel.x == art::kDiffNameSelectedX);
```
```cpp
// SOURCE: tests/select_art_test.cpp:812-818 (defined after test_name_margin, before test_edit_row_geometry)
// x of the left / right slanted side of skewed_quad(r, kRows) at reference y.
float side_x(const Rect& r, bool right, float y) {
    const auto q = art::skewed_quad(r, theme::skew::kRows);
    const Vec2 a = right ? q[1] : q[0];
    const Vec2 b = right ? q[2] : q[3];
    return a.x + (b.x - a.x) * ((y - a.y) / (b.y - a.y));
}
```
A measured value may be printed for the log, as `test_hint_layout` does (`:778`: `std::cout << "    hint line " << ... << " (mock 308..970)\n";`).

The existing check that must keep passing unchanged: `tests/select_art_test.cpp:614-615` (`tick_rect(row, false, 9).x + 20 < row.x + art::kDiffBestRight - 60`; 436 < 521 at 537).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | UPDATE | `kDiffBestRight` 547 → 537, new `kDiffBestSelectedRight = 533`, comments, declare `difficulty_best_right` |
| `src/screens/select_art.cpp` | UPDATE | Define `difficulty_best_right`; the bests loop calls it (`:634`) |
| `tests/select_art_test.cpp` | UPDATE | New `test_best_margin()` (both gaps, AC 2, room after the last tick, real fonts), header comment line |
| `.claude/skills/verify/features/song-select.md` | UPDATE | One driving step: how to show best scores with a seeded `scores.json` (only if it worked in the End-to-End run) |
| `TODO.md` | UPDATE | Tick line 52 (`(#131)`); local only, the file is gitignored |

No file is created. `src/render/theme.hpp`, `assets/`, `tests/select_screen_test.cpp` and `tests/CMakeLists.txt` are not changed.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Capture the "before" screenshots

- **File**: none (evidence only, in `/tmp/blaze4k-verify/131-before/evidence/`)
- **Action**: RUN, before editing any source file
- **Implement**: Follow the `verify` skill (`.claude/skills/verify/SKILL.md`, recipe `.claude/skills/verify/features/song-select.md`) with the unmodified build and the fixture pack. Seed the run's own data dir with four best scores **before** `launch`, so the rows show percentages (the keys are the fixture's `Blaze Anthem` charts):
  ```bash
  B=.claude/skills/verify/scripts/b4k.sh
  R=/tmp/blaze4k-verify/131-before
  $B doctor
  mkdir -p $R/data
  cat > $R/data/scores.json <<'EOF'
  {
    "scores": {
      "f6aa45043607306c": {"dp": 1000, "grade": "quad_star", "percent": 1.0, "timestamp": 1791000000},
      "96dd02b27954ac3b": {"dp": 800, "grade": "quad_star", "percent": 1.0, "timestamp": 1791000000},
      "1a180a10c0d25c92": {"dp": 444, "grade": "S-", "percent": 0.8888, "timestamp": 1791000000},
      "221ef40c4dab1693": {"dp": 22, "grade": "D", "percent": 0.0731, "timestamp": 1791000000}
    },
    "version": 1
  }
  EOF
  $B launch --run $R
  $B keys $R Return && $B wait-screen $R Select
  sleep 1.5
  $B keys $R Down
  $B shot $R 01-challenge-selected   # Blaze Anthem: CHALLENGE 100.00% selected (gold); HARD 100.00%, MEDIUM 88.88%, EASY 7.31%, BEGINNER ---
  $B stop $R
  grep '\[Scores\]' $R/evidence/game.log   # expect: [Scores] Loaded '.../131-before/data/scores.json'
  ```
  Read the PNG and check the song title and the five best values before citing it. Then run the measurement script from End-to-End Verification on it and keep its two output lines.
  - If the rows show `---` although the file was seeded, read `game.log` for a `[Scores]` warning, keep the shot (it still shows today's gap, as in "Today's gap in the running game"), and report it. Do not hunt for another way to fake scores.
  - If `$R` already holds a live instance, pick another run name. Do not delete another run's directory.
  - If `shot` fails because the window is not viewable, follow the skill: do not move the owner's windows, report the missing "before" shot, and continue. AC 5 needs only the "after" shots.
- **Mirror**: `.claude/skills/verify/features/song-select.md`
- **Validate**: `ls /tmp/blaze4k-verify/131-before/evidence/01-challenge-selected.png`

### Task 2: Move the best % constants and declare the helper

- **File**: `src/screens/select_art.hpp`
- **Action**: UPDATE
- **Implement**:
  - `:73` `kDiffBestRight = 547.0f` → `537.0f`, and remove its trailing comment (`// measured from mock: 17px in from the right edge`), which is no longer true.
  - Add on the next line: `inline constexpr float kDiffBestSelectedRight = 533.0f;`
  - Extend the comment block at `:64-68` in the file's terse style, so it says the value now departs from the mock. For example:
    ```cpp
    // The best % ends 10px left of the mock, 14px in the selected row (#131): the baked
    // outline's inner edge is at x 553 at mid-height, 550 in the selected row (the mock's
    // is at 563), and it slants 1.5px closer at the text's baseline.
    ```
  - After the `difficulty_name_x` declaration (`:225-226`), add:
    ```cpp
    // Right edge of the best % in `row`: row.x + 537 (533 selected).
    [[nodiscard]] float difficulty_best_right(const Rect& row, bool selected);
    ```
  - Do **not** touch `kDiffRowWidth`, `kDiffNameX`, `kDiffNameSelectedX`, `kDiffMeterCentreX`, `kDiffTickX`, `kDiffNameBudget`, the `kEdit*` constants, or anything in `src/render/theme.hpp`.
- **Mirror**: `src/screens/select_art.hpp:69-70` and `:225-226`
- **Validate**: `cmake --build build -j` (it links: nothing calls the helper before Task 3)

### Task 3: Define the helper and use it at the draw site

- **File**: `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Right after `difficulty_name_x` (`:230-232`), add:
    ```cpp
    float difficulty_best_right(const Rect& row, bool selected) {
        return row.x + (selected ? kDiffBestSelectedRight : kDiffBestRight);
    }
    ```
  - In the bests loop of `draw_difficulty_rows` (`:625-636`), change the x argument at `:634` from `L.x(row.x + kDiffBestRight)` to `L.x(difficulty_best_right(row, selected))`. `selected` and `row` are already in scope (`:630-631`). Leave the style choice, `centred_top(...)` and `TextAlign::Right` as they are.
  - Leave the names loop (`:598-612`), the meter loop (`:613-624`), `draw_ticks` and `draw_difficulty_row_art` as they are.
- **Mirror**: `src/screens/select_art.cpp:230-232` and `:610`
- **Validate**: `cmake --build build -j`

### Task 4: Pin the margin in the tests

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Add `test_best_margin()` right after `test_edit_row_geometry()` (it ends at `:977`; the new test needs `side_x`, `:812-818`) and call it from `main` right after `test_edit_row_geometry();` (`:1392`).
     ```cpp
     void test_best_margin() {
         // #131: the best % ends 14-18px inside the row's right outline. The outline is
         // edit_row_rects' ring (test_edit_row_geometry holds it to all ten baked PNGs): its
         // inner edge is `inner`'s right side, slanted by kRows, so it is closest to the
         // text at the baseline.
         constexpr float kMinGap = 14.0f;
         constexpr float kMaxGap = 18.0f;
         // Room kept between the last tick and the widest best: three tick pitches.
         constexpr float kMinTickGap = 3.0f * theme::layout::kDiffTickPitch;

         // The draw site's x: each row state gets its own offset (row x 44, 58 selected).
         const Rect row = art::difficulty_row_rect(0, 3);
         const Rect sel = art::difficulty_row_rect(3, 3);
         TEST_CHECK(art::difficulty_best_right(row, false) == 581.0f);
         TEST_CHECK(art::difficulty_best_right(sel, true) == 591.0f);
         TEST_CHECK(art::difficulty_best_right(row, false) - row.x == art::kDiffBestRight);
         TEST_CHECK(art::difficulty_best_right(sel, true) - sel.x == art::kDiffBestSelectedRight);

         blaze4k::TextRenderer& text = loaded_text();
         struct Case {
             Rect r;
             bool selected;
             theme::TextStyle style;
         };
         const Case cases[2] = {{row, false, theme::text::kDiffBest},
                                {sel, true, theme::text::kDiffBestSelected}};
         float gap_mid[2] = {};
         float gap_base[2] = {};
         for (std::size_t i = 0; i < 2; ++i) {
             const Case& c = cases[i];
             const Rect inner = art::edit_row_rects(c.r, c.selected).inner;
             const float right = art::difficulty_best_right(c.r, c.selected);
             const float mid_y = c.r.y + c.r.h * 0.5f;
             // The line is centred in the row (centred_top in select_art.cpp).
             const float base_y =
                 c.r.y + (c.r.h - text.line_height(c.style)) * 0.5f + text.ascent(c.style);
             TEST_CHECK(base_y > mid_y && base_y < c.r.y + c.r.h);
             gap_mid[i] = side_x(inner, true, mid_y) - right;
             gap_base[i] = side_x(inner, true, base_y) - right;
             TEST_CHECK(gap_mid[i] >= kMinGap && gap_mid[i] <= kMaxGap);
             TEST_CHECK(gap_base[i] >= kMinGap && gap_base[i] <= kMaxGap);
             // The widest bests start well after the last tick.
             const float tick_end = art::tick_rect(c.r, c.selected, 9).x + art::kTickWidth;
             for (const char* best : {"100.00%", "88.88%", "---"}) {
                 TEST_CHECK(right - text.measure(best, c.style) >= tick_end + kMinTickGap);
             }
         }
         // The selected row's gap to the gold frame is at least the other rows' gap.
         TEST_CHECK(gap_mid[1] >= gap_mid[0]);
         TEST_CHECK(gap_base[1] >= gap_base[0]);
         std::cout << "    best % gap " << gap_mid[0] << " / " << gap_mid[1] << " (mid), "
                   << gap_base[0] << " / " << gap_base[1] << " (baseline)\n";
         std::cout << "  - best % margin ok.\n";
     }
     ```
     Adapt names and formatting to the file, but keep every check.
     - Expected values: gaps **16 / 17** at mid-height and **14.5175 / 15.3693** at the baseline; `100.00%` leaves 81.2 px (unselected) and 69.5 px (selected) after the last tick, against the 54 px floor. This body was dry-run in a scratch program against the current library, with stand-ins for the two constants and the helper: it compiles with `-Wall -Wextra -Wpedantic` and no warning, and passes at 537 / 533.
     - On `main` the test cannot pass: `difficulty_best_right` and `kDiffBestSelectedRight` do not exist, and with 547 in both rows the gaps are 6 / 3 (mid) and 4.5 / 1.4 (baseline).
     - Sensitivity (same dry run): 547 / 547 fails six checks. A shared 537 fails four (selected gaps 13 / 11.4). 538 / 533 fails `kMinGap` at the unselected baseline (13.5). 537 / 534 fails the baseline comparison (14.37 against 14.52). 1 px further in (536 / 532, 537 / 532) still passes the gap checks, so the `== 581.0f` / `== 591.0f` checks are what pin the exact values; they also fail if the helper's two branches are swapped.
  2. Add a line to the header comment block (`:1-19`): `// #131: the best % ends 10px further left (14px in the selected row); gap to the row's right outline in both row states and room after the last tick.`
  3. Do **not** edit `test_ticks()` (`:601-617`). Its `kDiffBestRight - 60` check passes at 537.
- **Mirror**: `tests/select_art_test.cpp:651-699` (`test_name_margin`) and `:812-818` (`side_x`)
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/select_art_test`

### Task 5: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:52`: `- [ ]` → `- [x]`. Change only that line. The file is gitignored, so this is a local edit. Do **not** touch `.agents/issues/todo-issues.md`.
- **Validate**: `sed -n 52p TODO.md` starts with `- [x]`.

### Task 6: Record the seeded-scores step in the verify recipe

- **File**: `.claude/skills/verify/features/song-select.md`
- **Action**: UPDATE, after the End-to-End run. **Skip it** (and report that) if the seeded launch did not show the percentages.
- **Implement**: The recipe lists the `chart-score` sub-feature but has no step that shows a best %. Add one bullet to `## Driving it with b4k.sh`, after "Pick a chart", in the file's style (bold lead, commands, what the screen shows). It must say:
  - write `RUN/data/scores.json` before `$B launch` (`mkdir -p RUN/data` first; `launch` keeps an existing data dir);
  - the four `Blaze Anthem` keys and percents from Task 1 (Challenge `f6aa45043607306c` and Hard `96dd02b27954ac3b` at `1.0`, Medium `1a180a10c0d25c92` at `0.8888`, Easy `221ef40c4dab1693` at `0.0731`; each record needs `dp`, `grade`, `percent`, `timestamp`);
  - what the screen then shows on `Blaze Anthem`: `100.00%`, `100.00%`, `88.88%`, `7.31%`, `---`, and that `game.log` has `[Scores] Loaded`.

  Add one line to `## Gotchas`: a fresh run has no scores, so every row shows `---`; the keys are content hashes of the fixture charts and change if a fixture chart's notes change.
  Keep the file's four H2 sections and their order (`features/README.md`, "Feature entry contract"). Do not edit `SKILL.md` or `b4k.sh`.
- **Mirror**: `.claude/skills/verify/features/song-select.md` (existing bullets); commit `c3bd902` made the same kind of edit for #130
- **Validate**: `git diff --stat -- .claude/skills/verify` shows only `features/song-select.md`

---

## Validation

```bash
# Build (existing Release build dir)
cmake --build build -j

# Lint: no linter is configured. Gate on zero new compiler warnings in the touched TUs:
touch src/screens/select_art.cpp tests/select_art_test.cpp
cmake --build build -j 2>&1 | grep -i "warning" | grep "select_art" || echo "no new warnings"

# Tests (MUST run sandboxed: tests open the real audio device)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# The new section, run directly (same sandbox prefix)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/select_art_test | grep -A1 "best % gap"
#   expect: "    best % gap 16 / 17 (mid), 14.5175 / 15.3693 (baseline)" then "  - best % margin ok."
```

Expected: **51/51 pass** (baseline 51/51 on `4dabf4b`; no executable is added).

Static checks:

```bash
# Only the best % constants, the helper and one draw expression change in src/
git diff -- src | grep -E '^[+-][^+-]' | grep -vE '^[+-]\s*//'
#   expect: kDiffBestRight, kDiffBestSelectedRight (added), the difficulty_best_right declaration and definition,
#   and the L.x(...) line in select_art.cpp
# The name, meter and ticks did not move (AC 4)
git diff -- src | grep -E '^[+-].*(kDiffRowWidth|kDiffNameX|kDiffNameSelectedX|kDiffMeterCentreX|kDiffTickX|kDiffNameBudget|kTickWidth) =' ; echo "exit $? (expect 1)"
# Only select_art changed in src/; timing / judgment / input / assets untouched
git diff --stat -- src                                                          # expect select_art.cpp and select_art.hpp only
git diff --stat -- src/timing src/audio src/input src/gameplay src/render assets tests/CMakeLists.txt   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-issues   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable).** `select_art_test`'s new `test_best_margin` pins the draw x of both row states, the 14–18 px gap to the outline at mid-height and at the baseline, the selected-vs-unselected comparison (AC 2) and the room between the last tick and `100.00%` with the real fonts (AC 3). `test_ticks`, `test_meter_clearance` and `test_name_margin` still pin the tick, meter and name positions (AC 4). `test_edit_row_geometry` keeps the outline used by the new test tied to the baked PNGs, and `test_render_smoke` walks the full select draw path (unselected, selected and Edit rows) at six window sizes.

2. **`/verify` screenshots with best scores (AC 5, AC 3).** Use the `verify` skill with the fixture pack and the seed file from Task 1, after the final build.
   ```bash
   B=.claude/skills/verify/scripts/b4k.sh
   R=/tmp/blaze4k-verify/131-after
   cmake --build build -j
   $B doctor
   mkdir -p $R/data
   cp /tmp/blaze4k-verify/131-before/evidence/scores.json $R/data/scores.json   # or the Task 1 heredoc again
   $B launch --run $R
   $B keys $R Return && $B wait-screen $R Select
   sleep 1.5
   $B keys $R Down
   $B shot $R 01-challenge-selected   # Blaze Anthem: CHALLENGE 100.00% selected; HARD 100.00%, MEDIUM 88.88%, EASY 7.31%, BEGINNER --- unselected
   $B keys $R Right
   $B shot $R 02-hard-selected        # HARD 100.00% selected; CHALLENGE 100.00% unselected above it
   $B stop $R
   grep '\[Scores\]' $R/evidence/game.log
   ```
   Check the song title and the best values in each shot before citing it. The run uses the fixture pack, so say so in the report (stub clock; no timing claim is made).

   Expected, comparing `01-challenge-selected` with the Task 1 "before" shot:
   - Every best % ends further left: 10 px in the unselected rows, 14 px in the selected row.
   - In the selected row the gold `100.00%` no longer sits against the gold frame. Its gap looks the same as or slightly larger than the gaps in the rows below.
   - `100.00%` is well clear of the last tick in both the selected and an unselected row.
   - The difficulty names, meter numbers and ticks are in the same place as in the "before" shot.

   Numeric check for `01-challenge-selected` (1280x720, so window px = reference px; allow ±1 px). Save this script outside the repo (for example `/tmp/blaze4k-verify/best_gap.py`) and run `python3 best_gap.py SHOT.png` on the "before" and the "after" shot. It was dry-run on today's `---` shot (see "Today's gap in the running game": 4 px and 7 px).
   ```python
   # Usage: python3 best_gap.py SHOT.png   (1280x720 shot, selected row in slot 0)
   import sys
   from PIL import Image
   im = Image.open(sys.argv[1]).convert("RGB")
   assert im.size == (1280, 720), im.size
   gold = lambda p: p[0] > 0x90 and p[1] > 0x70 and p[2] < 0x60
   steel = lambda p: p[0] > 0x80 and p[1] > 0x88 and p[2] > 0xA0
   ring = lambda p: 0x1C < p[0] < 0x40 and p[2] > 0x48
   def runs(y, hit, x0=470, x1=626):
       out, start = [], None
       for x in range(x0, x1):
           on = hit(im.getpixel((x, y)))
           if on and start is None: start = x
           if not on and start is not None: out.append((start, x)); start = None
       return out
   # (name, row top, row height, ink test, outline test)
   for name, top, h, ink, edge in (("selected", 372, 52, gold, gold), ("unselected", 434, 44, steel, ring)):
       best = None
       for y in range(top + 14, top + h - 9):          # the text band: cap top .. baseline
           ink_runs = runs(y, ink)
           if edge is ink:                              # gold text and gold frame: the frame is the last run
               if len(ink_runs) < 2 or ink_runs[-1][0] < 600: continue
               frame, text_end, text_start = ink_runs[-1][0], ink_runs[-2][1], ink_runs[0][0]
           else:
               edges = runs(y, edge, 590, 626)
               if not ink_runs or not edges: continue
               frame, text_end, text_start = edges[0][0], ink_runs[-1][1], ink_runs[0][0]
           if best is None or frame - text_end < best[0]:
               best = (frame - text_end, y, text_start, text_end, frame)
       print(name, "no text found" if best is None else
             "tightest gap %d px at y %d (text %d..%d, outline at %d)" % best)
   ```

   | Row in `01-challenge-selected` | Quantity | Before | After |
   |---|---|---|---|
   | `CHALLENGE`, selected (row x 58, y 372..424) | `100.00%` ink, left..right (widest over the band) | 534..604 | **520..590** |
   | | gold frame's inner edge at mid-height (y 398) | 608 | 608 |
   | | tightest gap (script) | 2–4 | **16–18** |
   | | last tick box ends at | 450 | 450 (70 px before the text) |
   | `HARD`, unselected (row x 44, y 434..478) | `100.00%` ink, left..right | 528..590 | **518..580** |
   | | ring at mid-height (y 456) | 597 | 597 |
   | | tightest gap (script) | 5–7 | **15–17** |
   | | last tick box ends at | 436 | 436 (82 px before the text) |

   Pass: both "after" gaps are inside 14–18 px, and the selected gap is not smaller than the unselected gap. The script prints one scanline's text span, so read the left end of `100.00%` from the PNG if it is needed.

3. **Edit rows (only if `songs/` exists).** The Edit row's outline is code-drawn (#130), so check one against a baked row. No seed file: every best is `---`.
   ```bash
   R=/tmp/blaze4k-verify/131-after-edit
   $B launch --run $R -- --songs "$PWD/songs"
   $B keys $R Return && $B wait-screen $R Select
   sleep 1.5
   $B keys $R Up Up Up Up Up
   $B shot $R 03-edit-unselected      # Virtual Emotion: CHALLENGE selected; BlueChaos (Edit), HARD, Phrekwenci, Rynker unselected
   $B stop $R
   ```
   Check the song title (keys can be dropped right after the Select transition; see the recipe's Gotchas). Expected from `best_gap.py`, against today's measured values: selected `CHALLENGE` `---` text 586..604 → **572..590**, gap 4 → **18**; unselected `BlueChaos` (Edit) text 574..590 → **564..580**, gap 7 → **17**. The `---` of `HARD` below it ends at the same x as `BlueChaos`'s. If `songs/` is missing, report that this shot was not taken; AC 5 does not depend on it.

   Read every PNG you cite. Run `stop` even after a failed attempt.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| One shared offset cannot meet AC 1 and AC 2: the selected row is 3 px tighter at any shared value | Two offsets (537 / 533), measured from the baked art and the code's own ring geometry. The new test compares the two gaps at two heights | In scope |
| `100.00%` moves toward the ticks | It still starts 69.5 px (selected) / 81.2 px (unselected) after the last tick. The new test requires 54 px with the real fonts, and the existing `- 60` check still passes | In scope |
| The offset choice lives in a GL draw call that no test observes (finding L1 of #126's review) | The choice is a pure helper, `difficulty_best_right`, asserted for both row states | In scope |
| The test takes the outline from `edit_row_rects`, not from the PNGs directly | `test_edit_row_geometry` holds those rects to all ten PNGs within 1 px (worst measured error 0.34 px). The unselected baseline gap has 0.5 px of slack above the 14 px floor | In scope |
| Real-font metrics drift if the fonts change | The test measures line height, ascent and widths at run time with the shipped fonts | In scope |
| The row textures are re-baked with a different outline | `test_edit_row_geometry` fails first and names the PNG; the gap test follows the corrected rects | In scope |
| The best % no longer sits at the mock's x | Intentional, per the owner's request. The header comment records the deviation and #131. The mock's 15 px gap to the outline is restored | In scope |
| The seeded `scores.json` does not show (a fixture chart changed, so its key changed; or the game rejects a record) | `grep '\[Scores\]'` in the log shows the load result. Fallback: the shots show `---`, which still shows the margin on a selected and an unselected row; report that no `%` was shown and skip Task 6 | In scope |
| Seeding touches the owner's data | It writes only to the run's own `RUN/data`, which `stop` deletes. `build/data/scores.json` is never read or written | In scope |
| The `/verify` window cannot be captured (owner on another workspace) | Follow the skill: do not move windows, retry later, and report a missing shot rather than claiming AC 5 | In scope |
| Other window sizes and aspect ratios | The x goes through the same `L.x(...)` as before; `test_render_smoke` draws at six sizes. No new code path | In scope |
| Root cause: the baked row textures place their art 10–13 px inside the mock's position, which also caused #122 and #126 | Not changed here. This plan compensates in the text position, as those did | Out of scope (flag to owner) |
| Timing path touched (principle 1) | Only presentation constants, one pure function and one draw x change. The `git diff --stat` check on timing, audio, input and gameplay is in Validation | In scope |

---

## Open Questions

None blocks the work. The plan uses the recommended default for each.

1. **Selected row: 533 or 534?**
   - **Default (planned): 533.** The selected gap is then at least the unselected gap at mid-height and at the baseline (17.0 against 16.0, 15.4 against 14.5), so AC 2 holds however a screenshot is measured. The gold frame is 2 px wide and bright, and the selected text is larger, so 1 px more room suits it.
   - Alternative: 534. The gaps are equal at mid-height (16.0 / 16.0) and the tightest ink gap matches the mock's 15 px in both rows, but at the baseline the selected row is 0.15 px tighter than the others (14.37 against 14.52). The eye cannot see that, but a pixel measurement can come out 1 px short. With 534 the test's baseline comparison needs a 0.25 px tolerance.
   - Each value is a one-line change if the owner wants it tighter or looser after seeing the screenshots.

2. **How far in unselected rows?** The issue says about 14–18 px and suggests 537–541.
   - **Default (planned): 537.** Only 535–537 keeps 14 px at the text's baseline, where the slanted outline is closest (the issue's 541 gives 10.5 px there). 537 is the smallest move and restores the mock's 15 px ink gap.

3. **Seeding `RUN/data/scores.json` for `/verify`.** A fresh run has no scores, so without it the shots can only show `---`.
   - **Default (planned): seed the run's own data dir before `launch` and record the step in the song select recipe (Task 6).** The game loads the file through the player's normal path, no test-only flag is used, and the owner's `build/data` is not touched. The planning session verified the file with `load_high_scores` but did not launch the game, so Task 1 is the first real run.
   - Alternative: accept `---` in the shots (AC 5 as worded is still met) and rely on the unit test for `100.00%` (AC 3). Then drop Task 6.

---

## Acceptance Criteria

- [ ] The best % ends further left in every difficulty row: `kDiffBestRight` 537 (−10) in unselected rows and `kDiffBestSelectedRight` 533 (−14) in the selected row, leaving 16.0 / 17.0 px to the outline's inner edge at mid-height and 14.5 / 15.4 px at the baseline (tested; inside the issue's 14–18 px)
- [ ] In the selected row the gap to the gold frame is at least the unselected rows' gap, at mid-height and at the baseline (tested; visible in the `/verify` shot)
- [ ] A `100.00%` best stays clear of the last tick: 69.5 px in the selected row, 81.2 px in the others (tested with the real fonts, 54 px floor)
- [ ] The difficulty name, meter number and ticks have not moved (`kDiffNameX`, `kDiffNameSelectedX`, `kDiffMeterCentreX`, `kDiffTickX` unchanged; existing tests pass)
- [ ] `/verify` song select screenshots show the new margin on a selected and an unselected row
- [ ] The `kDiffBestRight` comment says the value departs from the mock (#131)
- [ ] Build has no new warnings. Tests pass 51/51
- [ ] `TODO.md:52` ticked (local). `.agents/issues/todo-issues.md` untouched
