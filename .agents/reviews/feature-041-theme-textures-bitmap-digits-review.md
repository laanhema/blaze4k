# Code Review: feature/041-theme-textures-bitmap-digits

**Scope**: Branch `feature/041-theme-textures-bitmap-digits` vs `main` (no commits yet; all changes uncommitted/untracked), GitHub issue #89. `.agents/stories/todo-stories.md` is an unrelated owner edit and is excluded.
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the new `src/render/theme_textures.{hpp,cpp}` module (manifest parser, pure placement math, `ThemeTextures` / `BitmapDigits` owners), its wiring (`App` ownership + GL-safe shutdown order, `ScreenContext::theme`, `main.cpp`), the CMake changes and `tests/theme_textures_test.cpp`. The code meets every acceptance criterion of #89: the parser is defensive and never throws, every rect is bounds-checked, content-box placement / slice3 / slice9 / frame / tile / cropped-fill math is correct against the committed manifest, and the headless and missing-asset paths log once and stay safe. Only two Low nits.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/render/theme_textures.cpp:717-722` — digit flat fallback draws padded, overlapping boxes.**
   When a digit atlas is missing or failed to upload, each glyph's flat quad is the full padded glyph rect (`digit_glyph_rect`, e.g. `digits_chrome` '0' is 202 px wide with a 144.38 advance and `origin_x` 28), so neighbouring 25%-alpha quads overlap into darker bands, and the space glyph (110x228) draws a visible box. *Recommendation:* for the flat fallback only, draw `{pen_x, y, advance * k, h * k}` and skip `' '`, so the fallback reads as one block per character. Cosmetic, degraded path only.

2. **`src/render/theme_textures.hpp:272-274` — `load()` return contract is out of date.**
   The comment says `load` "Returns true when the manifest parsed", but `theme_textures.cpp:790-796` also returns false for a manifest that parses yet yields no textures and no fonts (for example `{}`), which is the deviation the implementation report describes. *Recommendation:* add "and lists at least one texture or digit font" to the comment.

**Noted, not a finding**
- Unknown name / missing manifest: `draw_sprite`, `draw_stretch_x` and digits draw nothing, while the rect-based helpers draw a flat quad over the caller's rect. This includes `draw_frame`, which tints the hole/banner at 25% alpha. This is the owner's decision and the plan's documented deviation.
- With a missing manifest, each distinct name logs once on first draw, in addition to the single load line. This matches the plan's "each unknown name is logged once" rule, and `warned_` is bounded by the finite set of names in code.
- Tests run against an uninitialised `GlQuadRenderer`, so the draw helpers' fallback dispatch (ring-only frame, nothing drawn for an unknown sprite) is checked only for "no crash / logs once". The pure math they call (`frame_ring_rects`, `fill_cropped_rect`, etc.) is asserted directly. The AC's required coverage is all present.

## Verification notes

- Placement math checked by hand against the committed manifest. `logo` content at `(150,168)`, k=0.5, gives image `{110,128,1060,270}`. `banner_frame` hole `{48,100,560,157}` gives image `{44,96,568,165}`. `life_frame` slice9, chip/`diff_row_*_selected` slice3 (caps measured from the image edges, padding included) and the bottom-anchored `life_stripes` UVs (`v1 = 1`, Repeat wrap) all line up with the `UVRect` top-left convention in `gl_quad_renderer.cpp:237-240`.
- `fill_cropped_piece` keeps the gradient fixed: v is cropped to `[vc1 - f*(vc1-vc0), vc1]` of the content span, and the test pins the same screen y for v=0.75 at 25% and 50% fill.
- Lifetime: `App::~App` calls `theme_textures_.shutdown()` before `window_.shutdown()` destroys the GL context. `uploads` holds `string_view` / `Slot*` into `unordered_map` nodes, which stay stable across rehash.
- Untrusted-input hardening: 1 MiB manifest cap, 256 textures / 8 fonts / 32 glyph keys, name-length cap, plain `*.png` names only (no `/ \ :`, `..`, leading `.`, control chars), ints limited to [0, 4096], finite floats, size_px checked against the PNG header, glyph rects checked against the probed atlas size. nlohmann/json v3.11.3 parses and destroys iteratively, so deep nesting cannot overflow the stack.
- Headless detection uses the same `glad_glGenTextures == nullptr` check as `texture.cpp:22`. The whole `assets/` tree is copied next to the executable (`CMakeLists.txt:162-168`), so the `default_directory()` exe-relative fallback works.
- Timing principle: everything loads at init and is presentation-only. Nothing touches the music clock or the judgment path.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j8`) | PASS |
| Warnings gate: `-fsyntax-only` recompile of `theme_textures.cpp`, `app.cpp`, `main.cpp` with the build's flags (`-std=c++20 -Wall -Wextra -Wpedantic`) plus `-Wshadow -Wconversion`; test TU with build flags | PASS (0 warnings in project code) |
| Lint | N/A (no linter configured) |
| Tests: `ctest --test-dir build --output-on-failure` inside `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net` (confirmed inside: `/dev/snd` and `/run/user/$UID` empty) | PASS, 44/44, including the new `theme_textures_test` (12 cases). No tests skipped or env-guarded. `theme_textures_test` runs headless by design (no GL context, no-op renderer), and every assertion still executes. |
| Clang | Not run (no `clang++` on host); GCC only |

## What's Good

- The three-layer split (pure parser, pure geometry, thin GL owner) makes almost everything testable without GL. The tests pin real manifest values rather than synthetic ones.
- Error handling is thorough: one bad entry or glyph is skipped with a named warning instead of discarding the whole manifest, and the malformed-input test counts exactly 17 warnings.
- Log discipline is good. Headless mode prints exactly one line (no per-PNG `[Texture]` spam), and missing or mis-sized PNGs are reported once at load rather than per frame.
- Heterogeneous-lookup maps keep the per-frame `find(string_view)` free of allocations.
- The GL-safe shutdown order in `App::~App` is explicit and commented.

## Recommendation

Ready to merge. The two Low nits (digit flat-fallback geometry, the `load()` doc comment) can be fixed now or folded into #92 to #95 when screens start using the module.
