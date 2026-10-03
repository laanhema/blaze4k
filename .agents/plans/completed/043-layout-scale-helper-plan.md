# Plan: Shared 720p Layout-Scale Helper (#91)

> **Owner decision (applied at implementation, overrides this plan where they differ):** the
> narrow-aspect Open Question is resolved as **fit the whole layout**: `s = min(h / 720, w / 1280)`,
> with the `1280·s x 720·s` column centred on **both** axes (`origin.y = (h - 720·s) / 2`).
> 16:9 is unchanged; 21:9 is unchanged (height-limited); 16:10 is width-limited and gets bands above
> and below (1920x1200 → s = 1.5, origin (0, 60); 1280x800 → s = 1, origin (0, 40)). Because `s`
> now depends on width, `layout_scale_factor(w, h)` and `text_layout_scale(w, h)` take both
> dimensions, and `TextRenderer::set_window_height(h)` became `set_window_size(w, h)`. See the
> implementation report for details.

## Summary

Every `theme::layout` value (and every `TextStyle` size) is in a 1280x720 reference space. Before the Cabinet screens (#92 to #95) start placing art, they need one shared way to turn reference coordinates into window pixels. This change adds a header-only, `constexpr`, GL-free helper `src/render/theme_layout.hpp` in `blaze4k::theme`:

- `layout_scale_factor(h)` returns `s = h / 720`, or `1` when `h <= 0`.
- `layout_scale(w, h)` returns a `LayoutScale {s, origin}`. `origin.x = (w - 1280·s) / 2` is the left edge of the `1280·s`-wide content column, centred horizontally, and `origin.y = 0`. A non-positive width or height returns the identity (`s = 1`, origin `{0, 0}`).
- Members map reference values to screen pixels: `px(len)`, `x(ref_x)`, `y(ref_y)`, `point(Vec2)`, `rect(Rect)`, and `column()` (the content column as a screen `Rect`).

The note field already centres itself with `(screen_w - field_width) / 2` (`note_field.hpp:69-71`). Reference x = 640 maps to exactly `w / 2` at every size, so the two agree. `text_layout_scale()` in `ttf_font.cpp` (#90) becomes a one-line wrapper over `theme::layout_scale_factor()`, so text atlases (#90), texture draw size (#89, which already takes `s` as a parameter) and screen placement all use one definition of `s`. A new `theme_layout_test` covers 720p, 1440p, 1080p, 16:10, 21:9, the note-field centre, and degenerate sizes. No screen uses the helper yet: the consumers are #92 to #95.

## User Story

As the developer building the Cabinet screens
I want one helper that maps 1280x720 reference rects and points to window pixels
So that every screen scales and centres the same way, from 720p to ultrawide, without duplicating math or dividing by zero on a minimised window.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY (small; plus a REFACTOR of `text_layout_scale` onto it) |
| Complexity | LOW |
| Systems Affected | `src/render` (new `theme_layout.hpp`, `ttf_font.cpp` delegation, `theme.hpp` comment), `tests/` (new `theme_layout_test`), `tests/CMakeLists.txt` |
| GitHub Issue | #91 (TODO-19; blocked by #87, closed; blocks #92, #93, #94, #95) |
| Plan sequence | 043 (running plan sequence; the issue number is #91) |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build with `cmake --build build -j8` |
| C++ compiler | GCC 16.2.1 | C++20 with `-Wall -Wextra -Wpedantic` and no `-Werror`. Add **no new warnings** |
| Baseline tests | **45/45 pass** | Run on `main` @ `4707ccb` with the sandboxed command in Validation. Start green and stay green. After this change the count is **46** (`theme_layout_test` added) |
| Sandbox requirement | — | `audio_test` opens the real audio device, so **always** run ctest through the `bwrap` command |
| Reference constants | `src/render/theme.hpp:181-183` | `theme::layout::kRefWidth = 1280.0f`, `kRefHeight = 720.0f`. `theme_test.cpp:25-26` already static_asserts them |
| Existing `s` | `src/render/ttf_font.hpp:192-193`, `ttf_font.cpp:315-317` | `text_layout_scale(int h)` returns `h / 720.0f`, or `1` when `h <= 0` (hard-coded `720.0f`). Pinned by `ttf_font_test.cpp:729-733`. `TextRenderer::set_window_height` calls it (`ttf_font.cpp:809`) |
| `s` in theme textures | `src/render/theme_textures.hpp:16-17` | Takes `s` as a parameter and already cites "#91". No change needed |
| Note-field centring | `src/gameplay/note_field.hpp:66-71` | `field_left(w) = (w - field_width) / 2`. It is negative when the window is narrower than the field. The helper's column uses the same "centre, may go negative" rule |
| Window size source | `src/app/window.cpp:125`, `main.cpp:346,405` | Screens get `w, h` in **pixels** (`SDL_GetWindowSizeInPixels`) via `Screen::render(ctx, renderer, w, h)` (`screen.hpp:86-87`), which is the same height `TextRenderer` bakes at |
| Header-only constexpr precedent | `src/render/geometry.hpp`, `src/render/theme.hpp` | Both are header-only with `constexpr` helpers, so `blaze4k_core`'s source list in `CMakeLists.txt` needs **no** change |
| Spec source | `docs/cabinet-theme/IMPLEMENTATION_PLAN.md:74-78` | "`s = window_height / 720`, and a 1280·s-wide content column centred horizontally (wider windows show more background, as the game already does for the note field)" |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

---

## Patterns to Follow

### Header-only constexpr helpers in the theme namespace

```cpp
// SOURCE: src/render/theme.hpp:14-21
namespace blaze4k::theme {

// 0xRRGGBB -> straight-alpha Color (same convention as the rest of the renderer).
[[nodiscard]] constexpr Color hex(std::uint32_t rgb, float alpha = 1.0f) {
    return Color{static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, ...
```

### Pixel-space types (top-left origin)

```cpp
// SOURCE: src/render/geometry.hpp:14-26
struct Rect { float x = 0.0f; float y = 0.0f; float w = 0.0f; float h = 0.0f; };
struct Vec2 { float x = 0.0f; float y = 0.0f; };
```

### Degenerate size: safe value, no throw, no log

```cpp
// SOURCE: src/render/ttf_font.cpp:315-317
float text_layout_scale(int window_height) {
    return window_height <= 0 ? 1.0f : static_cast<float>(window_height) / 720.0f;
}
```

### Centring rule to match

```cpp
// SOURCE: src/gameplay/note_field.hpp:67-71
// Left edge of the horizontally centred field on a `screen_w`-wide screen
// (negative when the window is narrower than the field).
[[nodiscard]] double field_left(int screen_w) const {
    return (static_cast<double>(screen_w) - field_width()) * 0.5;
}
```

### Tests (plain executable, `TEST_CHECK` abort macro, compile-time `static_assert`s, explicit call list in `main`)

```cpp
// SOURCE: tests/theme_test.cpp:13-27
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

// Compile-time proof that render/theme.hpp is usable from a TU.
static_assert(blaze4k::theme::hex(0xFF0000).r == 1.0f && ...);
```

```cmake
# SOURCE: tests/CMakeLists.txt:444-459 (theme_test registration)
add_executable(theme_test theme_test.cpp)
target_link_libraries(theme_test PRIVATE blaze4k_core)
add_test(NAME theme_test COMMAND theme_test)
```

---

## Pinned Semantics (the contract the tests pin)

| Input `(w, h)` | `s` | `origin` | `column()` | Notes |
|---|---|---|---|---|
| 1280x720 | 1 | {0, 0} | {0, 0, 1280, 720} | Identity: `rect(r) == r` and `point(p) == p` **exactly** |
| 2560x1440 | 2 | {0, 0} | {0, 0, 2560, 1440} | 2x: `rect(theme::layout::kBanner) == {96, 200, 1120, 314}` exactly |
| 1920x1080 | 1.5 | {0, 0} | {0, 0, 1920, 1080} | 16:9 at a non-integer scale |
| 3440x1440 (21:9) | 2 | {440, 0} | {440, 0, 2560, 1440} | Centred, with 440 px of background on each side |
| 2560x1080 (21:9) | 1.5 | {320, 0} | {320, 0, 1920, 1080} | Centred, with 320 px on each side |
| 1920x1200 (16:10) | 1.6667 | {−106.67, 0} | {−106.67, 0, 2133.33, 1200} | The column is **wider** than the window, and the overflow is cropped equally on both sides (see Open Questions) |
| 1280x800 (16:10, Steam Deck) | 1.1111 | {−71.11, 0} | {−71.11, 0, 1422.22, 800} | Same as 16:10 above |
| `w <= 0` or `h <= 0` (0, −1, INT_MIN on either axis) | 1 | {0, 0} | {0, 0, 1280, 720} | Identity. No division, and every output is finite |

Mapping rules (identical for every size):

- `px(v) = v·s`
- `x(rx) = origin.x + rx·s`
- `y(ry) = origin.y + ry·s`
- `point(p) = {x(p.x), y(p.y)}`
- `rect(r) = {x(r.x), y(r.y), r.w·s, r.h·s}`
- `column() = {origin.x, origin.y, kRefWidth·s, kRefHeight·s}`

Invariants:

- `x(kRefWidth / 2) == w / 2`, which is the note-field centre (`NoteField::field_left(w) + field_width / 2`) for every valid size.
- `layout_scale(w, h).s == layout_scale_factor(h)` whenever `w > 0`.
- `text_layout_scale(h) == layout_scale_factor(h)` for every `h`.

There is no pixel snapping: outputs are floats, and rounding is the caller's choice.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/theme_layout.hpp` | CREATE | `LayoutScale`, `layout_scale_factor()`, `layout_scale()`. Header-only and constexpr |
| `src/render/ttf_font.cpp` | UPDATE | `text_layout_scale` delegates to `theme::layout_scale_factor` (removes the duplicated `720.0f`) |
| `src/render/ttf_font.hpp` | UPDATE | Update the comment at lines 19-22 and 192: `s` comes from `theme::layout_scale_factor` (#91) |
| `src/render/theme.hpp` | UPDATE | Change the header comment (lines 4-5) to point at `render/theme_layout.hpp` instead of "the same way the existing screens do" |
| `tests/theme_layout_test.cpp` | CREATE | Unit tests for the table above, plus static_asserts |
| `tests/CMakeLists.txt` | UPDATE | Register `theme_layout_test` after `theme_test` |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: `theme_layout.hpp`

- **File**: `src/render/theme_layout.hpp`
- **Action**: CREATE
- **Implement**:
  - Write a header comment that explains the 1280x720 reference space, `s = window_height / 720`, and the centred `1280·s` content column. Say that wider windows show more background (as the note field does) and that narrower-than-16:9 windows crop the column equally on both sides. Say that degenerate sizes give the identity. Add "Pure, GL-free; presentation only, nothing here touches the music clock or judgment path."
  - Includes: `render/geometry.hpp`, `render/theme.hpp`.
  - `namespace blaze4k::theme {`
    - `[[nodiscard]] constexpr float layout_scale_factor(int window_height)` returns `window_height <= 0 ? 1.0f : static_cast<float>(window_height) / layout::kRefHeight`.
    - `struct LayoutScale { float s = 1.0f; Vec2 origin{}; ... }` with `[[nodiscard]] constexpr` members `px(float)`, `x(float)`, `y(float)`, `point(Vec2)`, `rect(Rect)` and `column()`, following the Pinned Semantics.
    - `[[nodiscard]] constexpr LayoutScale layout_scale(int window_width, int window_height)`. If `window_width <= 0 || window_height <= 0`, return `LayoutScale{}`. Otherwise set `s = layout_scale_factor(h)` and `origin = {(static_cast<float>(w) - layout::kRefWidth * s) * 0.5f, 0.0f}`.
  - Keep everything `constexpr` so the tests can `static_assert` the 720p and 1440p cases.
- **Mirror**: `src/render/theme.hpp:14-21` (namespace, `[[nodiscard]] constexpr`), `src/render/geometry.hpp` (header-only), `note_field.hpp:67-71` (centring rule).
- **Validate**: `cmake --build build -j8` (the header is compiled by Task 2's include and Task 3's test).

### Task 2: One definition of `s`

- **Files**: `src/render/ttf_font.cpp`, `src/render/ttf_font.hpp`, `src/render/theme.hpp`
- **Action**: UPDATE
- **Implement**:
  - `ttf_font.cpp`: `#include "render/theme_layout.hpp"`, and make `text_layout_scale(int h)` return `theme::layout_scale_factor(h)`. Keep the function and its signature, because `TextRenderer::set_window_height` and `ttf_font_test.cpp:729-733` use it.
  - `ttf_font.hpp:20` and `:192`: change the comments to "`s` is the layout scale, `theme::layout_scale_factor` (#91)…" and "Same as `theme::layout_scale_factor`".
  - `theme.hpp:4-5`: change to "…multiply by `s` and place them in the centred content column with `render/theme_layout.hpp` (#91)." This is comment-only, and `theme.hpp` must **not** include `theme_layout.hpp`, which would create a cycle.
  - Do **not** change `theme_textures.hpp`, which already takes `s` as a parameter and cites #91.
- **Validate**: `cmake --build build -j8`. Then run sandboxed ctest and confirm `ttf_font_test` still passes, unchanged.

### Task 3: `theme_layout_test`

- **Files**: `tests/theme_layout_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Implement**:
  - Copy the `TEST_CHECK` macro from `theme_test.cpp:13-20`. Add `approx(a, b, eps = 1e-3f)` and `rect_eq` / `rect_approx` helpers.
  - **static_asserts**:
    - `layout_scale(1280, 720)` has `s == 1` and origin `{0, 0}`;
    - `layout_scale(2560, 1440).s == 2`;
    - `layout_scale(0, 0).s == 1`;
    - `layout_scale_factor(-5) == 1`.
  - `test_identity_720p`: `rect(r) == r` exactly for `kBanner`, `kLifeBar`, `kMedallion` and `kRecordRibbon`. `point({kInfoX, kSongTitleTop})` is unchanged, `px(44) == 44`, and `column() == {0, 0, 1280, 720}`.
  - `test_2x_1440p`: `rect(kBanner) == {96, 200, 1120, 314}` exactly, `px(kTopBarHeight) == 128`, and `column() == {0, 0, 2560, 1440}`.
  - `test_1080p`: `s == 1.5` and origin.x == 0.
  - `test_ultrawide_21_9`: for 3440x1440, origin.x == 440, `column()` is centred (`column.x * 2 + column.w == w`), and `rect(kBanner).x == 440 + 96`. Check 2560x1080 the same way, with origin.x == 320.
  - `test_16_10`: for 1920x1200 and 1280x800, `s ≈ h/720`, origin.x ≈ −106.667 and −71.111, `column().w > w`, and the column is centred (`2·origin.x + column.w ≈ w`).
  - `test_field_centre_matches_note_field`: for every size above, `x(kRefWidth / 2) ≈ w / 2`. That is the `NoteField::field_left(w) + field_width()/2` centre (`note_field.hpp:69-71`). Plain math is enough, so there is no need to construct a `NoteField`.
  - `test_degenerate`: check `(0, 0)`, `(0, 720)`, `(1280, 0)`, `(-1, 720)`, `(1280, -1)`, `(INT_MIN, INT_MIN)` and `(-1280, -720)`. Each must give the identity (`s == 1`, origin `{0, 0}`), with `std::isfinite` on every field of `rect(kBanner)` and `column()`. `layout_scale_factor(0)` and `layout_scale_factor(INT_MIN)` must both equal 1.
  - `test_large_sizes`: for `(INT_MAX, INT_MAX)` and 7680x4320, outputs are finite and 7680x4320 gives `s == 6`.
  - `test_text_scale_agrees`: for `h` in {−5, 0, 1, 720, 1080, 1200, 1440, 2160}, `text_layout_scale(h) == theme::layout_scale_factor(h)` (include `render/ttf_font.hpp`).
  - `main` calls each test in order and prints `"theme_layout_test: all passed"`, matching the house style.
  - `tests/CMakeLists.txt`: add `add_executable(theme_layout_test theme_layout_test.cpp)`, `target_link_libraries(... PRIVATE blaze4k_core)` and `add_test(NAME theme_layout_test COMMAND theme_layout_test)` after `theme_test` (line 459), with a one-line `# #91:` comment.
- **Mirror**: `tests/theme_test.cpp:1-40`, `tests/CMakeLists.txt:444-459`.
- **Validate**: `cmake --build build -j8`, then sandboxed ctest. Expect 46/46.

---

## Validation

```bash
# Build (incremental, existing Release tree); expect no warnings from the touched files
cmake --build build -j8 2>&1 | grep -iE "warning|error" ; echo "build exit: ${PIPESTATUS[0]}"

# Lint: no separate linter is configured; the -Wall -Wextra -Wpedantic build above is the lint gate

# Tests (sandboxed: hides the audio device + user PipeWire socket, no network)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
# Expect: 100% tests passed out of 46

# Static checks
git status --short                                   # expect: only the files in "Files to Change" (+ the owner's todo-stories.md, untouched)
git diff --stat -- .agents/stories/todo-stories.md   # expect: no diff from this work
grep -rn "720\.0f" src/render/ttf_font.cpp           # expect: no match (s has one definition now)
```

## End-to-End Verification

1. **Automated pure-path proof.** Run the sandboxed ctest command above.
   - `theme_layout_test` pins every row of Pinned Semantics (identity at 720p, 2x at 1440p, centring at 21:9, crop at 16:10, degenerate and huge sizes) and the note-field centre invariant.
   - `ttf_font_test` proves the `text_layout_scale` delegation did not change behaviour.
2. **Run the test binary directly** (no window or audio): `./build/tests/theme_layout_test`. Expect the "all passed" line and exit code 0. If the test binaries land elsewhere in this tree, find the path with `ctest --test-dir build -N -V | grep theme_layout`.
3. **Real app init (headless, safe for the agent).** Inside the same `bwrap` sandbox, from the repo root, run `./build/blaze-4k --headless --smoke-test 5 --data-dir "$(mktemp -d)"`. Expect the same output as on `main` (TextRenderer 4/4 fonts, one no-GL line) and "Blaze 4k shut down cleanly.". This proves the `ttf_font.cpp` include change links in the real binary.
4. **No visual change is expected.** No screen calls the helper yet. The live checks at 1280x720 and 2560x1440 against `docs/cabinet-theme/reference/` belong to #92 to #95. The agent must not launch the GUI, because it opens a window and plays audio.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Windows narrower than 16:9 (16:10, 4:3, portrait) crop the column. At 1920x1200 the 44-px-in stat panel and the 40-px-in life bar land at about −33 and −40 px | The issue and the cabinet plan specify height-based `s` and say 16:10 is a tested size. Pin the crop in the tests and raise it as an Open Question for the owner. A fit-to-width or pillar/letterbox policy would be a later, separate change inside `layout_scale()`, so every screen would pick it up at once | In scope (pin + flag); out of scope (policy change) |
| Two definitions of `s` drift apart (text atlases vs placement) | `text_layout_scale` delegates, and `test_text_scale_agrees` pins equality across heights | In scope |
| Float rounding breaks "exact identity / exact 2x" | The 720p and 1440p cases are exact in IEEE float (`s` is 1.0 or 2.0, and `x·1 + 0` is exact). The tests use `==` only for those, and `approx` for 16:10 | In scope |
| An include cycle if `theme.hpp` includes `theme_layout.hpp` | `theme_layout.hpp` includes `theme.hpp`, and the reverse is comment-only | In scope |
| Int extremes (`INT_MIN`, `INT_MAX`) | Non-positive values short-circuit before any arithmetic. `INT_MAX` → float is finite, and the test checks `isfinite` | In scope |
| Sub-pixel positions blur 1px art at odd scales (for example 1.5x) | Out of scope here. The helper returns floats, and #92 to #95 decide per element whether to round (the theme textures are baked at 2x, so this matters mainly for hairlines) | Out of scope (documented) |
| Gameplay note field is not yet scaled by `s` (it uses a fixed `kColumnWidth`) | Unchanged by this issue. The helper only guarantees the same horizontal centre. Scaling the field is part of the gameplay screen work (#93) | Out of scope |

---

## Open Questions

- **Narrow-aspect policy (16:10 and below).** The spec's `s = h / 720` crops about 107 px on each side of a 1920x1200 window, and about 71 px at 1280x800 (Steam Deck), so left-edge chrome (stat panel, life bar, banner) partly leaves the screen. **Proposed default:** implement the spec literally (height-based, centred, cropped), because the issue's AC and the cabinet plan both say so and text atlases already bake at `h / 720`. **Alternative for the owner:** `s = min(h / 720, w / 1280)` with the column vertically centred. That always fits, but it adds bands at the top and bottom on 16:10 and means text must bake at the same `s`. Non-blocking for #91: the decision can be applied later inside `layout_scale()` alone.
- **Degenerate result.** The issue only says "safe". **Proposed default:** the identity (`s = 1`, origin `{0, 0}`) when either dimension is `<= 0`, matching `text_layout_scale`'s existing `h <= 0 → 1`. Non-blocking.
- **API shape.** **Proposed default:** a free `theme::layout_scale(w, h)` that returns a small value type, computed by each screen in `render()` from the `w, h` it already gets. There is no new `ScreenContext` field: it costs a few flops and keeps the screen seam unchanged. Non-blocking.

---

## Acceptance Criteria

- [ ] `theme::layout_scale(w, h)` returns `s` and the content-column origin, and maps reference `Rect`s and points (plus lengths and the column rect) to screen pixels
- [ ] 1280x720 is the exact identity, 2560x1440 is an exact 2x, and 21:9 sizes centre the column with equal background on both sides
- [ ] `theme_layout_test` covers 720p, 1440p, 1080p, 16:10 (1920x1200 and 1280x800) and 21:9 (3440x1440 and 2560x1080), plus the note-field centre invariant
- [ ] Degenerate sizes (0 or negative width or height, INT_MIN) return the identity with finite outputs and no division
- [ ] `text_layout_scale` delegates to `theme::layout_scale_factor`, so there is one definition of `s`
- [ ] The build has no new warnings, the sandboxed `ctest` passes 46/46, and `todo-stories.md` is untouched
