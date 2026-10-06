# TODO Issues

**Source**: `TODO.md` · **Generated**: 2026-10-02 · **Updated**: 2026-10-07 (TODO-31 added; 2026-10-06: TODO-30 added; 2026-10-05: TODO-27 – TODO-29 added; 2026-10-03: TODO-13 – TODO-14 added; TODO-15 – TODO-26 added from `blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md`, see "Cabinet theme" section)

## Skipped

- **Done (`[x]`)** — 28 items: `TODO.md` lines 1–15, 18–30 (font capitals bug, song-select audio, difficulty order, tab legend, key-repeat scrolling, song list room, attract timeout, legend overlap, song list rendering, results delay, disappearing holds, ITG arrow colors ×2, Cel noteskin, hold-end artifact, colored difficulties, receptor/hit effects, options SFX, assist-tick toggle, calibrate → Esc, auto `songs/` folder, remap background music, white high-score flash, best % in song select, assist-tick timing, options room, remap table layout, bigger receptors).
- **Already tracked** — none on the first run. Second run: `TODO.md:16` (#55, TODO-1), `TODO.md:35` (#60, TODO-7); lines 31–34, 36, 37 are now marked done. Third run: `TODO.md:16` (#55), `:35` (#60), `:40` (#78); lines 36–39 are now marked done. Fifth run (2026-10-05): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–42 are marked done. Sixth run (2026-10-06): `TODO.md:17` (#98), `:35` (#60); lines 1–16, 18–34, 36–45 are marked done. Seventh run (2026-10-07): `TODO.md:17` (#98), `:35` (#60), `:46` (#118, TODO-30); lines 1–16, 18–34, 36–45 are marked done.
- **Deferred by user** — `TODO.md:17` "Something Blaze themed visuals would be cool" (too vague for now; ID TODO-2 left unused). Still deferred on the second and third runs. Fourth run: covered by the Cabinet theme issues (#87–#98).
- **Non-tasks** — none.

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

### Description

Current: mines register as hit far more often than in In The Groove, even when the player isn't deliberately stepping on them. Expected: mines explode only under OpenITG's rules — a step within the mine window when the mine is the closest note, or the column being held at the moment the mine crosses the receptor.

### Acceptance Criteria

- [ ] Root cause is identified and documented on the issue
- [ ] A step aimed at a tap whose column also has a nearby mine judges the tap, matching OpenITG note-selection behavior
- [ ] A held column only explodes a mine at the moment it crosses, not one already in the past (e.g. after a frame hitch)
- [ ] Unit tests in `tests/` cover step-on-mine, hold-through-mine, and tap-next-to-mine cases
- [ ] The mine window value is confirmed against OpenITG source

### Technical Notes

- Step path: `src/gameplay/judgment_engine.cpp:75-94` picks the closest unjudged note **including mines**, then `handle_step_mine` explodes it if within `windows.hit_mine` (0.070 s). Check how OpenITG `Player::Step` chooses between mines and taps.
- Crossing path: `JudgmentEngine::cross_mines` (`judgment_engine.cpp:325`) explodes any not-yet-complete mine with `time <= music_time` while the column is held. Mines stay incomplete until expiry, so a late-held key can still explode a mine that already passed. OpenITG only checks mines crossed since the last update.
- `held_columns` is polled from `ctx.action_down` in `src/screens/gameplay_screen.cpp:58-62`; check that a missed release (focus loss, remap) can't leave a column stuck held.
- Follow AGENTS.md: check OpenITG source before changing semantics.

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

### Description

Question: do Blaze 4k's Fantastic/Excellent/Great/Decent/Way Off windows (and the hold/roll/mine windows) match In The Groove / OpenITG exactly, both as configured values and as applied in the judgment path?

### Acceptance Criteria

- [ ] Each value in `assets/data/judgment_constants.json` `windows_seconds` is checked against OpenITG source, with file:line references recorded on the issue
- [ ] `judge_window_scale` / `judge_window_add` defaults are checked against OpenITG's preference defaults (e.g. any `TimingWindowAdd`)
- [ ] It is confirmed whether window edges are inclusive or exclusive as in OpenITG, and that the global offset is applied once
- [ ] Any mismatch has a follow-up bug filed (or is fixed directly with a unit test if trivial)

### Technical Notes

- Values: `assets/data/judgment_constants.json` (claims OpenITG `f2c129fe` metrics.ini); compiled defaults in `src/timing/judgment_constants.cpp:20` and nearby; classification in `JudgmentConstants::classify_tap`.
- Current values: Fantastic 21.5 ms, Excellent 43 ms, Great 102 ms, Decent 135 ms, Way Off 180 ms, Mine 70 ms, Hold 320 ms, Roll 350 ms.
- Related: the mine window feeds TODO-3.

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

### Description

Question: why does the best global offset change between Bluetooth and wired headphones, and why do OBS recordings drift out of sync? Then propose a fix so switching output devices doesn't require manual recalibration.

### Acceptance Criteria

- [ ] Root cause is documented: whether the music clock reflects the decoder cursor rather than what is actually heard, and how big the per-device latency is
- [ ] Explain why the OBS capture is out of sync even though the in-game feel is fine (game is calibrated for the Bluetooth path; OBS captures pre-output audio and video with no device latency)
- [ ] At least one solution is evaluated, e.g. per-output-device saved offsets keyed by device name, or querying device latency from miniaudio / PipeWire, with tradeoffs written down
- [ ] Follow-up implementation issues are filed for the chosen approach

### Technical Notes

- Clock source: `SoundStream::get_position_frames` (`src/audio/sound_stream.cpp:138`) uses `ma_sound_get_cursor_in_pcm_frames`, which is the engine/decoder position and excludes device and Bluetooth codec latency (often 150–300 ms for BT vs ~10–30 ms wired).
- Clock + single global offset: `src/timing/music_clock.hpp` (`time = frames / rate + global_offset`); calibration in `src/timing/offset_calibration.cpp`; persisted in `src/data/config.hpp`.
- Device enumeration/selection in `src/audio/audio_engine.cpp`; miniaudio exposes device info/IDs that could key a per-device offset table.
- AGENTS.md: the music clock stays the single timing source. Any latency compensation goes into the offset, never into wall-clock or frame-delta logic.

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

### Description

Current: Cel receptors show stray white/light pixels around their edges against the song background. Expected: receptor edges blend cleanly into the background with no halo.

### Acceptance Criteria

- [ ] Receptors render without a visible light fringe over dark and bright song backgrounds
- [ ] Tap notes, holds, mines, and explosions don't gain or keep a similar fringe after the fix
- [ ] Receptor flash/brightness behavior (beat pulse, press glow) is unchanged
- [ ] Build passes and existing render tests pass

### Technical Notes

- Likely cause: straight (non-premultiplied) alpha with `GL_LINEAR` / mipmap filtering (`src/render/texture.cpp:98-100`). Fully transparent texels with white RGB bleed into edge samples. Usual fixes: premultiply alpha at load time and blend with `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`, or bleed edge colors into transparent texels.
- Changing only the receptor blend mode (`BlendMode` in `src/render/geometry.hpp`, applied in `src/gameplay/note_field_renderer.cpp:30`) would likely hide the halo but alter the look. Check the texture first.
- Receptor art: `assets/noteskins/cel/_Down Receptor tex 4x1 (res 256x64).png`, loaded in `src/gameplay/noteskin.cpp:24`.

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

### Description

The options menu's X-mod list stops at 6x. Extend it up to 8x so fast readers can pick higher multipliers.

### Acceptance Criteria

- [ ] The options menu X-mod cycle includes 7x and 8x after 6x
- [ ] Selecting 7x/8x is saved to config (`gameplay.speed_mod`) and restored on next launch
- [ ] Notes scroll at the selected multiplier in gameplay, and wrapping from 8x back to 1x works
- [ ] Options-menu unit tests cover the new values

### Technical Notes

- Change `kXModValues` in `src/screens/options_menu.cpp:40` (currently `{1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0}`).
- `parse_speed_mod` (`src/gameplay/speed_mod.cpp`) accepts any finite positive value, so no parser change is expected.
- Assumption: add whole-number steps (7x, 8x). Half steps such as 3.5x are not added unless requested.

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

### Description

Current: the end cap of holds and rolls is drawn past the tail position, so the visual end of the hold reaches the receptor after the moment it can be released. Expected: the drawn end of the hold lines up with the release time, as in OpenITG with the Cel noteskin.

### Acceptance Criteria

- [ ] The visible end of a hold/roll (body + cap) reaches the receptor at the note's tail time, in both up and down scroll
- [ ] Rolls get the same fix
- [ ] Hold bodies don't develop a gap or seam between the body and the cap
- [ ] Verified visually against the metronome sync-test chart (or a hold test chart) at 1x and a high X-mod

### Technical Notes

- `draw_hold` in `src/gameplay/note_field_renderer.cpp:37-80` draws the body from head to `tail_y` and then the cap starting **at** `tail_y` and extending a full `width` beyond it (`cap_top = reverse ? tail_y - width : tail_y`). That adds about one note size of length past the tail.
- Check how OpenITG/StepMania `NoteDisplay::DrawHoldBottomCap` positions the cap relative to the tail (and the Cel noteskin's metrics), and match it, e.g. end the body earlier or center/offset the cap.
- Tail y comes from `field.screen_y(item.tail_offset)` (`note_field_renderer.cpp:118`).

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

### Description

The life bar is drawn as a horizontal bar at the bottom center of the screen, where it gets in the way of the incoming arrows. Move it to the left side of the screen and draw it vertically so it stays out of the note field.

### Acceptance Criteria

- [ ] During gameplay the life bar is drawn vertically on the left side of the screen and fills from bottom (empty) to top (full)
- [ ] The bar doesn't overlap the note field, receptors, combo, or judgment text at common window sizes (e.g. 1280x720, 1920x1080, and a narrow 4:3 window)
- [ ] Fill level, the danger color below 30% life, and the frame/background styling still work as before
- [ ] The bottom-center grade/score text keeps its current position and isn't clipped or overlapped
- [ ] Build passes and existing gameplay/HUD tests pass

### Technical Notes

- All life bar drawing is in `HudRenderer::render_life` (`src/gameplay/hud_renderer.cpp:165`). It currently uses `bar_w = min(width * 0.40, 480)`, `bar_h = 16` and sits just above the grade text. Swap the axes, anchor the bar to a left margin, and size its height from the screen height.
- The note field is centered (`field_left = (screen_w - field.field_width()) * 0.5` in `src/gameplay/note_field_renderer.cpp:144`), so a left-margin bar clears it unless the window is very narrow. Clamp the bar to stay left of `field_left`.
- Called from `src/gameplay/gameplay_view.cpp:327`. Palette constants live at `hud_renderer.cpp:24-30`.
- Assumption: "left hand side" means the left edge of the screen, not right next to the note field. Scroll direction (reverse) doesn't change where the bar sits.

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

### Description

Current: the bitmap font only has glyphs for `0-9`, `A-Z`, `a-z`, space, and `. % - + * [ ]`, so any other character in a song title or artist is silently skipped. ASCII punctuation (`'`, `!`, `?`, `&`, `(`, `)`, `:`, `/`, `,`, …) disappears, and non-ASCII UTF-8 titles (accents, Japanese) render as gaps or nothing. `text_width` also counts bytes, not characters, so layouts with multi-byte titles are off. Expected: song titles and artists show readably in song select, including common punctuation, with a sensible fallback for characters the font can't draw.

### Acceptance Criteria

- [ ] Common printable ASCII punctuation (at least `' ! ? & ( ) : / , " # _ = ~`) renders in song select titles and artists, e.g. "Don't Promise Me" shows its apostrophe
- [ ] UTF-8 text is decoded per code point: characters the font can't draw show a visible placeholder (or a transliterated/ASCII-folded form), and a multi-byte character takes up one cell, not one per byte
- [ ] When the simfile has `#TITLETRANSLIT` / `#ARTISTTRANSLIT` and the original title has characters the font can't draw, the transliterated text is shown instead
- [ ] Malformed UTF-8 in a simfile title doesn't crash or hang rendering
- [ ] Unit tests cover glyph lookup for punctuation, `text_width` on multi-byte input, and the translit fallback

### Technical Notes

- Glyph table and lookup: `kGlyphs` / `glyph_for` in `src/render/bitmap_font.cpp:17-97`. `draw_text` iterates `char`s and skips unknown glyphs; `text_width` returns `text.size() * 6 * pixel`.
- Song select draws titles at `src/screens/select_screen.cpp:612` (info panel) and `:644` (song list). The fix in `bitmap_font.cpp` also covers the results and gameplay screens.
- Translit fields are already parsed (`src/chart/simfile_parser.cpp:82-87`, `src/chart/song_metadata.hpp:11-13`) but not used for display.
- Related: #55 (TODO-1) replaces the bitmap font with a TrueType atlas. Glyph coverage there decides how much non-ASCII text can render natively. This bug can be fixed now in the bitmap font, but keep the UTF-8 decoding and fallback logic font-agnostic so it carries over.
- Simfile text is untrusted input (AGENTS.md), so UTF-8 decoding must be bounds-checked.

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

### Description

Question: what should the Blaze 4k splash/title screen look like? Right now it is just "BLAZE 4K" in the 5x7 bitmap font with a row of four flat colored squares and "PRESS START". Pick a concrete visual direction and split the work into follow-up issues.

### Acceptance Criteria

- [ ] A visual direction is chosen and recorded on the issue (e.g. logo art or a styled logotype, background, motion/animation, color palette), with a mockup or reference images
- [ ] Asset needs are listed with their sources and licenses (original art, or OFL/CC-licensed fonts and images that can be redistributed)
- [ ] The direction stays within PRD scope (2D textured quads only; no video or 3D backgrounds)
- [ ] Follow-up implementation issues are filed and linked from this issue

### Technical Notes

- Current screen: `src/screens/title_screen.cpp` (logo text at `:52`, receptor-colored squares at `:54-66`, "PRESS START" at `:68`). The attract screen (`src/screens/attract_screen.cpp`) shares the Title → Attract loop and should match the new look.
- Rendering is limited to textured quads through `GlQuadRenderer` (`src/render/gl_quad_renderer.hpp`). Logo art would load like other textures via `src/render/texture.cpp` / `texture_cache.cpp` from a new `assets/` subfolder.
- Related: #55 (TODO-1, TrueType font) affects how the logotype and "PRESS START" can be styled. `TODO.md:17` ("Blaze themed visuals", deferred) may share the same visual direction.
- Assumption: "splash screen" means the title screen ("BLAZE 4K" / "PRESS START"), not a separate startup/loading splash.

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

### Description

During a song the HUD draws the current live grade (e.g. "A-", "**") at the bottom centre of the screen. Remove it so the bottom of the playfield is free; the grade still appears on the results screen.

### Acceptance Criteria

- [ ] No grade text is drawn anywhere on the gameplay screen during play
- [ ] Percent, combo, judgment-count chips, and the life bar still render where they do today
- [ ] The results screen still shows the final grade unchanged
- [ ] Tests that check the bottom-centre grade layout are removed or updated, and the build and `ctest` pass

### Technical Notes

- The grade is drawn in `src/gameplay/hud_renderer.cpp:180-185` ("Bottom-centre: live grade"), positioned by `grade_text_rect` (`:50`).
- `grade_text_rect` is declared in `src/gameplay/hud_renderer.hpp:29-33` and checked by `test_grade_text_clear` in `tests/hud_renderer_test.cpp:152`. Drop both if nothing else uses them.
- Keep `format_grade` (`hud_renderer.cpp:84`) if the results screen or tests still use it. Keep `state.grade` in the HUD state if other code reads it.
- Assumption: remove the grade outright. A toggle in Options wasn't asked for.

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

### Description

Edit charts carry a custom name in the simfile, but song select and results only show the label "Edit". A song with several edits shows them as identical rows. Show the chart's own name for Edit charts, and fall back to "Edit" when the name is empty.

### Acceptance Criteria

- [ ] In song select, an Edit chart's row shows its chart name (with meter and best % as today) instead of "Edit"
- [ ] The results screen difficulty line shows the same name for Edit charts
- [ ] An Edit chart with an empty name still shows "Edit"; non-Edit difficulties (Beginner…Challenge) display exactly as before
- [ ] A long name is shortened so it doesn't overlap the meter or best-% columns
- [ ] Parser/select tests cover an `.sm` and an `.ssc` fixture with a named Edit chart

### Technical Notes

- The name is already parsed into `Chart::description` (`src/chart/chart.hpp:79`). SM: the 2nd `#NOTES` field (`src/chart/simfile_parser.cpp:170`). SSC: `#CHARTNAME` / `#DESCRIPTION` (`simfile_parser.cpp:136`). Both SSC tags currently write the same variable, so whichever appears last wins. Check StepMania 5 source (`NotesLoaderSSC.cpp`, how `Steps` description vs. chart name is shown for edits) for which field is the edit name before changing that precedence.
- Display sites: `src/screens/select_screen.cpp:675` (difficulty row) and `src/screens/results_screen.cpp:153-156` (difficulty line). A small shared helper (e.g. next to `song_display_title` in `src/screens/song_display_text.cpp`) avoids duplicating the fallback logic. Run names through the same UTF-8 text handling as song titles (#77).
- Keep `difficulty_color` keyed on `chart.difficulty` ("Edit") so edits keep their color.
- High-score keys (`src/data/high_scores.cpp:150`) already include a note-data hash, so separate Edit charts don't share scores. No change needed there.

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

### Description

The Cabinet theme pack sits untracked in `blaze4k-cabinet-theme/`. Copy its assets, fonts, `theme.hpp` and design docs into the repository layout, so the engine and screen issues can build on tracked files.

### Acceptance Criteria

- [ ] `assets/fonts/` holds Audiowide + Saira Condensed (Medium, Bold, ExtraBold) `.ttf` files, with both OFL licence texts next to them
- [ ] `assets/theme/cabinet/` holds all 63 textures, `manifest.json` and the two bitmap digit atlases, and a build copies them next to the binary with no CMake change
- [ ] `src/render/theme.hpp` is in the tree and compiles as part of the build (included by at least one TU or a compile-only test)
- [ ] The plan, README and `reference/` mock-ups are kept under `docs/` (e.g. `docs/cabinet-theme/`), and the untracked `blaze4k-cabinet-theme/` folder is removed
- [ ] Fresh clone + `cmake -B build && cmake --build build && ctest --test-dir build` passes

### Technical Notes

- `CMakeLists.txt:161-167` already copies the whole `assets/` folder post-build.
- `theme.hpp` depends only on `render/geometry.hpp` (`Color`, `Rect`).
- No code uses the assets yet. Loading is TODO-17 (textures) and TODO-18 (fonts).
- Assumption: design docs go under `docs/cabinet-theme/`. Later issues refer to the pack by that path.
- Mention the font licences in the top-level README/LICENSE notes if the repo lists third-party assets.

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

### Description

The theme needs parallelograms (meter ticks, judgment bars, chips), per-corner gradients and sheared italic glyphs. The renderer only draws axis-aligned or rotated rects with one colour, and every texture is clamp-to-edge. Add a four-corner quad entry point and an optional repeat wrap mode for textures.

### Acceptance Criteria

- [ ] `GlQuadRenderer::draw_quad_points(corners, texture, uv, colours)` draws a quad from four corners (TL, TR, BR, BL), each with its own colour, through the existing batch
- [ ] `Texture::from_file(path, mipmaps, wrap)` accepts an optional `Wrap::Repeat` (default stays clamp), plus a nearest-filter option for `scanlines.png`
- [ ] Existing draw calls render exactly as before (no change to `draw_quad` / `draw_textured_quad` output)
- [ ] Unit tests cover corner order, per-vertex colour (premultiplied like the other paths) and UV mapping for a repeat-wrapped UV range > 1
- [ ] Headless (uninitialized) renderer stays a safe no-op for the new call

### Technical Notes

- `src/render/gl_quad_renderer.hpp`: add next to `draw_textured_quad`. Reuse `append_quad`'s vertex layout (x, y, u, v, r, g, b, a) and the texture-change flush. Colours must go through the same `premultiply` path (#59).
- `src/render/texture.cpp:105-108` hard-codes `GL_LINEAR` and `GL_CLAMP_TO_EDGE`. Thread the wrap and filter choices through `from_file` / upload.
- Tests in the style of `tests/texture_test.cpp` and `tests/note_field_renderer_test.cpp`.
- Keep `BlendMode::Add` for explosions. No new shaders.

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

### Description

Screens should draw Cabinet textures by name, without hard-coding pixel offsets. Add a theme texture module that loads `manifest.json` and every PNG once at startup. It provides draw helpers for each manifest `kind` and draws numbers with the two baked digit fonts.

### Acceptance Criteria

- [ ] `assets/theme/cabinet/manifest.json` is parsed with nlohmann/json, and every listed PNG is loaded once at app init (mipmaps for grades, judgments and logo; repeat wrap for `tile` kinds)
- [ ] Helpers exist and position the manifest's `content_px` box, not the padded image: `draw_sprite`, `draw_slice3`, `draw_slice9`, `draw_frame`, `draw_tiled`, `draw_fill_cropped` (UV-cropped so the life gradient doesn't slide)
- [ ] `BitmapDigits` draws strings from `digits_chrome` / `digits_white` (`0-9 . % /` and space) using each glyph's rect, `origin_x` and `advance`, with left, centre and right alignment
- [ ] A missing or unreadable PNG or manifest logs once and falls back to a flat quad. A malformed manifest never crashes, and headless tests keep running
- [ ] Unit tests cover manifest parsing, content-box placement, slice3/slice9 rect math, the cropped-fill UVs and digit layout widths

### Technical Notes

- New `src/render/theme_textures.{hpp,cpp}`. Load with `Texture::from_file` (wrap mode from TODO-16).
- Textures are @2x for 1280x720. Draw size = `size_px / 2 * s`, where `s` is the layout scale (TODO-19). Take `s` as a parameter so this issue doesn't need TODO-19.
- Load at app init (`src/app/app.cpp`, where other assets and the judgment constants load) and pass to screens through `ScreenContext`, the way other shared services reach screens.
- Fallback spirit: the noteskin's optional explosions (`src/gameplay/noteskin.cpp`).
- The manifest is local, trusted data, but treat it defensively anyway (bounds-check rects against image size).

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

### Description

All runtime UI text in the Cabinet design (song titles, artists, meters, hints, combo) uses Saira Condensed with tracking, synthetic italic and hard drop shadows. Add a TrueType font atlas and text renderer built on stb_truetype that implements `theme::TextStyle`, and keep today's UTF-8 safety rules.

### Acceptance Criteria

- [ ] `FontAtlas` bakes printable ASCII, Latin-1 and Latin Extended-A per (font file, pixel size) with `stbtt_PackFontRanges` (2x2 oversampling), uploaded as white RGBA with coverage in alpha so tinting and premultiplied blending work
- [ ] `TextRenderer::measure` / `draw` apply a `TextStyle`'s tracking, `kItalicShear` (plus an extra group shear), and `Shadow::Hard2/3` (black copy offset down, then the colour), with left, centre and right alignment
- [ ] UTF-8 goes through `unicode_text` (zero-width skipped, malformed bytes → U+FFFD), and code points the font lacks draw the placeholder box. A fuzz-style test with malformed input doesn't crash
- [ ] Atlases bake at `size_px * window_height / 720` and re-bake only when the window height changes. Measure-based `...` truncation replaces `truncate_to_cells`, with its tests ported
- [ ] A missing or corrupt `.ttf` logs once and falls back (bitmap font or no-op) instead of crashing

### Technical Notes

- New `src/render/ttf_font.{hpp,cpp}`. stb is already fetched (`CMakeLists.txt` stb FetchContent). Add `stb_truetype` / `stb_rect_pack` implementation defines in one TU, like `src/render/stb_image_impl.cpp`.
- Fonts and styles: `theme::kFontFiles`, `theme::text::*` in `src/render/theme.hpp`. Expect about 15 (font, size) pairs. Draw italic glyphs with `draw_quad_points` (TODO-16).
- UTF-8 helpers: `src/render/unicode_text.hpp` (`next_code_point`, `is_zero_width`, `fold_to_ascii`). Current truncation contract: `truncate_to_cells` in `src/render/bitmap_font.hpp`. Tests: `tests/bitmap_font_test.cpp`, `tests/unicode_text_test.cpp`.
- Keep `bitmap_font` for debug overlays until no screen uses it. Deleting it belongs to TODO-26 or later.
- This is the implementation of #55 (TODO-1, "Replace the 5x7 bitmap UI font with a TrueType font").

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

### Description

Every `theme::layout` value is in a 1280x720 reference space. Add one helper that every screen uses to map reference coordinates to the window: scale `s = window_height / 720` and a `1280·s`-wide content column centred horizontally.

### Acceptance Criteria

- [ ] A helper returns `s` and the content column origin for a given window size, and maps reference `Rect`s and points to screen pixels
- [ ] At 1280x720 the mapping is identity. At 2560x1440 it's a 2x scale. At ultrawide sizes the column is centred, with extra background on both sides
- [ ] Unit tests cover 720p, 1440p, 16:10 and 21:9 window sizes
- [ ] Degenerate sizes (0 or negative width/height) return a safe result and don't divide by zero

### Technical Notes

- Put it next to `theme.hpp` (e.g. `src/render/theme_layout.hpp`), using `theme::layout::kRefWidth/kRefHeight`.
- Screens already centre the note field this way. Match that behaviour (`src/gameplay/gameplay_view.cpp`).
- Text atlas baking (TODO-18) and texture draw size (TODO-17) both use the same `s`.

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

### Description

Replace the bitmap-text "BLAZE 4K" and flat coloured squares with the Cabinet title from `reference/cabinet-v3-title.png`: background, chrome logo, subtitle, four Cel tap notes, PRESS START plate, footer and scanlines. Give the attract screen the same background and logo.

### Acceptance Criteria

- [ ] Title draws `bg_title`, `logo`, `subtitle`, four Cel tap notes (rotated per column) at `kTitleArrowsTop`, `press_start` blinking at 1 Hz as now, and the `scanlines` overlay
- [ ] Footer shows "SINGLE · 4 PANEL" on the left and "BLAZE 4K v<version>" on the right in `text::kFooter`. The version comes from a `BLAZE4K_VERSION` compile definition set from CMake `project(VERSION …)`
- [ ] Attract uses the same background and logo, and keeps its pulse and receptor-blink logic with Cel receptors
- [ ] Side-by-side check against `reference/cabinet-v3-title.png` at 1280x720 and 2560x1440 matches layout and crispness
- [ ] Screen transitions and timings (Confirm → Select, attract timeout) are unchanged, and existing screen tests pass

### Technical Notes

- Files: `src/screens/title_screen.cpp` (currently `draw_text_centered("BLAZE 4K")` + four `draw_quad`s), `src/screens/attract_screen.cpp`.
- Tap notes: reuse `NoteSkin::head` (`src/gameplay/noteskin.hpp`) with a fixed beat per column. The mock shows a different quantization colour per arrow (red/blue/purple/yellow). The `NoteSkin` instance today lives in `gameplay_view.cpp`, so the title needs access to one (e.g. through `ScreenContext`).
- `CMakeLists.txt:3` has `project(blaze-4k VERSION 0.1.0 …)`. Add `target_compile_definitions(... BLAZE4K_VERSION="${PROJECT_VERSION}")`.
- Answers #78 (TODO-12, splash-screen visual direction).

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

### Description

Match `reference/cabinet-v3-gameplay.png`. Remove the live percent and per-window judgment chips. Replace the life bar with the chrome frame and gradient fill, add the difficulty badge top-left, and draw judgments with the baked `judgment_*` sprites plus a sheared combo line. Leave the note field untouched.

### Acceptance Criteria

- [ ] The live percent, the judgment chips and their per-window colours are no longer drawn. Scoring, combo and life still update (the score keeper is untouched)
- [ ] Life bar = `life_frame` (9-slice) + `life_fill`, or `life_fill_danger` below 30% (`kLifeDangerThreshold`), cropped with stable colours + `life_stripes` tiled, keeping `layout_life_bar`'s field-clearance logic
- [ ] `diff_badge` at `kDiffBadge` is tinted by `theme::difficulty`, with e.g. "HARD 8" in the difficulty's ink colour
- [ ] Judgment pop draws `judgment_<kind>` with the current scale and fade curve, and the combo (`kComboNumber` + `kComboLabel` "COMBO") sits under it at `kComboTop`, with the group sheared −10°
- [ ] Note field, receptors, Cel noteskin and background dim are unchanged. `hud_renderer_test` and `gameplay_screen_test` are updated for the removed elements and pass

### Technical Notes

- `src/gameplay/hud_renderer.cpp`: the percent (`percent_text_rect`, `format_percent` around :113) and the chips (`draw_chip` around :123-142) go. `kLifeBarMinInset` exists to keep the frame below the percent text. Revisit it once the percent is gone, and start from `theme::layout::kLifeBar`.
- `src/gameplay/judgment_animator.cpp:139-150`: swap `draw_text_centered(pop_label_)` for the sprite, and move the combo from the top of the screen (y = 8) to under the judgment.
- Judgment sprite names: fantastic, excellent, great, decent, wayoff, miss, ok, ng, mine (`manifest.json`).
- Difficulty label for Edit charts: show the chart name (#84) or "EDIT", using `theme::difficulty::kEdit`.
- Timing rule: all animation stays on fixed `dt`, and nothing touches the music clock.
- Draw order for batching: badge/frames → fill → stripes → judgment sprite → text.

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

### Description

Rebuild the song select presentation to match `reference/cabinet-v3-select.png`: top bar with SELECT MUSIC and mod chips, framed banner with title/artist/BPM, slanted difficulty rows with meter ticks and best %, a slanted wheel with pack header, and the gold-key hint bar. Navigation logic stays as it is.

### Acceptance Criteria

- [ ] Draws `bg_select`, `bar_top` + `title_select_music`, and speed/scroll chips (`chip` tinted `kCyan` / `kGreen`, with text such as "SPEED 2.5x", "UPSCROLL")
- [ ] Banner draws inside `banner_frame` (`banner_fallback` when the song has none), with title (`kSongTitle`), artist (`kArtist`) and BPM (`kBpm`) below, and long titles truncated by measured width
- [ ] Difficulty rows use `diff_row_<name>[_selected]` with name, meter, 10 × `diff_tick` (filled up to the meter in the difficulty colour, else `kTickOff`) and best % ("---" when none). Edit charts get a neutral row with their chart name (#84)
- [ ] Wheel uses `wheel_pack`, `wheel_row` and `wheel_row_selected` with `kWheelIndent` by distance from the selection, shows no artist on the wheel, and scrolls with an eased ~80 ms vertical slide on fixed `dt`
- [ ] Hint bar shows "↑↓ SONG · ←→ DIFFICULTY · ENTER PLAY · TAB OPTIONS · ESC TITLE" with gold keys. Existing `select_screen_test` behaviour (window logic, key repeat, preview audio) still passes

### Technical Notes

- `src/screens/select_screen.cpp` (728 lines). Difficulty rows around :675-690, wheel window around :647 ("window is 13 rows tall"). The plan says keep the 13-row window logic, but at the new row heights (62 px rows, 92 px selected, 10 px gap) only about 7–8 rows fit in 720p. Keep the windowing algorithm and set the visible count from layout.
- The font has no ↑↓←→ glyphs. Draw the arrow keys as small quads or noteskin arrows, or add them to an atlas (assumption, check against the mock).
- Diff row textures exist for beginner/easy/medium/hard/challenge only. For Edit, draw a `draw_quad_points` parallelogram in `theme::difficulty::kEdit` (assumption).
- Long song titles and non-Latin scripts keep the placeholder behaviour from #77.
- Batch order: background → bars/frames → rows → text per atlas → overlays (≈10–25 flushes).

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

### Description

Rebuild the results screen to match `reference/cabinet-v3-results.png`: SCORE SCREEN top bar with difficulty badge, title and artist, stat panels on the left, grade medallion in the centre, chrome percentage with record/failed ribbon, and judgment bars on the right. Map the existing reveal animation onto the new parts.

### Acceptance Criteria

- [ ] Draws `bg_results`, `bar_top` + `title_score_screen`, and badge ("HARD 8"), song title and artist right-aligned in the bar
- [ ] Left stat panels (`stat_panel` + label + `digits_white`): MAX COMBO, DANCE POINTS "n / max", and HOLDS OK / NG / MINES
- [ ] Centre: `medallion` + `grade_<tier>` (e.g. `quad_star` → `grade_quad_star`, `S+` → `grade_S_plus`) + tier label ("ONE STAR"), the percentage in `digits_chrome` counting up as now, and `record_ribbon` on a new record or `failed_ribbon` on fail
- [ ] Right: six judgment rows (FANTASTIC…MISS) with labels in judgment colours, `draw_quad_points` track (`kBarTrack`) + fill proportional to count, and right-aligned counts. Hint bar shows "ENTER CONTINUE"
- [ ] Reveal: title fades in, grade slams 2.4x → 1x on the medallion, stats fade, ribbon pops (white flash on NEW RECORD kept). `results_screen_test` / `results_anim_test` are updated ("SCORE SCREEN") and pass

### Technical Notes

- Files: `src/screens/results_screen.cpp` (today `draw_text("RESULTS")`, `grade_color`, FAILED text around :225-232), `src/screens/results_anim.cpp`.
- Grade labels come from `JudgmentConstants::grade_tiers` (`src/timing/judgment_constants.cpp:49-66`). Map the label to the texture name (`+` → `_plus`, `-` → `_minus`). There's no F texture. On fail, show the earned tier with `failed_ribbon` (the current screen shows grade + FAILED), unless the owner wants otherwise.
- Bar fill fraction: count / total judged taps (assumption, the mock's bar lengths are consistent with this). Draw the bars with `draw_quad_points` and `skew::kRows`.
- `digits_*` glyphs are `0-9 . % /` and space only. Format numbers so no other characters reach `BitmapDigits`.
- Medallion: `kMedallion`. Percent: `kPercentTop`. Ribbon: `kRecordRibbon`.

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

### Description

The options overlay wasn't in the mock-ups. Restyle it with the parts song select already uses (scrim, plate header, slanted rows, gold selection, gold values), so it no longer looks like the old bitmap UI on top of the new select screen.

### Acceptance Criteria

- [ ] Opening options dims the select screen with `theme::color::kGameplayScrim` (72% black) and shows a centred panel with a `bar_top` / `stat_panel`-style header
- [ ] Each option (speed type, speed, scroll, assist tick, calibrate offset, remap input, …) is a slanted row (`diff_row`- or `wheel_row`-style), with the selected row in gold (`wheel_row_selected`) and values right-aligned in `kGold`
- [ ] All rows fit at 1280x720 without overlapping the hint legend (no regression of the earlier "options room" fixes)
- [ ] Option behaviour, sounds and transitions (to Calibrate / Remap and back with Esc) are unchanged, and `options_menu_test` passes

### Technical Notes

- The options UI lives in `src/screens/options_menu.cpp` (the plan says "same file" as select, but it's its own file). Row labels are around :238-250.
- Reuse the layout helper (TODO-19), text styles (`kWheelRow`, `kWheelSelected`, `kHintKey`) and texture helpers from TODO-17/18.
- Free design: not in the mock-ups. Keep it consistent with select.

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

### Description

Input Remap and Calibration weren't in the mock-ups and still use bitmap text. Give them `bg_select`, the top bar with a runtime title, slanted rows and the hint bar, so every screen in the loop shares the Cabinet look.

### Acceptance Criteria

- [ ] Both screens draw `bg_select`, `bar_top` with a runtime title ("REMAP INPUT" / "CALIBRATE OFFSET") in a Saira style, and a hint bar with gold keys
- [ ] Remap shows the binding table as slanted rows (action, bound keys), with the selected row in gold and "RESET TO DEFAULTS" as a row
- [ ] Calibration shows its phase word (GET READY / TAP ON THE BEAT / DONE), sample count and offset in the new type, centred and legible at 720p and 1440p
- [ ] Input handling, calibration math and Esc-back-to-options behaviour are unchanged. `input_remap_screen_test` and `calibration_screen_test` pass

### Technical Notes

- Files: `src/screens/input_remap_screen.cpp` (title at :162, reset row at :208), `src/screens/calibration_screen.cpp` (title at :146, phase words at :149-155, samples/offset at :165-174).
- Optional, from the plan: bake chrome `GET READY` / `TAP ON THE BEAT` / `DONE` sprites in the `judgment_*` style. Not required for this issue, since runtime text is fine.
- Calibration must not change its timing path. The UI is presentation only (principle 1).
- Related: #74 (calibration offset bug) is independent of this restyle.

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

### Description

The parser already reads `#SUBTITLE` / `#SUBTITLETRANSLIT`, but no screen draws them, so songs that share a title (e.g. the several "Disconnect" songs) look identical in the song wheel. Draw the subtitle wherever the song title is shown so these songs can be told apart.

### Acceptance Criteria

- [ ] Wheel rows for songs with a non-empty `#SUBTITLE` show the subtitle next to or under the title, so songs sharing a title are visibly different
- [ ] The song info panel on the select screen shows the subtitle with the title
- [ ] The results screen title bar shows the subtitle too
- [ ] The subtitle uses `#SUBTITLETRANSLIT` when the active font can't draw the native text, using the same rule as title and artist
- [ ] Long title + subtitle text is truncated by measured width and never overlaps other wheel or panel elements. Songs without a subtitle look the same as today

### Technical Notes

- Data is already there: `SongMetadata::subtitle` / `subtitle_translit` (`src/chart/song_metadata.hpp`), filled in `src/chart/simfile_parser.cpp:96-103`.
- Add `song_display_subtitle(...)` next to `song_display_title` / `song_display_artist` in `src/screens/song_display_text.{hpp,cpp}` (same `select_display_text` coverage rule, both bitmap and TrueType overloads).
- Draw sites: wheel labels `src/screens/select_screen.cpp:~653` → `select_art::draw_wheel`; info panel `select_screen.cpp:~596` → `select_art::draw_song_info` (`src/screens/select_art.cpp:~476`); results `src/screens/results_screen.cpp:~110` / `~358`.
- Reference: SM5 `TextBanner` / `MusicWheelItem` show the subtitle as a smaller second line under the main title. Assumption: follow that here (a smaller second line, or `Title (subtitle)` on one line if a row has no room for two). Pick whichever fits the Cabinet wheel layout cleanly.
- Display only. High-score keys (`make_chart_key`) are based on chart notes and don't change.
- Add unit tests for the subtitle display choice next to the existing `song_display_text` tests.

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

### Description

The judgment pop (Fantastic, Excellent, Great, Decent, Way Off, Miss) is too large on the gameplay screen and gets in the way. Scale it down to about half its current size.

### Acceptance Criteria

- [ ] The judgment sprite is drawn at about 50% of its current size at rest, at both 1280x720 and 2560x1440
- [ ] The pop animation keeps the same shape (eases up to ~1.25x, settles to 1.0x, then fades) relative to the new smaller size
- [ ] The smaller judgment stays centred on the note field and doesn't overlap the combo line below it. Move `kJudgmentTop` / combo position only if needed
- [ ] The bitmap fallback label (no theme / missing sprite) is scaled down the same way
- [ ] `tests/judgment_animator_test.cpp` is updated for the new size and passes

### Technical Notes

- Sizing: `JudgmentAnimator::render_judgment` / `judgment_pop_rect` in `src/gameplay/judgment_animator.cpp:~159-210` draw the `judgment_<kind>` sprite at `L.s * scale`. Add a named constant for the display scale (e.g. `kJudgmentDisplayScale = 0.5f`) in `src/gameplay/judgment_animator.hpp` and apply it to both the sprite and `kJudgmentPopPixel`. Don't edit the sprite art.
- Layout constants: `kJudgmentTop` in `src/render/theme.hpp:209`. `kJudgmentContentRef` (444x66 ref px) describes the sprite content box.
- "Perhaps 50%" is the owner's estimate. Keep the factor in one constant so it's easy to change after a playtest.
- Presentation only: nothing touches the music clock or judgment path.

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

### Description

The mouse cursor stays visible over the note field during play. Hide it while a song is being played and bring it back when leaving gameplay.

### Acceptance Criteria

- [ ] The mouse cursor is hidden while the gameplay screen is active, in both windowed and fullscreen modes
- [ ] The cursor is visible again on the results screen and every other screen after gameplay ends, is quit, or fails
- [ ] Pausing (and the pause window, #60, once it exists) doesn't leave the cursor stuck hidden after leaving gameplay
- [ ] Headless / unit-test paths (no window) don't crash. The cursor call is null-guarded like other `ScreenContext` services
- [ ] Builds on Linux, Windows and macOS

### Technical Notes

- SDL3: `SDL_HideCursor()` / `SDL_ShowCursor()`. Keep SDL out of `src/screens/` (AGENTS.md "Thin platform wrapper"): add a small method on the window wrapper (`src/app/window.{hpp,cpp}`, e.g. `set_cursor_visible(bool)`) and expose it to screens through `ScreenContext` (`src/screens/screen.hpp`) as a nullable pointer or `std::function`, like `action_down`.
- Hide in `GameplayScreen::enter` and show in `GameplayScreen::exit` (`src/screens/gameplay_screen.cpp`). Assumption: hide during gameplay only, since the task says "when playing". Menus don't use the mouse either, so hiding it everywhere would also work if the owner prefers.
- Also make sure the cursor is shown again on app shutdown and on the `--gameplay-demo` path.

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

### Description

Question: does a hit mine cost the same life in Blaze 4k as in OpenITG (and StepMania), both as configured values and as applied by the life bar? Mines currently feel like they do far too much damage.

### Acceptance Criteria

- [ ] The life a mine takes (`life_deltas.hit_mine`, 5%) and the full-bar override (`hot_downgrade`, 10%) are checked against OpenITG source, with file:line references recorded on the issue
- [ ] Mine side effects beyond the life delta are compared too: the combo-to-regain-life debt after a hit (`regen_combo_after_miss`, 5 good steps refill nothing) and the DP penalty (`dp_weights.hit_mine`, -6)
- [ ] StepMania 5's defaults for the same values are recorded for comparison; OpenITG stays the reference the game follows
- [ ] A single mine takes life at most once, whether stepped on or held through, proven by a unit test
- [ ] Any mismatch is fixed with a unit test in `tests/life_keeper_test.cpp`, or filed as a follow-up bug

### Technical Notes

- Values: `assets/data/judgment_constants.json` (`life_deltas.hit_mine` = -0.05, `hot_downgrade` = -0.1, `regen_combo_after_miss` = 5, `dp_weights.hit_mine` = -6); its `source` field cites OpenITG `metrics.ini`, `PrefsManager.cpp` and `LifeMeterBar.cpp`. Loaded by `src/data/judgment_constants_loader.cpp`.
- Life path: `src/gameplay/life_keeper.cpp` — `HitMine` applies `delta_for_tap(TapJudgment::HitMine)` immediately; `delta_for_tap` swaps in `hot_downgrade` when the bar is full (`is_hot()`); `apply()` mirrors `LifeMeterBar::ChangeLife` including the combo-to-regain debt.
- Mine triggering: `src/gameplay/judgment_engine.cpp` — `handle_step_mine` (stepped on, `hit_mine` window 70 ms) and `cross_mines` (column held for `pad_stick` as the mine crosses; cursor ensures each mine is evaluated once, see #56).
- Scoring: `src/gameplay/score_keeper.cpp` (mine hit adds DP penalty, doesn't break combo).
- Likely reason mines feel harsh: on a full bar a mine costs 10% instead of 5%, and the next 5 positive judgments refill nothing. Both are intentional OpenITG mirrors, so the spike should confirm them against source rather than assume they're wrong.
- Same shape as #57 (verify judgment windows against OpenITG). AGENTS.md: OpenITG is the reference authority for life behavior.

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

### Description

In the song select difficulty rows, the meter number sits in a tight gap between the difficulty name tab and the first of the 10 meter ticks, so it looks cramped. Move the number and the ticks slightly to the right together, giving the number more room.

### Acceptance Criteria

- [ ] The meter number and the 10 meter ticks both move right by the same small amount, keeping their position relative to each other
- [ ] A two-digit meter (e.g. `12`) in the selected row's larger font clears both the name tab and the first tick
- [ ] The last tick still ends well before the best-% column; the existing gap check in `tests/select_art_test.cpp` passes and the `tick_rect` position checks are updated to the new x values
- [ ] The change applies the same way to normal rows, the selected row, and the code-drawn Edit row
- [ ] A song select screenshot (via `/verify`) shows a chart with a two-digit meter laid out cleanly

### Technical Notes

- Layout constants: `src/screens/select_art.hpp` — `kDiffMeterCentreX = 174` (number centre) and `kDiffTickX = 194` (first tick), both measured from the row's content x. Shift both by the same delta (assumed ~10–16 px in layout units; "ever so slightly").
- Tick geometry: `tick_rect` in `src/screens/select_art.cpp` (pitch `layout::kDiffTickPitch` = 18, `src/render/theme.hpp`); ticks currently span x 194–376. Best % is right-aligned at `kDiffBestRight = 547`, so there is roughly 80 px of slack before the "100.00%" text.
- Meter text styles: `theme::text::kDiffMeter` (28 px) / `kDiffMeterSelected` (32 px) in `src/render/theme.hpp`; the name tab is ~150 px wide (`kEditTabWidth`), so the number's left side is bounded by the tab edge.
- Tests: `tests/select_art_test.cpp` `test_ticks()` hard-codes tick x positions (238/400 and 252/414) — update them to the new offsets.
- Assumption: only these two constants change; the row art, name, and best % stay where they are.

### Dependencies

- Blocked by: None
- Blocks: None
