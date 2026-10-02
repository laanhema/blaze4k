# Plan: Verify Judgment Timing Windows Against OpenITG (#57)

## Summary

Issue #57 is a spike: do Blaze 4k's Fantastic/Excellent/Great/Decent/Way Off windows, and its
hold/roll/mine windows, match In The Groove / OpenITG, both as configured values and as applied in the
judgment path? **The OpenITG lookup was done while writing this plan** (commit
`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`, the same commit already pinned in
`src/timing/judgment_constants.hpp:10-14`). Every value and reference is in the **Verification Report**
below. Short version:

- **All 8 base windows match** OpenITG's `[Preferences]` metrics (`metrics.ini:90-103`).
- **Edge inclusivity matches**: `<=` on every tap/mine edge, and a note expires only once it is strictly
  older than the Way Off window.
- **The global offset is applied exactly once**, with the same sign convention as OpenITG.

Three mismatches were found:

| # | Mismatch | Class |
|---|----------|-------|
| M1 | `judge_window_scale` / `judge_window_add` are loaded and validated but **never applied**. OpenITG applies `base * Scale + Add` to every tap, mine, hold and roll window (`Player.cpp:34-74`), including the Boo-based miss expiry (`:1710-1713`) | **Trivial: fix in this issue** with unit tests |
| M2 | The `judge_window_add` default is `0.0`. Real ITG dedicated cabinets launch with `--type=Preferences-cabinet` (`assets/arcade-patch/start-3.sh:17,38`), which sets `JudgeWindowAdd=0.0015` (`metrics.ini:262`, also on the original ITG2 drive data `assets/d4/Themes/default/metrics.ini:241`). The effective arcade windows are therefore 1.5 ms wider (Fantastic ±23.0 ms, not ±21.5 ms) | **Trivial data change, fixed in this issue** (Decision D1). It needs owner sign-off, see Open Question 1 |
| M3 | OpenITG's `MercifulBeginner=1` (`metrics.ini:157`) adds 0.5 s to the Boo window on Beginner charts (`Player.cpp:55-56`), suppresses early Boos (`:1089-1093`) and clamps negative DP/grade weights to 0 (`ScoreKeeperMAX2.cpp`). Blaze has none of this | **Not trivial: file a follow-up issue** (needs the chart difficulty in the engine, the scorer and the life keeper) |

Implementation: add `JudgmentConstants::effective_windows()`, which returns `base * scale + add` for the eight judge
windows (`pad_stick` is never adjusted), and route every window read in the judgment path through it.
Then flip the default `judge_window_add` to `0.0015`, update the tests, post the report on #57, and file the
MercifulBeginner follow-up.

## User Story

As a competitive ITG player
I want Blaze 4k's judgment windows to be the ones a real In The Groove cabinet uses, applied exactly as OpenITG applies them
So that my Fantastics, grades and scores mean the same thing they do on real hardware

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX (spike with a trivial fix) |
| Complexity | LOW |
| Systems Affected | `timing/judgment_constants` (`effective_windows`, validation, default `judge_window_add`), `gameplay/judgment_engine` (window reads), `assets/data/judgment_constants.json`, tests (`judgment_constants_test`, `judgment_engine_test`, `music_clock_test`) |
| GitHub Issue | #57 (related: #56 mine window, already merged) |
| Branch | `feature/031-verify-judgment-timing-windows` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 | `build/` already configured (Release, host-native) |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic` |
| Baseline tests | **38/38 pass** | `cmake --build build -j && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build` gives "100% tests passed out of 38" (0.44 s) on `main` @ `eff3761` |
| Upstream source | `<scratchpad>/oitg/` | Blobless clone, see "Obtaining OpenITG source" below. `git rev-parse HEAD` = `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (2020-12-17, "ok it's really over now…") |
| Test harness | plain `TEST_CHECK` + `std::abort` executables, one per area | `tests/judgment_engine_test.cpp:13-19`, registered in `tests/CMakeLists.txt` and linked to `blaze4k_core` |
| App smoke | `./build/blaze-4k --headless --smoke-test 10` | `src/main.cpp:37-38,81` |
| Off-limits | `.agents/stories/todo-stories.md` (untracked) | Do not modify, stage or revert it. Do **not** use `/create-story` for the follow-up, because it edits the story files. Use `gh issue create` |

**Start green, stay green:** 38 tests pass now. This plan adds **no new test target**, only cases in existing
ones, so **38/38** are expected afterwards.

### Obtaining OpenITG source (for re-verification during implementation)

The scratchpad is session-specific, so re-clone if `<scratchpad>/oitg` is gone. Clone it **outside the repo**:

```bash
S=<scratchpad>   # or any temp dir outside /home/lauri/github/blaze4k
git clone -q --filter=blob:none --no-checkout https://github.com/openitg/openitg.git "$S/oitg"
git -C "$S/oitg" checkout -q f2c129fe65c65e4a9b3a691ff35e7717b4e8de51
git -C "$S/oitg" rev-parse HEAD   # must print f2c129fe65c65e4a9b3a691ff35e7717b4e8de51
```

All `file:line` references below are relative to that checkout's root.

---

## Verification Report (to be posted on #57; AC 1–3)

OpenITG commit: `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (github.com/openitg/openitg).
OpenITG name → Blaze name: Marvelous→Fantastic, Perfect→Excellent, Great→Great, Good→Decent, Boo→Way Off, OK→hold_ok, Roll→hold_roll, Mine→hit_mine.

### How OpenITG resolves a preference at runtime

1. Compiled default in `src/PrefsManager.cpp:86-96` (StepMania 4 values).
2. `StepMania.cpp:1125-1128` loads the theme section `Preferences`, or the one named by `--type=<section>`, through
   `ThemeManager::LoadPreferencesFromSection` (`ThemeManager.cpp:897-930`). Lookups walk the `Fallback=` chain
   (`ThemeManager.cpp:863-890`), so the most specific section wins.
3. The arcade launcher picks the section per machine: `--type=Preferences-cabinet` on dedicated cabinets (the default
   branch), `Preferences-kit` / `Preferences-kit-vga` on kits, `Preferences-school` on school builds
   (`assets/arcade-patch/start-3.sh:8-18,38`). Home builds use `[Preferences]`.
4. Chains (`assets/patch-data/Themes/default/metrics.ini`): `[Preferences-cabinet]` (`:239`, `Fallback=Preferences-arcade` `:240`)
   → `[Preferences-arcade]` (`:165`, `Fallback=Preferences` `:166`) → `[Preferences]` (`:22`). The arcade section overrides
   no timing or scoring key. The cabinet section overrides only `GlobalOffsetSeconds=-0.012` (`:248`) and
   `JudgeWindowAdd=0.0015` (`:262`).

### AC 1 — `windows_seconds` values

| Blaze key (`assets/data/judgment_constants.json:10-20`, `src/timing/judgment_constants.cpp:19-29`) | Blaze | OpenITG effective base | OpenITG source | Compiled default (overridden) | Match |
|---|---|---|---|---|---|
| `fantastic` | 0.0215 | 0.021500 | `metrics.ini:98` `JudgeWindowSecondsMarvelous` | 0.0225 `PrefsManager.cpp:88` | ✅ |
| `excellent` | 0.043 | 0.043000 | `metrics.ini:97` `JudgeWindowSecondsPerfect` | 0.045 `:89` | ✅ |
| `great` | 0.102 | 0.102000 | `metrics.ini:96` `JudgeWindowSecondsGreat` | 0.090 `:90` | ✅ |
| `decent` | 0.135 | 0.135000 | `metrics.ini:95` `JudgeWindowSecondsGood` | 0.135 `:91` | ✅ |
| `way_off` | 0.18 | 0.180000 | `metrics.ini:94` `JudgeWindowSecondsBoo` | 0.180 `:92` | ✅ |
| `hit_mine` | 0.07 | 0.070000 | `metrics.ini:93` `JudgeWindowSecondsMine` | 0.090 `:95` | ✅ |
| `hold_ok` | 0.32 | 0.320000 | `metrics.ini:99` `JudgeWindowSecondsOK` | 0.250 `:93` | ✅ |
| `hold_roll` | 0.35 | 0.350 | no metrics override in any section, so compiled `PrefsManager.cpp:94` `JudgeWindowSecondsRoll` applies | — | ✅ |
| `pad_stick` (not a judge window) | 0.05 | 0.05 | `metrics.ini:103` `PadStickSeconds` | 0 `:250` | ✅ (never scaled, which is correct: it is not passed through `AdjustedWindow*`) |

The original ITG2 drive data (`assets/d4/Themes/default/metrics.ini:89-96`) has the same values.

### AC 2 — `judge_window_scale` / `judge_window_add`

| Key | Blaze default | OpenITG | Source | Verdict |
|---|---|---|---|---|
| `judge_window_scale` | 1.0 | 1.000000 in all sections | `metrics.ini:90` ("don't allow overriding of timing"); compiled 1.0 `PrefsManager.cpp:86` | ✅ value |
| `judge_window_add` | 0.0 | 0.000000 in `[Preferences]` (home), **0.0015 on dedicated cabinets**, 0 on kits (the `-0.001666` line is commented out), +0.002 on PS2 | `metrics.ini:91`, **`:262`** (comment `:250-261`: "this is RoXoR's value for all dedicated cabinets… we'll enforce RoXoR's standard"), `:278`, `:362`; `assets/d4/…/metrics.ini:241`; compiled 0 `PrefsManager.cpp:87` | ❌ **M2**: arcade-cabinet value is 0.0015 |
| Application | **never applied** (only loaded and validated: `judgment_constants_loader.cpp:98-99`, `judgment_constants.cpp:85-90`) | `fSecs = base * Scale + Add` for every tap/mine window (`Player.cpp:34-58`, scale `:49`, add `:50`) and hold/roll window (`Player.cpp:60-74`, `:70-71`). The miss expiry uses the adjusted Boo window (`GetMaxStepDistanceSeconds`, `Player.cpp:1710-1713` → `UpdateTapNotesMissedOlderThan` `:440,1368-1397`) | ❌ **M1** |

Not applicable to Blaze: the player-option `m_fTimingScale` multiplier (`Player.cpp:53`, default 1.0 `PlayerOptions.cpp:33`,
an out-of-scope mod), the music rate (`Player.cpp:929,1712`, no rate mods in v1), and the Beginner +0.5 s Boo (`Player.cpp:55-56`, see M3).

Effective windows after the fix with the adopted cabinet add (0.0015): Fantastic **23.0 ms**, Excellent **44.5**, Great **103.5**,
Decent **136.5**, Way Off **181.5**, Mine **71.5**, Hold OK **321.5**, Roll **351.5**; pad_stick stays 50.
(Non-authoritative corroboration: the community-standard ITG mode in ITGmania/Simply Love uses the same 21.5 ms + 1.5 ms add.)

### AC 3 — edge inclusivity and global offset

| Behavior | OpenITG | Blaze | Match |
|---|---|---|---|
| Tap edges | `fSecondsFromPerfect <= window` on each tier (`Player.cpp:957-961`), on `fabsf(fNoteOffset)` (`:935`) | `delta <= window` (`judgment_constants.cpp:131-136`), on `std::fabs` | ✅ inclusive |
| Mine edge | `<= ADJUSTED_WINDOW_TAP(TW_Mine)` (`Player.cpp:948`) | `if (delta > hit_mine) return;`, i.e. hit when `<=` (`judgment_engine.cpp:141`) | ✅ inclusive |
| Beyond Way Off | `TNS_NONE`, no judgment (`Player.cpp:962`) | `Miss` from `classify_tap` → no event (`judgment_engine.cpp:111-113`) | ✅ |
| Miss expiry | rows strictly before `row(now − Boo)` are missed (`Player.cpp:1373,1397`, range ends at `idx-1`) | `note.time >= now − way_off` → not yet (`judgment_engine.cpp:210,218`), so a note exactly at the edge is still steppable | ✅ (OpenITG quantizes to rows; equivalent within 1/48 beat) |
| Hold/roll life | linear decay `fDeltaTime / window` (`Player.cpp:563,574`) | `1 − elapsed / window` (`judgment_engine.cpp:290-298`) | ✅ (window value gets M1) |
| Global offset sign and count | Applied once, symmetrically in time↔beat conversion: `+GlobalOffsetSeconds` in `GetBeatAndBPSFromElapsedTime` (`TimingData.cpp:192`), `−` in `GetElapsedTimeFromBeat` (`:255`). `m_fMusicSeconds` is the raw stream position (`GameState.cpp:795,804`), so `fNoteOffset = (chartTime − offset) − music` (`Player.cpp:916,926,929`) | Applied once in `MusicClock::time_seconds()` = `frames/rate + offset` (`music_clock.cpp:49-60`), set once per song (`gameplay_view.cpp:70`). Input times derive from that same clock sample without re-adding it (`gameplay_view.cpp:135,153-154`; `judgment_input.hpp:10-18`). Chart times use only the song `#OFFSET` (`timing_data.cpp:149-197`). The update path uses the same clock (`gameplay_view.cpp:187`). So `delta = (music + offset) − chartTime = −fNoteOffset` | ✅ once, same sign (Blaze `delta_ms` = OpenITG `fTapNoteOffset`, `Player.cpp:1099`) |
| Cabinet `GlobalOffsetSeconds=-0.012` (`metrics.ini:248`) | hardware/monitor calibration for ITG cabinets | Blaze has its own calibration wizard (#20); not adopted | n/a (intentional) |

Float vs double: OpenITG compares in `float` and Blaze in `double`. The edge difference is below 1e-8 s, far below
input timestamp resolution, so it is accepted.

### Follow-up (M3): MercifulBeginner

`MercifulBeginner=1` (`metrics.ini:157`; compiled false `PrefsManager.cpp:128`). On `DIFFICULTY_BEGINNER` (`Player.cpp:1724-1737`) it:
(a) widens the Boo window by 0.5 s (`Player.cpp:55-56`), which also delays misses (`:1712`); (b) shows but does **not** record an early Boo
(`Player.cpp:1089-1093`); (c) clamps negative DP/grade weights to 0 (`ScoreKeeperMAX2.cpp:529,544,570,585`; not in courses, `:187`).
Blaze implements none of this (no `merciful`/`beginner` logic in `src/gameplay`). It touches the engine, the scorer and chart-difficulty plumbing, so
it is **not trivial**. File it as a follow-up.

---

## Patterns to Follow

### Pure constants module (no deps beyond `<array>`/`<string>`)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:96-102
    static const JudgmentConstants& compiled_defaults();

    [[nodiscard]] bool validate(std::string* error = nullptr) const;

    // Pure lookups (units: seconds, percent as fraction 0.0-1.0)
    [[nodiscard]] TapJudgment classify_tap(double delta_seconds) const;
```

### Validation error style
```cpp
// SOURCE: src/timing/judgment_constants.cpp:64-122
    auto fail = [error](const std::string& reason) { if (error != nullptr) { *error = reason; } return false; };
    ...
    if (!std::isfinite(windows.judge_window_scale)) {
        return fail("judge_window_scale must be finite");
    }
```

### OpenITG provenance comments
```cpp
// SOURCE: src/timing/judgment_constants.hpp:28-34
    // OpenITG `PadStickSeconds` (arcade metrics.ini:103; compiled default 0 at
    // src/PrefsManager.cpp:250). ... Not a judge window:
    // `judge_window_scale/add` must never apply to it.
```

### Tests
```cpp
// SOURCE: tests/judgment_engine_test.cpp:58-97 (fresh chart+engine per case, exact event fields)
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, note_time + c.delta);
            TEST_CHECK(engine.events().size() == 1);
```
```cpp
// SOURCE: tests/judgment_constants_test.cpp:107-121 (parity block) and :266-275 (validate() reason check)
        TEST_CHECK(!negative.validate(&reason));
        TEST_CHECK(reason.find("pad_stick") != std::string::npos);
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/judgment_constants.hpp` | UPDATE | Declare `effective_windows()`; default `judge_window_add = 0.0015`; correct the layering comment (`[Preferences]` is the base/home layer; cabinets add `[Preferences-cabinet]`) |
| `src/timing/judgment_constants.cpp` | UPDATE | Implement `effective_windows()`; `classify_tap` uses it; validation of scale and effective windows; compiled default add 0.0015 with provenance comment |
| `src/gameplay/judgment_engine.cpp` | UPDATE | Mine window, miss expiry (threshold and synthetic `hit_time`/`delta_ms`) and hold/roll decay read effective windows; `pad_stick` stays raw |
| `assets/data/judgment_constants.json` | UPDATE | `"judge_window_add": 0.0015`; extend the `source` string to cite `[Preferences-cabinet]` `JudgeWindowAdd` (`metrics.ini:262`) and `start-3.sh:17` |
| `tests/judgment_constants_test.cpp` | UPDATE | Parity (add 0.0015), effective values, exact-edge inclusivity, scale/add arithmetic, validation of bad scale/add, pad_stick unadjusted |
| `tests/judgment_engine_test.cpp` | UPDATE | Use `w = k.effective_windows()` instead of `k.windows.*`; add cases proving scale/add reach the engine (tap, mine, expiry, hold) |
| `tests/music_clock_test.cpp` | UPDATE | Composition test: offset applied once through `MusicClock` → `music_time_for_event` → `JudgmentEngine` |
| `tests/gameplay_screen_test.cpp` | UPDATE (comment only, optional) | `:49` comment says "way_off (0.18 s)"; make it "effective way_off (0.1815 s)" |

No changes to `src/data/judgment_constants_loader.*` (keys are already parsed), screens, render or input.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: `effective_windows()` and validation

- **File**: `src/timing/judgment_constants.hpp`, `src/timing/judgment_constants.cpp`
- **Action**: UPDATE
- **Implement**:
  - Declare `[[nodiscard]] TimingWindows effective_windows() const;`. Doc comment: OpenITG `AdjustedWindowTap/Hold`
    (`Player.cpp:34-74`) compute `base * JudgeWindowScale + JudgeWindowAdd` for fantastic..way_off, hit_mine, hold_ok and hold_roll.
    `pad_stick` is copied unchanged. The returned struct has `judge_window_scale = 1.0` and `judge_window_add = 0.0`, so it is
    never adjusted twice.
  - Implement it as `w.x = windows.x * windows.judge_window_scale + windows.judge_window_add` for the 8 judge windows. The
    order is the same as OpenITG (scale first, then add).
  - `classify_tap`: `const TimingWindows w = effective_windows();`, then use `w.*` in the existing `<=` chain. Keep the NaN → Miss guard.
  - `validate`: after the existing finite checks, require `judge_window_scale > 0` ("judge_window_scale must be > 0"), and
    require every effective judge window to be finite and `> 0` ("effective timing window (base * scale + add) must be > 0").
    Base monotonicity plus a positive scale and a common add keep the effective windows monotonic, so no second monotonic check
    is needed.
  - Update the header comment block (`judgment_constants.hpp:8-14`, `.cpp:9-14`). Base windows come from `[Preferences]`
    (`metrics.ini:90-103`). Dedicated cabinets run `--type=Preferences-cabinet` (`start-3.sh:17`), which only adds
    `JudgeWindowAdd=0.0015` (`metrics.ini:262`) and a hardware `GlobalOffsetSeconds` that Blaze does not adopt.
- **Mirror**: `judgment_constants.cpp:64-122` (validation), `.hpp:28-34` (provenance comment style)
- **Validate**: `cmake --build build -j`

### Task 2: Route engine window reads through `effective_windows()`

- **File**: `src/gameplay/judgment_engine.cpp`
- **Action**: UPDATE
- **Implement**: in each function that reads a judge window, take a local `const TimingWindows w = constants_->effective_windows();`
  (cheap: 11 doubles, no allocation) and use it:
  - `handle_step_mine` (`:141`): `w.hit_mine`
  - `expire_notes` (`:210,229-230,248-249`): `w.way_off` for the threshold and for the synthetic `hit_time_seconds`/`delta_ms`
    (mirrors `GetMaxStepDistanceSeconds`, `Player.cpp:1710-1713`)
  - `update_holds` (`:293,297`): `w.hold_ok`, `w.hold_roll` (`AdjustedWindowHold`, `Player.cpp:60-74`)
  - `cross_mines` (`:357`): **keep** `constants_->windows.pad_stick` (raw, not a judge window)
  - `handle_step_tap` already goes through `classify_tap` (Task 1).
  Add a short comment at each site citing the OpenITG `ADJUSTED_WINDOW_*` call. Then
  `grep -n "windows\.\(fantastic\|excellent\|great\|decent\|way_off\|hit_mine\|hold_ok\|hold_roll\)" src/` must return only
  `judgment_constants.*` and `judgment_constants_loader.cpp`.
- **Mirror**: existing OpenITG-citing comments in `judgment_engine.cpp:11,97,112,142`
- **Validate**: `cmake --build build -j && SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`.
  All 38 should still pass, because scale/add are still identity at this point.

### Task 3: Adopt the arcade-cabinet `judge_window_add = 0.0015` (Decision D1)

- **File**: `src/timing/judgment_constants.hpp` (`TimingWindows::judge_window_add` initializer), `src/timing/judgment_constants.cpp` (`compiled_defaults`, `:29`), `assets/data/judgment_constants.json` (`:20`, plus the `source` string at `:3`)
- **Action**: UPDATE
- **Implement**: set it to `0.0015` in all three places. Comment: `RoXoR/OpenITG dedicated-cabinet JudgeWindowAdd (metrics.ini:262, [Preferences-cabinet]; selected by assets/arcade-patch/start-3.sh:17)`.
  Append to the JSON `source` string: `; judge_window_add is [Preferences-cabinet] JudgeWindowAdd (metrics.ini:262), the section dedicated ITG cabinets launch with (assets/arcade-patch/start-3.sh:17)`.
  Keep `judge_window_scale = 1.0`.
- **Validate**: `cmake --build build -j`. Some tests are now **expected** to fail (`judgment_constants_test` parity `:121`, and
  `judgment_engine_test` edge cases that use `k.windows.way_off + 1e-3`, because 1 ms is less than the new 1.5 ms add). Tasks 4–5 fix them.

### Task 4: Constants tests

- **File**: `tests/judgment_constants_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - Parity block (`:121`): `nearly(defaults.windows.judge_window_add, 0.0015)`. Add a comment citing `metrics.ini:262`.
  - New section "effective windows" (OpenITG `Player.cpp:34-74`), with `const auto w = defaults.effective_windows();`. Use a 1e-12 tolerance:
    fantastic 0.0230, excellent 0.0445, great 0.1035, decent 0.1365, way_off 0.1815, hit_mine 0.0715, hold_ok 0.3215,
    hold_roll 0.3515; `w.pad_stick == 0.05` (unadjusted); `w.judge_window_scale == 1.0`, `w.judge_window_add == 0.0`.
  - Arithmetic: copy the defaults, set scale 2.0 and add 0.01, and check `effective_windows().fantastic == 0.0215*2+0.01` and
    `.hold_roll == 0.35*2+0.01`, with `pad_stick` still 0.05 (scale is applied before add, as in `Player.cpp:49-50`).
  - Edge inclusivity on `classify_tap`, for each tier boundary `e` in `w` (fantastic..way_off): `classify_tap(e)` returns that
    tier, `classify_tap(-e)` returns the same tier (symmetric), and `classify_tap(std::nextafter(e, 1.0))` returns the next tier
    (Miss after way_off). Use `#include <cmath>`, which is already included.
  - Validation: scale `0.0` is rejected, and the reason contains `judge_window_scale`. An add of `-0.03`, which makes the fantastic
    window negative, is rejected, and the reason contains `effective`. Both also fall back through the loader when written to JSON
    (`{"windows_seconds": {"judge_window_add": -0.03}}`), following the `:244-251` pattern.
  - JSON override: `{"windows_seconds": {"judge_window_add": 0.0}}` loads with 0.0, which gives the home `[Preferences]` timing,
    and `effective_windows().fantastic == 0.0215`.
  - The seed-file check (`find_seed_file`, `:88-100`, compared with the defaults) must stay green with the JSON change from Task 3.
- **Mirror**: `tests/judgment_constants_test.cpp:107-121, 205-216, 244-275`
- **Validate**: `ctest --test-dir build -R judgment_constants_test --output-on-failure`

### Task 5: Engine tests

- **File**: `tests/judgment_engine_test.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `main`, add `const blaze4k::TimingWindows w = k.effective_windows();` next to `k` (`:56`). Mechanically replace every
    `k.windows.{fantastic,excellent,great,decent,way_off,hit_mine,hold_ok,hold_roll}` with `w.*` (23 sites; list them with
    `grep -n "k.windows" tests/judgment_engine_test.cpp`). Leave `k.windows.pad_stick` sites as they are.
  - New section "judge_window_scale/add reach the engine" (OpenITG `Player.cpp:34-74`). Use a local
    `JudgmentConstants wide = compiled_defaults(); wide.windows.judge_window_add = 0.02;` with
    `const auto ww = wide.effective_windows();`:
    1. Tap at `+ (k.windows.way_off + 0.01)` (beyond the base, inside the widened window) → one `Tap` event, `window == WayOff`.
       With `k` the same step yields no event (already covered at `:96`).
    2. Mine stepped at `+ (k.windows.hit_mine + 0.01)` → `HitMine` with `wide`, and no event with an add-0 copy.
    3. Expiry: an `update` at `note + k.windows.way_off + 0.005` → no Miss with `wide`; an `update` at `note + ww.way_off + 1e-6` →
       Miss, with `delta_ms ≈ ww.way_off*1000`.
    4. Hold: the head is hit, then the column is released. An `update` at `head + k.windows.hold_ok + 0.005` is still not NG with `wide`.
       Mirror the existing hold-NG test at `:209`.
  - Inclusive edges in the engine (not exact, because of double rounding): a step at `note + w.way_off - 1e-6` gives a `WayOff`
    event; an `update` at `note + w.way_off - 1e-6` gives no Miss yet.
- **Mirror**: `tests/judgment_engine_test.cpp:58-97, 125-141, 150-180, 200-215`
- **Validate**: `ctest --test-dir build -R judgment_engine_test --output-on-failure`

### Task 6: Offset-applied-once composition test (AC 3)

- **File**: `tests/music_clock_test.cpp`
- **Action**: UPDATE
- **Implement**: a new case that chains the real components the way `GameplayView` does (`gameplay_view.cpp:70,135,153-154,187`):
  `MusicClock clock([]{ return SamplePosition{frames, 48000}; }); clock.set_global_offset_seconds(0.050);`. Pick raw sample time
  `2.0 − 0.050` (`frames = (2.0-0.05)*48000 = 93600`, exactly representable), so `clock.time_seconds()` is 2.0. Use a chart with one
  tap at 2.0. Call `engine.handle_step(0, music_time_for_event(ts, ts, clock.time_seconds()))` → one `Tap` event with `|delta_ms| < 1e-6`
  and `window == Fantastic`. Check the sign too: with offset `+0.010` and the same frames, `delta_ms ≈ −40` (hit early → negative,
  matching OpenITG `fTapNoteOffset = −fNoteOffset`, `Player.cpp:1099`). Also check that an event aged 5 ms (`ts = ref − 5'000'000`)
  gives a music time of exactly `clock.time_seconds() − 0.005`, which shows no second offset is added on the input path. Add the
  includes `gameplay/judgment_engine.hpp`, `gameplay/judgment_input.hpp` and `chart/chart.hpp`. The target already links `blaze4k_core`.
- **Mirror**: existing `music_clock_test.cpp` cases plus the `make_note` helper pattern from `judgment_engine_test.cpp:27-36`
- **Validate**: `ctest --test-dir build -R music_clock_test --output-on-failure`

### Task 7: Optional comment touch-up

- **File**: `tests/gameplay_screen_test.cpp:49`
- **Action**: UPDATE (comment only)
- **Implement**: "way_off (0.18 s)" → "effective way_off (0.1815 s)". The 0.25 s step logic is unaffected.
- **Validate**: build

### Task 8: Full suite and smoke

- **Validate**: see Validation. Expect **38/38**, no new warnings in the touched files, and a headless smoke run that exits 0.

### Task 9: Post the Verification Report on #57 (AC 1–3)

- **Action**: write the **Verification Report** section above, plus a "Resolution" paragraph, to a scratch file and post it:
  `gh issue comment 57 --body-file <scratchpad>/issue57-report.md`. The Resolution paragraph says M1 and M2 are fixed in this
  branch (name the tests) and gives the M3 follow-up issue number from Task 10. Post it after Task 10 so the number exists.
  Include the clone commands so the report can be reproduced.
- **Validate**: `gh issue view 57 --json comments --jq '.comments[-1].body' | head -5`

### Task 10: File the M3 follow-up issue (AC 4)

- **Action**: `gh issue list --state all --search "MercifulBeginner OR merciful beginner"` must return nothing first. Then
  `gh issue create --title "Implement OpenITG MercifulBeginner (Beginner: +0.5 s Way Off window, no early Way Off, no negative DP)" --label timing --label gameplay --label scoring --label bug --body-file <scratch>`.
  Use the repo's issue body style (`**Type**: Bug · **Priority**: Low · **Complexity**: Medium`, Description, Acceptance
  Criteria checkboxes, Technical Notes) with the M3 references above, "Related: #57". Do **not** use `/create-story`
  (it edits `.agents/stories/*`, and `todo-stories.md` is off-limits). Do not add it to the project board unless the owner asks.
- **Validate**: `gh issue view <new#> --json title,labels`

---

## Validation

```bash
# Build (warnings in touched files = lint gate; no linter configured)
cmake --build build -j 2>&1 | grep -E "warning|error" ; cmake --build build -j

# Tests: expect 38/38 (headless, no window or sound)
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure

# Targeted
ctest --test-dir build -R "judgment_constants_test|judgment_engine_test|music_clock_test|metronome_sync_test|gameplay_screen_test|score_keeper_test|life_keeper_test" --output-on-failure

# No raw judge-window reads left in the judgment path
grep -rn "windows\.\(fantastic\|excellent\|great\|decent\|way_off\|hit_mine\|hold_ok\|hold_roll\)" src/ \
  | grep -v "src/timing/judgment_constants\.\|src/data/judgment_constants_loader\.cpp"   # expect no output

# Scope guard
git diff --stat
git status --short .agents/stories/todo-stories.md   # still "??", untouched
```

## End-to-End Verification

1. **Automated (headless, required):** `music_clock_test` (Task 6) drives the real `MusicClock` → `music_time_for_event` →
   `JudgmentEngine` chain with a non-zero global offset, which proves the offset is applied once and has the right sign.
   `judgment_engine_test` proves scale/add reach every window read (tap, mine, expiry, hold). `metronome_sync_test` still passes
   (it measures sync against the base Fantastic, 21.5 ms, which is stricter than the effective 23 ms; leave it as is).
2. **App smoke (headless, required):** `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/blaze-4k --headless --smoke-test 10`
   exits 0, and the log shows no judgment-constants fallback warning (the shipped JSON still validates). Do **not** launch a windowed
   or interactive binary from the agent.
3. **Owner play-test (interactive; the agent doesn't run it):** play any chart. Judgments should feel the same, with very slightly
   more Fantastics on borderline hits (+1.5 ms per edge). Setting `"judge_window_add": 0.0` in `data/judgment_constants.json`
   restores the home/`[Preferences]` timing with no rebuild.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| M2 changes the timing feel (every window +1.5 ms), so scores are not comparable with older local high scores | 1.5 ms is below human perception, and existing high scores stay valid records. It is one JSON key, so the owner can revert it without a rebuild (Open Question 1) | In scope |
| Some existing engine tests hard-code `base + 1e-3` edges, which break under add 0.0015 | Task 5 switches them to `effective_windows()`. Task 3 notes the expected interim failures | In scope |
| Double application of scale/add (for example a caller passing `effective_windows()` back into `classify_tap`) | `effective_windows()` returns scale 1 and add 0, so re-adjusting it is a no-op. Only `classify_tap` and the engine call it | In scope |
| A user JSON with a large negative add makes windows ≤ 0 | `validate` rejects it, and the loader falls back to the defaults with a warning (existing mechanism) | In scope |
| Seconds vs rows: OpenITG's miss expiry is quantized to note rows (`BeatToNoteRow`, `lrintf` at 48 rows per beat); Blaze compares seconds | Same within one row (≤ 1/48 beat). This difference already existed before this change and is documented in the report | Out of scope (flag only) |
| Per-machine `GlobalOffsetSeconds` (cabinet −0.012, kit −0.007) is not adopted | It compensates for cabinet hardware latency; Blaze calibrates per user (#20) | Out of scope (intentional) |
| `InputDebounceTime` in `[Preferences-arcade]` (input filter, not judgment) was not checked | Not a judgment window, so it is outside #57. Mention it in the report as unverified | Out of scope (flag only) |
| MercifulBeginner (M3) is missing, so Beginner charts are judged more harshly than in OpenITG | Follow-up issue (Task 10) | Out of scope (follow-up) |

---

## Decisions

- **D1 — Adopt `judge_window_add = 0.0015` (dedicated-cabinet value) as the default.** The issue and TODO ask for "authentic In The
  Groove" timings. The PRD's value proposition is "the authentic ITG arcade feel" (PRD:18), and it says scores should "mean the same
  thing they do on real hardware" (PRD:104). OpenITG's own comment (`metrics.ini:250-261`) calls 0.0015 "RoXoR's value for all
  dedicated cabinets… we'll enforce RoXoR's standard", and it is on the original ITG2 drive data (`d4/…/metrics.ini:241`). The base
  windows stay at the `[Preferences]` values, because no cabinet section overrides them.
- **D2 — Scale/add live in one helper (`effective_windows()`)**, not at each call site, so the formula appears once. The struct it
  returns is already normalized (scale 1, add 0).
- **D3 — `pad_stick` is never adjusted** (OpenITG does not pass `PadStickSeconds` through `AdjustedWindow*`), as the existing
  header comment already requires.
- **D4 — No new test target**; the suite stays at 38.

## Open Questions

1. **Ship the cabinet `JudgeWindowAdd=0.0015` (D1), or keep the home `[Preferences]` 0.0?** *Proposed default:* 0.0015, for the
   rationale in D1. If the owner prefers the home value, revert Task 3 and the parity line in Task 4. Everything else (M1, tests, report)
   stays, because it is independent of the value. The report on #57 must state which value shipped either way.
2. **MercifulBeginner scope.** Is it in scope for v1, given that Beginner charts are in scope? *Proposed default:* file it as a Low-priority
   bug (Task 10) and let the owner triage it. It is not implemented here.

---

## Acceptance Criteria

- [ ] Every `windows_seconds` value checked against OpenITG with file:line references, posted on #57 (Task 9; AC 1)
- [ ] `judge_window_scale` / `judge_window_add` defaults checked against OpenITG preference defaults and the per-section overrides (`JudgeWindowAdd` in `[Preferences]` and `[Preferences-cabinet]`) (AC 2)
- [ ] Inclusive edges confirmed and pinned by tests (Task 4 exact-edge `classify_tap`, Task 5 engine edges); global offset applied once, pinned by Task 6 (AC 3)
- [ ] M1 (scale/add not applied) fixed with unit tests; M2 (cabinet add) applied with a parity test; M3 filed as a follow-up issue (AC 4)
- [ ] Build passes with no new warnings in touched files; 38/38 tests pass; headless smoke exits 0
- [ ] `.agents/stories/todo-stories.md` untouched
