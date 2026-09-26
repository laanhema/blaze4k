# Code Review: feature/018-song-select-wheel (Song Select wheel)

**Scope**: branch `feature/018-song-select-wheel` vs `main`, including all untracked new files (issue #18)
**Recommendation**: APPROVE WITH NITS

## Summary

This branch introduces the real Song Select wheel: `SelectScreen` (pack-grouped,
SELECTABLE-filtered library list with banner art, BPM/artist, passthrough difficulty
+ meter + best-grade), a delayed/looping `PreviewPlayer`, a path-keyed `TextureCache`
with stb_image-backed `Texture::from_file`, a thin `GameplayScreen` host around the
pre-existing `GameplayView`, and the `ScreenContext` wiring to carry a `PlayRequest`
across the screen transition. The layering (pure helpers, config→options mapping,
pointer handoff owned by `main`) matches the PRD patterns, and the build plus all 19
CTest targets pass. The untrusted-image path is the only place where the hardening
discipline is incomplete: the dimension cap is enforced after decoding, leaving a
decompression-bomb allocation. Two behavioural notes (held-note state source, and
selection reset on re-enter) are worth addressing but are not blockers.

## Issues Found

### Critical

None.

### High Priority

1. **`src/render/texture.cpp:140-152` — dimension cap is enforced *after* the decode, so a
   crafted banner can OOM the process.** The 16 MiB *file-size* check does not bound decoded
   memory, and `stb_image`'s own default `STBI_MAX_DIMENSIONS` is `1 << 24` (16.7M), so a
   heavily-compressed ~kilobyte PNG declaring e.g. `16000x16000` is accepted by `stbi_load`,
   which allocates `width*height*4 ≈ 1 GiB` before line 147 rejects it. Per AGENTS.md, image
   input is untrusted and must never crash. Recommend probing with `stbi_info()` (or defining
   `STBI_MAX_DIMENSIONS 4096` in the implementation TU) *before* `stbi_load`.

### Medium Priority

2. **`src/screens/gameplay_screen.cpp:45-57` — `held_` is reconstructed from the event stream
   instead of the authoritative input state.** The demo path in `main.cpp:268-273` reads
   `InputManager::is_action_down(...)`; `GameplayScreen` derives the same state by replaying
   press/release events. Any missed release (e.g. window focus loss — `InputManager` has no
   focus handling, confirmed by `rg focus src/input`) leaves `held_` stuck `true`, sustaining
   a hold note forever. Recommend exposing the authoritative down-state (or an
   `InputManager*`/`is_action_down` functor) on `ScreenContext` and sampling it per tick.

3. **`src/screens/select_screen.cpp:106-125,175-186` — re-entering Select always resets the
   highlight to the first song.** `enter()` unconditionally calls `rebuild()`, which sets
   `selected_song_ = 0`. After aborting a run (Back from Gameplay → Select) or returning from
   any transition, the player loses their wheel position instead of landing back on the song
   they were viewing. Consider preserving the selection keyed by song identity (or skipping
   rebuild when the library pointer is unchanged).

### Suggestions

4. **`src/screens/select_screen.cpp:160-173` — `request_preview_for_selected` takes a `ctx`
   parameter it never uses (`(void)ctx;`).** Drop the parameter.
5. **`src/render/texture.cpp:126-129` — the 16 MiB limit is documented as an
   allocation guard but only bounds the compressed byte count.** Either add the pre-decode
   dimension probe (finding 1) or adjust the comment/constant name to reflect that it is a
   read-size guard, not a decoded-size cap.
6. **`tests/preview_player_test.cpp` — the successful playback path is untested.** All
   preview tests exercise the `Waiting → failed load → Idle` path (no valid audio fixture), so
   the loop/seek-back logic at `preview_player.cpp:47-51` and `stream_.play()`/`seek_seconds`
   under a real load have no coverage. Add a lightweight fixture or a `SoundStream` seam so
   the delay-elapsed → Active → wrap window is asserted.
7. **`src/screens/select_screen.cpp:60-81` — the `format_bpm_range` doc claims `"" -> "?"`,
   but `TimingData` always seeds a 120 BPM segment (per `tests/select_screen_test.cpp:209`),
   making the `"?"` branch effectively dead.** Minor doc/intent mismatch.

### Noted, not a finding

- Textures are deliberately not freed when GL is unavailable and invalid textures are cached
  so a path is not retried each frame — documented behavior, correct for headless.
- `PlayRequest` pointing into `main`'s long-lived `SongLibrary` is safe: `library`,
  `play_request`, and `app` outlive `shell` (declared 183/210/212 vs 220 in `main.cpp`).
- Results/pause/retry, background art, and the `.agents/{plans,reports}` artifacts are out of
  scope for this review per the brief.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build (`cmake --build build`) | PASS |
| Lint | N/A (no clang-format/clang-tidy config or lint target in repo) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 19/19 (incl. new `select_screen_test`, `preview_player_test`) |

Notes:
- No tests were reported skipped; GTest-style skips are not used in this project.
- The preview success/loop path is effectively unexercised (no valid audio fixture), see
  suggestion 6.
- Purity probe `rg -n "chrono|GetTicks|SDL_GetTicks|this_thread" src/screens src/audio/preview_player.*`
  returned no matches — gameplay/timing remains wall-clock free; delay uses injected `fixed_dt`.

## What's Good

- Clean separation: `gameplay_options_from_config` and `format_bpm_range`/`grade_display_label`
  are pure and directly unit-tested; `PreviewPlayer`'s delay is driven only by `fixed_dt`.
- Solid untrusted-input handling elsewhere: `Texture::from_file` never throws, checks
  empty/missing/oversize files, frees `stbi` pixels on the reject path, and falls back to a
  placeholder on decode failure or headless; `TextureCache` keeps node-stable pointers and
  stores invalid textures to avoid per-frame retries.
- Only the selected banner is decoded, and the cache is cleared on `exit()`, keeping texture
  memory bounded.
- Back-navigation contract is kept in sync between `handle_back()` and `back_navigates()`, with
  a clarifying comment; SELECTABLE:NO filtering and wheel wrap-around are tested.
- `Texture::from_file` failure paths and the empty-library select screen are both covered.

## Recommendation

Approve with nits. Before merge (or as a fast follow for this issue):
1. Fix finding 1 (pre-decode dimension probe / cap) — the one genuine robustness gap.
2. Consider findings 2 and 3; at minimum file follow-ups if deferred.
3. Address suggestions 4 and 6 opportunistically.
