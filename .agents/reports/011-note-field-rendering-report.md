# Implementation Report

**Plan**: `.agents/plans/011-note-field-rendering-plan.md`
**Branch**: `feature/011-note-field-rendering`
**Status**: COMPLETE

## Summary

Built the gameplay rendering foundation for Blaze 4k: a 2D OpenGL 3.3 textured-quad
pipeline (`src/render/`) and a gameplay layer (`src/gameplay/`) with pure speed-mod math,
deterministic note-field layout, a procedural placeholder noteskin, a field renderer, and a
`GameplayView` host whose time source is exclusively the B1 `MusicClock`. A temporary
`--gameplay-demo` CLI harness exercises the real GL path and clock end-to-end.

Gameplay time comes only from `MusicClock` (bound to `SoundStream` in production, with a
documented synthetic-PCM stub for the harness). `speed_mod.*` and `note_field.*` are free of
SDL/GL/audio/chrono dependencies.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Render value types | `src/render/geometry.hpp` | ✅ |
| 2 | Texture RAII wrapper | `src/render/texture.hpp`, `.cpp` | ✅ |
| 3 | Batched 2D quad renderer | `src/render/gl_quad_renderer.hpp`, `.cpp` | ✅ |
| 4 | Speed-mod math (pure) | `src/gameplay/speed_mod.hpp`, `.cpp` | ✅ |
| 5 | Note field layout (pure) | `src/gameplay/note_field.hpp`, `.cpp` | ✅ |
| 6 | Placeholder noteskin | `src/gameplay/noteskin.hpp`, `.cpp` | ✅ |
| 7 | Note field renderer | `src/gameplay/note_field_renderer.hpp`, `.cpp` | ✅ |
| 8 | Gameplay view host | `src/gameplay/gameplay_view.hpp`, `.cpp` | ✅ |
| 9 | Temporary CLI harness | `src/main.cpp` | ✅ |
| 10 | Register sources + test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 11 | Test suite | `tests/note_field_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DFETCHCONTENT_BASE_DIR=...`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ zero warnings under `-Wall -Wextra -Wpedantic` |
| `./build/tests/note_field_test` | ✅ 10/10 sub-checks |
| `ctest --test-dir build --output-on-failure` | ✅ 100% (11/11: 10 prior + new) |
| Purity grep (`SDL\|glad\|gl[A-Z]\|ma_\|chrono\|thread\|GetPerformanceCounter` on `speed_mod.*`, `note_field.*`) | ✅ no matches (exit 1) |
| Clock-source grep on `gameplay_view.cpp` | ✅ only `MusicClock`/`SamplePosition` bindings |
| Headless smoke, X-mod | ✅ exits 0, logs chart counts + speed mod, no GL calls |
| Headless smoke, `C400 --downscroll` | ✅ exits 0 |
| GL demo run (`--gameplay-demo ... --speed 2x`, real GL 4.6 context) | ✅ 120 frames, no errors |
| Scratch `glReadPixels` rasterization check (solid + textured quad) | ✅ passed, then deleted |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/render/geometry.hpp` | CREATE | 38 |
| `src/render/texture.hpp` | CREATE | 42 |
| `src/render/texture.cpp` | CREATE | 106 |
| `src/render/gl_quad_renderer.hpp` | CREATE | 69 |
| `src/render/gl_quad_renderer.cpp` | CREATE | 260 |
| `src/gameplay/speed_mod.hpp` | CREATE | 33 |
| `src/gameplay/speed_mod.cpp` | CREATE | 126 |
| `src/gameplay/note_field.hpp` | CREATE | 73 |
| `src/gameplay/note_field.cpp` | CREATE | 116 |
| `src/gameplay/noteskin.hpp` | CREATE | 40 |
| `src/gameplay/noteskin.cpp` | CREATE | 74 |
| `src/gameplay/note_field_renderer.hpp` | CREATE | 27 |
| `src/gameplay/note_field_renderer.cpp` | CREATE | 114 |
| `src/gameplay/gameplay_view.hpp` | CREATE | 69 |
| `src/gameplay/gameplay_view.cpp` | CREATE | 130 |
| `src/main.cpp` | UPDATE | +107/−5 |
| `CMakeLists.txt` | UPDATE | +7 |
| `tests/CMakeLists.txt` | UPDATE | +10 |
| `tests/note_field_test.cpp` | CREATE | 338 |

## Deviations from Plan

1. **`NoteFieldConfig` gained `column_width`** (default 64 px). Required by `column_x` /
   `field_width` to implement the resolved decision "columns 64px wide centered". Treated as
   Blaze 4k-owned placeholder config.
2. **`NoteFieldRenderer` is a class with a `render(...)` method**, not a free function. It is a
   member of `GameplayView` per the plan's own struct layout, so a class is the natural shape.
3. **Per-vertex color instead of a `u_tint` uniform.** Quad color is a vertex attribute, so
   rectangles of different colors batch together and flushing only happens on texture change.
4. **`parse_speed_mod` also accepts a prefix `X` form** (e.g. `X2`) in addition to the suffix
   `2x`/`2X`, because the plan's own test case uses `"X2"`. OpenITG's regex is suffix-only; the
   prefix form is a superset and cannot change suffix behavior.
5. **Test 4's "no valid BPM → 1.0" fallback is implemented but not unit-tested.** `TimingData`'s
   constructor always seeds a 120 BPM segment, and non-positive/non-finite BPMs are rejected on
   insert, so an empty/zero-BPM timing is unreachable through the public API. The test instead
   asserts `max_chart_bpm == 150`, `M600 -> 4.0`, X-mod passthrough, and field-equivalence of
   M600 vs X4.
6. **`NoteSkin::init()` guards on `glad_glGenTextures` before any GL/texture call**, so headless
   mode attempts no GL functions at all (satisfies E2E step 3) rather than relying on GL entry
   points being null-safe.
7. **`receptor_y` is resolved per-frame** in `GameplayView::render` from
   `fraction * framebuffer_height` (0.15 upscroll / 0.85 downscroll) instead of a fixed pixel
   value, keeping the field correct across window resizes.
8. **`GameplayView::init` calls `skin_.init()` internally** (the plan's Task 8 list did not spell
   this out); it is safe/no-op headless. `GlQuadRenderer` remains owned by `main.cpp`.
9. **Audio could not be verified live.** The fixture `Blaze Anthem/music.ogg` is a 15-byte
   placeholder; `SoundStream::load` returns `MA_INVALID_FILE` (`error code: -10`) in headless and
   display runs alike. The documented synthetic stub clock therefore always supplies time in the
   demo. E2E step 4's "notes scroll in time with the music" and "C400 constant across a
   tempo-shifting section" could not be confirmed against real audio; the GL draw path itself was
   exercised for 120 frames on a real GL 4.6 context, and rasterization/projection were proven by
   a temporary `glReadPixels` scratch program (deleted after use).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/note_field_test.cpp` | (1) speed-mod parse incl. case-insensitivity + invalid/reject; (2) C-mod BPM-independent; (3) X-mod beat spacing across BPM change; (4) M-mod → X-mod equivalence + max BPM; (5) offset driven only by supplied time, monotonic approach; (6) up/down scroll mirror; (7) X-mod frozen vs C-mod moving through a stop; (8) tap/hold/roll/mine distinctness; (9) culling incl. partially visible holds; (10) column ordering/layout |
