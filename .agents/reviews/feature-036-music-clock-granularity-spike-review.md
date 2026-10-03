# Code Review: feature/036-music-clock-granularity-spike

**Scope**: Branch `feature/036-music-clock-granularity-spike` vs `main` (no commits yet; uncommitted `AGENTS.md`, `docs/AUDIO_LATENCY.md`; untracked `.agents/plans/completed/036-music-clock-granularity-spike-plan.md`, `.agents/reports/036-music-clock-granularity-spike-plan-report.md`). Spike for GitHub issue #71.
**Recommendation**: APPROVE (with nits)

## Summary

This is a docs-only spike. It rewrites "Clock granularity" in `docs/AUDIO_LATENCY.md` with measured callback steps, the inconsistent `(cursor, reference_ns)` pairing explanation, pinned-commit OpenITG / SM 5.0.12 / SM 5_1-new references, the miniaudio options, the A–E evaluation and the recorded owner decision (B + C). It also amends AGENTS.md principle 1 and design pattern 1 to allow bounded, re-anchored interpolation. I checked every in-repo, miniaudio and upstream citation against source (upstream fetched at the pinned commits), and all of them hold. The decision record matches the #71 comment and follow-up #81. There are no blocking issues. One Medium wording gap in the new AGENTS.md rule should be tightened, and a few Low consistency nits remain.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

- **`AGENTS.md:13` (also `:87`, `docs/AUDIO_LATENCY.md:295`, `:307-309`): "bounded to one update period" is ambiguous when engine updates burst inside one device callback.**
  `ma_engine_config.periodSizeInFrames` fixes the *engine* update size, and miniaudio says "the underlying device may be a different size" (`miniaudio.h:11190`). The doc itself records bursting below the graph quantum (`docs/AUDIO_LATENCY.md:179-180`, 1133 updates but only 576 distinct cursor values). It also says Bluetooth sinks "may force a larger quantum anyway" (`:294`). With B (480-frame engine updates) on such a sink, `onProcess` fires several times back to back per device callback. The anchors then cluster in time, and a clamp of "one (engine) update period" ahead would stall the interpolated clock for most of each device period. That reintroduces the stepping C is meant to remove, and the binding AGENTS.md rule as worded would forbid the fix. #81 lists "burst callbacks" as a test case, but the principle text is the authority an implementer will follow.
  *Recommendation:* define the bound in AGENTS.md and the decision text as the interval between distinct anchors (the observed device-callback cadence), or "one device period". Alternatively, say that anchors taken in the same burst collapse to the last one.

### Suggestions (Low)

- **`docs/AUDIO_LATENCY.md:147` (and `:29`): "jitter of up to ±9 ms" contradicts the measured peak.** The same doc measures a raw residual max of 14.37–14.60 ms (`:169`, `:368`), and row A states "max ±14.6 ms" (`:293`). The ±9 ms figure is the idealised uniform-step bound and does not match the data, because the step minimum is 388 frames, so steps are uneven. *Recommendation:* say "rms 6.2 ms, peaks up to ~14.6 ms" (or "typically within ±9 ms") in both places.
- **`docs/AUDIO_LATENCY.md:45`: stale range `src/audio/sound_stream.cpp:138-154`.** The function spans `:138-153`. The spike corrected this at `:141` and in #81, but the "How the music clock works today" table still says `:138-154`, so the doc is inconsistent with itself. *Recommendation:* change it to `:138-153`.
- **`docs/AUDIO_LATENCY.md:416`: `ctest --preset <os>-release` does not match any test preset.** `CMakePresets.json` test presets are `windows-msvc-release` / `macos-clang-release`, and the line above names them. A literal reading such as `windows-release` fails. That step also runs `audio_test`, which plays a short test tone (as `:386` notes), and the section intro at `:399-400` says runs are silent. *Recommendation:* write `ctest --preset windows-msvc-release` / `macos-clang-release`, and note the tone.
- **`README.md:9`: still the pre-#71 principle-1 wording.** It now diverges from `AGENTS.md:13`. The implementation report acknowledges this but does not resolve it. *Recommendation:* mirror the amended sentence, or have README link to AGENTS.md for the principle.

**Noted, not a finding:**
- The `gran2` probe is not committed, so the owner procedure (`docs/AUDIO_LATENCY.md:397-419`) needs it rebuilt from the prose recipe. This is the plan's explicit default (Open Question 4).
- `src/gameplay/judgment_input.hpp:10` still cites OpenITG `Player.cpp:908-919`. I verified that the computation is at `:918-926` @ `f2c129fe`. It is deferred to #81 by plan, and #81 has an AC for it.
- AGENTS.md was amended in this spike rather than in the follow-up, which deviates from the plan's Open Question 2. The report says the owner instructed this, and the #71 decision comment (author `laanhema`) announces it.
- AC "wired 3.5 mm + Bluetooth measured" is not done by the agent and is recorded as owner steps or "not measured". The plan scoped this to the owner.

## Citation verification

| Source | Rows checked | Result |
| --- | --- | --- |
| In-repo (`sound_stream.cpp:138-153`, `:146-148`; `app.cpp:152`; `gameplay_view.cpp:137,156`; `judgment_input.hpp:10-19`) | all | OK (except the stale `:138-154` at doc `:45`) |
| miniaudio 0.11.21 (`7044`, `7048`, `11172`, `11181`, `11190-11191`, `11200`, `12099-12105`, `22241-22244`, `28751-28753`, `30252-30276`, `30520`, `74988-74989`) | all | OK |
| OpenITG @f2c129fe (`Player.cpp:918-926`, `RageSound.cpp:688-731/712-728/742-756`, `GameSoundManager.cpp:408/481-483/523`, `ALSA9Helpers.cpp:392-405`, `GameState.cpp:795-804`, `RageSoundDriver_CA.cpp:158-164`) | spot-checked core rows | OK |
| SM 5.0.12 @45e0787a (`RageSoundDriver_PulseAudio.cpp:297-324`, `RageSoundDriver_AU.cpp:162/214-217/322-325`, `RageSoundDriver_Generic_Software.cpp:490-523`, `Player.cpp:1976-1978/2140-2145`, `GameSoundManager.cpp:568-570/606`) | spot-checked core rows | OK |
| SM 5_1-new @825467bc (`RageSoundDriver_PulseAudio.cpp:234-238/309-337`, `Generic_Software.cpp:511-549`) | all | OK |
| Commits `464f0f703bd2` (2011-05-02, stutter) and `6dcb310c21b8` (2026-07-27, last touch of 5_1-new Pulse driver) | both | OK (GitHub API) |
| Decision record vs #71 comment 5968315939 and #81 body | — | Consistent |

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build | N/A: no source changed (`git diff main -- src tests CMakeLists.txt cmake` is empty). `make -n -C build` schedules no compile/link steps, so the build tree is current |
| Lint | N/A: none configured in this repo |
| Whitespace (`git diff main --check`) | PASS |
| Tests | PASS: 41/41 under `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure`. I first confirmed inside the sandbox that `/dev/snd` and `/run/user/$UID` were empty, so no real audio device was reachable. No skipped or env-guarded tests: the "Skipping" lines in verbose output are parser/library log messages, not test skips |

## What's Good

- Every number is labelled **measured** (device, backend, date, run count) or **estimate**. The single-machine caveat is stated, and the non-reproduced 18.79 ms outlier is reported honestly next to the clean runs.
- The upstream research is careful. Every row cites a pinned commit and line, and all the rows I checked were accurate. The 5_1-new retry-loop change is correctly flagged as a recent upstream change rather than reference behavior.
- The root-cause explanation (a stale cursor paired with a "now" timestamp, `:194-204`) is precise and actionable for #81.
- Unsafe advice was removed: the null-sink / `PULSE_SINK` rerouting instructions are replaced with a volume-0, no-rerouting rule.
- The A–E table keeps the #71 letters clearly separate from the #58 options, and quotes principle 1 "as worded before #71".
- The scope is tight: no `src/`, `tests/` or build changes.

## Recommendation

Mergeable as a spike. Before merging, tighten the AGENTS.md / decision wording on what "one update period" means when engine updates burst (Medium), so #81 implements against an unambiguous rule. The Low items are one-line doc fixes and can go in the same pass or into #81. The PR should reference #71. It can close #71 if the owner treats the posted decision as satisfying AC 3/4 (#81 is filed).
