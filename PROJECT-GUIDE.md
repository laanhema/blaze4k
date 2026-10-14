# Project guide

Blaze 4k is an offline 4-panel arrow rhythm game for Linux, Windows, and macOS, built as a faithful homage to In The Groove, Mungyodance, and StepMania. You hit scrolling arrows in time with music on a keyboard or a USB dance pad, scored with OpenITG dance points and judged against strict ITG timing windows.

The game reads StepMania `.sm` and `.ssc` simfiles alongside standard audio files (MP3, OGG, WAV) directly from your song folders. It outputs hardware-accelerated OpenGL graphics, synchronized audio streams, and local JSON files for settings and high scores. Players who grew up on arcade cabinets get the authentic Judge-4 timing feel without accounts, internet access, background video distractions, or theme scripting bloat. Gameplay time is locked to audio stream position, state stays local, and the scope centers strictly on 4-panel single-song play done right.

## Big picture

Most 4-panel players with years of simfile packs still run StepMania 5 or old OpenITG forks. Those engines carry decades of theme hacks, fragile Lua scripts, and timing quirks tied to render loops. Newer commercial rhythm games go the opposite direction, wrapping basic play in forced online logins, battle passes, and telemetry. Blaze 4k cuts all of that away. It is a single portable C++ binary: drop your song packs into `songs/`, launch the executable, and play.

A session moves through a clear loop from disk to pad to results:

1. Boot and library scan. `src/main.cpp` parses CLI flags and resolves directories via `src/data/data_paths.cpp`. `SongLibrary` (`src/chart/song_library.cpp`) scans song folders, reads `.sm` and `.ssc` files through `SimfileParser` (`src/chart/simfile_parser.cpp`), and indexes charts by foot rating.
2. Screen navigation. `ScreenManager` (`src/screens/screen_manager.cpp`) runs the screen state machine across Title, Attract, Select, Gameplay, and Results, plus Calibration and Input Remap.
3. Song wheel. In `SelectScreen` (`src/screens/select_screen.cpp`), players scroll banners, hear audio previews via `PreviewPlayer` (`src/audio/preview_player.cpp`), pick difficulty steps, and set speed mods (`src/gameplay/speed_mod.cpp`).
4. Audio-anchored playback. `AudioEngine` (`src/audio/audio_engine.cpp`) starts music decoding on `SoundStream` (`src/audio/sound_stream.cpp`). `MusicClock` (`src/timing/music_clock.cpp`) tracks hardware buffer playback, while `ClockInterpolator` (`src/timing/clock_interpolator.cpp`) anchors to audio callbacks and smoothly interpolates time between updates using a monotonic clock clamped to one device period.
5. Timestamped judgments. `InputManager` (`src/input/input_manager.cpp`) polls SDL3 for hardware timestamps down to the nanosecond. On tap, `JudgmentEngine` (`src/gameplay/judgment_engine.cpp`) compares the hit time against note rows and logs an immutable `JudgmentEvent` (`src/gameplay/judgment.hpp`). That log drives `ScoreKeeper` (`src/gameplay/score_keeper.cpp`) for dance points and percent, and `LifeKeeper` (`src/gameplay/life_keeper.cpp`) for life bar drain.
6. Results and records. `ResultsScreen` (`src/screens/results_screen.cpp`) reveals the letter or star grade, dance points, and judgment breakdown. If you set a personal best, `HighScoreManager` (`src/data/high_scores.cpp`) writes the record into `scores.json` keyed by a content hash of the chart.

Audio streaming, input polling, simfile parsing, and rendering run on independent tracks with zero shared mutable global state. When no physical audio device is detected or the game runs in headless testing mode, `src/audio/audio_engine.cpp` falls back to `AudioBackend::Null` with a synthetic monotonic clock. When chart background art is missing, the renderer defaults to `assets/fallback_bg.png`.

## Codebase map

The codebase is organized into modular subsystems with strict architectural boundaries. Key files and directories:

- `src/main.cpp` parses CLI flags, wires subsystem dependencies into the application instance, and manages the process lifecycle without global singletons.
- `src/app/` handles the SDL3 window, OpenGL 3.3 context creation, fixed-timestep simulation ticks, vsync pacing, and frame time metrics.
- `src/screens/` holds the arcade screen state machine and all menu, song wheel, gameplay, results, and options views.
- `src/timing/` implements the music clock, monotonic interpolator, OpenITG timing windows, and offset calibration math.
- `src/input/` wraps SDL3 device polling to capture nanosecond event timestamps and map keyboard keys or pad buttons to virtual columns.
- `src/chart/` parses `.sm` and `.ssc` files, computes tempo changes and stops, and indexes song packs with defensive allocation limits.
- `src/audio/` wraps miniaudio for multi-threaded music streaming, device-callback clock anchoring, sound effects, assist ticks, and select previews.
- `src/render/` batches 2D quads in OpenGL 3.3, caches textures, builds font atlases via stb_truetype, and renders the cabinet UI theme.
- `src/gameplay/` runs note field layout, ITG hit windows, combo counts, life bar draining, and OpenITG dance-point scoring.
- `src/data/` loads and saves JSON configs and high scores, resolving local and XDG paths across platforms.
- `src/util/parse_number.hpp` converts strings to numbers with strict overflow checks, keeping parsing safe without C++ exceptions.
- `assets/` bundles default noteskins, UI fonts, cabinet textures, fallback backgrounds, and verified OpenITG judgment constants.
- `tests/` contains parser fuzzing harnesses, timing benchmarks, audio null-device unit checks, and the metronome sync chart run by ctest.
- `scripts/` holds developer scripts for repository linting, code formatting, symbol font extraction, and clean-clone builds.

## Tech stack and design decisions

Blaze 4k is a single native desktop executable with no runtime virtual machine, no garbage collector, and no web view shell. In a rhythm game where a 16-millisecond window separates Fantastic from Excellent, engine stutter or garbage collection pauses ruin the experience. Building directly on C++20, SDL3, and OpenGL keeps frame pacing locked to vsync and input handling sub-millisecond.

### Framework and runtime

The engine is standard C++20, compiling cleanly under GCC, Clang, and MSVC with zero warnings. C++20 gives us `std::span` and `std::string_view` for parsing simfiles without heap churn, plus deterministic RAII lifetimes for OpenGL textures and audio streams. CMake (minimum 3.24) orchestrates the build. It pulls pinned third-party dependencies through `FetchContent`, caching downloaded sources in `build/_deps` so repeat builds stay offline and predictable.

### Runtime dependencies

The project relies on 5 runtime packages:

- `SDL3` handles window creation, OpenGL contexts, and raw input events. Its event system timestamps controller inputs at driver poll time with nanosecond precision, which removes polling jitter entirely. SDL3 also handles gamepad and dance pad hotplugging out of the box.
- `glad` loads OpenGL 3.3 Core profile function pointers. Because Blaze 4k renders purely 2D textured quads, we need direct modern OpenGL bindings without the overhead or architectural bloat of a 3D engine like bgfx.
- `nlohmann_json` serializes configuration options, high scores, and timing constant tables. Player scores and offset settings belong in human-readable, portable files you can inspect with a text editor, not an embedded SQLite database.
- `miniaudio` decodes audio and streams playback across ALSA, PulseAudio, PipeWire, WASAPI, and CoreAudio. We rejected SDL_audio because miniaudio provides direct control over device buffer periods, sample offsets, and callback timestamps required for sub-millisecond clock anchoring. Being single-header also means zero dynamic library headaches.
- `stb` supplies `stb_truetype` for rasterizing glyph atlases and `stb_image` for loading PNG and JPEG graphics. The game needs crisp arcade numbers, difficulty badges, and song banners. Pulling in heavy dependencies like FreeType and HarfBuzz for 2D bitmap text would be massive overkill.

### Music-driven clock and timing precision

In rhythm games, driving note scrolling from a frame delta (`dt`) or wall clock creates erratic jitter whenever the GPU drops a frame. In Blaze 4k, gameplay time is defined strictly by audio stream position: current playback samples divided by sample rate, plus a calibrated global offset.

Because audio hardware buffers update in discrete chunks (such as 480 frames at 48 kHz, or 10 ms), raw stream queries advance in stair-steps. `ClockInterpolator` bridges those steps using `std::chrono::steady_clock`. It re-anchors on every audio update, guarantees time never moves backward, and caps interpolation to one device period ahead of the latest anchor. If audio hardware is absent or the engine runs headless, `AudioEngine` switches to `AudioBackend::Null` with a synthetic monotonic clock so automated test suites run silently and deterministically.

### Event-sourced judgments and state isolation

Every hit or miss emits an immutable `JudgmentEvent` record containing the column, note timestamp, hit timestamp, delta in milliseconds, and the assigned window (Fantastic, Excellent, Great, Decent, Way Off, Miss, or Mine). Nothing in the judgment engine modifies player score or life directly. Instead, `ScoreKeeper` and `LifeKeeper` consume this event log.

This decoupling keeps gameplay logic pure and testable. A unit test feeds simulated hit timestamps into the engine and asserts the exact event log without spinning up graphics or audio. It also means replays, accuracy scatter plots, and hit histograms can be added later without touching core judgment code.

### Untrusted simfile parser hardening

Community `.sm` and `.ssc` files come from dozens of third-party tools and date back over twenty years. Many contain syntax oddities, split timing, unclosed tags, or massive measure counts. Blaze 4k treats simfiles as untrusted data.

The parser sets hard defensive limits before allocating: 16 MB maximum file size, 10,000 tags, 1,000 parameters per tag, and 1 MB per parameter. Individual charts cannot exceed 10,000 measures, 1,024 rows per measure, or 100,000 notes. Any chart exceeding these bounds logs a readable error and is skipped rather than crashing the game. All numeric conversions go through `src/util/parse_number.hpp`, checking bounds and preventing locale-dependent decimal parsing bugs.

## Development and testing

### Scripts

```bash
scripts/check.sh                              # Run lint, format check, strict compilation, and tests
scripts/lint.sh                               # Verify repository architectural rules and boundaries
scripts/format.sh --check                     # Check source formatting against .clang-format
cmake --preset linux-gcc-release              # Configure release build using Linux GCC preset
cmake --build --preset linux-gcc-release      # Compile optimized release binary
ctest --preset linux-gcc-release              # Execute the test suite against release build
cmake --preset asan-ubsan                     # Configure debug build with ASan and UBSan
cmake --build --preset asan-ubsan             # Compile instrumented sanitizer binaries
ctest --preset asan-ubsan                     # Run test suite checking for memory or UB defects
./scripts/fresh-clone-check.sh                # Test clean build and test execution from scratch
./build/blaze-4k --songs ./songs              # Launch game scanning local song directory
./build/blaze-4k --headless --smoke-test 120  # Run headless 120-frame smoke test for validation
```

### Docker deployment

Blaze 4k is built for desktop play on bare metal and does not run as a containerized service. When container builds run in continuous integration, standard Ubuntu or Debian images provide GCC, CMake, and the X11, Wayland, and ALSA development headers. The binary runs unprivileged with direct access to user input devices in `/dev/input` and host audio interfaces. CMake post-build commands copy bundled assets beside the binary, making the build folder fully portable without system-wide installation.

## Security and deployment

Because Blaze 4k is an offline desktop game, its security model focuses on untrusted local files:

- Simfile parser hardening. The only external input surface is parsing user-provided `.sm` and `.ssc` files. Strict limits on file size, tag counts, and chart rows prevent heap exhaustion and buffer overflows. Checked number parsing through `src/util/parse_number.hpp` eliminates format injection.
- Zero network exposure. The binary contains no HTTP client, no listening sockets, no telemetry, and no account sync. Remote exploitation, SSRF, and credential theft are completely out of scope.
- Local filesystem bounds. Configuration and score updates write only to `config.json` and `scores.json`. The game supports the XDG base directory specification via `--xdg` or `BLAZE4K_XDG=1`, keeping user files inside standard desktop data paths.
- Memory safety sanitizers. CI compiles the full test suite with AddressSanitizer and UndefinedBehaviorSanitizer using the `asan-ubsan` preset. Canary tests in `tests/sanitizer_canary.cpp` ensure that memory violations or float cast overflows terminate immediately instead of silently corrupting memory.
