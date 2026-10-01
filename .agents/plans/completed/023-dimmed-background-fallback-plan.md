# Plan: Dimmed Simfile Background During Gameplay with Fallback (D1)

## Summary

Render the selected song's simfile background behind the note field during gameplay, dimmed so
arrows stay readable, and draw a bundled fallback background for songs with no art. The path is
already resolved by the A7 scanner into `Song::resolved_background_path`
(`src/chart/song.hpp:22`); `GameplayScreen::enter` already holds the `Song`
(`gameplay_screen.cpp:25`). A new **procedural** `BackgroundRenderer` (`src/render/`) owns the
song image plus a compiled-in fallback gradient (mirroring `NoteSkin`'s "colored textured quads
only, no image files", `noteskin.cpp:11-40`). `GameplayView` gains an optional `background_path`
init argument, draws the background first in `render()` (before receptors/notes/HUD), and applies a
fixed dim + dark scrim for contrast. 2D textured quads only — no video, no 3D (locked decision).

## User Story

As a player
I want the simfile's background image shown dimmed behind the note field during gameplay
So that songs have visual identity without distracting from the arrows.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | LOW / SMALL |
| Systems Affected | `src/render/` (new `BackgroundRenderer` + cover-fit math), `src/gameplay/` (`GameplayView` init/render/shutdown), `src/screens/gameplay_screen.cpp` (pass path), `src/main.cpp` (demo path), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #23 ([D1]) |
| PRD refs | §5 story 7, §12 Phase D ("Background image rendering (dimmed), fallback background"), §15 locked decisions ("2D simfile backgrounds; no video/dancers"), §4 In Scope ("simfile background images (dimmed)") |
| Depends on | B3 (#? note field rendering, merged), A7 (#? song library scanner, `set_fallback_background` exists), C1 (#16 screens), C7 (#22 results) — all merged |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` for g++/Fedora 44 |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` (no `-Werror`) from root CMake |
| Cores | 16 | `-j16` safe |
| Baseline tests | **27/27 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 27" (0.39 s), run this session |
| Background path | `src/chart/song.hpp:22` | `resolved_background_path` + `has_custom_background`, filled by the scanner (`song_library.cpp:216-229`) |
| Fallback seam (dead) | `src/chart/song_library.hpp:20-24`, `song_library.cpp:227-229` | `set_fallback_background()` exists but **`main` never calls it**; no global fallback asset in `assets/` (only `assets/data/judgment_constants.json`) |
| Render entry | `src/gameplay/gameplay_view.cpp:180-218` | draws field then HUD; background must be inserted **first** |
| Renderer | `src/render/gl_quad_renderer.hpp:29-36` | `draw_quad` (solid) + `draw_textured_quad` (UV, tint); every call is a no-op when uninitialized (headless) |
| Texture factory | `src/render/texture.hpp:39-56` | `Texture::from_file` (PNG/JPG/BMP, 4096px / 16 MiB caps, headless → invalid), `from_rgba`, `solid` |
| Procedural precedent | `src/gameplay/noteskin.cpp:11-40`; `noteskin.hpp:17-18` | "no image files"; `init()` logs + returns false without GL; safe to draw regardless |
| Palette precedent | `src/gameplay/hud_renderer.cpp:14-29` | "Blaze 4k presentation, unsourced; no OpenITG parity requirement" constants |
| Fixtures | `tests/fixtures/reference_pack/Blaze Pack/` | `Glacier Groove/` + `Blaze Anthem/` + `Northern Lights/` have `bg*.png`; `Aurora Borealis/` has none (exercises fallback) |
| Test idiom | `tests/texture_test.cpp:12-19`; `tests/CMakeLists.txt:226-234`; `tests/score_keeper_test.cpp:638-661` | `TEST_CHECK` abort; one target per test; headless `GameplayView` construction pattern |

**Start green, stay green:** 27 tests pass; this plan adds **1** target (`background_test`) → **28
expected**. No `src/chart/*`, `src/audio/*`, `src/timing/*`, `src/gameplay/judgment*`,
`src/gameplay/score_keeper.*`, `src/gameplay/life_keeper.*`, `src/gameplay/note_field*`,
`src/gameplay/hud_renderer.*`, or `src/screens/*` (other than `gameplay_screen.cpp`) changes.

---

## Pinned Semantics

Authority: **PRD §4/§5 story 7/§12 Phase D/§15**, the existing **`Texture`/`GlQuadRenderer`**
contracts, and the **`NoteSkin` procedural-art pattern**. No OpenITG parity value is required for
the dim level or fallback look ("Blaze 4k presentation, unsourced", per `hud_renderer.cpp:14-17`);
only the 2D-only constraint is locked.

### Background sources (all resolved, no new scanner work)

- Song art: `Song::resolved_background_path` (scanner resolves the `background`/`bg` tag
  case-insensitively, then folder keywords `bg`/`background` — `song_library.cpp:216-223`).
- Fallback: **compiled-in** procedural gradient texture built by `BackgroundRenderer::init()` on a
  real GL context (like `NoteSkin::init()` building `Texture::solid`). No committed image, no
  scanner wiring, no asset-copy step. (See OQ1 if a real `assets/` image is required.)

### Cover-fit layout (pure, testable)

Backgrounds are aspect-scaled to **cover** the framebuffer (fill + crop, never stretch/letterbox).
Given `sw,sh` (screen) and `tw,th` (texture), with `screen_aspect = sw/sh`, `tex_aspect = tw/th`:

```cpp
// src/render/background_renderer.hpp
// Full-screen rect {0,0,sw,sh} is drawn with these UVs; crops the longer axis.
[[nodiscard]] UVRect cover_uv(int screen_w, int screen_h, int tex_w, int tex_h);
//   tex_aspect >  screen_aspect  -> crop horizontally: vis = screen_aspect/tex_aspect
//                                   u0=(1-vis)/2, u1=(1+vis)/2, v0=0, v1=1
//   tex_aspect <= screen_aspect  -> crop vertically:   vis = tex_aspect/screen_aspect
//                                   v0=(1-vis)/2, v1=(1+vis)/2, u0=0, u1=1
//   any non-positive dimension   -> UVRect{} (full texture)
```
Verified by hand: 4:3 texture on a 16:9 screen → `v0=0.125, v1=0.875, u=0..1` (no distortion).

### Dim + contrast (fixed presentation constants, tunable until D2)

```cpp
// src/render/background_renderer.cpp (presentation, unsourced; like hud_renderer.cpp:14-17)
constexpr float kBackgroundDim = 0.35f;            // image alpha over the black clear
constexpr Color kBackgroundOverlay{0.f, 0.f, 0.f, 0.30f}; // extra scrim, guarantees note contrast
constexpr int   kFallbackSize = 64;                // procedural gradient resolution
constexpr Color kFallbackTop{0.10f, 0.14f, 0.22f, 1.0f};
constexpr Color kFallbackBottom{0.02f, 0.03f, 0.06f, 1.0f};
```
Effective background brightness ≤ `0.35 * 0.70 ≈ 0.25`; receptors/notes/HUD draw at full
brightness on top, so note-field readability is preserved (AC3). `draw_background` is a no-op when
the renderer is uninitialized (headless), matching every other renderer in the repo.

### Draw order (locked by the issue: "behind the receptors")

`GameplayView::render` (`gameplay_view.cpp:180-218`) becomes: **background** → field
(receptors, bodies, tails, heads, mines) → HUD → life bar.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Background image path (`resolved_background_path`) | `src/chart/song.hpp:22`, resolved `song_library.cpp:216-229` | Sourced (reused, A7) |
| Image decode caps (4096px, 16 MiB, untrusted) | `src/render/texture.cpp:16-18,32-45,94-108` | Sourced (reused) |
| 2D-only backgrounds (no video/3D) | PRD §15 locked decisions; issue AC4 | Sourced |
| Cover-fit (fill + crop) vs stretch/letterbox | PRD/issue silent | **Design decision — OQ3** |
| Dim alpha `0.35` + scrim `0.30` | PRD/issue silent ("reduced brightness") | **Design decision — OQ2** |
| Fallback = compiled-in procedural gradient vs `assets/` image | AGENTS.md/issue note say "assets/ (fallback bg)"; `NoteSkin` precedent says no image files | **Design decision — OQ1** |

No judgment windows, DP weights, grade boundaries, life deltas, or timing values are introduced.

---

## Patterns to Follow

### Procedural texture init (no image files, headless-safe)
```cpp
// SOURCE: src/gameplay/noteskin.cpp:22-40
bool NoteSkin::init() {
    if (glad_glGenTextures == nullptr) {
        std::cerr << "[NoteSkin] No GL context available; noteskin disabled\n";
        return false;
    }
    white_ = Texture::solid(Color{1.0f, 1.0f, 1.0f, 1.0f});
    ...
}
```

### Untrusted image load (reuse caps/guards)
```cpp
// SOURCE: src/render/texture.hpp:44-49
static Texture from_file(const std::string& path); // invalid on empty/missing/oversize/headless, never throws
```

### Batched quad draw + headless no-op
```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:29-33
void draw_quad(const Rect& rect, Color color);
void draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color);
```

### Gameplay render order (insert background first)
```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:199-217
field_.compute_visible(...);
field_renderer_.render(field_, visible_items_, screen_w, screen_h, skin_, renderer);
hud_.render(score_.state(), screen_w, screen_h, renderer);
hud_.render_life(life_.life(), screen_w, screen_h, renderer);
```

### Screen → view asset handoff
```cpp
// SOURCE: src/screens/gameplay_screen.cpp:28-36
const std::string audio_path = song.resolved_music_path;
active_ = view_.init(chart, constants, audio_path, ctx.play_request->options);
```

### Test idiom + registration
```cpp
// SOURCE: tests/texture_test.cpp:12-19; tests/CMakeLists.txt:226-234
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
add_executable(background_test background_test.cpp)
target_link_libraries(background_test PRIVATE blaze4k_core)
add_test(NAME background_test COMMAND background_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/background_renderer.hpp` | CREATE | `BackgroundRenderer` + pure `cover_uv`; no SDL includes beyond `gl_quad_renderer`/`texture` |
| `src/render/background_renderer.cpp` | CREATE | Procedural fallback gradient (`from_rgba`), song image (`from_file`), cover-fit dimmed draw + scrim |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Add `BackgroundRenderer background_;`; add defaulted `background_path` param to `init` |
| `src/gameplay/gameplay_view.cpp` | UPDATE | `init`: `background_.init(); background_.load(background_path);`; `render`: draw background first; `shutdown`: `background_.shutdown();` |
| `src/screens/gameplay_screen.cpp` | UPDATE | Pass `song.resolved_background_path` to `view_.init(...)` |
| `src/main.cpp` | UPDATE | `--gameplay-demo`: resolve the simfile's background (if present) and pass it (parity; optional) |
| `CMakeLists.txt` | UPDATE | Add `src/render/background_renderer.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `background_test` |
| `tests/background_test.cpp` | CREATE | `cover_uv` math (equal/tall/wide/degenerate), headless init/load/render no-crash, path handling |

No changes to `src/chart/*`, `src/audio/*`, `src/timing/*`, `src/gameplay/judgment*`,
`score_keeper.*`, `life_keeper.*`, `note_field*`, `hud_renderer.*`, or other `src/screens/*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: `BackgroundRenderer` (procedural fallback + cover-fit)

- **File**: `src/render/background_renderer.hpp`, `src/render/background_renderer.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  // background_renderer.hpp
  #pragma once
  #include <string>
  #include "render/geometry.hpp"
  #include "render/texture.hpp"
  namespace blaze4k {
  class GlQuadRenderer;
  class BackgroundRenderer {
  public:
      BackgroundRenderer() = default;
      ~BackgroundRenderer();
      BackgroundRenderer(const BackgroundRenderer&) = delete;
      BackgroundRenderer& operator=(const BackgroundRenderer&) = delete;

      bool init();                       // builds the fallback gradient; false without GL (logs)
      void shutdown();
      void load(const std::string& path);// empty/failed -> image_ invalid, fallback used
      void render(GlQuadRenderer& renderer, int screen_w, int screen_h) const;

      [[nodiscard]] bool has_image() const { return image_.valid(); }
      [[nodiscard]] const Texture& image_texture() const { return image_; }
      [[nodiscard]] const Texture& fallback_texture() const { return fallback_; }
      [[nodiscard]] static UVRect cover_uv(int screen_w, int screen_h, int tex_w, int tex_h);
  private:
      Texture image_;
      Texture fallback_;
  };
  } // namespace blaze4k
  ```
  - `init()`: return `true` if `fallback_.valid()` already; if `glad_glGenTextures == nullptr`
    log `[BackgroundRenderer] No GL context available; fallback background disabled` and return
    `false`. Otherwise build `kFallbackSize × kFallbackSize` RGBA8 bytes with a vertical
    `kFallbackTop → kFallbackBottom` gradient and `Texture::from_rgba`; return `fallback_.valid()`.
    (Include `<glad/glad.h>` only for the `glad_glGenTextures` guard, exactly like `noteskin.cpp`.)
  - `shutdown()`: `image_.destroy(); fallback_.destroy();`.
  - `load(path)`: `image_.destroy(); if (path.empty()) { log using fallback; return; }`
    `image_ = Texture::from_file(path);` log `[BackgroundRenderer] background '<path>' (WxH)` when
    valid, else `... using fallback` (from_file already logs its own failure).
  - `cover_uv`: implement exactly per **Pinned Semantics**; return `UVRect{}` on any non-positive
    dimension.
  - `render`: guard `!renderer.is_initialized() || screen_w <= 0 || screen_h <= 0` (return). Choose
    `const Texture& tex = image_.valid() ? image_ : fallback_;`. If `tex.valid()` draw
    `renderer.draw_textured_quad(Rect{0,0,(float)screen_w,(float)screen_h}, tex, cover_uv(...), with_alpha(Color{1,1,1,1}, kBackgroundDim))`.
    Else (headless/fallback invalid) `renderer.draw_quad(Rect{0,0,(float)screen_w,(float)screen_h}, kFallbackBottom)`
    (a no-op with an uninitialized renderer). Always finish with the contrast scrim
    `renderer.draw_quad(Rect{0,0,(float)screen_w,(float)screen_h}, kBackgroundOverlay)`.
- **Mirror**: `src/gameplay/noteskin.cpp:11-40` (procedural init/shutdown); `hud_renderer.cpp:14-29` (palette); `gl_quad_renderer.hpp:29-33`.
- **Validate**: `cmake --build build -j16` (after Task 4 registers the `.cpp`).

### Task 2: `GameplayView` background integration

- **File**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**:
  - `.hpp`: `#include "render/background_renderer.hpp"`; add member `BackgroundRenderer background_;`
    near `NoteSkin skin_;`. Change the signature to
    `bool init(const Chart&, const JudgmentConstants&, const std::string& audio_path, const GameplayOptions&, const std::string& background_path = "");`
    (default keeps every existing test call `init(chart,k,"",options)` source-compatible).
  - `.cpp::init`: after `skin_.init();` add `background_.init(); background_.load(background_path);`.
  - `.cpp::render`: after the guard (`!ready_ || !renderer.is_initialized() || ...`) and **before**
    `field_.compute_visible(...)`, insert `background_.render(renderer, screen_w, screen_h);`.
  - `.cpp::shutdown`: add `background_.shutdown();` alongside `skin_.shutdown();`.
- **Mirror**: `gameplay_view.cpp:65-68,180-218,230-256`; `gameplay_view.hpp:43-44,75-79`.
- **Validate**: `cmake --build build -j16`; `./build/tests/score_keeper_test` and
  `./build/tests/life_keeper_test` stay green (headless fallback path).

### Task 3: Pass the resolved background from the shell and demo

- **File**: `src/screens/gameplay_screen.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `gameplay_screen.cpp:36`: `active_ = view_.init(chart, constants, audio_path, ctx.play_request->options, song.resolved_background_path);`
  - `main.cpp` `--gameplay-demo` branch (`:242-266`): resolve
    `fs::path(demo_path).parent_path() / parser.metadata().background_path` when non-empty and
    `fs::exists`; pass it to `gameplay.init(...)` as the 5th argument (else `""`). No library/scan
    change — the scanner's dead `set_fallback_background` seam is deliberately left for later (OQ4).
- **Mirror**: `gameplay_screen.cpp:28-36`; `main.cpp:242-250` (existing music-path resolution).
- **Validate**: `cmake --build build -j16`.

### Task 4: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/render/background_renderer.cpp` to the `blaze4k_core` source list
  (`CMakeLists.txt:95-99` area); append a `background_test` block mirroring
  `tests/CMakeLists.txt:226-234`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 5: `background_test`

- **File**: `tests/background_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`, no GL/window; bring up `GlQuadRenderer renderer;` left uninitialized):
  1. **cover_uv equal aspect** — `cover_uv(1280,720,1280,720)` → full `{0,0,1,1}`.
  2. **cover_uv tall texture (4:3 on 16:9)** — `cover_uv(1280,720,640,480)` → `u0==0,u1==1`,
     `v0≈0.125,v1≈0.875` (crop vertical; assert to 1e-6).
  3. **cover_uv wide texture (16:9 on 4:3)** — `cover_uv(800,600,1280,720)` → `v0==0,v1==1`,
     `u0≈0.125,u1≈0.875` (crop horizontal).
  4. **cover_uv degenerate** — any zero/negative dimension → `UVRect{}` (full).
  5. **Headless lifecycle** — default-construct `BackgroundRenderer b;` `b.init()` does not crash
     (returns false without GL); `b.load("")`, `b.load("/no/such/file.png")` do not crash and leave
     `has_image()==false`; `b.render(renderer, 1280, 720)` and `b.shutdown()` are safe.
- **Mirror**: `tests/texture_test.cpp:12-19` (macro/idiom); `tests/note_field_test.cpp` (pure assertions).
- **Validate**: `./build/tests/background_test` → exit 0.

### Task 6: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build and run everything; check for new warnings.
- **Validate**: see **Validation** below (`ctest` → **28/28**, no warnings).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 28/28: 27 existing + background_test)
ctest --test-dir build --output-on-failure

# Explicit new/affected tests
./build/tests/background_test
./build/tests/score_keeper_test
./build/tests/life_keeper_test
./build/tests/results_screen_test

# 2D-only / no forbidden includes in the new render module
rg -n "video|3D|glad" src/render/background_renderer.hpp
# expected: no matches (glad only in the .cpp for the glGenTextures guard)

# Warning budget
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless and non-blocking (no window/GL/audio device required) and use `--data-dir`
so the developer's real `data/` is untouched.

1. **Song with art** (AC1): the fixture `Glacier Groove` has `bg.png` in-folder. The demo path
   resolves it; the log shows `[BackgroundRenderer] background '<...>/bg.png' (WxH)`.
   ```bash
   ./build/blaze-4k --headless --smoke-test 30 \
     --gameplay-demo "tests/fixtures/reference_pack/Blaze Pack/Glacier Groove/Glacier Groove.sm"
   # exit 0; background loaded log; note field + HUD still render (headless no-ops)
   ```
2. **Song without art** (AC2): `Aurora Borealis` has no `bg*.png`; the log shows the procedural
   fallback being used, and gameplay runs to the smoke-test exit.
   ```bash
   ./build/blaze-4k --headless --smoke-test 30 \
     --gameplay-demo "tests/fixtures/reference_pack/Blaze Pack/Aurora Borealis/Aurora Borealis.sm"
   # exit 0; "using fallback" logged
   ```
3. **Contrast / draw order** (AC3): `cover_uv` unit tests (Task 5) pin the non-distorting layout;
   the dim (`0.35`) + scrim (`0.30`) constants bound background brightness while notes/HUD draw
   after and at full brightness. Visual confirmation requires a display and is a manual developer
   check (out of automated scope).
4. **2D-only** (AC4): `background_renderer.*` draws textured/solid quads only; `rg` check in
   Validation.
5. **Regression**: `ctest --test-dir build --output-on-failure` → 28/28; the `--gameplay-demo`
   path, `select_screen_test`, `score_keeper_test`, `life_keeper_test`, and `results_screen_test`
   stay green.
6. `git status` shows new files under `src/render/` and `tests/`, edits limited to
   `gameplay_view.*`, `gameplay_screen.cpp`, `main.cpp`, and the CMake files.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Background too bright/visually noisy, hurting note readability | Fixed `kBackgroundDim` (0.35) + `kBackgroundOverlay` (0.30) scrim; draw notes/HUD after; constants tunable until D2 | **In scope** |
| Cover-fit math crops the wrong axis or distorts | Pure `cover_uv` with four unit-test cases including the hand-verified 4:3-on-16:9 case | **In scope** |
| Missing/undecodable/oversized background crashes or leaks | Reuse `Texture::from_file` (empty path, missing file, 4096px/16 MiB caps, headless → invalid, never throws); fallback texture covers the invalid case | **In scope** |
| Headless build/test hits GL calls | `init()` guards on `glad_glGenTextures` and returns false (like `NoteSkin`); all draw calls are no-ops with an uninitialized renderer | **In scope** |
| Song has no background and no global fallback asset configured | `BackgroundRenderer` always has the compiled-in procedural gradient, so a background always renders | **In scope** |
| Double decode cost (info probe + pixel decode) for large art | Bounded by the existing caps; decode happens once per run in `load()` | **In scope** — accepted |
| Replay/enter-exit leaks textures | `GameplayView::shutdown` destroys `background_`; `Texture` RAII; `init()` is idempotent | **In scope** |
| "Bundled fallback" reviewer expects a real `assets/` image file | Documented decision + OQ1; procedural fallback satisfies the AC text ("renders instead") | **Flagged** — OQ1 |
| Background also expected on Select/Attract/Results | Issue scope is gameplay-only; not implemented | **Out of scope** — OQ5 |

---

## Decisions

- **Procedural fallback owned by the render layer.** A compiled-in gradient texture (built in
  `BackgroundRenderer::init()` via `Texture::from_rgba`) means no committed binary asset, no
  asset-copy/install step, and a fallback that always exists — matching `NoteSkin`'s explicit
  "no image files" precedent and the engine-only philosophy. Surfaced as OQ1.
- **Background drawn in `GameplayView::render`, not `GameplayScreen`.** Keeps gameplay rendering
  in the already-tested view and the screen thin (its documented role); the background is just
  another gameplay asset like the noteskin.
- **Path passed through `init` with a defaulted argument.** Preserves all existing `GameplayView`
  test call sites; `GameplayScreen` supplies `resolved_background_path`.
- **Cover-fit (fill + crop).** No distortion, fills the framebuffer, matches typical arcade
  background behaviour; pure and unit-tested. Surfaced as OQ3.
- **Reuse the existing untrusted-image pipeline.** `Texture::from_file` already enforces the
  4096px / 16 MiB caps and never throws; no new hardening needed.
- **No scanner change.** The dead `SongLibrary::set_fallback_background` seam is left untouched;
  the render layer owns fallback. Surfaced as OQ4.

---

## Open Questions

1. **Blocking-ish — fallback representation: compiled-in procedural gradient (recommended) vs a
   committed `assets/backgrounds/fallback.png` wired via `SongLibrary::set_fallback_background`.**
   The issue's tech note and AGENTS.md both list `assets/ (fallback bg)`, while `NoteSkin` sets a
   "no image files" precedent and there is currently no asset-copy/install step. Proposed default:
   **procedural gradient** (no binary, always present, fresh-clone safe). If a real image is
   required, add the asset + `main` scanner wiring + CMake copy as a follow-up.
2. **Non-blocking — exact dim level.** Not specified ("reduced brightness"). Proposed default:
   `0.35` image alpha + `0.30` black scrim (≈0.25 effective brightness), tunable until D2. Confirm.
3. **Non-blocking — aspect handling.** Proposed default: **cover** (fill + crop). Alternatives:
   stretch (distorts) or fit (letterbox bars). Confirm.
4. **Non-blocking — scanner fallback seam.** Proposed default: **leave `set_fallback_background`
   unwired**; the render layer owns fallback. Alternative: wire a global fallback so
   `resolved_background_path` is never empty. Confirm.
5. **Non-blocking — scope.** Background applies to gameplay only (per the issue). Proposed default:
   **gameplay-only**; Select/Attract/Results backgrounds are separate D-phase polish. Confirm.

---

## Acceptance Criteria

- [ ] Given a song with a background image, gameplay renders it behind the receptors at reduced
      brightness (Tasks 1-3; E2E 1)
- [ ] Given a song without background art, the bundled (compiled-in) fallback background renders
      instead (Task 1; E2E 2)
- [ ] Given any background, note-field readability is preserved via the dim + scrim (Task 1;
      E2E 3; `cover_uv` tests)
- [ ] The implementation is 2D only — textured/solid quads, no video, no 3D (Task 1; Validation
      `rg` check)
- [ ] `ctest --test-dir build --output-on-failure` → **28/28**; `score_keeper_test`,
      `life_keeper_test`, `results_screen_test`, and the `--gameplay-demo` path stay green
      (Tasks 5-6; E2E 4-5)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic` (Validation)
- [ ] Open Questions OQ1-OQ5 confirmed or defaults accepted (fallback representation, dim level,
      aspect mode, scanner seam, scope)
