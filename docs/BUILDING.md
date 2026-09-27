# Building Tundra Dance

Tundra Dance is a C++20 / CMake project. It fetches all third-party dependencies
with CMake `FetchContent` (SDL3, glad, nlohmann/json, miniaudio, stb), so a
fresh clone builds with nothing but a compiler, CMake, and git.

- **CMake**: ≥ 3.24
- **Language**: C++20
- **Renderer**: OpenGL 3.3 core (2D textured quads)
- **Audio**: miniaudio (SDL3 input, GL output)

## Prerequisites

### Linux

A recent C++20 compiler (GCC or Clang; the project is built and tested with a
current toolchain — the verification host used GCC 16.2.1), CMake, git, and the
SDL3 build dependencies.

Fedora / RHEL:

```bash
sudo dnf install cmake gcc-c++ git alsa-lib-devel \
  wayland-devel wayland-protocols-devel libxkbcommon-devel systemd-devel \
  libX11-devel libXext-devel libXrandr-devel \
  libXi-devel libXcursor-devel libXfixes-devel mesa-libGL-devel
```

Debian / Ubuntu:

```bash
sudo apt install build-essential cmake git libasound2-dev libpulse-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev libudev-dev \
  libx11-dev libxext-dev libxrandr-dev libxi-dev \
  libxcursor-dev libxfixes-dev libgl1-mesa-dev
```

miniaudio selects an audio backend at runtime (PulseAudio/PipeWire/ALSA). If no
audio device/backend is available, the game still runs: gameplay falls back to a
synthetic stub clock (headless/demo only) and logs a readable warning.

### Windows

- Visual Studio 2022 with the **Desktop development with C++** workload
  (includes CMake and Ninja).
- No extra SDL3 packages: `FetchContent` builds SDL3 from source.

### macOS

- Xcode Command Line Tools: `xcode-select --install`.
- No extra SDL3 packages: SDL3 uses the system frameworks.

## Building

From the repository root:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Debug build:

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

The result is a native, portable binary (`build/tundra-dance`). No installer is
required.

### CMake presets

`CMakePresets.json` encodes the per-OS configure/build/test commands:

| Preset | Host | Status |
| ------ | ---- | ------ |
| `linux-gcc-release`, `linux-gcc-debug` | Linux / GCC | **Verified here** |
| `linux-portability` | Linux / default compiler (compile_commands.json) | Owner diagnostic |
| `linux-clang-release` | Linux / Clang | Owner-run (Clang not on the verification host) |
| `windows-msvc-release`, `windows-msvc-debug` | Windows / MSVC (VS 2022, x64) | Owner-run |
| `macos-clang-release`, `macos-clang-debug` | macOS / AppleClang | Owner-run |
| `benchmark` | Any | RelWithDebInfo + `perf_loop_test` only |

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release            # expect 34/34

# CPU-budget benchmark only
cmake --preset benchmark && cmake --build --preset benchmark
ctest --preset benchmark
```

Presets reuse the local `build/_deps` dependency cache when it exists
(`FETCHCONTENT_BASE_DIR`), so repeat runs are offline and deterministic. On a
brand-new machine the first configure downloads the dependencies.

## Testing

```bash
ctest --test-dir build --output-on-failure   # expect 34/34
```

Notable tests:

- `frame_stats_test` — deterministic percentile/edge-case math (no timing).
- `perf_loop_test` — headless full-song CPU-budget benchmark; prints
  `p50/p95/p99/max` ms and gates `p99 < 16.67 ms`, `max < 50 ms`. Set
  `TUNDRA_PERF_STRICT=0` to make the budget assertions advisory.
- `screen_manager_test` — arcade-loop "no dead ends" contract.
- `metronome_sync_test` — permanent sync regression chart.

### Fresh-clone check

```bash
./scripts/fresh-clone-check.sh              # offline: reuse build/_deps, expect 34/34
./scripts/fresh-clone-check.sh --online     # force a real FetchContent fetch
./scripts/fresh-clone-check.sh --committed  # export committed HEAD only
```

The script exports a clean source tree (never `build/`), configures, builds, and
runs the full suite from that tree — the Linux answer to "clean CMake build from
a fresh clone on at least one OS" (PRD §11). It is the Linux leg and requires
GNU tar; the temp tree is removed on success unless `--keep` is passed.

## Running

Tundra Dance ships engine-only: import your own SM/SSC song packs.

```bash
# Headless smoke test
./build/tundra-dance --headless --smoke-test 120 --songs /path/to/your/Songs

# Play with a frame-time report (real hardware)
./build/tundra-dance --perf-report --songs /path/to/your/Songs
./build/tundra-dance --help
```

## Portable data layout

PRD §4/§9: all state is local, portable, and offline.

- A `data/` folder next to the binary holds `config.json`, `scores.json`, and
  the calibration click sample. On Linux, `--xdg` (or `TUNDRA_XDG=1`) uses the
  XDG data directory instead. `--data-dir <path>` overrides both for testing.
- `assets/` (fallback background, bundled judgment constants, UI sounds) is
  copied next to the binary at build time via a POST_BUILD step.
- No network, no accounts, no server. Settings, high scores, and the calibrated
  global offset survive restart.

## Performance

`--perf-report` prints one block on exit:

```
[perf] frames=... min=...ms median=...ms p95=...ms p99=...ms max=...ms mean=...ms hitches=... budget=16.667ms (PASS; first frame includes startup)
```

Percentiles use the **nearest-rank** rule (`index = ceil(p/100 * n) - 1`,
clamped). The verdict compares `p99` against `--perf-budget-ms` (default
`16.67 ms`, the 60 fps frame period). For a vsync-rate host, pass the observed
vsync period, e.g. `--perf-budget-ms 8.33` for 120 Hz. See
[`CROSS_PLATFORM_VERIFICATION.md`](CROSS_PLATFORM_VERIFICATION.md) for the
owner-verification procedure.

## Troubleshooting

- **`FetchContent` fails**: check network access, or pre-populate
  `build/_deps` and reuse it (the default preset/fresh-clone behavior).
- **No audio backend**: install ALSA/PulseAudio headers; the game degrades
  gracefully to the stub clock.
- **Headless CI-like runs**: pass `--headless` to skip window/GL/audio.
