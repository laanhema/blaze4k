# Code Review (re-review r1): feature/053-audio-period-clock-interpolation

**Scope**: branch `feature/053-audio-period-clock-interpolation` vs `main` (`9ff64bd`), commits `2b6a336` (#81 implementation) and `861abe7` (fixes for the first review). GitHub issue #81. `.agents/stories/todo-stories.md` is excluded (unrelated, uncommitted owner edit). Prior report: `.agents/reviews/feature-053-audio-period-clock-interpolation-review.md`.
**Recommendation**: APPROVE WITH NITS (0 Critical, 0 High, 0 Medium, 3 Low)

## Summary

All five findings from the first review are fixed. The two Medium thread-safety gaps are closed correctly. `attach_clock_tap` now runs the same quiescence handshake as `detach_clock_tap` when it replaces a tap. `ClockTap::sound` and `engine_rate_` are atomics. I re-derived the Dekker argument for the new `exchange`-then-wait path and found no deadlock or hang path: the wait only runs on the game thread, `on_process` never blocks, and `in_process_` is cleared on every exit. The fix commit adds three small new issues, all Low: the stats-reset handoff clears its flag before zeroing the stats, four doc line citations are now stale, and the new replace-then-free test cannot actually detect the race it is meant to guard.

## Fix Verification (prior findings)

| # | Prior finding | Severity | Status | Evidence |
|---|---|---|---|---|
| 1 | Replacing the tap skipped the in-flight wait (use-after-free window; plain-pointer race on `ClockTap::sound`) | Medium | **FIXED** | `src/audio/audio_engine.cpp:144-148`: `exchange` (seq_cst), then `wait_for_audio_quiescence()` when `previous != nullptr && previous != tap`. `src/audio/clock_anchor.hpp:167-172`: `std::atomic<ma_sound*> sound` with an `is_always_lock_free` assert. `src/audio/sound_stream.cpp:110,138`: release stores. `src/audio/audio_engine.cpp:182`: acquire load into a local, so the audio thread reads the pointer exactly once. The test is `tests/clock_tap_test.cpp:175-188` (see Low 3). |
| 2 | `engine_rate_` was a plain `uint32_t`, written after the device auto-started | Medium | **FIXED** | `src/audio/audio_engine.hpp:97` is `std::atomic<uint32_t>`. All accesses are relaxed atomics (`audio_engine.cpp:86,124,180,198`, `audio_engine.hpp:61-63`). 0 is still ignored by the grouper (`clock_anchor.cpp:9`). |
| 3 | `now_fn()` returned `settings_.now_ns`, not the audio thread's snapshot | Low | **FIXED** | `src/audio/audio_engine.hpp:66` returns `now_ns_`. Every caller handles `nullptr`: `sound_stream.cpp:13-14,132,215-216`. In silent-fallback mode `now_ns_` is `nullptr` (`audio_engine.cpp:81`), so interpolation stays off, as intended. |
| 4 | Gameplay "Device callback interval" stats covered the whole session | Low | **FIXED** (one new nit, Low 1) | The game thread requests a reset (`audio_engine.cpp:193-195`). The audio thread applies it (`:175-178`) through `CallbackGrouper::reset_stats()` (`clock_anchor.cpp:53-57`), which keeps the period estimate. Gameplay requests the reset after `play()` succeeds (`gameplay_view.cpp:84-86`) and on the stub-to-audio switch (`:183`). The log at `:422-424` is still gated on `audio_started_`. Docs updated at `docs/AUDIO_LATENCY.md:238`. Test at `tests/clock_tap_test.cpp:212-228`. |
| 5 | `clock_probe` header claimed null-backend residuals are near zero | Low | **FIXED** | `tests/clock_probe.cpp:16-17` now says the run "proves the path works, not precision". This review's run measured raw 3.99 ms and interpolated 2.77 ms. |

## Thread-safety analysis of the fix commit

- **Replacement handshake (Dekker).** The game thread does an `exchange` (seq_cst RMW) on `active_tap_`, then a seq_cst load of `in_process_`. The audio thread does a seq_cst store of `in_process_ = true`, then a seq_cst load of `active_tap_`. In the single total order S, one of two cases holds:
  - The game thread's load comes before the audio thread's `true` store. Then the audio thread's tap load comes after the exchange in S and sees the new tap.
  - Otherwise, the game thread's load must read that `true` or a later value ([atomics.order]: the previous callback's `false` store happens-before the `true` store, so it cannot be read). In that case the game thread spins until the release store of `false` at `:190`. That store synchronizes with the seq_cst load, so every read of `previous->sound` and `previous->slot` happens-before the caller's `ma_sound_uninit`.
  - Reading a value that is `false` but stale is therefore impossible.
- **No deadlock or hang.** `wait_for_audio_quiescence` (`audio_engine.cpp:163-167`) is reached only from `attach_clock_tap`/`detach_clock_tap`, and those are called only from `SoundStream` on the game thread (`metronome.cpp:176`, `gameplay_view.cpp:82`, tests). Nothing registers a miniaudio end or notification callback that could call back in from the audio thread (`grep set_end_callback src` finds nothing). `on_process` is `noexcept`, takes no locks, and clears `in_process_` unconditionally (`:190`). The spin therefore lasts at most the rest of one `on_process` call (microseconds), or the start of the next one if the yield misses the gap. `shutdown()` does not wait, but `ma_engine_uninit` stops and joins the device thread before `now_ns_` is cleared. No case remains where an unloaded-but-replaced stream frees a sound the audio thread can still reach: each swap waits, and an active tap is detached with a wait.
- **Stats reset.** Requests are never lost: a store of `true` that races the audio thread's `exchange(false)` is applied on the next update. There is an ordering nit, see Low 1.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/audio/audio_engine.cpp:175-178` (with `:197-201`): the stats-reset handoff clears the flag before zeroing the stats, so `callback_stats()` can read the flag as `false` and still return pre-reset or mixed values.**
   `exchange(false, acq_rel)` publishes "reset done", but the three relaxed zero stores in `reset_stats()` are sequenced after it. A game-thread acquire load that sees `false` does not synchronize with those stores. In practice the window is nanoseconds and the gameplay log reads the stats minutes later, so the per-song numbers are fine. The immediate check at `tests/clock_tap_test.cpp:222` (`callbacks < before_reset.callbacks`) can in theory flake if the audio thread lands in that window. **Fix:** zero first, then publish: `if (flag.load(acquire)) { grouper_.reset_stats(); flag.store(false, release); }`. A request that arrives between the two calls is absorbed by a reset nanoseconds old, which is harmless.

2. **`docs/AUDIO_LATENCY.md:207`, `:48`, `:52`, `:235`: four `file:line` citations went stale when the fix commit shifted code.**
   - `audio_engine.cpp:160-176` (`on_process`) is now `:169-191`.
   - `gameplay_view.cpp:118-132` (clock binding) is now `:121-135`.
   - `gameplay_view.cpp:145-146` (the timed pair for input aging) is now `:148-149`.
   - `gameplay_view.cpp:357` (assist ticks from the raw cursor) is now `:361`.

   All four were correct at `2b6a336`. **Fix:** update the four numbers.

3. **`tests/clock_tap_test.cpp:175-188`: the new replace-then-free loop cannot detect the regression it claims to guard.**
   The comment says "attach must wait out an in-flight on_process". But `SoundStream::unload()` only calls `ma_sound_uninit`; the `ma_sound` storage (`sound_` unique_ptr) stays allocated. A late cursor read from the audio thread therefore would not crash, and no ASan or TSan is available on this host (`libasan.so`/`libtsan.so` are missing, and there is no clang). If the wait were deleted, the test would still pass. **Fix (optional):** add a cheap seam, such as a debug counter of quiescence waits that actually observed `in_process_ == true`, or a test hook that holds `on_process` open. Otherwise, reword the comment to say it is a crash and hang smoke test, and treat the Dekker argument above as the guarantee.

**Noted, not a finding:**
- The grouper's `max(group, prev_group)` can loosen the cap to about 2 periods for one interval. This is plan-accepted and tested.
- No TSan run was possible (no libtsan; no clang on the host). The plan made it an optional owner step.
- `min`/`max`/`callbacks` in `CallbackGrouper::stats()` are read as three independent relaxed loads, so a snapshot can be slightly torn. It is diagnostic only and predates this commit.
- After its tap is replaced, a stream keeps `tap_attached_ == true` and reads its own, now stale, slot. That falls back through the interpolator's stall rule. The behavior existed in the first review and is not changed by `861abe7`.
- Owner hardware and listening checks (wired, Bluetooth, Windows, macOS) are pending by design.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC) | PASS |
| Warnings gate: `audio_engine.cpp`, `clock_anchor.cpp`, `sound_stream.cpp`, `gameplay_view.cpp`, `metronome.cpp` (flags from `build/CMakeFiles/blaze4k_core.dir/flags.make`) and `clock_tap_test.cpp`, `clock_probe.cpp` (test flags), all recompiled with `-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -c -o /dev/null` | PASS (0 warnings) |
| Lint | N/A (no linter configured; the warnings gate stands in) |
| Tests, sandboxed (`bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`; I confirmed inside the sandbox that `/dev/snd` and `/run/user/$UID` are empty) | PASS, 51/51 |
| Skipped / env-guarded tests | None. `clock_tap_test` runs fully on miniaudio's null backend (no SKIP), including the new §6b replace-then-free loop and §8 stats reset. `select_screen_test` and `preview_player_test` fall back to the Null device in the sandbox; this is a sandbox fallback, not a skip |
| Flake check: `ctest -R clock_` ×10 sandboxed (`clock_interpolation_test`, `clock_tap_test`, `music_clock_test`) | PASS 10/10 |
| `clock_probe 480 --null --seconds 2` (sandboxed) | PASS: requested 480; callback interval min 480 / max 960; raw rms 3.99 ms, interpolated rms 2.77 ms, mean lead 5.07 ms |
| Headless gameplay smoke (`--headless --smoke-test 4 --gameplay-demo tests/fixtures/sync_test/metronome.sm`, sandboxed, scratch `--data-dir`) | PASS: "Blaze 4k shut down cleanly.", no hang. The sandbox has no device, so the app ran on the stub clock. The per-song stats-reset path in `gameplay_view.cpp` is therefore covered only by `clock_tap_test` §8, not end to end |
| Sanitizers (ASan/TSan) | NOT RUN: `libasan.so.8`/`libtsan.so.2` are not installed and clang is not available |
| Doc `file:line` citations | 4 stale (Low 2); the rest checked are unchanged |

## What's Good

- The fix is minimal and reuses one handshake: `wait_for_audio_quiescence()` is shared by attach and detach, so the two paths cannot drift.
- `on_process` loads `tap->sound` once into a local (`audio_engine.cpp:182`), so the null check and the cursor call always see the same pointer.
- The atomic `ClockTap::sound` has an `is_always_lock_free` assert, which keeps the audio thread wait-free on every target.
- `reset_stats()` is split out of `reset()`, so the per-song reset keeps the live period estimate and the interpolation cap is never disturbed.
- `callback_stats()` reports zeros while a reset is pending, instead of silently mixing in old numbers (apart from the tiny window in Low 1), and the header documents this.
- Docs and the header contract were updated in the same commit.

## Recommendation

Ready for the owner's hardware checks and the PR. The three Low items are cheap and optional: swap the reset order (Low 1), refresh four doc line numbers (Low 2), and add a test seam for the quiescence wait or reword its comment (Low 3). None of them blocks the merge.
