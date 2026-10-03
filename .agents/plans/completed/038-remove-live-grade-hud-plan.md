# Plan: Remove the Live Grade from the Gameplay HUD (#83)

## Summary

During a song, `HudRenderer::render` draws the current live grade (for example `A-` or `**`) at the bottom centre of the screen, using `format_grade` and `grade_text_rect` (`src/gameplay/hud_renderer.cpp:180-185`). The owner wants the bottom of the playfield clear. The fix removes that one draw block. It also deletes the `grade_text_rect` layout helper, whose only callers are the draw block and the life-bar clearance test `test_grade_text_clear`, and deletes that test. Then it updates the doc comments that still mention a HUD grade and ticks the TODO entry. The following stay exactly as they are:

- `format_grade`, which the results screen still uses (`src/screens/results_screen.cpp:178`) and `tests/score_keeper_test.cpp:632-635` tests.
- `ScoreState::grade`, which `src/screens/results.cpp:14` reads to build the results summary and the high-score record.
- The gameplay log line that prints the grade (`src/gameplay/gameplay_view.cpp:388`). It is console output, not HUD drawing.
- The percent, combo, judgment chips and life bar.

The results screen does not change.

## User Story

As a player
I want the bottom of the gameplay screen to be free of the live grade text
So that nothing distracts from the playfield, while I still see my final grade on the results screen

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `gameplay/hud_renderer` (draw block, `grade_text_rect`, comments), `tests/hud_renderer_test.cpp` (drop `test_grade_text_clear`), `TODO.md` |
| GitHub Issue | #83 |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Use `cmake --build build -j$(nproc)`, which builds incrementally |
| C++ compiler | GCC 16.2.1 (`/usr/bin/g++`) | C++20, with `-Wall -Wextra -Wpedantic` (`CMakeLists.txt:13`), no `-Werror`. The build must still add **no new warnings** |
| Baseline tests | **41/41 pass** | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` printed "100% tests passed out of 41" (0.43 s) on `main` @ `e390eb7`. Start green, stay green (still 41 tests: no target is added or removed) |
| Sandbox requirement | — | `audio_test` opens the real audio device. **Always** run ctest through the `bwrap` command above |
| Grade draw site | `src/gameplay/hud_renderer.cpp:180-185` | The only place the HUD draws the grade |
| `grade_text_rect` users | declared `hud_renderer.hpp:33`, defined `hud_renderer.cpp:50-55`, called at `hud_renderer.cpp:183` and `tests/hud_renderer_test.cpp:154` | No other callers (`grep -rn grade_text_rect src tests`) |
| `format_grade` users | `src/screens/results_screen.cpp:178`, `tests/score_keeper_test.cpp:632-635` | **Keep** |
| `ScoreState::grade` readers | `src/screens/results.cpp:14` (summary), `src/gameplay/score_keeper.cpp:199-201` (writer), `tests/results_test.cpp:70` | **Keep** |
| HUD caller | `src/gameplay/gameplay_view.cpp:326-327` | The `render(state, w, h, renderer)` signature stays the same, and `screen_h` is still read by the `<= 0` guard (`hud_renderer.cpp:130`), so it does not become unused |
| Test seam for "nothing drawn" | none | `GlQuadRenderer` (`src/render/gl_quad_renderer.hpp:19-50`) is a concrete GL class with no recording or mocking hook, and draws are no-ops when uninitialized. A unit test cannot observe which strings the HUD draws (see Open Questions) |
| README | `README.md:50` "Live HUD: score %, combo, judgment counts, life bar" | It already leaves out the grade, so **no change** |

---

## Patterns to Follow

### HUD draw blocks are commented by screen position, one block per element

```cpp
// SOURCE: src/gameplay/hud_renderer.cpp:137-145
    // Top-left: live percent.
    const std::string percent_text = format_percent(state.percent);
    const Rect percent_rect = percent_text_rect(percent_text);
    draw_text(renderer, percent_text, percent_rect.x, percent_rect.y, main_pixel, kTextColor);

    // Top-centre: live combo.
    ...
```

Delete the whole `// Bottom-centre: live grade.` block (the comment and the `if`), and leave nothing in its place. The function then ends after the last `draw_chip(...)` call.

### Exposed layout helpers carry a doc comment saying why they are public

```cpp
// SOURCE: src/gameplay/hud_renderer.hpp:29-33
// Screen rects of the top-left percent text and the bottom-centre grade text, as
// `HudRenderer::render` draws them (Blaze 4k presentation). Exposed so layout
// checks (e.g. life bar clearance) read the real HUD geometry.
[[nodiscard]] Rect percent_text_rect(const std::string& text);
[[nodiscard]] Rect grade_text_rect(const std::string& text, int screen_w, int screen_h);
```

After the change, the comment describes only the percent rect: "Screen rect of the top-left percent text, as `HudRenderer::render` draws it ...".

### Tests (plain executable, `TEST_CHECK` abort macro, one print line per case, explicit call list in `main`)

```cpp
// SOURCE: tests/hud_renderer_test.cpp:143-159, 242-257
void test_below_percent_text() { ... std::cout << "  - bar stays below the top-left percent text ok.\n"; }
...
int main() {
    ...
    test_below_percent_text();
    test_grade_text_clear();
    test_judgment_pop_clear();
```

Removing a case means deleting both the function and its call in `main`.

### Error handling

There is no new error path. This change only deletes rendering code, and no untrusted input is involved.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/hud_renderer.cpp` | UPDATE | Remove the bottom-centre grade draw block and the `grade_text_rect` definition. Fix the `kHudTextPixel` comment ("percent/combo/grade" → "percent/combo") |
| `src/gameplay/hud_renderer.hpp` | UPDATE | Remove the `grade_text_rect` declaration. Reword the layout-helper comment (percent only) and the `HudRenderer` class comment (drop "and grade") |
| `tests/hud_renderer_test.cpp` | UPDATE | Remove `test_grade_text_clear` and its call in `main` |
| `TODO.md` | UPDATE | Tick the `(#83)` entry (line 41) |

No files are created. `CMakeLists.txt` and `tests/CMakeLists.txt` do not change.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Stop drawing the grade in the HUD

- **File**: `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  - Delete lines 180-185 (the `// Bottom-centre: live grade.` comment and the `if (state.grade != nullptr) { ... }` block). Also delete the blank line before them, so `render` ends cleanly after the last `draw_chip` call.
  - Delete the `grade_text_rect` definition (lines 50-55) and the blank line after it.
  - Update the `kHudTextPixel` comment (line 40) from `// bitmap-font pixel for percent/combo/grade` to `// bitmap-font pixel for percent/combo`.
  - Keep `format_grade` (lines 84-99). It is public API that the results screen uses.
  - Keep `kGlyphRows`, which `percent_text_rect` still uses.
  - Keep `main_pixel` and `screen_h`, which are both still used.
- **Mirror**: the position-commented draw blocks at `hud_renderer.cpp:137-147`.
- **Validate**: `cmake --build build -j$(nproc) 2>&1 | grep -i "warning\|error"` prints nothing for `hud_renderer`. Expect a link error until Task 3 removes the test's `grade_text_rect` call, so you can do Tasks 1-3 before the first build.

### Task 2: Drop `grade_text_rect` from the header and fix the comments

- **File**: `src/gameplay/hud_renderer.hpp`
- **Action**: UPDATE
- **Implement**:
  - Delete line 33 (`[[nodiscard]] Rect grade_text_rect(...)`).
  - Rewrite the comment at lines 29-31 to: `// Screen rect of the top-left percent text, as HudRenderer::render draws it (Blaze 4k presentation). Exposed so layout checks (e.g. life bar clearance) read the real HUD geometry.` Wrap it at the file's existing ~90-column width.
  - Class comment (lines 56-59): change `Minimal live HUD: score percent, combo, per-window judgment counts, and grade,` to `Minimal live HUD: score percent, combo, and per-window judgment counts,`. The final grade is shown on the results screen only.
  - Leave the `format_grade` declaration and its comment (lines 20-22) unchanged.
- **Mirror**: the existing doc-comment style at `hud_renderer.hpp:24-27`.
- **Validate**: `grep -rn grade_text_rect src` finds no matches.

### Task 3: Remove the grade-clearance test

- **File**: `tests/hud_renderer_test.cpp`
- **Action**: UPDATE
- **Implement**: Delete `test_grade_text_clear()` (lines 152-159, plus the blank line after it) and the `test_grade_text_clear();` call in `main` (line 251). Do not touch any other test. `intersects`, `Size`, `layout_at` and `kCommonSizes` are still used by other cases, so no unused-function warnings appear.
- **Mirror**: the call list in `main` at `tests/hud_renderer_test.cpp:242-257`.
- **Validate**:
  - `grep -rn grade_text_rect src tests` finds no matches.
  - `cmake --build build -j$(nproc)` succeeds with no new warnings.
  - `./build/tests/hud_renderer_test` (or the path that ctest reports) prints every remaining case and `All life bar layout tests passed successfully!`.

### Task 4: TODO tick

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: Change line 41 from `- [ ]` to `- [x]` (the `(#83)` entry).
- **Validate**: `git diff TODO.md` shows a one-character change.

---

## Validation

```bash
# Build (incremental, existing Release tree)
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net \
  ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 41

# Static checks for the ACs
grep -rn "grade_text_rect" src tests          # expect: no output
grep -n "grade" src/gameplay/hud_renderer.cpp # expect: only format_grade's definition (lines ~76-91), no draw call
grep -rn "format_grade" src                   # expect: hud_renderer.{hpp,cpp} + results_screen.cpp:178 only
```

## End-to-End Verification

1. **Static proof that no grade is drawn during play.** The gameplay HUD is drawn only through `GameplayView::render` → `HudRenderer::render` / `render_life` (`gameplay_view.cpp:326-327`) and `JudgmentAnimator` (judgment pop labels only). After the change:
   - `grep -n "format_grade\|state.grade" src/gameplay/` must find no use in any draw path. The only acceptable matches are the `format_grade` definition and declaration in `hud_renderer.{hpp,cpp}` and the `score_keeper` grade bookkeeping.
   - The log line at `gameplay_view.cpp:388` is a console log, not drawing, and stays.
2. **Results screen unchanged.** `results_screen_test` and `results_test` pass, covering `grade_label` and the stored record. `git diff --stat` must not list `src/screens/results_screen.cpp`, `src/screens/results.cpp` or `src/gameplay/score_keeper.*`.
3. **Other HUD elements unchanged.** `git diff src/gameplay/hud_renderer.cpp` must show only deletions plus the one comment edit: the percent, combo, chip and life-bar code stays byte-identical. All 11 remaining `hud_renderer_test` cases pass, including `test_below_percent_text` and `test_no_field_overlap_common_sizes`.
4. **Optional visual check (owner only).** Launch the game, play any song for a few seconds, and confirm three things:
   - The bottom centre of the screen is empty.
   - Percent (top left), combo (top centre), chips (top right) and the vertical life bar (left) look as before.
   - The results screen still shows the large tier-colored grade.

   The agent must **not** launch the GUI binary itself, because it opens a window and plays audio.
5. Run the sandboxed ctest command above: 41/41.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Removing the wrong grade code breaks the results screen or high scores (`format_grade`, `ScoreState::grade`) | Keep both explicitly (Tasks 1-2). Results/high-score tests stay green, and `git diff --stat` must not touch `results*`/`score_keeper*` | In scope |
| A dangling `grade_text_rect` reference causes a compile or link error | Remove the declaration, definition and test call in one change set, then grep `src tests` for zero hits before building | In scope |
| No unit test can prove that "nothing is drawn" (no renderer seam) | Static grep checks plus diff review (E2E steps 1-3), and an optional owner visual check. Adding a recording renderer seam is a refactor beyond this Small issue | Out of scope (flagged in Open Questions) |
| A future HUD element is placed at the bottom without a clearance test, because the grade test that also guarded the life bar's bottom edge is gone | The life bar's own geometry tests (`test_height_scales`, `test_frame_wraps_back`) still pin its position. Any new bottom element should bring its own clearance test | Out of scope |
| Comments still mention a HUD grade, which misleads later readers | Task 1 (`kHudTextPixel`) and Task 2 (helper and class comments). Check with `grep -n "grade" src/gameplay/hud_renderer.*` | In scope |

---

## Open Questions

- **Remove outright or add an Options toggle?** The issue's technical notes assume outright removal, and nobody asked for a toggle. **Proposed default: remove outright** (Lean scope principle, AGENTS.md "Lean scope"). Non-blocking.
- **Add a regression test that the HUD draws no grade?** Doing so would need a draw-recording seam in `GlQuadRenderer`/`draw_text` (or a pure "HUD text items" function), and no such seam exists today. **Proposed default: no new seam.** Rely on removing the code, the static greps and the owner visual check, because the issue is Small and its ACs only require that grade-layout tests be "removed or updated". Non-blocking. The owner can ask for a seam as a follow-up.

---

## Acceptance Criteria

- [ ] No grade text is drawn anywhere on the gameplay screen during play (draw block removed; static greps clean)
- [ ] Percent, combo, judgment-count chips and the life bar still render where they do today (code unchanged; `hud_renderer_test` remaining cases pass)
- [ ] The results screen still shows the final grade unchanged (`format_grade`, `ScoreState::grade`, `results*` untouched; results tests pass)
- [ ] `grade_text_rect` and `test_grade_text_clear` removed; build has no new warnings; sandboxed `ctest` 41/41
- [ ] `TODO.md` `(#83)` entry ticked
- [ ] Follows existing patterns
