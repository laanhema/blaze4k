# Implementation Report

**Plan**: `.agents/plans/010-judgment-scoring-constants-plan.md`
**Branch**: `feature/010-judgment-scoring-constants`
**Status**: COMPLETE

## Summary

Added a data-driven judgment/scoring constants table for Blaze 4k, seeded with
OpenITG arcade runtime values (tag `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51`,
`assets/patch-data/Themes/default/metrics.ini`). The pure value type + classification
helpers live in `src/timing/judgment_constants.*` (platform-free), the non-throwing
`nlohmann/json` loader with per-field merge and whole-table fallback lives in
`src/data/judgment_constants_loader.*`, a seeded `assets/data/judgment_constants.json`
ships the authoritative values, and `App::init()` performs a non-fatal startup load.

Resolved user decisions incorporated: DP weights Fantastic=+5 / Great=+2 (Excellent=4);
six tap judgments including Excellent; OpenITG grade boundaries (quad=100%, triple=99%);
tap (6) + mine + hold/roll windows, mine included, Attack excluded; OpenITG OK/NG values
and compiled `JudgeWindowSecondsRoll=0.350`.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Define constants model header | `src/timing/judgment_constants.hpp` | ✅ |
| 2 | Defaults, validation, classification | `src/timing/judgment_constants.cpp` | ✅ |
| 3 | JSON loader with fallback | `src/data/judgment_constants_loader.{hpp,cpp}` | ✅ |
| 4 | Seed authoritative JSON table | `assets/data/judgment_constants.json` | ✅ |
| 5 | Startup load in `App` | `src/app/app.{hpp,cpp}` | ✅ |
| 6 | Register sources + test target | `CMakeLists.txt`, `tests/CMakeLists.txt` | ✅ |
| 7 | Test suite | `tests/judgment_constants_test.cpp` | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Configure (`cmake -B build -DFETCHCONTENT_BASE_DIR=...`) | ✅ |
| Build (`cmake --build build -j16`) | ✅ zero warnings under `-Wall -Wextra -Wpedantic` |
| Tests (`ctest --test-dir build --output-on-failure`) | ✅ 10/10 passed |
| New test explicit run (`./build/tests/judgment_constants_test`) | ✅ all sub-checks |
| E2E smoke with seed present | ✅ logs `[JudgmentConstants] Loaded 'assets/data/judgment_constants.json'`, clean exit |
| E2E smoke with seed absent | ✅ logs `[JudgmentConstants] Warning: no constants file found ... using compiled defaults`, clean exit, file restored |
| Module purity `rg -n "SDL\|miniaudio\|ma_" src/timing/judgment_constants.*` | ✅ no matches |
| Clock independence `rg -n "wall.clock\|SDL_GetTicks\|std::chrono" src/timing/judgment_constants.*` | ✅ no matches |
| Stale SM4 defaults `rg -n "0\.0225\|0\.045\|0\.09f\|0\.135f\|0\.18f" src/` | ✅ no matches |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/timing/judgment_constants.hpp` | CREATE | +87 |
| `src/timing/judgment_constants.cpp` | CREATE | +151 |
| `src/data/judgment_constants_loader.hpp` | CREATE | +23 |
| `src/data/judgment_constants_loader.cpp` | CREATE | +199 |
| `assets/data/judgment_constants.json` | CREATE | +69 |
| `tests/judgment_constants_test.cpp` | CREATE | +242 |
| `src/app/app.hpp` | UPDATE | +3 |
| `src/app/app.cpp` | UPDATE | +14 |
| `CMakeLists.txt` | UPDATE | +2 |
| `tests/CMakeLists.txt` | UPDATE | +10 |

## Deviations from Plan

1. **Grade tier labels are not sourced from JSON values.** `GradeTier::label` is
   `const char*` (fixed by the plan's header), so storing a JSON-parsed `std::string`
   would require owning per-instance storage. The loader validates each tier's `label`
   is a string but reuses the compiled literal labels by index. The seed file's labels
   still match, so test 6 deep-equality holds; editing labels in JSON has no runtime
   effect (percentages do). Documented in the loader (`sync_tier_labels`).
2. **Loader message parameter named `message`, not `warning`.** The plan's header used
   `std::string* warning`; since success also writes an info line to the same string,
   `message` is more accurate. Signatures/behavior are otherwise as specified.
3. **Extra validation:** `judge_window_scale` and `judge_window_add` are checked finite
   (in addition to the required positive-window and monotonicity checks).
4. **`continues_combo` handles `Num`** by returning `false` to keep the switch exhaustive
   under `-Wall` (no behavioral effect; `Num` is a sentinel, never produced).

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/judgment_constants_test.cpp` | 1. defaults parity vs OpenITG (windows/weights/life/17 tiers/validate); 2. `classify_tap` boundaries + symmetry + NaN→Miss; 3. combo continuation (Decent breaks); 4. grade tier lookup; 5. JSON override without recompile (partial merge); 6. shipped seed JSON deep-equality; 7. missing file fallback + warning; 8. malformed JSON fallback + warning; 9. invalid/non-monotonic fallback + partial merge over defaults |
