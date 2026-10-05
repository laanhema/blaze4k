# Code Review: feature/053-audio-period-clock-interpolation

**Scope**: branch `feature/053-audio-period-clock-interpolation` vs `main` (`9ff64bd`), uncommitted and untracked changes included. The branch has no commits; everything is in the working tree. GitHub issue #81. `.agents/stories/todo-stories.md` is excluded (unrelated owner edit).
**Recommendation**: NEEDS WORK (2 Medium, 3 Low)

## Summary

I reviewed the B + C implementation from #71: a 480-frame period request with the `audio.period_size_frames` override, anchors captured in `onProcess`, burst grouping, the seqlock handoff, the clamp-only `ClockInterpolator`, the timed `(music, ns)` pair for input aging, raw-cursor assist ticks, `clock_probe`, and the docs. The design matches the plan's pinned semantics and AGENTS.md principle 1. The seqlock fences are correct (Boehm pattern), the Dekker detach handshake is sound for the active tap, and the tests are thorough and pass in the sandbox. Two thread-safety gaps remain. Both are narrow, but each contradicts the plan's claim of being "TSan-clean by construction" and should be closed before merge: the tap-replacement path skips the quiescence wait, and `engine_rate_` is written after the device has already started.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

1. **`src/audio/audio_engine.cpp:139-145` (with `:147-150`, `src/audio/sound_stream.cpp:107-114`): replacing the tap skips the in-flight wait, so the old sound can be freed while the audio thread is still reading it.**
   `attach_clock_tap` stores the new tap and returns without waiting for `in_process_`. Later, `detach_clock_tap(old)` returns immediately because `old` is no longer the active tap. `SoundStream::unload()` then writes `tap_->sound = nullptr` and calls `ma_sound_uninit`. An `on_process` call that loaded `old` just before the swap may still be inside `ma_sound_get_cursor_in_pcm_frames(old->sound)`. That is a use-after-free window of a few microseconds plus a plain-pointer data race on `ClockTap::sound`. The re-attach path at `sound_stream.cpp:138` has the same kind of race: it writes `tap_->sound` while the tap is active. Production rarely hits this today, because only the metronome and gameplay streams attach and they are not unloaded right after a swap. `clock_tap_test` §5 exercises the pattern, but its 30 ms sleep hides the race.
   **Fix:** when `attach_clock_tap` replaces a non-null tap (use `exchange`), spin on `in_process_` the same way `detach_clock_tap` does before returning. Write `tap_->sound` only while the tap is not active, or make `sound` a `std::atomic<ma_sound*>`.

2. **`src/audio/audio_engine.cpp:72` / `:85` / `:166`: `engine_rate_` is a plain `uint32_t` written after `ma_engine_init` has already started the device.**
   Auto-start is the default: `noAutoStart == MA_FALSE`, `miniaudio.h:75158`. The audio thread reads `engine_rate_` in `on_process`, so the first callbacks race with the game thread's write. That is a C++ data race and TSan will report it. The grouper hides the practical effect, because it ignores rate 0. Still, this contradicts the plan's "no data race in the C++ memory-model sense" contract.
   **Fix:** make `engine_rate_` a `std::atomic<uint32_t>` (relaxed is enough). Alternatively, set `config.noAutoStart = MA_TRUE`, cache the rate, then call `ma_engine_start`.

### Suggestions (Low)

3. **`src/audio/audio_engine.hpp:62`: `now_fn()` returns `settings_.now_ns`, not the `now_ns_` snapshot the audio thread uses.**
   The implementation report (Deviation 6) says it returns the snapshot. If `configure()` is called after `init()`, `SoundStream` would timestamp with a different clock than the anchors. It is also inconsistent with `attach_clock_tap`, which checks `now_ns_`. **Fix:** return `now_ns_`.

4. **`src/gameplay/gameplay_view.cpp:419`, `src/audio/audio_engine.cpp:69`: the "Device callback interval (gameplay)" stats cover the whole session, not one song.**
   The grouper is reset only in `AudioEngine::init()`. Its min/max/callbacks therefore include select-screen previews, UI sounds, earlier songs and any device you switched to mid-session. The docs promise more than that: `docs/AUDIO_LATENCY.md` ("Gameplay logs … at the end"), `docs/CROSS_PLATFORM_VERIFICATION.md` ("when a song ends") and the plan's "per-gameplay measured callback interval". You will copy these numbers into the "Negotiated period per device" table. **Fix:** snapshot the stats at gameplay start and log the difference, or reset the stats atomics through a game-thread-safe request flag. Otherwise, document that the numbers are session-wide.

5. **`tests/clock_probe.cpp:16`: the header says null-backend residuals are "expected to be near zero", but they are not.**
   The measurements contradict it: `docs/AUDIO_LATENCY.md` records raw 3.99 / interpolated 2.70 ms, and this review's run gave 4.18 / 2.99 ms. **Fix:** align the comment with the docs ("proves the path works, not precision").

**Noted, not a finding:**
- The grouper's `max(group, prev_group)` can loosen the cap to about 2 periods for one interval after an early or merged callback. The plan accepted and tested this.
- No TSan run was possible because libtsan is missing. The plan made it an optional owner step.
- The race between miniaudio's internal cursor and game-thread cursor reads already existed and is out of scope.
- Owner hardware and listening checks (wired, Bluetooth, Windows, macOS) are pending by design.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC) | PASS |
| Warnings gate: 16 changed/new TUs recompiled with the build's own flags (`-O3 -std=c++20 -Wall -Wextra -Wpedantic`, `-c -o /dev/null`) | PASS (0 warnings) |
| Lint | N/A (no linter configured; the warnings gate stands in) |
| Tests, sandboxed (`bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; I confirmed `/dev/snd` and `/run/user/$UID` are empty inside the sandbox) | PASS, 51/51 |
| Skipped / env-guarded tests | None. `clock_tap_test` ran fully on the null backend (no SKIP). `select_screen_test` and `preview_player_test` fall back to miniaudio's Null device in the sandbox; this is a sandbox fallback, not a skip, and every assertion still runs |
| Flake check: `ctest -R clock_` ×5 sandboxed (runs `clock_interpolation_test`, `clock_tap_test`, `music_clock_test`) | PASS 5/5 |
| `clock_probe 480 --null --seconds 2` (sandboxed) | PASS: requested/negotiated 480; raw rms 4.18 ms, interpolated rms 2.99 ms |
| Headless smoke (`--headless --smoke-test 5 --start-screen select`, sandboxed, scratch data dir) | PASS: "Blaze 4k shut down cleanly.", no `configure() after init` warning |
| Static checks: no SDL in `src/audio`/`src/timing`, no `908-919`, no "follow-up issue #81", nothing staged | PASS |
| Doc `file:line` citations (`audio_engine.cpp:160`, `gameplay_view.cpp:118/145-146/357`, `calibration_screen.cpp:99-100`, `main.cpp:273`, `app.cpp:176`) | PASS |

## What's Good

- Pinned semantics are followed precisely. The ns timestamp is read before the cursor and the reset is taken after the miniaudio call. Together these give a clean "ignore pre-reset anchors" rule that handles backward seeks and resume.
- The seqlock uses atomics only, with correct release/acquire fences. The stress test checks a per-write invariant on every read.
- `clock_interpolation_test` pins every rule (steady, late, burst vs. per-update stall contrast, stall, pause/seek, song end, resampling, race window, skew, overflow), and every query checks the shared invariants.
- Assist ticks moved to the raw cursor with a clear rationale. Input aging now uses one consistent pair, as OpenITG does, and keeps a safe fallback for stub and injected sources.
- The config key follows existing warn-and-clamp patterns. A negative typo never silently selects the backend default.
- The docs are updated end to end, with honest "owner: not yet measured" markers.

## Recommendation

Fix the two Medium findings: the quiescence wait on tap replacement and an atomic (or pre-start cached) `engine_rate_`. The three Low items are cheap and worth doing in the same pass. Then re-run the sandboxed suite and the `clock_tap_test` flake loop. After that the branch is ready for the owner's hardware checks and the PR.
