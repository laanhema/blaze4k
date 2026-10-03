# Code Review: feature/044-title-attract-cabinet (#92)

**Scope**: Branch `feature/044-title-attract-cabinet` vs `main` (`b8e1212`), uncommitted and untracked changes included: `CMakeLists.txt`, `src/app/app.{hpp,cpp}`, `src/main.cpp`, `src/screens/{screen.hpp,title_screen.*,attract_screen.*}`, the new `src/screens/title_art.{hpp,cpp}`, the new `tests/title_screen_test.cpp` and `tests/CMakeLists.txt`. `.agents/stories/todo-stories.md` has unrelated owner edits and was left out.
**Recommendation**: APPROVE WITH NITS

## Summary

The Title and Attract screens now draw the Cabinet v3 art: `bg_title`, logo, subtitle, four Cel tap notes, the PRESS START plate, the footer and scanlines. The layout and blink math lives in a small GL-free `title_art` module, which the tests pin at 720p, 1440p, 21:9 and 16:10. App owns one `NoteSkin`, and screens reach it through `ScreenContext::noteskin`.

Every coordinate checks out against `theme::layout` and the manifest's `layout_720p` boxes. The blink and timeout behaviour is the same as before. GL resource lifetime is handled correctly: the skin is released before the context, and an invalid skin texture is never drawn. I found two Low nits and nothing that blocks a merge.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/screens/title_art.cpp:88`: `draw_skin_sprite` copies the gameplay `draw_sprite` helper (`src/gameplay/note_field_renderer.cpp:24`).**
   Both do the same steps: size = `box * scale`, set the blend mode, draw a rotated textured quad, reset to Alpha. The only difference is that the new one also checks `valid()`.
   *Recommendation:* add one shared helper next to `SkinSprite` (e.g. `draw_skin_sprite` in `gameplay/noteskin.hpp` or a small `render/` helper) with the `valid()` check, and use it from both places. Gameplay would then also get the white-quad protection. This fits naturally with the follow-up that lets `GameplayView` borrow App's skin (already flagged in the plan's Risks).

2. **`src/screens/attract_screen.cpp:48`: if the Cel skin falls back to the procedural skin, the lit receptor only changes size.**
   With Cel, `receptor(column, 0.0 / 0.5)` gives a 1.0 vs 0.55 brightness tint. The procedural `receptor()` ignores `beat` and always returns `procedural_receptor_tint(column)`, so the 4 Hz blink is just a 1.15x zoom. The old code also switched the lit receptor to a yellow colour.
   *Recommendation:* ignore this if the procedural skin only matters as a crash-safe fallback. Otherwise, scale the sprite tint when `!using_cel()`, e.g. multiply the RGB by 0.55 for unlit receptors, so the blink stays readable.

**Noted, not a finding** (the plan scoped these out or the owner accepted them):
- Cel textures are loaded twice, once by App and once by `GameplayView::skin_`.
- On non-16:9 windows, `bg_title` is stretched to the window, so the baked streaks and footer fade do not line up exactly with the arrows and footer text in the content column.
- `App` now includes `gameplay/noteskin.hpp` (a layering trade-off).
- AC4, the owner's side-by-side visual check against `docs/cabinet-theme/reference/cabinet-v3-title.png` at 1280x720 and 2560x1440, is still to do. The agent did not launch the GUI.
- The footer's vertical position may need a small constant adjustment after the owner's visual check.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j8`, GCC 16.2.1, Release) | PASS |
| Warnings gate: changed TUs (`title_art.cpp`, `title_screen.cpp`, `attract_screen.cpp`, `app.cpp`, `main.cpp`, `tests/title_screen_test.cpp`) recompiled to `/dev/null` with the targets' own `flags.make` (`-std=c++20 -Wall -Wextra -Wpedantic`) | PASS (0 warnings) |
| `BLAZE4K_VERSION` plumbing (`build/CMakeFiles/blaze4k_core.dir/flags.make`) | PASS (`-DBLAZE4K_VERSION=\"0.1.0\"`) |
| Lint | N/A (the project has no linter configured) |
| Tests: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` | PASS 47/47, including the new `title_screen_test` (7 cases) |
| Sandbox check | `/dev/snd` and `/run/user/<uid>` were confirmed empty inside the sandbox. `audio_test` and the smoke run used miniaudio's Null backend (`NULL Playback Device`). That is a device fallback that still runs every assertion, not a skip |
| Skipped or env-guarded tests | None |
| Headless smoke (`blaze-4k --headless --smoke-test 5 --data-dir <scratch>`, sandboxed) | PASS: `[ScreenManager] enter Title`, `Blaze 4k shut down cleanly.` |

## What's Good

- **Layout math is tested on its own.** It sits in pure functions, and the tests pin exact pixel positions at 720p and 1440p plus the 21:9 and 16:10 letterbox cases. The tests also check that the manifest's `layout_720p` sizes match the code.
- **Missing textures never show as white quads.** `skin_sprite_drawable()` catches the renderer's white-texture substitution, and a test covers it.
- **Correct GL teardown order.** `noteskin_.shutdown()` runs before `window_.shutdown()`, matching the text and theme teardown, and headless startup skips `init()` cleanly.
- **The version cannot be wrong silently.** `#error` stops a build where `BLAZE4K_VERSION` is missing, and the test checks the string with a regex and an exact comparison against CMake's value.
- **The footer's middle dot is safe.** It is a hex-escaped UTF-8 literal, and `covers_text` asserts the real Saira font has the glyph.
- **Timings are the same as before.** The blink and pulse rules were moved over unchanged. Animation still runs on the fixed `dt` and never touches the music clock. The render smoke test runs the Title → Select → Title → Attract → Title flow with real theme and text services and with null services.
- **No sprite blur at integer or 1.5x scales.** Sprite content positions are rounded to whole pixels.

## Recommendation

Merge once the owner has done the AC4 visual check at 1280x720 and 2560x1440. The two Low nits are optional and can go into the follow-up that shares App's `NoteSkin` with gameplay.
