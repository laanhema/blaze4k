# Plan: Results Polish — Grade Animations and NEW RECORD Flow (D3)

## Summary

Phase D's last polish item behind issue #25 is purely presentational: make the C7 results screen
(`src/screens/results_screen.cpp`) reveal its grade arcade-style instead of drawing it statically,
give a new personal best a visually distinct celebratory "NEW RECORD" sequence, and let any input
skip the reveal to the final state. The data-correct C7 screen already exists and is untouched
semantically (it still submits exactly once in `enter` and still returns to the wheel). The approach
follows the repo's proven D2 seam: a new **pure, SDL/GL/audio/clock-free `ResultsAnimator`** model
(`src/screens/results_anim.{hpp,cpp}`, mirroring `gameplay/judgment_animator.*`) whose outputs are
pure functions of an accumulated `fixed_dt` (presentation only), plus a thin rewrite of
`ResultsScreen::render`/`update` that consumes it. No judgment/scoring/timing/persistence path
changes; no new numerical parity values are introduced (all curves are "Blaze 4k presentation,
unsourced" under the locked 2D-only decision). `fixed_dt` drives only the reveal, exactly as it
drives only the D2 pop fade.

## User Story

As a player
I want animated grade reveals and a celebratory NEW RECORD flow on the results screen, so that
personal bests feel rewarding
So that personal bests feel rewarding and the results screen reads like a shipped arcade product.

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW–MEDIUM (one pure model + screen rewrite; no data/logic changes) |
| Systems Affected | `src/screens/` (new pure animator + `ResultsScreen` update/render only), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #25 ([D3]) |
| PRD refs | §12 Phase D ("Grade animations on results; 'NEW RECORD' flow"), §15 locked decisions (2D only), §5 story 6, §7.3 Results |
| Depends on | C7 (#22 results screen — merged), D2 (#24 judgment animator + UI sounds — merged) |
| Blocks | — |

---

## Scope

In scope: grade reveal animation, a distinct NEW RECORD finale, and input-skip on the results
screen. Out of scope (contract, PRD §13): any change to scores schema/persistence, timing windows,
grade thresholds, gameplay HUD, and any video/3D/dancer effect (2D textured/solid quads + bitmap
text only). Gameplay judgment SFX and additional UI sounds are out of scope (D2 closed UI sounds).

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `build/` already host-configured for g++/Fedora 44 |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` (no `-Werror`) from root CMake |
| Cores | 16 | `-j16` safe |
| Baseline tests | **31/31 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 31" (0.49 s), run this session |
| Results screen (C7) | `src/screens/results_screen.{hpp,cpp}` | Stateless render already; `update` ignores `fixed_dt`; `render` draws grade statically (`results_screen.cpp:127-171`) |
| Existing pure animator | `src/gameplay/judgment_animator.{hpp,cpp}` | Presentation-only model, `fixed_dt` fade, no clock; the exact pattern to mirror |
| Percentage/grade format | `src/gameplay/hud_renderer.hpp:15,22` | `format_percent` (truncate 2 dp, `+1e-6`, clamp), `format_grade` (`quad_star`→`****`) |
| Alpha helper | `src/render/geometry.hpp:29` | `with_alpha(Color, float)` |
| Bitmap text | `src/render/bitmap_font.hpp:19-24` | `draw_text`, `draw_text_centered` (no rotation; scale = `pixel`, alpha via color) |
| Results palette | `src/screens/results_screen.cpp:19-43` | Local constants incl. `grade_color(percent)` tier coloring |
| UI sound trigger | `src/screens/screen_manager.cpp:210-220` | Manager fires Move/Confirm/Back on menu screens before `active->update`; Results is a menu screen — a skip press already beeps Confirm |
| Test idiom | `tests/results_screen_test.cpp:22-29,104-126` | `TEST_CHECK` abort macro; `ScreenManager(0.0)` fixture + `SelectSpy`; cases 4/5/7 assert Confirm/Back → Select |
| Test registration | `tests/CMakeLists.txt:282-290` | One `add_executable`/`target_link_libraries(blaze4k_core)`/`add_test` per target |

**Start green, stay green:** 31 tests pass; this plan adds **1** target (`results_anim_test`) and
extends `results_screen_test` in place → **32 expected**. No changes to `src/gameplay/*`,
`src/timing/*`, `src/chart/*`, `src/audio/*`, `src/render/*` (beyond consumption), `src/data/*`,
`src/screens/results.{hpp,cpp}`, `src/screens/screen_manager.*`, or `src/main.cpp`.

---

## Pinned Semantics

Authority: **PRD §12 Phase D / §15**, issue **#25 AC1–AC3**, and the existing **C7 `ResultsScreen`**
+ **D2 `JudgmentAnimator`** contracts. No OpenITG value is required: grade thresholds/labels and
percent/grade formatting are reused from C7 (`hud_renderer.*`); every animation curve, duration, and
the NEW RECORD flourish is "Blaze 4k presentation, unsourced". The only locked constraint is **2D only**
(textured/solid quads + bitmap text; no video/3D/dancers).

### Pure reveal model (`ResultsAnimator`)

The model is advanced only by the injected `fixed_dt` (never wall-clock) and never touches the
judgment path (the results screen has no judgment path at all). All outputs are pure functions of
`elapsed_` so the screen stays headless-testable and the GL layer is a no-op when uninitialized.

```cpp
// src/screens/results_anim.hpp — no SDL/GL/audio/<chrono>/<ctime>; <cmath> only.
namespace blaze4k {

class ResultsAnimator {
public:
    // Blaze 4k presentation constants (unsourced; no OpenITG parity requirement).
    static constexpr double kGradeDelay = 0.15;        // s after enter
    static constexpr double kGradePopSeconds = 0.85;   // arcade "slam" from 2.4x to 1.0
    static constexpr double kPercentDelay = 0.45;
    static constexpr double kPercentCountSeconds = 0.80;
    static constexpr double kStatsDelay = 0.70;
    static constexpr double kRecordDelay = 1.15;       // after grade/stats have landed
    static constexpr double kRecordSeconds = 1.20;     // pulse window
    static constexpr double kRecordPulsePeriod = 0.40;
    static constexpr double kRevealSeconds = 2.40;     // >= the end of every element

    void reset(bool valid, bool new_record, bool failed);
    void update(double fixed_dt);  // elapsed_ = min(elapsed_ + max(0,fixed_dt), kRevealSeconds)
    void skip();                   // elapsed_ = kRevealSeconds (jump to final frame)

    [[nodiscard]] bool finished() const;
    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool new_record() const;
    [[nodiscard]] bool failed() const;
    [[nodiscard]] double elapsed() const;

    // Pure curves (unit-tested). Never NaN; clamp to sane ranges; 0 before a
    // delay and the settled value at/after the element's end.
    [[nodiscard]] static float title_alpha(double elapsed);
    [[nodiscard]] static float grade_scale(double elapsed);   // 2.4x -> ~0.8x -> 1.0x
    [[nodiscard]] static float grade_alpha(double elapsed);
    [[nodiscard]] static double percent_progress(double elapsed); // smoothstep 0..1 count-up
    [[nodiscard]] static float stats_alpha(double elapsed);
    [[nodiscard]] static float record_alpha(double elapsed);  // 0, then pulse 0.75..1, then 1
    [[nodiscard]] static float record_scale(double elapsed);  // punch 0.6 -> 1.15 -> 1.0
    [[nodiscard]] static float record_flash(double elapsed);  // accent overlay fade
    [[nodiscard]] static float failed_alpha(double elapsed);
    [[nodiscard]] static float reveal_alpha(double elapsed, double delay, double duration);

private:
    bool valid_ = false;
    bool new_record_ = false;
    bool failed_ = false;
    double elapsed_ = 0.0;
};
}
```

Proposed curve shapes (implementer may tune within the pinned invariants; tests assert the
invariants, not the exact easing):

- `reveal_alpha(elapsed, delay, duration)`: `0` if `elapsed <= delay`, `1` if
  `elapsed >= delay+duration`, else a smoothstep `t*t*(3-2*t)` on `t=(elapsed-delay)/duration`.
- `title_alpha` = `reveal_alpha(elapsed, 0.0, 0.25)`.
- `grade_scale`: `1.0` before `kGradeDelay`; then `p=clamp((elapsed-kGradeDelay)/kGradePopSeconds,0,1)`;
  `p<0.7 → 2.4 - 1.6*(p/0.7)` (2.4 → 0.8, the slam + undershoot), else `0.8 + 0.2*((p-0.7)/0.3)`
  (0.8 → 1.0). Ends exactly `1.0`.
- `grade_alpha` = `reveal_alpha(elapsed, kGradeDelay, 0.25)` (fades in as it slams).
- `percent_progress` = smoothstep on `clamp((elapsed-kPercentDelay)/kPercentCountSeconds,0,1)`,
  monotonic `0 → 1`; the displayed value is `format_percent(percent * percent_progress)`, which at
  `1.0` equals the C7 `format_percent(percent)` exactly, and is non-decreasing (truncation of an
  increasing product).
- `stats_alpha` = `reveal_alpha(elapsed, kStatsDelay, 0.35)` (windows/DP/combo rows).
- `record_alpha`: `0` before `kRecordDelay`; during `[kRecordDelay, kRecordDelay+kRecordSeconds]`
  multiply the reveal alpha by `0.75 + 0.25*cos(2π*phase)`, `phase=(elapsed-kRecordDelay)/kRecordPulsePeriod`;
  after, `1.0`.
- `record_scale`: punch `0.6 → 1.15 → 1.0` over `kRecordDelay .. +0.5`, then `1.0`.
- `record_flash`: `max(0, 1 - (elapsed-kRecordDelay)/0.35)` for the first third-second of the finale.
- `failed_alpha` = `reveal_alpha(elapsed, kStatsDelay, 0.40)`.

### Fade/scale rendering contract

- Grade is drawn only past `kGradeDelay`, at `pixel = max(4.0, w*0.008) * grade_scale`, color
  `with_alpha(grade_color(percent), grade_alpha)`.
- Percent counts up with `percent_progress`; DP/max-combo/judgment-window rows fade in with
  `stats_alpha`; song title/artist fade in with `title_alpha`.
- NEW RECORD: the banner is **absent** on a normal clear; on a new record it appears at
  `kRecordDelay`, punched by `record_scale`, pulsed by `record_alpha`, with a full-screen accent
  `record_flash` overlay drawn *behind* the text. `failed_alpha` drives a red `FAILED` banner and
  disables the NEW RECORD finale (`new_record_` is already false for failed runs — `results.cpp:35-38`).
- Footer hint: while `!finished()` draw `[ENTER] SKIP`; when finished draw the C7 hints
  (`[ENTER] CONTINUE` or `[ENTER] RETURN TO WHEEL`). The `NO RESULT` path (invalid summary) skips
  animation entirely and behaves exactly as C7.

### Input-skip semantics

- `update` first advances `animator_.update(fixed_dt)`.
- On a pressed `Confirm` / `Options` / `Right`: if `!animator_.finished()`, call `animator_.skip()`
  and return **without navigating** (AC3); if finished, transition to `Select` (C7 behavior).
- `Back` is unchanged: the manager's default back-navigation (`screen_manager.cpp:159-160`) still
  returns to `Select` immediately, with or without a finished reveal.
- This changes C7 observable behavior (a fresh Results now needs two Confirm presses to exit) —
  see **Open Question 1**.

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Grade thresholds/labels and percent/grade formatting | Reused C7 (`hud_renderer.hpp:15,22`; `judgment_constants.cpp`) | Sourced (reused, unchanged) |
| NEW RECORD rule (`new_record_` from `submit_high_score`) | Reused C7 (`results_screen.cpp:62-64`, `high_scores.cpp`) | Sourced (reused, unchanged) |
| Results palette incl. `grade_color` | Reused C7 (`results_screen.cpp:19-43`) | Sourced (reused, presentation) |
| All reveal delays/durations/curves, pulse, flash | PRD/issue silent; "Blaze 4k presentation, unsourced" (cf. D2 constants) | **Design decision — OQ2** |
| Skip press set + two-press Confirm behavior | Issue AC3 silent on which press / whether it also exits | **Design decision — OQ1** |
| First-ever clear gets the full NEW RECORD finale | Carried from C7 `submit_high_score` semantics | **Design decision — OQ3** |
| `fixed_dt` as the reveal clock | Presentation only; results has no judgment path; `PreviewPlayer`/`JudgmentAnimator` precedent | Sourced (pattern) |

No judgment windows, DP weights, grade boundaries, life deltas, timing values, or persisted fields
are introduced or changed.

---

## Patterns to Follow

### Pure presentation animator (no clock, headless no-op)
```cpp
// SOURCE: src/gameplay/judgment_animator.hpp:18-68; .cpp:35-99
void JudgmentAnimator::update(double fixed_dt, int combo) {
    if (has_pop_) { pop_elapsed_ += fixed_dt; if (!pop_active(...)) has_pop_ = false; }
}
```

### Fixed-dt presentation timer (never a gameplay clock)
```cpp
// SOURCE: src/audio/preview_player.cpp:30-34
if (state_ == PreviewState::Waiting) { timer_ += fixed_dt; if (timer_ < delay_seconds_) return; }
```

### Alpha + centered bitmap text
```cpp
// SOURCE: src/render/geometry.hpp:29; src/render/bitmap_font.hpp:23
constexpr Color with_alpha(Color color, float alpha);
void draw_text_centered(GlQuadRenderer&, const std::string&, float center_x, float y, float pixel, Color);
```

### Existing static grade draw to be animated
```cpp
// SOURCE: src/screens/results_screen.cpp:127-171
const GradeTier tier{0.0, summary_.grade_label.c_str()};
draw_text_centered(renderer, format_grade(tier), width*0.5f, height*0.31f,
                   std::max(4.0f, width*0.008f), grade_color(summary_.percent));
```

### Screen lifecycle + explicit transition
```cpp
// SOURCE: src/screens/results_screen.cpp:77-91
void ResultsScreen::update(ScreenContext& ctx, double /*fixed_dt*/, const std::vector<InputEvent>& events) { ... }
// SOURCE: src/screens/screen_manager.cpp:159-160
} else if (active_id_ == ScreenId::Results) { transition_to(ScreenId::Select); }
```

### Test idiom + registration
```cpp
// SOURCE: tests/results_screen_test.cpp:22-29,104-126; tests/CMakeLists.txt:282-290
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
add_executable(results_anim_test results_anim_test.cpp)
target_link_libraries(results_anim_test PRIVATE blaze4k_core)
add_test(NAME results_anim_test COMMAND results_anim_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/results_anim.hpp` | CREATE | Pure `ResultsAnimator`: timeline, skip, static curves |
| `src/screens/results_anim.cpp` | CREATE | Curve implementations; no SDL/GL/audio/clock |
| `src/screens/results_screen.hpp` | UPDATE | Add `ResultsAnimator animator_` member + `animator()` / `reveal_finished()` test accessors; refresh header comment |
| `src/screens/results_screen.cpp` | UPDATE | `enter` resets the animator; `update` advances + skip-vs-exit; `render` consumes the curves (scale/alpha/count-up/NEW RECORD finale/skip hint) |
| `tests/results_anim_test.cpp` | CREATE | Pure curve/timeline/skip tests (no GL) |
| `tests/results_screen_test.cpp` | UPDATE | Confirm now skips then exits; add skip/new-record-finale/reveal-active cases; keep all other cases green |
| `tests/CMakeLists.txt` | UPDATE | Register `results_anim_test` |
| `CMakeLists.txt` | UPDATE | Add `src/screens/results_anim.cpp` to `blaze4k_core` |

Not modified: `src/screens/results.{hpp,cpp}`, `src/screens/screen_manager.*`,
`src/screens/screen.hpp`, `src/main.cpp`, `src/gameplay/*`, `src/timing/*`, `src/chart/*`,
`src/audio/*`, `src/data/*`, `src/render/*`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure `ResultsAnimator` model

- **File**: `src/screens/results_anim.hpp`, `src/screens/results_anim.cpp`
- **Action**: CREATE
- **Implement**: Per **Pinned Semantics**. Header includes only `<cstdint>`-free basics (this module
  needs no integer types) and `<cmath>` lives in the `.cpp`. `reset` stores the three flags and
  zeroes `elapsed_`. `update` clamps `fixed_dt` to `>= 0` and `elapsed_` to `kRevealSeconds`.
  `skip` sets `elapsed_ = kRevealSeconds`. Implement each static curve as described, guarding
  `elapsed <= 0` and `duration <= 0` (return the settled value) so nothing is NaN. Keep it free of
  `Color`/GL/SDL; it returns only floats/doubles/bools.
- **Mirror**: `src/gameplay/judgment_animator.hpp:18-68`, `.cpp:75-99`.
- **Validate**: `cmake --build build -j16` (after Task 5 registers the `.cpp`).

### Task 2: `ResultsScreen` integration

- **File**: `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - `.hpp`: `#include "screens/results_anim.hpp"`; add `ResultsAnimator animator_;`; add accessors
    `[[nodiscard]] const ResultsAnimator& animator() const { return animator_; }` and
    `[[nodiscard]] bool reveal_finished() const { return animator_.finished(); }`. Refresh the class
    comment to note D3 reveal/skip.
  - `.cpp::enter`: after the submit block, `animator_.reset(summary_.valid, new_record_, summary_.failed);`
    (reset even for invalid so re-entry is clean).
  - `.cpp::update`: replace the body with: `animator_.update(fixed_dt);` then for each pressed event
    with `Confirm`/`Options`/`Right`: if `!animator_.finished()` → `animator_.skip(); return;`
    else `transition_to(ScreenId::Select); return;`. `fixed_dt` is now used (remove the `/*fixed_dt*/`
    comment). Back remains manager-owned.
  - `.cpp::render`: keep the backdrop + invalid path unchanged. For a valid summary:
    title/artist at `title_alpha`; grade drawn only when `grade_alpha > 0` with
    `pixel*grade_scale` and `with_alpha(grade_color, grade_alpha)`; percent as
    `format_percent(summary_.percent * animator_.percent_progress())`; DP/max-combo/windows/holds at
    `stats_alpha`; `failed` red banner at `failed_alpha`; if `animator_.new_record()` draw the accent
    `record_flash` full-screen overlay *before* the banner then the `NEW RECORD` banner with
    `record_scale`/`record_alpha`; footer hint `[ENTER] SKIP` while `!finished()`, else the C7
    `[ENTER] CONTINUE` / `[ENTER] RETURN TO WHEEL`. Every draw stays through
    `draw_text_centered`/`draw_quad` (headless no-op).
- **Mirror**: `src/screens/results_screen.cpp:47-172`; `src/gameplay/judgment_animator.cpp:131-152`.
- **Validate**: `cmake --build build -j16`; `./build/tests/results_screen_test` (updated in Task 4).

### Task 3: `results_anim_test` (pure model)

- **File**: `tests/results_anim_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no GL/SDL/window):
  1. **Initial**: `reset(...)` → `!finished()`, `elapsed()==0`; all alphas `0` (except settled-by-design
     values are only reached later).
  2. **Grade slam**: `grade_scale(0) == 1.0f`; `grade_scale(kGradeDelay) == 2.4f`;
     `grade_scale(kGradeDelay+kGradePopSeconds) == 1.0f`; some sample inside has value `< 1.0f`
     (the undershoot); `grade_alpha(kGradeDelay+kGradePopSeconds) == 1.0f`.
  3. **Percent count-up**: `percent_progress(0)==0.0`, `percent_progress(kRevealSeconds)==1.0`,
     monotonic across a few samples.
  4. **Stats/title/failed reveals**: `reveal_alpha` is 0 before delay and 1 after; `stats_alpha`
     and `failed_alpha` likewise; title fades in first.
  5. **NEW RECORD finale**: with `reset(true,true,false)`, `record_alpha`/`record_scale`/`record_flash`
     are 0 before `kRecordDelay`; `record_alpha` is >0 and oscillates in the pulse window;
     `record_alpha(kRevealSeconds)==1.0`; `record_flash` decays to 0. With `new_record=false` the
     finale queries are all 0.
  6. **Skip / finished**: after `skip()`, `finished()` is true, `record_alpha==1`, `percent_progress==1`,
     `grade_scale==1`; `update(kRevealSeconds*2)` also reaches finished; `update(-1.0)` does not move
     backwards; degenerate `reveal_alpha(x, d, 0)` returns the settled value without NaN.
  7. `reset(false,false,false)` is safe; queries do not crash.
- **Mirror**: `tests/judgment_animator_test.cpp` (curve invariant style); `tests/texture_test.cpp:12-19`.
- **Validate**: `./build/tests/results_anim_test` → exit 0.

### Task 4: Update `results_screen_test`

- **File**: `tests/results_screen_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - **Case 4 (Confirm → wheel)** and the final step of **case 7 (end-to-end)**: the first Confirm
    press now skips (`TEST_CHECK(fx.results->reveal_finished())`, active still `Results`), and a
    second Confirm press transitions to `Select`. Adjust the end-to-end final assertion likewise.
  - **New case: reveal gating** — after `start()`, `!reveal_finished()`; a `Confirm` press does
    **not** change `active_id()` and sets `reveal_finished()`; `[ENTER] CONTINUE` path only after.
  - **New case: NEW RECORD finale flag** — a first clear (`test_enter_submit_and_flag` summary)
    yields `results->animator().new_record() == true`; a pre-seeded higher score (case 2 fixture)
    yields `false`; a failed summary yields `false`.
  - Keep cases 1/2/3/3b/5/6 and case 7's ticket/submit/once-only assertions unchanged.
- **Mirror**: `tests/results_screen_test.cpp:191-201,233-300`.
- **Validate**: `./build/tests/results_screen_test` → exit 0.

### Task 5: Register source + test target

- **File**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: add `src/screens/results_anim.cpp` to the `blaze4k_core` source list (next to
  `src/screens/results.cpp` / `results_screen.cpp`), and append the `results_anim_test` block
  mirroring `tests/CMakeLists.txt:282-290`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 6: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build, run everything, scan for new warnings.
- **Validate**: see **Validation** below (`ctest` → **32/32**, no warnings).

---

## Validation

```bash
# Configure (CMake files changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 32/32: 31 existing + results_anim_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/results_anim_test
./build/tests/results_screen_test
./build/tests/screen_manager_test
./build/tests/judgment_animator_test

# Purity: the animator must stay free of SDL/GL/audio/wall-clock
rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time|<ctime>" \
  src/screens/results_anim.hpp src/screens/results_anim.cpp
# expected: no matches

# Warning budget (no new -Wswitch / unused-parameter / conversion warnings)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none

# Scope: no gameplay/scoring/timing/persistence edits
git diff --name-only | rg "gameplay/|timing/|chart/|audio/|data/|screen_manager|main\.cpp|results\.hpp|results\.cpp" ; # expected: none
```

## End-to-End Verification

All steps are headless/non-blocking (no window/GL/audio device required); use `--data-dir` so the
developer's real `data/` is untouched.

1. **Grade animates, then settles (AC1)**: `results_anim_test` cases 2–5 pin the slam curve, the
   count-up, and that every element reaches its final value by `kRevealSeconds`; `results_screen_test`'s
   headless `render` exercises the animated draw path with an uninitialized `GlQuadRenderer` (no-op).
2. **NEW RECORD visually distinct (AC2)**: `results_anim_test` case 5 proves the finale is inert on a
   normal clear and active on a record; `results_screen_test`'s finale-flag case ties it to
   `new_record_` from the real submit path. Manual (display): clear a chart twice — the second,
   better run plays the punch/pulse/flash the first did not.
3. **Input skips to final state (AC3)**: `results_screen_test`'s reveal-gating case asserts a press
   during the reveal finishes it without navigating; the updated Confirm cases assert the second
   press exits to `Select`. Manual: press a key mid-reveal and confirm the screen jumps to the final
   stats with the footer hint intact.
4. **No dead ends / no double submit (regression)**: `results_screen_test` cases 4/5/7 and the
   once-only assertions stay green; `screen_manager_test` back-nav is untouched.
5. **Regression suite**: `ctest --test-dir build --output-on-failure` → **32/32**.
6. **Smoke (optional)**: a fresh data dir, run the shell headless and confirm clean exit; e.g.
   ```bash
   rm -rf /tmp/blaze4k-e2e-d3 && mkdir -p /tmp/blaze4k-e2e-d3
   ./build/blaze-4k --headless --smoke-test 30 --start-screen select \
     --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-d3
   # exit 0
   ```
7. `git status` shows new files under `src/screens/` (`results_anim.*`) and `tests/`
   (`results_anim_test.cpp`); edits limited to `results_screen.*`, `tests/results_screen_test.cpp`,
   and the CMake files.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Confirm from a fresh Results now needs two presses to exit (behavior change from C7) | Intentional per AC3; documented; test updates pin "first skips, second exits"; surfaced as OQ1 | **In scope** (flag OQ1) |
| Count-up display ever exceeds/undershoots the true percent | `percent_progress` clamped to [0,1]; at 1.0 it is exactly C7's `format_percent(percent)`; truncation of an increasing product is non-decreasing | **In scope** |
| Grade scale/alpha produce NaN or a reversed curve | Pure guarded curves (`duration<=0`, `elapsed<=0`, clamps); `results_anim_test` asserts endpoints/overshoot | **In scope** |
| NEW RECORD finale shows on a normal clear or after a failed run | Gated on `animator_.new_record()` (itself from C7's `submit_high_score`, false for failed/unstorable runs); `results_anim_test` case 5 + `results_screen_test` finale-flag case | **In scope** |
| Animation driven by wall-clock or the music clock drift | Only `fixed_dt` advances the model (presentation), matching D2; results has no judgment path; purity grep in Validation | **In scope** |
| Skip press accidentally double-acts (skip *and* navigate) | Single `if/else` in `update`; reveal-gating test asserts active id is unchanged on the skip press | **In scope** |
| Rotation-based "arcade" flair impossible with bitmap text (no rotation) | Reveal uses scale/alpha/count-up/punch/flash only (2D-only locked decision); surfaced as OQ2 for flavor expectations | **Flagged** — OQ2 |
| `draw_text_centered` with `pixel*grade_scale` at 2.4x is very large / clips | Grade base pixel is small (`max(4.0, w*0.008)`); clamp scale in the curve; visual check (manual) | **In scope** |
| Headless render hits GL | All draws via the no-op-when-uninitialized `GlQuadRenderer`; `results_screen_test` case 6 renders uninitialized | **In scope** |

---

## Decisions

- **New pure `ResultsAnimator`, mirroring `JudgmentAnimator`.** Keeps the reveal deterministic and
  headless-testable, advancing only on `fixed_dt`; the screen stays a thin consumer. No new state on
  the data model.
- **Animate presentation only — never the numbers.** The grade, percent, DP, and counts are already
  pinned by C7; the animator only supplies scale/alpha/progress. At completion it is byte-for-byte
  the C7 output.
- **Skip is explicit and reversible-free.** `skip()` jumps to `kRevealSeconds`; `finished()` then
  gates navigation, so a single mechanism serves both the reveal and the exit.
- **NEW RECORD distinctness via a delayed punch/pulse/flash gated on `new_record_`.** Normal clears
  and failures never trigger it, so the sequence is unambiguous.
- **No new sounds, no settings, no schema.** D2 covered UI sounds; "reduce motion" is deferred
  (OQ5). Scores/structure untouched.
- **All reveal constants are unsourced presentation**, consistent with D2 ("Blaze 4k presentation, no
  OpenITG parity requirement"); no Value Provenance parity claim is made.

---

## Open Questions

1. **Blocking-ish — skip press semantics.** AC3 says input skips to the final state, but C7's
   Confirm/Options/Right already exit to the wheel. Proposed default: **Confirm/Options/Right skip
   while the reveal is running, and exit once finished** (two presses from a fresh Results). Back
   keeps exiting immediately via the manager. Alternative: only directions skip and Confirm always
   exits; or any press skips *and* a second press exits. Confirm, since it changes existing C7 tests.
2. **Non-blocking — reveal flavor and durations.** Proposed defaults: grade slam `2.4x → 0.8x → 1.0x`
   over `0.85 s` starting `0.15 s` after entry, percent count-up over `0.8 s`, stats at `0.7 s`, NEW
   RECORD punch/pulse/flash at `1.15 s`, total `2.4 s`; scale/alpha/count-up only (bitmap text has no
   rotation). Tune or accept. Say if a specific ITG/StepMania reveal style is expected.
3. **Non-blocking — first-ever clear gets the full finale?** C7's `submit_high_score` returns true
   for the first entry, so the very first clear plays the NEW RECORD sequence. Proposed default:
   **yes** (it is a personal best). Alternative: finale only when a prior record was beaten.
4. **Non-blocking — final combo on the results screen.** D2 added `JudgmentAnimator::celebrate` at
   run completion; D3 currently animates only the static results rows. Proposed default: **no extra
   combo flourish** on Results (out of #25's ACs). Confirm if wanted.
5. **Non-blocking — reduced-motion / auto-skip.** No settings screen hosts a motion toggle. Proposed
   default: **none for v1**; skip is manual. Confirm.
6. **Non-blocking — invalid `NO RESULT` path.** Proposed default: **no animation** (exactly C7),
   since there is nothing to reveal. Confirm.

---

## Acceptance Criteria

- [ ] Given the results screen, when it appears, the grade animates in arcade-style (slam
      scale/alpha + percent count-up) and settles to the exact C7 values (Tasks 1–3; E2E 1)
- [ ] Given a new record, the NEW RECORD sequence is visually distinct (delayed punch/pulse/flash)
      and absent on normal clears and failures (Tasks 1–2; E2E 2)
- [ ] Given the animations, a pressed input skips them to the final state without navigating; a
      subsequent Confirm/Back exits to the wheel (Tasks 2/4; E2E 3)
- [ ] `ctest --test-dir build --output-on-failure` → **32/32**; `results_screen_test`,
      `screen_manager_test`, `judgment_animator_test` stay green (Tasks 3–6; E2E 4/5)
- [ ] `src/screens/results_anim.*` stays SDL/GL/audio/wall-clock-free; zero new warnings under
      `-Wall -Wextra -Wpedantic` (Validation)
- [ ] No gameplay/scoring/timing/persistence changes; `fixed_dt` drives only the reveal
      (Validation scope grep)
- [ ] Open Questions OQ1–OQ6 confirmed or defaults accepted (skip semantics, reveal flavor, first-
      record finale, combo flourish, reduced-motion, invalid path)
