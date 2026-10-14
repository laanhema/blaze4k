# Plan: Verify and close miniaudio failed sound load leak (#145)

## Summary

When a sound file fails to load, miniaudio 0.11.21's internal resource manager leaks 744 bytes in `ma_sound_init_from_file_internal`. In issue #144, `SoundStream::load` (`src/audio/sound_stream.cpp`) introduced a standalone `ma_decoder_init_file` pre-check that detects missing or corrupt audio files before calling `ma_sound_init_from_file`, routing around the resource manager failure path. This plan verifies under AddressSanitizer and LeakSanitizer (`ctest --preset asan-ubsan`) that `audio_test`, `select_screen_test`, and `preview_player_test` run to 100% completion with zero leaks reported. The "Known reports" list in `docs/BUILDING.md` is updated to state "none", and `TODO.md` is updated to mark item 59 complete.

## User Story

As a developer and CI system running tests with AddressSanitizer and LeakSanitizer
I want failed sound-file loads to produce zero memory leaks
So that test runs execute to full completion without any leak reports or expected-report suppression rules

## Metadata

| Field | Value |
|-------|-------|
| Type | REFACTOR |
| Complexity | LOW |
| Systems Affected | `docs/BUILDING.md`, `TODO.md`, audio test verification |
| GitHub Issue | #145 |
| PRD Phase | N/A |
| Prototype Patch | N/A |
| Branch (suggested) | `feature/069-miniaudio-sound-load-leak` |
| Plan sequence | 069 |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `docs/BUILDING.md` | UPDATE | Remove #145 from "Known reports" in the Sanitizer build section |
| `TODO.md` | UPDATE | Mark item 59 complete (edited locally, gitignored) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Verify zero memory leaks under AddressSanitizer / LeakSanitizer

- **File**: `tests/audio_test.cpp`
- **Action**: UPDATE
- **Implement**: Run `audio_test`, `preview_player_test`, and `select_screen_test` directly and under `ctest --preset asan-ubsan` to verify that `SoundStream::load`'s pre-check routes around failed loads with zero memory leaks reported by LeakSanitizer.
- **Mirror**: `tests/audio_test.cpp:37-56`
- **Validate**: `ctest --preset asan-ubsan -R "(audio_test|select_screen_test|preview_player_test)" --output-on-failure`

### Task 2: Update docs/BUILDING.md

- **File**: `docs/BUILDING.md`
- **Action**: UPDATE
- **Implement**: In the "Known reports" section under the Sanitizer build instructions, update the text to state that no known reports remain, as the pre-check in `SoundStream::load` routes around failed sound loads on missing or corrupt files.
- **Mirror**: `docs/BUILDING.md:187-195`
- **Validate**: `scripts/lint.sh`

### Task 3: Mark TODO.md item 59 complete

- **File**: `TODO.md`
- **Action**: UPDATE
- **Implement**: In `TODO.md`, update line 59 from `- [ ] miniaudio leaks 744 bytes...` to `- [x] miniaudio leaks 744 bytes...`.
- **Mirror**: `TODO.md:58-59`
- **Validate**: `grep -F "miniaudio leaks 744 bytes" TODO.md`

---

## Validation

```bash
# Type check / Build
cmake --build build

# Lint
scripts/lint.sh

# Format check
scripts/format.sh --check

# Full CI validation check
scripts/check.sh

# Sanitizer build and full test suite
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan -j"$(nproc)" && ctest --preset asan-ubsan
```

## End-to-End Verification

1. Run the test binaries under AddressSanitizer and LeakSanitizer:
   ```bash
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/select_screen_test
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/preview_player_test
   ```
   Expected outcome: All three binaries exit with code 0 with zero LeakSanitizer or AddressSanitizer reports.
2. Run the complete sanitizer test suite:
   ```bash
   ctest --preset asan-ubsan --output-on-failure
   ```
   Expected outcome: 100% tests pass (all 55 tests passed), with `sanitizer_canary_*` proving sanitizer enforcement.
3. Run `scripts/check.sh`:
   Expected outcome: Prints `CHECK: PASS` with 100% tests passing in Release mode with warnings as errors and zero lint failures.

## Acceptance Criteria

- [ ] A failed sound-file load no longer leaks: LeakSanitizer reports nothing for `audio_test`, `select_screen_test` and `preview_player_test` under `ctest --preset asan-ubsan`
- [ ] `ctest --preset asan-ubsan` passes with no expected-report rule for this leak in `tests/CMakeLists.txt`
- [ ] The miniaudio pin in `CMakeLists.txt` remains unchanged (no repeated measurements in `docs/AUDIO_LATENCY.md` required)
- [ ] `scripts/check.sh` prints `CHECK: PASS`
- [ ] The "Known reports" list in `docs/BUILDING.md` (Sanitizer build) no longer names this leak
