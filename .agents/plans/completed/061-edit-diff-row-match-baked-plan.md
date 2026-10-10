# Plan: Make the Edit difficulty row match the other rows' size and position in song select

## Summary

In the song select difficulty list an Edit chart's row is the only one drawn in code (`draw_difficulty_row_art`, `src/screens/select_art.cpp:522-545`). It fills the whole 564 px row rect, and the selected one adds a gold quad 2 px **outside** it. The baked rows do not fill their content box: measured from all ten `diff_row_*.png` textures, the unselected art spans x **10..554** at mid-height (544 wide, a 1 px `#27325A` ring on all four sides) and the selected art spans x **12..552** (540 wide, a 2 px gold ring **inside** the 52 px content box). So an unselected Edit row is 20 px wider and starts 10 px further left, and a selected Edit row is 28 px wider, starts 14 px further left and is 4 px taller (56 against 52). This plan adds one pure helper, `edit_row_rects(row, selected)`, that returns the Edit row's frame, inner and tab rects from four named constants (insets 10 / 12, ring 1 / 2), and redraws the Edit row from those rects: body, a four-strip ring (`kEditEdge`, or gold when selected), and the 150 px tab inside the ring. The gold outset (`kEditSelectedOutset`) goes away. A scratch probe shows the new geometry is within **0.54 px** of the baked pixels on every row of every texture (today: 10.2 to 14.5 px off). A new test decodes the real PNGs with stb_image and holds the Edit geometry to them within 1 px. The row rect and all `kDiff*` text offsets stay as they are. This is presentation only: no clock, judgment or input code is touched.

## User Story

As a player browsing song select
I want an Edit chart's row to have the same size and position as the Beginner to Challenge rows
So that the difficulty list reads as one list, whether the Edit row is selected or not

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | `src/screens/select_art.hpp` (constants, one struct, one function, comments), `src/screens/select_art.cpp` (one new pure function, the Edit branch of `draw_difficulty_row_art`), `tests/select_art_test.cpp`, `TODO.md` |
| GitHub Issue | #130 ([TODO-35] Make the Edit difficulty row match the other rows' size and position in song select) |
| PRD Phase | N/A |
| Branch (suggested) | `feature/061-edit-diff-row-match-baked` |
| Plan sequence | `061`. `060` was used by #127 (branch `feature/060-stat-label-left-margin`); no `060` plan file is in the repo |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and current. Build with `cmake --build build -j` |
| C++ compiler | GCC 16.2.1 | C++20, `-O3 -Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | Run on `main` @ `299c2d3` with the sandboxed ctest command in Validation (2.86 s). The count stays **51**: no test executable is added |
| Sandbox requirement | n/a | Some tests open the real sound device. **Always** run ctest inside the `bwrap` prefix in Validation |
| Texture measurements | `assets/theme/cabinet/diff_row_*.png` and `diff_row_*_selected.png` (PIL, content box from `manifest.json`) | All five colours share the geometry. See "Measured geometry" |
| stb_image in tests | scratch program built with `select_art_test`'s exact flags and link line (`build/tests/CMakeFiles/select_art_test.dir/flags.make`, `link.txt`) | `#include <stb_image.h>` compiles with 0 warnings and `stbi_load` links from `libblaze4k_core.a` (`src/render/stb_image_impl.cpp`; `stb` is a PUBLIC link dependency of `blaze4k_core`, `CMakeLists.txt:156-163`). No CMake change is needed |
| Headless theme | `ThemeTextures::load` without GL | Logs `No GL context available; drawing flat fallbacks` and still returns the manifest entries (`entry(name)`: `file`, `width`, `height`, `content`) and `texture_scale()` = 2 |
| Earlier screenshots | `/tmp/blaze4k-verify/126-after/evidence/03-edit-unselected.png`, `04-edit-selected.png` (1280x720, taken after #126; #127 did not touch select) | Show today's Edit rows. Used for the reproduction below. `/tmp` may be cleared, so Task 1 takes fresh ones |
| Edit charts for `/verify` | `songs/` (owner's packs, gitignored): `Virtual Emotion` has Edit rows `BlueChaos`, `Phrekwenci`, `Rynker` | Reached with `Up` x5 from the first song (`.agents/reports/059-diff-name-left-margin-plan-report.md:59-60`). The fixture pack has no Edit chart |
| `TODO.md` | gitignored (`.gitignore:27`) | Ticked locally only, as in earlier plans |
| Off-limits file | `.agents/issues/todo-issues.md` | Unrelated, uncommitted owner edit. Do **not** stage, revert, edit or commit it |

### Forward references to #130

`grep -rn "#130\|TODO-35"` over `src/`, `tests/`, `docs/`, `README.md`, `AGENTS.md` finds only `TODO.md:51` (the source line, already tagged `(#130)`, which Task 6 ticks). There are no placeholders in code.

Two earlier documents point at this work without the number:

- `.agents/plans/completed/059-diff-name-left-margin-plan.md:365,384` (Risk row and Open Question 3): "The code-drawn Edit row is drawn 11–14 px left of the baked rows... file a separate issue to align it." This is that issue.
- `.agents/reviews/feature-057-diff-meter-shift-right-review.md:23` (finding L1): the Edit-tab guard in `test_meter_clearance` compares a mid-height width with a cap-top edge. Task 4 replaces that guard.

### Measured geometry (reference px, relative to the row's content x and y)

The row rect is the texture's content box: `draw_slice3(..., L.rect(row))` with `row.w = 564` = content width 1128 @2x, so the image maps 1:1 and nothing is stretched (`tests/select_art_test.cpp:816-817` pins the content sizes). Edges are fitted over every image row; all five colours agree to 0.02 px.

| Quantity | Unselected (`diff_row_*.png`, 44 tall) | Selected (`diff_row_*_selected.png`, 52 tall) |
|---|---|---|
| Opaque art, top..bottom | 0..44 | 0..52 (the gold ring is inside the content box) |
| Outer left edge at mid-height | **10.0** (fit 9.90) | **12.0** (fit 11.95) |
| Outer right edge at mid-height | **554.0** (fit 554.09) | **552.0** (fit 552.05) |
| Outer width | **544** | **540** |
| Slant (tan) | 0.213 = `theme::skew::kRows` | 0.213 |
| Ring | 1 px `#27325A`, opaque, on all four sides | 2 px `#FFD633` (gold), opaque, on all four sides |
| Tab fill, left..right at mid-height | **11.0..161.0** (150 wide) | **14.0..164.0** (150 wide) |
| Tab fill, top..bottom | 1..43 (inside the ring) | 2..50 (inside the ring) |
| Body, right edge at mid-height | 553.0 | 550.0 |
| Body colour | `#0B1030`, alpha 217/255 (0.85) | `#0A1030`, alpha 230/255 (0.90), with a tint of the tab colour fading out by x 377 |
| Outside the content box | nothing | soft gold glow, alpha 56/255 at most |

In code terms, with `skewed_quad` (`src/screens/select_art.cpp:234-238`):

- Unselected: frame `{row.x + 10, row.y, row.w - 20, row.h}`, ring 1, tab `{frame.x + 1, frame.y + 1, 150, frame.h - 2}`.
- Selected: frame `{row.x + 12, row.y, row.w - 24, row.h}`, ring 2, tab `{frame.x + 2, frame.y + 2, 150, frame.h - 4}`.

### Bug reproduction (evidence)

**A. Scratch probe against the baked pixels** (headless; C++ program linked to `build/libblaze4k_core.a`; decodes all ten PNGs with `stbi_load`; for every image row 4 px or more inside the content box it compares the art's outer left/right edge (alpha >= 128) and the tab fill's left/right edge (exact fill colour) with the code geometry's slanted edge at that y; worst case over all rows and all five colours):

| Geometry | Row state | Frame left | Frame right | Tab left | Tab right | Height |
|---|---|---|---|---|---|---|
| Today (`skewed_quad(row)`, tab at `row.x`, gold outset 2) | unselected | **10.15** | **10.16** | **11.50** | **10.99** | 0.00 |
| Today | selected | **14.24** | **14.21** | **14.54** | **13.96** | **4.00** |
| Planned (`edit_row_rects`) | unselected | 0.34 | 0.33 | 0.50 | 0.51 | 0.00 |
| Planned | selected | 0.31 | 0.31 | 0.54 | 0.54 | 0.00 |

The residual 0.3 to 0.5 px is the PNG's antialiasing at 2x (one image pixel is 0.5 reference px).

**B. Earlier screenshots** (1280x720, window px = reference px; scanline at each row's mid-height; `Virtual Emotion`):

| Row | Row rect x, y | Outer left | Tab left..right | Outer right | Top..bottom |
|---|---|---|---|---|---|
| `HARD`, baked, unselected | 44, 488 | 54 | 55..205 | 598 | 488..532 (44) |
| `BlueChaos`, Edit, unselected | 44, 434 | **44** | **44..194** | 608 (computed; the 0.85 body edge is too faint to detect) | 434..478 (44) |
| `CHALLENGE`, baked, selected | 58, 372 | 70 | 72..222 | 610 | 372..424 (52) |
| `BlueChaos`, Edit, selected | 58, 426 | **56** | **58..208** | **624** | **424..480 (56)** |

Both agree with the issue's report: the Edit row is larger, and the selected one sits in a different place.

### Issue-stated numbers, re-measured

| Issue note | Re-measured | Verdict |
|---|---|---|
| `diff_row_beginner.png` spans x 10.0 → 553.5 at mid-height (14.5 → 558 top, 5.5 → 549 bottom) | 10.0 → **554.0** (14.5 → 558.5 on the first row, 5.5 → 549.5 on the last) | Left edges right. The right edges are the last opaque pixel's left side, 0.5 px short. Width is 544 |
| `diff_row_beginner_selected.png` spans x 12.0 → 551.5 at mid-height | 12.0 → **552.0** | Same 0.5 px. Width is 540 |
| Code-drawn quad spans 0 → 564, and −2 → 566 with the selected outset | Confirmed (screenshot: 44..608 and 56..624) | Right |
| "about 20 px wider, starting 10–14 px further left" | Unselected: 20 px wider, 10 px left. Selected: **28 px** wider, 14 px left | Right for unselected; the selected row is worse than stated |
| `kEditTabWidth = 150` "measured from the baked rows' tab" | Tab fill is 150 wide (11..161, 14..164) | Right. The width stays; only its start moves |
| Not in the issue | The selected Edit row is **4 px taller** (56 against 52): its gold quad is outset 2 px above and below the row rect, while the baked gold ring is inside the content box | Fixed by this plan (ring inside the frame) |
| Not in the issue | Baked unselected rows have the 1 px `#27325A` ring on the left and right sides too; the code draws only top and bottom strips. Baked selected rows have no `#27325A` strips; the code draws them inside the gold. The baked tab sits inside the ring; the code tab is full height and covers the strips | Fixed by this plan (one ring per state, tab inside it) |
| Test line references (`select_art_test.cpp` ~555, ~626, ~670; `select_screen_test.cpp` ~278) | `:555-562`, `:626-627`, `:670-672`; `:278-381` | Right |

---

## Patterns to Follow

### Naming: layout constants in reference px with a measured-from comment and the issue number
```cpp
// SOURCE: src/screens/select_art.hpp:82-87
// Code-drawn Edit row (#84; there is no diff_row_edit texture).
inline constexpr float kEditTabWidth = 150.0f; // measured from the baked rows' tab
inline constexpr Color kEditBody = theme::hex(0x0B1030, 0.85f); // sampled from diff_row_beginner.png
inline constexpr Color kEditBodySelected = theme::hex(0x0B1030, 1.0f);
inline constexpr Color kEditEdge = theme::hex(0x27325A); // 1px top/bottom strips (sampled)
inline constexpr float kEditSelectedOutset = 2.0f;
```

### Pure layout helper next to its draw helper (GL-free, so the test pins it headless)
```cpp
// SOURCE: src/screens/select_art.hpp:214-222
// Tick `n` (0..9) of `row`: {row.x + 210 + n*18, centred, 20, 18 (20 selected)}.
[[nodiscard]] Rect tick_rect(const Rect& row, bool selected, int n);

// Left edge of the difficulty name in `row`: row.x + 21 (25 selected).
[[nodiscard]] float difficulty_name_x(const Rect& row, bool selected);

// The CSS skewX parallelogram of `rect` about its vertical centre (TL, TR, BR, BL):
// the top edge shifts right by skew*h/2, the bottom edge left by the same.
[[nodiscard]] std::array<Vec2, 4> skewed_quad(const Rect& rect, float skew);
```

### The draw site being changed
```cpp
// SOURCE: src/screens/select_art.cpp:529-544
// Code-drawn Edit row (#84): the baked rows' slanted body, edges and tab.
const float k = theme::skew::kRows;
if (selected) {
    const float o = kEditSelectedOutset;
    draw_solid(renderer, L, skewed_quad(Rect{row.x - o, row.y - o, row.w + 2 * o, row.h + 2 * o}, k),
               theme::color::kGold);
}
const std::array<Vec2, 4> body = skewed_quad(row, k);
draw_solid(renderer, L, body, selected ? kEditBodySelected : kEditBody);
if (row.h > 2.0f) {
    const float edge = 1.0f / row.h;
    draw_solid(renderer, L, quad_band(body, 0.0f, edge), kEditEdge);
    draw_solid(renderer, L, quad_band(body, 1.0f - edge, 1.0f), kEditEdge);
}
draw_solid(renderer, L, skewed_quad(Rect{row.x, row.y, kEditTabWidth, row.h}, k),
           style.colors.fill);
```
`quad_band` (`:78-81`) and `draw_solid` (`:83-90`) are file-local helpers and stay as they are.

### Error handling
Not applicable. These are constexpr layout values and a pure function. Draw helpers already no-op for null services and an uninitialised renderer (`src/screens/select_art.hpp:9-10`). The new helper clamps so that a degenerate row never yields a negative size.

### Tests (plain executable, `TEST_CHECK` abort macro, section print, real theme via `loaded_theme()`)
```cpp
// SOURCE: tests/select_art_test.cpp:114-119
blaze4k::ThemeTextures& loaded_theme() {
    static blaze4k::ThemeTextures theme;
    static const bool loaded = theme.load(kCabinet);
    TEST_CHECK(loaded);
    return theme;
}

// SOURCE: tests/select_art_test.cpp:790-799
void test_skewed_quad() {
    const auto q = art::skewed_quad(Rect{44, 372, 564, 44}, theme::skew::kRows);
    const float shift = theme::skew::kRows * 22.0f;
    TEST_CHECK(vec_eq(q[0], 44 + shift, 372) && vec_eq(q[1], 608 + shift, 372));
    ...
    std::cout << "  - skewed quad ok.\n";
}
```

### Decoding an image with stb_image (declarations only; the implementation lives in the core library)
```cpp
// SOURCE: src/render/texture.cpp:9, 218
#include <stb_image.h>
...
unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
```
No test decodes a PNG today. `select_art_test` will be the first; the scratch probe confirms it builds and links unchanged.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_art.hpp` | UPDATE | New constants `kEditInsetX`, `kEditSelectedInsetX`, `kEditBorder`, `kEditSelectedBorder`; remove `kEditSelectedOutset`; new `EditRowRects` + `edit_row_rects`; comments (`kDiffNameBudget`, Edit block, `draw_difficulty_row_art`) |
| `src/screens/select_art.cpp` | UPDATE | Implement `edit_row_rects`; draw the Edit row from it (body, four-strip ring, tab) |
| `tests/select_art_test.cpp` | UPDATE | New `test_edit_row_geometry()` (pure rects + baked-pixel comparison via stb_image); update the two Edit-tab guards in `test_meter_clearance` and `test_name_margin`; header comment line |
| `TODO.md` | UPDATE | Tick line 51 (`(#130)`); gitignored, local only |

No file is created. `tests/select_screen_test.cpp`, `tests/CMakeLists.txt`, `src/render/theme.hpp`, `src/screens/options_art.cpp` (it reads `kEditEdge`, which keeps its name and value) and `assets/` are not changed.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Capture the "before" screenshots

- **File**: none (evidence only, in `/tmp/blaze4k-verify/130-before/evidence/`)
- **Action**: RUN, before editing any source file
- **Implement**: Follow the `verify` skill (`.claude/skills/verify/SKILL.md`, recipe `.claude/skills/verify/features/song-select.md`) with the unmodified build:
  ```bash
  B=.claude/skills/verify/scripts/b4k.sh
  R=/tmp/blaze4k-verify/130-before
  $B doctor
  $B launch --run $R -- --songs "$PWD/songs"
  $B keys $R Return && $B wait-screen $R Select
  $B keys $R Up Up Up Up Up
  $B shot $R 01-edit-unselected      # Virtual Emotion: CHALLENGE selected; BlueChaos, Phrekwenci, Rynker (Edit) unselected
  $B keys $R Right
  $B shot $R 02-edit-selected        # BlueChaos (Edit) selected; CHALLENGE and HARD unselected
  $B stop $R
  ```
  Read both PNGs and check the song title before citing them. If `shot` fails because the window is not viewable, follow the skill: do not move the owner's windows, report the missing "before" shot, and continue (`/tmp/blaze4k-verify/126-after/evidence/03-edit-unselected.png` and `04-edit-selected.png` show the same state if they still exist). If `songs/` is missing, report that no Edit row can be shown and continue. AC 5 needs only the "after" shots.
- **Mirror**: `.claude/skills/verify/features/song-select.md`
- **Validate**: `ls /tmp/blaze4k-verify/130-before/evidence/`

### Task 2: Add the Edit row constants and the pure helper's declaration

- **File**: `src/screens/select_art.hpp`
- **Action**: UPDATE
- **Implement**:
  - Replace the Edit block at `:82-87` with (terse style, measurements in the comment):
    ```cpp
    // Code-drawn Edit row (#84; there is no diff_row_edit texture), matched to the baked
    // rows' art (#130). Measured from diff_row_*.png / diff_row_*_selected.png (all five
    // colours agree): the art is inset inside the 564px content box. At mid-height the
    // 1px kEditEdge ring spans x 10..554; the selected row's 2px gold ring spans x 12..552
    // and stays inside the content box (no outset). The tab is 150px wide inside the ring.
    inline constexpr float kEditInsetX = 10.0f;
    inline constexpr float kEditSelectedInsetX = 12.0f;
    inline constexpr float kEditBorder = 1.0f;
    inline constexpr float kEditSelectedBorder = 2.0f;
    inline constexpr float kEditTabWidth = 150.0f; // the tab fill: x 11..161 at mid-height, 14..164 selected
    inline constexpr Color kEditBody = theme::hex(0x0B1030, 0.85f); // sampled from diff_row_beginner.png
    inline constexpr Color kEditBodySelected = theme::hex(0x0B1030, 1.0f);
    inline constexpr Color kEditEdge = theme::hex(0x27325A); // the unselected row's 1px ring (sampled)
    ```
    `kEditSelectedOutset` is removed (its only user is `select_art.cpp:532`). `kEditTabWidth`, `kEditBody`, `kEditBodySelected` and `kEditEdge` keep their values.
  - `:74` (`kDiffNameBudget` comment): the Edit tab no longer ends at 150. Replace with, for example: `// The name ends at x 143 in the selected row (25 + 118), inside the tab (baked and Edit: it ends at x 161 at mid-height, 164 selected).` Leave the value `118.0f`.
  - In the "Pure layout" section, right after `skewed_quad` (`:220-222`), add:
    ```cpp
    // The code-drawn Edit row's rects for `row`, before the kRows skew (reference px):
    //  frame  the ring's outer edge = the baked art's opaque extent:
    //         {row.x + 10, row.y, row.w - 20, row.h} (12 / 24 selected)
    //  inner  `frame` inset by `border` on every side (1, 2 selected)
    //  tab    the first kEditTabWidth of `inner`
    // The three share the row's vertical centre, so skewed_quad() keeps their slanted
    // sides parallel and `border` px apart. Sizes never go negative.
    struct EditRowRects {
        Rect frame;
        Rect inner;
        Rect tab;
        float border = 0.0f;
    };
    [[nodiscard]] EditRowRects edit_row_rects(const Rect& row, bool selected);
    ```
  - `:322` (`draw_difficulty_row_art` comment): `// One difficulty row's art: the baked slice3, or the code-drawn Edit row (edit_row_rects).`
  - Do **not** touch `kDiffRowWidth`, `kDiffNameX`, `kDiffNameSelectedX`, `kDiffMeterCentreX`, `kDiffTickX`, `kDiffBestRight`, the `kDiffNameBudget` value, or anything in `src/render/theme.hpp`.
- **Mirror**: `src/screens/select_art.hpp:64-87` (constants), `:214-222` (helper declarations)
- **Validate**: builds together with Task 3 (`kEditSelectedOutset` is still referenced in the `.cpp` until then)

### Task 3: Implement `edit_row_rects` and draw the Edit row from it

- **File**: `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  1. After `skewed_quad` (`:234-238`), add `edit_row_rects`:
     ```cpp
     EditRowRects edit_row_rects(const Rect& row, bool selected) {
         const float inset = selected ? kEditSelectedInsetX : kEditInsetX;
         EditRowRects out;
         out.frame = Rect{row.x + inset, row.y, std::max(0.0f, row.w - 2.0f * inset),
                          std::max(0.0f, row.h)};
         out.border = std::min(selected ? kEditSelectedBorder : kEditBorder,
                               std::min(out.frame.w, out.frame.h) * 0.5f);
         const float b = out.border;
         out.inner = Rect{out.frame.x + b, out.frame.y + b, out.frame.w - 2.0f * b,
                          out.frame.h - 2.0f * b};
         out.tab = Rect{out.inner.x, out.inner.y, std::min(kEditTabWidth, out.inner.w), out.inner.h};
         return out;
     }
     ```
  2. Replace the Edit branch of `draw_difficulty_row_art` (`:529-544`). Keep the baked branch (`:525-528`) as it is. Draw order: body, ring, tab.
     ```cpp
     // Code-drawn Edit row (#84), on the baked rows' geometry (#130): the body, a ring
     // (kEditEdge, gold when selected) and the tab inside the ring.
     const float k = theme::skew::kRows;
     const EditRowRects g = edit_row_rects(row, selected);
     const Color ring = selected ? theme::color::kGold : kEditEdge;
     const std::array<Vec2, 4> frame = skewed_quad(g.frame, k);
     // The body runs under the opaque ring, so no seam can open between them.
     draw_solid(renderer, L, frame, selected ? kEditBodySelected : kEditBody);
     if (g.border > 0.0f) {
         const float t = g.border / g.frame.h;
         draw_solid(renderer, L, quad_band(frame, 0.0f, t), ring);
         draw_solid(renderer, L, quad_band(frame, 1.0f - t, 1.0f), ring);
         draw_solid(renderer, L,
                    skewed_quad(Rect{g.frame.x, g.inner.y, g.border, g.inner.h}, k), ring);
         draw_solid(renderer, L,
                    skewed_quad(Rect{g.inner.x + g.inner.w, g.inner.y, g.border, g.inner.h}, k), ring);
     }
     draw_solid(renderer, L, skewed_quad(g.tab, k), style.colors.fill);
     ```
     Why this shape:
     - The unselected body is 0.85 alpha, so the ring cannot be one filled quad under it (the body would blend with `#27325A` instead of the backdrop). Four opaque strips on top of the body give the baked result in both states with one code path.
     - `g.border > 0` implies `g.frame.h > 0` (the border is clamped to half the frame), so the division is safe. This replaces the old `row.h > 2.0f` guard.
     - The side strips use `g.inner.y` / `g.inner.h`, so they share the frame's vertical centre and line up with the top and bottom bands.
  3. Nothing else in the file changes: `draw_difficulty_rows` (`:556-621`) still passes `difficulty_row_rect(...)`, and the name, meter, tick and best % positions are the shared ones.
- **Mirror**: `src/screens/select_art.cpp:224-238` (pure helpers), `:529-544` (the code being replaced)
- **Validate**: `cmake --build build -j` (0 warnings in `select_art.cpp`)

### Task 4: Update the two Edit-tab guards in the existing tests

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. `test_meter_clearance`, `:626-627`. Replace the comment and check with:
     ```cpp
     // The code-drawn Edit tab is the baked tab (test_edit_row_geometry holds it to the
     // PNGs), so the baked check covers it. Its mid-height edge: 161, 164 selected.
     TEST_CHECK(art::kEditInsetX + art::kEditBorder + art::kEditTabWidth <= kTabTopNormal);
     TEST_CHECK(art::kEditSelectedInsetX + art::kEditSelectedBorder + art::kEditTabWidth <=
                kTabTopSelected);
     ```
     Values: 161 <= 163.5 and 164 <= 167.5.
  2. `test_name_margin`, `:670-672`. Replace the comment and check with:
     ```cpp
     // The Edit tab's right edge at the name's baseline (under 8px below the row's
     // centre in both rows); it is the baked tab's edge (#130).
     const float slant = theme::skew::kRows * 8.0f;
     TEST_CHECK(kNameRight + kMinClear <=
                art::kEditInsetX + art::kEditBorder + art::kEditTabWidth - slant);
     TEST_CHECK(kNameRight + kMinClear <=
                art::kEditSelectedInsetX + art::kEditSelectedBorder + art::kEditTabWidth - slant);
     ```
     Values: 147 <= 159.3 and 147 <= 162.3 (was 148.3).
  3. Leave everything else in both tests as it is. The `kTabLeftNormal = 12.5` / `kTabLeftSelected = 16.0` left-gap checks now hold for Edit rows too, because the Edit tab starts where the baked tab does.
- **Mirror**: `tests/select_art_test.cpp:615-688`
- **Validate**: `cmake --build build -j`

### Task 5: Add `test_edit_row_geometry()` (AC 4)

- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Add `#include <stb_image.h>` after the standard headers (`:19-30`), with a one-line comment: `// declarations only; blaze4k_core carries the implementation (stb_image_impl.cpp)`.
  2. Add a file-local helper in the anonymous namespace, before the new test:
     ```cpp
     // x of the left / right slanted side of skewed_quad(r, kRows) at reference y.
     float side_x(const Rect& r, bool right, float y) {
         const auto q = art::skewed_quad(r, theme::skew::kRows);
         const Vec2 a = right ? q[1] : q[0];
         const Vec2 b = right ? q[2] : q[3];
         return a.x + (b.x - a.x) * ((y - a.y) / (b.y - a.y));
     }
     ```
  3. Add `test_edit_row_geometry()` right after `test_skewed_quad()` (`:790-799`) and call it from `main` right after `test_skewed_quad();` (`:1213`). Three parts:

     **Part A: the rects, in the list's coordinates** (rows from `difficulty_row_rect`: `{44, 372, 564, 44}` and `{58, 534, 564, 52}`):
     - Unselected: `frame` = `{54, 372, 544, 44}`, `inner` = `{55, 373, 542, 42}`, `tab` = `{55, 373, 150, 42}`, `border == 1.0f`.
     - Selected: `frame` = `{70, 534, 540, 52}`, `inner` = `{72, 536, 536, 48}`, `tab` = `{72, 536, 150, 48}`, `border == 2.0f`.
     - Same height and top as the row in both states: `frame.y == row.y && frame.h == row.h` (today the selected gold quad is 4 px taller).
     - The whole slanted art stays inside the row rect: every corner of `art::skewed_quad(frame, theme::skew::kRows)` has `row.x <= x <= row.x + row.w` (today it does not: relative to the row's x the corners reach −4.7 and 568.7, and −8.0 and 572.0 when selected; planned: 5.3 and 558.7, and 6.5 and 557.5 when selected).
     - Degenerate rows give no negative size: for `Rect{0, 0, 0, 0}`, `Rect{0, 0, 10, 1}` and `Rect{0, 0, 564, 0}`, both states: `frame.w`, `frame.h`, `inner.w`, `inner.h`, `tab.w`, `tab.h` and `border` are all `>= 0.0f`.

     **Part B: against the baked pixels.** For each of `{"diff_row_beginner", kBeginner}`, `{"diff_row_easy", kEasy}`, `{"diff_row_medium", kMedium}`, `{"diff_row_hard", kHard}`, `{"diff_row_challenge", kChallenge}` (`theme::difficulty`) and each state (`name + "_selected"` when selected):
     - `const blaze4k::ThemeEntry* e = loaded_theme().entry(name); TEST_CHECK(e != nullptr);`
     - `stbi_load((kCabinet / e->file).string().c_str(), &w, &h, &n, 4)`; `TEST_CHECK(img != nullptr && w == e->width && h == e->height);` Free with `stbi_image_free` before leaving the iteration.
     - `const float px = 1.0f / loaded_theme().texture_scale();` (0.5 reference px per image px). The row is the content box at the origin: `const Rect row{0, 0, e->content.w * px, e->content.h * px};` and `TEST_CHECK(row.w == art::kDiffRowWidth);`
     - `const art::EditRowRects g = art::edit_row_rects(row, selected);`
     - The fill colour as 8-bit: `std::lround(colors.fill.r * 255.0f)` etc. A pixel is "fill" when its alpha is 255 and each channel is within 2 of the fill. (This tolerance separates Medium's `#FFD23A` from the gold ring `#FFD633`.)
     - For every image row `y` in `[e->content.y + 8, e->content.y + e->content.h - 8)` (4 reference px inside the top and bottom, clear of both rings): `ref_y = (y - e->content.y + 0.5f) * px`. Scan the row once for the first and last x with alpha >= 128 (`outer_l`, `outer_r = last + 1`) and for the first contiguous run of fill pixels (`tab_l`, `tab_r = end`). With `ref_x(x) = (x - e->content.x) * px` and `constexpr float kTol = 1.0f;` check:
       - `std::fabs(ref_x(outer_l) - side_x(g.frame, false, ref_y)) <= kTol`
       - `std::fabs(ref_x(outer_r) - side_x(g.frame, true, ref_y)) <= kTol`
       - `std::fabs(ref_x(tab_l) - side_x(g.tab, false, ref_y)) <= kTol`
       - `std::fabs(ref_x(tab_r) - side_x(g.tab, true, ref_y)) <= kTol`
     - Height (AC 1, AC 2): at the content box's centre column, the first and last image row with alpha >= 128 give the art's top and bottom; `TEST_CHECK(approx((bottom - top) * px, g.frame.h, 0.5f));` and `TEST_CHECK(approx((top - e->content.y) * px, 0.0f, 0.5f));`
     - Tab height (the ring's thickness): at the column `e->content.x + 160` (reference x 80, inside every tab), the first and last fill rows give `1..43` / `2..50`; check them against `g.tab.y` and `g.tab.y + g.tab.h` within 0.5.

     **Part C:** print `"  - Edit row geometry vs the baked rows ok.\n"`.

     Expected values, from the scratch probe: worst frame error 0.34, worst tab error 0.54, height error 0.00. On `main` the test cannot pass: `edit_row_rects` does not exist, and today's geometry is 10.2 to 14.5 px off with a 4 px height error when selected.
  4. Add a line to the header comment block (`:1-17`): `// #130: the code-drawn Edit row's frame, tab and height are held to the baked rows' pixels (all ten diff_row PNGs, decoded with stb_image).`
  5. Do **not** edit `test_row_style()` (`:524-564`: the Edit style is still `baked == false` with empty texture names), `test_render_smoke()` (it already walks a 7-chart song with two Edit rows, selected and unselected, at six window sizes) or `tests/select_screen_test.cpp` (`test_named_edit_charts`, `:278-381`, keeps passing: it reads only `kDiffNameBudget`).
- **Mirror**: `tests/select_art_test.cpp:790-822` (`test_skewed_quad`, `test_chrome_layout`); `src/render/texture.cpp:218` (`stbi_load` with 4 channels)
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/select_art_test`

### Task 6: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: `TODO.md:51`: `- [ ]` → `- [x]`. Change only that line. The file is gitignored, so this is a local edit. Do **not** touch `.agents/issues/todo-issues.md`.
- **Validate**: `sed -n 51p TODO.md` starts with `- [x]`.

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

Expected: **51/51 pass** (baseline 51/51 on `299c2d3`; no executable is added).

Static checks:

```bash
# The removed constant is gone everywhere
grep -rn "kEditSelectedOutset" src tests ; echo "exit $? (expect 1)"
# The shared row rect and text offsets did not move
git diff -- src | grep -E '^[+-].*(kDiffRowWidth|kDiffNameX|kDiffNameSelectedX|kDiffMeterCentreX|kDiffTickX|kDiffBestRight|kDiffNameBudget) =' ; echo "exit $? (expect 1)"
# Only select_art changed in src/; timing / judgment / input / assets untouched
git diff --stat -- src                                                          # expect select_art.cpp and select_art.hpp only
git diff --stat -- src/timing src/audio src/input src/gameplay src/render assets tests/CMakeLists.txt   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-issues   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable).** `select_art_test`'s new `test_edit_row_geometry` holds the Edit row's frame, tab and height to the pixels of all ten baked row textures, in both states (AC 1 to AC 4). `test_ticks`, `test_meter_clearance` and `test_name_margin` still pin the shared name, meter and tick positions, and `test_render_smoke` walks the full select draw path with selected and unselected Edit rows at six window sizes.

2. **`/verify` screenshots (AC 5).** Use the `verify` skill with the owner's packs, after the final build.
   ```bash
   B=.claude/skills/verify/scripts/b4k.sh
   R=/tmp/blaze4k-verify/130-after
   cmake --build build -j
   $B doctor
   $B launch --run $R -- --songs "$PWD/songs"
   $B keys $R Return && $B wait-screen $R Select
   $B keys $R Up Up Up Up Up
   $B shot $R 01-edit-unselected      # Virtual Emotion: CHALLENGE selected; BlueChaos, Phrekwenci, Rynker (Edit) unselected
   $B keys $R Right
   $B shot $R 02-edit-selected        # BlueChaos (Edit) selected; CHALLENGE and HARD unselected
   $B keys $R Right Right
   $B shot $R 03-edit-selected-lower  # Phrekwenci (Edit) selected, between HARD and Rynker
   $B stop $R
   ```
   Check the song title in each shot before citing it. If `songs/` is missing, report that the Edit shots were not taken; do not claim AC 5.

   Expected, comparing with the Task 1 "before" shots:
   - `01`: the three Edit rows start and end at the same x as `HARD` between them. Their tabs line up with `HARD`'s tab on both sides.
   - `02`: the selected `BlueChaos` row has the same gold frame position and size as the selected `CHALLENGE` row in `01` (shifted down by one row). It no longer sticks out to the left or right of the rows around it, and it is no taller.
   - In every shot the name, meter number, ticks and best % of an Edit row sit at the same x as in the baked rows (they did before as well).

   Numeric check, when the PNG is 1280x720 (window px = reference px), ±1 px:

   | Shot, row, scanline | Quantity | Before | After | Baked reference |
   |---|---|---|---|---|
   | `01`, `BlueChaos` unselected, y 456 | tab fill left..right | 44..194 | **55..205** | `HARD` at y 510: 55..205 |
   | `01`, `BlueChaos` unselected, y 456 | outline right end | 608 | **598** | `HARD` at y 510: 598 |
   | `02`, `BlueChaos` selected, y 452 | gold left, tab left..right | 56, 58..208 | **70, 72..222** | `CHALLENGE` in `01` at y 398: 70, 72..222 |
   | `02`, `BlueChaos` selected, y 452 | gold right end | 624 | **610** | `CHALLENGE` in `01`: 610 |
   | `02`, `BlueChaos` selected, column x 500 | gold top..bottom | 424..480 | **426..478** | `CHALLENGE` in `01`: 372..424 (52 tall) |

   Read every PNG you cite. Run `stop` even after a failed attempt.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The Edit row is matched to hand-typed numbers that drift from the art | The four constants come from a fit over every row of all ten PNGs, and the new test measures the PNGs at run time. If the textures are re-baked with different geometry, the test fails and names the edge | In scope |
| First test to decode a PNG directly (`<stb_image.h>` in a test) | The scratch probe built with the test target's exact flags and link line: 0 warnings, links from `libblaze4k_core.a`. `texture.cpp` already includes the same header under the same warning flags, so MSVC `/W4` sees nothing new. No CMake change | In scope |
| A seam shows between the ring and the 0.85-alpha body | The body is drawn over the whole frame and the opaque ring strips on top of it, so the pieces overlap instead of abutting | In scope |
| The colour match in the test confuses Medium's fill `#FFD23A` with the gold ring `#FFD633` | The match is exact to ±2 per channel with alpha 255; the two differ by 4 (G) and 7 (B). The probe ran all five colours | In scope |
| The 1 px ring is thinner than a pixel in windows smaller than 1280x720 (`L.s < 1`), so parts of it can drop out | Same as today's 1 px top and bottom strips. The baked rows blur instead. Not changed | Out of scope (flag only) |
| Code-drawn edges are not antialiased, so the Edit row's slanted sides are slightly harder than the baked rows' | Already true today. Geometry only, per the issue | Out of scope (flag only) |
| The selected Edit row still looks plainer than the baked selected rows: no outer gold glow, an opaque body (baked: 0.90 alpha) and no tab-colour tint fading across the body | The issue's assumption: geometry only, the glow is not reproduced. Raised as Open Question 1 | Out of scope (owner call) |
| Removing `kEditSelectedOutset` breaks another user | `grep` finds it only at `select_art.hpp:87` and `select_art.cpp:532`. `kEditEdge` (also read by `options_art.cpp:128`) keeps its name and value | In scope |
| The `/verify` window cannot be captured (owner on another workspace), or `songs/` is missing | Follow the skill: do not move windows, retry later, and report a missing shot rather than claiming AC 5. The fixture pack has no Edit chart | In scope |
| Timing path touched (principle 1) | Only presentation constants, one pure layout function and one draw branch change. The `git diff --stat` check on timing, audio, input and gameplay is in Validation | In scope |

---

## Open Questions

None blocks the work. The plan uses the recommended default for each.

1. **Should the selected Edit row also copy the baked selected rows' look, beyond geometry?** The baked selected rows have a soft gold glow outside the frame, a 0.90-alpha body, and a tint of the tab colour that fades out about 215 px into the body. The selected Edit row has none of these (opaque `#0B1030` body).
   - **Default (planned): leave it.** The issue says the fix matches geometry only and the glow is not reproduced. After this change the size and position are identical, and the remaining difference is shading.
   - Alternative: bake a `diff_row_edit` / `diff_row_edit_selected` texture pair in the neutral colour and drop the code-drawn row. This gives an exact match, antialiased edges included, but it adds assets and contradicts the issue's assumption that the Edit row stays code-drawn. A separate issue if wanted.

2. **Give Edit names back the room they lost in #126?** The Edit tab was the tightest right limit for names (150 at mid-height), which is why `kDiffNameBudget` dropped to 118 and `M. Poveromo` is shown as `M. Pover...` when selected (`059` plan, Open Question 1). The tab now ends at 161 (164 selected), as in the baked rows.
   - **Default (planned): keep 118.** The issue says the `kDiff*` offsets should not need to change, and the budget is shared by all rows. Only its comment is updated.
   - Alternative: raise the budget to about 128 in a follow-up. `M. Poveromo` needs 127 px; at x 25 it would end at 152, still 10 px inside the tab at the baseline (162.3) and 23 px before the widest meter number (175.1). It would also need `kNameRight = 143` in `test_name_margin` to move.

3. **The unselected rows' side ring.** The plan draws the 1 px `#27325A` ring on the left and right sides too, because the baked rows have it. Today's Edit row has only the top and bottom strips.
   - **Default (planned): draw all four sides.** It is what makes the Edit row's outline end where the baked rows' does (AC 1).

---

## Acceptance Criteria

- [ ] An unselected Edit row has the same visible width, height and left/right edges as an unselected baked row in the same slot (frame x 10..554 of the row at mid-height, 44 tall; tested against the PNGs within 1 px)
- [ ] A selected Edit row sits at the same position and has the same visible size as any other selected row, including its gold frame (frame x 12..552, 52 tall, 2 px gold ring inside the row rect; tested against the PNGs within 1 px)
- [ ] The Edit row's tab starts and ends at the same x as the other rows' tab (11..161, 14..164 selected; tested), and its name, meter, ticks and best % use the shared, unchanged offsets
- [ ] `tests/select_art_test.cpp` has `test_edit_row_geometry`, which checks the Edit row's geometry against the baked rows' pixels
- [ ] `/verify` song select screenshots of `Virtual Emotion` show the Edit rows lined up with the rows around them, selected and unselected
- [ ] `kEditSelectedOutset` is gone; `difficulty_row_rect` and the `kDiff*` values are unchanged
- [ ] Build has no new warnings. Tests pass 51/51
- [ ] `TODO.md:51` ticked. `.agents/issues/todo-issues.md` untouched
