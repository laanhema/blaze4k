# Plan: Shift the difficulty meter number and ticks right in song select

## Summary

In the song select difficulty rows, the meter number is centred at `row.x + 174` and the first of the 10 ticks starts at `row.x + 194` (`src/screens/select_art.hpp:67-68`). The issue's technical note assumes the name tab is about 150 px wide. Measuring the baked textures shows otherwise: the tab's right edge sits at **161 px at mid-height (164 px in the selected row) and 163.5 / 167.5 px near the top of the caps**, because the tab is slanted. So a selected `12` (24.8 px wide in `kDiffMeterSelected`) starts at 161.6, which is *under* the tab's slanted edge. That is the cramping the owner sees. This plan moves **both** constants right by the same **16 px**: `kDiffMeterCentreX` 174 → **190** and `kDiffTickX` 194 → **210**. The number keeps its exact spacing to the ticks (AC 1). The selected `12` then clears the tab top by about 10 px and the first tick's ink by about 10.6 px, so it sits centred in the gap (AC 2). The ticks end at 392, about 83 px before the widest best % (`100.00%` selected starts at 475.5) (AC 3). `tick_rect` and the meter draw read the constants, so normal, selected and code-drawn Edit rows all move together with no code change (AC 4). Tests: the four hard-coded `tick_rect` x values move by +16, and a new real-font clearance test pins AC 2 for meters 1..20 in both styles. This is presentation only: no clock, judgment or input code is touched.

## User Story

As a player browsing song select
I want the difficulty number to have clear space between the coloured name tab and the meter ticks
So that two-digit ratings like `12` read cleanly instead of crowding the tab

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/screens/select_art.hpp` (2 constants + 1 comment), `tests/select_art_test.cpp`, `TODO.md` |
| GitHub Issue | #122 ([TODO-31] Shift the difficulty meter number and ticks right in song select) |
| Branch (suggested) | `feature/057-diff-meter-shift-right` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and builds cleanly with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | Run on `main` @ `471e09b` with the sandboxed ctest command in Validation (2.86 s). The count stays **51** because no test executable is added |
| Sandbox requirement | — | Some tests open the real sound device. **Always** run ctest inside the `bwrap` prefix in Validation |
| Font measurements | scratch program linked against `build/libblaze4k_core.a`, `TextRenderer::measure` at 1280x720 | Widths in reference px. See "Measured geometry" below |
| Texture measurements | `assets/theme/cabinet/diff_row_hard{,_selected}.png`, `diff_tick.png` (PIL, content box from `manifest.json`) | See "Measured geometry" below |
| Two-digit meters in the fixture pack | none | `tests/fixtures/reference_pack` has meters 6/7/9 only. The `/verify` screenshot must use the owner's `songs/` (`-- --songs "$PWD/songs"`) |
| Two-digit meters in `songs/` | 136 charts ≥ 10 | E.g. `In The Groove/Anubis` Challenge 10, `In The Groove/Delirium` Challenge 12, `In The Groove/Euphoria` Challenge 12. Edit rows: `In The Groove 3/Hasse Mich` (BlueChaos, 13), `In The Groove 3/VerTex^3` (DOOM, 13) |
| Off-limits file | `.agents/issues/todo-issues.md` | Unrelated, uncommitted owner edits (+40/−2). Do **not** stage, revert or edit it |

### Forward references to #122

`grep -rn "#122\|TODO-31"` finds only `TODO.md:47` (the source line, already tagged `(#122)`, which Task 3 ticks) and the off-limits `.agents/issues/todo-issues.md`. There are no placeholders in `src/` or `tests/`.

### Measured geometry (reference px, relative to the row's content x)

**Name tab right edge (baked rows, every colour shares the art):**

| Row | ~20% height (cap top) | 50% | ~80% (baseline) |
|---|---|---|---|
| normal (`diff_row_hard.png`, 44 tall) | 163.5 | 161.0 | 158.0 |
| selected (`diff_row_hard_selected.png`, 52 tall) | 167.5 | 164.0 | 160.5 |
| code-drawn Edit (`kEditTabWidth` 150, skew 0.213) | ≈ 154 | 150 | ≈ 146 |

**First tick ink:** `diff_tick.png` is 40x36 @2x (20x18 ref). Its opaque x is 3..16.5 ref at mid-height (5..18.5 at the top, 1..14.5 at the bottom). So the visible tick starts at `kDiffTickX + 3`.

**Meter text widths** (`kDiffMeter` 28 / `kDiffMeterSelected` 32):

| Text | normal | selected |
|---|---|---|
| `9` | 13.3 | 15.2 |
| `10` | 22.3 | 25.5 |
| `12` | 21.7 | 24.8 |
| `18` | 22.6 | 25.9 |
| `20` | 26.2 | 29.9 |
| `88` | 27.4 | 31.3 |

**Best %:** `100.00%` is 63.8 (normal) / 71.5 (selected) wide, right-aligned at 547, so it starts at 483.2 / 475.5.

**Clearance for the shift `d`** (left = number's left edge − tab edge at cap top; right = first tick ink − number's right edge, which does **not** depend on `d`):

| Case | d = 0 (today) | d = 12 | **d = 16 (plan)** |
|---|---|---|---|
| `12` selected, left / right | **−5.9** / 10.6 | 6.1 / 10.6 | **10.1 / 10.6** |
| `20` selected, left / right | **−8.5** / 8.1 | 3.6 / 8.1 | **7.6 / 8.1** |
| `88` selected, left / right | **−9.2** / 7.4 | 2.9 / 7.4 | **6.9 / 7.4** |
| `12` normal, left / right | **−0.4** / 12.2 | 11.7 / 12.2 | **15.7 / 12.2** |

At d = 16 a selected `12` is centred between the slanted tab and the first tick (10.1 vs 10.6). That is the rationale for 16, the top of the issue's suggested 10–16 range.

---

## Patterns to Follow

### Layout constants (reference px, measured-from comment)
```cpp
// SOURCE: src/screens/select_art.hpp:63-73
// Difficulty row width (manifest diff_row_* content 1128 @2x).
inline constexpr float kDiffRowWidth = 564.0f;
// Inside a difficulty row, from the row's content x (measured from the mock; the
// manifest notes say 24 / 183 / 216).
inline constexpr float kDiffNameX = 15.0f;
inline constexpr float kDiffMeterCentreX = 174.0f;
inline constexpr float kDiffTickX = 194.0f;
inline constexpr float kDiffBestRight = 547.0f; // measured from mock: 17px in from the right edge
```

### The only consumers (no code change needed)
```cpp
// SOURCE: src/screens/select_art.cpp:224-228
Rect tick_rect(const Rect& row, bool selected, int n) {
    const float h = selected ? kTickSelectedHeight : kTickHeight;
    return Rect{row.x + kDiffTickX + static_cast<float>(n) * layout::kDiffTickPitch,
                row.y + (row.h - h) * 0.5f, kTickWidth, h};
}
// SOURCE: src/screens/select_art.cpp:602-603 (meter text, normal + selected + Edit rows alike)
text->draw(renderer, std::to_string(rows[i].chart->meter), L.x(row.x + kDiffMeterCentreX),
           centred_top(*text, L, row.y, row.h, style), style, TextAlign::Centre);
```

### Error handling
Not applicable. These are constexpr layout values. Draw helpers already no-op for null services (`select_art.hpp:14-15`).

### Tests (plain executable, `TEST_CHECK` abort macro, section print, real fonts via `loaded_text()`)
```cpp
// SOURCE: tests/select_art_test.cpp:575-591
void test_ticks() {
    ...
    const Rect row = art::difficulty_row_rect(0, 3);
    TEST_CHECK(rect_eq(art::tick_rect(row, false, 0), 238, 385, 20, 18));
    ...
    // The ticks end before the best % column.
    TEST_CHECK(art::tick_rect(row, false, 9).x + 20 < row.x + art::kDiffBestRight - 60);
    std::cout << "  - meter ticks ok.\n";
}
// SOURCE: tests/select_art_test.cpp:559-571 (real-font measure against a layout budget)
blaze4k::TextRenderer& text = loaded_text();
...
TEST_CHECK(text.measure(name, theme::text::kDiffNameSelected) <= art::kDiffNameBudget);
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | UPDATE | `kDiffMeterCentreX` 174 → 190, `kDiffTickX` 194 → 210. Update the comment above them and the `tick_rect` doc comment (`:209`, `194` → `210`) |
| `tests/select_art_test.cpp` | UPDATE | `test_ticks()` x values +16. Add a real-font meter clearance check (AC 2). Update the header comment |
| `TODO.md` | UPDATE | Tick line 47 (`(#122)`) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Shift the meter number and ticks by 16 px

- **File**: `src/screens/select_art.hpp`
- **Action**: UPDATE
- **Implement**:
  - `:67` `kDiffMeterCentreX = 174.0f` → `190.0f`. `:68` `kDiffTickX = 194.0f` → `210.0f`.
  - Edit the comment at `:64-65` so it stays truthful, keeping the file's terse style. For example: `// Inside a difficulty row, from the row's content x (measured from the mock; the manifest notes say 24 / 183 / 216). The meter and ticks sit 16px right of the mock (#122): the slanted tab reaches x 167.5 at the cap top of a selected row.`
  - `:209` doc comment: `{row.x + 194 + n*18, ...}` → `{row.x + 210 + n*18, ...}`.
  - Do **not** touch `kDiffNameX`, `kDiffBestRight`, `kDiffNameBudget`, `kEditTabWidth`, `layout::kDiffTickPitch`, or `select_art.cpp`.
- **Mirror**: `src/screens/select_art.hpp:63-73`
- **Validate**: `cmake --build build -j$(nproc)`

### Task 2: Update and extend the tests

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. In `test_ticks()` (`:583-587`), the row x values are 44 for the normal row and 58 for the selected one (`kDiffListX` + `kDiffRowSelectedShiftX` 14):
     - `238` → `254` and `400` → `416` (normal row, ticks 0 and 9)
     - `252` → `268` and `414` → `430` (selected row)
     - Leave y/w/h unchanged. Keep the existing gap check at `:589` as is (392 < 487 holds).
  2. Add a section `test_meter_clearance()`, called from `main` right after `test_ticks()`, that pins AC 2 with the real fonts:
     - Use test-local constants with a provenance comment: the baked tab's right edge at the cap top, `kTabTopNormal = 163.5f` and `kTabTopSelected = 167.5f` (measured from `diff_row_hard{,_selected}.png`), and the tick ink inset `kTickInkInset = 3.0f` (`diff_tick.png` opaque x at mid-height). Use a margin `kMinClear = 4.0f`.
     - For `meter` in 1..20 and both `(kDiffMeter, kTabTopNormal)` and `(kDiffMeterSelected, kTabTopSelected)`:
       - `w = text.measure(std::to_string(meter), style)`
       - `TEST_CHECK(art::kDiffMeterCentreX - w * 0.5f >= tab_top + kMinClear)`
       - `TEST_CHECK(art::kDiffMeterCentreX + w * 0.5f <= art::kDiffTickX + kTickInkInset - kMinClear)`
     - The Edit row's code-drawn tab is narrower (`art::kEditTabWidth` = 150 at mid), so it is covered by the baked check. Add one line asserting `art::kEditTabWidth <= kTabTopNormal` so that stays true.
     - Also assert that the number and ticks moved together: `art::kDiffTickX - art::kDiffMeterCentreX == 20.0f` (the spacing is unchanged from 194 − 174, AC 1).
     - Print `"  - meter clearance ok.\n"`.
     - Expected worst cases at d = 16: `20` selected, left 7.6 and right 8.1. Both pass with a 4 px margin. Before the change, `12` selected fails the left check (−5.9), so the test does guard the fix.
  3. Add a line to the header comment block (`:1-13`): `// #122: the meter number and ticks sit 16px right; real-font clearance from the slanted tab and the first tick.`
- **Mirror**: `tests/select_art_test.cpp:544-591` (`loaded_text()`, `TEST_CHECK`, section print)
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/select_art_test`

### Task 3: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:47`: `- [ ]` → `- [x]`. Change only that line. Do **not** touch `.agents/issues/todo-issues.md`.
- **Validate**: `git diff --stat -- TODO.md .agents/issues/todo-issues.md` (`TODO.md` shows 1 line changed. `todo-issues.md` shows only its pre-existing unstaged diff)

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j$(nproc)

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
touch src/screens/select_art.cpp tests/select_art_test.cpp
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning" | grep -E "select_art" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **51/51 pass** (baseline 51/51 on `471e09b`; no executable is added).

Static checks:

```bash
# Only the two constants (and comments) change in src/
git diff -- src | grep -E '^[+-][^+-]' | grep -vE '^\s*[+-]\s*//'   # expect exactly the 4 lines for kDiffMeterCentreX / kDiffTickX
# Timing / judgment / input / assets untouched
git diff --stat -- src/timing src/audio src/input src/gameplay assets   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-issues   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable):** `select_art_test` pins the new tick x values, the unchanged best % gap, the unchanged number-to-tick spacing, and real-font clearance for meters 1..20 in normal and selected rows. Its existing `test_render_smoke` walks the full select draw path (normal, selected and Edit rows) at 6 window sizes.
2. **`/verify` screenshot (AC 5).** Use the `verify` skill with the owner's packs. The fixture pack has no two-digit meter.
   ```bash
   B=.claude/skills/verify/scripts/b4k.sh
   cmake --build build -j$(nproc)
   $B doctor
   $B launch --run /tmp/blaze4k-verify/122-meter -- --songs "$PWD/songs"
   $B keys /tmp/blaze4k-verify/122-meter Return && $B wait-screen /tmp/blaze4k-verify/122-meter Select
   $B shot /tmp/blaze4k-verify/122-meter 01-select          # first song (e.g. Anubis): selected Challenge 10 on top
   $B keys /tmp/blaze4k-verify/122-meter Right && $B shot /tmp/blaze4k-verify/122-meter 02-unselected-10   # 10 in a normal row
   # Move Down to a 12 (e.g. Delirium / Euphoria) and an ITG3 Edit 13 (Hasse Mich / VerTex^3); check each
   # highlight in game.log, then shoot 03-twelve and 04-edit-13.
   $B stop /tmp/blaze4k-verify/122-meter
   ```
   Expected in each shot:
   - The number is fully clear of the coloured tab's slanted edge and has visible space before the first tick, in the selected row (32 px) and in normal rows (28 px).
   - The ticks end well before the best % (`---` or `NN.NN%`).
   - The Edit row (code-drawn tab) looks the same way.
   - The name, row art and best % have not moved.

   Read every PNG you cite. Run `stop` even after a failed attempt.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The issue note assumed a 150 px tab, so a 10 px shift would still leave wide selected meters touching the slanted tab | Measured the baked tab (161/164 mid, 163.5/167.5 cap top) and chose d = 16. The new real-font clearance test fails at d = 0 and passes at 16 | In scope |
| A three-digit or very wide meter (e.g. `100`) overflows | Real ITG meters are 1..20. Two digits fit up to `88` with ≥ 6.9 px each side. Wider is unrealistic, and the number is drawn centred, so it would spill evenly. Not tested beyond 20 | Out of scope (flag) |
| The ticks move toward the best % column | Ticks end at 392, and the widest best % (`100.00%` selected) starts at 475.5, which leaves 83 px. The existing `< kDiffBestRight - 60` check is kept | In scope |
| Real-font measurements drift if fonts change | The test measures at runtime with the shipped fonts instead of hard-coding widths, and only the tab/tick-ink geometry is a test-local constant with provenance | In scope |
| The Edit row's code-drawn tab (`kEditTabWidth` = 150) is ~11 px narrower than the baked tab (161 at mid). Its comment says "measured from the baked rows' tab" | Unaffected by this change. The meter just gets even more room there. Fixing the Edit tab width is a separate visual tweak | Out of scope (flag to owner) |
| The layout drifts from the mock / manifest notes (meter 183, ticks 216 @ manifest units) | Intentional per the owner's request. The header comment records the +16 deviation and #122 | In scope |
| Timing path touched (principle 1) | Only two presentation constants change. The `git diff --stat` check on timing/audio/input/gameplay is in Validation | In scope |

---

## Open Questions

None blocks the work. The plan already uses the recommended default.

1. **How far to shift?** The issue says "ever so slightly" and suggests about 10–16 px.
   **Recommendation: 16 px.** At 12 px, a selected `20` or `88` clears the slanted tab by only 3–4 px, and a selected `12` sits off-centre in its gap (6 vs 10.6). At 16 px, a selected `12` is centred between the tab and the first tick (10.1 / 10.6), and the ticks still end 83 px before the best %. This is a one-line change if the owner wants it tighter after seeing the screenshot.

---

## Acceptance Criteria

- [ ] The meter number and the 10 ticks both move right by 16 px and keep their relative position (`kDiffTickX - kDiffMeterCentreX == 20`, tested)
- [ ] A two-digit meter in the selected row's 32 px font clears both the name tab and the first tick (real-font clearance test, meters 1..20, ≥ 4 px)
- [ ] The last tick still ends well before the best % column. The existing gap check passes and the `tick_rect` x checks are updated (254/416, 268/430)
- [ ] The same shift applies to normal, selected and code-drawn Edit rows (shared constants; render smoke + screenshot)
- [ ] A `/verify` song select screenshot shows two-digit meters laid out cleanly
- [ ] Build has no new warnings. Tests pass 51/51
- [ ] `TODO.md:47` ticked. `.agents/issues/todo-issues.md` untouched
