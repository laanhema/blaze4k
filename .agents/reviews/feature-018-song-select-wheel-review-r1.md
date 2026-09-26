# Code Review (Re-review r1): feature/018-song-select-wheel (Song Select wheel)

**Scope**: branch `feature/018-song-select-wheel` (HEAD `db49f89`) vs `main` (`caeb605`), committed + uncommitted (working tree clean: `git status --porcelain` empty)
**Prior report**: `.agents/reviews/feature-018-song-select-wheel-review.md`
**Recommendation**: APPROVE WITH NITS

## Summary

This re-review verifies the six findings from the prior report against the current
branch tip. All six are fixed with matching regression tests: the High-severity
decompression-bomb path now probes the image header (`stbi_info`) *before* any
allocation and additionally caps the decoder with `STBI_MAX_DIMENSIONS 4096`; the
held-note state is now sampled from the authoritative input callback with
focus-loss clearing in `InputManager`; and Select preserves its wheel/chart
position across re-entry. Build and all 20 CTest targets pass. No new
Critical/High/Medium regressions were introduced by the fixes (including the new
`IAudioStream` seam, the `ScreenContext::action_down` `std::function`, and the
focus-loss handling). Two optional, non-blocking test/hardening nits are noted.

## Fix-Verification Table

| # | Prior finding (severity) | Verdict | Evidence |
|---|--------------------------|---------|----------|
| 1 | Dimension cap enforced *after* decode → OOM (High) | **FIXED** | `probe_image_header` uses `stbi_info` at `src/render/texture.cpp:26-37` and is called at `src/render/texture.cpp:149`, strictly before `stbi_load` at `:165`; the cap constant is `kMaxImageDimension = 4096` (`:18`). Defense in depth: `STBI_MAX_DIMENSIONS 4096` in `src/render/stb_image_impl.cpp:8`. Tests: `tests/texture_test.cpp:57-79` writes a <1 KiB PNG IHDR declaring `16000x16000`, asserts `!probe_image_header(...).ok` and `!from_file(...).valid()`; `tests/texture_test.cpp:81-96` pins the 4096 boundary (4096×4096 ok, 4096×4097 rejected). |
| 2 | `held_` reconstructed from event stream → stuck hold (Medium) | **FIXED** | `GameplayScreen::update` samples the authoritative callback when wired: `src/screens/gameplay_screen.cpp:44-50`; callback declared at `src/screens/screen.hpp:53-57`; wired in `src/main.cpp:295-297` to `InputManager::is_action_down`. Focus loss now clears cached state: `src/input/input_manager.cpp:116-124` (`clear_action_states`, declared `input_manager.hpp:35`). Tests: authoritative-sampling incl. missed release at `tests/select_screen_test.cpp:274-299`; focus-loss clearing at `tests/input_test.cpp:130-141`. |
| 3 | Re-entering Select resets highlight to first song (Medium) | **FIXED** | `rebuild()` snapshots the prior selection by `song->simfile_path` and restores it (with clamped chart index) at `src/screens/select_screen.cpp:127-166`; `enter()` still calls `rebuild()` (`:202`). Test: `tests/select_screen_test.cpp:253-272` (confirm → Back → both song and chart index preserved). |
| 4 | `request_preview_for_selected` takes an unused `ctx` (Suggestion) | **FIXED** | Parameter removed: declaration `src/screens/select_screen.hpp:65`, definition `src/screens/select_screen.cpp:187`. |
| 5 | 16 MiB limit documented as allocation guard but only bounds read size (Suggestion) | **FIXED** | Comment now states the dimension cap is on the header and `kMaxImageBytes` only bounds the read: `src/render/texture.cpp:14-17`; header doc updated to describe the pre-decode probe: `src/render/texture.hpp:44-49`. |
| 6 | Successful preview playback/loop path untested (Suggestion) | **FIXED** | `IAudioStream` seam added (`src/audio/sound_stream.hpp:11-25`; injectable ctor `src/audio/preview_player.hpp:35-38`, `preview_player.cpp:7`). `FakeAudioStream` + success/loop assertions at `tests/preview_player_test.cpp:65-106` (load → Active, seek to start, in-window no-wrap, past-window wrap) and zero-start loop at `:108-127`. |
| 7 | `format_bpm_range` doc claims `"" -> "?"` though dead (Suggestion) | **FIXED** | Doc now labels the empty-segment branch as defensive-only/unreachable from a parsed simfile: `src/screens/select_screen.hpp:17-19`. |

## Issues Found

### Critical

None.

### High Priority

None. (Prior High finding #1 is verified fixed; no new High regressions.)

### Medium Priority

None.

### Suggestions (new, non-blocking)

- **`tests/texture_test.cpp:74-75` — the `from_file` assertion cannot distinguish header
  rejection from the headless early-return.** In the test environment GL is unbound, so
  `Texture::from_file` would return an invalid texture even if it reached `stbi_load`
  (`src/render/texture.cpp:156-160`). The real rejection guarantee is carried by the
  direct `probe_image_header` assertion at `tests/texture_test.cpp:69-70`, which is
  correct; this is only a note that the `from_file` line is not independently probative.
- **`src/input/input_manager.cpp:118-124` — focus-loss clears state without emitting
  release events.** Production is covered because `GameplayScreen` samples the callback
  (`gameplay_screen.cpp:44-50`) and the demo path samples `is_action_down` directly
  (`main.cpp:267-273`), but any future consumer that replays the event stream for held
  state would not observe the release. Intentional per the comment; consider a release
  event in the queue if an event-replay consumer is ever added.

### Noted, not a finding

- The `ScreenContext::action_down` lambda captures `&app` by reference (`main.cpp:295-297`).
  `app` is declared before `shell` (`main.cpp:213` vs `:288`), so `app` outlives `shell`
  and the lambda's references are valid for the manager's whole lifetime.
- `IAudioStream` is fully implemented by `SoundStream`; every method `PreviewPlayer`
  calls is virtual overridden, and the interface has a virtual destructor — no slicing or
  partial-override hazard.
- `.agents/reviews/feature-017-*` files were intentionally excluded from this review.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build`) | PASS |
| Lint | N/A (no clang-format/clang-tidy config or lint target in repo) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 20/20 (incl. `select_screen_test`, `preview_player_test`, new `texture_test`) |

Notes:
- No tests were skipped or env-guarded; the suite uses no GTest-style skips.
- The prior report tallied 19 tests; the new `texture_test` target (`tests/CMakeLists.txt:196-204`)
  raises the total to 20. Both preview success/loop paths are now exercised via the seam.
- Purity probe `rg -n "chrono|GetTicks|SDL_GetTicks|this_thread" src/screens src/audio/preview_player.*`
  returns no matches — gameplay/timing remains wall-clock free; preview delay uses injected `fixed_dt`.

## What's Good

- The High finding is fixed at the correct layer: the header probe rejects oversized
  dimensions before `stbi_load`, with `STBI_MAX_DIMENSIONS` as independent defense in depth,
  and the crafted-IHDR tests pin both the reject and the exact 4096 boundary.
- The audio seam is minimal (`IAudioStream`) and made the previously-untested success/loop
  window deterministically testable without an audio device.
- Selection preservation uses stable song identity (simfile path), not list position, and
  clamps the retained chart index.
- Focus-loss handling plus per-tick callback sampling closes the stuck-hold hole, and both
  behaviors are backed by regression tests.

## Recommendation

Approve with nits. All six prior findings are verified fixed with tests, and the fixes
introduce no new Critical/High/Medium issues. The two suggestions are optional hardening/
test-clarity improvements and need not block merge.
