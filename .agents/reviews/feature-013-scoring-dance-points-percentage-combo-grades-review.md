# Code Review: feature/013-scoring-dance-points-percentage-combo-grades

**Scope**: Branch `feature/013-scoring-dance-points-percentage-combo-grades` vs `main`, including uncommitted modifications and untracked files (issue #13, [B5] Scoring).
**Recommendation**: APPROVE WITH NITS

## Summary

Reviewed the event-sourced scoring layer (`score_keeper.*`, `hud_renderer.*`, `score_keeper_test.cpp`) plus the
`GameplayView` integration and CMake wiring. The keeper is genuinely pure (no clocks/platform headers), derives
everything from the B4 `JudgmentEvent` log plus a chart-derived denominator, and reproduces OpenITG row/miss/combo
semantics accurately. I verified the key rules against the OpenITG checkout at
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (`ScoreKeeperMAX2.cpp`, `Player.cpp`, `NoteDataWithScoring.cpp`,
`NoteData.cpp`, `PlayerStageStats.cpp`, `PercentageDisplay.cpp`): last-tap/miss-dominance, `fTapNoteOffset` sign,
combo thresholds, possible-DP formula, DP% formula, grade lookup, and percent truncation all match. Build is clean
and 13/13 tests pass. Findings are latent robustness/contract and test-coverage items; no defect is reachable
through the production `GameplayView` path.

## Issues Found

### Critical
None.

### High Priority
None.

### Medium Priority

1. **`src/gameplay/score_keeper.cpp:95-130` — per-note idempotence is not actually implemented; duplicates can corrupt multi-note rows.**
   The header contract (`score_keeper.hpp:44`) says consumption is "idempotent per note", but `apply_tap_like`
   deduplicates only per **row** (`aggregate.judged >= aggregate.expected`). For a 2-note row, a duplicated event
   for note A increments `judged` to `expected`, resolves the row with A's window, and the guard then swallows note
   B's real event. Example: row {A, B}; events `A Fantastic`, `A Fantastic`(dup), `B Miss` → scores `+5`/combo 2
   instead of `-12`/combo 0. Not reachable today because `JudgmentEngine::drain_new_events`
   (`judgment_engine.cpp:385-391`) delivers each event exactly once, so this is latent — but it contradicts the
   documented contract and is untested (test 11 only duplicates single-note rows). Fix: guard per note_index, or
   document that duplicate `Tap`/`Miss` events are unsupported.

### Suggestions

2. **`src/gameplay/hud_renderer.cpp:92-97` — display clamp is unconditional.** OpenITG clamps only when
   `m_Last <= m_LastMax` (`PercentageDisplay.cpp:110-111`); Tundra always clamps to `[0,1]`. With v1's no-mods
   scope `actual <= possible` always holds (possible is chart-derived, actual is bounded by the same weights), so
   there is no observable difference, but the condition could be carried over for fidelity.
3. **`src/gameplay/score_keeper.cpp:23-44` — beat-sorted `chart.notes` is an undocumented precondition.** Row
   grouping (`note.beat != prev_beat`) only works if equal beats are contiguous. The parser guarantees this
   (`note_parser.cpp:244-248`), but the header does not state it and the keeper silently fragments rows on an
   unsorted chart. Consider asserting/documenting the invariant in `reset()`.
4. **`src/gameplay/score_keeper.cpp:155-163` — `apply_hold` scores any event with a valid `note_index`.** It does
   not verify the note is a hold/roll (nor that its head was hit). The B4 engine only emits hold outcomes after a
   hit head, so this is defensive-only; a cheap sanity check would make the invariant explicit.
5. **`tests/score_keeper_test.cpp:555-596` — AC2/AC3 exercised with a hand-built chart, not the checked-in reference pack.**
   The reference pack exists (`tests/fixtures/reference_pack/`) and is used by parser tests. A perfect-play run over
   a parsed reference chart (asserting `percent == 1.0`, quad star) would close the AC2/AC3 loop end-to-end; the
   hand-computed case already validates the formula, so this is coverage, not correctness.
6. **`src/gameplay/gameplay_view.cpp:191-193 — comment is stale.** The comment says the uninitialized-renderer no-op
   protects the HUD, but `render()` early-returns at line 158 when `!renderer.is_initialized()`, so `hud_.render`
   is never reached headless. Harmless but misleading.

## Validation Results

| Check | Status |
|-------|--------|
| Type Check / Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`) | PASS (0 warnings) |
| Lint | N/A (no linter configured for this project) |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS (13/13, incl. `score_keeper_test`) |

Additional checks:
- **Purity**: `rg` over `score_keeper.*` for SDL/GL/glad/miniaudio/chrono/steady_clock/performance_counter → clean
  (only the doc comment mentions "wall-clock"); no frame-delta/wall-clock input reaches scoring.
- **Parity** (OpenITG `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`): `LastTapNoteScoreTrack` greatest-offset/tie-later-track
  (`NoteDataWithScoring.cpp:133-159`) ↔ `score_keeper.cpp:119-125`; miss dominance ↔ `has_miss`; `fTapNoteOffset = hit - note`
  (`Player.cpp:929,1099`, `NoteTypes.h:14-16`) ↔ `delta_ms`; combo `>= TNS_GREAT` + miss-combo reset
  (`Player.cpp:1544-1553`, `ScoreKeeperMAX2.cpp:357-368`) ↔ `resolve_row`; possible DP
  `NumTaps*5 + NumHolds*5 + NumRolls*5` (`ScoreKeeperMAX2.cpp:440-452`, `NoteData.cpp:477-485,557-591`) ↔ `possible_dp`;
  DP% `actual/possible`, 0 if possible==0, 1 if equal (`PlayerStageStats.cpp:218-232`) ↔ `recompute_derived`;
  truncate-to-2-decimals after +1e-6 (`PercentageDisplay.cpp:135-146`) ↔ `format_percent`. Ratified decisions
  (Fantastic=+5/Great=+2, quad=100%/triple=99%, bitmap font, full-song `actual/possible` denominator,
  beat-keyed row aggregation with all-head rule + miss dominance, `continues_combo()`) all confirmed — not flagged.
- **Reference chart (AC2/AC3)**: test 13 builds a chart and hand-computes DP = 3, possible = 20, percent = 0.15,
  grade D, combo/max 1, counts F2/M1/OK1 — verified arithmetically. No parsed `reference_pack` chart is used (see
  suggestion 5).

## What's Good

- Clean event-sourced design: scoring is a single append-only consumer with no independent judgment and no timing input,
  exactly as AGENTS.md core principle 1 / PRD §6 requires.
- The row aggregation is the hard part, and it is correct: beat-keyed rows, mines excluded from rows and denominators,
  roll/OK head counts in `notes_in_row`, miss dominance, greatest-delta with later-column tie-break, and
  `continues_combo()` instead of raw enum comparison.
- Chart-derived denominator computed once in `reset()` and never recomputed — no frame logic in the scoring path.
- Good defensive null handling (`reset(nullptr)`, null constants) and a genuinely useful `is_complete()` for B6/C7.
- Tests are broad (15 cases): per-window weights, grade tiers, row/last-tap/miss semantics, combo rules, holds/rolls,
  mines, duplicates, incremental==batch, engine→drain→keeper integration, HUD formatters, and the `GameplayView` boundary.
- HUD is draw-only, uses the existing `GlQuadRenderer::draw_quad` primitive with an in-code 5×7 font (ratified), and its
  pure formatters are unit-covered despite GL not being headless-testable.

## Recommendation

No changes required to merge. The Medium idempotence finding is worth addressing (either make the guard per-note or fix
the contract wording) and the reference-pack test would strengthen AC2/AC3, but neither blocks: the production path
delivers each event exactly once and the scoring arithmetic is parity-correct.
