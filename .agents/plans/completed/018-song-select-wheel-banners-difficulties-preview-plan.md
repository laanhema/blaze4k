# Plan: Song Select Wheel — Banners, Difficulties, Best Grades, and Audio Preview (C3)

## Summary

Replace the C1 `SelectPlaceholderScreen` with a real **Song Select** screen that turns the scanned
`SongLibrary` into a scrollable, pack-grouped wheel. The wheel shows each song's banner art, title,
artist, and BPM; the highlighted song exposes a difficulty list with **passthrough** difficulty labels
and foot ratings (`chart.difficulty` + `chart.meter`) plus the player's **best grade per chart** read
from C2's `HighScores` via `make_chart_key`. Highlighting a song requests a delayed, looping **audio
preview** from the simfile's `sample_start`/`sample_length` (falling back to a sane default), using the
existing `SoundStream`/`AudioEngine`; navigation cancels the pending/playing preview. The whole screen
is navigable with keyboard **and** pad through the existing `Left/Down/Up/Right` + `Confirm`/`Back`
actions (arrow keys, DFJK, pad D-pad, and pad face buttons already map to these — no input-layer change
is required). Confirming a chart publishes a `PlayRequest` into `ScreenContext` and transitions to a
new, deliberately thin **GameplayScreen** that hosts the existing `GameplayView`.

Two supporting gaps are closed: (1) the shell path never scanned a songs folder, so `main.cpp` gains a
`--songs <dir>` scan and attaches the library to `ScreenContext`; (2) banners are real PNG/JPG files
but the engine has no image decoder, so this plan adds a vendored `stb_image` decode path behind
`Texture::from_file` plus a small path-keyed `TextureCache`. All new logic (wheel navigation,
difficulty selection, options derivation, preview scheduling, grade mapping) is pure/headless-testable;
no gameplay timing, judgment, or scoring code is touched.

## User Story

As a pad player
I want a song select wheel showing banners, artist/BPM, difficulty labels with foot ratings, my best
grade per chart, and audio previews
So that browsing my library feels like the arcade.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | HIGH (Large) |
| Systems Affected | `src/screens/` (new Select + thin Gameplay screens, `ScreenContext`), `src/audio/` (preview player), `src/render/` (stb_image decode + texture cache), `src/gameplay/` (extract `GameplayOptions` + config mapping), `src/main.cpp` (library scan + wiring), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #18 ([C3]) |
| PRD refs | §7.3 Screen flow (Select), §5 story 1, §12 Phase C, §11 success criteria; §7.7 difficulty passthrough (locked decision "Difficulty display: Passthrough from simfile") |
| Depends on | A3 (#3 audio), A7 (#7 library), C1 (#16 screens), C2 (#17 config/scores) — all merged |
| Blocks | — |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` from root CMake |
| Cores | 16 | `-j16` safe |
| Baseline tests | **17/17 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 17" (0.22 s), run this session |
| Existing screen seam | `src/screens/screen.hpp:25-29`, `screen_manager.hpp:33` | `ScreenContext{manager,config,scores}`; manager exposes `context()`; all fields default to `nullptr` (additive extension is safe) |
| Select placeholder | `src/screens/select_placeholder_screen.{hpp,cpp}` | Registered by `main.cpp:240`; C1 comment says "C3 replaces this file's registration (not the manager)". File can stay for `screen_manager_test`; only registration changes |
| Library scanner | `src/chart/song_library.{hpp,cpp}` | `scan_directory(root)`, `packs()→SongPack{name,banner_path,songs}`, per-song `resolved_banner_path`/`resolved_music_path`, pack grouping + fallback art. **Not currently called in the shell path** — only the `--gameplay-demo` path calls `SimfileParser` directly (`main.cpp:172-207`) |
| Song model | `src/chart/song.hpp:11-28`, `song_metadata.hpp`, `chart.hpp:10-27`, `timing_data.hpp:8-11,38` | `Song{metadata,charts,resolved_*}`; `SongMetadata{title,artist,sample_start,sample_length,selectable}`; `Chart{difficulty,meter}`; BPMs from `timing.bpms()` (`BpmSegment{beat,bpm}`) |
| High scores | `src/data/high_scores.hpp:39-60`, `high_scores.cpp:129-159,344-350` | `make_chart_key(song,chart)` (16 hex FNV-1a) and `find_high_score(scores,key)→const ScoreRecord*{grade,percent,dp,timestamp}`. Grade labels are `"quad_star"`, `"triple_star"`, `"single_star"`, `"S+"`, … |
| Config model | `src/data/config.hpp:30-68` | `AudioSettings.preview_volume=0.8`; `GameplaySettings{speed_mod:"1x",scroll:"up",fail_enabled:true}`; `OffsetSettings.global_offset_seconds` — the inputs to a gameplay run |
| Gameplay launch | `src/main.cpp:172-234` vs `235-269`; `gameplay_view.hpp:41-74` | `--gameplay-demo` builds a local `GameplayView` and drives it from App callbacks. The shell path has **no Gameplay screen**; `ScreenId::Gameplay` is only a spy in tests. `GameplayView::init(chart, constants, audio_path, options)`; `handle_input_events(events, reference_ns)`; `update(fixed_dt, held[4])`; `outcome()→InProgress|Cleared|Failed` |
| Judgment constants | `app.hpp:31`, `app.cpp:29-33`; `judgment_constants.hpp:92` | App loads `judgment_constants_` once from JSON; `GameplayView` needs it. Not in `ScreenContext` today |
| Audio | `audio_engine.hpp`, `sound_stream.hpp:21-38` | `SoundStream::load/play/seek_seconds/get_position_seconds/get_length_seconds/stop/set_volume`; lazy-inits `AudioEngine`, returns `false` (logs) when no device — safe headless |
| Input mappings | `input_manager.cpp:36-72` | Arrow keys/DFJK **and** pad D-pad/WEST/SOUTH/NORTH/EAST all map to `Left/Down/Up/Right`; Return/KP_Enter/Start→Confirm; Escape/Back→Back. `MenuUp/Down/Left/Right` are **unmapped** (do not rely on them) |
| Image decode | `render/texture.hpp:24-27`, `texture.cpp` | Only `from_rgba` / `solid`. `stb_image.h` **is** available at `build/_deps/stb-src/stb_image.h` (stb already FetchContent'd and linked) but no TU defines `STB_IMAGE_IMPLEMENTATION`. No JPG/PNG decode path exists |
| Fixtures | `tests/fixtures/reference_pack/Blaze Pack/` | 4 songs: Blaze Anthem (5 charts, `.sm`, `SAMPLESTART:30.0`), Aurora Borealis (1 chart), Glacier Groove (1 chart), Northern Lights (2 charts, `.ssc`, `SAMPLESTART:45.0`) |
| Test registration | `tests/CMakeLists.txt:156-164` | One `add_executable`/`target_link_libraries(... blaze4k_core)`/`add_test` block per target |
| Test idiom | `tests/judgment_constants_test.cpp:13-19`, `song_library_test.cpp:7-24,29-135` | `TEST_CHECK`, `std::filesystem::temp_directory_path()` fixtures, synth simfiles, `std::filesystem::remove_all` cleanup |

**Start green, stay green:** 17 tests pass; this plan adds **2** targets (`select_screen_test`,
`preview_player_test`) → **19 expected**. No behavioral change to timing/judgment/scoring/note-field;
`main.cpp` changes are additive and preserve the existing `--gameplay-demo` path.

---

## Pinned Semantics

Authority: **PRD §7.3 / locked decisions** for scope; **C2 `make_chart_key`/`HighScores`** for grade
lookup; **existing `SoundStream`** for preview playback. No gameplay constants are introduced.

### Navigation contract (keyboard + pad, PRD §12 Phase C validation)

- **Up / Down** = move the song-wheel highlight one song (wrapping at both ends).
- **Left / Right** = change the difficulty selection within the highlighted song (clamped to the
  chart count; no-op for a single-chart song). *(Clamp vs wrap flagged — OQ5.)*
- **Confirm** = publish `PlayRequest{song,chart,options}` and transition `Select → Gameplay`.
- **Back** = managed centrally; `ScreenManager::handle_back()` already sends `Select → Title`.
- Only the four directional actions + Confirm/Back are used, because those are the only actions mapped
  on **both** keyboard and gamepad (`input_manager.cpp:36-72`). No input-layer change is needed.

### Difficulty passthrough (locked decision "Passthrough from simfile")

- Display `chart.difficulty` verbatim (e.g. `"Beginner"`, `"Easy"`, `"Medium"`, `"Hard"`,
  `"Challenge"`, `"Edit"`) and `chart.meter` verbatim as the foot rating. No remapping, no reordering
  beyond the scanner's chart order.

### Best grade display

- For each chart: `const ScoreRecord* r = find_high_score(*ctx.scores, make_chart_key(song, chart))`.
- No record → `"---"`. Otherwise display the stored `grade` label, with the four star tiers rendered
  as `★` glyphs via a pure `grade_display_label(const std::string&)` helper
  (`quad_star→"★★★★"`, `triple_star→"★★★"`, `double_star→"★★"`, `single_star→"★"`; all others
  passthrough). *(Mapping flagged — OQ7.)*

### BPM display

- From `song.timing.bpms()`: empty → `"?"`; all equal → `"140"`; otherwise `"128-175"` (min–max).
  Pure helper `format_bpm_range(const TimingData&)`. *(First-vs-range flagged — OQ4.)*

### Audio preview

- Source: `song.resolved_music_path` (may be empty). Start point: `metadata.sample_start` when `> 0`,
  else `kDefaultPreviewStartSeconds = 0.0`. Length: `metadata.sample_length` when `> 0`, else
  `kDefaultPreviewLengthSeconds = 12.0` (the metadata default; PRD does not specify).
- **Delay:** after a highlight change, wait `kPreviewDelaySeconds` before starting. Proposed
  **0.5 s**; not sourced from OpenITG in this plan *(OQ3)*.
- **Loop:** while active, when `position >= start + length`, seek back to `start`. Stop only on further
  navigation, Confirm, or screen `exit()`.
- **Volume:** `ctx.config->audio.preview_volume` (C2), clamped to `[0,1]`.
- **Headless/device-less:** `SoundStream::load` fails gracefully (logs, returns `false`); the preview
  state machine must not crash and `SelectScreen::render` must remain a no-op through the uninitialized
  renderer.

### Banner decode

- `Texture::from_file(path)` decodes PNG/JPG/BMP via `stb_image` to RGBA8, then reuses `from_rgba`.
  Reject/return invalid when the path is empty, the file exceeds `kMaxImageBytes = 16 MiB`, exceeds
  `kMaxImageDimension = 4096`, or no GL context is available (headless). Never throws.
- `TextureCache` keys by resolved path string and returns a stable `const Texture*`; callers check
  `.valid()` and draw a colored placeholder quad when invalid. One `Texture::from_file` per unique path.

### Library source & lifetime

- `main.cpp` owns `SongLibrary library`, `GameConfig game_config`, `HighScores high_scores`,
  `PlayRequest play_request`, and `App app` for the whole program; `ScreenContext` points at them, so
  all pointers in `PlayRequest{song,chart}` remain valid for the shell's lifetime.
- Songs with `metadata.selectable != "YES"` (case-insensitive) are hidden from the wheel *(OQ6)*.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| `preview_volume` default `0.8` | `src/data/config.hpp:33` (C2) | Sourced |
| `sample_length` default `12.0` | `src/chart/song_metadata.hpp:23` | Sourced |
| `kPreviewDelaySeconds = 0.5` | **Unsourced** (UI feel only; OpenITG not consulted) | **Flagged OQ3** — propose 0.5, do not treat as parity |
| `kDefaultPreviewStartSeconds = 0.0` | PRD silent; chosen safe default | Flagged OQ3 |
| `kMaxImageBytes = 16 MiB`, `kMaxImageDimension = 4096` | Chosen untrusted-input caps (parser-hardening discipline, `AGENTS.md`) | Decision |
| Grade label strings | `assets/data/judgment_constants.json:40-56`, `judgment_constants.hpp:83-92` | Sourced |
| `kConfigVersion` etc. | C2 `src/data/config.hpp` | Sourced (unchanged) |

No judgment windows, DP weights, grade boundaries, or life deltas are introduced or changed.

---

## Patterns to Follow

### Screen lifecycle + context (C1)
```cpp
// SOURCE: src/screens/screen.hpp:25-44
struct ScreenContext { ScreenManager* manager = nullptr; GameConfig* config = nullptr; HighScores* scores = nullptr; };
class Screen { virtual ScreenId id() const = 0; virtual void enter(ScreenContext&) {} /* ... */ };
```

### Deferred transitions via the manager (never mutate the active screen mid-update)
```cpp
// SOURCE: src/screens/title_screen.cpp:29-40, screen_manager.cpp:80-120
ctx.manager->transition_to(ScreenId::Select); // applied by apply_pending() at frame boundary
```

### Screen consumes input events already polled by App
```cpp
// SOURCE: src/screens/title_screen.cpp:32-39
for (const InputEvent& event : events) { if (event.pressed && event.action == GameAction::Confirm) { ... } }
```

### Headless-safe draw (uninitialized renderer is a no-op)
```cpp
// SOURCE: src/render/bitmap_font.hpp:11-24, src/render/gl_quad_renderer.hpp:14-36
[[nodiscard]] float text_width(const std::string&, float);
void draw_text_centered(GlQuadRenderer&, const std::string&, float, float, float, Color);
void draw_textured_quad(const Rect&, const Texture&, const UVRect&, Color);
```

### Audio playback + position query
```cpp
// SOURCE: src/audio/sound_stream.hpp:21-38
bool load(const std::string&); bool play(); void stop(); bool seek_seconds(double);
double get_position_seconds() const; double get_length_seconds() const; void set_volume(float);
```

### Chart key + best-grade lookup (C2)
```cpp
// SOURCE: src/data/high_scores.hpp:39,59-60
[[nodiscard]] std::string make_chart_key(const Song&, const Chart&);
[[nodiscard]] const ScoreRecord* find_high_score(const HighScores&, const std::string&);
```

### Config → gameplay options
```cpp
// SOURCE: src/gameplay/speed_mod.hpp:22, src/data/config.hpp:41-45
[[nodiscard]] bool parse_speed_mod(std::string_view, SpeedMod&); // "1x"/"C400"/"M600"
// GameplaySettings{speed_mod, scroll("up"/"down"), fail_enabled}; OffsetSettings.global_offset_seconds
```

### Pure option/load helper header (no SDL/GL)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:1-19, src/data/config.hpp:9-18
// Keep pure logic free of platform/audio/GL so it is directly unit-testable.
```

### Test idiom + registration
```cpp
// SOURCE: tests/song_library_test.cpp:7-24, tests/judgment_constants_test.cpp:13-19
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
```
```cmake
# SOURCE: tests/CMakeLists.txt:156-164
add_executable(select_screen_test select_screen_test.cpp)
target_link_libraries(select_screen_test PRIVATE blaze4k_core)
add_test(NAME select_screen_test COMMAND select_screen_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/select_screen.hpp` | CREATE | Real Song Select screen: wheel state, difficulty selection, preview owner, test accessors |
| `src/screens/select_screen.cpp` | CREATE | Library→wheel flattening, navigation, best-grade/BPM/grade-label helpers, headless rendering |
| `src/screens/gameplay_screen.hpp` | CREATE | Thin `Screen` host around `GameplayView` (reads `PlayRequest`) |
| `src/screens/gameplay_screen.cpp` | CREATE | enter→init from request, held-column tracking, update/render/exit |
| `src/screens/play_request.hpp` | CREATE | Pure `PlayRequest{song,chart,options}` handoff struct (header-only) |
| `src/audio/preview_player.hpp` | CREATE | `PreviewPlayer` + observable `PreviewState` for delayed/looping preview |
| `src/audio/preview_player.cpp` | CREATE | Delay timer, load/seek/play, loop-at-length, stop; headless-safe |
| `src/render/texture_cache.hpp` | CREATE | Path-keyed `Texture` cache + placeholder accessor |
| `src/render/texture_cache.cpp` | CREATE | Lazy `Texture::from_file` per path; clear() |
| `src/render/stb_image_impl.cpp` | CREATE | `STB_IMAGE_IMPLEMENTATION` translation unit (`<stb_image.h>`) |
| `src/render/texture.hpp` | UPDATE | Add `static Texture from_file(const std::string&)` declaration |
| `src/render/texture.cpp` | UPDATE | Implement `from_file` via `stb_image` + size/dimension caps, reusing `from_rgba` |
| `src/gameplay/gameplay_options.hpp` | CREATE | Move `GameplayOptions` here; declare `gameplay_options_from_config` + `derive_best_grade`-independent helpers |
| `src/gameplay/gameplay_options.cpp` | CREATE | `gameplay_options_from_config(const GameConfig&)` using `parse_speed_mod` |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Include `gameplay_options.hpp`; remove the now-duplicated `GameplayOptions` definition (no behavior change) |
| `src/screens/screen.hpp` | UPDATE | Add `library`, `constants`, `play_request` pointers + `input_reference_ns` (all defaulted); forward decls |
| `src/screens/screen_manager.cpp` | UPDATE | `handle_back()` and `back_navigates()`: `Gameplay → Select` abort *(OQ9)* |
| `src/screens/screen_manager.hpp` | UPDATE | (comment only) document Gameplay in the back-navigation contract |
| `src/main.cpp` | UPDATE | `--songs <dir>`, `--start-screen <name>`, scan library, attach context, register Select/Gameplay, set `input_reference_ns` |
| `CMakeLists.txt` | UPDATE | Add the six new `.cpp` files to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `select_screen_test` and `preview_player_test` |
| `tests/select_screen_test.cpp` | CREATE | Wheel nav, difficulty nav, best-grade lookup, options derivation, Confirm→Gameplay handoff, empty-library safety |
| `tests/preview_player_test.cpp` | CREATE | Delay scheduling, cancel-on-nav, stop, loop math, headless no-crash |

Not modified: `src/timing/*`, `src/chart/*` (parser/scanner), `src/gameplay/{judgment,score,life,note_field}*`,
`src/render/{gl_quad_renderer,bitmap_font}.cpp`, `src/data/*`, existing `select_placeholder_screen.*`
(kept, no longer registered).

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Extract `GameplayOptions` + config mapping (pure)

- **Files**: `src/gameplay/gameplay_options.hpp`, `src/gameplay/gameplay_options.cpp`
- **Action**: CREATE; then UPDATE `src/gameplay/gameplay_view.hpp`
- **Implement**: Move the struct verbatim from `gameplay_view.hpp:22-27` into `gameplay_options.hpp`
  (`#include "chart/timing_data.hpp"` + `"gameplay/speed_mod.hpp"`):
  ```cpp
  struct GameplayOptions { SpeedMod speed{}; ScrollDirection scroll = ScrollDirection::Up;
                           double global_offset_seconds = 0.0; bool fail_enabled = true; };
  [[nodiscard]] GameplayOptions gameplay_options_from_config(const struct GameConfig& config);
  ```
  `gameplay_options_from_config` includes `data/config.hpp`, parses `gameplay.speed_mod` with
  `parse_speed_mod` (invalid → `SpeedMod{}` + `[GameplayOptions]` warning), maps
  `scroll=="down"→Down`, copies `offset.global_offset_seconds` and `gameplay.fail_enabled`.
  In `gameplay_view.hpp`: replace the struct body with `#include "gameplay/gameplay_options.hpp"`.
- **Mirror**: `src/gameplay/speed_mod.hpp:22-31`, `src/data/config.hpp:41-45`.
- **Validate**: `cmake --build build -j16` (after Task 12 registers the new `.cpp`).

### Task 2: `Texture::from_file` + `stb_image` TU

- **Files**: `src/render/texture.hpp`, `src/render/texture.cpp`, `src/render/stb_image_impl.cpp`
- **Action**: UPDATE, UPDATE, CREATE
- **Implement**:
  - `stb_image_impl.cpp`: `#define STB_IMAGE_IMPLEMENTATION` then `#include <stb_image.h>` (the `stb`
    interface target already provides the include dir; no other TU defines it — verified).
  - `texture.hpp`: add `static Texture from_file(const std::string& path);`.
  - `texture.cpp`: `kMaxImageBytes = 16u<<20`, `kMaxImageDimension = 4096`. Reject empty path / missing
    file / oversize file; if `!gl_available()` return invalid **without decoding** (keeps headless
    tests fast); else `stbi_load(path, &w,&h,&ch,4)`, bounds-check `w,h` against the cap, call
    `from_rgba`, `stbi_image_free`. Any failure → invalid texture + `[Texture]` log; never throws.
- **Mirror**: `src/render/texture.cpp:47-80` (`from_rgba` guards), `src/data/judgment_constants_loader.cpp` size cap.
- **Validate**: `cmake --build build -j16`.

### Task 3: `TextureCache`

- **Files**: `src/render/texture_cache.hpp`, `src/render/texture_cache.cpp`
- **Action**: CREATE
- **Implement**: `class TextureCache` holding `std::unordered_map<std::string, Texture>`:
  `const Texture* get(const std::string& path)` returns `nullptr` for empty path, else lazily
  `emplace(path, Texture::from_file(path))` and returns the stable entry pointer; `void clear()`;
  `bool empty() const`. Move-only `Texture` is stored via `emplace` (rehash moves are safe).
- **Mirror**: `src/render/texture.hpp:16-40` (move semantics).
- **Validate**: `cmake --build build -j16`.

### Task 4: `PreviewPlayer`

- **Files**: `src/audio/preview_player.hpp`, `src/audio/preview_player.cpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  enum class PreviewState { Idle, Waiting, Active };
  class PreviewPlayer {
  public:
      void request(const std::string& path, double start_seconds, double length_seconds);
      void update(double fixed_dt);
      void stop();
      void set_volume(float volume);
      [[nodiscard]] PreviewState state() const { return state_; }
      [[nodiscard]] const std::string& requested_path() const { return path_; }
      [[nodiscard]] int load_attempts() const { return load_attempts_; }
      [[nodiscard]] double delay_seconds() const { return delay_seconds_; }
      void set_delay_seconds(double s) { delay_seconds_ = s > 0.0 ? s : 0.0; }
  private:
      SoundStream stream_; PreviewState state_ = PreviewState::Idle;
      std::string path_; double start_ = 0.0, length_ = 0.0, timer_ = 0.0;
      double delay_seconds_ = kPreviewDelaySeconds; int load_attempts_ = 0;
  };
  inline constexpr double kPreviewDelaySeconds = 0.5; // OQ3
  ```
  `request`: `stream_.stop()`, store path/start/length, `timer_=0`, `state_=Waiting`.
  `update`: `Waiting` → `timer_ += dt`; when `timer_>=delay_` count an attempt; if `stream_.load(path_)`
  seek to `start_` and `play()` (volume applied), `state_=Active`, else `state_=Idle`. `Active` → if
  `stream_.is_playing()` and `length_>0` and `position >= start_+length_`, seek back to `start_`.
  `stop`: `stream_.stop()`, `state_=Idle`, clear path. `set_volume` clamps `[0,1]` and forwards.
  No exceptions; empty path → stay `Idle`.
- **Mirror**: `src/audio/sound_stream.hpp:21-38,117-165`.
- **Validate**: `cmake --build build -j16`.

### Task 5: `PlayRequest` + `ScreenContext` extension

- **Files**: `src/screens/play_request.hpp` (CREATE), `src/screens/screen.hpp` (UPDATE)
- **Action**: CREATE, UPDATE (additive)
- **Implement**:
  - `play_request.hpp`: `#include "gameplay/gameplay_options.hpp"`; forward-declare `Song`, `Chart`:
    ```cpp
    struct PlayRequest { const Song* song = nullptr; const Chart* chart = nullptr; GameplayOptions options{}; };
    ```
  - `screen.hpp`: add `<cstdint>`; forward-declare `SongLibrary`, `JudgmentConstants`, `PlayRequest`;
    extend `ScreenContext`:
    ```cpp
    const SongLibrary* library = nullptr;         // C3 reads; owned by main
    const JudgmentConstants* constants = nullptr; // GameplayScreen init; owned by App
    PlayRequest* play_request = nullptr;          // C3 writes; GameplayScreen reads
    std::uint64_t input_reference_ns = 0;         // set by main each tick (gameplay input aging)
    ```
    All defaulted → C1/C2 aggregate init unchanged.
- **Mirror**: `src/screens/screen.hpp:25-29` (C2's additive extension).
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` still passes.

### Task 6: `SelectScreen` — data model & navigation (pure logic)

- **Files**: `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`
- **Action**: CREATE
- **Implement**:
  - Build a flat, ordered list of selectable songs from `ctx.library->packs()` (skip
    `selectable != "YES"` case-insensitively), remembering pack boundaries for headers. Reset
    `selected_song_ = 0`, `selected_chart_ = 0` on `enter`.
  - `update`: `preview_.update(fixed_dt)` every tick; for each `pressed` event: `Up/Down` move the song
    (wrap), `Left/Right` adjust the chart index (clamp `[0, charts.size()-1]`), `Confirm` → fill
    `ctx.play_request->song/chart/options = gameplay_options_from_config(*ctx.config)` and
    `ctx.manager->transition_to(ScreenId::Gameplay)`. On song change, call the preview request helper
    (`sample_start`, `sample_length`). `enter` sets preview volume and requests the first preview.
  - `exit`: `preview_.stop()`; `texture_cache_.clear()` (GL resources released on screen change).
  - Pure helpers (anonymous namespace or file-local + declared for tests): `format_bpm_range(const
    TimingData&)`, `grade_display_label(const std::string&)`, `best_grade_for(ctx, song, chart)`.
  - Test accessors: `song_count()`, `selected_song_index()`, `selected_chart_index()`,
    `selected_song()`, `selected_chart()`, `chart_count()`.
  - `render`: pack header; highlighted title/artist/BPM; banner via `texture_cache_.get(...)` +
    `draw_textured_quad` (placeholder `draw_quad` when invalid); a windowed list of nearby songs;
    difficulty rows (`difficulty` + meter + best-grade label); footer hints. All through the no-op
    headless renderer.
- **Mirror**: `src/screens/title_screen.cpp` / `select_placeholder_screen.cpp` (lifecycle + draw),
  `src/chart/song_library.cpp:48-64` (iterate packs/songs).
- **Validate**: `cmake --build build -j16`.

### Task 7: `GameplayScreen` — thin `GameplayView` host

- **Files**: `src/screens/gameplay_screen.hpp`, `src/screens/gameplay_screen.cpp`
- **Action**: CREATE
- **Implement**:
  - `enter`: if `ctx.play_request` has a non-null chart, build `audio_path =
    song->resolved_music_path` (warn when empty), select `const JudgmentConstants& c = ctx.constants ?
    *ctx.constants : kDefaultConstants`, and `view_.init(*chart, c, audio_path, options)`;
    `active_ = view_.is_ready()`. Log `[GameplayScreen] started '<title>' <difficulty> <meter>`.
  - `update`: if `!active_` return; track `held_[4]` from `Left/Down/Up/Right` press/release; call
    `view_.handle_input_events(events, ctx.input_reference_ns)` then `view_.update(fixed_dt, held_)`;
    when `view_.outcome() != InProgress`, `ctx.manager->transition_to(ScreenId::Select)` (Results is
    C7 — OQ1).
  - `render`: `view_.render(renderer, w, h)`. `exit`: `view_.shutdown()`, `active_=false`.
  - `[[nodiscard]] bool is_ready() const`.
- **Mirror**: `src/main.cpp:172-234` (how the demo drives `GameplayView`), `gameplay_view.hpp:49-74`.
- **Validate**: `cmake --build build -j16`.

### Task 8: `ScreenManager` back semantics for Gameplay

- **Files**: `src/screens/screen_manager.cpp`, `src/screens/screen_manager.hpp`
- **Action**: UPDATE (small)
- **Implement**: in `handle_back()` add `else if (active_id_ == ScreenId::Gameplay)
  transition_to(ScreenId::Select);` and include `ScreenId::Gameplay` in `back_navigates()` so Escape
  aborts a run to Select instead of quitting the app. Keep the "keep in sync" comment accurate.
  *(Proposed behavior; OQ9.)*
- **Mirror**: `src/screens/screen_manager.cpp:122-135`.
- **Validate**: `cmake --build build -j16`; existing `screen_manager_test` still passes.

### Task 9: `main.cpp` — scan library, wire context, register screens

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add CLI: `--songs <dir>` and `--start-screen <title|select>`; update `print_help()`
    (`main.cpp:26-39`).
  - Before constructing `App` (or before `shell` setup): `blaze4k::SongLibrary library;` resolve the songs
    dir = `--songs` if given, else first existing candidate of `songs`, `data/songs`,
    `<exe_dir>/songs`; call `library.scan_directory(dir)` when found; log the count; never fatal.
    Set fallback banner/background only if a file exists (none ships today).
  - Add `blaze4k::PlayRequest play_request;` in `main` scope.
  - In the shell branch: register `SelectScreen` and `GameplayScreen` instead of
    `SelectPlaceholderScreen` (`main.cpp:240`); attach
    `shell->context().library = &library; shell->context().constants = &app.judgment_constants();
    shell->context().play_request = &play_request;`.
  - `shell->start(...)`: honor `--start-screen select`, else `ScreenId::Title`.
  - In the shell update callback set `shell->context().input_reference_ns = app.input_reference_ns();`
    before `shell->update(...)`.
  - Keep the `--gameplay-demo` path and both save-on-exit calls unchanged.
- **Mirror**: `src/main.cpp:124-160` (C2 path resolution), `236-268` (shell wiring).
- **Validate**: `cmake --build build -j16`; smoke in Validation.

### Task 10: Register sources and test targets

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add to the `blaze4k_core` list (`CMakeLists.txt:80-111`):
  `src/gameplay/gameplay_options.cpp`, `src/render/stb_image_impl.cpp`,
  `src/render/texture_cache.cpp`, `src/audio/preview_player.cpp`,
  `src/screens/select_screen.cpp`, `src/screens/gameplay_screen.cpp`.
  Append `select_screen_test` and `preview_player_test` blocks mirroring
  `tests/CMakeLists.txt:147-164`.
- **Mirror**: `CMakeLists.txt:80-111`, `tests/CMakeLists.txt:156-164`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 11: `preview_player_test`

- **File**: `tests/preview_player_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no real audio file, no GUI):
  1. `set_delay_seconds(0.25)`; `request("does-not-exist.ogg", 10.0, 12.0)` → `state()==Waiting`,
     `load_attempts()==0`.
  2. `update(0.1)` twice → still `Waiting`, `load_attempts()==0`; `update(0.1)` → `load_attempts()==1`
     (load attempt happened after the delay; fake path fails → `state()==Idle`, no crash).
  3. Re-`request` a second path; assert `requested_path()` updated and `state()==Waiting` (timer reset;
     `load_attempts()` unchanged until delay elapses again).
  4. `stop()` → `Idle`, and `update(1.0)` does not reload.
  5. Empty path or empty `request("",...)` → stays `Idle`, no attempt.
  6. `set_volume(2.0)` / `set_volume(-1.0)` do not crash (clamped internally).
- **Mirror**: `tests/audio_test.cpp` structure, `tests/judgment_constants_test.cpp:13-19`.
- **Validate**: `./build/tests/preview_player_test` → 0.

### Task 12: `select_screen_test`

- **File**: `tests/select_screen_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; synth temp pack like `song_library_test.cpp:33-108`, fake banner/audio
  text files so no real decode/playback; use a real `ScreenManager`):
  1. **Wheel load** — scan a temp pack with 3 selectable songs + 1 `#SELECTABLE:NO;` song; assert
     `song_count()==3` (NO hidden) and `selected_song_index()==0`.
  2. **Song nav** — `press(Up)` wraps to last, `press(Down)` wraps to first; highlight (`selected_song`)
     follows.
  3. **Difficulty nav** — a 3-chart song: `press(Right)` walks 0→1→2 then clamps at 2; `press(Left)`
     walks back to 0 and clamps; a 1-chart song ignores Left/Right.
  4. **Best grade** — seed `HighScores` with a record for `make_chart_key(song, chart)`; assert
     `best_grade_for` returns it and `grade_display_label("quad_star")=="★★★★"` /
     `("S+")=="S+"` / unknown passthrough.
  5. **BPM formatting** — `format_bpm_range` on empty (`"?"`), single (`"140"`), multi-segment
     (`"128-175"`).
  6. **Options derivation** — `gameplay_options_from_config` with `speed_mod="C400"`,
     `scroll="down"`, `fail_enabled=false`, offset `0.02` → `SpeedModType::CMod`, `value==400`,
     `ScrollDirection::Down`, `!fail_enabled`, `0.02`; invalid `speed_mod="zzz"` → default XMod (no
     crash).
  7. **Confirm handoff** — `press(Confirm)` on Select transitions the manager to
     `ScreenId::Gameplay`; the registered `GameplayScreen` `is_ready()` and the `PlayRequest` holds the
     highlighted `song`/`chart`. Then `press(Back)` returns to Select.
  8. **Empty library safety** — a `SelectScreen` over an empty `SongLibrary` enters, updates, renders
     (uninitialized renderer) and ignores nav without crashing; Confirm does nothing.
- **Mirror**: `tests/song_library_test.cpp:29-208`, `tests/screen_manager_test.cpp:37-92,301-372`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect **19/19**).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 19/19: 17 existing + select_screen_test + preview_player_test)
ctest --test-dir build --output-on-failure

# Explicit new tests
./build/tests/select_screen_test
./build/tests/preview_player_test

# Purity: select/option logic must stay free of SDL/GL/audio
rg -n "SDL_|glad|miniaudio" src/screens/select_screen.* src/gameplay/gameplay_options.*   # expected: no matches

# stb_image is the only new decode TU
rg -n "STB_IMAGE_IMPLEMENTATION" src/render
```

## End-to-End Verification

All steps are headless, non-blocking (no window, no GL, no audio device required), and isolated via
`--data-dir` so the developer's real `data/` and song library are untouched.

1. **Scan a real library and boot straight into Select** (AC 1 + wiring):
   ```bash
   rm -rf /tmp/blaze4k-e2e-select
   ./build/blaze-4k --headless --smoke-test 30 \
     --start-screen select --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-select
   # exits 0; logs "[SongLibrary] ...", "[SelectScreen] library: 4 songs, 6 charts",
   # "[ScreenManager] enter Select"; no window/GL/audio device needed
   ```
2. **Empty/missing library never crashes** (AC 1 robustness):
   ```bash
   ./build/blaze-4k --headless --smoke-test 10 --start-screen select \
     --songs /tmp/does-not-exist --data-dir /tmp/blaze4k-e2e-select; echo "exit=$?"
   # exit 0; logs a SongLibrary warning and "[SelectScreen] library: 0 songs"
   ```
3. **Wheel + difficulty + best grade + Confirm→Gameplay** (AC 3/4/5) — driven by synthetic input
   events against the real manager (headless, deterministic):
   ```bash
   ./build/tests/select_screen_test
   # asserts Up/Down/Left/Right navigation, star/grade label mapping, seeded best-grade lookup,
   # config→options derivation, Confirm publishes PlayRequest and transitions to Gameplay where
   # GameplayScreen.is_ready() is true, and Back returns to Select
   ```
4. **Audio preview scheduling** (AC 2) — no audio device required:
   ```bash
   ./build/tests/preview_player_test
   # asserts request→Waiting→load attempted only after the delay, cancel-on-nav resets the timer,
   # duplicate/re-request replaces the pending track, stop() cancels, and a missing file never crashes
   ```
5. **Preview uses `sample_start`** (AC 2 provenance): `Blaze Anthem` declares `#SAMPLESTART:30.0;`
   and `Northern Lights` declares `#SAMPLESTART:45.0;` (fixtures). The preview request helper passes
   `metadata.sample_start`; `select_screen_test` case 1 exercises the request on a fixture with a
   nonzero sample start (asserts `requested_path()` non-empty and the start value flows through).
6. **Regression**: `ctest --test-dir build --output-on-failure` → **19/19**, including unchanged
   `metronome_sync_test`, `music_clock_test`, `judgment_engine_test`, and `config_persistence_test`;
   the demo harness still runs and gameplay timing is untouched:
   ```bash
   ./build/blaze-4k --headless --gameplay-demo \
     "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm" --smoke-test 60
   ```
7. `git status` shows only additions under `src/screens/`, `src/audio/`, `src/render/`, `src/gameplay/`,
   the `screen.hpp`/`gameplay_view.hpp`/`screen_manager.*`/`main.cpp` edits, the CMake files, and the
   two new tests — no changes to `src/timing/`, `src/chart/`, or the judgment/scoring/note-field code.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| **No Gameplay screen exists**; the AC "gameplay launches" could balloon into building the whole gameplay/results screen | Ship a deliberately thin `GameplayScreen` that only hosts `GameplayView` and returns to Select on completion; Results/retry explicitly deferred to C7. Flagged **OQ1** | **In scope (thin)** — Results **out of scope** |
| Malicious/huge banner image exhausts memory during decode | Reject files > 16 MiB before decode; bounds-check decoded dimensions ≤ 4096; `stbi_load` result null-checked; reuse `from_rgba` guards; never throws; parser-hardening discipline (`AGENTS.md`) | **In scope** |
| Decoding banners on every frame stalls the wheel | `TextureCache` keys by resolved path; one decode/upload per unique banner; `clear()` on `exit` | **In scope** |
| Preview audio overlaps with gameplay audio or leaks across screens | `SelectScreen::exit()` calls `preview_.stop()`; the manager runs `exit()` before `enter()`; `SoundStream::stop()` seeks to 0 | **In scope** |
| Headless / device-less boot crashes or blocks on audio | `SoundStream::load` already fails gracefully; `PreviewPlayer` treats a failed load as `Idle`; tests use fake audio paths; `Texture::from_file` skips decode when no GL | **In scope** |
| `ScreenContext` extension breaks C1/C2 code or aggregate init | Four defaulted members with forward declarations only; existing tests re-run (Tasks 5/8/12) | **In scope** |
| Dangling `PlayRequest`/`ScreenContext` pointers | `main` owns `library`, `game_config`, `high_scores`, `play_request`, `app` for the whole run; documented lifetime contract | **In scope** |
| `GameplayScreen` gets events without `input_reference_ns` (the demo path uses `App` directly) | `main` sets `ctx.input_reference_ns = app.input_reference_ns()` each tick before `shell->update`; held columns tracked from events, not `InputManager` | **In scope** |
| Preview loop never re-triggers because `length` is 0 | `kDefaultPreviewLengthSeconds = 12.0` when `sample_length <= 0`; loop only when `length > 0` | **In scope** |
| Wheel ordering/scroll off-by-one on pack boundaries | Flatten once on `enter` and keep a parallel pack-header index; unit-tested wrap in `select_screen_test` case 2 | **In scope** |
| `--start-screen`/songs-folder default is product-owner preference | Non-fatal candidate list; scanned count logged; empty wheel is safe. Flagged **OQ4** | **In scope** — flagged |
| Gameplay completion → Results, retry, pause, difficulty reorder, options menu | Explicitly owned by C4/C7; `GameplayScreen` returns to Select | **Out of scope** — flagged |
| Rendering simfile *backgrounds* (dimmed) during gameplay | Phase D (D1); this ticket renders banner art only | **Out of scope** — flagged |

---

## Decisions

- **One real `SelectScreen`, thin `GameplayScreen`.** The select wheel is the deliverable; gameplay
  launch is satisfied by a minimal host around the already-tested `GameplayView`, not a reimplementation.
- **Keyboard + pad via the four directional actions.** Arrow keys/DFJK/pad D-pad/face buttons already
  share `Left/Down/Up/Right`; no input-layer change and both devices are covered by construction.
- **Difficulty is passthrough** (`chart.difficulty` + `chart.meter`), matching the locked PRD decision.
- **Best grade via C2's `make_chart_key` + `find_high_score`**, so the wheel and Results share one key.
- **Preview uses `SoundStream` with a configurable delay and loop**, driven by the injected fixed_dt —
  no wall-clock in the preview path.
- **Banner decode via vendored `stb_image`** (already FetchContent'd) behind `Texture::from_file` +
  `TextureCache`, with a colored placeholder when invalid/headless.
- **`GameplayOptions` moves to its own header** with a pure `gameplay_options_from_config`, so options
  mapping is testable without constructing a `GameplayView`.
- **Additive `ScreenContext` extension** (library/constants/play_request/input_reference_ns), mirroring
  C2's pattern, rather than new globals.
- **Back from Gameplay aborts to Select** (proposed), keeping Escape from quitting mid-run.

---

## Open Questions

1. **BLOCKING-ish — gameplay launch scope (OQ1).** #18's AC says "gameplay launches with the chosen
   chart and current options", but no Gameplay *screen* exists and Results/pause/retry are C7.
   **Proposed:** ship the thin `GameplayScreen` host here (init from `PlayRequest`, return to Select on
   completion), and leave Results/retry/pause to C7. Confirm the boundary, or state that #18 only needs
   the selection wired into the existing `--gameplay-demo`-style path (which would instead require
   main-level mode switching and no screen).
2. **Non-blocking — banner decoding.** PRD lists `stb_truetype` (fonts) and defers background image
   rendering to D1, but §7.3/§12 explicitly require *banner art* in Select. **Proposed:** add
   `stb_image` (already vendored) for banners now. Alternative: placeholder-only in C3 and real art in
   D1.
3. **Non-blocking — preview delay/start defaults.** No OpenITG source consulted; `kPreviewDelaySeconds
   = 0.5` and start `0.0` are chosen for feel. Confirm 0.5 s (or specify the OpenITG value + commit).
4. **Non-blocking — songs directory + BPM display.** Proposed default candidates `songs`, `data/songs`,
   `<exe_dir>/songs` with `--songs` override; BPM shown as a min–max range. Confirm location and
   whether the first BPM (or a specific segment) is preferred.
5. **Non-blocking — difficulty navigation clamp vs wrap.** Proposed clamp at the ends (ITG-like).
   Confirm wrap if desired.
6. **Non-blocking — `SELECTABLE:NO` songs.** The scanner does not filter them; proposed to hide them
   in Select. Confirm (and whether other metadata, e.g. `title_translit`, should be preferred for
   display when present).
7. **Non-blocking — best-grade rendering.** Proposed star glyphs (`★★★★`, `★★★`, `★★`, `★`) for the
   four star tiers and passthrough for letter grades; the stored label stays unchanged.
8. **Non-blocking — preview one-shot vs loop.** Proposed loop the `sample_length` window until
   navigation. Confirm.
9. **Non-blocking — Escape during gameplay.** Proposed `Gameplay → Select` (abort). If pause is wanted
   instead, that belongs with the gameplay-screen ticket (C7).

---

## Acceptance Criteria

- [ ] Entering Select lists the scanned library as a pack-grouped, scrollable wheel with banner art,
      artist, and BPM (Task 6; `select_screen_test` cases 1–2; E2E 1–2)
- [ ] Changing the highlighted song requests its audio preview after a sensible delay and cancels the
      previous/preview on navigation (Task 4/6; `preview_player_test`; E2E 4–5)
- [ ] The highlighted song shows passthrough difficulty labels + foot ratings with the best grade per
      chart (Tasks 5/6; `select_screen_test` cases 3–5)
- [ ] All wheel operations are navigable with keyboard **and** pad (Task 6; `select_screen_test`
      cases 2–3; mappings verified in `input_manager.cpp:36-72`)
- [ ] Confirm launches gameplay with the chosen chart and current options (Tasks 1/5/7; E2E 3)
- [ ] Corrupt/missing library, missing art, and missing audio never crash (Tasks 2/4/6; E2E 2, 4)
- [ ] `ctest --test-dir build --output-on-failure` → **19/19**; `--gameplay-demo` and all prior tests
      stay green (Task 12; E2E 6)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic`; no changes to `src/timing/` or judgment/
      scoring/note-field behavior; `select_screen`/`gameplay_options` stay SDL/GL/audio-free
