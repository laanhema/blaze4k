# Code Review: feature/023-dimmed-background (Issue #23 — dimmed background + committed fallback)

**Scope**: `feature/023-dimmed-background` vs `main`, including uncommitted working-tree changes
**Recommendation**: APPROVE WITH NITS

## Summary

The change adds a gameplay background layer (`BackgroundRenderer`) that draws cover-fit behind the
note field, dimmed via a `0.35` alpha tint plus a `0.30` black scrim, and wires a committed
`assets/backgrounds/fallback.png` through `SongLibrary::set_fallback_background` *before*
`scan_directory` so art-less songs resolve to it. New `resolve_first_existing` provides
cwd-then-executable-relative asset discovery, and CMake copies `assets/` next to the binary. All
acceptance criteria are met: draw order is background → field → HUD, cover-fit math is correct and
non-distorting, the fallback is discoverable from both the repo root and `build/`, the image path
reuses the existing `Texture` untrusted-input caps, and headless paths are no-ops. The full 28-test
suite passes. Findings are limited to one release-hygiene risk (the new asset is still untracked)
and minor polish items; nothing blocks merge.

## Criteria Verification

| Criterion | Result | Evidence |
|-----------|--------|----------|
| Background behind note field, dimmed | PASS | `gameplay_view.cpp:188` draws before `field_renderer_.render` (217) and `hud_` (221-222); dim `background_renderer.cpp:15-16` |
| Committed `fallback.png` used for art-less songs; no procedural gradient | PASS | Scanner assigns `fallback_background_` (`song_library.cpp:227-230`); `BackgroundRenderer` only draws a loaded file or a flat solid (`background_renderer.cpp:82-89`). No gradient code anywhere in `src/` (only in `.agents/` plan/report prose). |
| `set_fallback_background` BEFORE `scan_directory` | PASS | `main.cpp:204` precedes the scan in `main.cpp:205-228` |
| Runtime discovery: repo root AND `build/` | PASS | Ran both; both log `Fallback background: assets/backgrounds/fallback.png`. POST_BUILD copy at `CMakeLists.txt:155-160`; exe-relative candidate `main.cpp:192` |
| Cover-fit correct / non-distorting | PASS | `background_renderer.cpp:45-73`; unit tests cover equal, 4:3-on-16:9, 16:9-on-4:3, degenerate (`background_test.cpp:31-68`) |
| Readability preserved (dim + scrim) | PASS | `background_renderer.cpp:86,92` |
| Draw order background → field → HUD | PASS | `gameplay_view.cpp:188,217,221-222` |
| 2D only, no video/3D | PASS | Only `draw_textured_quad`/`draw_quad`; no video/3D symbols |
| Untrusted-image caps reused, no crash, headless no-op | PASS | Delegates to `Texture::from_file` (size/header caps + `gl_available`, `texture.cpp:127-175`); `render` guards `is_initialized` (`background_renderer.cpp:76`) |
| No leaks; `shutdown` destroys textures | PASS | `Texture` RAII; `GameplayView::shutdown` calls `background_.shutdown()` (`gameplay_view.cpp:257`), `load()` destroys prior texture (`background_renderer.cpp:27`) |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **`assets/backgrounds/fallback.png` is still untracked** (`git status` → `?? assets/backgrounds/`). If the branch is committed/PR'd without staging this file, a fresh clone silently degrades to the scrim-only path (the code degrades gracefully, but the issue's "committed fallback" AC fails). Ensure the asset is added to the commit.

### Suggestions (Low)

2. **Stray discovery candidate** — `main.cpp:191` probes `fs::path("backgrounds") / "fallback.png"`, which matches no real layout (the other candidates are `assets/backgrounds/…`). Dead candidate; remove or document why.
3. **Doc/behaviour mismatch** — `data_paths.hpp:34-38` describes the return as the first candidate that exists as a "readable regular file", but `resolve_first_existing` (`data_paths.cpp:46-58`) only tests `is_regular_file` and swallows all errors; readability/permissions are not checked. Soften the comment or add a readability probe.
4. **Unsourced presentation constants** — `background_renderer.cpp:15-18` (`kBackgroundDim=0.35`, `kBackgroundOverlay=0.30`, `kFallbackSolid`) are not sourced to OpenITG and are not in the JSON constants table. The in-code comment already labels them unsourced/tunable (good), but flagging per the review brief: they should move into config if/when presentation tuning is exposed.
5. **Log stream inconsistency** — `background_renderer.cpp:30,40-41` emit failure/headless notices via `std::cout`, whereas the texture layer and other failure paths use `std::cerr`.
6. **Brittle asset assertion** — `tests/background_test.cpp:124` hard-codes `1024x576`; any future art replacement breaks the test. Prefer asserting `width/height > 0` within the cap.
7. **Fixed temp dir name** — `tests/background_test.cpp:89` uses a constant `blaze4k_background_test` path; parallel test runs can collide. Use a unique suffix.

## Validation Results

| Check | Status |
|-------|--------|
| Configure (`cmake -B build`) | PASS |
| Build (`cmake --build build -j4`) | PASS (clean) |
| Lint / warning budget | PASS (no warnings surfaced during build) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 28/28 |
| Headless smoke from repo root (`./build/blaze-4k --headless --smoke-test 1`) | PASS — fallback resolved, exit 0 |
| Headless smoke from `build/` (`cd build && ./blaze-4k --headless --smoke-test 1`) | PASS — fallback resolved, exit 0 |
| Demo with art (Glacier Groove) / without art (Aurora Borealis) | PASS — correct per-song path / fallback, exit 0 |

No tests were skipped or environment-guarded in this run.

## What's Good

- Clean separation: `BackgroundRenderer` owns only draw/load; the caller owns path resolution, matching the codebase's thin-wrapper style.
- `cover_uv` is a pure static function, directly unit-tested incl. degenerate dimensions, and correctly crops rather than stretches.
- Fallback wiring happens in the right lifecycle position and `set_fallback_banner` is left untouched.
- `resolve_first_existing` is exception-free (`std::error_code`) and portable.
- The committed PNG keeps the repo clone-friendly (~2.9 KiB); CMake POST_BUILD copy plus the source-tree test define make discovery robust for both runtime and out-of-tree tests.
- Graceful degradation on missing/undecodable/headless art with a readable log line, consistent with the parser-hardening discipline.

## Recommendation

Approve. Before merging, stage `assets/backgrounds/fallback.png` so the fallback is committed (issue #23's core AC). The remaining items are optional polish and can be addressed or deferred.

---

*Review performed against `feature/023-dimmed-background` incl. uncommitted working-tree changes. Scope limited to the listed files; no code was modified.*
