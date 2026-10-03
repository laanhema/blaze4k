# Code Review: feature/043-layout-scale-helper (#91)

**Scope**: branch `feature/043-layout-scale-helper` vs `main` (no commits yet; all work is uncommitted/untracked). `.agents/stories/todo-stories.md` excluded (unrelated owner edit).
**Recommendation**: APPROVE WITH NITS

## Summary

Adds `src/render/theme_layout.hpp`, a header-only, `constexpr`, GL-free helper (`layout_scale_factor`, `LayoutScale{s, origin}` with `px/x/y/point/rect/column`, `layout_scale`) that maps the 1280x720 reference space to window pixels using the owner-approved fit policy `s = min(h/720, w/1280)` with the column centred on both axes. `text_layout_scale` now delegates to it and `TextRenderer::set_window_height(h)` became `set_window_size(w, h)`, so text atlases, theme textures and placement share one `s`. The code is correct, minimal and well tested; the only findings are documentation references that still describe the old height-only scale.

All four acceptance criteria of #91 are met (helper returns `s` + origin and maps rects/points; 720p identity, 1440p 2x, 21:9 centred with side bands; tests for 720p/1440p/16:10/21:9; degenerate sizes return the identity with no division). The 16:10 behaviour follows the owner decision (bands above/below), not the literal issue text, as instructed.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`docs/cabinet-theme/IMPLEMENTATION_PLAN.md:50-51`** — §2.2 still says atlases are "baked at `size_px * (window_height / 720)` and re-baked only when the window height changes", which now contradicts §2.4 as edited in this same diff (fit `s`, re-bake only when `s` changes).
   *Fix*: change to "baked at `size_px * s` (`s` from §2.4) and re-baked only when `s` changes".

2. **`docs/cabinet-theme/README.md:36`, `assets/theme/cabinet/manifest.json:9,1245,1366`** — texture/glyph draw instructions still say scale by `window_height / 720` (or `/ 1440` for @2x glyphs); on 16:10/4:3 that would oversize art relative to the fit layout used by #92-#95.
   *Fix*: reword to `s / 2` with `s = theme::layout_scale_factor(w, h)` (README), and either update the manifest `about`/`note` strings the same way or add a README line saying the manifest's `window_height` wording predates #91 if the manifest is a generated design export that should not be hand-edited.

**Noted, not a finding**: a minimised window (`w` or `h` <= 0) maps to `s = 1`, so `TextRenderer` re-bakes at 1x and again on restore. This pre-dates the change (same as `set_window_height(0)` on `main`) and the identity-on-degenerate rule is what #91 asks for; if it ever matters, `set_window_size` could ignore non-positive sizes and keep the current atlases.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j8`, GCC, Release, `-Wall -Wextra -Wpedantic`) | PASS |
| Warnings gate (re-compiled `src/render/ttf_font.cpp`, `src/app/app.cpp`, `tests/theme_layout_test.cpp`, `tests/ttf_font_test.cpp` to `/dev/null` with flags from `flags.make`) | PASS — 0 warnings (extra `-Wfloat-equal` only flags the intentional exact-equality test checks) |
| Lint | N/A — no separate linter; the warning build is the gate |
| Tests (`bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`) | PASS — 46/46 (baseline 45 + `theme_layout_test`). Sandbox confirmed: `/dev/snd` and `/run/user/$UID` empty inside it |
| Skipped / env-guarded tests | None skipped. Sandbox fallbacks that still run every assertion: `audio`-dependent tests use an injected fake clock provider ("No audio device"), and GL-dependent tests run their headless paths (`ttf_font_test` exercises `set_window_size` headless; the GL bake path is not covered by tests, but the new early-return logic sits before the GL check and is covered) |

## What's Good

- One definition of `s`: `text_layout_scale` is now a thin wrapper, `test_text_scale_agrees` pins it, and the hard-coded `720.0f` is gone from `ttf_font.cpp`.
- `layout_scale_factor` / `layout_scale` are `constexpr` with `static_assert`s for 720p, 1440p, degenerate and 16:10, and the degenerate path never divides (INT_MIN/INT_MAX covered).
- Sweep tests check invariants (fits, fills one axis, centred on both axes, reference centre == window/note-field centre) across 12 sizes rather than only spot values.
- `set_window_size` keeps atlases when the size changes but `s` does not (e.g. widening a 21:9 window), and the `sized_` flag correctly forces the first bake even at `s == 1`; `shutdown()` resets it.
- Comments in `theme.hpp`, `ttf_font.hpp`, `theme_textures.hpp` and the spec §2.4 were updated to the new definition, and the helper's header states it stays out of the music clock / judgment path.

## Recommendation

Approve. Optionally fix the two Low doc nits (stale `window_height / 720` wording) before or while #92-#95 start consuming the helper, then commit and open the PR.
