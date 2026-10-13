# TODO Issues

**Source**: `TODO.md` · **Generated**: 2026-10-02 · **Updated**: 2026-10-13 (TODO-42 – TODO-43 added; TODO-40 – TODO-41 added; 2026-10-12: TODO-38 – TODO-39 added; TODO-37 added; 2026-10-10: TODO-35 – TODO-36 added; TODO-33 – TODO-34 added; 2026-10-07: TODO-32 added; TODO-31 added; 2026-10-06: TODO-30 added; 2026-10-05: TODO-27 – TODO-29 added; 2026-10-03: TODO-13 – TODO-14 added; TODO-15 – TODO-26 added from `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md`, see "Cabinet theme" section)
**Last Updated**: 2026-10-13

## Status

Of the 38 issues, 32 are done, 5 are pending and 1 is not planned (closed as a duplicate). No pull request is open.

| Issue | Title | Pull request | Status | Acceptance criteria ticked |
| --- | --- | --- | --- | --- |
| #55 | [TODO-1] Replace the 5x7 bitmap UI font with a TrueType font | — | not planned | 0 of 5 |
| #56 | [TODO-3] Fix mines exploding too easily | #66 | done | 5 of 5 |
| #57 | [TODO-4] Verify judgment timing windows against OpenITG | #68 | done | 4 of 4 |
| #58 | [TODO-5] Investigate audio output latency differences (Bluetooth vs wired vs OBS) | #73 | done | 4 of 4 |
| #59 | [TODO-6] Remove the white fringe around receptors | #65 | done | 1 of 4 |
| #60 | [TODO-7] Add a pause menu during gameplay | — | pending | 0 of 5 |
| #61 | [TODO-8] Extend X-mod choices up to 8x | #63 | done | 4 of 4 |
| #62 | [TODO-9] Fix hold and roll tails extending past their end time | #64 | done | 2 of 4 |
| #76 | [TODO-10] Move the life bar to a vertical bar on the left side | #79 | done | 4 of 5 |
| #77 | [TODO-11] Fix song titles with special characters not rendering in song select | #80 | done | 5 of 5 |
| #78 | [TODO-12] Define the visual direction for a cooler splash/title screen | — | done | 4 of 4 |
| #83 | [TODO-13] Remove the live grade from the bottom of the gameplay screen | #86 | done | 3 of 4 |
| #84 | [TODO-14] Show the chart name for Edit difficulty charts | #85 | done | 5 of 5 |
| #87 | [TODO-15] Import the Cabinet theme pack into the repository | #99 | done | 5 of 5 |
| #88 | [TODO-16] Add general-quad and repeat-wrap renderer primitives | #100 | done | 5 of 5 |
| #89 | [TODO-17] Load theme textures from the manifest and add bitmap digits | #101 | done | 5 of 5 |
| #90 | [TODO-18] Add TrueType text rendering with stb_truetype atlases | #102 | done | 4 of 5 |
| #91 | [TODO-19] Add a shared 720p layout-scale helper | #103 | done | 4 of 4 |
| #92 | [TODO-20] Restyle the title and attract screens with the Cabinet look | #104 | done | 4 of 5 |
| #93 | [TODO-21] Restyle the gameplay HUD and judgment pop with the Cabinet look | #106 | done | 5 of 5 |
| #94 | [TODO-22] Restyle the song select screen with the Cabinet look | #105 | done | 5 of 5 |
| #95 | [TODO-23] Restyle the score screen with the Cabinet look | #107 | done | 5 of 5 |
| #96 | [TODO-24] Restyle the options overlay with Cabinet parts | #108 | done | 4 of 4 |
| #97 | [TODO-25] Restyle the Input Remap and Calibration screens with Cabinet parts | #109 | done | 4 of 4 |
| #98 | [TODO-26] Polish the Cabinet theme and verify it cross-platform | — | pending | 0 of 4 |
| #110 | [TODO-27] Show song subtitles in song select and results | #115 | done | 5 of 5 |
| #111 | [TODO-28] Make the gameplay judgment text 50% smaller | #113 | done | 5 of 5 |
| #112 | [TODO-29] Hide the mouse cursor during gameplay | #114 | done | 4 of 5 |
| #118 | [TODO-30] Compare mine damage against OpenITG / StepMania | #120 | done | 5 of 5 |
| #122 | [TODO-31] Shift the difficulty meter number and ticks right in song select | #123 | done | 5 of 5 |
| #124 | [TODO-32] Render symbol characters such as ☺ in song artist names | #125 | done | 5 of 5 |
| #126 | [TODO-33] Add left margin to the difficulty name text in song select | #128 | done | 5 of 5 |
| #127 | [TODO-34] Add left margin to the stat panel labels on the results screen | #129 | done | 4 of 5 |
| #130 | [TODO-35] Make the Edit difficulty row match the other rows' size and position in song select | #132 | done | 5 of 5 |
| #131 | [TODO-36] Add right margin to the best % text in the song select difficulty rows | #133 | done | 5 of 5 |
| #134 | [TODO-37] Make the automated tests silent by default | — | pending | 0 of 5 |
| #135 | [TODO-38] Add one check command (warnings as errors + tests) and export compile_commands.json | — | pending | 0 of 6 |
| #136 | [TODO-39] Add a sanitizer build preset (AddressSanitizer + UndefinedBehaviorSanitizer) | — | pending | 0 of 4 |

The criteria were checked on 2026-10-12 against the code at `d11de69` and one run of the test suite (51 of 51 passed, with the sound device hidden). The game window was not opened. A box is ticked when the code or that test run shows the criterion, or when the issue's review report records it as met and the code still holds what it names; a criterion that asks for a record on the issue was checked against the issue's comments. An unticked box of a done issue has a note. An As Built section names the file or the constant that holds the result. A pending or not planned issue was not audited. The delivery records are under `.agents/plans/completed/`, `.agents/reports/` and `.agents/reviews/`.

### Open Items Without an Issue

**Specified in the PRD but not built**

- Gameplay autoplay as the Attract demo (PRD §7.3)

**Open GitHub issues that no entry names**

- #69 Expose the active audio output device (backend, ID, name, buffer) and handle reroutes
- #70 Per-output-device global offset (remember calibration per headphones/speakers)

## Skipped

- **Done (`[x]`)** — 28 items: `TODO.md` lines 1–15, 18–30 (font capitals bug, song-select audio, difficulty order, tab legend, key-repeat scrolling, song list room, attract timeout, legend overlap, song list rendering, results delay, disappearing holds, ITG arrow colors ×2, Cel noteskin, hold-end artifact, colored difficulties, receptor/hit effects, options SFX, assist-tick toggle, calibrate → Esc, auto `songs/` folder, remap background music, white high-score flash, best % in song select, assist-tick timing, options room, remap table layout, bigger receptors).
- **Already tracked** — none on the first run. Second run: `TODO.md:16` (#55, TODO-1), `TODO.md:35` (#60, TODO-7); lines 31–34, 36, 37 are now marked done. Third run: `TODO.md:16` (#55), `:35` (#60), `:40` (#78); lines 36–39 are now marked done. Fifth run (2026-10-05): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–42 are marked done. Sixth run (2026-10-06): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–45 are marked done. Seventh run (2026-10-07): `TODO.md:17` (#98), `:35` (#60), `:46` (#118, TODO-30); lines 1–16, 18–34, 36–45 are marked done. Eighth run (2026-10-07): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–47 are marked done. Ninth run (2026-10-10): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–48 are marked done. Tenth run (2026-10-10): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–50 are marked done. Eleventh run (2026-10-12): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–52 are marked done. Twelfth run (2026-10-12): `TODO.md:17` (#98), `:35` (#60), `:53` (#134, TODO-37); lines 1–16, 18–34, 36–52 are marked done.
- **Deferred by user** — `TODO.md:17` "Something Blaze themed visuals would be cool" (too vague for now; ID TODO-2 left unused). Still deferred on the second and third runs. Fourth run: covered by the Cabinet theme issues (#87–#98).
- **Non-tasks** — none.
- **2026-10-13 run** — already tracked: `TODO.md:53` (#134, TODO-37), `TODO.md:54` (#135, TODO-38), `TODO.md:55` (#136, TODO-39). Only the two new lines 56–57 became issues.
- **2026-10-13 run, second** — already tracked: `TODO.md:55` (#136, TODO-39), `:56` (#141, TODO-40), `:57` (#142, TODO-41). Only the two new lines 58–59 became issues.

---

## [TODO-1] Replace the 5x7 bitmap UI font with a TrueType font

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `screens`, `render`
**Source**: `TODO.md:16` — "Better general font at some point?"
**GitHub**: #55 (closed 2026-10-03 as a duplicate of #90, TODO-18)
**Status**: not planned — issue closed as a duplicate of #90

### Description

All UI text is drawn with the hand-built 5x7 bitmap font, which looks blocky at larger sizes. Replace it with a texture-atlased TrueType font rendered via stb_truetype, as the PRD's tech stack already intends.

### Acceptance Criteria

- [ ] A redistributable (OFL/Apache/etc.) TTF font is added under `assets/fonts/` with its license file
- [ ] Text on title, select, options, gameplay HUD, and results screens renders from the TTF glyph atlas
- [ ] `text_width` / `draw_text_centered` measurements match the rendered glyphs so existing centered and right-aligned layouts don't overlap or clip
- [ ] Upper- and lower-case letters, digits, and punctuation used in the UI all render (no regression of the earlier capitals-only bug)
- [ ] A missing/corrupt font file falls back to the bitmap font with a log line instead of crashing

### Technical Notes

- Current font API: `src/render/bitmap_font.hpp` (`text_width`, `draw_text`, `draw_text_centered`, scaled by `pixel`). Keeping this API shape (or a thin wrapper over it) limits churn in `src/screens/*` and `src/gameplay/hud_renderer.cpp`.
- stb is already fetched (`CMakeLists.txt` section "5. stb (stb_truetype, etc.)"); bake one or two atlas sizes at startup and upload through `src/render/texture.cpp`.
- Callers pass a `pixel` scale designed for 5x7 cells; map it to a TTF pixel height so layouts keep roughly the same size.
- Assumption: font choice is open.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-3] Fix mines exploding too easily

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `gameplay`, `timing`
**Source**: `TODO.md:31` — "The mines get exploded way too easily. Figure out why this is."
**GitHub**: #56
**Status**: done — issue closed, PR #66 merged

### Description

Current: mines register as hit far more often than in In The Groove, even when the player isn't deliberately stepping on them. Expected: mines explode only under OpenITG's rules — a step within the mine window when the mine is the closest note, or the column being held at the moment the mine crosses the receptor.

### Acceptance Criteria

- [x] Root cause is identified and documented on the issue
- [x] A step aimed at a tap whose column also has a nearby mine judges the tap, matching OpenITG note-selection behavior
- [x] A held column only explodes a mine at the moment it crosses, not one already in the past (e.g. after a frame hitch)
- [x] Unit tests in `tests/` cover step-on-mine, hold-through-mine, and tap-next-to-mine cases
- [x] The mine window value is confirmed against OpenITG source

### Technical Notes

- Step path: `src/gameplay/judgment_engine.cpp:75-94` picks the closest unjudged note **including mines**, then `handle_step_mine` explodes it if within `windows.hit_mine` (0.070 s). Check how OpenITG `Player::Step` chooses between mines and taps.
- Crossing path: `JudgmentEngine::cross_mines` (`judgment_engine.cpp:325`) explodes any not-yet-complete mine with `time <= music_time` while the column is held. Mines stay incomplete until expiry, so a late-held key can still explode a mine that already passed. OpenITG only checks mines crossed since the last update.
- `held_columns` is polled from `ctx.action_down` in `src/screens/gameplay_screen.cpp:58-62`; check that a missed release (focus loss, remap) can't leave a column stuck held.
- Follow AGENTS.md: check OpenITG source before changing semantics.

### As Built

- Each mine is checked once, as it crosses the receptor `pad_stick` seconds behind the music (`src/gameplay/judgment_engine.cpp`).
- The mine window is `windows_seconds.hit_mine` (0.07 s) in `assets/data/judgment_constants.json`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-4] Verify judgment timing windows against OpenITG

**Type**: Spike
**GitHub Label**: spike
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `timing`
**Source**: `TODO.md:32` — "Check whether the timings for the grading of the steps is correct (fantastic, excellent, great, etc.). They should be authentic In The Groove / OpenITG timings."
**GitHub**: #57
**Status**: done — issue closed, PR #68 merged

### Description

Question: do Blaze 4k's Fantastic/Excellent/Great/Decent/Way Off windows (and the hold/roll/mine windows) match In The Groove / OpenITG exactly, both as configured values and as applied in the judgment path?

### Acceptance Criteria

- [x] Each value in `assets/data/judgment_constants.json` `windows_seconds` is checked against OpenITG source, with file:line references recorded on the issue
- [x] `judge_window_scale` / `judge_window_add` defaults are checked against OpenITG's preference defaults (e.g. any `TimingWindowAdd`)
- [x] It is confirmed whether window edges are inclusive or exclusive as in OpenITG, and that the global offset is applied once
- [x] Any mismatch has a follow-up bug filed (or is fixed directly with a unit test if trivial)

### Technical Notes

- Values: `assets/data/judgment_constants.json` (claims OpenITG `f2c129fe` metrics.ini); compiled defaults in `src/timing/judgment_constants.cpp:20` and nearby; classification in `JudgmentConstants::classify_tap`.
- Current values: Fantastic 21.5 ms, Excellent 43 ms, Great 102 ms, Decent 135 ms, Way Off 180 ms, Mine 70 ms, Hold 320 ms, Roll 350 ms.
- Related: the mine window feeds TODO-3.

### As Built

- `JudgmentConstants::effective_windows` applies `judge_window_scale` and `judge_window_add` in one place; the default `judge_window_add` is 0.0015 s.
- The MercifulBeginner mismatch was filed as #67, which is closed.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-5] Investigate audio output latency differences (Bluetooth vs wired vs OBS)

**Type**: Spike
**GitHub Label**: spike
**Priority**: Medium
**Complexity**: Large
**Phase**: Backlog
**Labels**: `audio`, `timing`
**Source**: `TODO.md:33` — "When recording gameplay with OBS the gameplay and audio are not in sync. However, when playing with bluetooth headphones the timing seems just fine. UPDATE - The optimal timing value varies between if I am using bluetooth headphones, and normal 3.5 mm jack headphones which have way less latency. Figure out why this is and try to come up with a solution."
**GitHub**: #58
**Status**: done — issue closed, PR #73 merged

### Description

Question: why does the best global offset change between Bluetooth and wired headphones, and why do OBS recordings drift out of sync? Then propose a fix so switching output devices doesn't require manual recalibration.

### Acceptance Criteria

- [x] Root cause is documented: whether the music clock reflects the decoder cursor rather than what is actually heard, and how big the per-device latency is
- [x] Explain why the OBS capture is out of sync even though the in-game feel is fine (game is calibrated for the Bluetooth path; OBS captures pre-output audio and video with no device latency)
- [x] At least one solution is evaluated, e.g. per-output-device saved offsets keyed by device name, or querying device latency from miniaudio / PipeWire, with tradeoffs written down
- [x] Follow-up implementation issues are filed for the chosen approach

### Technical Notes

- Clock source: `SoundStream::get_position_frames` (`src/audio/sound_stream.cpp:138`) uses `ma_sound_get_cursor_in_pcm_frames`, which is the engine/decoder position and excludes device and Bluetooth codec latency (often 150–300 ms for BT vs ~10–30 ms wired).
- Clock + single global offset: `src/timing/music_clock.hpp` (`time = frames / rate + global_offset`); calibration in `src/timing/offset_calibration.cpp`; persisted in `src/data/config.hpp`.
- Device enumeration/selection in `src/audio/audio_engine.cpp`; miniaudio exposes device info/IDs that could key a per-device offset table.
- AGENTS.md: the music clock stays the single timing source. Any latency compensation goes into the offset, never into wall-clock or frame-delta logic.

### As Built

- `docs/AUDIO_LATENCY.md` holds the root cause, the section "Why OBS recordings are out of sync", the options and the decision.
- The follow-ups are #69 and #70 (open) and #71 (closed).

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-6] Remove the white fringe around receptors

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `render`
**Source**: `TODO.md:34` — "The receptors have a small bits of white pixels around them, it looks unclean. Perhaps using a different blending mode for them could fix it? So it just blends with the background image when playing a song."
**GitHub**: #59
**Status**: done — issue closed, PR #65 merged. Of 4 criteria, 3 not checked.

### Description

Current: Cel receptors show stray white/light pixels around their edges against the song background. Expected: receptor edges blend cleanly into the background with no halo.

### Acceptance Criteria

- [ ] Receptors render without a visible light fringe over dark and bright song backgrounds
  - *Not checked:* every texture upload premultiplies alpha and the blend function is `GL_ONE, GL_ONE_MINUS_SRC_ALPHA` (`src/render/texture.cpp`, `src/render/gl_quad_renderer.cpp`); the window was not opened for this update.
- [ ] Tap notes, holds, mines, and explosions don't gain or keep a similar fringe after the fix
  - *Not checked:* the same premultiplied path draws taps, holds, mines and explosions; the window was not opened for this update.
- [ ] Receptor flash/brightness behavior (beat pulse, press glow) is unchanged
  - *Not checked:* the window was not opened for this update.
- [x] Build passes and existing render tests pass

### Technical Notes

- Likely cause: straight (non-premultiplied) alpha with `GL_LINEAR` / mipmap filtering (`src/render/texture.cpp:98-100`). Fully transparent texels with white RGB bleed into edge samples. Usual fixes: premultiply alpha at load time and blend with `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`, or bleed edge colors into transparent texels.
- Changing only the receptor blend mode (`BlendMode` in `src/render/geometry.hpp`, applied in `src/gameplay/note_field_renderer.cpp:30`) would likely hide the halo but alter the look. Check the texture first.
- Receptor art: `assets/noteskins/cel/_Down Receptor tex 4x1 (res 256x64).png`, loaded in `src/gameplay/noteskin.cpp:24`.

### As Built

- The render pipeline uses premultiplied alpha: uploads premultiply, and additive draws use `GL_ONE, GL_ONE`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-7] Add a pause menu during gameplay

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `gameplay`, `screens`
**Source**: `TODO.md:35` — "Make a clean pause window when pausing playing a song."
**GitHub**: #60
**Status**: pending

### Description

Add a clean pause overlay during gameplay so the player can stop mid-song and choose what to do next, instead of having no pause or an abrupt exit.

### Acceptance Criteria

- [ ] Pressing Back (Esc / pad Back) during gameplay pauses music and the note field and shows a centered, dimmed overlay
- [ ] The overlay offers at least Resume, Restart song, and Quit to song select, navigable with menu keys and Confirm
- [ ] Resuming continues from the exact paused position with judgments still in sync (no notes judged during the pause, no clock jump)
- [ ] Back while paused resumes; quitting doesn't save a high score for the aborted play
- [ ] Assist ticks are silenced while paused and resume in sync

### Technical Notes

- Back is currently routed through `App::process_events` (`src/app/app.cpp:136`). Gameplay must consume it (`event_cb_`) so the app doesn't exit.
- `SoundStream::pause()` already keeps the stream position (`src/gameplay/gameplay_view.cpp:220-226` uses it for fail). Since the clock reads the stream cursor, it freezes on its own. Also stop `assist_player_` and freeze judgment/animation updates.
- Screen code: `src/screens/gameplay_screen.cpp`, `src/gameplay/gameplay_view.cpp`. The overlay can follow the options overlay style (`src/screens/options_menu.cpp`).
- Use the UI sounds from `src/audio/ui_sounds.cpp` for open/close/navigate.
- Assumption: "pause window" means an in-game overlay menu. Practice-mode features (seek/rewind) stay out of scope per the PRD.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-8] Extend X-mod choices up to 8x

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `screens`
**Source**: `TODO.md:36` — "Need more XMod values, up until 8 I think."
**GitHub**: #61
**Status**: done — issue closed, PR #63 merged

### Description

The options menu's X-mod list stops at 6x. Extend it up to 8x so fast readers can pick higher multipliers.

### Acceptance Criteria

- [x] The options menu X-mod cycle includes 7x and 8x after 6x
- [x] Selecting 7x/8x is saved to config (`gameplay.speed_mod`) and restored on next launch
- [x] Notes scroll at the selected multiplier in gameplay, and wrapping from 8x back to 1x works
- [x] Options-menu unit tests cover the new values

### Technical Notes

- Change `kXModValues` in `src/screens/options_menu.cpp:40` (currently `{1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0}`).
- `parse_speed_mod` (`src/gameplay/speed_mod.cpp`) accepts any finite positive value, so no parser change is expected.
- Assumption: add whole-number steps (7x, 8x). Half steps such as 3.5x are not added unless requested.

### As Built

- The X-mod choices are `kXModValues` in `src/screens/options_menu.cpp`: 1, 1.5, 2, 2.5, 3, 4, 5, 6, 7 and 8.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-9] Fix hold and roll tails extending past their end time

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `render`, `gameplay`
**Source**: `TODO.md:37` — "Hold-arrows (and most likely rolls aswell) are a little bit too long. They have this tail end that extends beyond the moment they are meant to be let go."
**GitHub**: #62
**Status**: done — issue closed, PR #64 merged. Of 4 criteria, 2 not checked.

### Description

Current: the end cap of holds and rolls is drawn past the tail position, so the visual end of the hold reaches the receptor after the moment it can be released. Expected: the drawn end of the hold lines up with the release time, as in OpenITG with the Cel noteskin.

### Acceptance Criteria

- [x] The visible end of a hold/roll (body + cap) reaches the receptor at the note's tail time, in both up and down scroll
- [x] Rolls get the same fix
- [ ] Hold bodies don't develop a gap or seam between the body and the cap
  - *Not checked:* the window was not opened for this update.
- [ ] Verified visually against the metronome sync-test chart (or a hold test chart) at 1x and a high X-mod
  - *Not checked:* no screen check was made for this update; `note_field_renderer_test` passed on 2026-10-12.

### Technical Notes

- `draw_hold` in `src/gameplay/note_field_renderer.cpp:37-80` draws the body from head to `tail_y` and then the cap starting **at** `tail_y` and extending a full `width` beyond it (`cap_top = reverse ? tail_y - width : tail_y`). That adds about one note size of length past the tail.
- Check how OpenITG/StepMania `NoteDisplay::DrawHoldBottomCap` positions the cap relative to the tail (and the Cel noteskin's metrics), and match it, e.g. end the body earlier or center/offset the cap.
- Tail y comes from `field.screen_y(item.tail_offset)` (`note_field_renderer.cpp:118`).

### As Built

- The hold body stops `HoldSprites::tail_inset_scale` note sizes before the tail, and the cap starts there; for Cel the value is `kCelHoldBodyStopFromTail = 0.5` (`src/gameplay/noteskin.hpp`).

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-10] Move the life bar to a vertical bar on the left side

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `render`
**Source**: `TODO.md:38` — "Change the position of the lifebar from bottom of the screen (kind of in way of the incoming arrows) to be instead on left hand side and rotated to be in vertical orientation."
**GitHub**: #76
**Status**: done — issue closed, PR #79 merged. Of 5 criteria, 1 not met.

### Description

The life bar is drawn as a horizontal bar at the bottom center of the screen, where it gets in the way of the incoming arrows. Move it to the left side of the screen and draw it vertically so it stays out of the note field.

### Acceptance Criteria

- [x] During gameplay the life bar is drawn vertically on the left side of the screen and fills from bottom (empty) to top (full)
- [x] The bar doesn't overlap the note field, receptors, combo, or judgment text at common window sizes (e.g. 1280x720, 1920x1080, and a narrow 4:3 window)
- [x] Fill level, the danger color below 30% life, and the frame/background styling still work as before
- [ ] The bottom-center grade/score text keeps its current position and isn't clipped or overlapped
  - *Not met:* the bottom-centre grade and score text are no longer drawn during gameplay (#83, #93), so there is nothing to keep in place.
- [x] Build passes and existing gameplay/HUD tests pass

### Technical Notes

- All life bar drawing is in `HudRenderer::render_life` (`src/gameplay/hud_renderer.cpp:165`). It currently uses `bar_w = min(width * 0.40, 480)`, `bar_h = 16` and sits just above the grade text. Swap the axes, anchor the bar to a left margin, and size its height from the screen height.
- The note field is centered (`field_left = (screen_w - field.field_width()) * 0.5` in `src/gameplay/note_field_renderer.cpp:144`), so a left-margin bar clears it unless the window is very narrow. Clamp the bar to stay left of `field_left`.
- Called from `src/gameplay/gameplay_view.cpp:327`. Palette constants live at `hud_renderer.cpp:24-30`.
- Assumption: "left hand side" means the left edge of the screen, not right next to the note field. Scroll direction (reverse) doesn't change where the bar sits.

### As Built

- The geometry is `layout_life_bar` in `src/gameplay/hud_renderer.hpp`; the bar now uses the Cabinet `life_frame` art (#93).
- The danger colour starts below `kLifeDangerThreshold`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-11] Fix song titles with special characters not rendering in song select

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `render`, `screens`
**Source**: `TODO.md:39` — "If songs name has special symbols the text is not rendered in song select view (bug)"
**GitHub**: #77
**Status**: done — issue closed, PR #80 merged

### Description

Current: the bitmap font only has glyphs for `0-9`, `A-Z`, `a-z`, space, and `. % - + * [ ]`, so any other character in a song title or artist is silently skipped. ASCII punctuation (`'`, `!`, `?`, `&`, `(`, `)`, `:`, `/`, `,`, …) disappears, and non-ASCII UTF-8 titles (accents, Japanese) render as gaps or nothing. `text_width` also counts bytes, not characters, so layouts with multi-byte titles are off. Expected: song titles and artists show readably in song select, including common punctuation, with a sensible fallback for characters the font can't draw.

### Acceptance Criteria

- [x] Common printable ASCII punctuation (at least `' ! ? & ( ) : / , " # _ = ~`) renders in song select titles and artists, e.g. "Don't Promise Me" shows its apostrophe
- [x] UTF-8 text is decoded per code point: characters the font can't draw show a visible placeholder (or a transliterated/ASCII-folded form), and a multi-byte character takes up one cell, not one per byte
- [x] When the simfile has `#TITLETRANSLIT` / `#ARTISTTRANSLIT` and the original title has characters the font can't draw, the transliterated text is shown instead
- [x] Malformed UTF-8 in a simfile title doesn't crash or hang rendering
- [x] Unit tests cover glyph lookup for punctuation, `text_width` on multi-byte input, and the translit fallback

### Technical Notes

- Glyph table and lookup: `kGlyphs` / `glyph_for` in `src/render/bitmap_font.cpp:17-97`. `draw_text` iterates `char`s and skips unknown glyphs; `text_width` returns `text.size() * 6 * pixel`.
- Song select draws titles at `src/screens/select_screen.cpp:612` (info panel) and `:644` (song list). The fix in `bitmap_font.cpp` also covers the results and gameplay screens.
- Translit fields are already parsed (`src/chart/simfile_parser.cpp:82-87`, `src/chart/song_metadata.hpp:11-13`) but not used for display.
- Related: #55 (TODO-1) replaces the bitmap font with a TrueType atlas. Glyph coverage there decides how much non-ASCII text can render natively. This bug can be fixed now in the bitmap font, but keep the UTF-8 decoding and fallback logic font-agnostic so it carries over.
- Simfile text is untrusted input (AGENTS.md), so UTF-8 decoding must be bounds-checked.

### As Built

- UTF-8 is decoded by `src/render/unicode_text.cpp`, and `src/screens/song_display_text.hpp` picks the transliterated title or artist when the font cannot draw the native text.
- UI text is now drawn with TrueType fonts (#90); the bitmap font remains as the fallback.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-12] Define the visual direction for a cooler splash/title screen

**Type**: Spike
**GitHub Label**: spike
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `render`
**Source**: `TODO.md:40` — "Cooler splash screen."
**GitHub**: #78 (closed 2026-10-03: direction = Cabinet theme, follow-ups #87–#98)
**Status**: done — issue closed, no pull request found

### Description

Question: what should the Blaze 4k splash/title screen look like? Right now it is just "BLAZE 4K" in the 5x7 bitmap font with a row of four flat colored squares and "PRESS START". Pick a concrete visual direction and split the work into follow-up issues.

### Acceptance Criteria

- [x] A visual direction is chosen and recorded on the issue (e.g. logo art or a styled logotype, background, motion/animation, color palette), with a mockup or reference images
- [x] Asset needs are listed with their sources and licenses (original art, or OFL/CC-licensed fonts and images that can be redistributed)
- [x] The direction stays within PRD scope (2D textured quads only; no video or 3D backgrounds)
- [x] Follow-up implementation issues are filed and linked from this issue

### Technical Notes

- Current screen: `src/screens/title_screen.cpp` (logo text at `:52`, receptor-colored squares at `:54-66`, "PRESS START" at `:68`). The attract screen (`src/screens/attract_screen.cpp`) shares the Title → Attract loop and should match the new look.
- Rendering is limited to textured quads through `GlQuadRenderer` (`src/render/gl_quad_renderer.hpp`). Logo art would load like other textures via `src/render/texture.cpp` / `texture_cache.cpp` from a new `assets/` subfolder.
- Related: #55 (TODO-1, TrueType font) affects how the logotype and "PRESS START" can be styled. `TODO.md:17` ("Blaze themed visuals", deferred) may share the same visual direction.
- Assumption: "splash screen" means the title screen ("BLAZE 4K" / "PRESS START"), not a separate startup/loading splash.

### As Built

- The chosen direction is the Cabinet theme; its mock-ups are under `docs/cabinet-theme/reference`.
- The follow-ups are #87 to #98; #98 is still open.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-13] Remove the live grade from the bottom of the gameplay screen

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `render`
**Source**: `TODO.md:41` — "Get rid of the grade located at the bottom of the screen when playing a song."
**GitHub**: #83
**Status**: done — issue closed, PR #86 merged. Of 4 criteria, 1 not met.

### Description

During a song the HUD draws the current live grade (e.g. "A-", "**") at the bottom centre of the screen. Remove it so the bottom of the playfield is free; the grade still appears on the results screen.

### Acceptance Criteria

- [x] No grade text is drawn anywhere on the gameplay screen during play
- [ ] Percent, combo, judgment-count chips, and the life bar still render where they do today
  - *Not met:* the combo and the life bar are still drawn, but the percent and the judgment-count chips were removed from the gameplay HUD later (#93).
- [x] The results screen still shows the final grade unchanged
- [x] Tests that check the bottom-centre grade layout are removed or updated, and the build and `ctest` pass

### Technical Notes

- The grade is drawn in `src/gameplay/hud_renderer.cpp:180-185` ("Bottom-centre: live grade"), positioned by `grade_text_rect` (`:50`).
- `grade_text_rect` is declared in `src/gameplay/hud_renderer.hpp:29-33` and checked by `test_grade_text_clear` in `tests/hud_renderer_test.cpp:152`. Drop both if nothing else uses them.
- Keep `format_grade` (`hud_renderer.cpp:84`) if the results screen or tests still use it. Keep `state.grade` in the HUD state if other code reads it.
- Assumption: remove the grade outright. A toggle in Options wasn't asked for.

### As Built

- No grade is drawn during gameplay; `format_grade` is used by the results screen only.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-14] Show the chart name for Edit difficulty charts

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `chart`
**Source**: `TODO.md:42` — "When a simfile has "Edit" type difficulty charts, I would prefer it would show the name of the chart. Currently it just shows "Edit" but I know these custom charts have a custom name aswell."
**GitHub**: #84
**Status**: done — issue closed, PR #85 merged

### Description

Edit charts carry a custom name in the simfile, but song select and results only show the label "Edit". A song with several edits shows them as identical rows. Show the chart's own name for Edit charts, and fall back to "Edit" when the name is empty.

### Acceptance Criteria

- [x] In song select, an Edit chart's row shows its chart name (with meter and best % as today) instead of "Edit"
- [x] The results screen difficulty line shows the same name for Edit charts
- [x] An Edit chart with an empty name still shows "Edit"; non-Edit difficulties (Beginner…Challenge) display exactly as before
- [x] A long name is shortened so it doesn't overlap the meter or best-% columns
- [x] Parser/select tests cover an `.sm` and an `.ssc` fixture with a named Edit chart

### Technical Notes

- The name is already parsed into `Chart::description` (`src/chart/chart.hpp:79`). SM: the 2nd `#NOTES` field (`src/chart/simfile_parser.cpp:170`). SSC: `#CHARTNAME` / `#DESCRIPTION` (`simfile_parser.cpp:136`). Both SSC tags currently write the same variable, so whichever appears last wins. Check StepMania 5 source (`NotesLoaderSSC.cpp`, how `Steps` description vs. chart name is shown for edits) for which field is the edit name before changing that precedence.
- Display sites: `src/screens/select_screen.cpp:675` (difficulty row) and `src/screens/results_screen.cpp:153-156` (difficulty line). A small shared helper (e.g. next to `song_display_title` in `src/screens/song_display_text.cpp`) avoids duplicating the fallback logic. Run names through the same UTF-8 text handling as song titles (#77).
- Keep `difficulty_color` keyed on `chart.difficulty` ("Edit") so edits keep their color.
- High-score keys (`src/data/high_scores.cpp:150`) already include a note-data hash, so separate Edit charts don't share scores. No change needed there.

### As Built

- The row label comes from `chart_display_label` (`src/screens/song_display_text.hpp`): the chart's name for an Edit chart, and `Edit` when the name is empty.

### Dependencies

- Blocked by: None
- Blocks: None

---

# Cabinet theme

**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md` (+ `blaze4k-cabinet-theme/README.md`) · **Generated**: 2026-10-03

Tasks come from the plan's §5 order table, with detail from §2–§3. One issue per plan step, one PR per issue. The §5 row "3.4 / 3.7 options, remap, calibration" is split into TODO-24 and TODO-25, because §3 lists them as separate steps in separate files. TODO-15 (importing the pack) is not a plan row. It's implied by README.md ("copied straight into the repo root") and every other step needs it.

---

## [TODO-15] Import the Cabinet theme pack into the repository

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Setup
**Labels**: `render`, `build`
**Source**: `blaze4k-cabinet-theme/README.md:3` — "The folder layout mirrors the repository, so the contents can be copied straight into the repo root."
**GitHub**: #87
**Status**: done — issue closed, PR #99 merged

### Description

The Cabinet theme pack sits untracked in `blaze4k-cabinet-theme/`. Copy its assets, fonts, `theme.hpp` and design docs into the repository layout, so the engine and screen issues can build on tracked files.

### Acceptance Criteria

- [x] `assets/fonts/` holds Audiowide + Saira Condensed (Medium, Bold, ExtraBold) `.ttf` files, with both OFL licence texts next to them
- [x] `assets/theme/cabinet/` holds all 63 textures, `manifest.json` and the two bitmap digit atlases, and a build copies them next to the binary with no CMake change
- [x] `src/render/theme.hpp` is in the tree and compiles as part of the build (included by at least one TU or a compile-only test)
- [x] The plan, README and `reference/` mock-ups are kept under `docs/` (e.g. `docs/cabinet-theme/`), and the untracked `blaze4k-cabinet-theme/` folder is removed
- [x] Fresh clone + `cmake -B build && cmake --build build && ctest --test-dir build` passes

### Technical Notes

- `CMakeLists.txt:161-167` already copies the whole `assets/` folder post-build.
- `theme.hpp` depends only on `render/geometry.hpp` (`Color`, `Rect`).
- No code uses the assets yet. Loading is TODO-17 (textures) and TODO-18 (fonts).
- Assumption: design docs go under `docs/cabinet-theme/`. Later issues refer to the pack by that path.
- Mention the font licences in the top-level README/LICENSE notes if the repo lists third-party assets.

### As Built

- `assets/theme/cabinet` holds 65 PNG files (63 textures and two digit atlases) and `manifest.json`; the fonts and their OFL texts are in `assets/fonts`.
- The plan, the README and the mock-ups are under `docs/cabinet-theme`.

### Dependencies

- Blocked by: None
- Blocks: TODO-17, TODO-18, TODO-19

---

## [TODO-16] Add general-quad and repeat-wrap renderer primitives

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Engine additions
**Labels**: `render`, `tests`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:156` — "2.1 renderer primitives | S | —"
**GitHub**: #88
**Status**: done — issue closed, PR #100 merged

### Description

The theme needs parallelograms (meter ticks, judgment bars, chips), per-corner gradients and sheared italic glyphs. The renderer only draws axis-aligned or rotated rects with one colour, and every texture is clamp-to-edge. Add a four-corner quad entry point and an optional repeat wrap mode for textures.

### Acceptance Criteria

- [x] `GlQuadRenderer::draw_quad_points(corners, texture, uv, colours)` draws a quad from four corners (TL, TR, BR, BL), each with its own colour, through the existing batch
- [x] `Texture::from_file(path, mipmaps, wrap)` accepts an optional `Wrap::Repeat` (default stays clamp), plus a nearest-filter option for `scanlines.png`
- [x] Existing draw calls render exactly as before (no change to `draw_quad` / `draw_textured_quad` output)
- [x] Unit tests cover corner order, per-vertex colour (premultiplied like the other paths) and UV mapping for a repeat-wrapped UV range > 1
- [x] Headless (uninitialized) renderer stays a safe no-op for the new call

### Technical Notes

- `src/render/gl_quad_renderer.hpp`: add next to `draw_textured_quad`. Reuse `append_quad`'s vertex layout (x, y, u, v, r, g, b, a) and the texture-change flush. Colours must go through the same `premultiply` path (#59).
- `src/render/texture.cpp:105-108` hard-codes `GL_LINEAR` and `GL_CLAMP_TO_EDGE`. Thread the wrap and filter choices through `from_file` / upload.
- Tests in the style of `tests/texture_test.cpp` and `tests/note_field_renderer_test.cpp`.
- Keep `BlendMode::Add` for explosions. No new shaders.

### As Built

- `GlQuadRenderer::draw_quad_points` is in `src/render/gl_quad_renderer.hpp`; `Texture` takes `Wrap::Repeat` and `Filter::Nearest` (`src/render/texture.cpp`).

### Dependencies

- Blocked by: None
- Blocks: TODO-17, TODO-18, TODO-23

---

## [TODO-17] Load theme textures from the manifest and add bitmap digits

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Engine additions
**Labels**: `render`, `tests`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:157` — "2.3 theme textures + digits | S | 2.1"
**GitHub**: #89
**Status**: done — issue closed, PR #101 merged

### Description

Screens should draw Cabinet textures by name, without hard-coding pixel offsets. Add a theme texture module that loads `manifest.json` and every PNG once at startup. It provides draw helpers for each manifest `kind` and draws numbers with the two baked digit fonts.

### Acceptance Criteria

- [x] `assets/theme/cabinet/manifest.json` is parsed with nlohmann/json, and every listed PNG is loaded once at app init (mipmaps for grades, judgments and logo; repeat wrap for `tile` kinds)
- [x] Helpers exist and position the manifest's `content_px` box, not the padded image: `draw_sprite`, `draw_slice3`, `draw_slice9`, `draw_frame`, `draw_tiled`, `draw_fill_cropped` (UV-cropped so the life gradient doesn't slide)
- [x] `BitmapDigits` draws strings from `digits_chrome` / `digits_white` (`0-9 . % /` and space) using each glyph's rect, `origin_x` and `advance`, with left, centre and right alignment
- [x] A missing or unreadable PNG or manifest logs once and falls back to a flat quad. A malformed manifest never crashes, and headless tests keep running
- [x] Unit tests cover manifest parsing, content-box placement, slice3/slice9 rect math, the cropped-fill UVs and digit layout widths

### Technical Notes

- New `src/render/theme_textures.{hpp,cpp}`. Load with `Texture::from_file` (wrap mode from TODO-16).
- Textures are @2x for 1280x720. Draw size = `size_px / 2 * s`, where `s` is the layout scale (TODO-19). Take `s` as a parameter so this issue doesn't need TODO-19.
- Load at app init (`src/app/app.cpp`, where other assets and the judgment constants load) and pass to screens through `ScreenContext`, the way other shared services reach screens.
- Fallback spirit: the noteskin's optional explosions (`src/gameplay/noteskin.cpp`).
- The manifest is local, trusted data, but treat it defensively anyway (bounds-check rects against image size).

### As Built

- The manifest loader, the sprite and slice helpers, and `BitmapDigits` are in `src/render/theme_textures.hpp`.

### Dependencies

- Blocked by: TODO-15, TODO-16
- Blocks: TODO-20, TODO-21, TODO-22, TODO-23

---

## [TODO-18] Add TrueType text rendering with stb_truetype atlases

**Type**: Feature
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Cabinet theme — Engine additions
**Labels**: `render`, `tests`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:158` — "2.2 TrueType text | M | 2.1"
**GitHub**: #90
**Status**: done — issue closed, PR #102 merged. Of 5 criteria, 1 not met.

### Description

All runtime UI text in the Cabinet design (song titles, artists, meters, hints, combo) uses Saira Condensed with tracking, synthetic italic and hard drop shadows. Add a TrueType font atlas and text renderer built on stb_truetype that implements `theme::TextStyle`, and keep today's UTF-8 safety rules.

### Acceptance Criteria

- [x] `FontAtlas` bakes printable ASCII, Latin-1 and Latin Extended-A per (font file, pixel size) with `stbtt_PackFontRanges` (2x2 oversampling), uploaded as white RGBA with coverage in alpha so tinting and premultiplied blending work
- [x] `TextRenderer::measure` / `draw` apply a `TextStyle`'s tracking, `kItalicShear` (plus an extra group shear), and `Shadow::Hard2/3` (black copy offset down, then the colour), with left, centre and right alignment
- [x] UTF-8 goes through `unicode_text` (zero-width skipped, malformed bytes → U+FFFD), and code points the font lacks draw the placeholder box. A fuzz-style test with malformed input doesn't crash
- [ ] Atlases bake at `size_px * window_height / 720` and re-bake only when the window height changes. Measure-based `...` truncation replaces `truncate_to_cells`, with its tests ported
  - *Not met:* text is measured and truncated by width, but `truncate_to_cells` is still in `src/render/bitmap_font.hpp` and is called from `src/screens/song_display_text.cpp`.
- [x] A missing or corrupt `.ttf` logs once and falls back (bitmap font or no-op) instead of crashing

### Technical Notes

- New `src/render/ttf_font.{hpp,cpp}`. stb is already fetched (`CMakeLists.txt` stb FetchContent). Add `stb_truetype` / `stb_rect_pack` implementation defines in one TU, like `src/render/stb_image_impl.cpp`.
- Fonts and styles: `theme::kFontFiles`, `theme::text::*` in `src/render/theme.hpp`. Expect about 15 (font, size) pairs. Draw italic glyphs with `draw_quad_points` (TODO-16).
- UTF-8 helpers: `src/render/unicode_text.hpp` (`next_code_point`, `is_zero_width`, `fold_to_ascii`). Current truncation contract: `truncate_to_cells` in `src/render/bitmap_font.hpp`. Tests: `tests/bitmap_font_test.cpp`, `tests/unicode_text_test.cpp`.
- Keep `bitmap_font` for debug overlays until no screen uses it. Deleting it belongs to TODO-26 or later.
- This is the implementation of #55 (TODO-1, "Replace the 5x7 bitmap UI font with a TrueType font").

### As Built

- `FontAtlas` and the text renderer are in `src/render/ttf_font.hpp`, built on `stbtt_PackFontRanges`.
- The shear is `kItalicShear` in `src/render/theme.hpp`.

### Dependencies

- Blocked by: TODO-15, TODO-16
- Blocks: TODO-20, TODO-21, TODO-22, TODO-23

---

## [TODO-19] Add a shared 720p layout-scale helper

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Engine additions
**Labels**: `render`, `screens`, `tests`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:159` — "2.4 layout scale helper | S | —"
**GitHub**: #91
**Status**: done — issue closed, PR #103 merged

### Description

Every `theme::layout` value is in a 1280x720 reference space. Add one helper that every screen uses to map reference coordinates to the window: scale `s = window_height / 720` and a `1280·s`-wide content column centred horizontally.

### Acceptance Criteria

- [x] A helper returns `s` and the content column origin for a given window size, and maps reference `Rect`s and points to screen pixels
- [x] At 1280x720 the mapping is identity. At 2560x1440 it's a 2x scale. At ultrawide sizes the column is centred, with extra background on both sides
- [x] Unit tests cover 720p, 1440p, 16:10 and 21:9 window sizes
- [x] Degenerate sizes (0 or negative width/height) return a safe result and don't divide by zero

### Technical Notes

- Put it next to `theme.hpp` (e.g. `src/render/theme_layout.hpp`), using `theme::layout::kRefWidth/kRefHeight`.
- Screens already centre the note field this way. Match that behaviour (`src/gameplay/gameplay_view.cpp`).
- Text atlas baking (TODO-18) and texture draw size (TODO-17) both use the same `s`.

### As Built

- The helper is in `src/render/theme_layout.hpp`, and `tests/theme_layout_test.cpp` covers the window sizes.

### Dependencies

- Blocked by: TODO-15
- Blocks: TODO-20, TODO-21, TODO-22, TODO-23

---

## [TODO-20] Restyle the title and attract screens with the Cabinet look

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Screens
**Labels**: `screens`, `frontend`, `render`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:160` — "3.1–3.2 title + attract | S | 2.2, 2.3"
**GitHub**: #92
**Status**: done — issue closed, PR #104 merged. Of 5 criteria, 1 not checked.

### Description

Replace the bitmap-text "BLAZE 4K" and flat coloured squares with the Cabinet title from `reference/cabinet-v3-title.png`: background, chrome logo, subtitle, four Cel tap notes, PRESS START plate, footer and scanlines. Give the attract screen the same background and logo.

### Acceptance Criteria

- [x] Title draws `bg_title`, `logo`, `subtitle`, four Cel tap notes (rotated per column) at `kTitleArrowsTop`, `press_start` blinking at 1 Hz as now, and the `scanlines` overlay
- [x] Footer shows "SINGLE · 4 PANEL" on the left and "BLAZE 4K v<version>" on the right in `text::kFooter`. The version comes from a `BLAZE4K_VERSION` compile definition set from CMake `project(VERSION …)`
- [x] Attract uses the same background and logo, and keeps its pulse and receptor-blink logic with Cel receptors
- [ ] Side-by-side check against `reference/cabinet-v3-title.png` at 1280x720 and 2560x1440 matches layout and crispness
  - *Not checked:* the layout constants are in `src/screens/title_art.hpp`; no side-by-side check with the mock-up was made for this update.
- [x] Screen transitions and timings (Confirm → Select, attract timeout) are unchanged, and existing screen tests pass

### Technical Notes

- Files: `src/screens/title_screen.cpp` (currently `draw_text_centered("BLAZE 4K")` + four `draw_quad`s), `src/screens/attract_screen.cpp`.
- Tap notes: reuse `NoteSkin::head` (`src/gameplay/noteskin.hpp`) with a fixed beat per column. The mock shows a different quantization colour per arrow (red/blue/purple/yellow). The `NoteSkin` instance today lives in `gameplay_view.cpp`, so the title needs access to one (e.g. through `ScreenContext`).
- `CMakeLists.txt:3` has `project(blaze-4k VERSION 0.1.0 …)`. Add `target_compile_definitions(... BLAZE4K_VERSION="${PROJECT_VERSION}")`.
- Answers #78 (TODO-12, splash-screen visual direction).

### As Built

- The title art is in `src/screens/title_art.cpp`; the footer version comes from the `BLAZE4K_VERSION` definition that `CMakeLists.txt` sets.

### Dependencies

- Blocked by: TODO-17, TODO-18, TODO-19
- Blocks: TODO-26

---

## [TODO-21] Restyle the gameplay HUD and judgment pop with the Cabinet look

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Cabinet theme — Screens
**Labels**: `gameplay`, `screens`, `render`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:161` — "3.5 gameplay HUD | M | 2.2, 2.3"
**GitHub**: #93
**Status**: done — issue closed, PR #106 merged

### Description

Match `reference/cabinet-v3-gameplay.png`. Remove the live percent and per-window judgment chips. Replace the life bar with the chrome frame and gradient fill, add the difficulty badge top-left, and draw judgments with the baked `judgment_*` sprites plus a sheared combo line. Leave the note field untouched.

### Acceptance Criteria

- [x] The live percent, the judgment chips and their per-window colours are no longer drawn. Scoring, combo and life still update (the score keeper is untouched)
- [x] Life bar = `life_frame` (9-slice) + `life_fill`, or `life_fill_danger` below 30% (`kLifeDangerThreshold`), cropped with stable colours + `life_stripes` tiled, keeping `layout_life_bar`'s field-clearance logic
- [x] `diff_badge` at `kDiffBadge` is tinted by `theme::difficulty`, with e.g. "HARD 8" in the difficulty's ink colour
- [x] Judgment pop draws `judgment_<kind>` with the current scale and fade curve, and the combo (`kComboNumber` + `kComboLabel` "COMBO") sits under it at `kComboTop`, with the group sheared −10°
- [x] Note field, receptors, Cel noteskin and background dim are unchanged. `hud_renderer_test` and `gameplay_screen_test` are updated for the removed elements and pass

### Technical Notes

- `src/gameplay/hud_renderer.cpp`: the percent (`percent_text_rect`, `format_percent` around :113) and the chips (`draw_chip` around :123-142) go. `kLifeBarMinInset` exists to keep the frame below the percent text. Revisit it once the percent is gone, and start from `theme::layout::kLifeBar`.
- `src/gameplay/judgment_animator.cpp:139-150`: swap `draw_text_centered(pop_label_)` for the sprite, and move the combo from the top of the screen (y = 8) to under the judgment.
- Judgment sprite names: fantastic, excellent, great, decent, wayoff, miss, ok, ng, mine (`manifest.json`).
- Difficulty label for Edit charts: show the chart name (#84) or "EDIT", using `theme::difficulty::kEdit`.
- Timing rule: all animation stays on fixed `dt`, and nothing touches the music clock.
- Draw order for batching: badge/frames → fill → stripes → judgment sprite → text.

### As Built

- The badge and the life bar are drawn by `src/gameplay/hud_renderer.cpp` (`kDiffBadge`, `life_fill_danger`), and the judgment and combo by `src/gameplay/judgment_animator.cpp` (`kComboTop`).
- The judgment sprite was made half as large later (#111).

### Dependencies

- Blocked by: TODO-17, TODO-18, TODO-19
- Blocks: TODO-26

---

## [TODO-22] Restyle the song select screen with the Cabinet look

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Large
**Phase**: Cabinet theme — Screens
**Labels**: `screens`, `frontend`, `render`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:162` — "3.3 song select | L | 2.2, 2.3"
**GitHub**: #94
**Status**: done — issue closed, PR #105 merged

### Description

Rebuild the song select presentation to match `reference/cabinet-v3-select.png`: top bar with SELECT MUSIC and mod chips, framed banner with title/artist/BPM, slanted difficulty rows with meter ticks and best %, a slanted wheel with pack header, and the gold-key hint bar. Navigation logic stays as it is.

### Acceptance Criteria

- [x] Draws `bg_select`, `bar_top` + `title_select_music`, and speed/scroll chips (`chip` tinted `kCyan` / `kGreen`, with text such as "SPEED 2.5x", "UPSCROLL")
- [x] Banner draws inside `banner_frame` (`banner_fallback` when the song has none), with title (`kSongTitle`), artist (`kArtist`) and BPM (`kBpm`) below, and long titles truncated by measured width
- [x] Difficulty rows use `diff_row_<name>[_selected]` with name, meter, 10 × `diff_tick` (filled up to the meter in the difficulty colour, else `kTickOff`) and best % ("---" when none). Edit charts get a neutral row with their chart name (#84)
- [x] Wheel uses `wheel_pack`, `wheel_row` and `wheel_row_selected` with `kWheelIndent` by distance from the selection, shows no artist on the wheel, and scrolls with an eased ~80 ms vertical slide on fixed `dt`
- [x] Hint bar shows "↑↓ SONG · ←→ DIFFICULTY · ENTER PLAY · TAB OPTIONS · ESC TITLE" with gold keys. Existing `select_screen_test` behaviour (window logic, key repeat, preview audio) still passes

### Technical Notes

- `src/screens/select_screen.cpp` (728 lines). Difficulty rows around :675-690, wheel window around :647 ("window is 13 rows tall"). The plan says keep the 13-row window logic, but at the new row heights (62 px rows, 92 px selected, 10 px gap) only about 7–8 rows fit in 720p. Keep the windowing algorithm and set the visible count from layout.
- The font has no ↑↓←→ glyphs. Draw the arrow keys as small quads or noteskin arrows, or add them to an atlas (assumption, check against the mock).
- Diff row textures exist for beginner/easy/medium/hard/challenge only. For Edit, draw a `draw_quad_points` parallelogram in `theme::difficulty::kEdit` (assumption).
- Long song titles and non-Latin scripts keep the placeholder behaviour from #77.
- Batch order: background → bars/frames → rows → text per atlas → overlays (≈10–25 flushes).

### As Built

- The select screen art is in `src/screens/select_art.cpp`. Later issues moved single offsets in the difficulty rows (#122, #126, #130, #131).

### Dependencies

- Blocked by: TODO-17, TODO-18, TODO-19
- Blocks: TODO-24, TODO-25, TODO-26

---

## [TODO-23] Restyle the score screen with the Cabinet look

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Large
**Phase**: Cabinet theme — Screens
**Labels**: `screens`, `frontend`, `render`, `scoring`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:163` — "3.6 score screen | L | 2.2, 2.3"
**GitHub**: #95
**Status**: done — issue closed, PR #107 merged

### Description

Rebuild the results screen to match `reference/cabinet-v3-results.png`: SCORE SCREEN top bar with difficulty badge, title and artist, stat panels on the left, grade medallion in the centre, chrome percentage with record/failed ribbon, and judgment bars on the right. Map the existing reveal animation onto the new parts.

### Acceptance Criteria

- [x] Draws `bg_results`, `bar_top` + `title_score_screen`, and badge ("HARD 8"), song title and artist right-aligned in the bar
- [x] Left stat panels (`stat_panel` + label + `digits_white`): MAX COMBO, DANCE POINTS "n / max", and HOLDS OK / NG / MINES
- [x] Centre: `medallion` + `grade_<tier>` (e.g. `quad_star` → `grade_quad_star`, `S+` → `grade_S_plus`) + tier label ("ONE STAR"), the percentage in `digits_chrome` counting up as now, and `record_ribbon` on a new record or `failed_ribbon` on fail
- [x] Right: six judgment rows (FANTASTIC…MISS) with labels in judgment colours, `draw_quad_points` track (`kBarTrack`) + fill proportional to count, and right-aligned counts. Hint bar shows "ENTER CONTINUE"
- [x] Reveal: title fades in, grade slams 2.4x → 1x on the medallion, stats fade, ribbon pops (white flash on NEW RECORD kept). `results_screen_test` / `results_anim_test` are updated ("SCORE SCREEN") and pass

### Technical Notes

- Files: `src/screens/results_screen.cpp` (today `draw_text("RESULTS")`, `grade_color`, FAILED text around :225-232), `src/screens/results_anim.cpp`.
- Grade labels come from `JudgmentConstants::grade_tiers` (`src/timing/judgment_constants.cpp:49-66`). Map the label to the texture name (`+` → `_plus`, `-` → `_minus`). There's no F texture. On fail, show the earned tier with `failed_ribbon` (the current screen shows grade + FAILED), unless the owner wants otherwise.
- Bar fill fraction: count / total judged taps (assumption, the mock's bar lengths are consistent with this). Draw the bars with `draw_quad_points` and `skew::kRows`.
- `digits_*` glyphs are `0-9 . % /` and space only. Format numbers so no other characters reach `BitmapDigits`.
- Medallion: `kMedallion`. Percent: `kPercentTop`. Ribbon: `kRecordRibbon`.

### As Built

- The score screen art is in `src/screens/results_art.cpp` and `src/screens/results_screen.cpp`; the stat labels got more left margin later (#127).

### Dependencies

- Blocked by: TODO-16, TODO-17, TODO-18, TODO-19
- Blocks: TODO-26

---

## [TODO-24] Restyle the options overlay with Cabinet parts

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Cabinet theme — Screens
**Labels**: `screens`, `frontend`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:164` — "3.4 / 3.7 options, remap, calibration | M | 3.3" (part 1 of 2: §3 step 4, options overlay)
**GitHub**: #96
**Status**: done — issue closed, PR #108 merged

### Description

The options overlay wasn't in the mock-ups. Restyle it with the parts song select already uses (scrim, plate header, slanted rows, gold selection, gold values), so it no longer looks like the old bitmap UI on top of the new select screen.

### Acceptance Criteria

- [x] Opening options dims the select screen with `theme::color::kGameplayScrim` (72% black) and shows a centred panel with a `bar_top` / `stat_panel`-style header
- [x] Each option (speed type, speed, scroll, assist tick, calibrate offset, remap input, …) is a slanted row (`diff_row`- or `wheel_row`-style), with the selected row in gold (`wheel_row_selected`) and values right-aligned in `kGold`
- [x] All rows fit at 1280x720 without overlapping the hint legend (no regression of the earlier "options room" fixes)
- [x] Option behaviour, sounds and transitions (to Calibrate / Remap and back with Esc) are unchanged, and `options_menu_test` passes

### Technical Notes

- The options UI lives in `src/screens/options_menu.cpp` (the plan says "same file" as select, but it's its own file). Row labels are around :238-250.
- Reuse the layout helper (TODO-19), text styles (`kWheelRow`, `kWheelSelected`, `kHintKey`) and texture helpers from TODO-17/18.
- Free design: not in the mock-ups. Keep it consistent with select.

### As Built

- The overlay is drawn by `src/screens/options_art.cpp` over `theme::color::kGameplayScrim`.

### Dependencies

- Blocked by: TODO-22
- Blocks: TODO-26

---

## [TODO-25] Restyle the Input Remap and Calibration screens with Cabinet parts

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Cabinet theme — Screens
**Labels**: `screens`, `frontend`, `input`, `timing`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:164` — "3.4 / 3.7 options, remap, calibration | M | 3.3" (part 2 of 2: §3 step 7, Input Remap and Calibration)
**GitHub**: #97
**Status**: done — issue closed, PR #109 merged

### Description

Input Remap and Calibration weren't in the mock-ups and still use bitmap text. Give them `bg_select`, the top bar with a runtime title, slanted rows and the hint bar, so every screen in the loop shares the Cabinet look.

### Acceptance Criteria

- [x] Both screens draw `bg_select`, `bar_top` with a runtime title ("REMAP INPUT" / "CALIBRATE OFFSET") in a Saira style, and a hint bar with gold keys
- [x] Remap shows the binding table as slanted rows (action, bound keys), with the selected row in gold and "RESET TO DEFAULTS" as a row
- [x] Calibration shows its phase word (GET READY / TAP ON THE BEAT / DONE), sample count and offset in the new type, centred and legible at 720p and 1440p
- [x] Input handling, calibration math and Esc-back-to-options behaviour are unchanged. `input_remap_screen_test` and `calibration_screen_test` pass

### Technical Notes

- Files: `src/screens/input_remap_screen.cpp` (title at :162, reset row at :208), `src/screens/calibration_screen.cpp` (title at :146, phase words at :149-155, samples/offset at :165-174).
- Optional, from the plan: bake chrome `GET READY` / `TAP ON THE BEAT` / `DONE` sprites in the `judgment_*` style. Not required for this issue, since runtime text is fine.
- Calibration must not change its timing path. The UI is presentation only (principle 1).
- Related: #74 (calibration offset bug) is independent of this restyle.

### As Built

- Both screens share `src/screens/setup_art.hpp` (the titles `REMAP INPUT` and `CALIBRATE OFFSET`, and the `RESET TO DEFAULTS` row).

### Dependencies

- Blocked by: TODO-22
- Blocks: TODO-26

---

## [TODO-26] Polish the Cabinet theme and verify it cross-platform

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Cabinet theme — Polish
**Labels**: `data`, `render`, `performance`
**Source**: `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md:165` — "3.8 polish + cross-platform check | S | all"
**GitHub**: #98
**Status**: pending

### Description

Finish the theme: add a config toggle for the scanlines overlay, check the performance budget, and run the cross-platform verification on Windows, macOS and Linux.

### Acceptance Criteria

- [ ] A persisted config setting (e.g. `video.scanlines`, default on) turns the scanlines overlay off and on, and config validation and persistence tests cover it
- [ ] `--perf-report` over a full song shows no regression vs. before the theme. Gameplay is no more expensive than before (fewer HUD quads)
- [ ] `docs/CROSS_PLATFORM_VERIFICATION.md` is updated with a theme pass: Linux machine-checked, Windows/macOS owner steps listed, with screenshots of each screen at 1280x720 and 2560x1440
- [ ] No screen still uses `bitmap_font` for player-facing text. Either delete it or keep it documented as debug-only

### Technical Notes

- Config: `src/data/config.hpp` (`VideoSettings`), `src/data/config_loader.cpp`, `tests/config_persistence_test.cpp`. Whether to expose the toggle in the options overlay is up to the owner (assumption: config-file only is enough).
- Optional, from the plan: slowly rotate the results sunburst by splitting it out of `bg_results.png` into its own texture. Not required.
- VRAM note from the plan: `bg_select` / `bg_results` can ship at 1x if memory matters. Keep `bg_title` at 2x.

### Dependencies

- Blocked by: TODO-20, TODO-21, TODO-22, TODO-23, TODO-24, TODO-25
- Blocks: None

---

## [TODO-27] Show song subtitles in song select and results

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `screens`, `chart`
**Source**: `TODO.md:43` — "If songs have subtexts in their names they dont show up right now in song select. I happen to know for example that the Disconnect songs have a subtext or something after them. Currently they just show up as "Disconnect", "Disconnect", "Disconnect" in the song select even though they are different songs after all."
**GitHub**: #110
**Status**: done — issue closed, PR #115 merged

### Description

The parser already reads `#SUBTITLE` / `#SUBTITLETRANSLIT`, but no screen draws them, so songs that share a title (e.g. the several "Disconnect" songs) look identical in the song wheel. Draw the subtitle wherever the song title is shown so these songs can be told apart.

### Acceptance Criteria

- [x] Wheel rows for songs with a non-empty `#SUBTITLE` show the subtitle next to or under the title, so songs sharing a title are visibly different
- [x] The song info panel on the select screen shows the subtitle with the title
- [x] The results screen title bar shows the subtitle too
- [x] The subtitle uses `#SUBTITLETRANSLIT` when the active font can't draw the native text, using the same rule as title and artist
- [x] Long title + subtitle text is truncated by measured width and never overlaps other wheel or panel elements. Songs without a subtitle look the same as today

### Technical Notes

- Data is already there: `SongMetadata::subtitle` / `subtitle_translit` (`src/chart/song_metadata.hpp`), filled in `src/chart/simfile_parser.cpp:96-103`.
- Add `song_display_subtitle(...)` next to `song_display_title` / `song_display_artist` in `src/screens/song_display_text.{hpp,cpp}` (same `select_display_text` coverage rule, both bitmap and TrueType overloads).
- Draw sites: wheel labels `src/screens/select_screen.cpp:~653` → `select_art::draw_wheel`; info panel `select_screen.cpp:~596` → `select_art::draw_song_info` (`src/screens/select_art.cpp:~476`); results `src/screens/results_screen.cpp:~110` / `~358`.
- Reference: SM5 `TextBanner` / `MusicWheelItem` show the subtitle as a smaller second line under the main title. Assumption: follow that here (a smaller second line, or `Title (subtitle)` on one line if a row has no room for two). Pick whichever fits the Cabinet wheel layout cleanly.
- Display only. High-score keys (`make_chart_key`) are based on chart notes and don't change.
- Add unit tests for the subtitle display choice next to the existing `song_display_text` tests.

### As Built

- The subtitle, and `#SUBTITLETRANSLIT` when the font cannot draw it, come from `src/screens/song_display_text.hpp`; select and results both use it.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-28] Make the gameplay judgment text 50% smaller

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `render`
**Source**: `TODO.md:44` — "The step grade text in gameplay screen is too large (fantastic, excellent, great, etc...) It should be perhaps 50% smaller."
**GitHub**: #111
**Status**: done — issue closed, PR #113 merged

### Description

The judgment pop (Fantastic, Excellent, Great, Decent, Way Off, Miss) is too large on the gameplay screen and gets in the way. Scale it down to about half its current size.

### Acceptance Criteria

- [x] The judgment sprite is drawn at about 50% of its current size at rest, at both 1280x720 and 2560x1440
- [x] The pop animation keeps the same shape (eases up to ~1.25x, settles to 1.0x, then fades) relative to the new smaller size
- [x] The smaller judgment stays centred on the note field and doesn't overlap the combo line below it. Move `kJudgmentTop` / combo position only if needed
- [x] The bitmap fallback label (no theme / missing sprite) is scaled down the same way
- [x] `tests/judgment_animator_test.cpp` is updated for the new size and passes

### Technical Notes

- Sizing: `JudgmentAnimator::render_judgment` / `judgment_pop_rect` in `src/gameplay/judgment_animator.cpp:~159-210` draw the `judgment_<kind>` sprite at `L.s * scale`. Add a named constant for the display scale (e.g. `kJudgmentDisplayScale = 0.5f`) in `src/gameplay/judgment_animator.hpp` and apply it to both the sprite and `kJudgmentPopPixel`. Don't edit the sprite art.
- Layout constants: `kJudgmentTop` in `src/render/theme.hpp:209`. `kJudgmentContentRef` (444x66 ref px) describes the sprite content box.
- "Perhaps 50%" is the owner's estimate. Keep the factor in one constant so it's easy to change after a playtest.
- Presentation only: nothing touches the music clock or judgment path.

### As Built

- The size and the pop curve are in `src/gameplay/judgment_animator.cpp`, positioned from `theme::layout::kJudgmentTop`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-29] Hide the mouse cursor during gameplay

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `app`, `screens`
**Source**: `TODO.md:45` — "Hide mouse when playing the game."
**GitHub**: #112
**Status**: done — issue closed, PR #114 merged. Of 5 criteria, 1 only partly verified.

### Description

The mouse cursor stays visible over the note field during play. Hide it while a song is being played and bring it back when leaving gameplay.

### Acceptance Criteria

- [x] The mouse cursor is hidden while the gameplay screen is active, in both windowed and fullscreen modes
- [x] The cursor is visible again on the results screen and every other screen after gameplay ends, is quit, or fails
- [x] Pausing (and the pause window, #60, once it exists) doesn't leave the cursor stuck hidden after leaving gameplay
- [x] Headless / unit-test paths (no window) don't crash. The cursor call is null-guarded like other `ScreenContext` services
- [ ] Builds on Linux, Windows and macOS
  - *Partly verified:* the Linux build ran on 2026-10-12. No Windows or macOS build is recorded.

### Technical Notes

- SDL3: `SDL_HideCursor()` / `SDL_ShowCursor()`. Keep SDL out of `src/screens/` (AGENTS.md "Thin platform wrapper"): add a small method on the window wrapper (`src/app/window.{hpp,cpp}`, e.g. `set_cursor_visible(bool)`) and expose it to screens through `ScreenContext` (`src/screens/screen.hpp`) as a nullable pointer or `std::function`, like `action_down`.
- Hide in `GameplayScreen::enter` and show in `GameplayScreen::exit` (`src/screens/gameplay_screen.cpp`). Assumption: hide during gameplay only, since the task says "when playing". Menus don't use the mouse either, so hiding it everywhere would also work if the owner prefers.
- Also make sure the cursor is shown again on app shutdown and on the `--gameplay-demo` path.

### As Built

- Screens call `ScreenContext::set_cursor_visible`, which `src/main.cpp` wires to `Window::set_cursor_visible`; it is null in headless and unit-test runs.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-30] Compare mine damage against OpenITG / StepMania

**Type**: Spike
**GitHub Label**: spike
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `gameplay`, `scoring`
**Source**: `TODO.md:46` — "I think the mines do way too much damage. Check OpenITG / Stepmania how much damage they do in those and compare to this game."
**GitHub**: #118
**Status**: done — issue closed, PR #120 merged

### Description

Question: does a hit mine cost the same life in Blaze 4k as in OpenITG (and StepMania), both as configured values and as applied by the life bar? Mines currently feel like they do far too much damage.

### Acceptance Criteria

- [x] The life a mine takes (`life_deltas.hit_mine`, 5%) and the full-bar override (`hot_downgrade`, 10%) are checked against OpenITG source, with file:line references recorded on the issue
- [x] Mine side effects beyond the life delta are compared too: the combo-to-regain-life debt after a hit (`regen_combo_after_miss`, 5 good steps refill nothing) and the DP penalty (`dp_weights.hit_mine`, -6)
- [x] StepMania 5's defaults for the same values are recorded for comparison; OpenITG stays the reference the game follows
- [x] A single mine takes life at most once, whether stepped on or held through, proven by a unit test
- [x] Any mismatch is fixed with a unit test in `tests/life_keeper_test.cpp`, or filed as a follow-up bug

### Technical Notes

- Values: `assets/data/judgment_constants.json` (`life_deltas.hit_mine` = -0.05, `hot_downgrade` = -0.1, `regen_combo_after_miss` = 5, `dp_weights.hit_mine` = -6); its `source` field cites OpenITG `metrics.ini`, `PrefsManager.cpp` and `LifeMeterBar.cpp`. Loaded by `src/data/judgment_constants_loader.cpp`.
- Life path: `src/gameplay/life_keeper.cpp` — `HitMine` applies `delta_for_tap(TapJudgment::HitMine)` immediately; `delta_for_tap` swaps in `hot_downgrade` when the bar is full (`is_hot()`); `apply()` mirrors `LifeMeterBar::ChangeLife` including the combo-to-regain debt.
- Mine triggering: `src/gameplay/judgment_engine.cpp` — `handle_step_mine` (stepped on, `hit_mine` window 70 ms) and `cross_mines` (column held for `pad_stick` as the mine crosses; cursor ensures each mine is evaluated once, see #56).
- Scoring: `src/gameplay/score_keeper.cpp` (mine hit adds DP penalty, doesn't break combo).
- Likely reason mines feel harsh: on a full bar a mine costs 10% instead of 5%, and the next 5 positive judgments refill nothing. Both are intentional OpenITG mirrors, so the spike should confirm them against source rather than assume they're wrong.
- Same shape as #57 (verify judgment windows against OpenITG). AGENTS.md: OpenITG is the reference authority for life behavior.

### As Built

- The values are `life_deltas.hit_mine` (-0.05), `life_deltas.hot_downgrade` (-0.1) and `dp_weights.hit_mine` (-6) in `assets/data/judgment_constants.json`.
- The dance-point mismatch was filed as #119 and is closed (PR #121).

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-31] Shift the difficulty meter number and ticks right in song select

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `frontend`
**Source**: `TODO.md:47` — "There needs to be a little bit more room for the songs difficulty number in song select screen. What I would like to be done is move the difficulty number and the difficulty bars ever so slightly to the right, that way the numbers would have more room."
**GitHub**: #122
**Status**: done — issue closed, PR #123 merged

### Description

In the song select difficulty rows, the meter number sits in a tight gap between the difficulty name tab and the first of the 10 meter ticks, so it looks cramped. Move the number and the ticks slightly to the right together, giving the number more room.

### Acceptance Criteria

- [x] The meter number and the 10 meter ticks both move right by the same small amount, keeping their position relative to each other
- [x] A two-digit meter (e.g. `12`) in the selected row's larger font clears both the name tab and the first tick
- [x] The last tick still ends well before the best-% column; the existing gap check in `tests/select_art_test.cpp` passes and the `tick_rect` position checks are updated to the new x values
- [x] The change applies the same way to normal rows, the selected row, and the code-drawn Edit row
- [x] A song select screenshot (via `/verify`) shows a chart with a two-digit meter laid out cleanly

### Technical Notes

- Layout constants: `src/screens/select_art.hpp` — `kDiffMeterCentreX = 174` (number centre) and `kDiffTickX = 194` (first tick), both measured from the row's content x. Shift both by the same delta (assumed ~10–16 px in layout units; "ever so slightly").
- Tick geometry: `tick_rect` in `src/screens/select_art.cpp` (pitch `layout::kDiffTickPitch` = 18, `src/render/theme.hpp`); ticks currently span x 194–376. Best % is right-aligned at `kDiffBestRight = 547`, so there is roughly 80 px of slack before the "100.00%" text.
- Meter text styles: `theme::text::kDiffMeter` (28 px) / `kDiffMeterSelected` (32 px) in `src/render/theme.hpp`; the name tab is ~150 px wide (`kEditTabWidth`), so the number's left side is bounded by the tab edge.
- Tests: `tests/select_art_test.cpp` `test_ticks()` hard-codes tick x positions (238/400 and 252/414) — update them to the new offsets.
- Assumption: only these two constants change; the row art, name, and best % stay where they are.

### As Built

- The meter and the ticks are placed by `kDiffMeterCentreX` (190) and `kDiffTickX` (210) in `src/screens/select_art.hpp`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-32] Render symbol characters such as ☺ in song artist names

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `render`, `screens`
**Source**: `TODO.md:48` — "Special character names for artists don't render. A good example of this: there is a song called \"Delirium\" in the \"In The Groove\" pack whose artist name is just a smiley. It renders the artist name as just a plain block."
**GitHub**: #124
**Status**: done — issue closed, PR #125 merged

### Description

Current: the ITG song "Delirium" has `#ARTIST:☺;` (U+263A, UTF-8 `E2 98 BA`) and an empty `#ARTISTTRANSLIT`, so song select shows the hollow placeholder box instead of the smiley. The TrueType atlas only bakes U+0020–U+017F, and none of the bundled fonts (Saira Condensed, Audiowide) has a glyph for U+263A, so the existing translit/ASCII-fold fallbacks have nothing to fall back to. Expected: common symbol characters in artist (and title) text render as the real glyph.

### Acceptance Criteria

- [x] Delirium's artist shows a smiley glyph, not the placeholder box, in song select
- [x] The same artist text renders correctly on the results screen
- [x] Code points that are still missing from every loaded font keep drawing the placeholder box, and malformed UTF-8 still doesn't crash
- [x] Unit tests cover glyph lookup and `covers_text` for at least U+263A
- [x] A song select screenshot (via `/verify`) shows Delirium's artist rendered

### Technical Notes

- Root cause: `kBakedRanges` in `src/render/ttf_font.cpp:46` covers U+0020–U+007E, U+00A0–U+00FF, and U+0100–U+017F only (`kBakedGlyphCount = 319`, `src/render/ttf_font.hpp:67`). Missing code points use `fold_to_ascii` (`src/render/unicode_text.hpp`), else the in-atlas placeholder box (`ttf_font.cpp` ~591).
- Font coverage (from `fc-query`): Saira Condensed and Audiowide have no Miscellaneous Symbols (U+2600–U+26FF) glyphs, so baking a wider range alone isn't enough.
- Likely approach: bundle a small OFL symbol fallback font (e.g. a Noto Sans Symbols 2 subset, with its OFL text in `assets/fonts/`), bake a symbol range from it (at least U+2600–U+26FF), and look glyphs up in the fallback face when the primary face lacks them. Keep atlas size within the existing cap.
- Display selection: `song_display_artist` / `song_display_title` in `src/screens/song_display_text.{hpp,cpp}` pick translit text based on `TextRenderer::covers_text`; coverage must count fallback-face glyphs so the native text is chosen when it can be drawn.
- Assumptions: scope is symbols that show up in real ITG/SM pack metadata (Miscellaneous Symbols, and possibly Dingbats/arrows); full CJK coverage is out of scope here.

### As Built

- Symbols are drawn from `assets/fonts/NotoSansSymbols-Subset.ttf`, which `scripts/build-symbol-font.sh` builds.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-33] Add left margin to the difficulty name text in song select

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `frontend`
**Source**: `TODO.md:49` — "Add a little bit more margin to left hand side to song difficulty text when selecting song. It is especially evident on the difficulty currently chosen which is positioned a bit differently."
**GitHub**: #126
**Status**: done — issue closed, PR #128 merged

### Description

In the song select difficulty rows, the difficulty name (`HARD`, `CHALLENGE`, an Edit chart's name, …) starts very close to the slanted left edge of its coloured tab. It is most noticeable on the currently selected row, which is taller and uses a larger font. Move the name text slightly right so it has a visible left margin in every row.

### Acceptance Criteria

- [x] The difficulty name starts further right inside its tab in every difficulty row, by a small amount (about 6–10 px in layout units)
- [x] In the selected row, the gap between the name and the slanted left edge of the tab looks at least as large as in the unselected rows
- [x] The longest names (`CHALLENGE`, `BEGINNER`, truncated Edit chart names) still fit inside the tab and stay clear of the meter number
- [x] The meter number, ticks and best % stay where they are
- [x] A song select screenshot (via `/verify`) shows the new margin on a selected and an unselected row

### Technical Notes

- Name x offset: `kDiffNameX = 15` in `src/screens/select_art.hpp`, measured from the row's content x; drawn left-aligned in `draw_difficulty_rows` (`src/screens/select_art.cpp`, the "names" loop) at `row.x + kDiffNameX`.
- Why the selected row looks worse: `difficulty_row_rect` makes it 52 px tall instead of 44 (`layout::kDiffRowSelectedHeight` / `kDiffRowHeight`, `src/render/theme.hpp`) and shifts it right by `kDiffRowSelectedShiftX = 14`, and its name uses `theme::text::kDiffNameSelected` (22 px) instead of `kDiffName` (20 px). The tab's left edge is slanted, so in the taller row it cuts closer to the larger text. The selected row may need a larger offset than the others, or one offset sized for the selected row.
- Name width budget: `kDiffNameBudget = 128` (the tab is ~150 px wide at mid-height, `kEditTabWidth = 150`). Moving the name right by N px means lowering the budget by about N so truncated Edit names still end inside the tab; the meter number is centred at `kDiffMeterCentreX = 190`.
- Tests: `tests/select_art_test.cpp` and `tests/select_screen_test.cpp` check that every difficulty name, in both text styles, measures within `kDiffNameBudget`. They keep passing only if the standard names still fit the reduced budget.
- Assumptions: "difficulty text" is the difficulty name, not the meter number or best %; only the name moves, and the baked row art is unchanged.

### As Built

- The name's left edge comes from `difficulty_name_x` in `src/screens/select_art.hpp`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-34] Add left margin to the stat panel labels on the results screen

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `frontend`
**Source**: `TODO.md:50` — "Give more left margin to \"Max combo\", \"Dance points\" and \"Holds & Mines\" texts in Score Screen window."
**GitHub**: #127
**Status**: done — issue closed, PR #129 merged. Of 5 criteria, 1 not checked.

### Description

On the results screen, the three stat panels (max combo, dance points, holds and mines) draw their label text close to the panel's left edge. Move the `MAX COMBO`, `DANCE POINTS` and `HOLDS OK` / `NG` / `MINES` labels right so they have more left margin, keeping the numbers under them aligned.

### Acceptance Criteria

- [x] `MAX COMBO`, `DANCE POINTS` and the `HOLDS OK` / `NG` / `MINES` label row start further right from their panel's left edge (about 8–12 px in layout units)
- [x] The numbers under each label move right by the same amount, so label and value stay aligned along the panel's slant
- [x] In the holds panel, the `MINES` column still fits inside the panel with wide values
- [x] `tests/results_screen_test.cpp` is updated for the new offset and passes
- [ ] A results screen screenshot (via `/verify`) shows the new margin on all three panels
  - *Not checked:* no screenshot is recorded in a review, and the window was not opened for this update.

### Technical Notes

- Shared offset: `kStatTextPadX = 20` in `src/screens/results_art.hpp`, applied by `stat_text_x(panel, centre_y)` in `src/screens/results_art.cpp` (`panel.x + kStatTextPadX + skew::kStatPanel * (panel centre y - centre_y)`). Labels and values both call it, so raising the constant moves them together and keeps them aligned along the -8° slant.
- Call sites in `src/screens/results_screen.cpp`: the `MAX COMBO` / `DANCE POINTS` labels and the `kHoldLabels` row in the "TrueType text" block, and the combo, dance point and hold values in the digit block above it.
- Holds panel width: columns come from `hold_columns` (`max(label, value) + kHoldColumnGap` of 34); the panel is 360 px wide (`layout::kStatPanelWidth`). Check that the third column (`MINES` and its value) still ends inside the panel after the shift. The dance points value (`earned/max`) is the widest value in the other two panels.
- Tests: `tests/results_screen_test.cpp` (~line 581) computes label and value x through `stat_text_x`; update any expectation that hard-codes the 20 px pad. Update the `kStatTextPadX` comment (`manifest: "label at x+20, y+14"`) to say the value now departs from the manifest.
- Assumptions: "Holds & Mines" is the third panel's `HOLDS OK` / `NG` / `MINES` labels; the panel art and the judgment rows on the right are unchanged.

### As Built

- The label and value inset is `kStatTextPadX = 30` in `src/screens/results_art.hpp`. This issue has no review report under `.agents/reviews/`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-35] Make the Edit difficulty row match the other rows' size and position in song select

**Type**: Bug
**GitHub Label**: bug
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `frontend`
**Source**: `TODO.md:51` — "For some reason Edit-type song charts difficulty block is larger than the others in song select screen, and also when you select it, it is placed differently on the UI. It should behave just like the other difficulty settings."
**GitHub**: #130
**Status**: done — issue closed, PR #132 merged

### Description

In the song select difficulty list, an Edit chart's row is visibly larger than the Beginner–Challenge rows, and when it is selected it sits in a different place than the other rows do when selected. Expected: the Edit row has the same size and position as every other difficulty row, selected or not. The Edit row is the only one drawn in code instead of from baked art, and it fills the whole row rect while the baked art is inset.

### Acceptance Criteria

- [x] An unselected Edit row has the same visible width, height and left/right edges as an unselected Beginner–Challenge row in the same slot
- [x] A selected Edit row sits at the same position and has the same visible size as any other selected row, including its gold frame
- [x] The Edit row's colour tab starts and ends at the same x as the other rows' tab, and its name, meter, ticks and best % sit where they do in the other rows
- [x] A test in `tests/select_art_test.cpp` checks the Edit row's geometry against the baked rows'
- [x] A song select screenshot (via `/verify`) of a song with an Edit chart shows the Edit row lined up with the rows around it, selected and unselected

### Technical Notes

- Row art: `draw_difficulty_row_art` in `src/screens/select_art.cpp` draws the baked rows with `theme.draw_slice3(..., L.rect(row))` and the Edit row (`style.baked == false`, #84) as `skewed_quad(row, theme::skew::kRows)` solids — body, 1 px top/bottom edge strips, and a `kEditTabWidth = 150` tab starting at `row.x`. A selected Edit row adds a gold quad outset by `kEditSelectedOutset = 2`.
- Measured cause: the baked art's opaque body is inset inside its content rect. `diff_row_beginner.png` (content 564×44 at 1x) spans x 10.0 → 553.5 at mid-height (14.5 → 558 at the top, 5.5 → 549 at the bottom). `diff_row_beginner_selected.png` (content 564×52) spans x 12.0 → 551.5 at mid-height. The code-drawn quad spans 0 → 564 at mid-height, and −2 → 566 with the selected outset: about 20 px wider, and starting 10–14 px further left.
- Likely fix: inset the Edit row's body, tab and gold frame to the baked rows' measured extents, for the unselected and the selected state, with named constants next to `kEditTabWidth` in `src/screens/select_art.hpp` that record the measurement. Measure the baked tab's left and right edge for the Edit tab as well.
- The row rect (`difficulty_row_rect`) and the `kDiff*` text offsets are shared by all rows and should not need to change. The `kDiffNameBudget` comment ("inside the 150px Edit tab") depends on where the Edit tab ends; update it if the tab moves.
- Tests: `tests/select_art_test.cpp` (Edit style checks ~line 555, Edit tab checks ~lines 626 and 670) and `tests/select_screen_test.cpp` (named Edit charts through the real render path, ~line 278).
- Assumptions: the Edit row stays code-drawn (no `diff_row_edit` texture is added); the fix matches geometry only, and the soft glow baked into the other selected rows is not reproduced.

### As Built

- The Edit row's frame, inner and tab rects come from `edit_row_rects` in `src/screens/select_art.hpp`.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-36] Add right margin to the best % text in the song select difficulty rows

**Type**: Enhancement
**GitHub Label**: enhancement
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `screens`, `frontend`
**Source**: `TODO.md:52` — "In song select screen, last high score percentage text needs more right margin. It hugs too closely to the outline currently."
**GitHub**: #131
**Status**: done — issue closed, PR #133 merged

### Description

In the song select difficulty rows, the best % at the right end of each row ends only a few pixels from the row's right outline. It is closest in the selected row, next to the gold frame. Move the text left so it has a visible right margin in every row.

### Acceptance Criteria

- [x] The best % text ends further left in every difficulty row, leaving about 14–18 px (layout units) to the row's right outline
- [x] In the selected row, the gap between the best % and the gold frame looks at least as large as in the unselected rows
- [x] A `100.00%` best still stays clear of the last difficulty tick
- [x] The difficulty name, meter number and ticks stay where they are
- [x] A song select screenshot (via `/verify`) shows the new margin on a selected and an unselected row

### Technical Notes

- Right edge: `kDiffBestRight = 547` in `src/screens/select_art.hpp`, measured from the row's content x; drawn right-aligned in the "bests" loop of `draw_difficulty_rows` (`src/screens/select_art.cpp`) at `row.x + kDiffBestRight`, in `theme::text::kDiffBest` (20 px) or `kDiffBestSelected` (22 px, gold).
- Why it hugs the outline: the constant's comment says "17px in from the right edge", but that is the edge of the 564 px row rect. The baked art's body ends earlier: at mid-height it ends at x 553.5 in an unselected row (6.5 px gap) and at x 551.5 in the selected row (4.5 px gap, to the gold frame). The right edge is slanted (`theme::skew::kRows`), from 558 at the top to 549 at the bottom of an unselected row.
- A value of about 537–541 gives a 14–18 px gap in unselected rows. The selected row is tighter and its text is larger, so it may need its own offset, as the name got in #126 (`kDiffNameSelectedX`).
- Room on the left: the last tick ends at x 392 (`kDiffTickX = 210` + 9 × 18 + 20). `tests/select_art_test.cpp` (~line 611) requires `tick_rect(row, false, 9).x + 20 < row.x + kDiffBestRight - 60`, which still holds at 537.
- Update the `kDiffBestRight` comment to say the value now departs from the mock.
- Related: [TODO-35] moves the Edit row's right edge in to match the baked rows. Either order works; measure the gap on a baked row.
- Assumptions: "last high score percentage text" is the best % at the right end of each difficulty row; "outline" is the row's right edge (the gold frame when selected).

### As Built

- The right edge of the best % is `kDiffBestRight = 537`, and `kDiffBestSelectedRight = 533` in the selected row (`src/screens/select_art.hpp`).

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-37] Make the automated tests silent by default

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `tests`, `audio`
**Source**: `TODO.md:53` — "Make the automated tests silent by default: no test may open the real sound device unless it is explicitly asked to. Tests should use the audio engine's existing null backend (`use_null_backend`); an explicit opt-in (for example an environment variable) keeps real-device runs possible for latency measurements such as `clock_probe`. Plain `ctest` must then be safe without the `bwrap` sandbox prefix. #technical (from the 2026-10-12 retro, finding 2)"
**GitHub**: #134
**Status**: pending

### Description

Some tests open the machine's default sound device: `audio_test` and `music_clock_test` do it directly, and other tests may reach the audio engine through UI sounds, previews, the assist tick or the gameplay view. The only protection today is a `bwrap` sandbox prefix that is not part of this repository, so a run that forgets it reaches the owner's real device. Make the tests use the audio engine's silent null device by default, with an explicit opt-in for real-device runs.

### Acceptance Criteria

- [ ] No test opens the real sound device unless it is explicitly asked to
- [ ] Tests use the audio engine's existing null backend (`use_null_backend`)
- [ ] An explicit opt-in (for example an environment variable) keeps real-device runs possible for latency measurements such as `clock_probe`
- [ ] Plain `ctest` is safe without the `bwrap` sandbox prefix
- [ ] Running a test binary directly, outside `ctest`, is silent too

### Technical Notes

- The seam exists: `AudioEngineSettings::use_null_backend` in `src/audio/audio_engine.hpp`, handled in `AudioEngine::init()` (`src/audio/audio_engine.cpp`). `tests/clock_tap_test.cpp` already sets it through `configure(...)`.
- Tests that call `AudioEngine::instance().init()` with default settings: `tests/audio_test.cpp`, `tests/music_clock_test.cpp`.
- Other code that reaches `AudioEngine::instance()`: `src/audio/ui_sounds.cpp`, `src/audio/assist_tick_player.cpp`, `src/audio/sound_stream.cpp`, `src/gameplay/gameplay_view.cpp`. Which tests open a device through these was not checked; candidates include `assist_tick_test`, `preview_player_test`, `ui_sounds_test` and `calibration_screen_test`.
- The engine is a lazily initialized singleton reached from several places, so one central switch is safer than editing each test. It has to hold for directly run binaries too, so an environment variable set only through ctest's test properties is not enough on its own.
- The header documents `use_null_backend` as "Never set by main.cpp". Keep the game's own device selection unchanged.
- `tests/clock_probe.cpp` is built but not registered with ctest. It measures the real device by default and has `--null`. Real-device measurement must stay possible.
- Evidence for the criteria: the engine prints `[AudioEngine] Output device: '<name>' via <backend>` at init, so a test log shows which backend each test used.
- `music_clock_test` skips its audio integration part when `init()` fails ("no audio device"). Report any test part that changes between skipped and run.
- The null device is real-time-paced but wakes unevenly (see the `clock_probe.cpp` header), so do not tighten timing-precision assertions against it. No change to the music-clock path (AGENTS.md principle 1).
- Docs: the Testing section of `docs/BUILDING.md`; `docs/AUDIO_LATENCY.md` and the `clock_probe.cpp` header refer to "the test sandbox".
- Assumptions: "automated tests" means the ctest-registered tests and their binaries. The game's own headless smoke run (`blaze-4k --headless --smoke-test`) is out of scope. The sandbox prefix in use today is `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net`.

### Dependencies

- Blocked by: None
- Blocks: [TODO-38], [TODO-39]

---

## [TODO-38] Add one check command (warnings as errors + tests) and export compile_commands.json

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `build`, `tests`
**Source**: `TODO.md:54` — "Add one check command for the repo: a `scripts/check.sh` that builds with compiler warnings treated as errors and runs all tests, and turn on CMake's `compile_commands.json` export for the normal build. Agents and the owner then use one command instead of each review improvising its own warning check. #technical (from the 2026-10-12 retro, finding 3)"
**GitHub**: #135
**Status**: pending

### Description

The repository has no single command that proves a change is clean. An incremental build does not re-emit warnings for files it has already compiled, so each review recompiles the changed sources by hand to look for warnings, and `compile_commands.json` exists only in the `linux-portability` preset. Add one check script that builds with warnings treated as errors and runs all tests, and export `compile_commands.json` from the normal build.

### Acceptance Criteria

- [ ] `scripts/check.sh` builds with compiler warnings treated as errors and runs all tests
- [ ] The normal build exports `compile_commands.json`
- [ ] The script fails when a project source file has a warning or a test fails, and succeeds otherwise
- [ ] Warnings inside third-party code (SDL3, glad, miniaudio, stb) do not fail it
- [ ] `docs/BUILDING.md` names the script as the command to run before a commit
- [ ] `docs/BUILDING.md` no longer states a fixed test count

### Technical Notes

- Warning flags are set at `CMakeLists.txt:10-14` with `add_compile_options(-Wall -Wextra -Wpedantic)` before `FetchContent`, so they also reach the fetched dependencies. Adding `-Werror` there would fail on dependency warnings.
- `miniaudio` and `stb` are `INTERFACE` libraries, so their implementations compile inside project sources. Their headers need to count as system includes, or the implementation files need their own flags.
- Make warnings-as-errors a CMake option that is off by default, so the owner-run Windows and macOS presets are unchanged.
- Give the check its own build directory through a preset. The `base` preset in `CMakePresets.json` uses `build-${presetName}` and reuses `build/_deps` through `FETCHCONTENT_BASE_DIR`. A configure that shares `_deps` can rewrite `build/_deps/*-build`, so confirm that the owner's `build/` still builds afterwards.
- With warnings as errors, a file with a warning never compiles, so a later incremental build reports it again.
- `compile_commands.json` is already in `.gitignore`. Only the `linux-portability` preset sets `CMAKE_EXPORT_COMPILE_COMMANDS` today. "The normal build" means the plain `cmake -B build` configure and the `linux-gcc-*` presets.
- `scripts/fresh-clone-check.sh` shows the existing script style (`set -euo pipefail`, usage text).
- `docs/BUILDING.md` says "expect 34/34" at lines 88, 102 and 117, while the suite has 51 tests. Remove the count instead of updating it: `ctest` prints it. Leave `docs/CROSS_PLATFORM_VERIFICATION.md` alone, because its 34/34 is a dated record of a past check.
- Not checked: whether a full rebuild of today's tree is free of warnings.
- Assumption: the script targets the Linux/GCC host. Clang is not installed there.

### Dependencies

- Blocked by: [TODO-37] (the script runs the tests, which open the real sound device until then)
- Blocks: None
- Related: [TODO-39]

---

## [TODO-39] Add a sanitizer build preset (AddressSanitizer + UndefinedBehaviorSanitizer)

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `build`, `tests`
**Source**: `TODO.md:55` — "Add a sanitizer build: a CMake preset that builds and runs the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer, so memory errors and undefined behaviour are caught automatically instead of by code reading. #technical (from the 2026-10-12 retro, finding 3)"
**GitHub**: #136
**Status**: pending

### Description

Memory errors and undefined behaviour are found today only when a reviewer spots them in the code. Reviews have found three such bugs by reading: an out-of-range number cast, a texture destroyed while the renderer still used it, and a sound freed while the audio thread was reading it. Add a build preset that runs the tests under GCC's AddressSanitizer and UndefinedBehaviorSanitizer so this class of bug is caught automatically.

### Acceptance Criteria

- [ ] A CMake preset builds and runs the tests with GCC's AddressSanitizer and UndefinedBehaviorSanitizer
- [ ] All tests pass under the preset, or each sanitizer report is fixed or filed as its own issue
- [ ] `docs/BUILDING.md` documents the preset and the packages it needs
- [ ] The normal release and debug builds are unchanged

### Technical Notes

- **Owner step first**: the sanitizer runtimes are not installed on the Linux host. `gcc -fsanitize=address,undefined` fails at link with `cannot find /usr/lib64/libubsan.so.1.0.0`, and `rpm -q libasan libubsan` reports both as not installed. Run `sudo dnf install libasan libubsan` before starting this issue.
- Presets live in `CMakePresets.json`. The `benchmark` preset is an existing special-purpose preset with its own configure, build and test entries.
- Sanitizers slow the code down. `perf_loop_test` checks a CPU budget and other tests check timing, so they may need to be left out of this preset or given a looser limit in it. Do not change them in the normal builds.
- Earlier review findings of this class: `read_integer` in `src/data/high_scores.cpp` (out-of-range `double` to `int64_t` cast), the symbol atlas rebake in `src/render/ttf_font.cpp` (texture destroyed while `GlQuadRenderer` held quads), and the tap replacement in `src/audio/audio_engine.cpp` (sound freed while the audio thread read it).
- Assumptions: ThreadSanitizer is out of scope, because it cannot be combined with AddressSanitizer. The preset targets the Linux/GCC host.

### Dependencies

- Blocked by: [TODO-37] (the preset runs the tests, which open the real sound device until then)
- Blocks: None
- Related: [TODO-38]

---

## [TODO-40] Record draw calls in the headless renderer so tests can check them

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `render`, `tests`
**Source**: `TODO.md:56` — "Make draw calls testable without a window: let the headless renderer record what it was asked to draw (position, texture, colour), so a test can check a decision that today exists only inside a draw call."
**GitHub**: #141
**Status**: pending

### Description

Without a window, `GlQuadRenderer` does nothing and its callers return early, so a test that renders a screen passes even when the code draws the wrong thing. 18 of the 70 review reports flagged a test that cannot fail, and the fix each time was to move one decision into a small helper by hand. Let the headless renderer record the quads it is asked to draw, so a test can check the draw site itself.

### Acceptance Criteria

- [ ] The headless renderer records each quad it is asked to draw (corners, texture, colour, blend mode), and a test can read the recording
- [ ] A test of the code-drawn Edit difficulty row checks the recorded quads (body, four ring strips, tab, each with its colour), selected and not selected
- [ ] That test fails when a ring strip gets the wrong rectangle or colour
- [ ] The real game draws the same as before, shown by a `/verify` screenshot of song select
- [ ] The "review only" row about draw-call decisions in the `AGENTS.md` rule table is updated to name the recorder

### Technical Notes

- `GlQuadRenderer` (`src/render/gl_quad_renderer.hpp`) documents that "a headless (uninitialized) renderer is a safe no-op". Its draw calls all end in `quad_vertices`, which is pure, so a recording mode can keep the vertices and the texture id instead of sending them to GL.
- 21 call sites in 8 files return early on `renderer.is_initialized()` (`src/screens/results_art.cpp` has 8; also `hud_renderer.cpp`, `judgment_animator.cpp`, `note_field_renderer.cpp`, `gameplay_view.cpp`, `background_renderer.cpp`, `ttf_font.cpp`, `main.cpp`). Decide whether a recording renderer answers true there, or the guards move behind one predicate.
- The Edit row is drawn in `src/screens/select_art.cpp` (`draw_difficulty_row_art`, through `draw_solid` and `edit_row_rects`). `edit_row_rects` is already tested in `tests/select_art_test.cpp`; which rectangle and colour each quad gets is not.
- Earlier per-case fixes of this class: `difficulty_name_x` and `edit_row_rects` in `src/screens/select_art.cpp`, `merge_symbol_slots` in `src/render/ttf_font.cpp`.
- `NoteFieldRenderer::last_drawn_quads()` is an existing, narrower counter of the same idea.
- Assumption: solid quads only. Sprites and text stay out of scope: `Texture` is invalid without GL and the text renderer checks `gl_present()`, so those draws stop before they reach the renderer.
- The recording must cost nothing in the real game (`perf_loop_test` gates the frame budget).
- From the 2026-10-13 `/correct` pass.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-41] Add a formatter check for changed lines

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `build`, `infrastructure`
**Source**: `TODO.md:57` — "Add a formatter check for changed lines only: a `.clang-format` (100 columns, matching the current style) and a step in `scripts/check.sh` that fails when a line the change touches is not formatted, with the fix command in the error."
**GitHub**: #142
**Status**: pending

### Description

Four reviews flagged comment or code lines that were wrapped unevenly or ran past the width of their neighbours, and nothing checks it. Add a `.clang-format` that matches the current style, and make `scripts/check.sh` fail when a line that a change touches is not formatted. The existing code keeps its formatting.

### Acceptance Criteria

- [ ] A `.clang-format` at the repo root, 100 columns, matching the current style
- [ ] `scripts/check.sh` fails when a line the change touches is not formatted, and the error prints the command that fixes it
- [ ] The existing code is not reformatted
- [ ] CI and the host use the same clang-format version
- [ ] Formatting three existing files with the new config changes little, with the result shown in the pull request
- [ ] The rule table in `AGENTS.md` gains the row

### Technical Notes

- The host has `clang-format` 23.1.1, `git-clang-format` and `clang-format-diff.py` in `~/.local/bin`. The repository has no `.clang-format`.
- Current style, to match: 4 spaces, attached braces, `const char*` pointer alignment, namespaces not indented, includes left in their written order. 297 lines in `src/` and `tests/` are longer than 100 columns today, which is why only changed lines are checked.
- `git clang-format --diff <base>` checks only changed lines, and needs the base commit. `.github/workflows/ci.yml` checks out with the default depth of 1, so CI has to fetch the base.
- `scripts/check.sh` runs `scripts/lint.sh` as its first step. The new step belongs next to it and should print the rule name, the file and the fix command, like the lint rules.
- CI installs its tools with `apt-get`. Pin the formatter there to the host's version, because two versions can format the same line differently.
- The review findings of this class were in reports 027, 028, 035 and 037. Two of the four lines were under 100 columns, so a column limit alone would not have caught them; comment re-wrapping (`ReflowComments`) decides whether those are caught.
- Assumption: 100 columns. The owner approved it on 2026-10-13 without naming another width.
- From the 2026-10-13 `/correct` pass.

### Dependencies

- Blocked by: None
- Blocks: None

---

## [TODO-42] Stop miniaudio reading freed memory when a sound file fails to load

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Medium
**Phase**: Backlog
**Labels**: `audio`
**Source**: `TODO.md:58` — "miniaudio reads freed memory when a sound file fails to load."
**GitHub**: #144
**Status**: pending

### Description

When a sound file is missing or corrupt, `ma_sound_init_from_file` (called by `SoundStream::load`) reads a data buffer node that miniaudio has just freed. AddressSanitizer reports it as heap-use-after-free in `ma_resource_manager_data_buffer_node_acquire`. The pinned miniaudio 0.11.21 has it, and 0.11.25 (the newest tag) still does. It is not a crash today: the read happens in the same thread right after the free, and the value only gates a branch that is not taken. Under the `asan-ubsan` preset (#136) it stops `audio_test`, `select_screen_test` and `preview_player_test` at their first failed load, so those three pass there only by an expected-report rule.

### Acceptance Criteria

- [ ] Loading a missing or corrupt sound file no longer produces the AddressSanitizer heap-use-after-free report in `ma_resource_manager_data_buffer_node_acquire` (reworded from: "Fix it or route around it")
- [ ] The expected-report rule for `audio_test`, `select_screen_test` and `preview_player_test` is removed from the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt` (reworded from: "then remove the expected-report rule for `audio_test`, `select_screen_test` and `preview_player_test` from the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt`")
- [ ] `ctest --preset asan-ubsan` runs those three tests in full and they pass (reworded from: "so `ctest --preset asan-ubsan` runs those tests in full")
- [ ] `scripts/check.sh` prints `CHECK: PASS` (inferred)
- [ ] The "Known reports" list in `docs/BUILDING.md` (Sanitizer build) no longer names this report (inferred)

### Technical Notes

- Found by `ctest --preset asan-ubsan` (#136). The failing call is `ma_sound_init_from_file` in `SoundStream::load` (`src/audio/sound_stream.cpp`); `src/audio/assist_tick_player.cpp` has the only other call. Reproduce: `UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test`.
- In `miniaudio.h` 0.11.21 the node is freed in the `done:` block of `ma_resource_manager_data_buffer_node_acquire` and its `isDataOwnedByResourceManager` field is read in the next statement. Tags 0.11.22 to 0.11.25 have the same code.
- Options:
  - (a) A pre-check in `SoundStream::load`: open the file with `ma_decoder_init_file`, return false when that fails, `ma_decoder_uninit`, then load as today. Planning for #136 tried this in scratch on unmodified miniaudio: every test passed under the preset, and the leak (TODO-43) was gone too because the failing path was no longer reached. Cost: the game opens each sound file twice on load.
  - (b) A local patch to miniaudio that moves the init-notification block above the free.
  - (c) Report it upstream and wait.
- The rule to remove is the `set_tests_properties(audio_test select_screen_test preview_player_test ...)` call and its comment in the `if(BLAZE_SANITIZE)` block of `tests/CMakeLists.txt`. Once the report is gone, those three tests fail under the preset with "Required regular expression not found" until the rule is removed.
- The audio library drives the music clock (AGENTS.md, principle 1). A miniaudio version change needs the measurements in `docs/AUDIO_LATENCY.md` repeated.
- From the #136 sanitizer run.

### Dependencies

- Blocked by: #136 (the `asan-ubsan` preset)
- Blocks: TODO-43

---

## [TODO-43] Stop miniaudio leaking 744 bytes when a sound file fails to load

**Type**: Technical
**GitHub Label**: technical
**Priority**: Medium
**Complexity**: Small
**Phase**: Backlog
**Labels**: `audio`
**Source**: `TODO.md:59` — "miniaudio leaks 744 bytes each time a sound file fails to load."
**GitHub**: #145
**Status**: pending

### Description

Each time a sound file fails to load, miniaudio 0.11.21 leaks 744 bytes: `ma_sound_init_from_file_internal` does not free its data source when the load fails. LeakSanitizer reports it under the `asan-ubsan` preset (#136), but only once the use-after-free on the same path (TODO-42) no longer stops the test first. miniaudio 0.11.23 fixed it. The leak is small and happens only on a failed load, so it is not a crash.

### Acceptance Criteria

- [ ] A failed sound-file load no longer leaks: LeakSanitizer reports nothing for `audio_test`, `select_screen_test` and `preview_player_test` under `ctest --preset asan-ubsan` (inferred)
- [ ] `ctest --preset asan-ubsan` passes with no expected-report rule for this leak in `tests/CMakeLists.txt` (inferred)
- [ ] If the miniaudio version changes, the measurements in `docs/AUDIO_LATENCY.md` are repeated and recorded there (inferred)
- [ ] `scripts/check.sh` prints `CHECK: PASS` (inferred)
- [ ] The "Known reports" list in `docs/BUILDING.md` (Sanitizer build) no longer names this leak (inferred)

### Technical Notes

- Found by `ctest --preset asan-ubsan` (#136). The failing call is `ma_sound_init_from_file` in `SoundStream::load` (`src/audio/sound_stream.cpp`); `src/audio/assist_tick_player.cpp` has the only other call. Reproduce: `UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test`. The leak report shows only once the use-after-free (TODO-42) is out of the way, because AddressSanitizer stops the test there first.
- Measured in the #136 planning run with the use-after-free patched in a scratch copy: 744 bytes per failed load, 2, 10 and 3 times in `audio_test`, `select_screen_test` and `preview_player_test`.
- Upstream: fixed in miniaudio 0.11.23 ("Fixed a memory leak in the resource manager when opening a file fails"). The pin is `GIT_TAG 0.11.21` in `CMakeLists.txt`.
- Options:
  - The same pre-check as TODO-42, option (a): open the file with `ma_decoder_init_file` in `SoundStream::load` and return false when that fails, so the leaking path is not reached. Planning tried it in scratch: no leak report.
  - Move the miniaudio pin to 0.11.23 or later. The audio library drives the music clock (AGENTS.md, principle 1), so a version change needs the measurements in `docs/AUDIO_LATENCY.md` repeated.
- Assumption: if TODO-42 lands the pre-check, this issue is verified and closed with it.
- From the #136 sanitizer run.

### Dependencies

- Blocked by: #136 (the `asan-ubsan` preset), TODO-42
- Blocks: None
