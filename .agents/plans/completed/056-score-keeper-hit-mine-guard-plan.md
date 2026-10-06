# Plan: Guard ScoreKeeper Hit-Mine DP to Once per Mine (#119)

## Summary

#118 made `LifeKeeper` take life for a mine at most once. It did this with a guarded `consume_hit_mine`: bounds check, then `NoteType::Mine` check, then the per-note `note_scored_` guard (`src/gameplay/life_keeper.cpp:158-171`). `ScoreKeeper` still applies the hit-mine weight for **every** `HitMine` event (`src/gameplay/score_keeper.cpp:81-85`): −6 DP plus one `TapJudgment::HitMine` tally each time. A duplicate event therefore costs DP twice and counts the mine twice on the results screen, while life is taken only once.

This plan moves the `HitMine` case into one private helper, `ScoreKeeper::apply_hit_mine(const JudgmentEvent&)`, which uses the same three guards in the same order as `LifeKeeper::consume_hit_mine`. It reuses the existing `note_scored_` vector, which is safe because mines have `note_row_ == -1` and `apply_tap_like` never marks them. It also adds one new test block in `tests/score_keeper_test.cpp` covering duplicate, out-of-range, negative and foreign-index events. Combo stays untouched (`ScoreKeeperMAX2.cpp:333-341`), `AvoidedMine` stays stats-only, and no values change. Players are not affected today, because `JudgmentEngine` emits one `HitMine` per mine (`tests/judgment_engine_test.cpp:667-692`). This is a defensive fix that keeps the two keepers consistent with AGENTS.md "Event-sourced judgments" (each note's events take effect at most once).

## User Story

As a player (and as the maintainer of the event-sourced scoring path)
I want a hit mine to cost DP and add to the results tally at most once, matching how life already works
So that score, the results screen and the life bar always agree, even if the event log ever contains a duplicate

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX (defensive; not reachable by players today) |
| Complexity | LOW |
| Systems Affected | `gameplay/score_keeper` (hit-mine guard), `tests/score_keeper_test.cpp` |
| GitHub Issue | #119 (Guard ScoreKeeper hit-mine DP to once per mine (LifeKeeper parity)) |
| Related | #118 / plan `completed/055-mine-damage-openitg-parity-plan.md` (Open Question 1 → this issue), #67 (MercifulBeginner clamp) |
| Branch (suggested) | `feature/056-score-keeper-hit-mine-guard` |
| PRD Phase | N/A |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| Build | `build/` (CMake, Unix Makefiles, GCC, C++20 `-Wall -Wextra -Wpedantic`) | `cmake --build build` is clean on `main` @ `f87428f`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | `main` @ `f87428f`, sandboxed ctest, 2.9 s. Target after this plan: **51/51**. There are no new executables, only a new block inside `score_keeper_test` |
| Sandbox | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net` | Some tests touch real audio hardware, so **always** run them sandboxed, including any direct test-binary run |
| Test harness | plain `TEST_CHECK` + `std::abort` executable | `tests/score_keeper_test.cpp:20-26`, registered at `tests/CMakeLists.txt:125-133` |
| Forward refs to #119 | none in `src/`, `tests/`, `docs/`, `.agents/plans/` | The only predecessor note is plan 055, Open Question 1 ("three-line change in `score_keeper.cpp:81-85` plus one `score_keeper_test` case") |
| Existing `HitMine` events in `score_keeper_test` | lines ~349-356 (mine at idx 2), ~459-468 (mine at idx 1), ~734-740 (mine at idx 5) | **All three already target real `NoteType::Mine` notes**, so issue AC 4 needs no test edits. Re-confirm during Task 3 |
| Other `ScoreKeeper` consumers | `src/gameplay/gameplay_view.hpp:85-117` (`score_` member), `tests/judgment_animator_test.cpp` (comment only) | `GameplayView` feeds engine events, which already hold one event per mine, so its behavior is unchanged |

### Bug reproduction (BUG_FIX evidence)

You can see the problem by reading the code. No harness run is needed, because the engine never emits the duplicate, which is the very reason the fix is defensive.

| Input (chart: idx0 Mine, idx1 Tap; possible DP 5) | Current `ScoreKeeper` (`score_keeper.cpp:81-85`) | `LifeKeeper` after #118 | After this plan |
|---|---|---|---|
| `HitMine` idx 0, then a duplicate `HitMine` idx 0 | DP −12, HitMine tally 2 | life −0.05 once | DP −6, tally 1 |
| `HitMine` idx 1 (a tap's index) | DP −6, tally 1 | ignored | ignored |
| `HitMine` idx 99 / idx −1 | DP −6, tally 1 each (index never checked) | ignored | ignored |

The new test block (Task 2) fails against the current code on the duplicate, foreign and out-of-range assertions. That makes the block the red step. Task 1 turns it green.

---

## Pinned Semantics (the contract the tests pin)

1. A `HitMine` event changes `actual_dp` by `dp_weight(dp_weights.hit_mine)` (−6, or 0 under MercifulBeginner) and adds 1 to `tap_counts[HitMine]`. It does this **at most once per mine note index**.
2. A `HitMine` event is **ignored** (no DP, no tally, guard not touched) when `note_index < 0`, when `note_index >= notes.size()`, or when `chart_->notes[note_index].type != NoteType::Mine`.
3. A rejected `HitMine` on a tap's index does **not** mark that tap scored. The tap's real `Tap`/`Miss` event still resolves its row normally afterward.
4. A hit mine never changes `combo`, `max_combo` or `miss_combo` (`ScoreKeeperMAX2.cpp:333-341`).
5. `AvoidedMine` and `RollHit` stay no-ops. `is_complete()` is unaffected, because mines never join a row.
6. `recompute_derived()` still runs after every `consume` call, rejected events included. That is harmless, because it is a pure recompute.

Guard order matches `LifeKeeper::consume_hit_mine` exactly: bounds, then type, then the duplicate check, then mark, then apply.

---

## Patterns to Follow

### Per-note guarded helper (mirror exactly)

```cpp
// SOURCE: src/gameplay/life_keeper.cpp:158-171
void LifeKeeper::consume_hit_mine(const JudgmentEvent& event) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= note_scored_.size()) {
        return;
    }
    if (chart_->notes[static_cast<std::size_t>(index)].type != NoteType::Mine) {
        return; // only mine notes explode
    }
    if (note_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard: a mine takes life at most once (OpenITG grades it, Player.cpp:1096)
    }
    note_scored_[static_cast<std::size_t>(index)] = true;
    apply(delta_for(event));
}
```

### Local naming and type-checked guard in `ScoreKeeper`

`ScoreKeeper`'s private per-kind helpers are named `apply_*` (`apply_tap_like`, `apply_hold`: `src/gameplay/score_keeper.hpp:73-74`). `apply_hold` already does the bounds → type → duplicate sequence:

```cpp
// SOURCE: src/gameplay/score_keeper.cpp:161-172
void ScoreKeeper::apply_hold(const JudgmentEvent& event) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= hold_scored_.size()) {
        return;
    }
    if (!chart_->notes[static_cast<std::size_t>(index)].is_hold_or_roll()) {
        return; // only hold/roll heads produce hold outcomes
    }
    if (hold_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard
    }
    hold_scored_[static_cast<std::size_t>(index)] = true;
```

### Why reusing `note_scored_` is safe

```cpp
// SOURCE: src/gameplay/score_keeper.cpp:100-112
    if (note_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard: one event per note (idempotent per note)
    }
    const int row = note_row_[static_cast<std::size_t>(index)];
    if (row < 0) {
        return; // mine / unscored note
    }
    note_scored_[static_cast<std::size_t>(index)] = true;
```

`apply_tap_like` marks `note_scored_` only **after** the `row < 0` check, so it never marks a mine index. The only writer for a mine index will be `apply_hit_mine`. A stray `Tap` event on a mine index stays a no-op whether or not the mine was hit: it is rejected by the row check before the hit and by the guard after it, with the same outcome.

### Tests (numbered blocks, hand-computed arithmetic in comments, one `std::cout` line per block)

```cpp
// SOURCE: tests/life_keeper_test.cpp:652-683 (the #118 sibling block to mirror)
        // 20a. Duplicate / foreign HitMine events take life once. OpenITG grades a
        // hit mine (Player.cpp:1096), so it can never explode again.
        //   start 0.5; mine idx 0 -> -0.050 = 0.45; duplicate -> ignored = 0.45
        //   HitMine on a tap index / out of range -> ignored             = 0.45
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 0.0, NoteType::Mine));
            chart.notes.push_back(make_note(1, 1.0, NoteType::Tap));
            ...
            fresh.consume(make_mine(1, 1));
            TEST_CHECK(approx(fresh.life(), 0.5));
            fresh.consume(make_tap(1, 1, blaze4k::TapJudgment::Fantastic, 0.0));
            TEST_CHECK(approx(fresh.life(), 0.508));
            std::cout << "  - 20a. duplicate / foreign HitMine events take life once.\n";
        }
```

```cpp
// SOURCE: tests/life_keeper_test.cpp:76-84 (event helper to copy into score_keeper_test's anonymous namespace)
blaze4k::JudgmentEvent make_mine(int note_index, int column) {
    blaze4k::JudgmentEvent event;
    event.kind = blaze4k::JudgmentKind::HitMine;
    event.column = column;
    event.note_index = note_index;
    event.note_type = blaze4k::NoteType::Mine;
    event.window = blaze4k::TapJudgment::HitMine;
    return event;
}
```

`score_keeper_test.cpp` helpers live in the anonymous namespace at lines 28-92 (`make_tap`, `make_miss`, `make_hold_outcome`). Add `make_hit_mine` next to `make_miss`.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/gameplay/score_keeper.hpp` | UPDATE | Declare `void apply_hit_mine(const JudgmentEvent& event);` after `apply_hold` (line 74). Update the `note_scored_` comment (line 92): "tap-like events" → "tap-like events and hit mines" |
| `src/gameplay/score_keeper.cpp` | UPDATE | Replace the body of the `HitMine` case (lines 81-85) with `apply_hit_mine(event);`. Implement the guarded helper after `apply_hold` |
| `tests/score_keeper_test.cpp` | UPDATE | Add a `make_hit_mine` helper and a new block **9b** after block 9 (Mines, ~line 479) |

No other files. JSON constants, `LifeKeeper`, `JudgmentEngine`, results and HUD stay untouched.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Guarded `apply_hit_mine` in `ScoreKeeper`

- **File**: `src/gameplay/score_keeper.hpp`, `src/gameplay/score_keeper.cpp`
- **Action**: UPDATE
- **Implement**:
  - Header: add `void apply_hit_mine(const JudgmentEvent& event);` directly below `apply_hold` (line 74). Change the line 92 comment to `// per-note idempotence guard for tap-like events and hit mines`.
  - `.cpp`, `consume()` switch: replace the `HitMine` case body with
    ```cpp
    case JudgmentKind::HitMine:
        apply_hit_mine(event); // once per mine; never changes combo
        break;
    ```
  - `.cpp`, new helper placed after `apply_hold` (before `recompute_derived`):
    ```cpp
    void ScoreKeeper::apply_hit_mine(const JudgmentEvent& event) {
        const int index = event.note_index;
        if (index < 0 || static_cast<std::size_t>(index) >= note_scored_.size()) {
            return;
        }
        if (chart_->notes[static_cast<std::size_t>(index)].type != NoteType::Mine) {
            return; // only mine notes score a hit mine
        }
        if (note_scored_[static_cast<std::size_t>(index)]) {
            return; // duplicate guard: a mine scores at most once (OpenITG grades it, Player.cpp:1096)
        }
        note_scored_[static_cast<std::size_t>(index)] = true;
        // Hit mine scores but never changes combo (ScoreKeeperMAX2.cpp:333-341).
        state_.actual_dp += dp_weight(constants_->dp_weights.hit_mine);
        state_.tap_counts[static_cast<std::size_t>(TapJudgment::HitMine)]++;
    }
    ```
  - Keep the `dp_weight()` wrapper (MercifulBeginner clamp, #67). `chart_` and `constants_` are already non-null at this point because of the early return in `consume()` (`score_keeper.cpp:64-66`).
- **Mirror**: `src/gameplay/life_keeper.cpp:158-171` (guard order, comment wording) and `src/gameplay/score_keeper.cpp:161-172` (local `apply_*` style)
- **Validate**: `cmake --build build` (no new warnings), then sandboxed `ctest --test-dir build -R score_keeper_test --output-on-failure`. Existing blocks 6, 9 and 17 must stay green, because their mine events target real mine notes.

### Task 2: New test block 9b: duplicate / foreign `HitMine` events score once (#119)

- **File**: `tests/score_keeper_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - Add a helper `make_hit_mine(int note_index, int column)` in the anonymous namespace after `make_miss` (lines 56-65). Copy `tests/life_keeper_test.cpp:76-84`.
  - Add block **9b** directly after block 9's closing brace (~line 479), in the same style: a comment with hand-computed arithmetic and one `std::cout` line.
    ```text
    // 9b. Duplicate / foreign HitMine events score once (#119; LifeKeeper parity,
    //     OpenITG grades a hit mine once, Player.cpp:1096).
    //   chart: idx0 Mine @0.0, idx1 Tap @1.0 -> possible DP 5
    //   HitMine idx1 (tap) / idx99 / idx-1 -> ignored          DP  0, mine tally 0, combo 0
    //   Tap idx1 Fantastic (guard slot not consumed)           DP  5, Fantastic 1,  combo 1
    //   HitMine idx0                                           DP -1, mine tally 1, combo 1
    //   HitMine idx0 again (duplicate)                         DP -1, mine tally 1, combo 1
    ```
    Assertions, after each step:
    - After the three foreign/out-of-range events: `actual_dance_points() == 0`, `tap_counts[HitMine] == 0`, `combo == 0`, `!is_complete()`.
    - After `make_tap(1, 1, Fantastic, 0.0)`: `actual_dance_points() == 5`, `tap_counts[Fantastic] == 1`, `combo == 1`, `is_complete()`. This proves AC 2: the rejected mine event did not use up the tap's guard slot.
    - After `make_hit_mine(0, 0)`: `actual_dance_points() == -1`, `tap_counts[HitMine] == 1`, `combo == 1`, `max_combo == 1`.
    - After a second `make_hit_mine(0, 0)`: the **same** values (−1, 1, 1, 1), plus `approx(percent(), -0.2)`.
    - `std::cout << "  - 9b. Duplicate / foreign HitMine events score once (DP, tally, combo).\n";`
  - Optional, kept small: a second sub-case where the duplicate arrives on a Beginner chart, asserting the tally stays 1 while DP stays 0 (MercifulBeginner). Skip it if it adds noise. Block 17 already covers the clamp.
- **Mirror**: `tests/life_keeper_test.cpp:652-683` (block 20a), `tests/score_keeper_test.cpp:445-479` (block 9 layout)
- **Validate**: sandboxed `./build/tests/score_keeper_test`. Every line prints and it exits 0. (TDD check, optional: with Task 1 stashed, 9b aborts on the duplicate/foreign assertions.)

### Task 3: Confirm existing `HitMine` test cases target real mines (issue AC 4)

- **File**: `tests/score_keeper_test.cpp`
- **Action**: VERIFY (edit only if a mismatch is found)
- **Implement**: for each `JudgmentKind::HitMine` construction (`grep -n "JudgmentKind::HitMine" tests/score_keeper_test.cpp`), confirm that `note_index` points at a `NoteType::Mine` in that block's chart. Expected (checked while writing this plan): block 6 → idx 2 = Mine, block 9 → idx 1 = Mine, block 17 → idx 5 = Mine. If any points at a tap, retarget it to a real mine note and adjust the expected values. Optionally switch the three inline constructions to `make_hit_mine` for consistency. This is a pure refactor with identical fields, so do it only if it keeps the diff small.
- **Validate**: sandboxed `ctest --test-dir build -R score_keeper_test --output-on-failure`

### Task 4: Full suite and smoke

- **Validate**: the full Validation block below. Expect 51/51, no new warnings, and a headless smoke exit 0.

---

## Validation

```bash
# Build (warnings in touched files = lint gate; no linter configured)
cmake --build build 2>&1 | grep -E "warning|error" ; cmake --build build

# Tests: expect 51/51. Some tests touch real audio hardware, so ALWAYS run sandboxed.
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Targeted (same sandbox prefix)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net \
  ctest --test-dir build -R "score_keeper_test|life_keeper_test|judgment_engine_test|results_test|results_screen_test|gameplay_screen_test" --output-on-failure

# Direct binary run (same sandbox prefix)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/tests/score_keeper_test

# Values untouched (expect no diff)
git diff --exit-code assets/data/judgment_constants.json src/timing/judgment_constants.cpp src/gameplay/life_keeper.cpp

# Scope guard
git diff --stat   # only src/gameplay/score_keeper.{hpp,cpp} and tests/score_keeper_test.cpp
```

## End-to-End Verification

1. **Automated (headless, required):** block 9b drives `ScoreKeeper::consume` with duplicate, out-of-range, negative and foreign-index `HitMine` events and pins DP, tally and combo after each step. Blocks 6, 9 and 17 keep proving that one real mine hit scores −6 (0 on Beginner) without touching combo. `life_keeper_test` 20a–20c and `judgment_engine_test` 16.x still prove the engine → keeper chain emits and applies one event per mine, so score and life now agree by construction.
2. **App smoke (headless, required):**
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   exits 0.
3. **Optional visual check:** `/verify` can play a chart with mines and screenshot the results screen's mine count. Not required: for real play the event stream is unchanged, so the screen output is identical before and after.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Reusing `note_scored_` could interact with tap-row aggregation | Mines have `note_row_ == -1` (`score_keeper.cpp:36-40`), and `apply_tap_like` marks only after the `row < 0` check (`:108-112`), so mine slots are only ever written by `apply_hit_mine`. Same reasoning as plan 055, Decision D3 | In scope (verified by 9b's tap-after-foreign-mine step) |
| The new type check would silently ignore an existing test that sends `HitMine` on a tap index | All three existing cases target real mines (Task 3 re-checks them). Any mismatch shows up as a failed DP/tally assertion, not a silent pass | In scope |
| The MercifulBeginner clamp is dropped while moving the code | The helper keeps `dp_weight(...)`. Block 17 asserts Beginner DP 10 and Hard −14, both including one mine | In scope |
| `ScoreKeeper` and `LifeKeeper` duplicate the guard logic (no shared helper) | Already accepted duplication (`life_keeper.hpp:36` notes the row table duplicates `ScoreKeeper::reset`). Extracting a shared helper is out of scope for a Small bug | Out of scope (flag only) |
| OpenITG skips mine DP once `bFailed` (`ScoreKeeperMAX2.cpp:337`) | Not reachable: Fail-On ends the song, and Fail-Off never sets `failed`. Unchanged from #118 | Out of scope (flag only) |

---

## Decisions

- **D1 — Reuse `note_scored_`** instead of adding a `mine_scored_` vector. This mirrors `LifeKeeper` (#118 D3) and follows the issue's technical note.
- **D2 — Bounds → type → duplicate guard order**, identical to `LifeKeeper::consume_hit_mine`. A type-less index guard was rejected: it would let a mine event on a tap's index mark the tap scored and swallow its real row judgment (issue AC 2).
- **D3 — Helper name `apply_hit_mine`** (local `apply_*` convention) instead of the issue's suggested `consume_hit_mine`. See Open Question 1.
- **D4 — No new test target**, so the suite stays at 51.

## Open Questions

1. **Helper name: `apply_hit_mine` or `consume_hit_mine`?** The issue's technical note says `consume_hit_mine` (the `LifeKeeper` name). `ScoreKeeper`'s own private helpers are `apply_tap_like` / `apply_hold`, while its public entry point is `consume`. *Proposed default:* `apply_hit_mine`, to match the file it lives in. Behavior is identical either way, so the implementer may switch to `consume_hit_mine` if the owner prefers the cross-keeper name. Not blocking.

---

## Acceptance Criteria

- [ ] `ScoreKeeper`'s `HitMine` path applies −6 DP and the `HitMine` tally at most once per mine. Duplicates have no effect (issue AC 1; 9b)
- [ ] Out-of-range, negative and non-mine-index `HitMine` events are ignored and do not use up the note's guard. The real tap still scores afterward (issue AC 2; 9b)
- [ ] `tests/score_keeper_test.cpp` covers duplicate, out-of-range and foreign-index events, with DP, tally and combo unchanged after the first hit (issue AC 3)
- [ ] The existing `HitMine` cases still target real mine notes (issue AC 4; Task 3)
- [ ] Hit mines still never change combo, `AvoidedMine` stays stats-only, and the MercifulBeginner clamp is preserved
- [ ] Build passes with no new warnings in touched files, 51/51 tests pass sandboxed, and the headless smoke exits 0 (issue AC 5)
- [ ] Diff limited to `src/gameplay/score_keeper.{hpp,cpp}` and `tests/score_keeper_test.cpp`
