# Plan: Route around miniaudio freed-node read on failed sound loads (#144)

## Summary

When an audio file is missing or corrupt, miniaudio's `ma_sound_init_from_file` (called by `SoundStream::load`) frees a data buffer node in `ma_resource_manager_data_buffer_node_acquire` and then immediately reads its `isDataOwnedByResourceManager` field. AddressSanitizer flags this as a `heap-use-after-free`, aborting the process. To allow the test suite to pass under the `asan-ubsan` preset (#136), `tests/CMakeLists.txt` previously had an expected-report regex matching this crash for `audio_test`, `select_screen_test`, and `preview_player_test`, truncating their execution.

The fix introduces a lightweight standalone pre-check in `SoundStream::load` using `ma_decoder_init_file` and `ma_decoder_uninit`. Because standalone decoders do not use miniaudio's resource manager, they fail cleanly on missing or corrupt files without triggering the use-after-free bug (and additionally prevent the 744-byte memory leak tracked in #145). When the pre-check fails, `SoundStream::load` logs the failure and returns false immediately, bypassing `ma_sound_init_from_file`. The expected-report rule in `tests/CMakeLists.txt` is removed, allowing all three tests to execute to completion and pass cleanly by exit code under both normal and sanitizer builds. `docs/BUILDING.md` is updated to remove the known report.

## User Story

As a developer and CI system running tests with AddressSanitizer
I want missing and corrupt audio files to fail cleanly without reading freed memory
So that `audio_test`, `select_screen_test`, and `preview_player_test` run to completion and pass without synthetic exception rules

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | LOW |
| Systems Affected | `audio/sound_stream`, `tests/CMakeLists.txt`, `docs/BUILDING.md` |
| GitHub Issue | #144 (Stop miniaudio reading freed memory when a sound file fails to load) |
| PRD Phase | N/A |
| Prototype Patch | `.agents/plans/068-miniaudio-sound-load-use-after-free-prototype.patch` |
| Branch (suggested) | `feature/068-miniaudio-sound-load-use-after-free` |
| Plan sequence | `068`. `067` was #139. No branch or merge since then took a number |

---

## Environment Findings

Planned on `main` @ `87f1887` (fast-forwarded with prototype patch), working tree clean.

| Tool / Fact | Value | Notes |
|---|---|---|
| Build | `build/` (Release, `-DBLAZE_WERROR=ON`, GCC) | Configured on this host |
| Baseline | `scripts/check.sh` prints `CHECK: PASS`, 53 of 53 tests | Green baseline on HEAD |
| Sanitizers | `build-asan-ubsan/` (Debug, `-DBLAZE_SANITIZE=ON`) | Configured via `cmake --preset asan-ubsan` |
| Baseline Sanitizers | `ctest --preset asan-ubsan` prints 55 of 55 tests passed | 3 tests passed via regex matching ASan report |
| Prototype run | Lint passes, format passes, zero compiler warnings | Verified in detached worktree |
| Prototype Sanitizer run | `ctest` passes 55 of 55 tests with regex removed | `audio_test`, `select_screen_test`, and `preview_player_test` run to 100% completion |
| Audio in tests | The null device (`AudioBackend::Null`) | Silent, safe without sandbox |
| Forward references to #144 | `tests/CMakeLists.txt:669-677`, `docs/BUILDING.md:189-199`, `TODO.md:58` | All locations identified |

### Bug reproduction (measured on HEAD, headless)

Command:
```bash
UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test
```

Output on HEAD (unfixed):
```
[AudioEngine] Output device: 'NULL Playback Device' via Null, requested period 480 frames (0 = backend default), period 480 frames x 3 @ 48000 Hz (10 ms/period; excludes OS mixer and Bluetooth latency)
  - AudioEngine opened the 'Null' backend.
=================================================================
==703856==ERROR: AddressSanitizer: heap-use-after-free on address 0x7c6daafe0054 at pc 0x000000590f31 bp 0x7ffdd43e95d0 sp 0x7ffdd43e95c8
READ of size 4 at 0x7c6daafe0054 thread T0
    #0 0x000000590f30 in ma_resource_manager_data_buffer_node_acquire .../miniaudio.h:68596
    #1 0x00000059229e in ma_resource_manager_data_buffer_init_ex_internal .../miniaudio.h:68818
    #2 0x000000593530 in ma_resource_manager_data_buffer_init_ex .../miniaudio.h:68935
    #3 0x00000059dc29 in ma_resource_manager_data_source_init_ex .../miniaudio.h:70188
    #4 0x0000005d02d6 in ma_sound_init_from_file_internal .../miniaudio.h:75920
    #5 0x0000005d26d8 in ma_sound_init_ex .../miniaudio.h:76064
    #6 0x0000005d0e2f in ma_sound_init_from_file .../miniaudio.h:75960
    #7 0x0000004174ae in blaze4k::SoundStream::load .../src/audio/sound_stream.cpp:78
    #8 0x000000401946 in main .../tests/audio_test.cpp:39
...
SUMMARY: AddressSanitizer: heap-use-after-free .../miniaudio.h:68596 in ma_resource_manager_data_buffer_node_acquire
```
Exit code: 1 (SIGABRT).

Output with fix (prototype patch applied):
```
[audio_test] Starting audio engine and sound stream tests...
[AudioEngine] Initialized successfully. Sample rate: 48000 Hz, Channels: 2
[AudioEngine] Output device: 'NULL Playback Device' via Null, requested period 480 frames (0 = backend default), period 480 frames x 3 @ 48000 Hz (10 ms/period; excludes OS mixer and Bluetooth latency)
  - AudioEngine opened the 'Null' backend.
[SoundStream] Failed to load audio file 'non_existent_file_123456.wav' (error code: -7)
  - Missing file handled gracefully (load returned false).
[SoundStream] Failed to load audio file '/tmp/td_audio_test/corrupt.wav' (error code: -1)
  - Corrupt file handled gracefully (load returned false).
  - Valid WAV loaded. Length: 1s
  - Seek to 0.5s reported position: 0.5s
[audio_test] All audio tests passed successfully!
```
Exit code: 0.

### Root Cause Analysis

In `miniaudio.h` (v0.11.21 through v0.11.25), `ma_resource_manager_data_buffer_node_acquire` has the following cleanup sequence:
```c
done:
    /* If we failed to initialize the data buffer we need to free it. */
    if (result != MA_SUCCESS) {
        if (nodeAlreadyExists == MA_FALSE) {
            ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);
            ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);
        }
    }

    /*
    The init notification needs to be uninitialized. This will be used if the node does not already
    exist, and we've specified ASYNC | WAIT_INIT.
    */
    if (nodeAlreadyExists == MA_FALSE && pDataBufferNode->isDataOwnedByResourceManager && (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) != 0) {
```
When file initialization fails (`result != MA_SUCCESS`), `pDataBufferNode` is freed via `ma_free(...)`. Eight lines later, `pDataBufferNode->isDataOwnedByResourceManager` is dereferenced in the `if` condition, causing a heap-use-after-free read.

---

## Decisions

| # | Decision | Reason |
|---|---|---|
| D1 | Route around the miniaudio bug with a `ma_decoder_init_file` pre-check in `SoundStream::load` | Avoids modifying third-party `miniaudio.h` or maintaining custom CMake patch machinery across platforms. `ma_decoder` operates directly on decoders without the resource manager, cleanly reporting `MA_DOES_NOT_EXIST` (-7) or format errors (-1). Additionally prevents the 744-byte leak (#145). |
| D2 | Remove `PASS_REGULAR_EXPRESSION` workaround from `tests/CMakeLists.txt` | With the clean return in place, `audio_test`, `select_screen_test`, and `preview_player_test` run to completion without error and are judged strictly by exit code 0. |
| D3 | Keep `AssistTickPlayer` unchanged | `AssistTickPlayer::init` generates a valid WAV file via `write_assist_tick_wav` before loading, or fails early on empty paths, never passing a missing or corrupt file to `ma_sound_init_from_file`. |

---

## Patterns to Follow

### Naming
```cpp
// SOURCE: src/audio/sound_stream.cpp:63-70
bool SoundStream::load(const std::string& filepath) {
    unload();

    if (!sound_) {
        sound_ = std::make_unique<ma_sound>();
    }
```

### Error Handling
```cpp
// SOURCE: src/audio/sound_stream.cpp:87-91
    if (result != MA_SUCCESS) {
        std::cerr << "[SoundStream] Failed to load audio file '" << filepath
                  << "' (error code: " << static_cast<int>(result) << ")\n";
        return false;
    }
```

### Tests
```cpp
// SOURCE: tests/audio_test.cpp:37-43
    // 2. Missing file test
    blaze4k::SoundStream stream;
    bool load_missing = stream.load("non_existent_file_123456.wav");
    TEST_CHECK(!load_missing);
    TEST_CHECK(!stream.is_loaded());
    std::cout << "  - Missing file handled gracefully (load returned false).\n";
```

---

## Files to Change

| File | Action | Covered by Patch | Purpose |
|------|--------|------------------|---------|
| `src/audio/sound_stream.cpp` | UPDATE | Yes | Add `ma_decoder` pre-check in `SoundStream::load` to return false early on missing/corrupt audio files |
| `tests/CMakeLists.txt` | UPDATE | Yes | Remove `set_tests_properties(... PASS_REGULAR_EXPRESSION ...)` workaround for the 3 tests under `BLAZE_SANITIZE` |
| `docs/BUILDING.md` | UPDATE | Yes | Remove #144 from the "Known reports" list and document the pre-check |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Add decoder pre-check in SoundStream::load

- **File**: `src/audio/sound_stream.cpp`
- **Action**: UPDATE
- **Implement**: In `SoundStream::load(const std::string& filepath)`, before calling `ma_sound_init_from_file`, initialize a temporary `ma_decoder` via `ma_decoder_init_file(filepath.c_str(), nullptr, &decoder)`. If it returns anything other than `MA_SUCCESS`, log an error message matching the existing format and return `false`. If it succeeds, uninitialize the decoder with `ma_decoder_uninit(&decoder)` and proceed to `ma_sound_init_from_file`.
- **Mirror**: `src/audio/sound_stream.cpp:78-91` (existing error logging pattern)
- **Validate**: `cmake --build build && ./build-asan-ubsan/tests/audio_test` (no ASan crash)

### Task 2: Remove expected-report rule from tests/CMakeLists.txt

- **File**: `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Delete the `set_tests_properties(audio_test select_screen_test preview_player_test PROPERTIES PASS_REGULAR_EXPRESSION ...)` block and its accompanying comment from within `if(BLAZE_SANITIZE)`.
- **Mirror**: `tests/CMakeLists.txt:652-667`
- **Validate**: `ctest --preset asan-ubsan -R "(audio_test|select_screen_test|preview_player_test)" --output-on-failure`

### Task 3: Update docs/BUILDING.md

- **File**: `docs/BUILDING.md`
- **Action**: UPDATE
- **Implement**: In the "Known reports" section, remove the bullet for #144 and the following paragraph describing the expected-report workaround for `audio_test`, `select_screen_test`, and `preview_player_test`. Note that `SoundStream::load` routes around failed loads so tests produce no report.
- **Mirror**: `docs/BUILDING.md:187-200`
- **Validate**: `scripts/lint.sh`

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

# Reproduction (BUG_FIX only): red on unfixed code, green after the fix
UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test
```

## End-to-End Verification

1. Run the reproduction command under AddressSanitizer on `audio_test`:
   ```bash
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/audio_test
   ```
   Expected outcome: Passes with exit code 0 and logs `All audio tests passed successfully!`, without printing `ERROR: AddressSanitizer: heap-use-after-free`.

2. Run `select_screen_test` and `preview_player_test` directly under AddressSanitizer:
   ```bash
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/preview_player_test
   UBSAN_OPTIONS=print_stacktrace=1 ./build-asan-ubsan/tests/select_screen_test
   ```
   Expected outcome: Both binaries execute all subtests to completion and exit with code 0 without any ASan error output.

3. Run the complete sanitizer test suite:
   ```bash
   ctest --preset asan-ubsan --output-on-failure
   ```
   Expected outcome: All 55 tests pass (100% tests passed), with `sanitizer_canary_*` confirming sanitizer enforcement and no test relying on `PASS_REGULAR_EXPRESSION` for crashes.

4. Run `scripts/check.sh`:
   Expected outcome: Prints `CHECK: PASS` with 100% tests passing in Release mode with `-DBLAZE_WERROR=ON`.

---

## Risks

| Risk | Mitigation | In / Out of Scope |
|------|------------|-------------------|
| Double-opening sound files on load adds I/O overhead | Files are only loaded when song selection changes or when entering gameplay; decode pre-check only reads the file header/metadata and closes immediately. Impact is negligible (< 1ms). | In-scope (accepted design trade-off) |
| Non-standard audio formats could behave differently between `ma_decoder` and `ma_sound` | `ma_sound_init_from_file` internally uses `ma_decoder_init_file` via resource manager's decoded supply; their decoding capabilities are identical. | In-scope |
| A corrupt WAV file in `AssistTickPlayer` might still trigger miniaudio's bug | `AssistTickPlayer` writes and verifies its own WAV file before loading; external simfiles do not pass through `AssistTickPlayer`. | In-scope |

---

## Open Questions

- **OQ1**: Does resolving #144 via the `ma_decoder_init_file` pre-check also close issue #145?
  *Proposed default*: Yes. Because `SoundStream::load` avoids calling `ma_sound_init_from_file_internal` on failed loads, the 744-byte leak is also prevented. Once verified under LeakSanitizer, issue #145 can be closed alongside #144.

- **OQ2**: Should `docs/BUILDING.md` keep the "Known reports" subsection or omit it if no known reports remain?
  *Proposed default*: Retain a concise note in `docs/BUILDING.md` that #145 is routed around in `SoundStream::load` via the pre-check, and remove the section once #145 is officially closed.

---

## Acceptance Criteria

- [ ] Loading a missing or corrupt sound file no longer produces the AddressSanitizer heap-use-after-free report in `ma_resource_manager_data_buffer_node_acquire`
- [ ] The expected-report rule for `audio_test`, `select_screen_test`, and `preview_player_test` is removed from the `BLAZE_SANITIZE` block in `tests/CMakeLists.txt`
- [ ] `ctest --preset asan-ubsan` runs those three tests in full and they pass
- [ ] `scripts/check.sh` prints `CHECK: PASS`
- [ ] The "Known reports" list in `docs/BUILDING.md` (Sanitizer build) no longer names this report
