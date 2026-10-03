# TODO Stories

**Source**: `TODO.md` · **Generated**: 2026-10-02 · **Updated**: 2026-10-03 (TODO-13 – TODO-14 added)

## Skipped

- **Done (`[x]`)** — 28 items: `TODO.md` lines 1–15, 18–30 (font capitals bug, song-select audio, difficulty order, tab legend, key-repeat scrolling, song list room, attract timeout, legend overlap, song list rendering, results delay, disappearing holds, ITG arrow colors ×2, Cel noteskin, hold-end artifact, colored difficulties, receptor/hit effects, options SFX, assist-tick toggle, calibrate → Esc, auto `songs/` folder, remap background music, white high-score flash, best % in song select, assist-tick timing, options room, remap table layout, bigger receptors).
- **Already tracked** — none on the first run. Second run: `TODO.md:16` (#55, TODO-1), `TODO.md:35` (#60, TODO-7); lines 31–34, 36, 37 are now marked done. Third run: `TODO.md:16` (#55), `:35` (#60), `:40` (#78); lines 36–39 are now marked done.
- **Deferred by user** — `TODO.md:17` "Something Blaze themed visuals would be cool" (too vague for now; ID TODO-2 left unused). Still deferred on the second and third runs.
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
**GitHub**: #55

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
**GitHub**: #78

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
