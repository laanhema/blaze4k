# Code Review: feature/024-noteskin-animations-ui-sounds (issue #24, D2)

**Scope**: Branch `feature/024-noteskin-animations-ui-sounds` vs `main`, including uncommitted
working-tree changes (branch has no commits; all changes are working-tree/untracked).
**Recommendation**: APPROVE WITH NITS

## Summary

D2 delivers a procedural direction-aware noteskin (`note_art` + `NoteSkin`), event-log-driven
judgment/combo popups (`JudgmentAnimator` wired into `GameplayView`), and synthesized menu UI
sounds (`UiSoundPlayer` + central `ScreenManager` triggers). Core principle 1 is upheld: the
judgment/scoring/timing path (`judgment_engine`, `score_keeper`, `life_keeper`, `note_field`,
`timing/`) is byte-for-byte unchanged, the animator consumes the existing `new_events_` slice
(no re-judging), and `fixed_dt` is presentation-only. Build is clean (no warnings), all 31 tests
pass, and the pure modules are free of SDL/GL/clock includes. Remaining issues are minor
presentation-timing edge cases, not correctness or safety blockers.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

- **`src/gameplay/judgment_animator.cpp:49` — combo milestones can be skipped entirely by
  chords.** The pop fires only when `combo % kComboMilestone == 0` is observed exactly, but
  `ScoreKeeper` advances combo by the whole row/chord size (`src/gameplay/score_keeper.cpp:147`,
  `state_.combo += aggregate.expected`), so a row crossing a multiple (e.g. 49 → 51) never lands on
  50 and no pop occurs. Recommendation: detect a *crossing* (`last < milestone*N <= combo`) instead
  of exact equality, or fire on `combo / 50 > last_milestone_ / 50`.

- **`src/gameplay/judgment_animator.cpp:49-50,65` — milestone dedupe never resets across a combo
  break.** `last_milestone_` is only updated upward and while combo is non-zero; after a miss
  (combo → 0) rebuilding to the same last milestone (e.g. 50 again) is suppressed by
  `combo != last_milestone_`, while a later 100 would still fire. Recommendation: reset
  `last_milestone_` when `combo == 0` (or key dedupe on the crossing index above), and cover it in
  `judgment_animator_test.cpp`.

### Suggestions (Low)

- **`src/audio/ui_sounds.cpp:194` — no preload; WAV decoded synchronously on each menu press.**
  The plan proposed pre-loading one `ma_sound` per sound, but `ma_engine_play_sound` decodes the
  file on the input/update thread every press. Sounds are tiny (~80–150 ms), so the hitch is
  negligible, but preloading would match the plan and avoid synchronous disk I/O on the input path.

- **`src/audio/ui_sounds.cpp:48-54` — WAV headers written in host endianness.** `write_u16`/`write_u32`
  reinterpret-cast host integers, which is only valid little-endian. All supported targets are LE, so
  this is theoretical; explicit byte assembly would make the serializer platform-independent and
  match the test's little-endian reader.

- **`src/audio/ui_sounds.cpp:159` — `file_size(path, exists_ec)` error sentinel treated as valid.**
  On a stat error `file_size` returns `static_cast<uintmax_t>(-1)`, so `< 44u` is false and a
  present-but-unstattable file is accepted and later played. Recommendation: check `exists_ec` and
  reject/regenerate on error.

- **`src/screens/screen_manager.cpp:210-219` — menu sounds fire for presses a screen may not act on.**
  A directional press on `Title` (which has no cursor) still plays `Move`. Documented as "the press
  that is about to navigate", so this is a minor UX nit rather than a bug.

- **`src/gameplay/gameplay_view.cpp:158-160` + `:172-173` — popups freeze on fail.** `update()`
  early-returns while `exited_`, so `judge_anim_.update` stops and the last popup stays at its last
  alpha/scale until the results transition instead of fading.

- **`src/gameplay/note_field_renderer.cpp:82` — tail caps still use the white `quad_texture`.**
  Bodies/heads/receptors/mines moved to procedural art but tail caps remain plain quads; a minor
  visual inconsistency (plan allowed "unchanged or body mask").

## Criteria Verification

| Criterion | Result |
|-----------|--------|
| Core principle 1: judgment/scoring/timing path unchanged | PASS — `git diff main` touches no `judgment_engine/score_keeper/life_keeper/note_field/timing`; animator only reads `new_events_`; `fixed_dt` presentation-only |
| Distinct tap/hold/roll/mine art; direction-aware | PASS — distinct hashes + per-column textures (`note_art.cpp`, `noteskin.cpp:118-141`); 4-way rotation covered by `note_art_test` |
| Procedural masks, no PNGs | PASS — `Texture::from_rgba` only |
| Headless-safe, no leaks | PASS — `init()` guards `glad_glGenTextures`; `shutdown()` destroys all textures; `Texture` move-assign destroys destination |
| Popups derive from event log | PASS — `GameplayView::update` feeds the same slice as `score_`/`life_` |
| Combo milestones deduped / fire once | PASS with caveats — dedupe works; chord-skip and break-reset gaps above |
| Last-event-wins on chords documented | PASS — `judgment_animator.hpp:27-28` |
| UI sounds synthesized WAVs | PASS — 16-bit mono PCM synth, `ui_sounds_test` validates header |
| Central menu-only triggers (Title/Attract/Select/Results); silent in Gameplay/Calibration/InputRemap | PASS — `is_menu_screen`, tested |
| Null-sink safe; no blocking/throw without device | PASS — null guards, `init` returns false, `play` no-ops/logs once |
| 2D-only (no video/3D) | PASS |
| Pure modules free of SDL/GL/clock includes | PASS — grep for `SDL`/`glad`/`<chrono>`/`clock` finds only comments; `judgment_animator.cpp`'s renderer header transitively pulls no GL |
| Unsourced presentation constants | Documented as "Tundra presentation, unsourced" (`judgment_animator.hpp:20`, `hud_renderer.hpp`, `ui_sounds.hpp:7`, `noteskin.hpp`) |

## Validation Results

| Check | Status |
|-------|--------|
| Configure (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) | PASS |
| Build (`cmake --build build -j16`) | PASS |
| Warnings (`-Wall -Wextra -Wpedantic`) | PASS — none (forced rebuild of changed units) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 31/31, 0 skipped |
| `note_art_test` / `judgment_animator_test` / `ui_sounds_test` | PASS |
| Regression (`note_field_test`, `screen_manager_test`, `score_keeper_test`, `life_keeper_test`, `results_screen_test`) | PASS |

## What's Good

- Clean separation of pure model (`note_art`, `judgment_animator`) from thin GL/audio layers,
  mirroring the established `BackgroundRenderer`/`PreviewPlayer` seams.
- The animator is genuinely event-sourced; no clock or re-judging leaks into the judgment path.
- Direction baked into per-column textures is the correct call — `UVRect` cannot express a 90°
  rotation, and the deviation is documented with rationale.
- Synthesized, asset-free WAVs keep the fresh-clone/engine-only philosophy; injectable
  `IUiSoundSink` makes the shell trigger logic unit-testable without a device.
- Test coverage is targeted at the actual contracts (distinct masks, coverage-preserving rotation,
  milestone dedupe, WAV layout, menu-vs-gameplay gating).

## Recommendation

Approve with nits. The two Medium items (chord-skip and combo-break re-fire of combo milestones)
are the only behavior worth tightening for AC3; everything else is optional polish. No changes
required before merge for a presentation-only feature, but fixing the milestone crossing logic in
`JudgmentAnimator::update` + a test would close the AC3 gap.
