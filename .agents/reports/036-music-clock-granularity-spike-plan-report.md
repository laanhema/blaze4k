# Implementation Report

**Plan**: `.agents/plans/completed/036-music-clock-granularity-spike-plan.md`
**Branch**: `feature/036-music-clock-granularity-spike`
**Status**: COMPLETE

## Summary

Spike #71 (music-clock granularity). Measured the audio-callback step of the music clock on the
current default Linux sink with a silent scratch probe (`gran2`, volume 0, all-zero buffer, 3 s runs,
3 runs each at period 0 / 480 / 256). Re-verified every OpenITG / StepMania 5.0.12 / 5_1-new and
miniaudio citation at the pinned commits. Rewrote "Clock granularity" in `docs/AUDIO_LATENCY.md` with
the measurements, the inconsistent-pairing explanation, upstream reference tables, miniaudio options,
the A–E evaluation and the **owner decision (2026-10-03): B + C approved** (E noted as possible future
upgrade; A and D not chosen). Added owner measurement procedures (wired 3.5 mm, Bluetooth,
Windows/macOS, all "not measured") and the `gran2` recipe; replaced the null-sink advice with the
volume-0 rule. Amended AGENTS.md principle 1 and design pattern 1 to record the bounded
interpolation exception (#71). Filed follow-up implementation issue #81 (added to the blaze4k
project board) and posted the decision comment on #71. No `src/`, `tests/` or build changes.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Create branch | `feature/036-music-clock-granularity-spike` | ✅ |
| 2 | Measure callback step (default sink only, silent probe) | scratchpad `probe/gran2.c` (not committed) | ✅ |
| 3 | Verify every upstream citation (U1–U5) with `sed -n` at pinned commits | scratchpad `oitg`, `sm512`, `sm5`; `build/_deps/miniaudio-src/miniaudio.h` | ✅ |
| 4 | Update audio latency doc (granularity, references, A–E, decision, measurements, owner procedure, probe recipe, follow-ups) | `docs/AUDIO_LATENCY.md` | ✅ |
| 4b | Amend principle 1 / design pattern 1 (owner instruction) | `AGENTS.md` | ✅ |
| 5 | Build + full suite + scope check | — | ✅ |
| 6 | Record the owner decision on #71 | GitHub #71 comment | ✅ |
| 7 | File follow-up implementation issue, add to board | GitHub #81 | ✅ |

## Measurements (2026-10-03, default sink `alsa_output.pci-0000_06_00.6.HiFi__Speaker__sink`, PulseAudio backend, 48 kHz)

| Requested period | Device | Step min/mean/max (frames) | Raw rms / max | Anchored estimate rms / max |
|---|---|---|---|---|
| 0 | 900 × 4 | 388 / 872.6–883.1 / 900 | 6.20 / 14.37–14.60 ms | 2.83–2.86 / 5.74–5.87 ms |
| 480 | 1440 × 1 | 480 / 480 / 480 | 3.26–3.27 / 7.46–7.64 ms | 1.44–1.46 / 2.80–2.91 ms |
| 256 | 768 × 1 | 256 / 256 / 256 | 1.54 / 2.81–3.18 ms | 0.08–0.13 / 0.26–0.57 ms |

Default-period mean is inside the plan's 870–900 expectation (#58 reproduced). `pactl get-default-sink`
identical before and after; no sink, mute or routing change.

## Citation verification

All U1–U5 rows confirmed at the pinned ranges. Corrections applied in the doc text:
- `src/audio/sound_stream.cpp:137-153` → `:138-153` (function starts at 138).
- miniaudio PipeWire warning cited as `:30254-30258` (comment + 25 ms constant); the comment itself is `:30255-30256`.
- `6dcb310c21b8` (last commit touching SM 5_1-new `RageSoundDriver_PulseAudio.cpp`) confirmed via GitHub API (shallow clone cannot show it).

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j`) | ✅ up to date |
| Lint | n/a (none configured) |
| Tests (bwrap sandbox) | ✅ 100% tests passed out of 41 |
| Scope (`git diff --stat main -- src tests CMakeLists.txt`) | ✅ empty |
| `grep -n "null-sink\|PULSE_SINK" docs/AUDIO_LATENCY.md` | ✅ no matches |

## E2E Verification

1. Measurement reproducibility: ✅ 3 runs at default period, means 872.6–883.1 (within ±5 %), `backend=PulseAudio … internalPeriod=900 x 4`, silent, default sink unchanged.
2. Citation check: ✅ `snd_pcm_delay`, `AudioGetCurrentHostTime`, `m_LastPosition`, `PA_STREAM_INTERPOLATE_TIMING`, `m_LastBeatUpdate.Ago()` all found at the cited lines.
3. Doc coherence: ✅ "does not claim how StepMania does it" removed; A–E table present; decision recorded with links to #81 and the #71 comment.
4. Issue state: ✅ #71 decision comment posted; follow-up #81 filed (owner approved B + C).
5. No behavior change: ✅ 41/41, scope diff empty.

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `docs/AUDIO_LATENCY.md` | UPDATE | +230/-22 |
| `AGENTS.md` | UPDATE | +2/-2 |
| `.agents/reports/036-music-clock-granularity-spike-plan-report.md` | CREATE | report |
| `.agents/plans/036-…-plan.md` → `.agents/plans/completed/` | MOVE | — |

## Deviations from Plan

- **Owner decision applied (overrides plan's "pending" defaults).** Doc records "Owner decision (2026-10-03): B + C approved" with rationale instead of "Owner decision: pending". Task 6 became "record the decision" (comment on #71) instead of a decision request. Task 7 executed: follow-up #81 filed and added to the blaze4k project board (status Backlog).
- **AGENTS.md amended in this PR** (plan Open Question 2 deferred it to the follow-up; owner asked for it now). Principle 1 and design pattern 1 only. Scope diff therefore shows `AGENTS.md` besides `docs/AUDIO_LATENCY.md`; `src/`, `tests/`, `CMakeLists.txt` untouched. `README.md:9` still repeats the old principle-1 wording (not edited, to keep the change minimal).
- **`docs/AUDIO_LATENCY.md:63` reworded** ("`PULSE_SINK=` overrides" → "a per-stream sink override") so the plan's `PULSE_SINK` grep returns nothing; the sentence's meaning (logged name can differ from the actual sink) is unchanged.
- **#58 Decision bullet clarified**: "`MusicClock` stays unchanged" → "… unchanged for #58", to avoid contradicting the B + C decision.
- **128-frame row** not re-run (plan's Task 2 runs 0/480/256 only); kept as a single planning-run row, marked as such, estimate column "not comparable".
- **18.79 ms anchor outlier** did not reproduce in the 3 default runs (max ≤ 5.87 ms); the doc reports both.
- No tests written: docs-only spike (plan: "No new tests and no code changes"). The probe stays uncommitted in the scratchpad.

## Tests Written

None (docs-only spike; validation = build + 41/41 + citation verification + silent probe runs).
