# Code Review: feature-068-miniaudio-sound-load-use-after-free

**Scope**: branch `feature/068-miniaudio-sound-load-use-after-free` vs merge base `f6002fe` (including uncommitted changes)
**Recommendation**: APPROVE

## Summary

Reviewed branch `feature/068-miniaudio-sound-load-use-after-free` implementing the fix for issue #144. The change introduces a standalone `ma_decoder_init_file` / `ma_decoder_uninit` pre-check in `SoundStream::load` to detect missing or corrupt files before `ma_sound_init_from_file`, avoiding miniaudio's `heap-use-after-free` in `ma_resource_manager_data_buffer_node_acquire`. All acceptance criteria are satisfied:

- **Criterion 1 (No ASan heap-use-after-free report)**: Met. Direct execution of `audio_test`, `preview_player_test`, and `select_screen_test` under `UBSAN_OPTIONS=print_stacktrace=1` in `build-asan-ubsan/` passed with exit code 0 and zero ASan reports.
- **Criterion 2 (Removal of expected-report rule in `tests/CMakeLists.txt`)**: Met. The `set_tests_properties` call matching the `heap-use-after-free` regex was removed from the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt`.
- **Criterion 3 (`ctest --preset asan-ubsan` runs all three tests in full and passes)**: Met. `ctest --preset asan-ubsan` completed 55/55 tests (100% pass), executing all subtests in full.
- **Criterion 4 (`scripts/check.sh` prints `CHECK: PASS`)**: Met. `scripts/check.sh` passed all 5 stages (lint, format, configure, build with `-DBLAZE_WERROR=ON`, and test 53/53) and printed `CHECK: PASS`.
- **Criterion 5 (`docs/BUILDING.md` no longer names #144)**: Met. Issue #144 was removed from the "Known reports" section in `docs/BUILDING.md`.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

**Noted, not a finding**: Opening audio files twice during load (pre-check via `ma_decoder_init_file` followed by `ma_sound_init_from_file`) introduces negligible I/O overhead (< 1ms metadata probe) and was an explicitly accepted trade-off in the plan to avoid patching vendored `miniaudio.h`.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` (Release) & `cmake --build --preset asan-ubsan` (Debug ASan+UBSan); Linux GCC C++20 |
| Warnings | NONE | Recompiled `src/audio/sound_stream.cpp` with `-Wall -Wextra -Wpedantic -Werror`; zero warnings emitted |
| Lint | PASS | `scripts/lint.sh` & `scripts/format.sh --check`; host Linux |
| Tests | PASS | `scripts/check.sh`: 53/53 tests passed; `ctest --preset asan-ubsan`: 55/55 tests passed (both silent null device) |
| UI verification | N/A | Audio file pre-check and test configuration only; no UI markup, layout, or shaders altered |

## What's Good

- Elegant and minimal bypass: Probing with `ma_decoder_init_file` avoids modifying vendored `miniaudio.h` while keeping the build completely clean.
- Tests now execute to completion under AddressSanitizer rather than relying on synthetic exit regex matches.
- All code formatting, linting rules, and documentation conventions in `AGENTS.md` and `docs/BUILDING.md` are strictly maintained.

## Recommendation

APPROVE. The change is complete, fully verified, and ready to be merged.
