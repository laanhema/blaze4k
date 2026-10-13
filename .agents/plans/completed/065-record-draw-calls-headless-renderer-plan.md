# Plan: Record draw calls in the headless renderer (#141)

## Summary

Without an active OpenGL window, `GlQuadRenderer` is uninitialized and its draw calls are no-ops. Furthermore, several screens return early when `!renderer.is_initialized()`. Consequently, headless unit tests that render a screen pass even when the rendering code draws the wrong rectangles, uses incorrect colors, or skips elements entirely. 18 of the project's first 70 code review reports flagged tests that cannot fail for this reason, and the previous workaround was to manually extract individual geometric decisions into small helper functions.

This change equips the headless `GlQuadRenderer` with a recording mode: when uninitialized, every draw call (`draw_quad`, `draw_textured_quad`, `draw_quad_points`) records an immutable `RecordedQuad` struct containing corner positions, texture ID, straight-alpha tint color, per-vertex colors, and the active blend mode. The test suite is expanded to verify this recording contract in `gl_quad_renderer_test`, and `select_art_test` receives a comprehensive test of the code-drawn Edit difficulty row (`draw_difficulty_row_art`) checking all six component quads (body, four ring strips, tab) in both unselected and selected states. The rule table in `AGENTS.md` is updated to name the recorder, and the prototype patch is validated and saved beside this plan.

## User Story

As a developer writing or maintaining screen and gameplay rendering code
I want the headless quad renderer to record what it is asked to draw
So that unit tests can directly verify draw-site geometry and color decisions without requiring a GPU window or extracting single-use pure helpers

## Metadata

| Field | Value |
|---|---|
| Type | ENHANCEMENT |
| Complexity | MEDIUM |
| Systems Affected | `render/gl_quad_renderer`, `render/geometry`, `screens/select_art`, `tests/gl_quad_renderer_test`, `tests/select_art_test`, `AGENTS.md` |
| GitHub Issue | #141 ([TODO-40] Record draw calls in the headless renderer so tests can check them) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/065-record-draw-calls-headless-renderer-prototype.patch` |
| Branch (suggested) | `feature/065-record-draw-calls-headless-renderer` |
| Plan sequence | `065`. `064` was #138 (PR #149). |

---

## Environment Findings

Planned on `main` @ `eb156e1`, working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| Build | `build/` (Release, `-DBLAZE_WERROR=ON`, GCC) | Preconfigured on host |
| Baseline | `scripts/check.sh` prints `CHECK: PASS`, 53 of 53 tests | Verified at planning time |
| Prototype run | Lint passes (`LINT: PASS`), builds with zero warnings, 53 of 53 tests pass | Verified in detached scratch worktree |
| Audio in tests | Silent null audio device | No real sound hardware opened |
| Forward references to #141 | `TODO.md:56`, `.agents/issues/todo-issues.md:1700-1736` | Specifies recording corners, texture, colour, blend mode, and Edit row test |
| Existing draw counter | `NoteFieldRenderer::last_drawn_quads()` (`src/gameplay/note_field_renderer.hpp:61`) | Existing narrow counter; the recorder generalizes this |

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Record quads automatically when `GlQuadRenderer` is uninitialized (`!initialized_`) | Enables zero-setup recording in headless tests. In the real game, `init()` sets `initialized_ = true`, which takes the GL path with zero recording overhead or allocation cost. |
| D2 | `begin()` clears the recorded list and resets blend mode to `Alpha`; `clear_recorded()` provides manual clearing | Matches the GL batch lifecycle where `begin()` starts a fresh frame batch. |
| D3 | Store both primary straight-alpha `Color color` and `std::array<Color, 4> colors` in `RecordedQuad` | `draw_quad` and `draw_textured_quad` pass a single straight-alpha tint; `draw_quad_points` passes per-corner colors. Storing `color` (`colours[0]`) makes single-color assertions concise while preserving per-vertex color inspection. |
| D4 | Place `rect_corners` in `src/render/geometry.hpp` | Avoids duplicate axis-aligned corner construction between `gl_quad_renderer.cpp` and `tests/gl_quad_renderer_test.cpp`, adhering to the single-definition rule. |
| D5 | Promote `quad_band` from `select_art.cpp` anonymous namespace to `blaze4k::select_art` in `select_art.hpp` | The Edit row draw code computes the top and bottom ring strips using `quad_band(frame, 0.0f, t)` and `quad_band(frame, 1.0f - t, 1.0f)`. Exposing this pure function allows tests to check exact geometric strips without reimplementing the math. |
| D6 | Defer modifying the 21 other call sites across 8 files that check `renderer.is_initialized()` | `draw_difficulty_row_art` in `select_art.cpp` does not check `is_initialized()`, so it works immediately. Loosening or replacing `is_initialized()` across other screens (like `ttf_font.cpp` or `results_art.cpp`) is deferred until those screens receive draw-site tests (see Open Question 1). |

---

## Patterns to Follow

### Pure Quad Helper
```cpp
// SOURCE: src/screens/select_art.hpp:237
[[nodiscard]] std::array<Vec2, 4> skewed_quad(const Rect& rect, float skew);
```

### Equality and Approximation in Tests
```cpp
// SOURCE: tests/select_art_test.cpp:89-103
bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}
bool same_color(blaze4k::Color a, blaze4k::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
```

### Uninitialized Renderer No-op Guard
```cpp
// SOURCE: src/render/gl_quad_renderer.cpp:263-266
void GlQuadRenderer::draw_quad(const Rect& rect, Color color) {
    if (!initialized_) {
        return;
    }
```

---

## Files to Change

| File | Action | Purpose | Covered by Patch |
|---|---|---|---|
| `src/render/geometry.hpp` | UPDATE | Add `rect_corners` helper function | Yes |
| `src/render/gl_quad_renderer.hpp` | UPDATE | Declare `struct RecordedQuad`, `recorded_quads()`, and `clear_recorded()` | Yes |
| `src/render/gl_quad_renderer.cpp` | UPDATE | Implement quad recording in `begin`, `set_blend_mode`, `draw_quad`, `draw_textured_quad`, `draw_quad_points`, `end` | Yes |
| `src/screens/select_art.hpp` | UPDATE | Declare `quad_band` helper | Yes |
| `src/screens/select_art.cpp` | UPDATE | Define `quad_band` in `namespace blaze4k::select_art` | Yes |
| `tests/gl_quad_renderer_test.cpp` | UPDATE | Replace `test_headless_noop` with `test_headless_recording`, reuse `blaze4k::rect_corners` | Yes |
| `tests/select_art_test.cpp` | UPDATE | Add `test_edit_row_recorded_quads` checking 6 quads, colors, and sensitivity | Yes |
| `AGENTS.md` | UPDATE | Update the draw-call decision rule in table to name the headless recorder | Yes |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Add `rect_corners` helper to `geometry.hpp`
- **File**: `src/render/geometry.hpp`
- **Action**: UPDATE
- **Implement**: Add `[[nodiscard]] constexpr std::array<Vec2, 4> rect_corners(const Rect& rect)` to `blaze4k::` returning `{TL, TR, BR, BL}` points.
- **Mirror**: `src/render/geometry.hpp:43-55`
- **Validate**: `scripts/check.sh`

### Task 2: Implement quad recording in `GlQuadRenderer`
- **File**: `src/render/gl_quad_renderer.hpp`, `src/render/gl_quad_renderer.cpp`
- **Action**: UPDATE
- **Implement**: Define `struct RecordedQuad { std::array<Vec2, 4> corners; unsigned int texture; Color color; std::array<Color, 4> colors; BlendMode blend_mode; }`. In `GlQuadRenderer`, add `recorded_quads()` returning `std::span<const RecordedQuad>` and `clear_recorded()`. When `!initialized_`, record in `draw_quad`, `draw_textured_quad`, and `draw_quad_points`; clear on `begin()`; update `blend_mode_` on `set_blend_mode()` and reset on `end()`.
- **Mirror**: `src/render/gl_quad_renderer.hpp:30-48`, `src/render/gl_quad_renderer.cpp:263-295`
- **Validate**: `scripts/check.sh`

### Task 3: Expose `quad_band` pure helper in `select_art`
- **File**: `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**: Move `quad_band` from anonymous namespace to `blaze4k::select_art` and declare in `select_art.hpp`.
- **Mirror**: `src/screens/select_art.hpp:237`, `src/screens/select_art.cpp:238-242`
- **Validate**: `scripts/check.sh`

### Task 4: Verify headless recorder contract in `gl_quad_renderer_test`
- **File**: `tests/gl_quad_renderer_test.cpp`
- **Action**: UPDATE
- **Implement**: Replace `test_headless_noop` with `test_headless_recording`, verifying recorded quad count, corners, color, texture ID, and blend mode across multiple draw calls. Replace local `rect_corners` with `using blaze4k::rect_corners;`.
- **Mirror**: `tests/gl_quad_renderer_test.cpp:185-200`
- **Validate**: `ctest --test-dir build --output-on-failure -R gl_quad_renderer_test`

### Task 5: Add Edit difficulty row quad test in `select_art_test`
- **File**: `tests/select_art_test.cpp`
- **Action**: UPDATE
- **Implement**: Add `test_edit_row_recorded_quads()` to test `draw_difficulty_row_art` for both unselected and selected states. Assert that exactly 6 quads are recorded: body (0), top strip (1), bottom strip (2), left strip (3), right strip (4), tab (5). Verify each quad's corners, color, texture ID (0), and blend mode (`Alpha`). Assert sensitivity: changing strip color or strip rectangle fails the match.
- **Mirror**: `tests/select_art_test.cpp:822-868`
- **Validate**: `ctest --test-dir build --output-on-failure -R select_art_test`

### Task 6: Update `AGENTS.md` rule table
- **File**: `AGENTS.md`
- **Action**: UPDATE
- **Implement**: Update line 129 ("A decision made inside a draw call gets a pure helper that the test calls.") to name the headless recorder (`GlQuadRenderer::recorded_quads`).
- **Mirror**: `AGENTS.md:129`
- **Validate**: `scripts/lint.sh`

### Task 7: Full test suite and lint validation
- **File**: N/A
- **Action**: VALIDATE
- **Implement**: Run `scripts/check.sh` to ensure all 53 tests pass and lint succeeds.
- **Validate**: `scripts/check.sh`

---

## Validation

```bash
# Type check / Build with warnings as errors
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBLAZE_WERROR=ON
cmake --build build -j"$(nproc)"

# Lint
scripts/lint.sh

# Run all tests
scripts/check.sh
```

## End-to-End Verification

1. Run `scripts/check.sh` to confirm all 53 unit tests pass.
2. Run `./build/blaze-4k --songs tests/fixtures/reference_pack` (or invoke `/verify song-select`) to visually verify that Song Select displays and difficulty rows render identically to the reference appearance.

---

## Risks

| Risk | Mitigation | In / Out of Scope |
|---|---|---|
| Recording overhead affects frame rate in real gameplay | In production, `GlQuadRenderer::init()` succeeds, setting `initialized_ = true`. All draw calls take the existing hardware GL path; no quad recording or allocations occur. | In scope (verified by design) |
| Future tests on other screens return early on `is_initialized()` | Call sites in other screens can be loosened or given `can_draw()` predicates as those screens get test coverage. | Out of scope for #141 (scoped strictly to `GlQuadRenderer` and Edit row) |
| Non-solid draws (sprites, fonts) in headless mode | The issue explicitly establishes that textures are invalid without GL and text renderer checks `gl_present()`, so sprites and text stop before reaching the renderer. | Out of scope (documented assumption) |

---

## Open Questions

- **Question 1: Migration of remaining 21 `is_initialized()` call sites across 8 files**
  - *Context*: 21 call sites in `results_art.cpp`, `hud_renderer.cpp`, `gameplay_view.cpp`, `note_field_renderer.cpp`, etc., return early on `!renderer.is_initialized()`.
  - *Options*:
    1. (Proposed default) Keep `is_initialized()` strictly reflecting OpenGL context initialization and leave existing guards unchanged in this PR. `select_art.cpp` does not use `is_initialized()` and works immediately. Migrate other screens when tests for them are added.
    2. Change `is_initialized()` to return `initialized_ || is_recording()`.
    3. Introduce `can_draw()` predicate across all 21 call sites now.
  - *Rationale*: Option 1 limits blast radius and prevents unintended execution in complex screen paths that might attempt invalid texture lookups.

---

## Acceptance Criteria

- [ ] The headless renderer records each quad it is asked to draw (corners, texture, colour, blend mode), and a test can read the recording
- [ ] A test of the code-drawn Edit difficulty row checks the recorded quads (body, four ring strips, tab, each with its colour), selected and not selected
- [ ] That test fails when a ring strip gets the wrong rectangle or colour
- [ ] The real game draws the same as before, shown by a `/verify` screenshot of song select
- [ ] The "review only" row about draw-call decisions in the `AGENTS.md` rule table is updated to name the recorder
- [ ] All 53 tests pass under `scripts/check.sh` with zero warnings under `-DBLAZE_WERROR=ON`
- [ ] `scripts/lint.sh` passes with zero violations
