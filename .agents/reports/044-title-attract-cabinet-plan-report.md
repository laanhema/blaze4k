# Implementation Report

**Plan**: `.agents/plans/completed/044-title-attract-cabinet-plan.md`
**Branch**: `feature/044-title-attract-cabinet`
**Status**: COMPLETE (the owner's visual check, E2E step 4 / AC4, is still to do)

## Summary

The Title and Attract screens now use the Cabinet v3 look (#92). Both build on the theme services from #89 (`ThemeTextures`), #90 (`TextRenderer`) and #91 (`theme::layout_scale`).

**Title** draws these layers, back to front:

- the `bg_title` background, stretched to the window;
- the `logo` and `subtitle` sprites, centred;
- four Cel tap notes. Each column is rotated and has a fixed colour: 4th red, 8th blue, 12th purple, 16th yellow. All use beat 0.0;
- the `press_start` plate, blinking on 0.5 s / off 0.5 s, the same rule as before;
- the footer text, in `text::kFooter`: "SINGLE · 4 PANEL" on the left and "BLAZE 4K v0.1.0" on the right;
- the `scanlines` overlay.

**Attract** draws the same background, plus:

- the logo, with its existing brightness pulse;
- Cel receptors with the existing 4 Hz blink. The lit receptor uses beat 0.0 brightness and a 1.15x zoom;
- the PRESS START plate, with the pulse applied as alpha;
- the scanlines overlay.

It has no subtitle and no footer.

**Shared code.** Both screens use the new `src/screens/title_art.{hpp,cpp}`. It holds pure layout and blink helpers that need no GL, and thin draw helpers. One of the draw helpers is guarded so it never draws an un-uploaded skin texture (the renderer would otherwise draw a solid white quad).

**Noteskin and version.**

- `App` now owns one `NoteSkin`. It is loaded in `init()` only when there is a window (headless skips it) and released before the GL context is destroyed.
- Screens reach it through the new `ScreenContext::noteskin` pointer.
- The version comes from a new PUBLIC compile definition, `BLAZE4K_VERSION="${PROJECT_VERSION}"`. `title_art.cpp` stops the build with `#error` if it is missing.

Owner decisions applied (the plan's defaults):

- `bg_title` is stretched to the window;
- Attract shows the background, the logo, the pulsing PRESS START plate, the scanlines and the receptor blink;
- the tap notes use fixed beat 0.0;
- the version shown is the real one (0.1.0), with no version bump;
- the hard on/off blink stays;
- `App` owns the shared `NoteSkin`;
- the TODO.md #55 font item is ticked.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Add the `BLAZE4K_VERSION` define and the `title_art.cpp` source to the build | `CMakeLists.txt` | ✅ |
| 2 | Add `ScreenContext::noteskin`, the App-owned `NoteSkin` (init/shutdown) and the wiring in `main` | `src/screens/screen.hpp`, `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp` | ✅ |
| 3 | Create the `title_art` helper module | `src/screens/title_art.hpp`, `src/screens/title_art.cpp` | ✅ |
| 4 | Draw the Title screen in the Cabinet style | `src/screens/title_screen.cpp`, `src/screens/title_screen.hpp` | ✅ |
| 5 | Draw the Attract screen in the Cabinet style | `src/screens/attract_screen.cpp`, `src/screens/attract_screen.hpp` | ✅ |
| 6 | Add and register `title_screen_test` | `tests/title_screen_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| – | Tick the #55 font item (owner decision) | `TODO.md` (gitignored) | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j8`, including a forced rebuild of every touched source) | ✅ exit 0, with no compiler warnings or errors from project sources |
| Lint (the `-Wall -Wextra -Wpedantic` build is the lint gate) | ✅ no new warnings |
| Tests (sandboxed `bwrap … ctest --test-dir build --output-on-failure`) | ✅ 100% of 47 passed (46 before, plus `title_screen_test`) |
| `git status --short` | ✅ only files from the plan, plus the owner's untouched `todo-stories.md` and the plan file |
| `todo-stories.md` | ✅ not modified or staged by this work. Its diff is the owner's edit from before this branch |
| `grep bitmap_font` in the title and attract screens | ✅ no match |
| `grep BLAZE4K_VERSION` | ✅ found in the CMake define (`CMakeLists.txt:148`) and in the `#error` guard and its use in `title_art.cpp` |
| E2E 1: automated (ctest) | ✅ |
| E2E 2: headless smoke (`bwrap … ./build/blaze-4k --headless --smoke-test 5 --data-dir <tmp>`) | ✅ prints `[TitleScreen] logo + "Press Start"`, `[ScreenManager] enter Title` and `Blaze 4k shut down cleanly.`, exits 0, and prints no NoteSkin line. The ALSA lines come from the sandbox hiding `/dev/snd` |
| E2E 3: version plumbing (`flags.make`) | ✅ `BLAZE4K_VERSION=\"0.1.0\"` |
| E2E 4: visual check (AC4) | ⏳ The owner must do this. The agent may not launch the GUI |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `CMakeLists.txt` | UPDATE | +4/-0 |
| `src/app/app.cpp` | UPDATE | +10/-2 |
| `src/app/app.hpp` | UPDATE | +5/-0 |
| `src/main.cpp` | UPDATE | +1/-0 |
| `src/screens/screen.hpp` | UPDATE | +6/-0 |
| `src/screens/title_art.hpp` | CREATE | +84 |
| `src/screens/title_art.cpp` | CREATE | +104 |
| `src/screens/title_screen.cpp` | UPDATE | +39/-35 |
| `src/screens/title_screen.hpp` | UPDATE | +2/-1 |
| `src/screens/attract_screen.cpp` | UPDATE | +31/-43 |
| `src/screens/attract_screen.hpp` | UPDATE | +2/-1 |
| `tests/title_screen_test.cpp` | CREATE | +322 |
| `tests/CMakeLists.txt` | UPDATE | +18/-0 |
| `TODO.md` | UPDATE (gitignored, local only) | +1/-1 |

## Deviations from Plan

1. **Smoke test idle timeout.** The plan builds the smoke test's `ScreenManager` with `attract_timeout = 0.5` from the start. That makes the 0.6 s step meant to render the PRESS START "off" phase idle straight into Attract. The test therefore:
   - builds the manager with the idle timeout disabled (`0.0`);
   - renders both blink phases;
   - calls `set_idle_timeout_seconds(0.5)` before checking the idle → Attract edge.

   The transitions checked are the same as in the plan.
2. **`TODO.md` is gitignored.** The tick on line 16 is a local edit only and does not show in `git status`.
3. **Extra test coverage (no change in behaviour).** The footer-top formula is also checked at 1920x1200, and `centred_sprite_pos` has a half-pixel rounding case.

Everything else matches the plan.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/title_screen_test.cpp` | Each case below is a function in this file |

- **`test_arrow_layout`**
  - Arrow centres and box size at 1280x720 and 2560x1440 (exact), and at 3440x1440 and 1920x1200 (approximate).
  - The quantization for each column, and clamping for columns -1 and 4.
- **`test_centred_sprite_pos`**
  - The manifest content sizes for logo, subtitle and press_start.
  - All three sprite positions at the four window sizes.
  - Half-pixel rounding.
- **`test_prompt_blink`**
  - The true/false table for the blink.
  - At 60 Hz: 10 rising edges in 10.5 s, each 60 ± 1 steps apart.
- **`test_attract_helpers`**
  - The pulse stays within [0.30, 1.00].
  - The receptor sequence is 0, 1, 2, 3, 0.
- **`test_footer_text`**
  - The version comes from `BLAZE4K_VERSION` and matches `\d+\.\d+\.\d+`.
  - The exact UTF-8 bytes of the left string.
  - Saira Bold has every glyph in both strings (`covers_text`).
  - The footer top position at 720p and at 16:10.
- **`test_skin_sprite_guard`**
  - An uninitialised `NoteSkin`'s heads and receptors, and a default `SkinSprite`, are never drawn.
- **`test_render_smoke_with_services`**
  - Title and Attract render with the real headless theme and text services, an uninitialised skin and an uninitialised renderer, at five window sizes, in every blink phase and with every receptor lit.
  - The transitions still work: Title + Confirm → Select, Back → Title, idle → Attract, Attract + Confirm → Title.
  - Both screens still render with no services attached.
