# Code Review: feature/010-judgment-scoring-constants

**Scope**: Branch `feature/010-judgment-scoring-constants` vs `main`, including all uncommitted work
(tracked modifications + untracked files). GitHub issue #10 ([B2] Data-driven judgment and scoring
constants from OpenITG).
**Recommendation**: APPROVE WITH NITS

## Summary

The change introduces a pure `JudgmentConstants` value type with compiled OpenITG-seeded defaults and
lookup helpers (`src/timing/`), a non-throwing `nlohmann/json` loader with per-field merge and
whole-table fallback (`src/data/`), a seeded `assets/data/judgment_constants.json`, a non-fatal
`App::init()` load, and a dedicated test target. Values were verified against the cited OpenITG commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` and all match; the build is warning-free and 10/10 tests
pass. The module correctly encodes the ratified decisions (Fantastic/Excellent/Great = +5/+4/+2 DP,
six tap judgments, quad=100%/triple=99%, no Attack window, roll=0.350). Remaining issues are minor
validation/test-coverage nits, not correctness blockers.

## Value Provenance Verification

Sampled against the cloned upstream repo at the pinned commit (`/tmp/opencode/openitg`,
`HEAD == f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`).

| Constant | Tundra | Upstream | Source | Match |
|----------|--------|----------|--------|-------|
| `windows.fantastic` | 0.0215 | `JudgeWindowSecondsMarvelous=0.021500` | `metrics.ini` | ✅ |
| `windows.excellent` | 0.0430 | `JudgeWindowSecondsPerfect=0.043000` | `metrics.ini` | ✅ |
| `windows.great` | 0.1020 | `JudgeWindowSecondsGreat=0.102000` | `metrics.ini` | ✅ |
| `windows.decent` | 0.1350 | `JudgeWindowSecondsGood=0.135000` | `metrics.ini` | ✅ |
| `windows.way_off` | 0.1800 | `JudgeWindowSecondsBoo=0.180000` | `metrics.ini` | ✅ |
| `windows.hit_mine` | 0.0700 | `JudgeWindowSecondsMine=0.070000` | `metrics.ini` | ✅ |
| `windows.hold_ok` | 0.3200 | `JudgeWindowSecondsOK=0.320000` | `metrics.ini` | ✅ |
| `windows.hold_roll` | 0.3500 | `JudgeWindowSecondsRoll=0.350f` (compiled default) | `PrefsManager.cpp:94` | ✅ |
| `judge_window_scale/add` | 1.0 / 0.0 | `JudgeWindowScale/Add` | `metrics.ini:90-91` | ✅ |
| DP/grade weights | 5/4/2/0/-6/-12/-6/5/0 | `PercentScoreWeight*` / `GradeWeight*` | `metrics.ini` | ✅ |
| Life deltas | 0.008/0.008/0.004/0/-0.05/-0.1/-0.05/0.008/-0.08 | `LifeDeltaPercentChange*` | `metrics.ini` | ✅ |
| `merciful_drain` | false | `MercifulDrain=0` | `metrics.ini` | ✅ |
| Grade tiers | 17 thresholds `1.00 … -1000` | `[PlayerStageStats] GradePercentTier01..17` | `metrics.ini:4433-4449` | ✅ |
| Combo (Great+ continues) | `continues_combo` | `MinScoreToContinueCombo=TNS_GREAT`; `Player.cpp:1547-1567` | ✅ |
| Window boundary `|delta| <= w` | `classify_tap` | `Player.cpp:957-963` uses `<=` | ✅ |

Note: the `[Grade]` theme labels for tiers 1–4 are star glyphs upstream, not `quad_star`/`triple_star`;
Tundra uses its own internal identifiers deliberately (plan-documented), so this is not a value error.

Internal consistency:

- Compiled defaults (`src/timing/judgment_constants.cpp:15-52`) == seed JSON
  (`assets/data/judgment_constants.json`) — confirmed by test 6 and by direct comparison.
- Test 1 pins the same OpenITG values, so a future drift of `compiled_defaults()` fails CI.

## Issues Found

### Critical

None.

### High Priority

None.

### Medium Priority

None.

### Suggestions (Low)

1. **`src/timing/judgment_constants.cpp:92-97` — `validate()` never checks `grade_tiers[0].min_percent`
   for finiteness.** The loop starts at `i = 1` (to compare against a predecessor), so the highest tier
   is the only one whose `std::isfinite` is not asserted. `nlohmann` rejects overflow-to-`inf` at parse
   time and JSON has no `NaN`, so this is not currently reachable, but the check is inconsistent with
   every other tier and should be done for all indices.
2. **`src/data/judgment_constants_loader.cpp:97-104, 110-114, 172` — JSON `grade_tiers[*].label` is
   validated but discarded.** `override_grade_tiers` requires a string `label`, then sets it to
   `nullptr` and `sync_tier_labels` overwrites every label from the compiled table. Editing labels in
   JSON silently has no effect, which is mildly surprising for a schema field that is present and
   validated. Documented as a deviation in the implementation report; consider dropping `label` from
   the accepted JSON schema or noting it as non-configurable in the seed file.
3. **`tests/judgment_constants_test.cpp` — the actual startup path
   (`load_judgment_constants_from_candidates`) is untested.** Only `load_judgment_constants` is
   exercised, so candidate precedence and the "no candidate found" fallback branch (the one `App::init`
   uses) have no automated coverage.
4. **`src/data/judgment_constants_loader.cpp:26-35` — `override_number<int>` silently truncates
   non-integral JSON numbers and is UB for out-of-range doubles.** `it->get<int>()` on a `number_float`
   performs `static_cast<int>`, so `"fantastic": 4.9` becomes `4` without warning, and a finite but
   huge value (e.g. `1e20`) is out-of-range for `int`. Validate integrality/range (or load as `double`
   then reject non-integers) rather than relying on the cast.
5. **`src/data/judgment_constants_loader.cpp:128-132` — no size cap when reading the config file.**
   `buffer << file.rdbuf()` reads the whole file unbounded, contrary to the project's "untrusted input,
   allocation-capped" principle for data files. A cap with fallback would be more consistent.
6. **`src/app/app.cpp:31-37` — severity routing is coupled to the loader's message wording.** `init()`
   chooses `std::cerr` vs `std::cout` by prefix-matching `"[JudgmentConstants] Warning:"`. A future
   wording change silently routes warnings to stdout. Returning an explicit severity/status alongside
   the message would be more robust.
7. **`src/timing/judgment_constants.hpp:23` — `enum class HoldJudgment` is defined but never used.**
   Dead interface surface; remove or defer to B4 until hold judgment is implemented.
8. **`src/timing/judgment_constants.cpp:89-91` — `if (grade_tiers.empty())` is dead code** for
   `std::array<GradeTier, 17>` (always false). Harmless, but misleading.
9. **Seed asset is not deployed next to the binary.** No CMake copy/install places
   `assets/data/judgment_constants.json` in the build output, and `App::init` resolves candidates
   relative to CWD, so a standalone run always falls back to compiled defaults. Explicitly deferred to
   C4/C5 in the plan, but worth tracking so the "data-driven" path is actually reachable in shipped
   builds.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j16`, incl. forced recompile of the 3 new TUs) | PASS — zero warnings under `-Wall -Wextra -Wpedantic` |
| Tests (`ctest --test-dir build --output-on-failure`) | PASS — 10/10 (9 prior + `judgment_constants_test`), 0 skipped |
| Lint | N/A (project has no separate linter; warnings-as-signal via compiler flags) |
| Upstream provenance sample | PASS — windows/weights/life/grade tiers/combo match pinned OpenITG commit |
| Module purity (`src/timing/` free of SDL/audio/filesystem/JSON) | PASS |

## What's Good

- Sound separation: the pure model (`src/timing/`) has no platform/IO dependencies and the JSON/
  filesystem concerns live in `src/data/`, mirroring `music_clock`.
- Loader is genuinely non-throwing (`json::parse(..., false)` + `error_code` filesystem + `try/catch`
  around field overrides) and falls back to a whole valid table rather than a partially-invalid one.
- Strong test coverage of the acceptance criteria: defaults parity, boundary classification incl. NaN
  and symmetry, combo semantics, grade lookup, partial overrides, and missing/malformed/invalid
  fallbacks.
- Per-field merge over compiled defaults keeps partial JSON safe, and `compiled_defaults()` is a
  function-local static returned by const reference.
- Values encode the ratified decisions exactly and cite the upstream commit; the seed JSON carries a
  `source` provenance string.

## Recommendation

Approve with nits. The implementation satisfies issue #10's acceptance criteria and faithfully
reproduces OpenITG values. The suggested fixes are optional hardening/cleanup — most credibly (1) the
`grade_tiers[0]` finiteness check, (4) integral/range validation for integer weights, and (3) a test
for `load_judgment_constants_from_candidates` — none block this change.

Base directory for this skill: /home/lauri/.claude/skills/review
