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

## Cabinet theme pass (#98)

The Cabinet theme (#89 to #97) restyled every screen. This pass checks the
polish work of #98: the `video.scanlines` toggle, the frame cost against the
pre-theme build, and a look at every screen. Linux is machine-checked here;
Windows and macOS are owner steps and are **not verified**.

### Linux (machine-checked on the verification host)

Host for this pass: Fedora, x86_64, GCC, Mesa 26.2.5 on an AMD Radeon
(radeonsi) with a 1920x1080 display, fixture pack on the stub clock
(`time source stub`, so no sound plays).

- **Scanlines toggle.** `video.scanlines` (default `true`, config file only, no
  options entry) is read by `read_video` in `src/data/config_loader.cpp`, saved
  by `save_config`, and applied by `title_art::draw_scanlines`, the one function
  every screen's scanlines call forwards to. `config_persistence_test` covers the
  default, the round trip and the wrong-type warning; `title_screen_test`
  (`test_scanlines_toggle`) checks through the headless recorder that the overlay
  quad is drawn when on and not drawn when off. In the real window, with
  `"video": {"scanlines": false}` in `config.json`, the title screen loses the
  horizontal line texture and the saved `config.json` keeps `"scanlines": false`
  (`screenshots/720p-10-title-scanlines-off.png` against
  `screenshots/720p-01-title.png`).
- **Frame cost against the pre-theme build.** Pre-theme build: commit `a2ec06c`
  (before the theme textures were loaded), built Release from a scratch worktree.
  Both binaries ran the same key script (Title, Select, Blaze Anthem BEGINNER 1 on
  the fixture pack, play to Results, back to Select) with
  `--perf-report --no-vsync`, four runs each, interleaved. The frame pacing of the
  game loop holds both builds at about 60 fps even with vsync off, so frame time
  shows pacing and hitches; CPU time (from `/proc/<pid>/stat`) and quads per frame
  show cost.

  | Measure (gameplay phase unless noted) | Pre-theme | Cabinet theme |
  | ------------------------------------- | --------- | ------------- |
  | `[perf]` median frame time, whole run | 16.65 ms in all 4 runs | 16.65 ms in all 4 runs |
  | `[perf]` p99, whole run (4 runs) | 17.27, 17.34, 17.36, 19.08 ms | 17.16, 17.29, 17.80, 18.55 ms |
  | `[perf]` max, whole run | 148 to 176 ms (the first frame, startup) | 250 ms in all 4 runs (the first frame; theme load and font bake) |
  | Quads drawn per gameplay frame | 295.5 | 34.8 |
  | Draw calls (flushes) per gameplay frame | 7.3 | 11.7 |
  | Select and Results quads per frame | 3268 and 1534 | 261 and 148 |
  | Process CPU during gameplay (about 5.9 s) | 0.25 to 0.29 s | 0.34 to 0.38 s |
  | Process CPU, whole run | 0.85 to 0.88 s | 1.63 to 2.11 s |

  Reading: the frame-time distribution is unchanged (p99 is within run-to-run
  noise; `hitches` counts frames over 16.67 ms, which the 60 fps pacing makes
  common in both builds: 307 to 328 before, 324 to 338 after), and the theme draws
  about 8 times fewer quads in gameplay (34.8 vs 295.5 quads/frame). It issues 11.7
  draw calls per frame (each component already batches its quads; the 11-12 calls
  correspond to distinct textures: background, scrim, receptors, taps, life frame,
  fill, stripes, judgment pop, difficulty badge, combo number, combo label, and
  text). Per owner decision on #98 (Finding H1), the gameplay criterion was
  reworded to: "no frame-time regression and fewer quads (34.8 vs 295.5 quads/frame)".
  Over a full 117-second real song (*Anubis* from *In The Groove*, 7,446 frames,
  silent audio with real hardware device callbacks), gameplay CPU was 7.93 s total
  (6.8% of one core) with a median frame time of 16.653 ms (p95 16.89 ms, p99 17.54 ms)
  and zero audio dropouts over 11,551 callbacks.
- **Screenshots** of each screen, in `docs/cabinet-theme/screenshots/`:
  Title, Select (packs closed, pack open), Options overlay, Calibration, Input
  Remap, Gameplay, and Results at **1280x720** (`720p-*.png`), **1920x1080**
  (`1080p-*.png`), and **2560x1440** (`1440p-*.png` captured via Xvfb on an
  isolated display `:98` without changing the host display mode), plus Attract
  at 1280x720 and 2560x1440, and Title with scanlines off at 1280x720 and 2560x1440.
  The layout scale is `theme::layout_scale` in `src/render/theme_layout.hpp`;
  `title_screen_test` pins the title layout at exactly 2x for 2560x1440.
- **`bitmap_font`.** Kept, and documented in `src/render/bitmap_font.hpp` as a
  fallback and debug text path. No themed screen draws player-facing text with
  it: text goes through `TextRenderer`. It is drawn only when a theme asset or a
  TrueType face is missing (the judgment pop label in
  `src/gameplay/judgment_animator.cpp`, the `TextRenderer` bitmap fallback in
  `src/render/ttf_font.cpp`) and by `SelectPlaceholderScreen`, which
  `src/main.cpp` does not register. `song_display_text.cpp` uses its
  `truncate_to_cells` and `font_covers_text` helpers.

### Owner checklist (Windows and macOS)

Mark each row "owner-verified" only after running it on the target hardware.

- [ ] **Windows (MSVC)** owner-verified: build with the `windows-msvc-release`
  preset, launch with a song pack, visit Title, Attract, Select (open a pack),
  the Options overlay, Calibration, Input Remap, Gameplay and Results at
  1280x720 and at 2560x1440; compare each against the Linux screenshots (colours,
  text crispness, banner and background alignment, no missing texture falling
  back to a flat colour; the log has no `[ThemeTextures]` or `[TextRenderer]`
  warning).
- [ ] **macOS (AppleClang)** owner-verified: the same, with the
  `macos-clang-release` preset; on a Retina display check the framebuffer size
  (the layout uses pixel size, not window points) and that text stays crisp.
- [ ] **Scanlines toggle** owner-verified on Windows and macOS: set
  `"video": {"scanlines": false}` in `config.json` next to the binary, restart,
  confirm the overlay is gone on every screen; set it back to `true`.
- [ ] **Frame cost** owner-verified on each OS: `--perf-report` over a full song
  (as in AC2), pasting the `[perf]` block with the machine and GPU.

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
- **Audio period (all OSes, #81)**: the engine requests a 480-frame period
  (10 ms @ 48 kHz) by default; the `[AudioEngine] Output device` log line shows
  the requested and negotiated period, and gameplay logs the measured device
  callback interval when a song ends. If playback crackles or drops out, raise
  `audio.period_size_frames` in `config.json` (for example to `960`), or set it
  to `0` for the backend default, then restart. Values other than `0` are
  clamped to 128–4096. Re-run the offset calibration after any change. Record
  the negotiated period per device in `docs/AUDIO_LATENCY.md` ("Negotiated
  period per device"); Windows (WASAPI) and macOS (CoreAudio) are
  **owner: not yet measured**.

After upgrading to the #81 music clock (smaller period + interpolated clock),
after changing `audio.period_size_frames`, or after switching OS, audio device,
or backend, re-run the offset calibration wizard — the saved `global_offset_seconds` is device/OS-specific. See
`docs/AUDIO_LATENCY.md` for why (Bluetooth adds latency the game clock cannot
see) and for the OBS recording sync workaround.

## Sign-off

| Criterion | Owner | Date | Result |
| --------- | ----- | ---- | ------ |
| Automated Linux leg (AC1 Linux, AC3 logic, AC2 proxy) | repo | — | ✅ 34/34 |
| Windows build (AC1) | | | |
| macOS build (AC1) | | | |
| Real FPS over a full song (AC2) | | | |
| Pad loop traversal (AC3) | | | |
| OpenITG side-by-side feel (AC4) | | | |
