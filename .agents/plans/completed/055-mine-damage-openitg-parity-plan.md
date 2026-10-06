# Plan: Compare Mine Damage Against OpenITG / StepMania (#118)

## Summary

Issue #118 is a spike. The question: does a hit mine cost the same life in Blaze 4k as in OpenITG (and StepMania 5), both as
configured values and as applied by the life bar? **The upstream lookup was done while writing this plan.** OpenITG commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` is the same one already pinned in `assets/data/judgment_constants.json:3`. StepMania 5
is pinned at commit `825467bcd81c812b33ad684dc04dd151b2d5dec3` (the head of `5_1-new`). The full **Verification Report** is below.
Short version:

- **Every mine value matches OpenITG** as it runs: the life delta −0.050, the full-bar ("hot") override −0.10, the regain debt
  5 / max 10, the fail debt 10 / 10, merciful drain off, DP and grade weight −6, no combo change, mine window 70 ms.
- **The life path also matches**: the same order of operations as `LifeMeterBar::ChangeLife`, and the hot override applies to mines
  because `TNS_HIT_MINE < TNS_GOOD` in OpenITG's enum.
- **OpenITG mines are about 3× gentler than stock StepMania 5** (SM5: −0.160 per mine, and the hot penalty applies to any loss).
  Blaze is not over-tuned.
- **Why mines feel harsh** (expected behavior, not a bug): on a full bar a mine costs −0.10, the same as a Miss. After any mine
  the next **4** positive rows refill nothing and the 5th pays. So a mine at mid-bar really costs about 0.05 + 4 × 0.008 = **0.082**
  compared with not touching it. The issue text says "5 good steps refill nothing". The source shows 4 suppressed and the 5th paying,
  and Blaze already does exactly that (`tests/life_keeper_test.cpp:402-415`).

Two small mismatches were found. Neither changes gameplay values:

| # | Mismatch | Class |
|---|----------|-------|
| M1 | `LifeKeeper` has **no per-mine duplicate guard**. The `HitMine` case calls `apply()` directly (`src/gameplay/life_keeper.cpp:94-96`), but the header promises "idempotent per note" (`src/gameplay/life_keeper.hpp:50`) and taps and holds have guards (`life_keeper.cpp:111-113,150-152`). OpenITG grades the mine (`tn.result.tns = score`, `Player.cpp:1096`), so the step search (`bAllowGraded=false`, `Player.cpp:904-905`) never finds it again. The engine already emits one `HitMine` per mine (`judgment_engine.cpp:166-169`; `tests/judgment_engine_test.cpp:667-692`), so this does not affect players today. But a duplicate event would take life twice | **Trivial: fix in this issue** with unit tests (AC 4 + AC 5) |
| M2 | Wrong provenance comment: `src/timing/judgment_constants.hpp:83-84` says `hot_downgrade` is "Pinned from src/PrefsManager.cpp". There is no such preference. It is the literal `-0.10f` in `LifeMeterBar.cpp:119` (tap) and `:175` (hold NG) | **Trivial: fix the comment** in this issue |

No follow-up bug is needed for the life path. The `ScoreKeeper` has the same missing mine guard for DP, but #118 is about life,
so it is only flagged (Open Question 1).

## User Story

As an ITG player who steps on the occasional mine
I want a mine to cost exactly what it costs on an OpenITG cabinet, and only once
So that the life bar feels like the real game, and I know the harshness is authentic, not a bug

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX (spike with a trivial fix) |
| Complexity | LOW |
| Systems Affected | `gameplay/life_keeper` (mine duplicate guard), `timing/judgment_constants.hpp` (comment only), `tests/life_keeper_test.cpp` |
| GitHub Issue | #118 ([TODO-30] Compare mine damage against OpenITG / StepMania) |
| Related | #56 (mines exploded too easily: crossing cursor, merged), #57 / plan 031 (same spike shape for judgment windows), #67 (MercifulBeginner) |
| Branch (suggested) | `feature/055-mine-damage-openitg-parity` |
| PRD Phase | N/A |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` configured and valid for this checkout. `cmake --build build` is clean on `main` @ `246add8` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | `main` @ `246add8`, sandboxed ctest (see Validation), 2.9 s. Target after this plan: **51/51** (no new executables, only new cases in `life_keeper_test`) |
| Sandbox | `bwrap … --tmpfs /dev/snd --unshare-net` | Tests touch real audio hardware, so **always** run them sandboxed. Use the same prefix for any direct test-binary run |
| Test harness | plain `TEST_CHECK` + `std::abort` executable | `tests/life_keeper_test.cpp:18-24`, registered at `tests/CMakeLists.txt:147-155`, linked to `blaze4k_core` |
| OpenITG source | raw files at commit `f2c129fe…` | `src/LifeMeterBar.cpp`, `src/PrefsManager.cpp`, `src/Player.cpp`, `src/ScoreKeeperMAX2.cpp`, `src/GameConstantsAndTypes.h`, `assets/patch-data/Themes/default/metrics.ini`. See "Obtaining upstream source" |
| StepMania 5 source | raw files at commit `825467bc…` | `src/LifeMeterBar.cpp`, `src/PrefsManager.cpp`, `src/Player.cpp`, `src/ScoreKeeperNormal.cpp`, `src/GameConstantsAndTypes.h`, `Themes/_fallback/metrics.ini` |
| `TODO.md` | git-ignored (`.gitignore:27`) | Line 46 already carries `(#118)`. Do not stage it. Ticking its checkbox is the owner's call |
| Forward refs to #118 | `.agents/issues/todo-issues.md:1059`, `TODO.md:46` | Both are the issue record only, with no code placeholders. No action needed |

### Obtaining upstream source (for re-verification during implementation)

The scratchpad is session-specific. Fetch again into a fresh directory **outside the repo** if it is gone:

```bash
S=<scratchpad>/upstream; mkdir -p "$S/oitg" "$S/sm5"
O=f2c129fe65c65e4a9b3a691ff35e7717b4e8de51
for f in src/LifeMeterBar.cpp src/PrefsManager.cpp src/Player.cpp src/ScoreKeeperMAX2.cpp src/GameConstantsAndTypes.h \
         assets/patch-data/Themes/default/metrics.ini; do
  curl -sfL -o "$S/oitg/$(basename $f)" "https://raw.githubusercontent.com/openitg/openitg/$O/$f"; done
M=825467bcd81c812b33ad684dc04dd151b2d5dec3
for f in src/LifeMeterBar.cpp src/PrefsManager.cpp src/Player.cpp src/ScoreKeeperNormal.cpp src/GameConstantsAndTypes.h \
         Themes/_fallback/metrics.ini; do
  curl -sfL -o "$S/sm5/$(basename $f)" "https://raw.githubusercontent.com/stepmania/stepmania/$M/$f"; done
```

Below, OpenITG paths are relative to the OpenITG repo root and SM5 paths to the StepMania repo root. "`metrics.ini`" means
`assets/patch-data/Themes/default/metrics.ini` (OpenITG) or `Themes/_fallback/metrics.ini` (SM5).

---

## Verification Report (to be posted on #118; AC 1–3)

OpenITG commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`. StepMania 5 commit `825467bcd81c812b33ad684dc04dd151b2d5dec3` (`5_1-new`).
Names: OpenITG `TNS_HIT_MINE` / SM5 `TNS_HitMine` → Blaze `TapJudgment::HitMine` / `JudgmentKind::HitMine`.

### How OpenITG resolves a preference at runtime

The compiled default in `src/PrefsManager.cpp` is overridden by the theme's `[Preferences]` section (`metrics.ini:22`). Dedicated
cabinets launch with `[Preferences-cabinet]` (`:239`, `Fallback=Preferences-arcade` `:240`), and that falls back to `[Preferences-arcade]`
(`:165`, `Fallback=Preferences` `:166`). **Neither arcade section overrides any life, regen, mine or DP key** (checked with
`sed -n 165,290p metrics.ini | grep -i "life\|regen\|mine\|merciful\|drain"`, no hits). So `[Preferences]` is the effective value
on every OpenITG build. Plan 031 documented the same resolution chain for the judge windows.

### AC 1 — Mine life delta and full-bar override

| Blaze key | Blaze value (JSON / compiled) | OpenITG effective | OpenITG source | OpenITG compiled default (overridden) | Match |
|---|---|---|---|---|---|
| `life_deltas.hit_mine` | −0.05 (`assets/data/judgment_constants.json:67`; `src/timing/judgment_constants.hpp:76`) | **−0.050** | `metrics.ini:126` `LifeDeltaPercentChangeHitMine=-0.050`; read at `src/LifeMeterBar.cpp:114` | −0.160 (`src/PrefsManager.cpp:105`) | ✅ |
| `life_deltas.hot_downgrade` | −0.10 (`json:70`; `judgment_constants.hpp:85`) | **−0.10** (hard-coded literal, not a preference) | `src/LifeMeterBar.cpp:118-119` `if( IsHot() && score < TNS_GOOD ) fDeltaLife = -0.10f;` (hold NG: `:174-175`) | — | ✅ (M2: comment cites the wrong file) |
| Hot override covers mines | `delta_for_tap` includes `HitMine` (`src/gameplay/life_keeper.cpp:181-184`) | yes | enum order `TNS_NONE, TNS_HIT_MINE, TNS_AVOIDED_MINE, TNS_MISS, TNS_BOO, TNS_GOOD…` (`src/GameConstantsAndTypes.h:131-139`), so `TNS_HIT_MINE < TNS_GOOD` | — | ✅ |
| "Hot" threshold | `life >= 1.0` (`life_keeper.hpp:75`) | `m_fLifePercentage >= 1` | `src/LifeMeterBar.cpp:280-283` | — | ✅ |
| Mine applied immediately, once | `HitMine` → `apply()` (`life_keeper.cpp:94-96`), no guard (**M1**) | once per mine | `Player.cpp:1057-1065` (`ChangeLife( score )` on `TNS_HIT_MINE`), then `tn.result.tns = score` (`:1096`). The step search skips graded notes (`GetClosestNote(…, false)` `:904-905`, `GetClosestNoteDirectional` `:790-791`), and so does the held crossing (`CrossedMineRow` → `Step` `:1461-1488`) | — | ✅ engine / ⚠️ keeper (M1) |
| Avoided mine | no life change (`life_keeper.cpp:97-102`) | no `ChangeLife` call | `Player.cpp:1414-1416` (only sets `TNS_AVOIDED_MINE`) | — | ✅ |
| Mine window | `windows_seconds.hit_mine` 0.07 (`json:12`) | 0.070 | `metrics.ini:93` `JudgeWindowSecondsMine=0.070000`; used at `Player.cpp:946-949` | 0.090 (`src/PrefsManager.cpp:95`) | ✅ (already verified in #57) |
| Order of operations | `delta_for_tap` (hot) → `apply` (merciful → regen → failed-freeze → fail-debt → clamp) (`life_keeper.cpp:162-251`) | same | `ChangeLife(TNS)` `src/LifeMeterBar.cpp:100-153` → `ChangeLife(float)` `:202-261` | — | ✅ |

### AC 2 — Side effects beyond the life delta

| Blaze key | Blaze | OpenITG effective | OpenITG source | Match |
|---|---|---|---|---|
| `regen_combo_after_miss` | 5 (`json:71`) | 5 (no metrics override) | `src/PrefsManager.cpp:120`; applied for **any** negative delta, mines included, at `src/LifeMeterBar.cpp:214-226` | ✅ |
| `max_regen_combo_after_miss` | 10 (`json:73`) | 10 | `src/PrefsManager.cpp:122` | ✅ |
| `regen_combo_after_fail` / `max_…_after_fail` | 10 / 10 (`json:72,74`) | 10 / 10 | `src/PrefsManager.cpp:119,121`; `src/LifeMeterBar.cpp:245-253` | ✅ |
| Regain debt semantics | `max(debt−1,0)` then suppress while `> 0` (`life_keeper.cpp:222-226`) | same | `src/LifeMeterBar.cpp:208-213` | ✅ |
| Debt after one mine | 4 suppressed, the 5th pays (`tests/life_keeper_test.cpp:402-415`) | 4 suppressed, the 5th pays | follows from `:211-212` with debt 5 | ✅ (the issue text "5 good steps refill nothing" is off by one; the code is right) |
| `merciful_drain` | false (`json:75`) | false | `metrics.ini:156` `MercifulDrain=0` (compiled default true, `src/PrefsManager.cpp:124`) | ✅ |
| Life difficulty / progressive lifebar | not modeled (inert) (`life_keeper.cpp:211-213`) | 1.0 / 0 | `src/PrefsManager.cpp:98,221`, no metrics override | ✅ (inert) |
| `dp_weights.hit_mine` | −6 (`json:26`) | −6 | `metrics.ini:116` `PercentScoreWeightHitMine=-6` (compiled −2, `src/PrefsManager.cpp:136`); `ScoreKeeperMAX2.cpp:333-341,509` | ✅ |
| `grade_weights.hit_mine` | −6 (`json:37`) | −6 | `metrics.ini:106` `GradeWeightHitMine=-6` (compiled −8, `src/PrefsManager.cpp:146`) | ✅ |
| Combo | unchanged by a hit mine (`src/gameplay/score_keeper.cpp:81-85`) | unchanged | `ScoreKeeperMAX2.cpp:333-341` (no combo code); row scoring skips mines (`Player.cpp:1130-1134`) | ✅ |
| Beginner DP clamp | `merciful_` → `dp_weight` clamps negatives (`score_keeper.cpp:12-13`) | `max(0, w)` | `ScoreKeeperMAX2.cpp:529-530` (MercifulBeginner, #67) | ✅ |
| DP after failing | not modeled (Fail-On ends the song; Fail-Off never sets `failed`) | DP for a mine is skipped once `bFailed` | `ScoreKeeperMAX2.cpp:337` | n/a (cannot be reached in Blaze) |

### AC 3 — StepMania 5 defaults (comparison only; OpenITG stays the reference)

| Value | OpenITG (effective) | StepMania 5 default | SM5 source |
|---|---|---|---|
| Mine life delta | −0.050 | **−0.160** | `metrics.ini:656` `LifePercentChangeHitMine=-0.160`; `src/LifeMeterBar.cpp:123` |
| Miss / Boo (W5) for scale | −0.100 / −0.050 | −0.080 / −0.040 | `metrics.ini:655,654` |
| Full-bar override | `-0.10` when `score < TNS_GOOD` | `min(delta, -0.10)` for **any** negative delta, when `HarshHotLifePenalty` (default **true**) | `src/LifeMeterBar.cpp:129-131`; `src/PrefsManager.cpp:202` |
| Hot threshold | `>= 1` | `>= HotValue` = 1.0 | `src/LifeMeterBar.cpp:278-280`; `metrics.ini:639` |
| Initial life | 0.5 | 0.5 | `metrics.ini:637` |
| Regain debt per loss / max | 5 / 10 | 5 / **5** | `src/PrefsManager.cpp:199-200` (comment: "this was 10 by default in SM3.95") |
| Regain debt on fail | 10 / 10 | not present (removed) | `src/LifeMeterBar.cpp:197-245` has no fail-debt branch |
| Merciful drain | off | off | `src/PrefsManager.cpp:201` |
| DP (PercentScoreWeight) hit mine | −6 | −2 | `metrics.ini:1352` |
| Grade weight hit mine | −6 | −8 | `metrics.ini:1364` |
| Hit mine breaks combo | no | no (`MineHitIncrementsMissCombo=false`) | `metrics.ini:577`; `src/ScoreKeeperNormal.cpp:397-406` |
| Mine window | 0.070 | 0.090 ("same as great") | `src/Player.cpp:129-130` |
| Once per mine | graded on hit | graded on hit (`GetClosestNote(…, false)`) | `src/Player.cpp:2232` |

**Conclusion:** Blaze mines use OpenITG values exactly, and they cost under a third of a stock StepMania 5 mine. The harsh feeling
comes from two authentic OpenITG rules: (a) on a full bar a mine is forced to −0.10, the same as a Miss, and (b) the regain debt.
After a mine the next 4 positive rows refill nothing, so a mine at mid-bar effectively costs about 0.082 compared with a clean row.
**No value changes.**

---

## Pinned Semantics (the contract the tests pin)

- One `HitMine` event changes life **at most once per mine note**. A second `HitMine` with the same `note_index` is ignored, just as
  duplicate tap and hold-outcome events already are (`life_keeper.cpp:111-113,150-152`).
- A `HitMine` event is applied only if `note_index` is in range **and** `chart->notes[note_index].type == NoteType::Mine`. Otherwise it is
  ignored, like `consume_hold_outcome` rejects non-hold notes (`life_keeper.cpp:147-149`). Engine events always satisfy this
  (`judgment_engine.cpp:158-180` uses the mine's own index).
- The guard **reuses `note_scored_`**. Mines never set it today, because `consume_tap_like` returns at `row < 0` before marking
  (`life_keeper.cpp:114-118`), so no new member is needed. Update the member comment (`life_keeper.hpp:87`) to say it covers mines too.
- Values and order of operations do not change. `delta_for(event)` stays the delta source, so the hot override still applies.

---

## Patterns to Follow

### Per-note duplicate guard (mirror exactly)

```cpp
// SOURCE: src/gameplay/life_keeper.cpp:142-155
void LifeKeeper::consume_hold_outcome(const JudgmentEvent& event) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= hold_scored_.size()) {
        return;
    }
    if (!chart_->notes[static_cast<std::size_t>(index)].is_hold_or_roll()) {
        return; // only hold/roll heads produce hold outcomes
    }
    if (hold_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard: one outcome per hold/roll
    }
    hold_scored_[static_cast<std::size_t>(index)] = true;
    apply(delta_for(event));
}
```

### OpenITG provenance comments

```cpp
// SOURCE: src/gameplay/life_keeper.cpp:179-184
    // Hot downgrade: while the bar is full, WayOff/Miss/mine are forced to the
    // hot penalty (OpenITG `IsHot() && score < TNS_GOOD`, LifeMeterBar.cpp:118-119).
```

Cite `File.cpp:line` from the pinned commit. Do not cite issue numbers in code comments unless the surrounding code already does
(`judgment_engine.cpp:375` cites `#56`).

### Tests (numbered blocks, hand-computed arithmetic in comments, one `std::cout` line per block)

```cpp
// SOURCE: tests/life_keeper_test.cpp:400-416
    // 12. Regen: after a loss, positive gains are suppressed for the debt period.
    {
        blaze4k::Chart chart = tap_rows(10);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_miss(0, 0)); // life 0.4, debt = 5
        ...
        TEST_CHECK(approx(keeper.life(), 0.408)); // debt paid on the 5th win
        std::cout << "  - 12. combo-to-regain suppresses post-loss gains.\n";
    }
```

Engine + keeper integration: build a `Chart`, call `engine.reset(&chart, &k)`, drive `engine.handle_step(col, t)` and
`engine.update(t, held_col(c))`, then `engine.drain_new_events(drained); keeper.consume(drained);`
(`tests/life_keeper_test.cpp:427-450`). Held-through-mine driving: `tests/judgment_engine_test.cpp:669-692` (press, then a
time grid of `update(…, held_col(0))`) and `:704-714` (a press 0.5 s before the mine is out of the mine window, so the mine stays live).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/life_keeper.hpp` | UPDATE | Declare `consume_hit_mine(const JudgmentEvent&)`; extend the `note_scored_` comment to mines |
| `src/gameplay/life_keeper.cpp` | UPDATE | M1: route `JudgmentKind::HitMine` through a guarded `consume_hit_mine` |
| `src/timing/judgment_constants.hpp` | UPDATE | M2: fix the `hot_downgrade` provenance comment (comment only) |
| `tests/life_keeper_test.cpp` | UPDATE | Make tests 10 and 11 use real mine notes; add block 20 (mine parity: duplicate guard, stepped and held once, regen debt, hot plus debt) |

No JSON, loader, engine or ScoreKeeper changes. No new test target.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Guarded mine consumption in `LifeKeeper` (M1)

- **File**: `src/gameplay/life_keeper.hpp`, `src/gameplay/life_keeper.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: add `void consume_hit_mine(const JudgmentEvent& event);` next to `consume_hold_outcome` (`life_keeper.hpp:71`). Change the
    `note_scored_` comment (`:87`) to `// per-note idempotence guard for tap-like events and hit mines`.
  - Source: replace the `HitMine` case body (`life_keeper.cpp:94-96`) with `consume_hit_mine(event); // immediate on trigger, once per mine`.
  - Add `consume_hit_mine` after `consume_hold_outcome`. Mirror its shape: bounds-check against `note_scored_.size()`; return if
    `chart_->notes[i].type != NoteType::Mine` (`// only mine notes explode`); return if `note_scored_[i]`
    (`// duplicate guard: a mine takes life at most once (OpenITG grades it, Player.cpp:1096)`); set `note_scored_[i] = true`;
    `apply(delta_for(event));`.
- **Mirror**: `src/gameplay/life_keeper.cpp:142-155`
- **Validate**: `cmake --build build`. Expect `life_keeper_test` to **fail at test 11** (it uses `make_mine` on tap indices) until Task 3.

### Task 2: Fix the `hot_downgrade` provenance comment (M2)

- **File**: `src/timing/judgment_constants.hpp:80-85`
- **Action**: UPDATE (comment only)
- **Implement**: replace "Pinned from src/PrefsManager.cpp at commit …" with: the value is the hard-coded `-0.10f` literal in
  `src/LifeMeterBar.cpp:119` (tap/mine, `score < TNS_GOOD`) and `:175` (hold NG) at commit
  `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`; OpenITG has no preference for it. Keep the `-0.10` value. **Do not** edit the JSON `source`
  string (`json:3`): it lists "PrefsManager.cpp + LifeMeterBar.cpp" together for five keys and is still correct as a whole.
- **Validate**: `cmake --build build`

### Task 3: Update existing tests to use real mine notes

- **File**: `tests/life_keeper_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - **Test 10** (`:329-350`): after `tap_rows(4)`, set `chart.notes[3].type = blaze4k::NoteType::Mine; chart.tap_count = 3; chart.mine_count = 1;`
    so `make_mine(3, 3)` targets a real mine. Incremental and batch must still agree.
  - **Test 11, mine sub-case** (`:376-383`): build `tap_rows(70)`, then append `make_note(0, 100.0, NoteType::Mine)` (index 70,
    `mine_count = 1`), fill to full with `full_keeper`, and consume `make_mine(70, 0)`. Still expect `0.9`.
- **Mirror**: test 11's hold sub-case (`:384-398`) already appends a special note at index 70 after 70 taps.
- **Validate**: build and run `life_keeper_test` sandboxed (see Validation). Tests 1–19 pass.

### Task 4: New block 20 — mine parity with OpenITG (#118)

- **File**: `tests/life_keeper_test.cpp` (insert before the final "All life/fail tests passed" line)
- **Action**: UPDATE
- **Implement** (one sub-block per item, each with hand-computed arithmetic in comments and a `std::cout` line):
  - **20a. Duplicate / foreign `HitMine` events take life once.** Chart: one mine at index 0 plus one tap at index 1. Consume `make_mine(0,0)`
    twice → `0.45` (this **fails before Task 1**: it gives 0.40). Then `make_mine(1,1)` (a tap index) and `make_mine(99,0)` (out of range)
    → life still `0.45`. Second keeper on the same chart (debt 0): `make_mine(1,1)` → still `0.5`, then
    `make_tap(1, 1, Fantastic, 0.0)` → `0.508`. This proves that a mine event on a tap's index neither takes life nor marks the tap scored.
  - **20b. Stepped on, then held through: life taken once (engine → keeper).** Chart: mine col 0 at 2.0 s. `engine.handle_step(0, 2.0)`.
    Then `for t in 2.0..3.0 step 0.01: engine.update(t, held_col(0))`; drain → consume. Assert exactly one `HitMine` in the drained
    log, zero `AvoidedMine`, and `keeper.life() == 0.45`.
  - **20c. Held through only: life taken once.** Chart: mine col 0 at 2.0 s. `engine.handle_step(0, 1.5)` (0.5 s away, so outside the
    70 ms window: no event and the mine stays live; same setup as `judgment_engine_test.cpp:704-707`). Then
    `for t in 1.5..3.0 step 0.01: engine.update(t, held_col(0))`; drain → consume. Assert one `HitMine`, zero `AvoidedMine`,
    life `0.45`. Repeat at grid 0.1 and 0.001 (a lambda like `hold_through_mine`, `judgment_engine_test.cpp:669`) to show the
    result does not depend on the update rate.
  - **20d. Mine regain debt matches `LifeMeterBar.cpp:208-226`.** Chart: mine at index 0 (t=0), taps at indices 1..6 (t=1..6).
    `make_mine(0,0)` → 0.45 (debt 5). Fantastic rows 1–4 → still `0.45`; row 5 → `0.458`. Second case, on a separate chart (mine at
    index 0, taps at indices 1..12): mine → 0.45, then `make_miss(1,1)` → 0.35 with debt `min(10, 5+5) = 10`. Fantastic rows 2–10
    (9 rows) are suppressed and row 11 pays (`0.358`). This pins that mines and misses share one debt.
  - **20e. Full bar: mine forced to the hot penalty, then debt.** Reuse the Task 3 test-11 chart shape (70 taps + mine at index 70; fill
    with Fantastic until `life() == 1.0`, at index `idx`). `make_mine(70, 0)` → `0.9` (not `0.95`). The next 4 Fantastic rows
    (`idx..idx+3`) → still `0.9`; the 5th → `0.908`. Needs `idx + 5 <= 70`: filling 0.5 → 1.0 takes 63 rows, so `idx = 63` and rows 63–67 fit.
  - **20f. A mine on the same beat as a tap never joins its row.** Chart: tap col 0 and mine col 1, both at beat/time 1.0. Consume the tap
    Fantastic → `0.508` right away (the row resolves without the mine), then `make_mine(1,1)` → `0.458`. Mirrors
    `Player.cpp:1130-1134` (row judging only for `tap`/`hold_head`).
- **Mirror**: tests 12 (`:400-416`), 13 (`:418-451`), 17 (`:558-574`); `count_kind` helper from `judgment_engine_test.cpp:556-559`
  (copy it locally into block 20; a lambda over the drained vector is enough).
- **Validate**: build, then sandboxed `life_keeper_test`. Expect every line through "20f" and "All life/fail tests passed".

### Task 5: Full suite and smoke

- **Validate**: see Validation. Expect **51/51**, no new warnings in touched files, and a headless smoke run that exits 0.

### Task 6: Post the Verification Report on #118 (AC 1–3)

- **Action**: copy the **Verification Report** section above, the M1/M2 table and a short "Resolution" paragraph into a scratch file and post it:
  `gh issue comment 118 --body-file <scratchpad>/issue118-report.md`. The Resolution says: all values match OpenITG; M1 fixed (name tests
  20a–20c), M2 comment fixed; no follow-up bug filed for life; the ScoreKeeper DP guard is the decision recorded under Open Question 1.
  Include the curl commands from "Obtaining upstream source" so anyone can reproduce the report.
- **Do not** change the issue state, labels or project board (closing it is `/issue-done`'s job).
- **Validate**: `gh issue view 118 --json comments --jq '.comments[-1].body' | head -5`

---

## Validation

```bash
# Build (warnings in touched files = lint gate; no linter configured)
cmake --build build 2>&1 | grep -E "warning|error" ; cmake --build build

# Tests: expect 51/51. Tests touch real audio hardware, so ALWAYS run sandboxed.
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Targeted (same sandbox prefix)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net \
  ctest --test-dir build -R "life_keeper_test|judgment_engine_test|score_keeper_test|judgment_constants_test|gameplay_screen_test" --output-on-failure

# Direct binary run (same sandbox prefix)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/life_keeper_test

# Values untouched (expect no diff)
git diff --exit-code assets/data/judgment_constants.json src/timing/judgment_constants.cpp

# Scope guard
git diff --stat   # only life_keeper.{hpp,cpp}, judgment_constants.hpp (comment), tests/life_keeper_test.cpp
```

## End-to-End Verification

1. **Automated (headless, required):** `life_keeper_test` block 20 drives the real `JudgmentEngine` → `drain_new_events` → `LifeKeeper`
   chain for a stepped-on mine and a held-through mine at three update rates, and proves one mine = one life loss (0.45). 20a proves a
   duplicated event cannot double-apply. 20d and 20e pin the OpenITG regain debt and hot penalty for mines. `judgment_engine_test` 16.x
   still proves the engine emits one event per mine.
2. **App smoke (headless, required):**
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   exits 0, and the log shows no judgment-constants fallback warning.
3. **Optional visual check:** `/verify` can play a chart with mines and screenshot the life bar. Not required, because no values or
   rendering change.
4. **Owner play-test (interactive; the agent doesn't run it):** mines cost the same as before. That is the point of the spike: the
   harshness is authentic OpenITG behavior.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The type check in `consume_hit_mine` silently ignores the existing tests that use `make_mine` on tap indices (tests 10 and 11) | Task 3 converts them to real mine notes before block 20. Task 1 notes the interim failure | In scope |
| `ScoreKeeper` has the same missing mine guard (`score_keeper.cpp:81-85`), so a duplicate `HitMine` would subtract DP twice | Not reachable today (the engine emits one event per mine, `judgment_engine_test` 16.7). The decision is Open Question 1 | Out of scope (flag only) |
| The owner expected "too much damage" to be a bug, and this spike changes no values | The report shows OpenITG = −0.05 at file:line, and SM5 = −0.16. Explain the hot penalty and regain debt (about 0.082 effective at mid-bar). A gentler non-OpenITG option would break AGENTS.md principle 2, so it is Open Question 2 | In scope (report) |
| The issue text says "5 good steps refill nothing", but the source and the code give 4 suppressed and the 5th paying | Record it in the report as an issue-text correction. The existing test 12 and new 20d pin the source behavior | In scope (report) |
| The seconds-based crossing cursor differs from OpenITG's note-row cursor under stops/warps | Already documented at `judgment_engine.cpp:383`. Unchanged | Out of scope (flag only) |
| OpenITG skips mine DP once `bFailed` (`ScoreKeeperMAX2.cpp:337`) | Cannot be reached: Fail-On ends the song, and Fail-Off never sets `failed` | Out of scope (flag only) |
| DrainType NoRecover / SuddenDeath mine values (`LifeMeterBar.cpp:130,144`) | Drain types are not in Blaze's mod scope (PRD: speed mods + scroll only) | Out of scope |

---

## Decisions

- **D1 — No value changes.** Every mine value already matches OpenITG's effective `[Preferences]` (AGENTS.md principle 2: OpenITG is the
  reference for life behavior). SM5 values are recorded for comparison only.
- **D2 — Type-checked and deduplicated mine consumption** (`note_type == Mine` plus `note_scored_`), mirroring `consume_hold_outcome`.
  Alternative considered: dedupe by index only, with no type check, which needs no test edits. Rejected because the keeper would then
  accept a mine event on a tap's index and mark that tap as scored, which would block its real row judgment.
- **D3 — Reuse `note_scored_`** instead of adding a `mine_scored_` vector. Mines never set it otherwise (`life_keeper.cpp:114-118`).
- **D4 — No new test target**; the suite stays at 51.

## Open Questions

1. **Add the same duplicate-mine guard to `ScoreKeeper` (DP)?** *Proposed default:* no, not in #118. The AC covers life only, the engine
   already guarantees one event per mine, and `ScoreKeeper` has no per-note guard for mines yet. If the owner wants it symmetric, it is a
   three-line change in `score_keeper.cpp:81-85` plus one `score_keeper_test` case. It can be folded in here or filed as a Low-priority
   follow-up.
2. **Offer a gentler-than-OpenITG mine option?** *Proposed default:* no. The values are OpenITG-exact, and AGENTS.md principle 2 ("Faithful,
   not novel") rules out tuning. Players who want different values can already edit `data/judgment_constants.json`
   (`life_deltas.hit_mine`, `hot_downgrade`, `regen_combo_after_miss`) without a rebuild. This is an owner decision. The plan does not pick it.

---

## Acceptance Criteria

- [ ] `life_deltas.hit_mine` (−0.05) and `hot_downgrade` (−0.10) checked against OpenITG with file:line references, posted on #118 (Task 6; AC 1)
- [ ] Regain debt (`regen_combo_after_miss` 5 / max 10; 4 suppressed, 5th pays) and DP penalty (`dp_weights.hit_mine` −6) compared with file:line references (AC 2), pinned by tests 20d/20e
- [ ] StepMania 5 defaults recorded for the same values (AC 3)
- [ ] A single mine takes life at most once, whether stepped on (20b) or held through (20c), and duplicates are ignored (20a) (AC 4)
- [ ] M1 fixed with unit tests in `tests/life_keeper_test.cpp`; M2 comment corrected; no follow-up bug needed for life (AC 5)
- [ ] Build passes with no new warnings in touched files; 51/51 tests pass sandboxed; headless smoke exits 0
- [ ] Issue state and project board left untouched
