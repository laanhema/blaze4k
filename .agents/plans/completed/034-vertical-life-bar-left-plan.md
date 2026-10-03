# Plan: Vertical Life Bar on the Left Side (#76)

## Summary

The life bar is a 16 px tall horizontal bar at the bottom centre, up to 480 px wide
(`src/gameplay/hud_renderer.cpp:165-195`). It sits right in the lane where up-scroll arrows arrive.
We move it to the **left edge of the screen** and make it **vertical**, filling from the bottom
(empty) to the top (full):

- **Pure layout helper.** Put the geometry in a new GL-free function, `layout_life_bar(life,
  screen_w, screen_h, field_left)`, that returns the frame, back and fill rects plus a danger flag.
  `HudRenderer::render_life` then only draws those three quads. This follows the house pattern of
  pure, headlessly testable layout helpers (`layout_hold`, `note_field_renderer.hpp:43-44`).
- **Field clamp.** The bar is anchored to a fixed left margin and clamped so it always stays at
  least a 16 px gap left of the note field's left edge.
- **Shared field-left formula.** That edge comes from a new `NoteField::field_left(screen_w)`, so
  the HUD and `NoteFieldRenderer` share one formula instead of duplicating
  `(screen_w - field_width) * 0.5`.
- **Height.** The bar's height scales with the screen height: centred vertically and kept below the
  top-left percent text.
- **Unchanged.** The palette, the 2 px frame, the back colour, the fill colour and the `< 0.3`
  danger tint all stay the same. The grade/score text is not touched.

## User Story

As a player
I want the life bar drawn vertically on the left side of the screen
So that it never covers the incoming arrows, receptors, combo or judgment text

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `gameplay/hud_renderer` (new `LifeBarLayout` + `layout_life_bar`, `render_life` signature), `gameplay/note_field` (new `field_left(screen_w)` accessor), `gameplay/note_field_renderer` (use the accessor), `gameplay/gameplay_view` (pass field left), tests (new `hud_renderer_test`, one `note_field_test` case), `TODO.md` |
| GitHub Issue | #76 |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Release); `cmake --build build -j16` is incremental |
| C++ compiler | GCC 16.2.1 | C++20 |
| Baseline tests | **38/38 pass** | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build` reports "100% tests passed out of 38" (0.52 s), on `main` @ `a36ed72` |
| Life bar draw | `src/gameplay/hud_renderer.cpp:165-195` (`render_life`) | Today: `bar_w = min(w*0.40, 480)`, `bar_h = 16`, `border = 2`, centred at `y = h - 8 - 21 - 12 - 16` |
| Palette | `src/gameplay/hud_renderer.cpp:24-30` | `kLifeBackColor`, `kLifeFrameColor`, `kLifeFillColor`, `kLifeDangerColor`, `kLifeDangerThreshold = 0.3` (metrics.ini:2565 per header comment) |
| Single caller | `src/gameplay/gameplay_view.cpp:327` | `hud_.render_life(life_.life(), screen_w, screen_h, renderer);`. `field_` (a `NoteField`) is in scope there |
| Field geometry | `note_field_renderer.cpp:144`, `noteskin.hpp:98-99`, `gameplay_view.cpp:63` | `field_left = (screen_w - 4*108) * 0.5`. The field is **432 px wide at every resolution**, with no scaling |
| Receptor row | `gameplay_view.cpp:69, 245` | `receptor_y = 0.15*h` (up) / `0.85*h` (reverse). It spans only the field's columns |
| Other HUD text | `hud_renderer.cpp:115-161`, `judgment_animator.cpp:131-151` | Percent at top-left (x 8, y 8, pixel 3: 7 rows × 3 = 21 px tall, `"100.00%"` = 126 px wide). Combo at top centre. Chips at top right. Grade at bottom centre. Judgment pop centred at `0.42h`, at most `"FANTASTIC"` × 5 px × 1.25 = 338 px wide, so it stays inside the 432 px field width |
| Window size bounds | `src/data/config.hpp:20-28` | Default 1280x720; `kMinWindowDimension = 320`. No minimum enforced in `window.cpp`, and the window is resizable |
| GL in tests | `gl_quad_renderer.hpp:9-13` | Draws are no-ops when headless, so the geometry must be tested through the pure helper |

### Computed layout (with the constants proposed below)

| Window | `field_left` | Bar frame x-range | Bar frame y-range | Gap to field |
|--------|--------------|-------------------|-------------------|--------------|
| 1280x720 | 424 | 22–42 | 142–578 | 382 px |
| 1920x1080 | 744 | 22–42 | 214–866 | 702 px |
| 640x480 (narrow 4:3) | 104 | 22–42 | 94–386 | 62 px |
| 1024x768 (4:3) | 296 | 22–42 | 151.6–616.4 | 254 px |
| 500x400 (very narrow) | 34 | clamped (see Task 1) | 78–322 | ≥ 16 px if possible, else best effort |

**Start green, stay green:** 38 tests pass now. This plan adds one new test target
(`hud_renderer_test`) and one case to `note_field_test`, so **39/39** are expected afterwards.

---

## Pinned Semantics

- **Fill direction.** The fill height is `back.h * clamped_life`, anchored to the bottom:
  `fill.y = back.y + back.h * (1 - life)`. Life 1.0 fills the whole back, and life 0.0 gives a
  zero-height fill.
- **Clamp and danger.** `life` is clamped to [0, 1], as today. Danger is `clamped < 0.3` (strict),
  exactly as today (`hud_renderer.cpp:192-193`), so life 0.3 is *not* danger.
- **No upstream constants.** The geometry is Blaze 4k presentation with no OpenITG parity
  requirement, which matches the existing `// Blaze 4k presentation, unsourced` comment. The only
  semantic value, the 0.3 threshold, is unchanged.
- **Scroll direction.** Reverse scroll does **not** move the bar (issue technical note).

### Proposed geometry constants (presentation, unsourced)

| Constant | Value | Rationale |
|----------|-------|-----------|
| `kLifeBarLeft` | 24 px | x of the inner bar. With the 2 px frame, the frame starts at 22 px, inside the percent text's column (x 8–134) and below it |
| `kLifeBarThickness` | 16 px | The current `bar_h`, so the bar keeps the same visual weight |
| `kLifeBarBorder` | 2 px | Unchanged |
| `kLifeBarFieldGap` | 16 px | Minimum clearance from the frame's right edge to `field_left`, which also covers small explosion overdraw |
| `kLifeBarMinThickness` | 6 px | The narrowest the bar is allowed to shrink in very narrow windows |
| Vertical inset | `max(0.2 * h, 40)` above and below | Centres the bar over 60% of the height. 40 px keeps the frame below the percent text (bottom at y 29) even in tiny windows |

---

## Patterns to Follow

### Pure layout helper + result struct exposed in the header (headless-testable)
```cpp
// SOURCE: src/gameplay/note_field_renderer.hpp:34-44
struct HoldLayout {
    double body_end_y = 0.0;  // body's tail-side edge == the cap's unclipped head-side edge
    bool has_body = false;    // false once the head reaches/passes body_end_y
    bool has_cap = false;     // false without cap art, or when the head is past the whole cap
    ...
};
[[nodiscard]] HoldLayout layout_hold(double head_y, double tail_y, bool reverse,
                                     double cap_size, double tail_inset, bool has_cap);
```

### Presentation constants in the anonymous namespace with the "unsourced" note
```cpp
// SOURCE: src/gameplay/hud_renderer.cpp:24-30
// Life bar palette + geometry (Blaze 4k presentation, unsourced; no OpenITG parity
// requirement). The only semantic value is the 0.3 danger threshold.
constexpr Color kLifeBackColor{0.10f, 0.12f, 0.18f, 0.90f};
...
constexpr double kLifeDangerThreshold = 0.3;
```

### Guard style for degenerate screen sizes (no exceptions, early return)
```cpp
// SOURCE: src/gameplay/hud_renderer.cpp:167-169
if (screen_w <= 0 || screen_h <= 0) {
    return;
}
```

### Small inline NoteField accessors
```cpp
// SOURCE: src/gameplay/note_field.hpp:65-67
[[nodiscard]] double column_x(int column, double field_left) const;
[[nodiscard]] double field_width() const { return 4.0 * config_.column_width; }
[[nodiscard]] double screen_y(double offset) const;
```

### Tests (plain executable, `TEST_CHECK` abort macro, `near` helper, section prints)
```cpp
// SOURCE: tests/note_field_renderer_test.cpp:1-40
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)
...
void test_upscroll_cel() {
    const HoldLayout l = layout_hold(100.0, 500.0, false, S, I, true);
    TEST_CHECK(near(l.body_end_y, 452.0));
    ...
    std::cout << "  - up-scroll Cel cap centred on the tail ok.\n";
}
```

### Test registration
```cmake
# SOURCE: tests/CMakeLists.txt:125-133
add_executable(score_keeper_test
    score_keeper_test.cpp
)

target_link_libraries(score_keeper_test PRIVATE
    blaze4k_core
)

add_test(NAME score_keeper_test COMMAND score_keeper_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/note_field.hpp` | UPDATE | Add `[[nodiscard]] double field_left(int screen_w) const` (the centred field's left edge) |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | Line 144: use `field.field_left(screen_w)` (same value, single source of truth) |
| `src/gameplay/hud_renderer.hpp` | UPDATE | Add the `LifeBarLayout` struct and `layout_life_bar(...)`. Change `render_life` to take `double field_left`. Update the doc comment from "horizontal" to "vertical, left side" |
| `src/gameplay/hud_renderer.cpp` | UPDATE | Geometry constants, the `layout_life_bar` implementation, and `render_life` drawing from the layout |
| `src/gameplay/gameplay_view.cpp` | UPDATE | Line 327: pass `field_.field_left(screen_w)` |
| `tests/hud_renderer_test.cpp` | CREATE | Layout tests: orientation, bottom-up fill, clamp, danger, no overlap at 1280x720 / 1920x1080 / 640x480 / 1024x768, narrow clamp, degenerate sizes, grade-text clearance |
| `tests/CMakeLists.txt` | UPDATE | Register `hud_renderer_test` |
| `tests/note_field_test.cpp` | UPDATE | One case: `field_left(1280) == 424` with the 108 px column width, and `field_left` + `field_width` is centred |
| `TODO.md` | UPDATE | Tick line 38 (`(#76)`) once it is done, following prior issues (lines 30-37) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: `LifeBarLayout` + `layout_life_bar` (pure)

- **File**: `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header (after `judgment_color`):
    ```cpp
    // Vertical life bar geometry (Blaze 4k presentation): anchored to the left screen
    // edge, centred vertically over ~60% of the height, filling bottom (empty) to top
    // (full), and clamped to stay at least a small gap left of `field_left` (the note
    // field's left edge). `visible` is false for non-positive screen sizes.
    struct LifeBarLayout {
        bool visible = false;
        bool danger = false;   // clamped life < 0.3
        Rect frame{};          // backing frame quad (inner rect grown by the border)
        Rect back{};           // empty-bar background
        Rect fill{};           // filled portion, bottom-anchored inside `back`
    };
    [[nodiscard]] LifeBarLayout layout_life_bar(double life, int screen_w, int screen_h,
                                                double field_left);
    ```
  - Change the `render_life` declaration to
    `void render_life(double life, int screen_w, int screen_h, double field_left, GlQuadRenderer& renderer) const;`
    and rewrite its comment ("ITG-style vertical life bar on the left side ... see
    `layout_life_bar`").
  - `.cpp` geometry constants go next to the palette (same "unsourced" comment): `kLifeBarLeft =
    24.0f`, `kLifeBarThickness = 16.0f`, `kLifeBarBorder = 2.0f`, `kLifeBarFieldGap = 16.0f`,
    `kLifeBarMinThickness = 6.0f`, `kLifeBarInsetFraction = 0.2f`, `kLifeBarMinInset = 40.0f`.
  - `layout_life_bar` algorithm:
    1. If `screen_w <= 0 || screen_h <= 0`, return `{}` (`visible == false`).
    2. Clamp `life` to [0, 1] (keep the existing if/else style). Set
       `danger = clamped < kLifeDangerThreshold`.
    3. Vertical: `inset = max(h * 0.2, 40)`, `y = inset`, `bar_h = max(0, h - 2 * inset)`.
    4. Horizontal clamp: `max_right = field_left - kLifeBarFieldGap - kLifeBarBorder` is the
       largest allowed right edge of the inner bar. Start with `x = kLifeBarLeft` and
       `w = kLifeBarThickness`. If `x + w > max_right`:
       - First slide left: `x = max(kLifeBarBorder, max_right - w)`.
       - If it still does not fit, shrink: `w = max(kLifeBarMinThickness, max_right - x)`.

       Below that limit, the bar stays at `x = border` with the minimum width and may touch the
       field. That only happens for windows narrower than about 484 px (Open Question 2).
    5. `back = {x, y, w, bar_h}`, `frame = {x - border, y - border, w + 2*border, bar_h + 2*border}`,
       `fill_h = bar_h * clamped`, `fill = {x, y + bar_h - fill_h, w, fill_h}`, `visible = true`.
- **Mirror**: `src/gameplay/note_field_renderer.hpp:34-44` (struct + free function).
  `hud_renderer.cpp:165-195` (clamp + guard style).
- **Validate**: `cmake --build build -j16` (fails until Task 3 updates the caller, so do Tasks 1–3
  together before building)

### Task 2: `render_life` draws from the layout

- **File**: `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**: Replace the body with:
  - `const LifeBarLayout bar = layout_life_bar(life, screen_w, screen_h, field_left);`
  - `if (!bar.visible) return;`
  - Draw `frame` in `kLifeFrameColor`, then `back` in `kLifeBackColor`, then `fill` in
    `bar.danger ? kLifeDangerColor : kLifeFillColor`. This keeps today's draw order and colours
    (AC 3).
  - Skip the fill quad when `fill.h <= 0`, so no zero-area quad is drawn. Optional: the renderer
    tolerates it, but skipping keeps the batch tidy.
- **Mirror**: `hud_renderer.cpp:187-194`
- **Validate**: (built with Task 3)

### Task 3: `NoteField::field_left` + callers

- **Files**: `src/gameplay/note_field.hpp`, `src/gameplay/note_field_renderer.cpp`,
  `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - `note_field.hpp`, next to `field_width()`:
    ```cpp
    // Left edge of the horizontally centred field on a `screen_w`-wide screen
    // (negative when the window is narrower than the field).
    [[nodiscard]] double field_left(int screen_w) const {
        return (static_cast<double>(screen_w) - field_width()) * 0.5;
    }
    ```
  - `note_field_renderer.cpp:144`: `const double field_left = field.field_left(screen_w);`. The
    value is the same, so this is a pure refactor.
  - `gameplay_view.cpp:327`:
    `hud_.render_life(life_.life(), screen_w, screen_h, field_.field_left(screen_w), renderer);`
- **Mirror**: `note_field.hpp:66` (inline accessor)
- **Validate**: `cmake --build build -j16` builds cleanly with no new warnings, and the existing 38
  tests still pass:
  `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`

### Task 4: `hud_renderer_test` (new target)

- **Files**: `tests/hud_renderer_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Action**: CREATE / UPDATE
- **Implement**: Use the `TEST_CHECK` + `near` helpers (copy from `note_field_renderer_test.cpp:8-27`)
  and one `test_*` function per case, each printing `"  - ... ok."`. Use
  `field_left = (w - 4 * NoteSkin::kColumnWidth) * 0.5` (or construct a `NoteField` with
  `column_width = NoteSkin::kColumnWidth` and call `field_left(w)`, which also exercises Task 3).
  Cases:
  1. **Vertical, left, full**: at 1280x720 with life 1.0, `visible`, `back.h > back.w`,
     `frame.x >= 0`, `frame.x + frame.w <= 1280 * 0.5` (left half), `fill` equals `back`.
  2. **Bottom-up fill**: at life 0.5, `near(fill.h, back.h * 0.5)`,
     `near(fill.y + fill.h, back.y + back.h)` (bottom anchored), and `fill.x/w == back.x/w`. At
     life 0.25, the fill top is at 75% down the back.
  3. **Clamp**: life −0.5 gives `fill.h == 0`. Life 1.7 gives `fill.h == back.h`.
  4. **Danger**: 0.29 is `danger`. 0.3 is not (strict `<`). 1.0 is not.
  5. **Frame wraps back**: `frame` = `back` grown by 2 px on every side.
  6. **No field overlap at common sizes** (AC 2): for 1280x720, 1920x1080, 640x480, 1024x768 and
     800x600, `frame.x + frame.w + 16 <= field_left`, and `back.w == 16` (no shrinking was needed).
  7. **Below the percent text**: `frame.y >= 8 + 21` (percent text bottom) at every size above,
     and at 320x240.
  8. **Grade text clear** (AC 4): build the grade text's rect the way `HudRenderer::render` does
     (`text_width("****", 3)` centred, `y = h - 8 - 21`, height 21). Assert that it does not
     intersect `frame` at 1280x720 and 640x480. Add a local `intersects(Rect, Rect)` helper.
  9. **Narrow clamp**: at 500x400 (`field_left = 34`), `frame.x + frame.w + 16 <= 34` (it slid
     left and shrank), `back.w >= 6`, `frame.x >= 0`. At 400x400 (`field_left = -16`),
     `visible` still holds, `back.w == 6`, and nothing is NaN or negative-sized.
  10. **Degenerate**: `(w,h) = (0,720)`, `(1280,0)`, `(-1,-1)` are `!visible`.
  11. **Vertical extent scales with height**: `back.h` at 1080p is about 1.5× `back.h` at 720p
      (432 vs 648), and the bar is vertically centred
      (`near(back.y, 720 - (back.y + back.h))`).
  - Register it in `tests/CMakeLists.txt` after `score_keeper_test`, mirroring lines 125-133
    (link `blaze4k_core`).
- **Mirror**: `tests/note_field_renderer_test.cpp:1-40`, `tests/CMakeLists.txt:125-133`
- **Validate**: `cmake --build build -j16 && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build -R hud_renderer_test --output-on-failure`

### Task 5: `note_field_test` case for `field_left`

- **File**: `tests/note_field_test.cpp`
- **Action**: UPDATE
- **Implement**: Add one case next to the existing `column_x` checks (around line 395). With
  `column_width = 108`, check that `field_left(1280) == 424`, `field_left(320) == -56`, and that
  `field_left(w) + field_width() * 0.5 == w * 0.5`. Follow the file's existing helper and print style.
- **Mirror**: `tests/note_field_test.cpp:390-400`
- **Validate**: `ctest --test-dir build -R note_field_test --output-on-failure`

### Task 6: Tick the TODO entry

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: In line 38, change `- [ ]` to `- [x]`. Keep the `(#76)` suffix, following lines 30-37.
- **Validate**: `git diff TODO.md` shows a one-character change

### Task 7: Full validation and smoke

- Run the Validation block below. Expect 39/39 tests and a clean headless smoke test.

---

## Validation

```bash
# Build (incremental, Release)
cmake --build build -j16

# Lint: no linter configured in this repo; the compiler warnings from the build are the gate (no new warnings)

# Tests (expect 100% of 39)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# Debug build sanity (optional, catches assert-only issues)
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug -j16
```

## End-to-End Verification

1. **Automated (headless, required):** `hud_renderer_test` runs the real `layout_life_bar` that
   `render_life` draws from. It checks:
   - orientation and bottom-up fill (AC 1)
   - field clearance at 1280x720, 1920x1080, 640x480, 1024x768 and 800x600 (AC 2)
   - unchanged clamp and danger semantics, and frame/back nesting (AC 3)
   - grade-text clearance (AC 4)

   `note_field_test` pins the shared `field_left` formula that both the HUD and the note field
   renderer now use.
2. **App smoke (headless, required):**
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 10`
   exits 0. Never launch a windowed or interactive binary from the agent.
3. **Owner play-test (interactive; the agent does not run it):**
   - At the default 1280x720, start any song. A 16 px vertical bar sits at the left edge, filling
     from bottom to top and full at start. Miss notes and it drains downward. Below 30% it turns
     red.
   - Set `"width": 1920, "height": 1080` (then `640` × `480`) under `video` in the user
     `config.json`, or resize the window. The bar stays at the left, does not touch the receptors
     or arrows, and scales with the height.
   - Turn on Reverse scroll. The bar does not move.
   - The bottom-centre grade text is exactly where it was, and nothing overlaps it.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Windows narrower than about 484 px (allowed down to 320) leave no room left of the field | Slide left, then shrink to 6 px. Below that, accept a touch or overlap. The field itself already spills off-screen below 432 px. Documented in Open Question 2 | In scope: clamp. Out of scope: rescaling the field |
| HUD and field-renderer drift if one changes the centring formula | Single `NoteField::field_left` accessor used by both, pinned by `note_field_test` | In scope |
| Tap explosions or receptor bumps drawn slightly wider than the 108 px lane | A 16 px gap on top of `field_left`. Receptor zoom ≤ 1.0 (`gameplay_view.cpp:311-313`) | In scope (gap constant) |
| Bar top colliding with the top-left percent text in short windows | `max(0.2h, 40)` inset, tested down to 320x240 | In scope |
| `render_life` signature change breaks callers | Only one caller (`gameplay_view.cpp:327`, verified by grep) | In scope |
| Future pause overlay (#60) or song-title HUD wanting the left edge | Not present today. Revisit when #60 lands | Out of scope (flag only) |

---

## Decisions

- **Left edge, not hugging the field.** This follows the issue's technical note: the bar sits at a
  fixed 24 px margin from the left screen edge, not next to the note field.
- **Testable seam.** Geometry lives in a pure `layout_life_bar` instead of making
  `GlQuadRenderer` recordable. This matches the existing headless-test pattern (`layout_hold`) and
  needs no GL changes.
- **One new test target.** `hud_renderer_test` is a new target rather than growing
  `score_keeper_test`. `score_keeper_test` only includes the HUD header for `format_*`, and life-bar
  layout is unrelated to scoring.
- **Thickness kept at 16 px.** The bar keeps today's visual weight, satisfying "styling still works
  as before".

---

## Open Questions

1. **Bar length.** Proposed: 60% of the screen height, centred (`max(0.2h, 40)` inset top and
   bottom), which gives 432 px at 720p and 648 px at 1080p. The issue only says to "size its height
   from the screen height". A near full-height bar (for example `h - 2*48`) is an equally valid
   one-constant change if the owner prefers it.
2. **Very narrow windows (< ~484 px wide).** Proposed: clamp (slide left, shrink to 6 px), and
   otherwise accept a touch. The ACs only require common sizes (1280x720, 1920x1080, narrow 4:3
   such as 640x480), and all of them have at least 62 px clearance. Hiding the bar entirely in tiny
   windows is the alternative, but it would hide fail-state information.
3. **Exact margins (24 px left, 16 px field gap).** These are unsourced presentation values with no
   OpenITG parity requirement. Easy to tune after the owner play-test.

---

## Acceptance Criteria

- [ ] The life bar is drawn vertically at the left side during gameplay, filling bottom (empty) to top (full)
- [ ] No overlap with the note field, receptors, combo or judgment text at 1280x720, 1920x1080 and 640x480 (asserted in `hud_renderer_test`)
- [ ] Fill level, the `< 0.3` danger colour, and the frame/back styling are unchanged in colour and semantics
- [ ] The bottom-centre grade/score text is untouched and not overlapped
- [ ] `cmake --build build` passes with no new warnings, and 39/39 tests pass
- [ ] Follows existing patterns (pure layout helper, anonymous-namespace "unsourced" constants, `TEST_CHECK` tests)
