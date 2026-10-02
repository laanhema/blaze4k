# Plan: Fix Hold and Roll Tails Extending Past Their End Time (#62)

## Summary

`draw_hold` (`src/gameplay/note_field_renderer.cpp:42-84`) runs the hold/roll body all the way to
`tail_y` and then puts the 64x64 Cel end cap **entirely past** the tail
(`cap_top = reverse ? tail_y - width : tail_y`, `:76`). The visible end of every hold and roll
therefore lands one full note box (96 px) after its release time. StepMania and OpenITG stop the
body `StopDrawingHoldBodyOffsetFromTail` units before the tail and start the cap there. The Cel
skin sets that to **-32** of a 64-unit arrow, which puts the cap **centred on the tail**, exactly
like a tap arrow centred on its beat. The fix:

- Add that metric to `HoldSprites` as a per-skin value: Cel uses 0.5 of the note size, and the
  procedural fallback has no cap so it uses 0.
- Pull the hold geometry out into a pure, GL-free `layout_hold()` function. It ends the body at
  `tail_y ∓ inset`, butts the cap against it with no seam, and clips the cap at the head centre
  as OpenITG/SM5 do.
- Pass the scroll direction in explicitly, rather than inferring it from `tail_y < head_y`.
- Anchor the body tiling at the body/cap junction.

Up-scroll and down-scroll mirror each other, and holds and rolls share the code path. A new headless
`note_field_renderer_test` pins the geometry.

## User Story

As a player
I want the drawn end of a hold or roll to line up with the moment I may release it
So that I can read release timing from the note field the way I would in OpenITG with the Cel noteskin

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | `gameplay/note_field_renderer` (hold geometry + draw), `gameplay/noteskin` (`HoldSprites` metric), tests (new `note_field_renderer_test`, `noteskin_test`), `tests/CMakeLists.txt`, Cel `README.md` (doc line) |
| GitHub Issue | #62 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Unix Makefiles, Release, `build/_deps` populated) |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic` (no `-Werror`) |
| Cores | 16 | `-j16` is safe |
| Baseline tests | **37/37 pass** | `cmake --build build -j && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure` reports "100% tests passed out of 37" (0.45 s), on `main` @ `d771f12` |
| Hold draw | `src/gameplay/note_field_renderer.cpp:37-84` | The cap is drawn from `tail_y` outward, a full `width` (= `kNoteSize * width_scale` = 96 px) past the tail. Body tiling is anchored at `tail_y` (`:54-73`) |
| Direction inference | `note_field_renderer.cpp:50` | `reverse = tail_y < head_y`. This is ambiguous when `head_y == tail_y`, for example a held hold at its exact tail time where the head is clamped to the receptor (`:122-128`) |
| Skin metric source | `src/gameplay/noteskin.cpp:327-337` | Cel gives `HoldSprites{body, cap, 1.0f, 4.0f, kWhite}` and the fallback gives `{&body_, nullptr, 0.6f, 0.0f, tint}` (positional aggregate init) |
| Field geometry | `src/gameplay/gameplay_view.cpp:61-69` | `pixels_per_beat = 64`, `note_height = kNoteSize = 96`. Down = receptor at 0.85 h, notes travel downward. `NoteField::screen_y` is at `note_field.cpp:110-115` |
| Culling | `src/gameplay/note_field.cpp` `compute_visible` | Pads the head..tail span by `note_height / 2` (48 px). After the fix, that pad exactly covers the cap's 48 px overshoot. Today's 96 px overshoot can be culled while partly on screen |
| GL in tests | `GlQuadRenderer` headless is a no-op (`gl_quad_renderer.hpp:9-14`) | Draw calls can't be observed headless, so the geometry must be a pure function to be testable |
| Hold-bearing fixture | `tests/fixtures/sync_test/metronome.sm` has **taps only** | Visual check (AC 4) needs a hold chart. `songs/In The Groove*` packs are available locally |
| Upstream sources | `<scratchpad>/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`, `<scratchpad>/workshop` @ `5ba831ae039e7319b7ae5f223b3532c05e8b4072`, `<scratchpad>/sm5_NoteDisplay.cpp` (StepMania tag `v5.0.12` = `45e0787a7457c1b9071522463aa902d59ae3a2ee`) | Cloned or fetched for provenance (see below) |

**Start green, stay green:** 37 tests pass now. This plan adds **one** test target
(`note_field_renderer_test`), so **38/38** are expected afterwards.

---

## Value Provenance

| Value / behaviour | Source | Notes |
|-------------------|--------|-------|
| Body stops 32 units before the tail (`StopDrawingHoldBodyOffsetFromTail=-32`) | Cel Workshop `Cel - Workshop/metrics.ini:144` @ `5ba831a` | The vendored skin (`assets/noteskins/cel/README.md`) |
| Body starts at the head centre (`StartDrawingHoldBodyOffsetFromHead=0`) | same, `:143` | Unchanged: the body already starts at `head_y` |
| Flip body/caps in reverse (`FlipHoldBodyWhenReverse=1`) | same, `:154` | Already implemented (vertical UV flip) |
| Body ends at `fYTail + StopDrawingHoldBodyOffsetFromTail`; bottom cap spans `[that, that + frameHeight]` | OpenITG `src/NoteDisplay.cpp:648-649, 755-756` | With -32 and a 64-unit cap, the cap is centred on the tail |
| Cap is not drawn on the head side of the head centre ("don't draw any part of the tail that is before the middle of the head") | OpenITG `src/NoteDisplay.cpp:774-775`; SM5 `NoteDisplay.cpp:1053-1058` (`y_start_pos = max(y_start_pos, y_head)`) | Texture v is offset by the clipped distance (OpenITG `:799-800` `fTopDistFromTail`, SM5 `:795`) |
| Reverse is symmetric: with `FlipHoldBodyWhenReverse`, `y_head -= StopDrawing…`, so the swapped bottom-cap art spans `[tail-32, tail+32]` | SM5 v5.0.12 `NoteDisplay.cpp:1094-1117, 1133-1134, 1191` | OpenITG's reverse path (`:1003-1007`) uses a separate TopCap part and `StartDrawingHoldBodyOffsetFromHead` (0 in this skin), which would leave the cap fully past the tail in reverse. The vendored Workshop metrics were authored for SM5 ("Made for Version Stepmania 5", `metrics.ini:2`), so SM5's symmetric semantics apply. This also satisfies AC 1 ("in both up and down scroll") |
| Cap height = 1 note box (64 units → `kNoteSize` = 96 px) | `noteskin.cpp:333` ("64x64 cap per 64-unit arrow"), `noteskin.hpp:85-87` | Unchanged |
| Cap art fills nearly its whole height | Measured this session: hold caps have alpha > 0 in rows 0–506 of 512 (≈ 63.4 of 64 units), roll caps in rows 0–451 (≈ 56.4 units) | So the cap's far edge **is** the visible end of a hold, and a roll ends about 7.6 units earlier. No per-art trimming is needed |

**Resulting geometry** at 720p (`kNoteSize` = 96), measured along the hold from head to tail, where `d` = +1 for
up-scroll and −1 for down-scroll:

- body: `head_y` to `tail_y − d·48`
- cap: `tail_y − d·48` to `tail_y + d·48`, centred on the tail
- visible end: `tail_y + d·48` (was `tail_y + d·96`)

At the tail time, `tail_y` equals the receptor centre, so the remaining cap lies exactly inside the
receptor box (receptor centre ± 48) and under the pinned head. That matches how a tap arrow covers the
receptor at its hit time.

---

## Patterns to Follow

### Pure, GL-free sheet/tween math exposed for headless tests
```cpp
// SOURCE: src/gameplay/noteskin.hpp:37-42, 69-71
// Cel sheet math, pure (no GL) so it is testable headless.
[[nodiscard]] UVRect cel_tap_frame(NoteQuantization quantization, double beat);
...
[[nodiscard]] ExplosionTween cel_explosion_tween(double elapsed, double duration);
```
`layout_hold()` follows the same shape: a free `[[nodiscard]]` function and a small result struct,
declared in the header, with no GL types beyond `geometry.hpp`.

### Named Cel constants with provenance comments
```cpp
// SOURCE: src/gameplay/noteskin.hpp:56-61
// Cel explosion tweens (metrics.ini [GhostArrowDim]): W1..W5 play ...
inline constexpr double kCelTapExplosionSeconds = 0.15;
inline constexpr double kCelHeldExplosionSeconds = 0.09;
```

### Current segment loop and UV flip (keep the style, change only the anchor)
```cpp
// SOURCE: src/gameplay/note_field_renderer.cpp:54-73
for (double near = 0.0; tile > 0.0 && near < length; near += tile) {
    const double far = std::min(near + tile, length);
    const double y_near = tail_y + toward_head * near;
    ...
    const UVRect uv = reverse ? UVRect{0.0f, v_near, 1.0f, v_far}
                              : UVRect{0.0f, v_far, 1.0f, v_near};
```

### Error handling
There are no new failure modes. Missing art is already handled by `body == nullptr` (return 0) and
`cap == nullptr` (no cap). Degenerate geometry (zero or negative body length, a cap fully behind the head)
must quietly draw nothing rather than producing a negative-height quad.

### Tests (idiom)
```cpp
// SOURCE: tests/noteskin_test.cpp:9-24, 142-152
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << "Assertion failed at " ...; std::abort(); } } while (0)
bool near(double a, double b) { return std::abs(a - b) < 1e-5; }
void test_tap_frames() { ... std::cout << "  - tap sheet frames and quantization bands ok.\n"; }
int main() { std::cout << "noteskin_test\n"; test_tap_frames(); ... std::cout << "noteskin_test passed\n"; }
```
Registration mirrors `tests/CMakeLists.txt:312-320` (`add_executable` + `target_link_libraries(... blaze4k_core)` + `add_test`).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/noteskin.hpp` | UPDATE | Add `kCelHoldBodyStopFromTail = 0.5f` with provenance. Add `HoldSprites::tail_inset_scale` (appended after `tint`). Fix the "cap just past the tail" doc comment |
| `src/gameplay/noteskin.cpp` | UPDATE | Cel `hold()` passes `kCelHoldBodyStopFromTail`. The fallback keeps 0 (it has no cap) |
| `src/gameplay/note_field_renderer.hpp` | UPDATE | Declare `struct HoldLayout` and the pure `layout_hold(...)`. Update the class doc comment |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | Implement `layout_hold`. `draw_hold` takes an explicit `reverse`, uses the layout for the body end, tiling anchor and clipped cap quad/UV. `render()` passes `direction == Down` |
| `tests/note_field_renderer_test.cpp` | CREATE | Headless geometry tests for up/down, seam, clipping, tail-time, fallback, and a regression for the old overshoot |
| `tests/CMakeLists.txt` | UPDATE | Register `note_field_renderer_test` |
| `tests/noteskin_test.cpp` | UPDATE | The fallback hold has `tail_inset_scale == 0`, and the Cel constant is 0.5 |
| `assets/noteskins/cel/README.md` | UPDATE | Cap row: "Hold/roll end caps, centred on the hold's end (metrics.ini `StopDrawingHoldBodyOffsetFromTail=-32`)" |

No CMake source-list change is needed for `blaze4k_core` (`note_field_renderer.cpp` is already listed at
`CMakeLists.txt:116`). Leave `TODO.md` alone: merged items such as `:36` (#61) are not ticked by
implementation runs. Leave the untracked `.agents/stories/todo-stories.md` alone too.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Skin metric for where the body stops

- **File**: `src/gameplay/noteskin.hpp`, `src/gameplay/noteskin.cpp`
- **Action**: UPDATE
- **Implement**:
  1. In `noteskin.hpp`, next to the other Cel constants (around `:56-61`), add:
     ```cpp
     // Cel metrics.ini StopDrawingHoldBodyOffsetFromTail=-32 (of a 64-unit arrow): the hold/roll
     // body stops half a note before the tail and the 64x64 end cap starts there, so the cap is
     // centred on the tail (OpenITG NoteDisplay.cpp DrawHoldBody/DrawHoldBottomCap; SM5 mirrors
     // it in reverse). Fraction of the note size.
     inline constexpr float kCelHoldBodyStopFromTail = 0.5f;
     ```
     Declare it **before** `struct HoldSprites`, or anywhere in the namespace, since only `noteskin.cpp` and
     the tests use it.
  2. Rewrite the `HoldSprites` comment (`:24-26`): "a body strip from the head to `tail_inset_scale`
     note sizes before the tail, plus an optional end cap that starts there". Append a new last member
     **after** `tint`, so the existing positional initialisers stay valid:
     ```cpp
     // How far before the tail (toward the head) the body stops and the cap starts, relative
     // to the note size: StopDrawingHoldBodyOffsetFromTail / -64. Ignored without a cap.
     float tail_inset_scale = 0.0f;
     ```
  3. In `noteskin.cpp:331-333`, the Cel return becomes
     `HoldSprites{body, cap, 1.0f, 4.0f, kWhite, kCelHoldBodyStopFromTail}`. The fallback (`:335-336`) is
     unchanged, because a defaulted 0 means the body still runs to the tail with no cap.
- **Mirror**: `noteskin.hpp:56-61` (Cel constants with metric citations)
- **Validate**: `cmake --build build -j16 --target blaze4k_core`

### Task 2: Pure hold layout

- **File**: `src/gameplay/note_field_renderer.hpp`, `src/gameplay/note_field_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Header (above `class NoteFieldRenderer`):
     ```cpp
     // Screen-y layout of one hold/roll (pure, no GL). `d` = +1 when the tail is drawn below the
     // head (up-scroll), -1 for down-scroll. The body runs head_y..body_end_y; the cap starts at
     // body_end_y and extends cap_size further (centred on the tail for Cel). Like OpenITG
     // DrawHoldBottomCap, no part of the cap is drawn on the head side of head_y.
     struct HoldLayout {
         double body_end_y = 0.0;  // body's tail-side edge == the cap's unclipped head-side edge
         bool has_body = false;    // false once the head reaches/passes body_end_y
         bool has_cap = false;     // false without cap art, or when the head is past the whole cap
         double cap_near_y = 0.0;  // cap head-side edge after clipping at head_y
         double cap_far_y = 0.0;   // cap tail-side edge: the visible end of the hold
         float cap_v_near = 0.0f;  // cap texture v at cap_near_y (0 unclipped; art is head-on-top)
     };
     [[nodiscard]] HoldLayout layout_hold(double head_y, double tail_y, bool reverse,
                                          double cap_size, double tail_inset, bool has_cap);
     ```
  2. Implementation (in the namespace, outside the anonymous one):
     ```cpp
     HoldLayout layout_hold(double head_y, double tail_y, bool reverse, double cap_size,
                            double tail_inset, bool has_cap) {
         const double d = reverse ? -1.0 : 1.0; // screen-y step from head toward tail
         HoldLayout out;
         const bool cap = has_cap && cap_size > 0.0;
         out.body_end_y = cap ? tail_y - d * tail_inset : tail_y;
         out.has_body = d * (out.body_end_y - head_y) > 0.0;
         if (cap) {
             out.cap_far_y = out.body_end_y + d * cap_size;
             const double clipped = std::max(0.0, d * (head_y - out.body_end_y));
             if (clipped < cap_size) {
                 out.has_cap = true;
                 out.cap_near_y = out.body_end_y + d * clipped;
                 out.cap_v_near = static_cast<float>(clipped / cap_size);
             }
         }
         return out;
     }
     ```
- **Validate**: `cmake --build build -j16 --target blaze4k_core`

### Task 3: Use the layout in `draw_hold` and pass the direction explicitly

- **File**: `src/gameplay/note_field_renderer.cpp`
- **Action**: UPDATE
- **Implement**:
  1. Signature: `int draw_hold(GlQuadRenderer&, const HoldSprites&, double x, double head_y, double tail_y, bool reverse, double screen_h)`.
     Delete `const bool reverse = tail_y < head_y;` (`:50`).
  2. Compute
     `const HoldLayout layout = layout_hold(head_y, tail_y, reverse, width, NoteSkin::kNoteSize * hold.tail_inset_scale, hold.cap != nullptr);`.
     Keep `width = kNoteSize * width_scale`, which is still both the strip width and the square cap edge.
  3. Body: `if (layout.has_body)`, then `length = std::abs(layout.body_end_y - head_y)`. The loop is the same as now,
     but every `tail_y` becomes `layout.body_end_y`. The tile pattern is now anchored at the body/cap junction
     (texture v = 1 at the junction), so the body's tail-side edge always meets the cap's head-side
     edge (v = 0) as the Cel art is authored. The `tile` fallback (`length` when `tile_scale == 0`) uses the
     new length.
  4. Cap: `if (layout.has_cap)`, then `top = std::min(cap_near_y, cap_far_y)`, `bottom = std::max(...)`, and skip if
     `bottom < 0 || top > screen_h` (the same culling as body segments). UV:
     - up-scroll: `UVRect{0, cap_v_near, 1, 1}`
     - reverse: `UVRect{0, 1, 1, cap_v_near}` (vertical flip, as now)

     Quad: `Rect{x - width/2, top, width, bottom - top}`.
  5. **Seam guard:** when unclipped, the last body segment's tail-side edge and the cap's head-side edge are
     both computed from the same `layout.body_end_y` double. Convert that to `float` once and reuse it for both,
     so the two quads share an identical edge with no hairline gap or overlap under alpha blending.
  6. `render()` (`:129-131`): pass `field.config().direction == ScrollDirection::Down` as `reverse`.
     Reuse the same expression as the held clamp at `:123` (hoist it to a local `const bool reverse`).
  7. Update the comments: `draw_hold`'s header comment (`:37-41`, "then its end cap just past the tail" becomes
     "body stops `tail_inset` before the tail; the cap starts there (centred on the tail for Cel) and is
     clipped at the head centre"), and the draw-order comment in `note_field_renderer.hpp:29-31` if it
     mentions caps.
- **Mirror**: `note_field_renderer.cpp:54-82` (keep the loop/UV conventions)
- **Validate**: `cmake --build build -j16 2>&1 | grep -E "warning|error"` gives no new output for the touched files

### Task 4: Headless geometry tests

- **File**: `tests/note_field_renderer_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Action**: CREATE / UPDATE
- **Implement**: include `gameplay/note_field_renderer.hpp` and `gameplay/noteskin.hpp`. Use the `TEST_CHECK`/`near`
  idiom from `noteskin_test.cpp`. Set `S = NoteSkin::kNoteSize` (96) and `I = S * kCelHoldBodyStopFromTail` (48).
  Write one `void test_*()` per case:
  1. **Up-scroll, Cel**: `layout_hold(100, 500, false, S, I, true)` gives `body_end_y == 452`, `has_body`,
     `cap_near_y == 452`, `cap_far_y == 548`, `cap_v_near == 0`, and the cap centre `(near+far)/2 == 500` (the tail).
  2. **Down-scroll mirror**: `layout_hold(500, 100, true, S, I, true)` gives `body_end_y == 148`, cap 148 to 52,
     centre 100.
  3. **No seam**: for both directions, `cap_near_y == body_end_y` exactly (`==`, not `near`) when unclipped.
  4. **Visible end vs tail (regression)**: `std::abs(cap_far_y - tail_y) == S / 2`. That is strictly less than
     `S`, the old overshoot, in both directions.
  5. **Held hold near its end (clip)**: up-scroll, head clamped to the receptor `head_y = 400`, `tail_y = 420`.
     Expect `!has_body`, `has_cap`, `cap_near_y == 400`, `cap_far_y == 468`, and `cap_v_near == (400 - 372) / S`.
     Mirror it for down-scroll (`head 400, tail 380` gives near 400, far 332).
  6. **At the exact tail time** (`head_y == tail_y == receptor = 400`) for **both** directions: `has_cap`,
     `cap_v_near == 0.5`, and the remnant lies inside the receptor box: `std::abs(cap_far_y - 400) <= S / 2`, on the
     correct side (up: far > 400; down: far < 400). This proves the explicit `reverse` matters.
  7. **Head past the whole cap**: up-scroll `head_y = 600, tail_y = 500` gives `!has_body && !has_cap`, with no
     negative extents.
  8. **Short hold** (shorter than the inset): `layout_hold(100, 120, false, S, I, true)` gives `!has_body`, a cap clipped
     at 100, and `cap_v_near == (100 - 72) / S`.
  9. **Fallback skin (no cap)**: `layout_hold(100, 500, false, S, 0, false)` gives `body_end_y == 500` (unchanged
     behaviour), `!has_cap`. Also check that `has_cap = true` with `cap_size = 0` gives `!has_cap` and `body_end_y == tail`.

  Register the test in `tests/CMakeLists.txt`, mirroring `noteskin_test` (`:312-320`):
  ```cmake
  add_executable(note_field_renderer_test
      note_field_renderer_test.cpp
  )

  target_link_libraries(note_field_renderer_test PRIVATE
      blaze4k_core
  )

  add_test(NAME note_field_renderer_test COMMAND note_field_renderer_test)
  ```
- **Mirror**: `tests/noteskin_test.cpp:1-24, 142-152`; `tests/CMakeLists.txt:312-320`
- **Validate**: `cmake -B build && cmake --build build -j16 --target note_field_renderer_test && ctest --test-dir build -R note_field_renderer_test --output-on-failure`.
  The reconfigure is needed because `tests/CMakeLists.txt` changed, and `cmake --build` also re-runs it automatically.

### Task 5: Skin-metric assertions

- **File**: `tests/noteskin_test.cpp`
- **Action**: UPDATE
- **Implement**: in `test_headless_fallback` (`:126-127`), extend the hold check to
  `hold.cap == nullptr && hold.tile_scale == 0.0f && hold.tail_inset_scale == 0.0f`. Add
  `TEST_CHECK(near(blaze4k::kCelHoldBodyStopFromTail, 0.5));` with a comment citing `metrics.ini:144` (-32 of 64).
  The Cel `hold()` itself needs GL, so it can't be exercised headless.
- **Validate**: `ctest --test-dir build -R noteskin_test --output-on-failure`

### Task 6: Docs line

- **File**: `assets/noteskins/cel/README.md`
- **Action**: UPDATE
- **Implement**: the `BottomCap` table row's "Used for" becomes: "Hold/roll end caps, centred on the hold's end
  (the body stops half a note before it: `metrics.ini` `StopDrawingHoldBodyOffsetFromTail=-32`)".
- **Validate**: review only.

### Task 7: Full suite

- **Validate**: run the full build and ctest (see Validation). Expect **38/38**, with no new warnings in the touched files.

---

## Validation

```bash
# Build (tests/CMakeLists.txt changes, so cmake re-runs automatically)
cmake --build build -j16 2>&1 | grep -E "warning|error" ; cmake --build build -j16

# Tests: expect 38/38 (headless, no window or sound)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# Targeted
ctest --test-dir build -R "note_field_renderer_test|noteskin_test|note_field_test|gameplay_screen_test" --output-on-failure

# Scope guard: only the renderer, skin, tests and Cel README change
git diff --stat
git diff --name-only | grep -E "judgment|note_field\.cpp|gameplay_view|timing/"   # expect no output
git status --short .agents/stories/todo-stories.md                              # still "??", untouched
```

There is no linter configured, so the `-Wall -Wextra -Wpedantic` output is the lint gate.

## End-to-End Verification

1. **Automated (headless, required):** `note_field_renderer_test` runs the exact `layout_hold()` that
   `draw_hold` uses in the real render path, so the geometry in the tests is the geometry on screen.
   The full ctest run (38/38) covers `gameplay_screen_test`, which drives `GameplayView::render` headless as a
   no-op smoke test.
2. **App smoke (headless, required):** `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   should exit 0. Do **not** launch a windowed or interactive binary from the agent.
3. **Visual check (owner, interactive; the agent doesn't run it; required by AC 4):** the metronome chart has no
   holds, so use any ITG song with holds and rolls from `songs/In The Groove*`. Alternatively, run
   `./build/blaze-4k --gameplay-demo <song.sm> --speed 1x`, then `--speed 8x`, and repeat both with `--downscroll`. Check:
   - a hold that is held to the end: its end cap shrinks into the receptor and nothing sticks out past the
     receptor box when the OK flash fires;
   - an unhit hold scrolling past: its rounded end goes past the receptor about half a note after the release
     beat, the same way a tap arrow's lower half does, instead of a full note later;
   - there is no visible gap or line between the body and the cap at 1x or 8x, in either direction;
   - rolls behave the same way.

   Compare against OpenITG/StepMania with Cel if one is available.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| **AC 1 interpretation:** "visible end reaches the receptor at the tail time" could mean the cap's far edge is *flush* with the tail centre, not centred on it | The default is OpenITG/SM5 parity (cap centred, so at the tail time it fills exactly the receptor box). That follows the issue's "as in OpenITG with the Cel noteskin" and core principle 2. The single constant `kCelHoldBodyStopFromTail` switches to flush (1.0) if the owner prefers. See Open Questions | In scope (documented) |
| OpenITG's own reverse path differs (separate TopCap part, `StartDrawingHoldBodyOffsetFromHead`), so with these metrics it would leave the cap past the tail in reverse | Follow SM5 v5.0.12's symmetric reverse handling (the vendored skin is SM5-authored), which is required by "both up and down scroll" | In scope (documented in provenance) |
| A hairline seam between body and cap from float rounding or alpha overlap | The shared float edge (Task 3.5), plus the exact-equality test (Task 4.3) | In scope |
| Changing the body tiling anchor shifts the visible pattern phase by half a note | This is cosmetic and still travels with the note. The Cel art is authored so the body's tail edge meets the cap | In scope (accepted) |
| A direction inferred from `tail_y < head_y` flips at `head_y == tail_y`, drawing the remnant on the wrong side in down-scroll | Pass `reverse` explicitly and test it (Task 4.6) | In scope |
| Culling pad (`note_height/2` = 48) vs cap overshoot | After the fix the overshoot is exactly 48, so nothing is culled while visible. This is an improvement over today's 96 | In scope (no change needed) |
| `pixels_per_beat` (64) vs `kNoteSize` (96) differs from ITG's 1:1 ratio, so a note box spans 1.5 beats at 1x | Pre-existing (the bigger-receptors TODO). Centring is ratio-independent | Out of scope (flag only) |
| Roll caps' art ends about 7.6 units short of the frame bottom | That is how the art is authored and matches OpenITG | Out of scope |
| The hold head on the same row as other taps, or the let-go gray (`HoldLetGoGrayPercent`) | Unrelated to the tail length | Out of scope |

---

## Decisions

- **Centred cap (OpenITG/SM5 Cel parity)** rather than flush-to-tail, because the issue says "as in OpenITG
  with the Cel noteskin".
- **Make it a per-skin metric** (`HoldSprites::tail_inset_scale`) rather than a renderer constant, which mirrors
  `StopDrawingHoldBodyOffsetFromTail` and keeps the cap-less procedural fallback unchanged (the body runs exactly to the tail).
- **Clip the cap at the head centre** in both directions (OpenITG `:774-775`; SM5 `:1056`). This shows only
  the remaining end while a hold is held near its end, and for very short holds.
- **Pure `layout_hold()` plus a new test target**, because GL draws can't be observed headless (one new target, 37 → 38).

## Open Questions

- **Centred vs flush end.** OpenITG/SM5 Cel centres the cap on the tail, so an unheld hold's rounded end passes the
  receptor centre half a note after the release beat (like the lower half of a tap arrow). A literal reading of AC 1
  could instead want the cap's far edge exactly at the tail centre. *Proposed default:* centred, for parity. If the owner
  wants flush, set `kCelHoldBodyStopFromTail = 1.0f` and update tests 1–2/4/6. That one-constant change is a deliberate
  deviation from OpenITG.
- **A permanent hold test chart.** AC 4 allows "the metronome sync-test chart (or a hold test chart)". The metronome has
  no holds. *Proposed default:* verify with existing ITG songs and don't add a fixture. A hold/roll sync chart could be a
  follow-up story if the owner wants one in `tests/fixtures/sync_test/`.

---

## Acceptance Criteria

- [ ] The visible end of a hold/roll (body + cap) is centred on the tail, so at the tail time it sits within the receptor box, in both up and down scroll (Tasks 2–4)
- [ ] Rolls get the same fix (shared `draw_hold` path; the Cel roll sprites carry the same metric)
- [ ] There is no gap or seam between the body and the cap (shared edge, exact-equality test)
- [ ] Verified visually at 1x and 8x, up and down scroll (owner, per End-to-End step 3)
- [ ] All tasks completed; the build has no new warnings in touched files
- [ ] 38/38 tests pass (start green at 37, plus one new target)
- [ ] Follows existing patterns (pure headless-testable math, Cel constants with metric provenance, `TEST_CHECK` idiom)
