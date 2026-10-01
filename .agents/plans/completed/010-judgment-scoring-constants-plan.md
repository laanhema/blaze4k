# Plan: Data-Driven Judgment and Scoring Constants from OpenITG

## Summary

Introduce a single, data-driven constants table that holds **every** tunable judgment/scoring number
for Blaze 4k — Judge-4 timing windows, dance-point (DP) weights, grade weights, grade-boundary
percentages, and life deltas — seeded with values lifted verbatim from OpenITG source and loadable
from JSON without recompiling (PRD §6 pattern 3). The module is split so the model stays pure and
free of platform/IO dependencies:

1. `src/timing/judgment_constants.{hpp,cpp}` — a `JudgmentConstants` value type + a compiled-in
   `compiled_defaults()` table holding the OpenITG-verified numbers, validation, and the pure
   classification helpers (`classify_tap`, `continues_combo`, `grade_for_percent`).
2. `src/data/judgment_constants_loader.{hpp,cpp}` — a `nlohmann/json` loader that merges a JSON file
   over the compiled defaults and falls back to the compiled table on a **missing, unreadable,
   malformed, or invalid** file, logging a `[JudgmentConstants]` warning.
3. A seeded `assets/data/judgment_constants.json` containing the authoritative OpenITG values, plus a
   minimal `App::init()` startup load so the "missing/malformed → fallback + log warning" acceptance
   criterion is exercised by the real entry point.

No gameplay/judgment engine is built here (that is B4/B5/B6): B2 only produces and validates the
constants and the lookup functions those stories will consume. Values are pinned from OpenITG tag
commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` (see **Value Provenance**).

## User Story

As a competitive player
I want timing windows, DP weights, grade boundaries, and life deltas loaded from a JSON config table
seeded with OpenITG-verified values
So that scoring matches real ITG behavior and stays tunable.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | LOW |
| Systems Affected | `src/timing/`, `src/data/` (NEW dir), `src/app/` (startup load), `assets/data/`, `CMakeLists.txt`, `tests/` |
| GitHub Issue | #10 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | `nlohmann_json::nlohmann_json` already fetched and linked `PUBLIC` to `blaze4k_core` (`CMakeLists.txt:40-47,102`) |
| Baseline tests | 9/9 pass | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 9" (0.19s) |
| Existing JSON use in `src/` | none | `nlohmann/json.hpp` is only used by `tests/sanity_test.cpp`; B2 is the first production JSON loader |
| `src/data/` | does not exist | Must be created; add dir to `blaze4k_core` sources |
| Assets dir | `assets/` exists, empty | Seed JSON goes under `assets/data/` |
| Upstream source | `/tmp/opencode/openitg` @ `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` | Cloned for provenance; arcade runtime overrides live in `assets/patch-data/Themes/default/metrics.ini` |

**Start green, stay green:** 9 tests currently pass; this plan adds 1 test target
(`judgment_constants_test`) → 10 expected.

---

## Pinned Semantics

**Authority for B2 is OpenITG** (PRD §15 "Reference materials"). OpenITG has two layers of values:

1. **Compiled code defaults** — `src/PrefsManager.cpp` (these are StepMania 4-era defaults).
2. **Arcade patch-data runtime overrides** — `assets/patch-data/Themes/default/metrics.ini`, applied
   over the data install by the arcade patch (`setup-dev-env.sh:19`, `gen-arcade-patch.sh`) and
   therefore the **effective ITG runtime values**. Theme/config metrics override code defaults, so the
   patch-data layer wins.

Blaze 4k seeds the **arcade runtime values** (layer 2) and documents layer 1 where it differs.

### Judgment naming map (Blaze 4k ← OpenITG ← StepMania)

| Blaze 4k | OpenITG metric name | OpenITG enum | SM legacy |
|--------|---------------------|--------------|-----------|
| Fantastic | `JudgeWindowSecondsMarvelous` | `TNS_MARVELOUS` / `TW_Marvelous` | W1 |
| Excellent | `JudgeWindowSecondsPerfect` | `TNS_PERFECT` / `TW_Perfect` | W2 |
| Great | `JudgeWindowSecondsGreat` | `TNS_GREAT` / `TW_Great` | W3 |
| Decent | `JudgeWindowSecondsGood` | `TNS_GOOD` / `TW_Good` | W4 |
| Way Off | `JudgeWindowSecondsBoo` | `TNS_BOO` / `TW_Boo` | W5 |
| Miss | (no window — beyond Way Off) | `TNS_MISS` | Miss |
| HitMine | `JudgeWindowSecondsMine` | `TNS_HIT_MINE` / `TW_Mine` | Mine |

**Window application:** `AdjustedWindowTap` applies `scale` then `add` then an optional per-difficulty
`fTimingScale` (`src/Player.cpp:34-63`). The arcade patch pins `JudgeWindowScale=1.000000` and
`JudgeWindowAdd=0.000000` (`metrics.ini:90-91`), so the base windows *are* the Judge-4-tight windows.
Blaze 4k v1 implements no judge-scaling UI; the constants are used as-is.

**Miss boundary:** a tap is a Miss when `|delta|` exceeds the Way Off window (`TW_Boo`, the widest tap
window — `src/Player.cpp:43`); there is no separate miss window.

**Combo:** Fantastic/Excellent/Great continue combo; Decent, Way Off, and Miss reset it
(`src/Player.cpp:1554-1567`; `src/ScoreKeeperMAX2.cpp:358-366`). `HitMine` is scored via
`HandleTapScore` only and never touches combo (`src/ScoreKeeperMAX2.cpp:333-341`). PRD §7.2 and issue
AC4 phrase this as "Decent breaks combo".

**Scoring type:** default `SCORING_MAX2` (`src/PrefsManager.cpp:261`), i.e. the ITG dance-point/percent
system whose weights are `PercentScoreWeight*`; the percentage denominator uses the Fantastic weight
(`src/ScoreKeeperMAX2.cpp:352-354`).

**Life:** per-judgment life deltas applied via `LifeMeterBar::ChangeLife`
(`src/LifeMeterBar.cpp:108-114`); `MercifulDrain=0` in arcade, so negative deltas are **not** scaled
by current life (`metrics.ini:156`; scaling guard at `src/LifeMeterBar.cpp:204`).

---

## Value Provenance

All values below are transcribed from the cloned OpenITG repository at commit
**`f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`** ("ok it's really over now…", 2020-12-17).

> **Arcade runtime table** = `assets/patch-data/Themes/default/metrics.ini` (wins over code defaults).
> **Compiled default** = `src/PrefsManager.cpp` (StepMania 4 defaults; used only where no arcade
> override exists, and cited for the discrepancy notes).

### Timing windows (Judge-4-tight, arcade runtime)

| Blaze 4k constant | Value (s) | Upstream key | Source (file:line) |
|-----------------|-----------|--------------|--------------------|
| `windows.fantastic` | 0.021500 | `JudgeWindowSecondsMarvelous` | `assets/patch-data/Themes/default/metrics.ini:98` |
| `windows.excellent` | 0.043000 | `JudgeWindowSecondsPerfect` | `assets/patch-data/Themes/default/metrics.ini:97` |
| `windows.great` | 0.102000 | `JudgeWindowSecondsGreat` | `assets/patch-data/Themes/default/metrics.ini:96` |
| `windows.decent` | 0.135000 | `JudgeWindowSecondsGood` | `assets/patch-data/Themes/default/metrics.ini:95` |
| `windows.way_off` | 0.180000 | `JudgeWindowSecondsBoo` | `assets/patch-data/Themes/default/metrics.ini:94` |
| `windows.hit_mine` | 0.070000 | `JudgeWindowSecondsMine` | `assets/patch-data/Themes/default/metrics.ini:93` |
| `windows.hold_ok` | 0.320000 | `JudgeWindowSecondsOK` | `assets/patch-data/Themes/default/metrics.ini:99` |
| `windows.hold_roll` | 0.350000 | `JudgeWindowSecondsRoll` | **compiled default** `src/PrefsManager.cpp:94` (no arcade override; step-off-and-back-on allowance) |
| `judge_window_scale` | 1.000000 | `JudgeWindowScale` | `assets/patch-data/Themes/default/metrics.ini:90` |
| `judge_window_add` | 0.000000 | `JudgeWindowAdd` | `assets/patch-data/Themes/default/metrics.ini:91` |

Note: the compiled default for the Fantastic window is `0.0225` and Great is `0.090`
(`src/PrefsManager.cpp:88,90`); the arcade patch tightens these to `0.0215` / `0.102`. The plan seeds
the arcade values.

### DP (percent-score) and grade weights (arcade runtime, identical tables)

| Blaze 4k constant | Value | Upstream key | Source (file:line) |
|-----------------|-------|--------------|--------------------|
| `dp_weights.fantastic` | 5 | `PercentScoreWeightMarvelous` | `metrics.ini:122` |
| `dp_weights.excellent` | 4 | `PercentScoreWeightPerfect` | `metrics.ini:121` |
| `dp_weights.great` | 2 | `PercentScoreWeightGreat` | `metrics.ini:120` |
| `dp_weights.decent` | 0 | `PercentScoreWeightGood` | `metrics.ini:119` |
| `dp_weights.way_off` | -6 | `PercentScoreWeightBoo` | `metrics.ini:118` |
| `dp_weights.miss` | -12 | `PercentScoreWeightMiss` | `metrics.ini:117` |
| `dp_weights.hit_mine` | -6 | `PercentScoreWeightHitMine` | `metrics.ini:116` |
| `dp_weights.hold_ok` | 5 | `PercentScoreWeightOK` | `metrics.ini:124` |
| `dp_weights.hold_ng` | 0 | `PercentScoreWeightNG` | `metrics.ini:123` |
| `grade_weights.*` | same 9 values as `dp_weights.*` | `GradeWeight*` | `metrics.ini:106-114` |

Weight application: `src/ScoreKeeperMAX2.cpp:509-547` (DP), `:549-591` (grade).
Percentage denominator = Fantastic weight: `src/ScoreKeeperMAX2.cpp:352-354`.

### Life deltas (arcade runtime)

| Blaze 4k constant | Value | Upstream key | Source (file:line) |
|-----------------|-------|--------------|--------------------|
| `life.fantastic` | +0.008 | `LifeDeltaPercentChangeMarvelous` | `metrics.ini:132` |
| `life.excellent` | +0.008 | `LifeDeltaPercentChangePerfect` | `metrics.ini:131` |
| `life.great` | +0.004 | `LifeDeltaPercentChangeGreat` | `metrics.ini:130` |
| `life.decent` | 0.000 | `LifeDeltaPercentChangeGood` | `metrics.ini:129` |
| `life.way_off` | -0.050 | `LifeDeltaPercentChangeBoo` | `metrics.ini:128` |
| `life.miss` | -0.100 | `LifeDeltaPercentChangeMiss` | `metrics.ini:127` |
| `life.hit_mine` | -0.050 | `LifeDeltaPercentChangeHitMine` | `metrics.ini:126` |
| `life.hold_ok` | +0.008 | `LifeDeltaPercentChangeOK` | `metrics.ini:134` |
| `life.hold_ng` | -0.080 | `LifeDeltaPercentChangeNG` | `metrics.ini:133` |
| `merciful_drain` | false/0 | `MercifulDrain` | `metrics.ini:156` |

Note: compiled life defaults differ materially (Boo `-0.040`, Miss `-0.080`, Mine `-0.160`,
Great `+0.004` — `src/PrefsManager.cpp:99-107`); the arcade patch supplies the ITG values above.

### Grade boundaries (arcade runtime) — resolves PRD §15 open item

`NumGradeTiersUsed=17` (`metrics.ini:4431`), `GradeTier02IsAllPerfects=0` (`metrics.ini:4432`).
Grades are assigned by `GetGradeFromPercent` ≥ threshold (`src/PlayerStageStats.cpp:141-153`), where
percent = Σ(count × grade_weight) ÷ Σ(possible × Fantastic grade weight) (`src/PlayerStageStats.cpp:156-188`).

| Tier | Label (theme) | `min_percent` | Source (file:line) |
|------|---------------|---------------|--------------------|
| 01 | ★★★★ (quad star) | 1.00 | `metrics.ini:4433` |
| 02 | ★★★ (triple star) | 0.99 | `metrics.ini:4434` |
| 03 | ★★ (double star) | 0.98 | `metrics.ini:4435` |
| 04 | ★ (single star) | 0.96 | `metrics.ini:4436` |
| 05 | S+ | 0.94 | `metrics.ini:4437` |
| 06 | S | 0.92 | `metrics.ini:4438` |
| 07 | S- | 0.89 | `metrics.ini:4439` |
| 08 | A+ | 0.86 | `metrics.ini:4440` |
| 09 | A | 0.83 | `metrics.ini:4441` |
| 10 | A- | 0.80 | `metrics.ini:4442` |
| 11 | B+ | 0.76 | `metrics.ini:4443` |
| 12 | B | 0.72 | `metrics.ini:4444` |
| 13 | B- | 0.68 | `metrics.ini:4445` |
| 14 | C+ | 0.64 | `metrics.ini:4446` |
| 15 | C | 0.60 | `metrics.ini:4447` |
| 16 | C- | 0.55 | `metrics.ini:4448` |
| 17 | D | -1000 | `metrics.ini:4449` |

Theme labels come from `metrics.ini:4410-4427` (`[Grade]` `Tier01..Tier17`, `Failed`).
`Grade.h:9-22` defines the `Grade` enum; `GradeToString` labels tiers `Tier01..Tier20`.

### Semantics (behavioral provenance)

| Semantic | Source (file:line) |
|----------|--------------------|
| Window = base × scale + add | `src/Player.cpp:34-63` |
| Combo continues on Great+, resets on Decent/WayOff/Miss | `src/Player.cpp:1554-1567`, `src/ScoreKeeperMAX2.cpp:358-366` |
| HitMine does not affect combo | `src/ScoreKeeperMAX2.cpp:333-341` |
| Life delta per judgment | `src/LifeMeterBar.cpp:108-114,128-130,169-181` |
| MercifulDrain scales negative deltas (disabled in arcade) | `src/LifeMeterBar.cpp:204`; `metrics.ini:156` |
| Grade percentile rule | `src/PlayerStageStats.cpp:141-153` |
| Default scoring type MAX2 | `src/PrefsManager.cpp:261` |

---

## Patterns to Follow

### Naming (modules / units)
```cpp
// SOURCE: src/timing/music_clock.hpp:28-49
class MusicClock {
public:
    void set_global_offset_seconds(double offset);
    [[nodiscard]] double global_offset_seconds() const;
    [[nodiscard]] double sample_time_seconds() const; // frames / rate
    static double seconds_from_pcm(uint64_t frames, uint32_t sample_rate);
};
```
`snake_case` methods, `[[nodiscard]]` on pure getters, `_seconds` unit suffix, `static` pure helpers.

### Error handling / logging
```cpp
// SOURCE: src/audio/sound_stream.cpp:63-67
if (result != MA_SUCCESS) {
    std::cerr << "[SoundStream] Failed to load audio file '" << filepath
              << "' (error code: " << static_cast<int>(result) << ")\n";
    return false;
}
```
Tagged `std::cerr` lines prefixed `[ModuleName]`; never throw for user-data errors — fall back to a
safe value.

### Directory-scan / filesystem guards
```cpp
// SOURCE: src/chart/song_library.cpp:254-256
bool SongLibrary::scan_directory(const std::filesystem::path& root_path) {
    std::error_code ec;
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
```
Use the `std::error_code` overloads of `<filesystem>` (non-throwing).

### Tests
```cpp
// SOURCE: tests/audio_test.cpp:14-20
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)
```
Plain `int main()` binaries, `TEST_CHECK`, temp-dir fixtures via `<filesystem>`, relative-path
fallback search for repo fixtures (`tests/parser_hardening_test.cpp:83-88`).

### Test + source registration
```cmake
# SOURCE: tests/CMakeLists.txt:82-90
add_executable(music_clock_test
    music_clock_test.cpp
)
target_link_libraries(music_clock_test PRIVATE blaze4k_core)
add_test(NAME music_clock_test COMMAND music_clock_test)
```
```cmake
# SOURCE: CMakeLists.txt:80-93
add_library(blaze4k_core STATIC
    ...
    src/timing/music_clock.cpp
)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/timing/judgment_constants.hpp` | CREATE | `TapJudgment`/`HoldJudgment` enums, `TimingWindows`, `Weights`, `LifeDeltas`, `GradeTier`, `JudgmentConstants`; compiled defaults, validation, classification helpers |
| `src/timing/judgment_constants.cpp` | CREATE | `compiled_defaults()` table (OpenITG values), `validate()`, `classify_tap()`, `continues_combo()`, `grade_for_percent()` |
| `src/data/judgment_constants_loader.hpp` | CREATE | `load_judgment_constants(path)` + candidate-path startup helper declaration |
| `src/data/judgment_constants_loader.cpp` | CREATE | nlohmann parse + merge-over-defaults + fallback with `[JudgmentConstants]` warnings |
| `assets/data/judgment_constants.json` | CREATE | Seeded table with the OpenITG-verified values + `source` provenance string |
| `src/app/app.hpp` | UPDATE | Store active `JudgmentConstants` + `[[nodiscard]] const JudgmentConstants& judgment_constants() const` |
| `src/app/app.cpp` | UPDATE | Load constants during `init()` from candidate paths; log result; never block startup |
| `CMakeLists.txt` | UPDATE | Add `src/timing/judgment_constants.cpp` and `src/data/judgment_constants_loader.cpp` to `blaze4k_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `judgment_constants_test` |
| `tests/judgment_constants_test.cpp` | CREATE | Defaults parity vs OpenITG, JSON round-trip, missing/malformed/invalid fallback, boundary classification, grade tiers |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Define the constants model header

- **File**: `src/timing/judgment_constants.hpp`
- **Action**: CREATE
- **Implement**:
  ```cpp
  namespace blaze4k {

  enum class TapJudgment { Fantastic, Excellent, Great, Decent, WayOff, Miss, HitMine, Num };
  enum class HoldJudgment { Ok, Ng, Num };

  struct TimingWindows {
      double fantastic = 0.0215;
      double excellent = 0.0430;
      double great     = 0.1020;
      double decent    = 0.1350;
      double way_off   = 0.1800;
      double hit_mine  = 0.0700;
      double hold_ok   = 0.3200;
      double hold_roll = 0.3500;
      double judge_window_scale = 1.0;
      double judge_window_add   = 0.0;
  };

  struct Weights { // used for both DP (percent) and grade weights; equal in OpenITG arcade
      int fantastic = 5, excellent = 4, great = 2, decent = 0, way_off = -6,
          miss = -12, hit_mine = -6, hold_ok = 5, hold_ng = 0;
  };

  struct LifeDeltas {
      double fantastic = 0.008, excellent = 0.008, great = 0.004, decent = 0.0,
             way_off = -0.050, miss = -0.100, hit_mine = -0.050,
             hold_ok = 0.008, hold_ng = -0.080;
      bool merciful_drain = false;
  };

  struct GradeTier { double min_percent = 0.0; const char* label = ""; };

  struct JudgmentConstants {
      TimingWindows windows;
      Weights dp_weights;
      Weights grade_weights;
      std::array<GradeTier, 17> grade_tiers{}; // index 0 = highest (quad star)
      LifeDeltas life;

      static const JudgmentConstants& compiled_defaults();

      [[nodiscard]] bool validate(std::string* error = nullptr) const;

      // Pure lookups (units: seconds, percent as fraction 0.0–1.0)
      [[nodiscard]] TapJudgment classify_tap(double delta_seconds) const;
      [[nodiscard]] bool continues_combo(TapJudgment j) const;
      [[nodiscard]] const GradeTier& grade_for_percent(double percent) const;
  };

  } // namespace blaze4k
  ```
  - Includes: `<array>`, `<string>` only. **No** `<filesystem>`, IO, SDL, or miniaudio (model stays
    pure, mirroring `music_clock.hpp`).
  - `#pragma once`; document the provenance commit in a header comment.
  - `grade_for_percent` on a percent below the last tier returns the last tier (D), matching
    "≥ threshold, first match wins" semantics.
- **Mirror**: `src/timing/music_clock.hpp:1-54`
- **Validate**: `cmake --build build -j16` (header alone compiles once included by Task 2).

### Task 2: Implement defaults, validation, and classification

- **File**: `src/timing/judgment_constants.cpp`
- **Action**: CREATE
- **Implement**:
  - `compiled_defaults()` returns a function-local `static const` with the **Value Provenance** table:
    windows, DP/grade weights, life deltas, and the 17 grade tiers
    (`1.00, 0.99, 0.98, 0.96, 0.94, 0.92, 0.89, 0.86, 0.83, 0.80, 0.76, 0.72, 0.68, 0.64, 0.60, 0.55, -1000`)
    with labels `quad_star, triple_star, double_star, single_star, S+, S, S-, A+, A, A-, B+, B, B-, C+, C, C-, D`.
  - `validate()`: all window values finite and `> 0`; demands
    `fantastic <= excellent <= great <= decent <= way_off` (monotonic); `hold_ok/hold_roll/hit_mine > 0`;
    grade tiers strictly non-increasing by `min_percent` and ≥ 1 tier; all life deltas finite; on
    failure write a human-readable reason to `*error` (when non-null) and return `false`.
  - `classify_tap(delta)`:
    - `|delta| <= windows.fantastic` → `Fantastic`
    - `<= excellent` → `Excellent`; `<= great` → `Great`; `<= decent` → `Decent`
    - `<= way_off` → `WayOff`; otherwise `Miss`
    - `HitMine` is **not** produced by `classify_tap` (mines are resolved separately by B4); assert
      `delta` is finite by treating NaN as `Miss` (documented).
  - `continues_combo(j)`: `true` for `Fantastic/Excellent/Great`; `false` for `Decent/WayOff/Miss`;
    `HitMine` → `true` (does not reset combo — `src/ScoreKeeperMAX2.cpp:333-341`).
  - `grade_for_percent(p)`: iterate tiers in order, first `p >= min_percent` wins; fallback = last tier.
- **Mirror**: `src/timing/music_clock.cpp` (pure math, guarded helpers, `[Module]` logging style)
- **Validate**: `cmake --build build -j16` with zero warnings under `-Wall -Wextra -Wpedantic`.

### Task 3: Implement the JSON loader with fallback

- **Files**: `src/data/judgment_constants_loader.hpp`, `src/data/judgment_constants_loader.cpp`
- **Action**: CREATE
- **Implement**:
  - Header:
    ```cpp
    namespace blaze4k {
    // Loads from `path`; on any failure returns compiled defaults and appends a
    // [JudgmentConstants] warning to `warning` (when non-null). Never throws.
    [[nodiscard]] JudgmentConstants load_judgment_constants(
        const std::filesystem::path& path, std::string* warning = nullptr);

    // Startup discovery order; first existing file wins, else defaults.
    [[nodiscard]] JudgmentConstants load_judgment_constants_from_candidates(
        const std::vector<std::filesystem::path>& candidates, std::string* warning = nullptr);
    } // namespace blaze4k
    ```
  - `.cpp` behavior:
    1. `std::error_code` existence check; missing/unreadable → warn + `compiled_defaults()`.
    2. Parse with `nlohmann::json::parse(..., nullptr, false)` (non-throwing); on `is_discarded()` →
       warn + defaults.
    3. Start from `compiled_defaults()`, then override **present** fields only (partial file keeps
       compiled defaults for absent keys). Accept the nested schema below; ignore unknown keys.
    4. Run `validate()`; on failure → warn + defaults (do not return a partially-invalid table).
    5. Success: return the merged table. Every path emits exactly one `[JudgmentConstants]` line on
       fallback; success logs an info line with the path.
  - **JSON schema** (also written to the seed file in Task 4):
    ```json
    {
      "version": 1,
      "source": "OpenITG f2c129fe65c65e4a9b3a691ff35e7717b4e8de51 assets/patch-data/Themes/default/metrics.ini",
      "windows_seconds": { "fantastic": 0.0215, "excellent": 0.043, "great": 0.102,
        "decent": 0.135, "way_off": 0.18, "hit_mine": 0.07, "hold_ok": 0.32,
        "hold_roll": 0.35, "judge_window_scale": 1.0, "judge_window_add": 0.0 },
      "dp_weights": { "fantastic": 5, "excellent": 4, "great": 2, "decent": 0,
        "way_off": -6, "miss": -12, "hit_mine": -6, "hold_ok": 5, "hold_ng": 0 },
      "grade_weights": { "fantastic": 5, "excellent": 4, "great": 2, "decent": 0,
        "way_off": -6, "miss": -12, "hit_mine": -6, "hold_ok": 5, "hold_ng": 0 },
      "grade_tiers": [ { "min_percent": 1.0, "label": "quad_star" }, ... { "min_percent": -1000, "label": "D" } ],
      "life_deltas": { "fantastic": 0.008, "excellent": 0.008, "great": 0.004, "decent": 0.0,
        "way_off": -0.05, "miss": -0.1, "hit_mine": -0.05, "hold_ok": 0.008,
        "hold_ng": -0.08, "merciful_drain": false }
    }
    ```
  - Include `<nlohmann/json.hpp>` (already PUBLIC-linked), `<filesystem>`, `<fstream>`, `<iostream>`,
    `<string>`, `<vector>`.
- **Mirror**: `src/chart/song_library.cpp` filesystem guards; `src/audio/sound_stream.cpp:63-67` logging.
- **Validate**: `cmake --build build -j16`.

### Task 4: Seed the authoritative JSON table

- **File**: `assets/data/judgment_constants.json`
- **Action**: CREATE
- **Implement**: Write the exact schema from Task 3 populated with the **Value Provenance** values
  (window ordering, full 17-tier list, arcade life deltas, `merciful_drain: false`). Include the
  `source` provenance string naming the upstream commit and file.
- **Mirror**: n/a (data file).
- **Validate**: Task 7 test 6 loads this file and asserts it equals `compiled_defaults()`.

### Task 5: Wire a startup load in `App`

- **Files**: `src/app/app.hpp`, `src/app/app.cpp`
- **Action**: UPDATE
- **Implement**:
  - `app.hpp`: `#include "timing/judgment_constants.hpp"`; add private member
    `JudgmentConstants judgment_constants_;` and
    `[[nodiscard]] const JudgmentConstants& judgment_constants() const { return judgment_constants_; }`.
  - `app.cpp`: in `init()`, call `load_judgment_constants_from_candidates({"data/judgment_constants.json",
    "assets/data/judgment_constants.json"})` (relative to CWD, matching the project's
    "portable data folder next to binary" intent — PRD §9); store the result and print the returned
    warning/info to `std::cout`/`std::cerr`. A load failure must **never** fail `init()` or abort
    startup (AC3).
  - Purely additive; no behavior change to the render/update loop.
- **Mirror**: `src/app/app.cpp` init logging (`[App] ...` style).
- **Validate**: `cmake --build build -j16 && ./build/blaze-4k --headless --smoke-test 5` — expect a
  `[JudgmentConstants] ...` line and a clean exit.

### Task 6: Register sources and the test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root `CMakeLists.txt`: add `src/data/judgment_constants_loader.cpp` and
    `src/timing/judgment_constants.cpp` to the `blaze4k_core` list (after `src/timing/music_clock.cpp`,
    line ~92).
  - `tests/CMakeLists.txt`: append a `judgment_constants_test` executable linking `blaze4k_core` and
    `add_test(...)`, mirroring the `music_clock_test` block.
- **Mirror**: `CMakeLists.txt:80-93`, `tests/CMakeLists.txt:82-90`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 7: Create the test suite

- **File**: `tests/judgment_constants_test.cpp`
- **Action**: CREATE
- **Implement** (use `TEST_CHECK`; deterministic, hardware-independent):
  1. **Defaults parity vs upstream**: assert every field of `compiled_defaults()` equals the exact
     OpenITG values (windows `0.0215/0.043/0.102/0.135/0.18/0.07/0.32/0.35`, DP/grade weights
     `5/4/2/0/-6/-12/-6/5/0`, life `0.008/0.008/0.004/0/-0.05/-0.1/-0.05/0.008/-0.08`, 17 tiers with
     the listed percentages). This is the source-parity assertion.
  2. **`classify_tap` boundaries**: exact boundary (`delta == fantastic`) → `Fantastic`; `fantastic+ε`
     → `Excellent`; `excellent+ε` → `Great`; `great+ε` → `Decent`; `decent+ε` → `WayOff`;
     `way_off+ε` → `Miss`; symmetric negative deltas classify identically; `NaN` → `Miss`.
  3. **Combo semantics**: `continues_combo` true for Fantastic/Excellent/Great/HitMine, false for
     Decent/WayOff/Miss (AC4 "Decent breaks combo").
  4. **Grade tiers**: `grade_for_percent(1.0)`→`quad_star`; `0.99`→`triple_star`; `0.98`→`double_star`;
     `0.96`→`single_star`; `0.94`→`S+`; `0.55`→`C-`; `0.54`→`D`.
  5. **Configurable without recompiling**: write a JSON file (temp dir) overriding `windows.great`
     (and one weight) with different values, load it, and assert `classify_tap`/weights change
     accordingly while untouched fields equal defaults.
  6. **Seed file parity**: locate `assets/data/judgment_constants.json` with the same relative-path
     fallback search used by `tests/parser_hardening_test.cpp:83-88`; load it; assert it deep-equals
     `compiled_defaults()`.
  7. **Missing file fallback**: load a non-existent path → result == defaults and a warning string was
     produced (AC3).
  8. **Malformed JSON fallback**: write `"{ not json"` → defaults + warning (AC3).
  9. **Invalid values fallback**: write a file with `windows.great = -1` (or non-monotonic windows) →
     defaults + warning; also a partial file missing keys → absent keys equal defaults and present
     overrides applied.
- **Mirror**: `tests/audio_test.cpp:14-70`, `tests/music_clock_test.cpp`, `tests/parser_hardening_test.cpp:83-88`.
- **Validate**: `ctest --test-dir build --output-on-failure` (expect 10/10).

---

## Validation

```bash
# Configure (build dir already exists; re-run only if CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 10/10: 9 existing + judgment_constants_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/judgment_constants_test

# Smoke-run the real entry point with the seed file present and absent
./build/blaze-4k --headless --smoke-test 5
mv assets/data/judgment_constants.json /tmp/opencode/jc.json && ./build/blaze-4k --headless --smoke-test 5 \
  && mv /tmp/opencode/jc.json assets/data/judgment_constants.json
```

## End-to-End Verification

1. `./build/tests/judgment_constants_test` prints all sub-checks; test 1 proves the compiled table
   matches the pinned OpenITG commit values, test 6 proves the seeded JSON matches them, and tests
   5/7/8/9 prove configurability + graceful fallback.
2. `ctest --test-dir build --output-on-failure` → **10/10**; confirm the 9 prior tests stay green
   (only additive registration changes).
3. Run `./build/blaze-4k --headless --smoke-test 5` with `assets/data/judgment_constants.json`
   present: expect an info `[JudgmentConstants]` line naming the file. Temporarily rename the seed
   file and re-run: expect a warning and a clean startup (fallback), then restore the file.
4. Enforce module purity:
   `rg -n "SDL|miniaudio|ma_" src/timing/judgment_constants.*` → **no matches** (timing model is
   platform-free); the loader is the only place that touches filesystem/JSON.
5. `rg -n "0\.0225|0\.045|0\.09f|0\.135f|0\.18f" src/` → no stale SM4 compiled defaults leak into
   `compiled_defaults()` (the arcade values are used instead).

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The issue/PRD example "Fantastic +2 DP" conflicts with OpenITG arcade `PercentScoreWeightMarvelous=5` (the `2` matches the *compiled StepMania default grade weight* `PrefsManager.cpp:140`, not the ITG runtime table) | Seed the runtime-effective arcade value (5) and document both; surface as a **blocking Open Question** for human ratification before B5 encodes tests | **In scope** — flagged |
| Two OpenITG value layers (code defaults vs arcade patch-data) could be confused | Plan explicitly seeds the arcade patch-data (runtime winner) and cites both layers in **Value Provenance**; test 1 pins the chosen values | **In scope** |
| Partial JSON could silently drop required values | Loader merges over compiled defaults per field; `validate()` rejects invalid tables and falls back whole-table | **In scope** |
| Malformed JSON throwing and crashing startup | Use non-throwing `json::parse(..., false)` + `is_discarded()`; `std::error_code` filesystem calls; never throw in loader | **In scope** |
| Windows/grade/life mismatch as later stories consume the table | Single source of truth + pure lookups (`classify_tap`, `continues_combo`, `grade_for_percent`); B4/B5/B6 must consume these | **In scope** |
| Adding `src/data/` and App startup load ripples into build/tests | Sources are additive; App change is non-fatal; run full 9-test suite after each task | **In scope** |
| Judge scaling / Attack windows / NG-from-hold edge cases not modeled | v1 fixes `judge_window_scale=1`, `judge_window_add=0`; Attack window excluded (no attacks in v1); NG life delta retained | **Out of scope** — documented |
| JSON asset not found relative to binary on some platforms | Candidate list covers `data/...` and `assets/data/...`; missing → compiled defaults (AC3). Robust asset-path resolution is C4/C5 work | **Out of scope** — documented |
| Seed file could drift from `compiled_defaults()` | Test 6 loads the shipped seed file and asserts deep equality with defaults | **In scope** |

---

## Decisions

- **One table, two modules**: the value type stays in `src/timing/` (pure, platform-free) while JSON
  IO lives in the new `src/data/`, matching the issue's "Files: `src/timing/` (windows), `src/data/`
  (JSON loading)" note.
- **Seed the arcade patch-data values**, not the StepMania-4 compiled defaults, because theme/config
  metrics override code and the arcade table is the effective ITG runtime behavior (PRD §15 names
  OpenITG the authority).
- **Model 6 tap judgments + HitMine + hold OK/NG**, following OpenITG's `TapNoteScore`/`HoldNoteScore`
  rather than the issue's 5-name list (which omits Excellent). See Open Question 2.
- **Per-field merge over defaults** for partial JSON, with whole-table fallback when validation fails.
- **Non-throwing loader** using `error_code` + `nlohmann::json::parse(..., false)`, mirroring the
  parser/simfile untrusted-input rule.
- **Startup wiring is minimal and non-fatal**: `App::init()` loads and logs, but gameplay does not
  consume the table until B4/B5/B6.

---

## Open Questions

1. **BLOCKING — "Fantastic +2 DP" vs OpenITG source (needs human decision).** PRD §5 story 2 and issue
   AC4 say "Fantastic +2 DP". OpenITG's runtime arcade table gives
   `PercentScoreWeightMarvelous=5` (DP) and `GradeWeightMarvelous=5`; the value `2` appears only as the
   *compiled StepMania-4 grade weight default* (`src/PrefsManager.cpp:140`) and as the arcade
   `PercentScoreWeightGreat`/`GradeWeightGreat`. Proposed default: **Fantastic = +5 DP, Great = +2 DP**
   (sourced arcade table), treating the PRD "+2" as a stale example from code defaults. Confirm before
   B5 writes scoring tests, otherwise the parity tests will encode the wrong number.
2. **Judgment count/naming.** The issue AC lists only Fantastic/Great/Decent/Way Off/Miss (5), but ITG
   and OpenITG define six tap judgments including **Excellent** (`TNS_PERFECT` / `PercentScoreWeightPerfect=4`).
   Proposed default: model all six (Fantastic, Excellent, Great, Decent, Way Off, Miss). Confirm
   whether Blaze 4k's HUD should surface Excellent or fold it into Fantastic.
3. **Grade example in PRD.** PRD §5 story 2 says "99%+ earns ★★★★"; OpenITG arcade sets four-star
   (quad) = **1.00** and three-star = **0.99**. Proposed default: follow OpenITG (four stars requires
   100%). Confirm the results-screen star mapping is Blaze 4k's own presentation choice.
4. **Runtime JSON location / precedence.** Proposed: `data/judgment_constants.json` (portable folder
   next to the binary, PRD §9) with `assets/data/judgment_constants.json` as a development fallback.
   Confirm before C4's config module formalizes path resolution.
5. **Scope of windows in the table.** Proposed: include tap windows (6), mine, hold OK/roll; exclude
   `Attack` window (`metrics.ini:100`, no attacks in v1) and judge-scaling UI. Confirm holding/roll
   windows belong in B2 rather than B4.
6. **Hold/roll NG semantics.** OpenITG's `JudgeWindowSecondsOK=0.320` and
   `LifeDeltaPercentChangeNG=-0.080` are included; roll re-hit window `JudgeWindowSecondsRoll=0.350` is
   a compiled default with no arcade override. Confirm these are the intended v1 hold/roll constants.

---

## Acceptance Criteria

- [ ] Given the constants table, when loaded from JSON, then Judge-4-tight windows, DP weights, grade
      boundaries, and life deltas are all configurable without recompiling (test 5)
- [ ] Given the seeded defaults, when compared against OpenITG source, then windows, weights, life
      behavior, and grade thresholds match the reference commit (tests 1 & 6 + Value Provenance)
- [ ] Given a missing or malformed constants file, when the game starts, then it falls back to
      compiled-in OpenITG defaults with a log warning (tests 7/8/9 + E2E step 3)
- [ ] Given the constants, when a Fantastic/Great/Decent/Way Off/Miss is evaluated, then the correct
      window boundaries apply and Decent breaks combo (tests 2 & 3)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → 10/10 pass (9 prior + `judgment_constants_test`)
- [ ] Follows existing module/naming/test/CMake patterns; `src/timing/` remains platform-free
