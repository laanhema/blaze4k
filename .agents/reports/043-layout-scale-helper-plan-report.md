# Implementation Report

**Plan**: `.agents/plans/completed/043-layout-scale-helper-plan.md`
**Branch**: `feature/043-layout-scale-helper`
**Status**: COMPLETE

## Summary

This change adds `src/render/theme_layout.hpp`, a header-only, `constexpr`, GL-free helper in `blaze4k::theme` that maps the 1280x720 Cabinet reference space to window pixels. It provides:

- `layout_scale_factor(w, h)`
- `LayoutScale {s, origin}` with `px`, `x`, `y`, `point`, `rect` and `column()`
- `layout_scale(w, h)`

Following the owner's decision on #91, the helper **fits the whole layout**:

- **Scale:** `s = min(h / 720, w / 1280)`.
- **Placement:** the `1280·s x 720·s` content column is centred on both axes.
- **Wide windows (21:9):** background bands appear at the sides.
- **Narrow windows (16:10, 4:3):** bands appear above and below. Nothing is ever cropped.
- **Degenerate sizes:** a width or height `<= 0` gives the identity (`s = 1`, origin `{0, 0}`) without dividing.

Text atlases now use the same `s`. `text_layout_scale(w, h)` is a wrapper over `theme::layout_scale_factor(w, h)`, and `TextRenderer::set_window_height(h)` became `set_window_size(w, h)`. As a result, text (#90), theme textures (#89, which take `s` as a parameter) and screen placement share one definition of `s`. No screen uses the helper yet; the consumers are #92 to #95.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Header-only constexpr layout-scale helper (fit policy, centred on both axes) | `src/render/theme_layout.hpp` | ✅ |
| 2 | One definition of `s`: `text_layout_scale(w, h)` delegates to `theme::layout_scale_factor`. `set_window_height` → `set_window_size`. Comment updates | `src/render/ttf_font.{hpp,cpp}`, `src/app/app.cpp`, `src/render/theme.hpp`, `src/render/theme_textures.hpp` | ✅ |
| 3 | `theme_layout_test` plus CMake registration. Existing `ttf_font_test` pins updated to the new signatures | `tests/theme_layout_test.cpp`, `tests/CMakeLists.txt`, `tests/ttf_font_test.cpp` | ✅ |
| – | Spec doc §2.4 updated to the owner decision. Owner-decision note added to the plan | `docs/cabinet-theme/IMPLEMENTATION_PLAN.md`, plan file | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j8`, `-Wall -Wextra -Wpedantic`) | ✅ exit 0, no compiler warnings or errors. The only configure output was the existing third-party CMake warnings (SDL ALSA and the glad `cmake_minimum_required` deprecation) |
| Lint (no separate linter; the warning build is the gate) | ✅ |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 100% passed, 46/46 (baseline 45, plus `theme_layout_test`) |
| `grep -rn "720\.0f" src/render/ttf_font.cpp` | ✅ no match |
| `todo-stories.md` | ✅ untouched by this work. Its diff is the owner's existing change |

### End-to-end

1. Sandboxed ctest: ✅ 46/46. `theme_layout_test` and `ttf_font_test` both pass.
2. `build/tests/theme_layout_test`, run inside the bwrap sandbox: ✅ printed `[theme_layout_test] All tests passed!` and exited 0.
3. `./build/blaze-4k --headless --smoke-test 5 --data-dir <tmp>` inside bwrap: ✅ the output matches `main`:
   - `[TextRenderer] Loaded 4/4 fonts`
   - one `No GL context available` line
   - `Blaze 4k shut down cleanly.`, exit 0
4. No GUI was launched. No visual change is expected until #92 to #95.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/theme_layout.hpp` | CREATE | +69 |
| `tests/theme_layout_test.cpp` | CREATE | +231 |
| `src/render/ttf_font.cpp` | UPDATE | +19/-7 |
| `src/render/ttf_font.hpp` | UPDATE | +18/-11 |
| `src/app/app.cpp` | UPDATE | +4/-4 |
| `src/render/theme.hpp` | UPDATE | +3/-3 |
| `src/render/theme_textures.hpp` | UPDATE (comment only) | +2/-1 |
| `tests/CMakeLists.txt` | UPDATE | +11/-0 |
| `tests/ttf_font_test.cpp` | UPDATE | +24/-10 |
| `docs/cabinet-theme/IMPLEMENTATION_PLAN.md` | UPDATE | +5/-3 |

## Deviations from Plan

1. **Owner decision: fit instead of height-only (overrides the plan).** The plan's Open Question on narrow windows was resolved as `s = min(h / 720, w / 1280)` (`layout::kRefHeight` / `kRefWidth`), with the column centred on both axes: `origin = ((w - 1280·s) / 2, (h - 720·s) / 2)`. Compared with the plan's Pinned Semantics:
   - **16:9 and 21:9:** unchanged.
   - **1920x1200:** `s = 1.5`, origin `(0, 60)`. The plan had `s = 1.667` and origin `(-106.67, 0)`.
   - **1280x800:** `s = 1`, origin `(0, 40)`. The plan had `s = 1.111` and origin `(-71.11, 0)`.
   - **Vertical placement:** `y(ry) = origin.y + ry·s`, and `origin.y` can now be non-zero.
   - **Tests:** the plan's `test_16_10` asserted cropping. It now asserts the fit, with bands above and below.
2. **`layout_scale_factor` takes `(w, h)`, not `(h)`.** `s` now depends on width, so the plan's single-argument signature could not express it. The invariant is now `layout_scale(w, h).s == layout_scale_factor(w, h)` for every input, degenerate ones included.
3. **`text_layout_scale(int h)` → `text_layout_scale(int w, int h)`, and `TextRenderer::set_window_height(int h)` → `set_window_size(int w, int h)`.** The plan said to keep the signature, but the owner asked text to use the same fit `s`. Further changes:
   - **Callers:** both `App` call sites now pass `window_.width(), window_.height()`.
   - **New members:** `TextRenderer` gains `baked_width()` and a `sized_` flag.
   - **Re-bake rule:** a resize that changes the size but not `s` (for example, widening a height-limited 21:9 window) records the new size and keeps the atlases, which depend only on `s`.
   - **Bake log:** now reads `Baked N atlases for WxH (s=…, MB, ms)` instead of `for Hp`.
   - **Tests:** the existing `ttf_font_test` checks were moved to the new signatures and extended with 16:10 and 21:9 fit cases, plus a check that a same-scale resize keeps the scale.
4. **`src/render/theme_textures.hpp`: comment-only edit.** The plan said not to touch it. Its convention comment stated `s = window_height / 720`, which the owner decision made wrong, so it now points at `theme::layout_scale_factor(w, h)`. No code changed.
5. **`docs/cabinet-theme/IMPLEMENTATION_PLAN.md` §2.4** was updated to the fit policy, so the spec matches the code. The archived plan also has an owner-decision note at the top.
6. **Extra test coverage beyond the plan:**
   - 4:3 (1024x768)
   - a non-exact 16:9 size (1366x768)
   - 640x360 and 3840x2160
   - a "fits inside the window, fills one axis, centred on both axes" sweep
   - `y(360) == h / 2`
   - `(INT_MAX, 0)` / `(0, INT_MAX)` degenerate cases
   - `(INT_MAX, 720)` / `(1280, INT_MAX)` large cases
7. **E2E step 2:** the plan said to run the test binary directly. To meet the owner's sandbox rule, it was run inside the same `bwrap` sandbox instead. The outcome is the same.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/theme_layout_test.cpp` | `static_assert`s: 720p identity, 1440p 2x, degenerate identity, 1920x1200 fit with `origin.y = 60`. Runtime tests: `test_identity_720p`, `test_2x_1440p`, `test_1080p`, `test_ultrawide_21_9` (3440x1440, 2560x1080), `test_16_10` (1920x1200, 1280x800, plus 4:3), `test_column_fits_and_fills`, `test_field_centre_matches_note_field` (x and y centre), `test_degenerate` (9 sizes including INT_MIN), `test_large_sizes` (INT_MAX, 8K), `test_text_scale_agrees` |
| `tests/ttf_font_test.cpp` (updated) | `text_layout_scale(w, h)` pins (degenerate, 16:9, 16:10, 21:9). Headless `set_window_size` checks: `baked_width` / `baked_height`, a same-scale 21:9 resize, the 16:10 fit scale, and the log line printed once |
