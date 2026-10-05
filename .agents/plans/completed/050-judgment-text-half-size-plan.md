# Plan: Gameplay judgment text at half size

## Summary

The judgment pop (FANTASTIC … MISS, OK/NG, MINE) on the gameplay screen is drawn at the sprite's full 444x66 reference size and gets in the way. This plan adds one named constant, `JudgmentAnimator::kJudgmentDisplayScale = 0.5f`, and one pure helper, `JudgmentAnimator::judgment_draw_scale(elapsed)`, which returns `kJudgmentDisplayScale * pop_scale(elapsed, kJudgmentPopSeconds)`. `render_judgment` uses that helper as its single scale for **both** the themed sprite and the bitmap fallback label. The pop curve is unchanged, so the shape stays the same relative to the new size: up to 1.25x, settles at 1.0x, then fades. The pure `judgment_pop_rect` keeps its contract. It already scales about the **centre of the full-size content box**, so the smaller sprite stays centred horizontally on the field and vertically on the old box centre (ref y 329). No layout constant has to move. At peak pop it now clears the combo line (ref y 368) with room to spare, glow included. The full-size sprite used to overlap that line by about 2 px at peak. This is presentation only: nothing touches the music clock, the judgment engine or the event log.

## User Story

As a player
I want the judgment word under the receptors to be smaller
So that it confirms my timing without covering the arrows and the combo

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `src/gameplay/judgment_animator.{hpp,cpp}`, `src/render/theme.hpp` (comment only), `tests/judgment_animator_test.cpp`, `tests/hud_renderer_test.cpp` |
| GitHub Issue | #111 ([TODO-28] Make the gameplay judgment text 50% smaller) |
| Branch (suggested) | `feature/050-judgment-text-half-size` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release) and builds cleanly with `cmake --build build -j` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **49/49 pass** | Run on `main` @ `368f8cd` with the sandboxed ctest command in Validation. The count stays **49**, because no test executable is added (new cases go into existing executables) |
| Sandbox requirement | — | Some tests open the real sound device. **Always** run ctest, and any test or app binary, inside the `bwrap` prefix in Validation |
| Judgment sprite geometry | `assets/theme/cabinet/manifest.json:643-660` (all 9 `judgment_*`) | `size_px` 1000x244, `content_px` {56, 56, 888, 132}, `texture_scale` 2. So the content is 444x66 ref px with a **28 ref px glow pad** on every side. `layout_720p` [418, 296, 444, 66] is the art's native placement. **Do not edit** the manifest or PNGs (issue note: "Don't edit the sprite art") |
| Layout constants | `src/render/theme.hpp:208-209` | `kJudgmentTop 296` (top of the full-size content box), `kComboTop 368` (combo number line-box top) |
| Layout scale | `src/render/theme_layout.hpp:28-66` | `s = min(h/720, w/1280)`, content column centred. 1280x720 → s 1, origin 0. 2560x1440 → s 2, origin 0 |
| Fallback label | `src/render/bitmap_font.cpp:204-255` | `draw_text_centered(text, cx, top_y, pixel)`: 6·pixel per cell, 7·pixel tall, **top-anchored**. `kJudgmentPopPixel` is **not** multiplied by `L.s` today (see Risks) |
| Off-limits file | `.agents/stories/todo-stories.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Forward references to #111

None in `src/`, `tests/`, `docs/` or earlier plans. The only hits are the story text in `.agents/stories/todo-stories.md:996-1003`, which is off-limits and already reflected here.

### Numbers this plan pins (reference px, content box only)

| State | 1280x720 (s=1) rect `{x, y, w, h}` | 2560x1440 (s=2) rect | Bottom vs combo top (`L.y(368)`) |
|---|---|---|---|
| Old, at rest (scale 1.0) | {418, 296, 444, 66} | {836, 592, 888, 132} | 362 < 368 |
| Old, peak (1.25) | {362.5, 287.75, 555, 82.5} | — | **370.25 > 368, overlaps** |
| **New, at rest (0.5)** | **{529, 312.5, 222, 33}** | **{1058, 625, 444, 66}** | 345.5 / 691 < 368 / 736 |
| **New, peak (0.625)** | {501.25, 308.375, 277.5, 41.25} | {1002.5, 616.75, 555, 82.5} | 349.625 / 699.25 |
| New, peak, **with glow** (pad 28 × 0.625 = 17.5 ref) | bottom 367.125 | bottom 734.25 | still < 368 / 736 |

Centre is (640, 329) at 720p and (1280, 658) at 1440p in every row. That is the old content-box centre.

---

## Patterns to Follow

### Named presentation constants on the animator (unsourced, Blaze-only)
```cpp
// SOURCE: src/gameplay/judgment_animator.hpp:26-31
// Blaze 4k presentation constants (unsourced; no OpenITG parity requirement).
static constexpr double kJudgmentPopSeconds = 0.6;
static constexpr double kComboPopSeconds = 0.5;
static constexpr int kComboMilestone = 50;
// Bitmap-font pixel of the fallback label (no theme / missing manifest entry).
static constexpr float kJudgmentPopPixel = 5.0f;
```

### Pure static helpers, documented in the header, tested directly
```cpp
// SOURCE: src/gameplay/judgment_animator.hpp:71-76, judgment_animator.cpp:81-90
[[nodiscard]] static float pop_scale(double elapsed, double duration);
...
float JudgmentAnimator::pop_scale(double elapsed, double duration) {
    if (duration <= 0.0 || elapsed >= duration) {
        return 1.0f;
    }
```

### Render path: one scale feeds both the sprite and the fallback
```cpp
// SOURCE: src/gameplay/judgment_animator.cpp:197-211
const theme::LayoutScale L = theme::layout_scale(w, h);
const float scale = pop_scale(pop_elapsed_, kJudgmentPopSeconds);
...
const Rect rect = judgment_pop_rect(L, theme->content_size(pop_sprite_, L.s), scale);
theme->draw_sprite(renderer, pop_sprite_, Vec2{rect.x, rect.y}, L.s * scale, ...);
...
draw_text_centered(renderer, pop_label_, L.x(...), L.y(theme::layout::kJudgmentTop),
                   kJudgmentPopPixel * scale, ...);
```

### Error handling (presentation code: guard, never throw)
```cpp
// SOURCE: src/gameplay/judgment_animator.cpp:194-196, 201
if (!renderer.is_initialized() || w <= 0 || h <= 0 || !has_pop_) {
    return;
}
if (theme != nullptr && !pop_sprite_.empty() && theme->entry(pop_sprite_) != nullptr) {
```
No new failure modes. The helper is pure arithmetic on already-clamped curves.

### Tests (plain executable, `TEST_CHECK` abort macro, explicit call list in `main`, real manifest)
```cpp
// SOURCE: tests/judgment_animator_test.cpp:17-24, 105-107, 139-151, 331-345
#define TEST_CHECK(expr) ... std::abort();
const blaze4k::ThemeManifest manifest =
    blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"));
void test_judgment_pop_rect() { ... TEST_CHECK(r.x == 418.0f && r.y == 296.0f ...); ...
    std::cout << "  - judgment pop rect ok.\n"; }
int main() { ... test_judgment_pop_rect(); ... }
```
Peak scan idiom: `tests/hud_renderer_test.cpp:199-205` (1001-sample max over the curve).

---

## Pinned Semantics (the contract the tests pin)

- `JudgmentAnimator::kJudgmentDisplayScale = 0.5f`. It is a public `static constexpr float` next to `kJudgmentPopPixel`, with the comment: "Display size of the judgment pop relative to the sprite's native 444x66 content box (and of the fallback label relative to `kJudgmentPopPixel`). Owner estimate on #111 ("perhaps 50%"): tune here after a playtest." This is the **only** place the factor appears.
- `static float judgment_draw_scale(double elapsed)` (pure, `[[nodiscard]]`) returns `kJudgmentDisplayScale * pop_scale(elapsed, kJudgmentPopSeconds)`. At `elapsed >= kJudgmentPopSeconds` it is exactly `kJudgmentDisplayScale` (0.5). Its peak over the curve is `kJudgmentDisplayScale * 1.25` (0.625). The ratio `judgment_draw_scale(e) / kJudgmentDisplayScale == pop_scale(e, kJudgmentPopSeconds)` for every `e`, so the curve shape is unchanged.
- `judgment_pop_rect(L, content, scale)` is **unchanged**: it centres on `(L.x(640), L.y(kJudgmentTop) + content.y / 2)`, where `content` is the full-size content size at `L.s`. `render_judgment` passes `judgment_draw_scale(pop_elapsed_)` as `scale`. The drawn sprite is therefore centred on the old content-box centre (ref 640, 329).
- Sprite: `draw_sprite(..., Vec2{rect.x, rect.y}, L.s * judgment_draw_scale(pop_elapsed_), tint)`. This is the same call shape as today, with only the scale value changed.
- Fallback: `draw_text_centered(pop_label_, L.x(640), L.y(kJudgmentTop), kJudgmentPopPixel * judgment_draw_scale(pop_elapsed_), tint)`. That is 2.5 px cells at rest and 3.125 at peak. It stays top-anchored at `kJudgmentTop` as today. The fallback has never been vertically centred, and changing its anchor is out of scope.
- Alpha (`pop_alpha`), duration, `consume`/`update`, the combo line and `kJudgmentTop`/`kComboTop` values are untouched.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/judgment_animator.hpp` | UPDATE | Add `kJudgmentDisplayScale`, declare `judgment_draw_scale`, update the `kJudgmentPopPixel`, `render_judgment` and `judgment_pop_rect` comments |
| `src/gameplay/judgment_animator.cpp` | UPDATE | Define `judgment_draw_scale`. `render_judgment` uses it for the sprite and the fallback |
| `src/render/theme.hpp` | UPDATE (comment only) | `kJudgmentTop` comment: it is the top of the **full-size** reference box. The drawn pop is centred on that box at `kJudgmentDisplayScale` |
| `tests/judgment_animator_test.cpp` | UPDATE | New `test_judgment_draw_scale` and `test_judgment_clears_combo`. Extend `test_judgment_pop_rect` with the half-size rects at 1280x720 and 2560x1440. Register them in `main` |
| `tests/hud_renderer_test.cpp` | UPDATE | `test_judgment_pop_clear` scans `judgment_draw_scale` (the real drawn peak) instead of raw `pop_scale` |

No files are created. `CMakeLists.txt` / `tests/CMakeLists.txt` are unchanged, because both test executables already exist.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Display-scale constant and pure helper

- **File**: `src/gameplay/judgment_animator.hpp`, `src/gameplay/judgment_animator.cpp`
- **Action**: UPDATE
- **Implement**:
  - In the "Blaze 4k presentation constants" block, add `static constexpr float kJudgmentDisplayScale = 0.5f;` with the comment from Pinned Semantics. Reword the `kJudgmentPopPixel` comment to: "Bitmap-font pixel of the fallback label at full size (no theme / missing manifest entry). Drawn at `kJudgmentPopPixel * judgment_draw_scale(...)`."
  - Under the pop-curve statics, declare `[[nodiscard]] static float judgment_draw_scale(double elapsed);` with the comment "The judgment pop's draw scale: `kJudgmentDisplayScale * pop_scale(elapsed, kJudgmentPopSeconds)`. The same curve shape, at the display size. Pure."
  - In the `.cpp`, define it right after `pop_active`, as a one-liner over `pop_scale`.
  - Update the `render_judgment` comment, "(content top at kJudgmentTop)", to "centred on the full-size content box whose top is kJudgmentTop, drawn at `judgment_draw_scale`". Update the `judgment_pop_rect` comment so it says `scale` is relative to the full-size `content`.
- **Mirror**: `judgment_animator.hpp:26-31` (constants), `:71-76` + `.cpp:81-105` (pure statics)
- **Validate**: `cmake --build build -j`

### Task 2: `render_judgment` uses the display scale for both paths

- **File**: `src/gameplay/judgment_animator.cpp`
- **Action**: UPDATE
- **Implement**: In `render_judgment`, replace `const float scale = pop_scale(pop_elapsed_, kJudgmentPopSeconds);` with `const float scale = judgment_draw_scale(pop_elapsed_);`. The sprite rect, the `L.s * scale` sprite scale and `kJudgmentPopPixel * scale` then all shrink together. Change nothing else in the function (alpha, guards and the fallback anchor stay as they are).
- **Mirror**: `judgment_animator.cpp:192-212`
- **Validate**: `cmake --build build -j`, then `grep -n "pop_scale(pop_elapsed_" src/gameplay/judgment_animator.cpp` returns **no** hits (the helper is the only scale source)

### Task 3: `kJudgmentTop` comment

- **File**: `src/render/theme.hpp`
- **Action**: UPDATE (comment only; the value stays 296)
- **Implement**: `constexpr float kJudgmentTop = 296.0f;   // top of the full-size (444x66) judgment content box; the pop is drawn centred on it at JudgmentAnimator::kJudgmentDisplayScale`. Keep the line short enough to match the neighbouring style. A two-line comment above the constant is fine.
- **Validate**: `cmake --build build -j`

### Task 4: `judgment_animator_test` for the new size

- **File**: `tests/judgment_animator_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - `test_judgment_draw_scale()` (new):
    - `TEST_CHECK(JudgmentAnimator::kJudgmentDisplayScale == 0.5f)`.
    - At rest: `judgment_draw_scale(d) == 0.5f` and `judgment_draw_scale(d * 3.0) == 0.5f`, where `d = kJudgmentPopSeconds`.
    - Shape: for `i` in 0..1000, `std::abs(judgment_draw_scale(e) - kJudgmentDisplayScale * pop_scale(e, d)) < 1e-6f` with `e = d * i / 1000.0`. The scanned peak is `> kJudgmentDisplayScale` and within `1e-3f` of `kJudgmentDisplayScale * 1.25f`.
    - Print `"  - judgment draw scale (half size, same pop curve) ok.\n"`.
  - `test_judgment_pop_rect()` (extend; keep the existing scale-1.0 and 1.25 assertions, since the pure function is unchanged):
    - At 1280x720, `judgment_pop_rect(L, kJudgmentContentRef, judgment_draw_scale(d))` equals `{529, 312.5, 222, 33}` (use `1e-3f` tolerances). Its width and height are exactly half of the scale-1.0 rect's, and its centre is (640, 329).
    - At 2560x1440, with `content = {ref.x * L.s, ref.y * L.s}`, it equals `{1058, 625, 444, 66}`, centred on (1280, 658).
  - `test_judgment_clears_combo()` (new):
    - Scan the peak `judgment_draw_scale` as above. For window sizes `{1280x720, 2560x1440, 1920x1080, 1280x1024 (letterboxed), 2560x1080 (pillarboxed)}`, compute `pop = judgment_pop_rect(L, ref * L.s, peak)`.
    - Assert `pop.y + pop.h < L.y(kComboTop)`. Also assert that the glow-inclusive bottom is `<= L.y(kComboTop)`. The glow bottom is `pop.y + pop.h + pad * L.s * peak`, where `pad` is read from the **real manifest** for every `judgment_*` sprite: `(entry.height - entry.content.y - entry.content.h) / manifest.texture_scale`, 28 ref px today.
    - Assert that the pop's horizontal centre equals `L.x(640)` (centred on the field).
    - Print `"  - half-size judgment clears the combo line ok.\n"`.
  - Register both new functions in `main` right after `test_judgment_pop_rect();`.
- **Mirror**: `tests/judgment_animator_test.cpp:105-128` (manifest loop), `:139-151` (rect asserts), `tests/hud_renderer_test.cpp:199-205` (peak scan)
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/judgment_animator_test` (if the binary is elsewhere, locate it with `find build -name judgment_animator_test -type f`)

### Task 5: `hud_renderer_test` checks the real drawn peak

- **File**: `tests/hud_renderer_test.cpp`
- **Action**: UPDATE
- **Implement**: In `test_judgment_pop_clear` (`:199-218`), scan `JudgmentAnimator::judgment_draw_scale(d * i / 1000.0)` instead of `pop_scale(...)`, and change `TEST_CHECK(peak > 1.0f)` to `TEST_CHECK(peak > JudgmentAnimator::kJudgmentDisplayScale)`. The centring and life-bar-clearance asserts stay as they are. They were already satisfied at full size, so they hold a fortiori.
- **Validate**: `cmake --build build -j && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build -R "hud_renderer_test|judgment_animator_test" --output-on-failure`

### Task 6: Full validation

- Run everything in **Validation**. Expect 49/49, with no new warnings in the touched files.

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
cmake --build build -j 2>&1 | grep -iE "warning" | grep -E "judgment_animator|hud_renderer_test|theme\.hpp" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **49/49 pass** (baseline 49/49 on `368f8cd`; no executable is added).

Static checks:

```bash
# The factor lives in exactly one place
grep -n "0\.5f" src/gameplay/judgment_animator.hpp   # exactly one hit: kJudgmentDisplayScale (the header has no 0.5f today; the .cpp's 0.5f hits are pre-existing centring math)
# render_judgment's only scale source is the helper
grep -n "pop_scale(pop_elapsed_" src/gameplay/judgment_animator.cpp   # expect no hits
# Presentation only: timing / judgment / art untouched
git diff --stat -- src/timing src/audio src/gameplay/judgment_engine.cpp src/gameplay/score_keeper.cpp src/gameplay/life_keeper.cpp assets   # expect empty
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-stories   # expect 0
```

## End-to-End Verification

1. **Automated (agent-runnable):** `judgment_animator_test` pins three things: the 0.5 rest size, the 0.625 peak, the unchanged curve shape and the exact rects at 1280x720 and 2560x1440; combo clearance (glow included) at five window sizes against the real manifest; and the headless render calls, with null theme (the fallback path) and the real theme. `hud_renderer_test` pins centring and life-bar clearance at the drawn peak. Both must pass under the sandboxed ctest.
2. **Headless app smoke (agent-runnable, inside bwrap, scratch data dir):** `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 5 --start-screen select --data-dir <scratch>/data`. It exits with `Blaze 4k shut down cleanly.` and adds no error lines. Gameplay itself is not reachable headless from the CLI (`--start-screen` takes only `title` / `select`). Step 1 covers the render path. Do **not** add a CLI flag.
3. **Windowed visual check (owner; the agent must not launch the GUI, since it opens a window and plays audio):** play any song at 1280x720.
   - Each step shows its judgment word at about half the old size, centred over the field, slightly above where the old word's middle was.
   - It still pops (a quick grow, then settles) and fades.
   - Once the combo shows (4+), the word sits clearly above "N COMBO" and never touches it, even at the top of the pop.
   - Repeat maximised at 2560x1440 if available: proportionally the same.
   - If it feels too small or too big, change only `kJudgmentDisplayScale`.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The scale factor gets duplicated (sprite vs fallback vs tests drift) | One constant plus one pure helper. `render_judgment` reads only the helper (static grep). Tests reference the constant, not a literal, except the single `== 0.5f` pin | In scope |
| The pop curve shape changes by accident | `pop_scale` / `pop_alpha` are not edited. `test_judgment_draw_scale` pins `draw = K · pop_scale` over 1001 samples | In scope |
| The smaller sprite drifts off-centre or into the combo line | `judgment_pop_rect` is unchanged (centre-anchored). The clearance test at five sizes includes the glow pad read from the real manifest | In scope |
| The fallback label at 2.5 px cells looks soft or uneven (fractional quads) | Acceptable for a missing-theme fallback that players should never see. It still renders (headless test). Flag only | In scope (flag) |
| The fallback label ignores `L.s` (`kJudgmentPopPixel * scale`, no `L.s`), so at 2560x1440 it is the same pixel size as at 720p. This is **pre-existing** | The AC says "scaled down the same way". This plan applies the same 0.5 factor and nothing else. Making the fallback resolution-aware is a separate change | Out of scope (flag; see Open Question 2) |
| The manifest `layout_720p` for `judgment_*` ([418, 296, 444, 66]) no longer matches the drawn rect | It records the art's native placement and is documentation only. No test pins it for `judgment_*` (only title / select entries are checked). The issue forbids editing the art, so leave it | Out of scope |
| Timing path touched (principle 1) | Only presentation code changes. The `git diff --stat` check on timing/audio/judgment/score/life is in Validation | In scope |

---

## Open Questions

Neither of these blocks the work. The plan already uses each recommended default.

1. **Where the smaller word sits vertically.** Option A: keep it centred where the old word's middle was. Option B: keep its top edge where the old top was, which moves it up about 16 px and leaves a bigger gap above the combo.
   **Recommendation: A.** It needs no layout change, the word stays visually paired with the combo line below it, and it still clears that line by about 18 px at rest and about 1 px of glow at the top of the pop (about 18 px for the letters). B is a one-line change in `judgment_pop_rect` if the playtest prefers it.
2. **Fallback label size at high resolutions.** The plain-pixel fallback (only seen when the theme is missing) has never scaled with window size. **Recommendation:** leave it, as the issue asks only to halve it. If wanted, a follow-up can multiply it by `L.s`.
3. **Is 50% right?** This is the owner's estimate ("perhaps 50%"). **Recommendation:** ship 0.5 and tune `kJudgmentDisplayScale` after a playtest. Nothing else needs to change.

---

## Acceptance Criteria

- [ ] At rest the judgment sprite is drawn at 50% of its old size: {529, 312.5, 222, 33} at 1280x720 and {1058, 625, 444, 66} at 2560x1440 (tested)
- [ ] The pop keeps its shape relative to the new size: up to ~1.25x (0.625 absolute), settles at 1.0x (0.5 absolute), same fade (tested)
- [ ] The smaller judgment stays centred on the note field and clears the combo line at peak, glow included, at five window sizes. `kJudgmentTop` / `kComboTop` are unchanged (tested)
- [ ] The bitmap fallback label is drawn at `kJudgmentPopPixel * judgment_draw_scale(...)`, scaled down the same way
- [ ] `tests/judgment_animator_test.cpp` is updated for the new size and passes. `hud_renderer_test` still passes
- [ ] The factor lives in one constant, `JudgmentAnimator::kJudgmentDisplayScale`
- [ ] The build has no new warnings, and the sandboxed ctest passes 49/49
- [ ] Nothing in the timing / judgment / scoring path, or in `assets/`, changed. `.agents/stories/todo-stories.md` is untouched
