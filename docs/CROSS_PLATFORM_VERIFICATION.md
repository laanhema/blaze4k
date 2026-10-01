# Cross-Platform Verification (D4)

This is the verification record for issue #26 / PRD §12 Phase D
("Cross-platform builds verified; performance pass"). It separates what the
repository can **machine-check** from what only the **owner** can verify on real
hardware. Nothing in the table below labelled "owner" has been executed by the
repository's automated checks — do not treat it as verified.

The verification host for the automated leg is **Linux/Fedora, x86_64, GCC
16.2.1, CMake 4.4.3**. There is no MSVC, Clang, MinGW, osxcross, GPU, or
dance-pad hardware there, so the Windows/macOS builds, real FPS, pad traversal,
and OpenITG feel test must be run by the owner.

## Acceptance-criteria map

| AC (issue #26 / PRD §11) | Machine-checked here | Owner-manual step | Evidence |
| ------------------------ | -------------------- | ----------------- | -------- |
| **AC1** fresh clone → native binaries (Linux) | ✅ `scripts/fresh-clone-check.sh`, `linux-gcc-*` presets → **34/34** | — | Script/pre­set output |
| **AC1** native Windows/macOS builds | ❌ no toolchain on host | ✅ run the `windows-msvc-*` / `macos-clang-*` presets | Configure/build/test log |
| **AC2** stable frame rate / no hitches | ⚠️ headless CPU-budget proxy (`perf_loop_test`) only | ✅ `--perf-report` over a full song on a GPU | `[perf]` block |
| **AC3** arcade loop has no dead ends | ✅ `screen_manager_test` arcade-loop contract + headless smoke | ✅ full pad traversal | Screenshot/log per screen |
| **AC4** OpenITG side-by-side feel | ❌ impossible headless | ✅ pad hardware feel test (PRD §12D) | Written notes |

Legend: ✅ machine-checked in this repo · ⚠️ partial proxy · ❌ owner only.

## Automated evidence already captured

Run from a clean checkout:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16
ctest --test-dir build --output-on-failure     # 100% tests passed out of 34
```

- **34/34 tests pass** in ~0.5 s.
- `perf_loop_test` plays the reference chart (`Aurora Borealis`) to `Cleared`
  headlessly and prints `p50/p95/p99/max` update milliseconds; the generated
  release build reported `p99` well under `0.02 ms` (order of magnitude only —
  the exact value varies per run and per host; this is a CPU-budget proxy,
  **not** a GPU/vsync FPS proof).
- `screen_manager_test` asserts every canonical screen (`Title`, `Attract`,
  `Select`, `Gameplay`, `Results`) is registered and has an exit edge, and that
  `Title → Select → Gameplay → Results → Select → Title` traverses.
- `scripts/fresh-clone-check.sh` exports a clean tree, builds, and runs
  **34/34**; `ctest --preset linux-gcc-release` runs **34/34**.

## Owner checklist

Mark each row "owner-verified" only after running it on the target hardware.
Capture the command output or artifact named in the last column.

### AC1 — native Windows and macOS builds

- [ ] **Windows (MSVC)** owner-verified
  1. `cmake --preset windows-msvc-release`
  2. `cmake --build --preset windows-msvc-release`
  3. `ctest --preset windows-msvc-release`
  - Expected: configure/build succeed; **34/34** tests pass.
  - Evidence: configure/build/test log. If a preset generator is unavailable,
    fall back to `cmake -B build -G "Visual Studio 17 2022" -A x64`.
- [ ] **macOS (AppleClang)** owner-verified
  1. `cmake --preset macos-clang-release`
  2. `cmake --build --preset macos-clang-release`
  3. `ctest --preset macos-clang-release`
  - Expected: **34/34** tests pass.
  - Evidence: configure/build/test log.
- [ ] **Linux Clang** owner-verified (optional, if Clang is installed):
  `cmake --preset linux-clang-release && cmake --build --preset linux-clang-release && ctest --preset linux-clang-release`

### AC2 — real frame rate over a full song

- [ ] Owner-verified on a mid-range machine with a GPU:
  1. Launch with a real song pack: `./build/blaze-4k --perf-report --songs <pack>`
  2. Play one full song, then exit cleanly.
  3. Read the `[perf]` block and compare `p99`/`max` to the display's vsync
     period (pass `--perf-budget-ms <period>` to have the verdict use it, e.g.
     `--perf-budget-ms 8.33` for 120 Hz).
  - Expected: `p99` ≈ vsync period, `hitches` ≈ 0, verdict `PASS`, gameplay
    visibly smooth with no hitching.
  - Evidence: paste the `[perf]` block. Note the machine, GPU, resolution, and
    refresh rate alongside it.

### AC3 — full arcade loop on pad hardware

- [ ] Owner-verified: traverse the whole loop with a USB dance pad —
  boot → Title → (idle) Attract → Select → Gameplay → Results → Select → Title.
  - Expected: every screen is reachable and escapable; no screen strands you;
    Back behaves as documented (`Attract → origin`, `Select → Title`,
    `Gameplay/Results/Calibration/InputRemap → Select`).
  - Evidence: note each transition plus any dead end. The logic-level contract
    is already covered by `screen_manager_test`; this checks the physical input
    path.

### AC4 — OpenITG side-by-side feel test (PRD §12D)

- [ ] Owner-verified: on pad hardware, play the same chart back-to-back in
  Blaze 4k and OpenITG and compare:
  - hit-delta spread has no systematic early/late bias (calibrate the global
    offset first with the in-game wizard),
  - scroll speed and note spacing feel equivalent at matched mods,
  - judgment timing windows feel equally tight,
  - no perceptible audio/visual desync.
  - Evidence: written notes with the chart, mods, offset, and any discrepancies.
  - Related automated guard: `metronome_sync_test` holds sync within one
    judgment window over a full song.

## Per-OS audio backend guidance (PRD §14)

Audio backend selection affects input-to-sound latency, so calibrate on the
target OS and device. miniaudio picks a backend at runtime:

- **Windows**: WASAPI (shared/exclusive) is preferred; exclusive mode lowers
  latency but can block other apps. Prefer the default shared mode unless the
  owner needs the lowest latency.
- **macOS**: CoreAudio is the default and is generally low-latency without extra
  configuration.
- **Linux**: PipeWire/PulseAudio via the PulseAudio compatibility layer, or ALSA
  directly. ALSA has the lowest ceiling but bypasses the desktop mixer; the
  Pulse/PipeWire path is the safer default.

After switching OS, audio device, or backend, re-run the offset calibration
wizard — the saved `global_offset_seconds` is device/OS-specific.

## Sign-off

| Criterion | Owner | Date | Result |
| --------- | ----- | ---- | ------ |
| Automated Linux leg (AC1 Linux, AC3 logic, AC2 proxy) | repo | — | ✅ 34/34 |
| Windows build (AC1) | | | |
| macOS build (AC1) | | | |
| Real FPS over a full song (AC2) | | | |
| Pad loop traversal (AC3) | | | |
| OpenITG side-by-side feel (AC4) | | | |
