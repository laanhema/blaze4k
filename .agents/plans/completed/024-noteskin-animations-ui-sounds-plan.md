# Plan: Noteskin Visuals, Judgment/Combo Pop Animations, and UI Sounds (D2)

## Summary

Phase D polish has three distinct deliverables behind issue #24: (A) replace the Phase B
procedural placeholder noteskin with real arrow art (distinct tap/hold/roll/mine, direction-aware),
(B) animate judgment and combo popups driven *directly* by the B4 event log already consumed by
scoring, and (C) play UI sounds on menu navigation/confirmations. All three are presentation-only
— audio/graphics are unlocked decisions (no OpenITG parity), only the **2D-only** locked decision
constrains them. The plan follows the repo's proven seams: a **pure, headless-testable model** for
each feature plus a thin draw/play layer that is a no-op without GL/audio (`NoteSkin`,
`BackgroundRenderer`, `HudRenderer`, `PreviewPlayer`). Noteskin art and UI sounds are **synthesized
procedurally at runtime** (`Texture::from_rgba`, mirroring `BackgroundRenderer`'s fallback gradient
and `write_click_track`'s WAV synth) so the engine stays asset-free and fresh-clone safe. This issue
is broad and **should ship as three separate PRs/sub-issues** (A/B/C); the plan documents all three
but keeps task groups independently releasable.

## User Story

As a player
I want a real noteskin, judgment and combo pop animations, and UI sounds
So that the game looks and sounds like a shipped arcade product.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY / ENHANCEMENT |
| Complexity | HIGH (three independent systems; recommended split into 3) |
| Systems Affected | `src/render/` (new art rasterizer), `src/gameplay/` (noteskin, renderer, new animator, gameplay_view, hud palette), `src/audio/` (new UI sound player), `src/screens/` (context + manager triggers), `src/main.cpp`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #24 ([D2]) |
| PRD refs | §12 Phase D ("Noteskin visuals, judgment/combo pop animations, UI sounds"), §15 locked decisions (2D only), §4/§5 (rendering), §6 pattern 3 (data-driven constants) |
| Depends on | B3 (#11 note field rendering), B4 (#12 judgment event log), D1 (#23 background) — all merged |

---

## Scope & Split Recommendation

Issue #24 bundles three deliverables that PRD §14/§12 explicitly calls "independently shippable".
**Recommendation: split #24 into three sub-issues and three PRs**, in this order (lowest coupling
first):

| Sub-issue | Deliverable | AC | Releasable alone |
|-----------|-------------|----|------------------|
| D2a | Noteskin visuals (arrow art) | AC1 | Yes |
| D2b | Judgment + combo pop animations | AC2, AC3 | Yes (needs D2a only for the head textures it reuses) |
| D2c | UI sounds on menus | AC4 | Yes |

This plan covers all three as task groups **A / B / C**; the shared `CMakeLists.txt` / test
registration task (D1) can be split per PR. If only one PR is taken now, do **A** (it is the
biggest visible win and unblocks B's judgment sprites, which reuse `NoteSkin`). Two out-of-scope
D-phase items are **not** part of this issue: results grade animations / "NEW RECORD" flow (§12
Phase D, separate), and a `stb_truetype` font atlas (see OQ6).

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured for g++/Fedora 44 |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` (no `-Werror`) from root CMake (`CMakeLists.txt:10-14`) |
| Cores | 16 | `-j16` safe |
| Baseline tests | **28/28 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 28" (0.37 s), run this session |
| Existing noteskin | `src/gameplay/noteskin.hpp:17-40`, `noteskin.cpp:25-48` | Procedural colored **1x1** textures; the "Phase B placeholder" to replace |
| Note draw site | `src/gameplay/note_field_renderer.cpp:35-111` | Receptors, bodies, tails, heads, mines — all `draw_textured_quad` |
| Event hook | `src/gameplay/gameplay_view.cpp:166-169` | `new_events_` already drained for score+life; animator consumes the same vector |
| Combo value | `src/gameplay/score_keeper.hpp:22` (`ScoreState::combo`), `gameplay_view.cpp:221` | Event-sourced; milestone detection reads it |
| Input dispatch | `src/screens/screen_manager.cpp:170-232` | Central press detection already exists (`back/confirm/options`); add move/confirm/back sounds here |
| Audio one-shots | `src/audio/audio_engine.cpp:19-37`; `sound_stream.cpp:47-60` | `AudioEngine` lazily inits; `raw_engine()` exposes `ma_engine` for `ma_engine_play_sound` |
| WAV synth idiom | `src/audio/metronome.cpp:68-137` | `write_click_track` — reuse the fixed-layout 16-bit mono PCM writer style |
| Asset copy | `CMakeLists.txt:155-160` | `assets/` copied next to the binary; `assets/` currently holds only `backgrounds/fallback.png` + `data/judgment_constants.json` |
| Test idiom | `tests/texture_test.cpp:12-19`; `tests/CMakeLists.txt` (one target/test); `tests/screen_manager_test.cpp` | `TEST_CHECK` abort macro; headless no-GL pattern |
| Fixtures | `tests/fixtures/reference_pack/`; `tests/fixtures/sync_test/metronome.sm` | Synchronous gameplay demo for E2E |

**Start green, stay green:** 28 tests pass; this plan adds **3** targets (`note_art_test`,
`judgment_animator_test`, `ui_sounds_test`) → **31 expected**. No changes to
`src/chart/*`, `src/timing/judgment_constants.*`, `src/gameplay/judgment_engine.*`,
`score_keeper.*`, `life_keeper.*`, `note_field.*`, or `src/data/*`. **The judgment/scoring path is
untouched** (core principle 1).

---

## Pinned Semantics

Authority: **PRD §12 Phase D / §15**, the existing **`Texture` / `GlQuadRenderer` / `NoteSkin` /
`HudRenderer` / `PreviewPlayer`** contracts. **No OpenITG value is required** for any of the three:
colors, shapes, animation curves, milestone interval, and SFX timbre are all "Tundra presentation,
unsourced" (like `hud_renderer.cpp:13`). The only locked constraint is **2D only** (textured/solid
quads; no video/3D/dancers).

### A. Noteskin art — procedural, direction-aware, tinted

`NoteSkin` keeps its `NoteStyle` colors and `column_tint`; it gains **white-alpha mask textures**
generated at init via `Texture::from_rgba`, then tinted at draw time (`draw_textured_quad` multiplies
texture × color). Pure rasterizers live in a new `src/render/note_art.{hpp,cpp}` (no GL):

```cpp
// src/render/note_art.hpp — pure, deterministic, no GL/SDL/clock.
enum class ArrowDirection { Left = 0, Down = 1, Up = 2, Right = 3 }; // == Note.column
// White RGB (1,1,1) + shape alpha coverage mask, size x size, tightly packed RGBA8.
[[nodiscard]] std::vector<uint8_t> make_arrow_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_hold_head_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_roll_head_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_mine_rgba(int size);
[[nodiscard]] std::vector<uint8_t> make_receptor_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_body_rgba(int size); // vertical bar/glow
```

Distinctness (AC1): tap = solid arrow; hold head = larger filled arrow with a square shoulder;
roll head = arrow with a dashed/segmented fill; mine = spiky radial ball; body = rounded vertical
bar. Each mask's nonzero-alpha pixel count and coverage signature differ, which the unit test pins.
A hand-verified 90° rotation helper ensures direction correctness:

```cpp
// src/gameplay/noteskin.hpp
[[nodiscard]] const Texture& head_texture(NoteType type) const; // Tap/Hold/Roll/Mine
[[nodiscard]] const Texture& receptor_texture() const;          // already exists
[[nodiscard]] const Texture& body_texture() const;
[[nodiscard]] UVRect arrow_uv(int column) const; // rotates a single arrow texture to the column dir
```

`arrow_uv` lets one arrow texture serve all four directions (rotate/swap UV corners); still pure and
tested. `NoteFieldRenderer::render` (`note_field_renderer.cpp:35-111`) picks
`skin.head_texture(item.type)` + `skin.arrow_uv(item.column)` for heads/receptors and
`skin.body_texture()` for bodies; mines use `make_mine_rgba`'s texture with the full UV.

### B. Judgment/combo pop animation — derived from the event log

New **pure** `src/gameplay/judgment_animator.{hpp,cpp}` (no clocks; render is a headless no-op).
It consumes exactly the events scoring consumes:

```cpp
class JudgmentAnimator {
public:
    void reset();
    void consume(const std::vector<JudgmentEvent>& events); // same vector as score_/life_
    void update(double fixed_dt, int combo);                 // fixed_dt = presentation only
    void render(GlQuadRenderer&, int w, int h) const;

    // Pure curves (unit-tested): pop-in then fade, clamped to [0,1].
    [[nodiscard]] static float pop_scale(double elapsed, double duration);
    [[nodiscard]] static float pop_alpha(double elapsed, double duration);
    [[nodiscard]] static bool  pop_active(double elapsed, double duration);
    // Pure label/color mapping from an event (reuses the HUD judgment palette).
    [[nodiscard]] static std::string judgment_label(const JudgmentEvent& e);
    [[nodiscard]] static Color       judgment_color(const JudgmentEvent& e);
};
```

- On `consume`, each visible event (`Tap` graded by `window`, `Miss`, `HitMine`, `HoldOk/Ng`,
  `RollOk/Ng`) sets the active popup `{label, color, elapsed=0}`. `AvoidedMine` and `RollHit` are
  stats/visual-refresh only and do **not** pop (they carry no score/combo). Multiple events in one
  tick: last wins (see OQ3).
- `update` advances `elapsed += fixed_dt` for the active popup and the combo pop; when
  `combo > 0 && combo % kComboMilestone == 0 && combo != last_milestone_`, it arms a larger combo
  pop (deduped so a milestone only fires once). `kComboMilestone` is a presentation constant
  (proposed `50`; OQ2).
- `render` draws the judgment label centered below the live combo via
  `draw_text_centered` (`bitmap_font.hpp:23`) with `pop_scale`/`pop_alpha`; the combo pop draws the
  live `combo` text larger. Draw order: after `HudRenderer` so popups sit on top.

**"Visuals match the log exactly" (AC2):** the animator is fed `new_events_`, the *same* append-only
log slice `ScoreKeeper`/`LifeKeeper` consume (`gameplay_view.cpp:166-169`) — no re-judging, no clock.
`fixed_dt` drives only the fade; it never enters judgment (documented, mirrors `PreviewPlayer`).

### C. UI sounds — central trigger, synthesized WAVs

New `src/audio/ui_sounds.{hpp,cpp}` with an injectable sink (mirrors `IAudioStream`) so the shell is
testable without an audio device:

```cpp
enum class UiSound { Move, Confirm, Back };
class IUiSoundSink { public: virtual ~IUiSoundSink() = default; virtual void play(UiSound) = 0; };
class UiSoundPlayer : public IUiSoundSink {
public:
    bool init(const std::filesystem::path& dir); // synth WAVs if missing; false w/o engine
    void play(UiSound sound) override;           // one-shot; no-op if not ready (logs once)
    [[nodiscard]] bool is_ready() const;
};
```

`init` writes three short 16-bit mono PCM WAVs (distinct pitch/envelope: move = short blip, confirm
= rising two-tone, back = falling) into `<data_dir>/sfx/`, reusing the `metronome.cpp:68-137` writer
idiom; then lazily inits `AudioEngine` (`audio_engine.cpp:19-37`) and pre-loads one `ma_sound` per
sound. `play` fires a one-shot (overlapping voices safe via `ma_engine_play_sound`); with no audio
device it logs once and stays silent (never throws, never blocks).

**Trigger point:** `ScreenContext` gains `IUiSoundSink* ui_sounds = nullptr;` (`screen.hpp:34-58`,
defaulted so all existing aggregate init is unchanged). `ScreenManager::update` already classifies
presses (`screen_manager.cpp:177-189`); add:

- `Move` on any directional press while the active screen is a menu
  (`Title`, `Attract`, `Select`, `Results`) — this covers Select's options overlay too.
- `Confirm` on `Confirm` (Attract's confirm-exit included).
- `Back` on `Back`.
- Suppressed for `Gameplay` (directions are steps), `Calibration`, `InputRemap` (their own
  navigation semantics), i.e. only the menu set above.

All triggers are `if (ctx_.ui_sounds != nullptr)` guarded, so headless unit tests and the
`--gameplay-demo` path are unaffected. **Gameplay hit/judgment SFX are out of scope** (UI/confirmation
sounds only, per AC4; OQ4).

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| 2D-only (no video/3D/dancers) | PRD §15 locked decisions; issue AC | Sourced (locked) |
| Judgment label/color palette | Reused from `hud_renderer.cpp:14-22` (unsourced presentation) | Sourced (reused) |
| Combo milestone interval | PRD/issue silent | **Design decision — OQ2** |
| Animation duration/curve | PRD/issue silent ("pops with animation") | **Design decision — OQ3** |
| Noteskin shape/art | PRD "noteskin visuals", engine-only asset philosophy | **Design decision — OQ1** |
| SFX timbre/duration | PRD "UI sounds" | **Design decision — OQ4** |
| UI trigger set / screen exclusions | Issue AC4 + existing `ScreenManager` input handling | **Design decision — OQ7** |

No judgment windows, DP weights, grade boundaries, life deltas, or timing values are introduced.

---

## Patterns to Follow

### Procedural texture init (no image files, headless-safe)
```cpp
// SOURCE: src/gameplay/noteskin.cpp:25-42
if (glad_glGenTextures == nullptr) { std::cerr << "[NoteSkin] No GL context available..."; return false; }
white_ = Texture::solid(Color{1,1,1,1});
```

### RGBA upload factory (reuse; untrusted-safe)
```cpp
// SOURCE: src/render/texture.hpp:39
static Texture from_rgba(int width, int height, const uint8_t* rgba);
```

### Batched, headless no-op quad draw
```cpp
// SOURCE: src/render/gl_quad_renderer.hpp:30-33
void draw_quad(const Rect& rect, Color color);
void draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color);
```

### Bitmap text (texture-atlased UI text; no font asset)
```cpp
// SOURCE: src/render/bitmap_font.hpp:19-24
void draw_text(...); void draw_text_centered(GlQuadRenderer&, const std::string&, float center_x, float y, float pixel, Color);
```

### Event drain + keepers (animator taps the same slice)
```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:166-169
new_events_.clear(); judge_.drain_new_events(new_events_);
score_.consume(new_events_); life_.consume(new_events_);
```

### Fixed-dt presentation timer (never a gameplay clock)
```cpp
// SOURCE: src/audio/preview_player.cpp:30-34
if (state_ == PreviewState::Waiting) { timer_ += fixed_dt; if (timer_ < delay_seconds_) return; }
```

### WAV one-shot synth (fixed-layout PCM, pure I/O)
```cpp
// SOURCE: src/audio/metronome.cpp:68-137
bool write_click_track(const std::filesystem::path& path, const MetronomeConfig& config);
```

### Central input classification in the shell
```cpp
// SOURCE: src/screens/screen_manager.cpp:177-189
for (const InputEvent& e : events) { if (!e.pressed) continue; if (e.action == GameAction::Back) ... }
```

### Test idiom + registration
```cpp
// SOURCE: tests/texture_test.cpp:12-19; tests/CMakeLists.txt (background_test block)
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
add_executable(note_art_test note_art_test.cpp)
target_link_libraries(note_art_test PRIVATE tundra_core)
add_test(NAME note_art_test COMMAND note_art_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/note_art.hpp` | CREATE | Pure white-alpha mask rasterizers + `ArrowDirection` |
| `src/render/note_art.cpp` | CREATE | Arrow/hold/roll/mine/receptor/body mask generation |
| `src/gameplay/noteskin.hpp` | UPDATE | Add `head_texture`, `body_texture`, `arrow_uv`; store generated textures |
| `src/gameplay/noteskin.cpp` | UPDATE | Build masks via `note_art` + `Texture::from_rgba`; destroy in `shutdown` |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | Use per-type/direction textures; body mask |
| `src/gameplay/hud_renderer.hpp` | UPDATE | Expose `judgment_color(...)` (shared palette) |
| `src/gameplay/hud_renderer.cpp` | UPDATE | Move chip palette into the shared accessor |
| `src/gameplay/judgment_animator.hpp` | CREATE | Pure popup/combo animation model + static curves/labels |
| `src/gameplay/judgment_animator.cpp` | CREATE | Consume events, advance timers, draw popups (headless no-op) |
| `src/gameplay/gameplay_view.hpp` | UPDATE | Add `JudgmentAnimator` member |
| `src/gameplay/gameplay_view.cpp` | UPDATE | Reset/consume/update/render animator in the existing hooks |
| `src/audio/ui_sounds.hpp` | CREATE | `UiSound`, `IUiSoundSink`, `UiSoundPlayer` |
| `src/audio/ui_sounds.cpp` | CREATE | Synth WAVs, lazy `AudioEngine`, one-shot play |
| `src/screens/screen.hpp` | UPDATE | `ScreenContext::ui_sounds` (defaulted) |
| `src/screens/screen_manager.cpp` | UPDATE | Menu move/confirm/back sound triggers |
| `src/main.cpp` | UPDATE | Construct `UiSoundPlayer`, wire into shell context |
| `CMakeLists.txt` | UPDATE | Add `note_art.cpp`, `judgment_animator.cpp`, `ui_sounds.cpp` to `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | Register 3 new test targets |
| `tests/note_art_test.cpp` | CREATE | Mask/rotation unit tests (no GL) |
| `tests/judgment_animator_test.cpp` | CREATE | Curves, label mapping, milestone dedupe, headless render |
| `tests/ui_sounds_test.cpp` | CREATE | WAV synth + fake-sink trigger tests |

No changes to `src/chart/*`, `src/timing/*`, `src/gameplay/judgment_engine.*`,
`score_keeper.*`, `life_keeper.*`, `note_field.*`, `src/data/*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Groups A/B/C map to the recommended PR split.

### Task A1: Pure note-art rasterizer

- **File**: `src/render/note_art.hpp`, `src/render/note_art.cpp`
- **Action**: CREATE
- **Implement**: `ArrowDirection` enum (`Left=0,Down=1,Up=2,Right=3`, matching `Note.column`,
  `note.hpp:26`) and `make_arrow_rgba` / `make_hold_head_rgba` / `make_roll_head_rgba` /
  `make_mine_rgba` / `make_receptor_rgba` / `make_body_rgba`. Each returns
  `size*size*4` bytes, white RGB with shape alpha. Rasterize by scanline point-in-shape tests
  (arrow = triangle + shaft; mine = radial spikes; body = rounded bar). Guard `size <= 0` → empty
  vector. No GL/SDL/includes beyond `<cstdint>`, `<vector>`.
- **Mirror**: `src/render/background_renderer.cpp` gradient builder (pure buffer → `from_rgba`);
  `metronome.cpp` scanline style.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16` (after D1).

### Task A2: `NoteSkin` art textures + direction UVs

- **File**: `src/gameplay/noteskin.hpp`, `src/gameplay/noteskin.cpp`
- **Action**: UPDATE
- **Implement**: Add members `Texture tap_tex_, hold_tex_, roll_tex_, mine_tex_, receptor_dir_, body_;`
  and `UVRect arrow_uv(int column) const`. In `init()` (after the existing `glad_glGenTextures`
  guard, `noteskin.cpp:30-33`) upload each mask via `Texture::from_rgba`. `head_texture(type)` maps
  Tap→`tap_tex_`, HoldHead→`hold_tex_`, RollHead→`roll_tex_`, Mine→`mine_tex_` (invalid textures when
  headless, exactly like today). `shutdown()` destroys them. Keep `style_for`, `column_tint`,
  `quad_texture` for source-compat with `note_field_test.cpp:275-279`.
- **Mirror**: `noteskin.cpp:25-48` (init/shutdown), `background_renderer.cpp` (from_rgba build).
- **Validate**: `cmake --build build -j16`; `./build/tests/note_field_test` stays green.

### Task A3: Note field renderer uses the art

- **File**: `src/gameplay/note_field_renderer.cpp`
- **Action**: UPDATE
- **Implement**: Receptors (`:35-42`) → `skin.receptor_texture()` + `skin.arrow_uv(column)`. Heads
  (`:86-97`) → `skin.head_texture(item.type)` + `skin.arrow_uv(item.column)`. Mines (`:99-111`) →
  `skin.head_texture(NoteType::Mine)` with `UVRect{}`. Bodies (`:47-68`) → `skin.body_texture()` with
  `UVRect{}` (keep the existing 0.65 alpha × tint). Tails (`:70-83`) unchanged or use the body mask.
  No layout math change.
- **Mirror**: existing `note_field_renderer.cpp` draw calls; `gl_quad_renderer.hpp:30-33`.
- **Validate**: `cmake --build build -j16`; `ctest --test-dir build` stays 28/28.

### Task A4: `note_art_test`

- **File**: `tests/note_art_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`, no GL/window): for each maker `size=64`: buffer length == `64*64*4`;
  RGB bytes are 255 where alpha > 0; center alpha > 0; corners (e.g. `(0,0)`) alpha == 0 for arrow/
  mine; distinct masks differ (hash of alpha channel unique across tap/hold/roll/mine/body);
  direction changes `make_arrow_rgba(Up) != make_arrow_rgba(Left)` while nonzero coverage is equal;
  `size=0` → empty. `arrow_uv` rotation: `arrow_uv(Up) == UVRect{}`, and left/down/right are the
  three distinct 90° rotations with swapped/flipped corners.
- **Mirror**: `tests/texture_test.cpp:12-19`; `tests/note_field_test.cpp` pure assertions.
- **Validate**: `./build/tests/note_art_test` → exit 0.

### Task B1: Shared judgment color accessor

- **File**: `src/gameplay/hud_renderer.hpp`, `src/gameplay/hud_renderer.cpp`
- **Action**: UPDATE
- **Implement**: Declare `[[nodiscard]] Color judgment_color(JudgmentKind kind, TapJudgment window,
  HoldJudgment hold);` in the header (replaces the file-local palette for the chip path). Implement
  by returning the existing `kFantasticColor`…`kHoldNgColor` constants (`hud_renderer.cpp:14-22`).
  Rewire `HudRenderer::render`'s `draw_chip` calls (`:112-123`) to use it. Behavior-preserving.
- **Mirror**: `hud_renderer.cpp:14-22,112-123`.
- **Validate**: `cmake --build build -j16`; existing results/HUD tests stay green.

### Task B2: `JudgmentAnimator`

- **File**: `src/gameplay/judgment_animator.hpp`, `src/gameplay/judgment_animator.cpp`
- **Action**: CREATE
- **Implement**: Per **Pinned Semantics B**. Constants (presentation, unsourced):
  `kJudgmentPopSeconds = 0.6`, `kComboPopSeconds = 0.5`, `kComboMilestone = 50`. `pop_scale` rises
  quickly to ~1.25 then settles to 1.0; `pop_alpha` stays 1 then fades to 0; `pop_active` is
  `elapsed < duration`. `judgment_label`: Fantastic/Excellent/Great/Decent/WayOff/Miss →
  "FANTASTIC"/"EXCELLENT"/"GREAT"/"DECENT"/"WAY OFF"/"MISS"; HitMine → "MINE"; HoldOk/RollOk →
  "OK"; HoldNg/RollNg → "NG"; RollHit/AvoidedMine → "" (no pop). `consume` skips empty labels.
  `render` (guard `!renderer.is_initialized() || w<=0 || h<=0`) draws judgment text centered at
  `y ≈ h*0.42` with `pixel = 5*pop_scale`, alpha `pop_alpha`, and the combo pop at top-center with
  `pixel = 6*pop_scale`. Uses `draw_text_centered` (`bitmap_font.hpp:23`).
- **Mirror**: `hud_renderer.cpp` render shape; `preview_player.cpp:25-34` timer idiom.
- **Validate**: `cmake --build build -j16`.

### Task B3: `GameplayView` animator integration

- **File**: `src/gameplay/gameplay_view.hpp`, `src/gameplay/gameplay_view.cpp`
- **Action**: UPDATE
- **Implement**: Add `JudgmentAnimator judge_anim_;` member. In `init()` call `judge_anim_.reset();`.
  In `update()` after `score_.consume(new_events_); life_.consume(new_events_);` (`:168-169`) add
  `judge_anim_.consume(new_events_);` and `judge_anim_.update(fixed_dt, score_.state().combo);`
  (call the animator even while `exited_` so the last pop fades — or skip; choose skip-on-exit to
  mirror `:156-158`). In `render()` after `hud_.render_life(...)` (`:222`) add
  `judge_anim_.render(renderer, screen_w, screen_h);`. In `shutdown()` add `judge_anim_.reset();`.
- **Mirror**: `gameplay_view.cpp:166-169,182-223,235-263`.
- **Validate**: `cmake --build build -j16`; `score_keeper_test`, `life_keeper_test`,
  `results_screen_test` stay green.

### Task B4: `judgment_animator_test`

- **File**: `tests/judgment_animator_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`): label mapping for every `JudgmentKind`/window/hold combination;
  `pop_scale(duration) == 1.0f` and `pop_alpha(duration) == 0.0f`, `!pop_active(duration)`;
  `pop_scale(0) >= 1.0f`, `pop_alpha(0) == 1.0f`; `consume` of a Fantastic event arms an active
  popup while `AvoidedMine`/`RollHit` do not; `update` twice with a milestone combo (e.g. `update(0.1,50)`)
  fires once and `update(0.1,50)` again does **not** re-fire (dedupe); a second distinct milestone
  (100) fires again; headless `render` (uninitialized `GlQuadRenderer`) and `reset` do not crash.
- **Mirror**: `tests/score_keeper_test.cpp` (event construction), `tests/texture_test.cpp:12-19`.
- **Validate**: `./build/tests/judgment_animator_test` → exit 0.

### Task C1: `UiSoundPlayer` + WAV synth

- **File**: `src/audio/ui_sounds.hpp`, `src/audio/ui_sounds.cpp`
- **Action**: CREATE
- **Implement**: `UiSound` enum, `IUiSoundSink`, `UiSoundPlayer`. `write_ui_sound_wav(path, sound)`
  helper (16-bit mono PCM, ~80–150 ms: Move = 880 Hz blip, Confirm = 660→990 Hz rising, Back =
  660→440 Hz falling), reusing the fixed-layout writer in `metronome.cpp:90-137`. `init(dir)`:
  `create_directories(dir/"sfx")`, synth any missing file, lazily `AudioEngine::instance().init()`
  (`sound_stream.cpp:47-60` pattern); return `false` (and set `ready_=false`) if the engine is
  unavailable, logging once. `play(sound)`: no-op when `!ready_`; else one-shot via
  `ma_engine_play_sound(raw_engine(), path, nullptr)` (overlap-safe; never blocks). Destructor
  releases sounds.
- **Mirror**: `metronome.cpp:68-137`; `sound_stream.cpp:47-60`; `audio_engine.cpp:19-37`.
- **Validate**: `cmake --build build -j16`.

### Task C2: Shell UI-sound triggers

- **File**: `src/screens/screen.hpp`, `src/screens/screen_manager.cpp`
- **Action**: UPDATE
- **Implement**: Add `IUiSoundSink* ui_sounds = nullptr;` to `ScreenContext` (last field, defaulted
  so existing aggregate init compiles). In `ScreenManager::update`, during the existing press loop
  (`screen_manager.cpp:177-189`), record `bool move_pressed` for `Left/Down/Up/Right` (gameplay
  directional actions; reuse the actions `options`/select already use) and, after classification,
  play with a helper `play_ui(UiSound)` guarded on `ctx_.ui_sounds != nullptr`:
  `Move` when `move_pressed && is_menu_screen(active_id_)`, `Confirm` on `confirm_pressed && is_menu`,
  `Back` on `back_pressed && is_menu`, where `is_menu_screen` = `Title|Attract|Select|Results`
  (excludes `Gameplay|Calibration|InputRemap`). Trigger before or after `apply_pending()` consistently
  (choose before, so the press that navigates sounds). `screen_manager_test.cpp` must stay green
  (null sink).
- **Mirror**: `screen_manager.cpp:170-232`; `screen.hpp:34-58`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` green.

### Task C3: Wire the player in `main`

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**: On the shell path (`main.cpp:325-383`), construct a `td::UiSoundPlayer ui_sounds;`
  (owned in `main`'s scope, like `play_request`), call `ui_sounds.init(paths.data_dir)` after
  `app.init()`, and set `shell->context().ui_sounds = &ui_sounds;` before `shell->start(...)`. The
  `--gameplay-demo` path (`:246-324`) stays untouched (no UI sounds). Log a readable line when the
  player is unavailable.
- **Mirror**: `main.cpp:229-230,325-345` (existing context wiring).
- **Validate**: `cmake --build build -j16`.

### Task C4: `ui_sounds_test`

- **File**: `tests/ui_sounds_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`): `write_ui_sound_wav` to a temp dir (use `std::filesystem::temp_directory_path()`
  + unique subdir) produces a RIFF/WAVE file with 16-bit mono PCM and nonzero data for each sound;
  the three sounds have distinct data sizes; a `FakeSink` recording `UiSound`s, placed on
  `ScreenContext.ui_sounds` with a `ScreenManager`, receives `Move` on a directional press in Select,
  `Confirm` on Confirm, `Back` on Back, and receives **nothing** during Gameplay/Calibration/
  InputRemap directional presses; `ScreenManager` with a null sink does not crash. `UiSoundPlayer::init`
  on a nonexistent/degenerate dir returns false without throwing and `play()` is then a no-op.
- **Mirror**: `tests/screen_manager_test.cpp` (fake screens + context), `tests/metronome_sync_test.cpp`
  (WAV generation), `tests/texture_test.cpp:12-19`.
- **Validate**: `./build/tests/ui_sounds_test` → exit 0.

### Task D1: Register sources and tests

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/render/note_art.cpp`, `src/gameplay/judgment_animator.cpp`,
  `src/audio/ui_sounds.cpp` to `tundra_core` (`CMakeLists.txt:80-129`); append `note_art_test`,
  `judgment_animator_test`, `ui_sounds_test` blocks mirroring the `background_test` block
  (`tests/CMakeLists.txt`). Split across PRs if implementing A/B/C separately.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task D2: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build, run everything, scan for new warnings.
- **Validate**: see **Validation** below (`ctest` → **31/31**, no warnings).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 31/31: 28 existing + note_art_test + judgment_animator_test + ui_sounds_test)
ctest --test-dir build --output-on-failure

# Explicit new/affected tests
./build/tests/note_art_test
./build/tests/judgment_animator_test
./build/tests/ui_sounds_test
./build/tests/screen_manager_test
./build/tests/note_field_test

# 2D-only / no forbidden includes in the new art + animator modules
rg -n "video|3D|glad|SDL" src/render/note_art.hpp src/gameplay/judgment_animator.hpp
# expected: no matches (glad only in noteskin.cpp / ui_sounds.cpp for engine guards)

# Judgment/scoring path untouched
git diff --name-only | rg "judgment_engine|score_keeper|life_keeper|note_field\.|timing/" ; # expected: none

# Warning budget
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none
```

## End-to-End Verification

All steps are headless and non-blocking where possible; use `--data-dir` so the developer's real
`data/` is untouched. Visual/audio confirmation needs a display+device and is a documented manual
step (the automated substitute is the unit suites).

1. **Noteskin art (AC1)**: `note_art_test` pins distinct tap/hold/roll/mine/receptor/body masks and
   the direction rotation; `note_field_test` confirms the renderer still lays out correctly. Manual:
   run a chart on a display and confirm four distinct directional arrows, a distinct mine, and
   hold/roll bodies/caps.
2. **Judgment pop matches the log (AC2)**: `judgment_animator_test` maps every event kind and proves
   the animator is driven by `consume(new_events_)`. Manual: hit a note and watch the matching label
   pop and fade.
   ```bash
   ./build/tundra-dance --headless --smoke-test 30 \
     --gameplay-demo "tests/fixtures/sync_test/metronome.sm"
   # exit 0; gameplay runs to the smoke-test exit (headless draw no-ops)
   ```
3. **Combo pop at milestones (AC3)**: `judgment_animator_test` fires the milestone exactly once and
   dedupes. Manual: build a 50-combo and watch the larger combo pop.
4. **UI sounds (AC4)**: `ui_sounds_test` proves the WAV synth and that a `FakeSink` receives
   Move/Confirm/Back on menu navigation but not during gameplay/calibration/remap. Manual: navigate
   Title/Select/Results with sound enabled and hear distinct blips.
5. **Regression**: `ctest --test-dir build --output-on-failure` → **31/31**; `score_keeper_test`,
   `life_keeper_test`, `results_screen_test`, `screen_manager_test`, `note_field_test` stay green.
6. `git status` shows new files under `src/render/`, `src/gameplay/`, `src/audio/`, `tests/`; edits
   limited to `noteskin.*`, `note_field_renderer.cpp`, `hud_renderer.*`, `gameplay_view.*`,
   `screen.hpp`, `screen_manager.cpp`, `main.cpp`, and the CMake files.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Procedural masks look crude; "final noteskin graphics" implies real art | Documented decision + OQ1; masks are data-driven and swappable later for a PNG pack without API change | **Flagged** — OQ1 |
| Animation uses `fixed_dt`, drifting from music/event timing | `fixed_dt` drives only the fade; the popup is armed *by the event log*, never by the clock; judgment path untouched | **In scope** |
| Combo milestone re-fires every frame at a multiple | Dedupe via `last_milestone_`; unit test asserts single fire | **In scope** |
| Multiple events in one tick (chords) fight over the popup | Last event wins (documented); HUD counts remain per-event | **Flagged** — OQ3 |
| UI sounds fire during gameplay steps or exit-confirm on Attract incorrectly | Central trigger gated to the menu screen set; null-sink guard; `ui_sounds_test` proves exclusion | **In scope** |
| Rapid menu input cuts off one-shots / overlaps | `ma_engine_play_sound` spawns independent one-shots (overlap-safe) | **In scope** |
| No audio device (headless/CI) breaks or blocks | `UiSoundPlayer::init` returns false, `play` logs once and no-ops; WAV synth is pure I/O | **In scope** |
| Generated WAVs litter the data dir | Written under `<data_dir>/sfx/`; regenerated only if missing/short (mirrors metronome click track) | **In scope** — accepted |
| Headless build hits GL calls in noteskin/animator | `NoteSkin::init` returns false without GL (unchanged); animator/renderer draw no-ops with uninitialized `GlQuadRenderer` | **In scope** |
| `arrow_uv` rotation wrong for a direction | Hand-verified + unit-tested four-way rotation against the enum | **In scope** |
| Commentary expects `stb_truetype` (issue tech note) | Bitmap font retains current behavior; font atlas is a separate effort | **Flagged** — OQ6 |
| Scope too large for one PR | Plan is task-grouped A/B/C for three PRs; recommend splitting the issue | **Flagged** — OQ5 |

---

## Decisions

- **Procedural noteskin art, not committed PNGs.** Masks are generated in-memory with
  `Texture::from_rgba` (like `BackgroundRenderer`'s fallback), keeping the repo asset-free and
  fresh-clone safe, and are tinted at draw time with the existing `NoteStyle` colors. Surfaced as OQ1.
- **One pure rasterizer module (`src/render/note_art`) + direction via UV rotation.** Keeps the
  shape generation testable without GL and avoids 16 direction×type textures.
- **Animator consumes the existing `new_events_` slice.** Guarantees "visuals match the log exactly"
  (AC2) with zero re-judging and no clock in the judgment path; `fixed_dt` is presentation only.
- **Combo milestones read event-sourced `ScoreState::combo`.** Deduped by last-fired milestone;
  interval is a single named constant (proposed 50).
- **UI sounds triggered centrally in `ScreenManager`, not per-screen.** The shell already classifies
  presses; one gated change covers every menu without touching Title/Select/Results/Attract, and is
  unit-testable via an injected `IUiSoundSink`.
- **Synthesized SFX WAVs.** No binary audio asset to author; reuses the proven `write_click_track`
  writer; one-shots via `ma_engine_play_sound`.
- **`stb_truetype` deferred.** PRD lists it for UI text, but the ACs and issue title are about
  noteskin/animations/sounds, and no font asset exists; the 5x7 bitmap font suffices for the popups.

---

## Open Questions

1. **Blocking-ish — noteskin representation: procedural masks (recommended) vs a committed PNG
   noteskin pack (e.g. `assets/noteskins/default/`).** The issue says "final noteskin graphics";
   AGENTS.md says engine-only and PRD §13 lists a bundled starter pack as a *distribution* decision.
   Proposed default: **procedural** (no binary asset, always present, swappable later). If real art
   is required, the owner must supply it and a follow-up wires `Texture::from_file` per mask.
2. **Non-blocking — combo milestone interval.** No canonical ITG value. Proposed default: **every
   50**, plus the final combo. Confirm or give the interval.
3. **Non-blocking — judgment pop duration/curve and chord handling.** Proposed defaults: **0.6 s**
   judgment pop, **0.5 s** combo pop; **last event in a tick wins** when a chord produces several
   events. Confirm.
4. **Non-blocking — UI sound representation and scope.** Proposed default: **synthesized WAVs**;
   **menu Move/Confirm/Back only** (no gameplay hit sound, which ITG/StepMania users configure
   separately). Confirm whether a gameplay judgment/hit SFX is wanted.
5. **Non-blocking — split.** Proposed: **split #24 into D2a/D2b/D2c** and ship three PRs. Confirm.
6. **Non-blocking — `stb_truetype`.** Issue tech note mentions it; current UI is a bitmap font.
   Proposed default: **keep the bitmap font** and track `stb_truetype` as a separate issue. Confirm.
7. **Non-blocking — trigger screens.** Proposed default: UI sounds on **Title/Attract/Select/
   Results** only; silenced in Gameplay/Calibration/InputRemap. Confirm the exclusion set (e.g.
   should Calibration/InputRemap navigation beep?).

---

## Acceptance Criteria

- [ ] Gameplay renders a distinct tap/hold/roll/mine noteskin (direction-aware arrows), replacing
      the Phase B placeholder quads (Tasks A1-A3; E2E 1)
- [ ] Judgment sprites pop with animation derived from the B4 event log, with no re-judging
      (Tasks B1-B3; E2E 2)
- [ ] Combo pop animations trigger at milestones, deduped (Task B2; E2E 3)
- [ ] UI sounds play on menu navigation and confirmations, and not during gameplay
      (Tasks C1-C3; E2E 4)
- [ ] `ctest --test-dir build --output-on-failure` → **31/31**; `note_field_test`,
      `screen_manager_test`, `score_keeper_test`, `life_keeper_test`, `results_screen_test` stay
      green (Tasks A4/B4/C4/D2; E2E 5)
- [ ] Judgment/scoring/timing paths unchanged; no frame/wall-clock in judgment (Validation
      `git diff` check)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic` (Validation)
- [ ] Open Questions OQ1-OQ7 confirmed or defaults accepted (noteskin representation, milestone,
      animation timing, UI sound scope, split, font, trigger screens)
