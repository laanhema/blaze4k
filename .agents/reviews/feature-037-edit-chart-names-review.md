# Code Review: feature/037-edit-chart-names (#84)

**Scope**: Branch `feature/037-edit-chart-names` vs `main`, including uncommitted changes (13 tracked files under `src/` and `tests/`, plus the untracked plan and implementation report). `.agents/stories/todo-stories.md` is excluded: it is an unrelated change.
**Recommendation**: APPROVE (with nits)

## Summary

The branch shows an Edit chart's description in place of the `Edit` label on the select-screen difficulty rows and on the results difficulty line, and falls back to `Edit` when the description is empty. It also fixes the SSC loader so that `#CHARTNAME` no longer overwrites `#DESCRIPTION` (last-wins bug), with the SM5 `#VERSION` < 0.74 rule. I checked every upstream citation against SM5 `5_1-new@825467b` and OpenITG `f2c129f`, and they all hold. The code is correct, bounds-safe, and well tested. All issue #84 acceptance criteria are met. The five findings below are all Low: cosmetic or edge-case parity nits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/screens/select_screen.cpp:694`: a row filled to the budget can run a few pixels past the highlight bar.**
   - The bar is drawn from `diff_x - 6` with width `0.42*width`, but `difficulty_row_text` gets the full `0.42*width` as the space available from `diff_x`.
   - The `floor` usually leaves enough slack: 12.6 px at 1280 and 11.4 px at 1920.
   - When the slack is under about 3.5 px, the last glyph's ink crosses the bar's right edge. At 1680 px (0.42 × 1680 = 705.6, so 47 cells = 705 px) the ink ends at `diff_x + 702.5` and the bar ends at `diff_x + 699.6`, about 2.9 px over.
   - It is cosmetic only: the row never gets near the wheel at `0.52*width`.
   - Fix: pass `width * 0.42f - 6.0f`, or `- 12.0f` for symmetric padding.

2. **`src/screens/select_screen.cpp:122-133`: the name budget depends on each row's `best` string, so the same name is shortened differently once a score exists.**
   - The tests show it: Bagpipe displays `mDaWg & Hatena Zubon` with `---`, and `mDaWg & Hatena...` with `100.00%`.
   - So setting a score changes how the chart is named, and two Edit rows with equally long names can be cut at different points.
   - Suggestion: reserve the widest best column (`"100.00%"`, 7 cells) in `fixed` so the name budget is the same on every row.

3. **`src/chart/simfile_parser.cpp:41-44` (and constants at `:34,37`): `parse_ssc_version` parses as `double`, so the "Mirrors SM5 StringToFloat" claim fails at the edges.**
   - SM5 compares `float`s (`strtof` then `isfinite`). Two inputs come out differently:
     - `"0.73999999"` rounds to `0.74f`, so SM5 treats the chart as modern. Here it stays `< 0.74` and is treated as old.
     - `"1e39"` overflows `float` to inf and becomes 0 in SM5 (old). Here it is a finite `double` and is treated as modern.
   - Both inputs are contrived; real files say `0.83`.
   - Fix: call `std::strtof` on a `std::string` copy, map non-finite results to 0, and keep both constants as `float`. This also avoids a double-to-float narrowing that would be undefined behavior if the code were later changed to cast.

4. **`src/screens/results_screen.cpp:57`: the "Challenge = 9 cells" minimum is defined twice.**
   - The local `kMinNameCells = 9` duplicates `kMinDifficultyLabelCells` in `select_screen.hpp`. The report lists this as a deliberate deviation, to avoid including `select_screen.hpp`.
   - Suggestion: move one constant next to `chart_display_label` in `song_display_text.hpp`, which both screens already include, so the two values cannot drift apart.

5. **`src/screens/select_screen.cpp:673`: the reflowed comment line is 108 characters long.**
   - The surrounding code wraps at about 100 columns. Rewrap the comment.

**Noted, not a finding** (scoped out by the plan or accepted as an open question):
- An SSC Edit whose name is only in `#CHARTNAME` shows `Edit`. This is faithful to SM5 `StepsDisplay.cpp:199-202` (plan Open Question 1).
- Results shows a longer (less shortened) name than select (Open Question 3).
- At narrow widths (≤ about 800 px) standard rows already overflow the bar.
- Pass 1 lets per-chart SSC `#BPMS`/`#OFFSET`/`#STOPS` leak into song-level timing. This predates the branch; follow-up candidate.

## Upstream citation check

I fetched the SM5 and OpenITG sources at the pinned commits and read every cited line. Each matches the code:
- **SM5**
  - `NotesLoaderSSC.cpp:74-78,315-318` (song and steps `#VERSION` → `song->m_fVersion`)
  - `:319-324` (`SetChartName`)
  - `:336-349` (`SetDescription` with the `< VERSION_CHART_NAME_TAG` branch)
  - `NotesLoaderSSC.h:32` (0.74f)
  - `Song.h:25` (0.83f) and `Song.cpp:78`
  - `RageUtil.cpp:1861-1869` (`strtof`, non-finite → 0)
  - `StepsDisplay.cpp:199-202` (`IsAnEdit()` → `GetDescription()`)
  - `Steps.h:72` and `Steps.cpp:283-292` (resolved difficulty)
- **OpenITG**
  - `DifficultyMeter.cpp:158,228-233`
- **Trimming**: SM5 trims the description, and Blaze's `MsdFile` already trims params (`msd_file.cpp:121-161`), so the two agree.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | PASS (already up to date) |
| Warnings gate | PASS: I recompiled the 5 changed `src/` files and 4 changed test files with `-fsyntax-only` using the build's own flags (`-std=c++20 -Wall -Wextra -Wpedantic`, plus the test targets' `flags.make` flags). Zero warnings. |
| Lint | N/A (no linter configured) |
| Tests | PASS: 41/41, run sandboxed with `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` |
| Sandbox check | Inside bwrap, `/dev/snd` and `/run/user/$UID` were empty, and `audio_test` reported `NULL Playback Device via Null` |
| Skipped / env-guarded tests | None found (`-V` output has no SKIP markers; the "Skipping" lines are the parser's own log output) |

## What's Good

- **Faithful to the references.** Every behavioral choice cites SM5/OpenITG at a pinned commit, and the citations are accurate. The Edit check uses the resolved difficulty (`IsAnEdit` after `TidyUpData`), not the raw label.
- **Last-wins regression is pinned.** The parser tests cover both tag orders, the exact 0.74 boundary, junk and non-finite versions, a steps-level `#VERSION` taking effect in file order, and the per-block reset.
- **`truncate_to_cells` is solid.** It cuts only on code-point boundaries, keeps a combining mark with its glyph, never rewrites malformed bytes, and uses the same cell model as `text_width`. A 10k-case fuzz test with a fixed seed backs this up.
- **Identity is unchanged.** `difficulty_color` and the high-score keys still use the raw label, and a test asserts that the key ignores the description.
- **Signed arithmetic with clamps** in both builders covers 320 px, zero-width and zero-pixel inputs, and all of these are tested.
- **Pure builders** (`difficulty_row_text`, `results_difficulty_line`) keep the draw sites to one line each and make the format exactly testable. Non-Edit rows are byte-identical to before.

## Recommendation

Mergeable as is. Findings 1 and 2 are small one-line layout tweaks worth taking if the owner's visual check (plan E2E 2) shows rows touching the bar edge. Findings 3 to 5 are optional cleanups.
