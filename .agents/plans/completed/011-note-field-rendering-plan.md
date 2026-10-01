# Plan: Note Field Rendering with Receptors, Speed Mods, and Scroll Direction

## Summary

Build the gameplay rendering foundation for Blaze 4k: a scrolling 4-panel note field with a fixed
receptor row, driven **exclusively** by the B1 `MusicClock` (PRD §6 pattern 1 / AGENTS.md core
principle 1). The work splits into a pure, unit-testable layout core and a thin OpenGL 3.3 layer:

1. `src/render/` (NEW) — a minimal 2D textured-quad pipeline: `geometry` value types, a move-only
   `Texture` RAII wrapper, and a batched `GlQuadRenderer` with an orthographic top-left projection.
   No font/background/image work (that is D1/D2).
2. `src/gameplay/` (NEW) — pure speed-mod math (`speed_mod`), deterministic note-field layout
   (`note_field`) that maps a note to a pixel offset from the receptor given an absolute music time,
   a placeholder `NoteSkin`, a `NoteFieldRenderer` that turns layout items into quads, and a
   `GameplayView` host that binds `MusicClock` to a `SoundStream` and renders the field.
3. A temporary `--gameplay-demo` CLI harness (in `main.cpp`) so the real GL path and the live clock
   can be exercised end-to-end before C1's screen state machine exists.

Speed-mod semantics are pinned to **OpenITG** `src/ArrowEffects.cpp`, `src/PlayerOptions.cpp`, and
`src/Player.cpp` at commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (see **Value Provenance**).
Holds, rolls, and mines render as visually distinct placeholder quads (body/tail for holds & rolls,
smaller distinct mine). No judgment/scoring/life logic is built here (that is B4/B5/B6).

## User Story

As a player
I want a scrolling 4-panel note field with receptors and C-mod, X-mod, and M-mod speed options plus up/down scroll
So that I can read charts at my preferred speed regardless of BPM changes.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | LARGE |
| Systems Affected | `src/render/` (NEW dir), `src/gameplay/` (NEW dir), `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #11 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3, glad (GL 3.3 core), miniaudio, nlohmann_json, stb already fetched; `glad` linked `PUBLIC` to `blaze4k_core` (`CMakeLists.txt:101-108`) |
| Baseline tests | 10/10 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 10" (0.22s) |
| `src/render/` | does not exist | Must be created; add sources to `blaze4k_core` |
| `src/gameplay/` | does not exist | Must be created; add sources to `blaze4k_core` |
| Rendering context | `Window::init()` creates GL 3.3 core + glad (`src/app/window.cpp:71-123`) | Headless mode creates **no** GL context (`window.cpp:48-52`) → GL cannot be unit-tested headless; test pure math + E2E on a display |
| stb_image | `stb/stb_image.h` fetched | **Not used in B3** — placeholder noteskin is procedural, so no image decode/implementation macro is added (D1/D2 owns image loading) |
| Upstream source | `/tmp/opencode/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Cloned for provenance |

**Start green, stay green:** 10 tests pass; this plan adds 1 test target (`note_field_test`) → 11 expected.

---

## Pinned Semantics

Authority for B3 is **OpenITG** (PRD §15). Blaze 4k reproduces OpenITG's mod math but re-expresses it
against its own `TimingData` (A5/A6) and `MusicClock` (B1) domains.

### Mod parsing (`src/PlayerOptions.cpp:263-287`)

| Blaze 4k input | OpenITG branch | Effect |
|--------------|----------------|--------|
| `Nx` | regex `^([0-9]+(\.[0-9]+)?)x$` | `XMod`: `scroll_speed = N`, beat spacing (time spacing 0) |
| `cN` | `sscanf("c%f")` | `CMod`: `scroll_bpm = N`, time spacing 1 |
| `mN` | `sscanf("m%f")` | `MMod`: `max_scroll_bpm = N`, beat spacing (time spacing 0) |

Defaults in OpenITG (`PlayerOptions.cpp:17-20`): `timeSpacing=0`, `scrollSpeed=1.0`, `scrollBPM=200`.

### Scroll offset formula (`src/ArrowEffects.cpp:33-56,104-138`)

Let `t` = current music time (seconds), `ARROW_SPACING = ARROW_SIZE = 64 px` (`ArrowEffects.cpp:11`,
`ScreenDimensions.h:30`).

```
beat spacing (X-mod, M-mod):  y = (note.beat           - seconds_to_beat(t)) * ARROW_SPACING * x_speed
time spacing (C-mod):         y = (note.time_seconds   - t)                  * (c_bpm / 60) * ARROW_SPACING
```

- For X-mod: `x_speed = x_value` (the `m_fScrollSpeed`).
- For M-mod: `x_speed = m_value / max_bpm(chart)` — OpenITG converts M-mod to an X-mod per-song:
  "set an X-mod equal to Mnum / fMaxBPM (e.g. M600 with 150 becomes 4x)" (`Player.cpp:234-236`). The
  divisor `fMaxBPM` is `DisplayBpms.GetMax()` when not secret (`Player.cpp:206-210`), else the actual
  max BPM via `GetActualBPM` (`Player.cpp:213-230`). Blaze 4k has no `#DISPLAYBPM` parse, so it uses the
  actual max over `TimingData::bpms()`.
- `y > 0` means the note is still **ahead** of the receptor (not yet hit); `y < 0` means passed.
- OpenITG multiplies `fYOffset` by `fScrollSpeed` at the end (`ArrowEffects.cpp:135`); for C-mod
  `scrollSpeed` stays at the `1.0` default, so the `c_bpm/60` term is the whole speed.

### BPM changes and stops

- **C-mod** is time-based: only `note.time_seconds - t` matters, and both are in real seconds, so a
  tempo change does **not** change scroll speed (AC2). During a stop, real time keeps advancing, so
  notes keep scrolling (matches `m_fMusicSecondsVisible` at `ArrowEffects.cpp:45`).
- **X/M-mod** are beat-based: `seconds_to_beat(t)` is used, which freezes the beat during a stop
  (`src/chart/timing_data.cpp:241-244`), so notes **freeze** through a stop (matches
  `m_fSongBeatVisible` at `ArrowEffects.cpp:36`).

### Scroll direction (`src/ArrowEffects.cpp:141-157`)

`ArrowGetReverseShiftAndScale` sets `fScale = SCALE(percent_reverse, 0, 1, 1, -1)`; `percent_reverse`
is `1` for `SCROLL_REVERSE` (downscroll), `0` by default (upscroll) (`PlayerOptions.cpp:539-556`).
Blaze 4k models this as a pure mirror about the receptor row:

```
upscroll   (default): screen_y = receptor_y + y
downscroll (reverse): screen_y = receptor_y - y
```

(The full ITG reverse shift also re-centers by half the reverse offset; Blaze 4k's pure mirror is a
documented simplification — Open Question 3.)

### Note field geometry (Blaze 4k-owned, placeholder)

- 4 columns of width `ARROW_SIZE = 64` centered on the playfield; column centers ordered L,D,U,R.
- One receptor per column at `receptor_y`. Upscroll receptor near the top of the screen, downscroll
  near the bottom (fractions are tunable config, not ITG-authoritative).
- Hold/roll: head at `offset(note)`, tail at `offset(note.beat + hold_length_beats)` (X/M) or
  `offset(hold_end_time_seconds)` (C). The tail is later, so its offset is **larger** (farther from
  the receptor). Body is the quad between head and tail.
- Mine: no body; a smaller distinct quad.

---

## Value Provenance

All behavioral values below are transcribed from the cloned OpenITG repository at commit
**`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`**.

| Value / Semantic | Value | Upstream source (file:line) |
|------------------|-------|-----------------------------|
| `ARROW_SPACING` (px/beat at 1x) | `64` | `src/ArrowEffects.cpp:11`; `src/ScreenDimensions.h:30` (`#define ARROW_SIZE (64)`) |
| Beat-spacing term | `(fNoteBeat - fSongBeat) * ARROW_SPACING` | `src/ArrowEffects.cpp:37-39` |
| Time-spacing term | `(fNoteSeconds - fSongSeconds) * (fBPM/60) * ARROW_SPACING` | `src/ArrowEffects.cpp:48-51` |
| Mod parse: `Nx` | `scrollSpeed=N`, `timeSpacing=0`, `maxScrollBPM=0` | `src/PlayerOptions.cpp:263-274` |
| Mod parse: `cN` | `scrollBPM=N`, `timeSpacing=1`, `maxScrollBPM=0` | `src/PlayerOptions.cpp:275-281` |
| Mod parse: `mN` | `maxScrollBPM=N`, `timeSpacing=0` | `src/PlayerOptions.cpp:283-287` |
| Defaults | `timeSpacing=0`, `scrollSpeed=1.0`, `scrollBPM=200`, `maxScrollBPM=0` | `src/PlayerOptions.cpp:17-20` |
| M-mod → X-mod | `scrollSpeed = maxScrollBPM / fMaxBPM` | `src/Player.cpp:234-237` |
| M-mod max BPM (displayed) | `DisplayBpms.GetMax()` when not secret, `max(0, ...)` | `src/Player.cpp:201-210` |
| M-mod max BPM (fallback) | actual max via `GetActualBPM` | `src/Player.cpp:212-230` |
| Scroll-speed application | `fYOffset *= m_fScrollSpeed` | `src/ArrowEffects.cpp:135` |
| Passed-note behavior | `if (fYOffset < 0) return fYOffset * fScrollSpeed` | `src/ArrowEffects.cpp:54-56` |
| Reverse scale | `SCALE(percent_reverse, 0,1, 1,-1)` | `src/ArrowEffects.cpp:151-156` |
| Reverse percent | `SCROLL_REVERSE` adds `1` (default 0) | `src/PlayerOptions.cpp:539-556` |

No value in this plan is invented; pixel geometry beyond `ARROW_SIZE` is explicitly Blaze 4k-owned
placeholder config (flagged in Open Questions).

---

## Patterns to Follow

### Naming (modules / units)
```cpp
// SOURCE: src/timing/music_clock.hpp:28-49
class MusicClock {
public:
    void set_global_offset_seconds(double offset);
    [[nodiscard]] double time_seconds() const;        // frames / rate + offset
    static double seconds_from_pcm(uint64_t frames, uint32_t sample_rate);
};
```
`snake_case` methods, `[[nodiscard]]` on pure getters, `_seconds` / `_beats` unit suffixes, pure
static helpers. Keep pure modules free of platform headers (mirror `music_clock.hpp:1-54`).

### Production clock binding
```cpp
// SOURCE: src/timing/music_clock.hpp:23-24
clock.set_source([&]{ return SamplePosition{s.get_position_frames(), s.get_sample_rate()}; });
```

### RAII platform wrapper (move-only)
```cpp
// SOURCE: src/audio/sound_stream.hpp:10-23
class SoundStream {
public:
    SoundStream();
    ~SoundStream();
    SoundStream(const SoundStream&) = delete;
    SoundStream(SoundStream&& other) noexcept;
};
```

### Error handling / logging
```cpp
// SOURCE: src/audio/sound_stream.cpp:63-67
if (result != MA_SUCCESS) {
    std::cerr << "[SoundStream] Failed to load audio file '" << filepath
              << "' (error code: " << static_cast<int>(result) << ")\n";
    return false;
}
```
Tagged `std::cerr` lines prefixed `[ModuleName]`; never throw for user-data errors — return `false` /
fall back to a safe value.

### Chart timing lookups
```cpp
// SOURCE: src/chart/timing_data.hpp:31-34
[[nodiscard]] double beat_to_seconds(double beat) const;
[[nodiscard]] double seconds_to_beat(double seconds) const;
[[nodiscard]] double get_bpm_at_beat(double beat) const;
[[nodiscard]] const std::vector<BpmSegment>& bpms() const;
```

### Note model
```cpp
// SOURCE: src/chart/note.hpp:25-35
struct Note {
    int column = 0;            // 0=Left, 1=Down, 2=Up, 3=Right
    double beat = 0.0;
    double time_seconds = 0.0;
    NoteType type = NoteType::Tap;
    double hold_length_beats = 0.0;
    double hold_end_time_seconds = 0.0;
};
```

### Tests
```cpp
// SOURCE: tests/music_clock_test.cpp:30-36
#define TEST_CHECK(expr) \
    do { if (!(expr)) { \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
        std::abort(); } } while (0)
```
Plain `int main()` binaries, `TEST_CHECK`, deterministic and hardware-independent.

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:80-95 / tests/CMakeLists.txt:92-100
add_library(blaze4k_core STATIC ... src/timing/music_clock.cpp ...)
add_executable(note_field_test note_field_test.cpp)
target_link_libraries(note_field_test PRIVATE blaze4k_core)
add_test(NAME note_field_test COMMAND note_field_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/geometry.hpp` | CREATE | `Color`, `Rect`, `UVRect` POD value types |
| `src/render/texture.hpp` | CREATE | Move-only GL `Texture` RAII wrapper |
| `src/render/texture.cpp` | CREATE | `from_rgba` / `solid` implementation + `[Texture]` logging |
| `src/render/gl_quad_renderer.hpp` | CREATE | `GlQuadRenderer` interface (init/begin/draw/end) |
| `src/render/gl_quad_renderer.cpp` | CREATE | GL 3.3 shader program, VAO/VBO, ortho projection, batching |
| `src/gameplay/speed_mod.hpp` | CREATE | `SpeedModType`, `SpeedMod`, `parse_speed_mod`, `resolve_x_speed` (pure) |
| `src/gameplay/speed_mod.cpp` | CREATE | Parse + M-mod resolution + max-BPM helper |
| `src/gameplay/note_field.hpp` | CREATE | `ScrollDirection`, `NoteFieldConfig`, `NoteRenderItem`, `NoteField` (pure layout) |
| `src/gameplay/note_field.cpp` | CREATE | Offset math, hold tail, culling, column x, screen y |
| `src/gameplay/noteskin.hpp` | CREATE | `NoteStyle` + `NoteSkin` interface + placeholder factory |
| `src/gameplay/noteskin.cpp` | CREATE | Placeholder styles/textures (colored textured quads) |
| `src/gameplay/note_field_renderer.hpp` | CREATE | Bridges `NoteField` output to `GlQuadRenderer` |
| `src/gameplay/note_field_renderer.cpp` | CREATE | Draw receptors, taps, hold/roll bodies, mines |
| `src/gameplay/gameplay_view.hpp` | CREATE | Host: chart + audio + `MusicClock` + field + renderer |
| `src/gameplay/gameplay_view.cpp` | CREATE | Load chart/audio, clock binding, update/render |
| `src/main.cpp` | UPDATE | Add temporary `--gameplay-demo`, `--speed`, `--downscroll` harness |
| `CMakeLists.txt` | UPDATE | Add render/ + gameplay/ sources to `blaze4k_core` |
| `tests/note_field_test.cpp` | CREATE | Speed-mod, layout, scroll, stop, note-type tests |
| `tests/CMakeLists.txt` | UPDATE | Register `note_field_test` |

---

## Tasks

Execute in order. Each task is atomic and verifiable. No production judgment/scoring code.

### Task 1: Render value types

- **File**: `src/render/geometry.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  struct Color { float r=1, g=1, b=1, a=1; };
  struct Rect  { float x=0, y=0, w=0, h=0; }; // top-left origin, pixels
  struct UVRect{ float u0=0, v0=0, u1=1, v1=1; };
  }
  ```
  Header-only, `#pragma once`, no GL includes. Add `constexpr Color with_alpha(Color, float)`.
- **Mirror**: `src/chart/note.hpp:1-36` (small POD value types).
- **Validate**: `cmake --build build -j16` once included by Task 2.

### Task 2: Texture RAII wrapper

- **Files**: `src/render/texture.hpp`, `src/render/texture.cpp`
- **Action**: CREATE
- **Implement**:
  - `class Texture` — move-only (delete copy, default `noexcept` move), `~Texture()` deletes the GL id.
  - `static Texture from_rgba(int w, int h, const uint8_t* rgba);` (uploads RGBA8, `GL_LINEAR`,
    `GL_CLAMP_TO_EDGE`), `static Texture solid(Color c);` (1×1 RGBA from `c`).
  - `[[nodiscard]] bool valid() const;`, `[[nodiscard]] unsigned int id() const;`,
    `[[nodiscard]] int width() const;`, `[[nodiscard]] int height() const;`.
  - On invalid input log `[Texture] ...` to `std::cerr` and return an invalid texture (never throw).
  - No file/image loading in B3 (D1/D2 owns it). Include `<glad/glad.h>` only in the `.cpp`.
- **Mirror**: `src/audio/sound_stream.hpp:10-23` (move-only RAII), `src/audio/sound_stream.cpp:63-67` (logging).
- **Validate**: `cmake --build build -j16`.

### Task 3: Batched 2D quad renderer

- **Files**: `src/render/gl_quad_renderer.hpp`, `src/render/gl_quad_renderer.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  class GlQuadRenderer {
  public:
      bool init();                       // compile shaders, create VAO/VBO, 1x1 white texture
      void shutdown();
      [[nodiscard]] bool is_initialized() const;
      void begin(int framebuffer_w, int framebuffer_h); // glViewport + ortho (top-left, y down)
      void draw_quad(const Rect&, Color tint);                                       // white tex
      void draw_textured_quad(const Rect&, const Texture&, const UVRect&, Color tint);
      void end();                        // flush pending vertices
  };
  ```
  - GL 3.3 core: one vertex+fragment shader pair; vertex attributes `vec2 pos` (pixels) + `vec2 uv`;
    uniform `mat4 u_projection`; fragment `texture(u_tex, v_uv) * u_tint`.
  - Ortho projection maps `(0,0)` top-left → `(w,h)` bottom-right in pixels (y down), so layout math
    can use screen coordinates directly.
  - Batch into a dynamic vertex buffer (6 verts/quad); flush in `end()` and when the bound texture
    changes. Blend enabled (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`).
  - Guard: `begin`/`draw_*` no-op safely if not initialized (so headless can skip GL).
  - Tag errors `[GlQuadRenderer] ...`; `init()` returns `false` on shader/link failure.
- **Mirror**: `src/app/window.cpp:71-123` (GL init/teardown), `src/audio/sound_stream.cpp:63-67` (logging).
- **Validate**: `cmake --build build -j16`.

### Task 4: Speed-mod math (pure)

- **Files**: `src/gameplay/speed_mod.hpp`, `src/gameplay/speed_mod.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  enum class SpeedModType { CMod, XMod, MMod };

  struct SpeedMod {
      SpeedModType type = SpeedModType::XMod;
      double value = 1.0;              // c-BPM, x multiplier, or m-BPM
  };

  // Accepts "Nx", "cN", "mN" (case-insensitive); false on invalid/negative/zero value.
  [[nodiscard]] bool parse_speed_mod(std::string_view text, SpeedMod& out);

  // Max BPM over TimingData; 0.0 if none/invalid.
  [[nodiscard]] double max_chart_bpm(const TimingData& timing);

  // Resolves the X-mod-equivalent multiplier: XMod->value; MMod->value/max_bpm (falls back to 1.0
  // with a [SpeedMod] warning if max_bpm <= 0); CMod->value (unused by callers).
  [[nodiscard]] double resolve_x_speed(const SpeedMod& mod, const TimingData& timing);
  }
  ```
  - Include only `<string_view>`, `<cmath>`, `"chart/timing_data.hpp"`. **No** SDL/GL/chrono/thread.
  - Parse mirrors OpenITG (`PlayerOptions.cpp:263-287`); reject non-finite values.
- **Mirror**: `src/timing/music_clock.cpp` (pure guarded helpers), `src/chart/timing_data.cpp:254-267`.
- **Validate**: `cmake --build build -j16`.

### Task 5: Note field layout (pure)

- **Files**: `src/gameplay/note_field.hpp`, `src/gameplay/note_field.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  enum class ScrollDirection { Up, Down };

  struct NoteFieldConfig {
      double pixels_per_beat = 64.0;   // OpenITG ARROW_SIZE
      double receptor_y = 0.0;         // pixels
      ScrollDirection direction = ScrollDirection::Up;
  };

  struct NoteRenderItem {
      const Note* note = nullptr;
      NoteType type = NoteType::Tap;
      int column = 0;
      double head_offset = 0.0;        // >0 = ahead of receptor
      double tail_offset = 0.0;        // >= head_offset for holds/rolls
      bool has_body = false;
  };

  class NoteField {
  public:
      void set_chart(const Chart* chart);
      void set_speed_mod(const SpeedMod&);   // resolves M-mod once against chart timing
      void set_config(const NoteFieldConfig&);
      [[nodiscard]] double effective_x_speed() const;
      [[nodiscard]] double offset_for_note(const Note&, double music_time_seconds) const;
      [[nodiscard]] double offset_for_beat(double beat, double music_time_seconds) const;
      [[nodiscard]] double tail_offset_for(const Note&, double music_time_seconds) const;
      void compute_visible(double music_time_seconds, double visible_top, double visible_bottom,
                           std::vector<NoteRenderItem>& out) const;
      [[nodiscard]] double column_x(int column, double field_left) const;
      [[nodiscard]] double screen_y(double offset) const; // receptor_y ± offset
  };
  }
  ```
  - Offset math exactly per **Pinned Semantics**; uses `chart_->timing.seconds_to_beat()` for X/M and
    `note.time_seconds` / `note.hold_end_time_seconds` for C.
  - `tail_offset_for`: X/M → `offset_for_beat(note.beat + note.hold_length_beats, t)`; C →
    `(note.hold_end_time_seconds - t) * (c_bpm/60) * pixels_per_beat`.
  - `compute_visible`: linear scan over `chart_->notes` (already beat-sorted, `note_parser.cpp:245-248`);
    skip items whose head and tail offsets both fall outside `[visible_top, visible_bottom]`; holds are
    kept if any part of the body intersects. For v1 a full scan is acceptable; a cursor/binary-search
    optimization is noted as future (Risk table).
  - Pure module: include `<vector>`, `"chart/chart.hpp"`, `"gameplay/speed_mod.hpp"`. No GL/SDL.
  - Guard null chart (return empty).
- **Mirror**: `src/timing/music_clock.cpp` (pure math), `src/chart/timing_data.cpp:147-252` (beat/time).
- **Validate**: `cmake --build build -j16`.

### Task 6: Placeholder noteskin

- **Files**: `src/gameplay/noteskin.hpp`, `src/gameplay/noteskin.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  struct NoteStyle {
      Color head_color;
      Color body_color;
      Color tail_color;
      double width = 56.0;
      double height = 56.0;
  };

  class NoteSkin {
  public:
      bool init();                       // creates procedural textures (needs GL context)
      void shutdown();
      [[nodiscard]] const Texture& receptor_texture() const;
      [[nodiscard]] const Texture& quad_texture() const;   // white 1x1, tinted per style
      [[nodiscard]] const NoteStyle& style_for(NoteType) const;
      [[nodiscard]] Color column_tint(int column) const;
  private:
      Texture white_;
      Texture receptor_;
      NoteStyle tap_, hold_, roll_, mine_;
  };
  }
  ```
  - Placeholder uses `Texture::solid` / `from_rgba` only (no image files). Distinctness:
    tap = cyan, hold = green head/body/tail, roll = amber/purple head/body/tail, mine = red at reduced
    size (e.g. 40×40), receptor = white/gray. `column_tint` multiplies a per-column hue so columns read
    apart. `init()` returns `false` if GL is unavailable (headless) and logs `[NoteSkin] ...`.
- **Mirror**: `src/audio/sound_stream.cpp:63-67` (guarded init + logging).
- **Validate**: `cmake --build build -j16`.

### Task 7: Note field renderer

- **Files**: `src/gameplay/note_field_renderer.hpp`, `src/gameplay/note_field_renderer.cpp`
- **Action**: CREATE
- **Implement**:
  - `void render(const NoteField&, const std::vector<NoteRenderItem>&, int screen_w, int screen_h,
    const NoteSkin&, GlQuadRenderer&);`
  - Draw order: receptor row (4 quads at `receptor_y`) → hold/roll bodies → tails → heads → mines
    (bodies first so heads sit on top).
  - Tap/hold head/roll head: `style.width × style.height` quad centered on `column_x`, at `screen_y(head_offset)`.
  - Hold/roll body: quad spanning `min(screen_y(head),screen_y(tail))` to `max(...)`, width
    `style.width * 0.6`, `body_color` with alpha; tail is a short cap quad at `screen_y(tail)`.
  - Mine: reduced quad at `screen_y(head_offset)` with mine style.
  - Guard `screen_h <= 0` / uninitialized renderer → no-op.
- **Mirror**: `src/app/app.cpp:122-131` (render guard style).
- **Validate**: `cmake --build build -j16`.

### Task 8: Gameplay view host (clock-driven)

- **Files**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {
  struct GameplayOptions {
      SpeedMod speed{};
      ScrollDirection scroll = ScrollDirection::Up;
      double global_offset_seconds = 0.0;
  };

  class GameplayView {
  public:
      // chart must outlive? No: copies the chart and its TimingData.
      bool init(const Chart& chart, const std::string& audio_path, const GameplayOptions&);
      void update(double fixed_dt);                       // starts audio once; no wall-clock reads
      void render(GlQuadRenderer&, int screen_w, int screen_h);
      [[nodiscard]] double music_time_seconds() const;    // clock_->time_seconds()
      [[nodiscard]] bool is_ready() const;
      void shutdown();
  private:
      Chart chart_;
      MusicClock clock_;
      SoundStream audio_;
      NoteField field_;
      NoteSkin skin_;
      NoteFieldRenderer field_renderer_;
      std::vector<NoteRenderItem> items_;
      double stub_frames_ = 0.0;   // harness-only fallback when audio is unavailable
      bool use_stub_ = false;
      bool audio_started_ = false;
  };
  }
  ```
  - `init`: copy chart, `field_.set_chart(&chart_)`, `field_.set_speed_mod(opts.speed)`, set
    `NoteFieldConfig` (receptor fraction: up ≈ 0.15, down ≈ 0.85; `pixels_per_beat = 64`), set clock
    offset, load+play `audio_path`. Bind production clock exactly per `music_clock.hpp:23-24`.
  - If audio load/play fails (headless), set `use_stub_ = true` and feed `MusicClock` a synthetic PCM
    source advanced by `fixed_dt * sample_rate` in `update`. **Harness only** — production gameplay
    always binds `SoundStream`; documented in the header.
  - `update`: ensure audio started once; for C-mod pass `clock_.time_seconds()` into layout.
  - `render`: compute visible items and delegate to `NoteFieldRenderer`; no-op if renderer uninitialized.
- **Mirror**: `src/app/app.cpp` loop wiring, `src/audio/sound_stream.cpp:130-145` (position accessors).
- **Validate**: `cmake --build build -j16`.

### Task 9: Temporary CLI harness in `main.cpp`

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add flags: `--gameplay-demo <simfile.sm|.ssc>`, `--speed <mod>` (e.g. `C400`, `1.5x`, `M600`),
    `--downscroll`. Reuse existing `--smoke-test N` to bound frames and `--headless`.
  - When `--gameplay-demo` is present: after `app.init()`, `SimfileParser::parse_file(path)`, select
    chart 0, resolve the music path relative to the simfile directory using
    `metadata.music_path` (`song_metadata.hpp:19`) when it exists on disk.
  - Construct `GameplayView`; `init` it; only `init` the `GlQuadRenderer`/`NoteSkin` when
    `!app.window().is_headless()`. Set `App::set_update_callback` / `set_render_callback` to forward
    to `GameplayView`, then `app.run()`.
  - Guard invalid mod text with a `[main]`/`[SpeedMod]` warning and default to X-mod 1x.
  - Clearly mark the harness as temporary pending C1's screen manager.
- **Mirror**: `src/main.cpp:17-45` (existing arg parsing + App lifecycle).
- **Validate**: `cmake --build build -j16`.

### Task 10: Register sources and the test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root: add `src/render/texture.cpp`, `src/render/gl_quad_renderer.cpp`,
    `src/gameplay/speed_mod.cpp`, `src/gameplay/note_field.cpp`, `src/gameplay/noteskin.cpp`,
    `src/gameplay/note_field_renderer.cpp`, `src/gameplay/gameplay_view.cpp` to `blaze4k_core`
    (after the `src/timing/` entries).
  - Tests: append the `note_field_test` block mirroring `music_clock_test` (`tests/CMakeLists.txt:82-90`).
- **Mirror**: `CMakeLists.txt:80-95`, `tests/CMakeLists.txt:82-100`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 11: Test suite

- **File**: `tests/note_field_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`, deterministic, no GL/audio; build synthetic `Chart` + `TimingData`):
  1. **Parse**: `"C400"`→CMod 400; `"1.5x"`→XMod 1.5; `"M600"`→MMod 600; `"c400"`/`"X2"` case-insensitive;
     invalid (`""`, `"abc"`, `"0x"`, `"-1x"`, `"C0"`, non-finite) → `false`.
  2. **C-mod constant across BPM change**: two charts with the same note times but different BPM
     layouts/stops; at a fixed `t`, `offset_for_note` is identical, and equals
     `(note.time_seconds - t) * (400/60) * 64`.
  3. **X-mod beat spacing**: `offset == (note.beat - seconds_to_beat(t)) * 64 * x`; verify px/beat is
     invariant across a BPM change (compute at two times either side of the change).
  4. **M-mod → X equivalence**: chart with max BPM 150; `resolve_x_speed(MMod 600)` == 4.0 and M600
     offsets equal X-mod 4.0 offsets. Guard: a chart with no valid BPM → `resolve_x_speed` returns 1.0.
  5. **Clock-driven monotonic approach**: feed `offset_for_note` with increasing `t`; offset decreases
     toward 0 then goes negative after the note time (uses only the passed time — proves no wall-clock
     dependency).
  6. **Scroll direction mirror**: for identical offset, `screen_y_up == receptor_y + off` and
     `screen_y_down == receptor_y - off`, i.e. mirrored about the receptor.
  7. **Stop semantics**: chart with a 1.0s stop; for X-mod, `seconds_to_beat` freezes so the offset is
     unchanged across the stop; for C-mod the offset decreases steadily (notes keep moving) — the AC2
     behavior distinction.
  8. **Note-type distinction**: hold and roll items have `has_body == true` and
     `tail_offset > head_offset`, with different `NoteStyle` colors; mine has `has_body == false` and a
     distinct color/size; tap has no body.
  9. **Culling**: a note far outside `[visible_top, visible_bottom]` is excluded; a hold whose head is
     off-screen but whose body intersects the window is included.
 10. **Column layout**: four columns yield strictly increasing distinct `column_x` values in L,D,U,R order.
- **Mirror**: `tests/music_clock_test.cpp`, `tests/note_parser_test.cpp` (synthetic chart construction), `tests/audio_test.cpp:14-20`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 11/11).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 11/11: 10 existing + note_field_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/note_field_test

# Headless harness smoke (uses the stub clock; fixture has real audio + holds/mines)
./build/blaze-4k --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --smoke-test 60

# Same with a C-mod and downscroll
./build/blaze-4k --headless --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" \
  --speed C400 --downscroll --smoke-test 60

# With a display: real GL path and live audio clock (manual visual check)
./build/blaze-4k --gameplay-demo \
  "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --speed 2x --smoke-test 300
```

## End-to-End Verification

1. `./build/tests/note_field_test` prints each sub-check; tests 2/3/7 prove C-mod is constant through
   BPM changes/stops while X/M follow beat spacing, and test 6 proves direction mirroring.
2. `ctest --test-dir build --output-on-failure` → **11/11**; confirm all 10 prior tests stay green
   (changes are additive registrations only).
3. `--headless --gameplay-demo ... --smoke-test 60` exits cleanly; confirm `[GameplayView]` logs the
   resolved chart (note/tap/hold/roll/mine counts) and the selected speed mod, and that no GL calls are
   attempted headless.
4. On a machine with a display, run the `--gameplay-demo` command and confirm: notes scroll toward the
   receptor row in time with the music, holds/rolls draw a body+tail, mines are distinct, `--downscroll`
   mirrors the field, and `C400` shows constant speed across a tempo-shifting section.
5. Enforce module purity:
   `rg -n "SDL|glad|gl[A-Z]|ma_|chrono|thread|GetPerformanceCounter" src/gameplay/speed_mod.* src/gameplay/note_field.*`
   → **no matches** (layout/math stay platform-free; only `src/render/` and `noteskin`/`gameplay_view`
   touch GL).
6. Confirm the production clock is the only time source: `rg -n "time_seconds|MusicClock|SamplePosition"
   src/gameplay/gameplay_view.cpp` shows the `MusicClock` binding and **no** frame-delta time math outside
   the documented headless stub.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| GL path cannot be unit-tested (headless creates no context, `window.cpp:48-52`) | Keep all layout/mod math pure and fully unit-tested; verify GL visually via `--gameplay-demo` on a display; headless smoke exercises the update path with the stub clock | **In scope** — test split |
| Pixel geometry (receptor position, spacing) is not ITG-authoritative | Pin `ARROW_SIZE=64` from OpenITG (`ScreenDimensions.h:30`); make all other geometry explicit tunable `NoteFieldConfig` and document as placeholder; D2 refines with the noteskin | **In scope** — documented |
| M-mod divisor BPM differs when `#DISPLAYBPM` is obfuscated | Use actual max over `TimingData::bpms()` (equivalent to OpenITG's `GetActualBPM` fallback, `Player.cpp:212-230`); `#DISPLAYBPM` parsing is out of scope | **Out of scope** — documented |
| `max_bpm <= 0` → division by zero for M-mod | `resolve_x_speed` guards and falls back to `1.0` with a `[SpeedMod]` warning | **In scope** |
| Full note-list scan each frame hurts on dense charts | Notes are beat-sorted (`note_parser.cpp:245-248`); v1 accepts the scan, keeps `compute_visible` API stable, and leaves cursor/binary-search as a later optimization | **Out of scope** — documented |
| Audio unavailable in dev/CI environments | `GameplayView` falls back to a documented stub PCM source through `MusicClock`; production always binds `SoundStream`; demo never crashes | **In scope** — harness |
| Temporary CLI harness overlaps C1's screen manager | Keep `GameplayView` free of any screen-manager coupling; the CLI wiring is isolated in `main.cpp` and explicitly marked temporary/removable | **In scope** — documented |
| Hold body drawn inverted when head has passed the receptor | Render body between `min`/`max` of head/tail screen Y and cull per-window; test 9 covers partial visibility | **In scope** |
| Batching/flush correctness (state changes mid-frame) | Flush on texture change and in `end()`; single white texture for solid quads; receptor drawn with its own texture in a separate pass | **In scope** |
| Adding new source dirs ripples into build/tests | Registrations are additive; run the full 10-test suite after each CMake change | **In scope** |

---

## Decisions

- **Two-layer split**: pure math (`speed_mod`, `note_field`) stays in `src/gameplay/` free of GL/SDL;
  the only GL code is `src/render/` plus `noteskin`/`gameplay_view` — mirroring how `music_clock` keeps
  `src/timing/` platform-free.
- **`NoteField` is time-parameterized**: it takes an absolute music time instead of holding a clock, so
  layout is deterministic and unit-testable and cannot read wall-clock internally. `GameplayView`
  supplies `MusicClock::time_seconds()`.
- **Speed-mod math pinned to OpenITG** `ArrowEffects.cpp` / `PlayerOptions.cpp` / `Player.cpp`; M-mod is
  resolved once to an X multiplier from the chart's max BPM, exactly as OpenITG does at song load.
- **`ARROW_SIZE = 64 px`** is the only pinned pixel value; receptor placement and column geometry are
  Blaze 4k placeholder config.
- **Scroll direction as a pure mirror** about the receptor row — simpler than ITG's reverse shift and
  sufficient for up/down toggling in v1 (Open Question 3).
- **Placeholder noteskin is procedural** (colored textured quads, distinct per type) with no image
  decoding; stb_image stays unused until D1/D2.
- **Temporary `--gameplay-demo` harness** provides E2E verification before C1, with no screen-manager
  coupling.

---

## Open Questions

1. **Default speed mod / direction (needs confirmation).** Proposed default `X-mod 1.0x`, upscroll
   (OpenITG's own default is `scrollSpeed=1.0`, `timeSpacing=0`, `PlayerOptions.cpp:17-20`). Confirm
   whether Blaze 4k should instead default to a C-mod value (e.g. `C400`, as the issue example uses) on
   first run.
2. **Receptor placement and playfield scale.** Proposed upscroll receptor at ≈15% of screen height,
   downscroll at ≈85%, columns 64px wide centered. These are Blaze 4k presentation values, not
   ITG-derived. Confirm acceptable as placeholders until D2.
3. **Downscroll fidelity.** Proposed pure mirror about `receptor_y`. OpenITG's reverse also applies a
   half-reverse-offset shift for centering (`ArrowEffects.cpp:141-157`). Confirm the simplified mirror is
   acceptable for v1 or should be made ITG-exact now.
4. **M-mod source of max BPM.** Proposed: max over `TimingData::bpms()` (equivalent to OpenITG's actual
   BPM path). `#DISPLAYBPM` obfuscation is not parsed, so displayed-vs-actual divergence cannot arise.
   Confirm.
5. **Temporary harness scope.** Proposed: ship `--gameplay-demo` in B3 and let C1 supersede it. Confirm
   this is preferable to deferring all live-render verification to B4.
6. **`M-mod` and `X-mod` value units.** Proposed: `Nx` accepts any positive float (e.g. `1.5x`); `mN`/`cN`
   accept positive BPM floats. Confirm no integer-only restriction is desired.

---

## Acceptance Criteria

- [ ] Notes scroll toward the receptor row with position derived from `MusicClock` time (never wall-clock/frame delta); `src/gameplay/speed_mod.*` and `src/gameplay/note_field.*` contain no platform/time headers (E2E steps 3/6 + purity grep)
- [ ] C400 keeps constant scroll speed through BPM changes and stops (test 2 + test 7)
- [ ] X-mod and M-mod follow OpenITG beat-spacing semantics; M-mod resolves as `m_bpm / max_chart_bpm` (tests 3 & 4, Value Provenance)
- [ ] Scroll direction toggles between upscroll (default) and downscroll as a mirror about the receptor (test 6 + E2E step 4)
- [ ] Holds, rolls, and mines render distinctly (body/tail for holds & rolls, smaller distinct mine) (test 8)
- [ ] Renderer is OpenGL 3.3 core, 2D textured quads only; placeholder noteskin, no image/video/3D (E2E step 4, `src/render/`)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 11/11 pass (10 prior + `note_field_test`)
- [ ] Follows existing module/naming/test/CMake patterns; no judgment/scoring/life logic added (B4+)
