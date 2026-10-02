# Plan: Extend X-mod Choices up to 8x (#61)

## Summary

The options overlay's X-mod list stops at 6x (`kXModValues` in
`src/screens/options_menu.cpp:40`). Add whole-number steps **7x** and **8x** to the end of the grid,
and make the X-mod value row **wrap**: Right/Confirm on 8x goes to 1x and Left on 1x goes to 8x,
mirroring how the speed-type row already cycles (`options_menu.cpp:179`). Persistence and gameplay
need no production changes. `options_menu_apply` already writes `"7x"`/`"8x"` through
`format_speed_mod`, `parse_speed_mod` accepts any finite positive value, and `NoteField` multiplies
beat spacing by the resolved X speed with no cap. Tests are added or updated to cover the new grid, the
wrap, the save/restore round-trip and the 8x scroll spacing.

## User Story

As a fast-reading player
I want to pick 7x and 8x in the options menu
So that I can read dense charts at my preferred higher scroll speed

## Metadata

| Field | Value |
|-------|-------|
| Type | ENHANCEMENT |
| Complexity | LOW |
| Systems Affected | `screens/options_menu` (data + adjust logic), tests (`options_menu_test`, `note_field_test`, `config_persistence_test`) |
| GitHub Issue | #61 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Unix Makefiles, `build/_deps` populated) |
| C++ Compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic` (no `-Werror`) |
| Cores | 16 | `-j16` is safe |
| Baseline tests | **37/37 pass** | `cmake --build build -j8 && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure` gives "100% tests passed out of 37" (0.44 s). Run this session on `main` @ `f25f904` |
| Options model | `src/screens/options_menu.{hpp,cpp}` | Pure (no SDL/GL/audio); the X grid is `kXModValues` at `:40`, X stepping at `:183-199` (**currently clamps**, does not wrap) |
| Options dispatch | `src/screens/select_screen.cpp:393-418` | Left gives `options_menu_adjust(-1)` and Right/Confirm give `+1`, then `options_menu_apply(*ctx.config)` on every change. **No change needed** |
| Speed parse | `src/gameplay/speed_mod.cpp:59-95` | Accepts `"Nx"` for any finite N > 0. **No change needed** |
| Gameplay consumption | `src/gameplay/gameplay_options.cpp:12`, `note_field.cpp:7-16,49` | `offset = Δbeat * pixels_per_beat * resolved_x_speed_`, with no upper clamp. **No change needed** |
| Config persistence | `src/data/config_loader.cpp:220-226,395` | `speed_mod` is an opaque non-empty string. **No change needed** |

**Start green, stay green:** 37 tests pass. This plan adds **no** new test targets; it only extends
three existing ones, so **37 are expected** afterwards.

---

## Pinned Semantics

1. **Grid** (ordered): `{1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8}`. That is 10 entries, whole-number steps
   above 6x only (per the issue's technical note). No 3.5x, 4.5x, and so on.
2. **X value row wraps** in both directions:
   - `+1` on 8x gives 1x, and `-1` on 1x gives 8x.
   - Index math mirrors the type cycle: `next = ((nearest + delta) % count + count) % count`.
   - Ten `+1` presses from any grid value return to that value.
3. **Off-grid seeded X values** (for example a hand-edited `"3.3x"` or `"10x"`) keep the existing snap-to-nearest
   behaviour (`options_menu.cpp:186-195`, where a tie goes to the lower index) and then step with wrap. For
   example, `"10x"` snaps to 8x, so `+1` gives 1x and `-1` gives 7x. Seeding itself does not clamp
   (`options_menu_from_config` writes `x_value` directly), so an off-grid value still persists
   until the player touches the row. This is unchanged.
4. **C/M rows are unchanged.** They keep stepping relative to the current value and clamp to [1, 9999]
   (`options_menu.cpp:200-205`). Wrapping applies to the X row only.
5. `OptionsMenu::set_speed_value` keeps clamping X to `[values.front(), values.back()]`, which now means
   [1, 8]. This is still correct because the wrap is done by index before calling it.
6. Display/persist strings: `format_number(7.0)` gives `"7"`, so the row shows `"7x"`/`"8x"` and
   `config.gameplay.speed_mod` is `"7x"`/`"8x"`. Both strings parse back to XMod 7/8.

## Value Provenance

The existing grid cites OpenITG commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`
(`options_menu.cpp:15-38`).

| Value | Source | Notes |
|-------|--------|-------|
| 1x..6x (8 entries) | OpenITG `assets/patch-data/Themes/default/metrics.ini:3694-3701` (already cited) | Unchanged |
| 8x | OpenITG `src/CodeDetector.cpp:220-221` (in-game speed-code sequence, which ends at 8.0; already cited in the comment) | OpenITG offers 8x as a speed, but through the code gesture rather than the PlayerOptions menu |
| 7x | **No OpenITG source**: product-owner request (issue #61, `TODO.md:36`) | A deliberate, documented deviation from the OpenITG menu grid |
| Wrap at the ends | Issue #61 AC ("wrapping from 8x back to 1x works") | Behaviour change: today the X row clamps (`options_menu_test.cpp:129-131` asserts the 6x ceiling) |

The provenance comment block must be updated to state that the grid is OpenITG's menu values
extended to 8x at the owner's request. Don't present 7x as OpenITG-sourced.

---

## Patterns to Follow

### Data-driven value table (single source)
```cpp
// SOURCE: src/screens/options_menu.cpp:40, 92-100
constexpr double kXModValues[] = {1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0};
...
std::vector<double> options_speed_values(SpeedModType type) {
    if (type == SpeedModType::XMod) {
        return std::vector<double>(std::begin(kXModValues), std::end(kXModValues));
    }
```

### Wrapping cycle (mirror for the X value row)
```cpp
// SOURCE: src/screens/options_menu.cpp:171-181
index = ((index + delta) % 3 + 3) % 3;
menu.speed_type = kSpeedTypeOrder[index];
```

### Current clamped X stepping (to be replaced with a wrap)
```cpp
// SOURCE: src/screens/options_menu.cpp:196-199
const long next =
    std::clamp(static_cast<long>(nearest) + delta, 0L,
               static_cast<long>(values.size()) - 1L);
menu.set_speed_value(values[static_cast<std::size_t>(next)]);
```

### Error handling
None is new. `parse_speed_mod` failures already fall back to the defaults
(`options_menu.cpp:133-147`, `gameplay_options.cpp:12-16`). Nothing in this change can fail.

### Tests (idiom)
```cpp
// SOURCE: tests/options_menu_test.cpp:10-17, 117-131
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
...
menu.set_speed_value(6.0);
blaze4k::options_menu_adjust(menu, +1);
TEST_CHECK(near(menu.x_value, 6.0)); // ceiling   <-- becomes wrap assertions
```
Each test is a `void test_*()` function that ends with a `std::cout << "  - ... ok.\n"` line and is
called from `main()` (`options_menu_test.cpp:330-344`). `note_field_test` uses numbered `{ ... }` blocks inside
`main()` with `approx()` (`note_field_test.cpp:23, 105-133`).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/screens/options_menu.cpp` | UPDATE | Append 7.0, 8.0 to `kXModValues`. Make X value stepping wrap. Update the provenance comment |
| `src/screens/options_menu.hpp` | UPDATE | Doc comments: `options_menu_adjust` ("clamps" becomes "X wraps; C/M clamp") and `options_speed_values` (the grid is no longer purely OpenITG-sourced) |
| `tests/options_menu_test.cpp` | UPDATE | Grid contents, forward/backward wrap, full cycle, 7x/8x display, apply/parse/seed round-trip, off-grid snap+wrap |
| `tests/note_field_test.cpp` | UPDATE | 8x offset equals 8 × the 1x offset (gameplay AC) |
| `tests/config_persistence_test.cpp` | UPDATE | Save/load round-trip of `"8x"` (the "restored on next launch" AC) |
| `TODO.md` | UPDATE (optional) | Tick `:36` (`- [ ]` becomes `- [x]`) if the repo's convention is to tick TODO items on completion. Check git history first, and leave it alone if unsure |

No new files and no CMake changes.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Extend the X grid and make the X row wrap

- **File**: `src/screens/options_menu.cpp`
- **Action**: UPDATE
- **Implement**:
  1. `:40`: `constexpr double kXModValues[] = {1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};`
  2. `:196-199`: replace the `std::clamp` index with a wrapping index:
     ```cpp
     const long count = static_cast<long>(values.size());
     const long next = ((static_cast<long>(nearest) + delta) % count + count) % count;
     menu.set_speed_value(values[static_cast<std::size_t>(next)]);
     ```
     Keep the nearest-snap loop above it unchanged. `<algorithm>` is still needed for the other
     `std::clamp` uses.
  3. `:15-38` provenance comment: state that the grid is the OpenITG PlayerOptions values
     (1x..6x, `metrics.ini:3694-3701`) **extended with 7x and 8x per owner request (#61)**. Note that 8x
     also appears in OpenITG's speed-code sequence (`CodeDetector.cpp:220-221`) and that 7x has no
     OpenITG source. Add a line saying the X row wraps at both ends (#61), while C/M clamp.
- **Mirror**: `options_menu.cpp:179` (type-row wrap)
- **Validate**: `cmake --build build -j16 --target blaze4k_core options_menu_test`. This compiles.
  `options_menu_test` is **expected to fail** at the old ceiling assertion until Task 3.

### Task 2: Update header doc comments

- **File**: `src/screens/options_menu.hpp`
- **Action**: UPDATE
- **Implement**:
  - `:63`: change "Advances the highlighted row's value by `delta` (row-specific; clamps)." to
    say that the X-mod value and speed type wrap, C/M values clamp to [1, 9999], and toggles flip.
  - `:66-70`: change "The ordered grid the menu steps through for X-mod (sourced from OpenITG ...)"
    to "OpenITG menu values extended to 8x; see options_menu.cpp for provenance".
- **Validate**: `cmake --build build -j16`

### Task 3: Options-menu unit tests for 7x/8x and wrap

- **File**: `tests/options_menu_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `test_speed_value_step_clamp` (`:117-131`), keep the 1x to 1.5x step. **Replace** the X floor/ceiling
    assertions:
    - Assert the grid: `options_speed_values(XMod)` has size 10, the back is 8.0, and the elements at [8] and [9]
      are 7.0 and 8.0.
    - Start at 6x, then `+1` gives 7x, `+1` gives 8x, and `+1` gives **1x** (wrap forward).
    - Start at 1x, then `-1` gives **8x** (wrap backward), and `-1` gives 7x.
    - Full cycle: from 2.5x, ten `+1` presses return to 2.5x.
  - Optionally rename the function to `test_speed_value_step_wrap_clamp` (and update `main()`). Leave the C/M
    clamp assertions unchanged.
  - Add `test_xmod_high_values_round_trip()`:
    - For 7.0 and 8.0: the `options_row_value_text(menu, kSpeedValueRow)` result equals `"7x"`/`"8x"`.
      `options_menu_apply` sets `config.gameplay.speed_mod == "7x"`/`"8x"`. `parse_speed_mod` gives XMod
      7/8. `options_menu_from_config` restores `speed_type == XMod` and `x_value` 7/8.
    - Seed from config `"8x"`, set `row = kSpeedValueRow`, `adjust(+1)`, then apply. The result is
      `speed_mod == "1x"` (the wrap persists).
    - Off-grid: seed `"10x"`. `x_value` stays 10 until adjusted, then `adjust(-1)` gives 7x (snap to 8x, then
      step down).
  - Register the new function in `main()`.
- **Mirror**: `options_menu_test.cpp:117-165, 281-326`
- **Validate**: `cmake --build build -j16 --target options_menu_test && ./build/tests/options_menu_test`
  (or `ctest --test-dir build -R options_menu_test --output-on-failure`)

### Task 4: Gameplay scroll at 8x

- **File**: `tests/note_field_test.cpp`
- **Action**: UPDATE
- **Implement**: add a numbered block next to the X-mod BPM-segment block (`:105-133`). Use the same
  chart shape (one tap at beat 16, `0=120`) and two fields set to `XMod 1.0` and `XMod 8.0`. For several `t`,
  assert `approx(x8.offset_for_note(n, t), 8.0 * x1.offset_for_note(n, t))` and
  `approx(x8.effective_x_speed(), 8.0)`. Print `"  - X-mod 8x scales beat spacing 8x."`.
- **Mirror**: `note_field_test.cpp:135-158` (M-mod vs X-mod equivalence loop)
- **Validate**: `ctest --test-dir build -R note_field_test --output-on-failure`

### Task 5: Persistence round-trip of "8x"

- **File**: `tests/config_persistence_test.cpp`
- **Action**: UPDATE
- **Implement**: after block 2 (`:115-157`), add a small block to a fresh temp path. Set
  `config.gameplay.speed_mod = "8x"`, run `save_config`, then `load_config`. Assert the status is `LoadedFromFile` and
  `loaded.gameplay.speed_mod == "8x"`. Reuse the existing temp-dir and `message`/`status` helpers in that
  file; don't alter block 2's `"C400"` assertions.
- **Mirror**: `config_persistence_test.cpp:115-157`
- **Validate**: `ctest --test-dir build -R config_persistence_test --output-on-failure`

### Task 6: Full suite

- **Validate**: the full build plus ctest (see Validation). Expect **37/37** passing and no new warnings in the
  touched files.

---

## Validation

```bash
# Build (no CMake files change, so no reconfigure is needed)
cmake --build build -j16 2>&1 | grep -E "warning|error" ; cmake --build build -j16

# Tests: expect 37/37 (headless, with no sound or window)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# Targeted
ctest --test-dir build -R "options_menu_test|note_field_test|config_persistence_test" --output-on-failure

# Scope guard: only the options model and tests change
git diff --stat   # expect src/screens/options_menu.{hpp,cpp} + 3 test files (+ optional TODO.md)
git diff --name-only | grep -E "select_screen|speed_mod|note_field\.cpp|gameplay_options|config_loader"  # expect no output
```

There is no linter configured, so the `-Wall -Wextra -Wpedantic` build output is the lint gate.

## End-to-End Verification

1. **Automated (headless, required):** the targeted ctest run above. `options_menu_test` exercises the
   exact functions `SelectScreen` calls on Left/Right/Confirm (`select_screen.cpp:405-417`),
   followed by `options_menu_apply`, the same call that writes the config `main` saves on exit.
2. **Scratch check against the real entry points (headless, optional):** in the session scratchpad,
   compile a tiny driver against `build/libblaze4k_core.a` that runs
   `GameConfig c; c.gameplay.speed_mod="6x"; auto m=options_menu_from_config(c); m.row=1;`. Apply
   `options_menu_adjust(m,+1)` three times, printing `options_row_value_text(m,1)` each time. The expected
   output is `7x`, `8x`, `1x`. Then `options_menu_apply(m,c)`, `save_config`/`load_config` to a scratch path, and
   `gameplay_options_from_config(loaded).speed.value` should give `1`. Don't write into the repo or user data dir.
3. **Manual (owner, interactive; the agent doesn't run it):** launch `./build/blaze-4k`, open
   Options on Select, then on SPEED TYPE=XMOD step SPEED right past 6x and check it reads 7x, 8x, 1x. Step
   left from 1x and check it reads 8x. Pick 8x, play a song, and confirm notes are spaced 8× wider than at 1x.
   Quit cleanly, relaunch, reopen Options, and confirm SPEED shows 8x.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Wrap is a behaviour change: existing tests assert the 6x ceiling and 1x floor (`options_menu_test.cpp:126-131`) | Replace those assertions deliberately in Task 3, because the AC requires the wrap | In scope |
| Players used to Left-mashing to "bottom out" at 1x now overshoot to 8x | Matches the existing speed-type row wrap. The value is shown live, so a single step back fixes it | In scope (accepted) |
| 7x isn't an OpenITG menu value, which conflicts with the "faithful, not novel" principle | Document it as an owner-requested deviation in the provenance comment and the Value Provenance table. Gameplay semantics (beat spacing × multiplier) are unchanged | In scope (documented) |
| At 8x on high-BPM charts, notes may appear only a short time before the receptor | Same per-frame visibility culling as other speeds (`note_field.cpp` `compute_visible`). Readability is the player's choice | Out of scope |
| Off-grid seeded values (e.g. `"10x"`) now wrap after snapping | Pinned Semantics §3 plus a test. Seeding never rewrites the value until it's adjusted | In scope |
| Changing C/M rows by accident (sharing the stepping code) | The wrap goes only inside the `XMod` branch. The C/M clamp tests stay as they are | In scope |

---

## Decisions

- **Wrap only the X value row, both directions.** The AC requires 8x to 1x. Wrapping backward too keeps
  it symmetric with the speed-type row (`options_menu.cpp:179`), which wraps both ways.
- **Whole-number steps only (7x, 8x).** This follows the issue's technical-note assumption.
- **No new test targets.** The behaviour lives in existing pure modules, so I'm extending the current tests.

## Open Questions

- **Wrap vs. clamp.** The AC says "wrapping from 8x back to 1x works", but the current X row
  *clamps* (6x + Right stays at 6x), so the AC implies new behaviour rather than describing what exists.
  *Proposed default:* implement the wrap in both directions for X only. If the owner meant only that
  the cycle reaches 8x, revert Task 1 step 2 and keep the ceiling/floor tests at 8x/1x.
- **Backward wrap (1x + Left gives 8x).** The AC only names the forward direction. *Proposed default:* wrap
  both ways for symmetry with the type row.
- **Half steps (6.5x, 7.5x).** These are not added, per the issue's stated assumption. *Default:* no.
- **`TODO.md:36` checkbox.** Tick it on completion only if the repo convention does so. Otherwise leave it.

---

## Acceptance Criteria

- [ ] The options menu X-mod cycle includes 7x and 8x after 6x
- [ ] Selecting 7x/8x is saved to config (`gameplay.speed_mod`) and restored on next launch
- [ ] Notes scroll at the selected multiplier in gameplay, and wrapping from 8x back to 1x works
- [ ] Options-menu unit tests cover the new values
- [ ] All tasks completed; the build has no new warnings in touched files
- [ ] 37/37 tests pass (start green, stay green)
- [ ] Follows existing patterns (data-driven grid, pure options model, `TEST_CHECK` idiom)
