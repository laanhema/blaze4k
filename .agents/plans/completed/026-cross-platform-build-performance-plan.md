# Plan: Cross-Platform Build Verification and Performance Pass (D4)

## Summary

Phase D's final gate behind issue #26 is a **verification** task, not a feature: prove the v1
CMake build is portable, measure the loop for hitches, and close the PRD §11 success criteria.
This host is **Linux/Fedora only** and has exactly one compiler installed (GCC 16.2.1); there is
no MSVC, no Clang, no MinGW, no macOS SDK, and no dance-pad hardware, so the Windows/macOS native
builds and the side-by-side OpenITG feel test **cannot be executed here**. This plan therefore
splits issue #26's acceptance criteria into (a) an **automatable Linux leg** — a reproducible
fresh-clone configure/build/ctest proof, a CMake preset matrix that encodes the Windows/macOS
build commands for the owner to run, a static portability audit, and a **headless CPU-budget +
full-song loop benchmark** — and (b) a **documented owner-verification checklist** for the GPU
frame-rate, native Windows/macOS builds, and pad/feel criteria that are physically impossible to
automate on this host. No gameplay/timing/scoring semantics change; the only production code
touched is presentation instrumentation (a pure `FrameStats` collector plus an opt-in
`--perf-report` that records already-computed frame periods).

## User Story

As the developer
I want verified builds on Windows, macOS, and Linux plus a performance pass, so that v1 ships
stable at vsync rate on a mid-range machine from a fresh clone
So that v1 ships stable at vsync rate on a mid-range machine from a fresh clone.

## Metadata

| Field | Value |
|-------|-------|
| Type | Technical (verification + docs + measurement tooling) |
| Complexity | MEDIUM (no gameplay logic; one pure collector, one benchmark, presets, scripts, docs) |
| Systems Affected | `src/app/` (pure `FrameStats` + App instrumentation), `src/main.cpp` (flag), `tests/`, `CMakePresets.json`, `scripts/`, `docs/`, `README.md` |
| GitHub Issue | #26 ([D4]) |
| PRD refs | §11 Success criteria, §12 Phase D ("Cross-platform builds verified; performance pass"), §4 Deployment (portable folder), §14 (per-OS audio backend guidance), §9 (portable `data/`) |
| Depends on | C3/C5/C7, D1/D2/D3 — all merged (#20–#25) |
| Blocks | — (final D item) |

---

## Scope

**In scope:** a pure frame-time collector, an opt-in real-hardware `--perf-report`, a headless
full-song CPU-budget benchmark, a CMake preset matrix, a fresh-clone verification script, a static
portability audit, and per-OS build + owner-verification documentation. Fixing the stale README
status line is included.

**Out of scope (contract, PRD §13):** any change to judgment windows, DP weights, grades, life,
timing, persistence schemas, rendering behavior, or gameplay logic. No new CI (AGENTS.md: "no CI
for v1"). No bundled soundtrack, installer, or distribution decision. No `.github/workflows`.

**Explicitly not automatable on this host (owner-manual, documented not faked):**
- Native Windows (MSVC) and macOS (Clang) builds → docs + presets only.
- Real vsync/GPU frame rate on a mid-range machine → instrumentation + owner procedure.
- Physical full-arcade-loop traversal on dance-pad hardware → logic-level loop test here + owner checklist.
- Side-by-side OpenITG feel test (PRD §12D validation) → owner checklist only.

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| OS | Fedora Linux 44, x86_64 | `/etc/os-release`; `uname -m` = `x86_64` |
| Cores | 16 | `nproc` → 16; `-j16` safe |
| CMake | 4.4.3 | `build/` already host-configured (`CMAKE_BUILD_TYPE=Release`, `/usr/bin/c++`) |
| Compiler (only one) | **GCC 16.2.1** (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic`, no `-Werror` (root `CMakeLists.txt:10-14`) |
| Clang / MSVC / MinGW / osxcross | **absent** | `clang++`, `x86_64-w64-mingw32-g++`, `o64-clang++` all not found → no cross/native verification possible |
| Ninja | absent | Presets use CMake's default generator (Unix Makefiles) on Linux |
| Audio dev headers | ALSA header + libpulse absent | miniaudio device init fails gracefully; `GameplayView` falls back to the **synthetic stub clock** (`gameplay_view.cpp:58-66`) → headless full-song loop is deterministic |
| Network | GitHub reachable | `git ls-remote` for SDL succeeded → a true online fresh-clone FetchContent is possible but slow |
| CI | none (`.github/` absent) | Per AGENTS.md "no CI for v1"; do not add workflows |
| `docs/` | **does not exist** | This plan creates it |
| Baseline tests | **32/32 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 32" (0.41 s), run this session |
| `build/_deps` | populated | `sdl3-src`, `glad-src`, `nlohmann_json-src`, `miniaudio-src`, `stb-src` present → fresh-clone script can reuse this as an offline dep cache |

**Start green, stay green:** 32 tests pass; this plan adds **2** targets (`frame_stats_test`,
`perf_loop_test`) and extends `app_test` + `screen_manager_test` in place → **34 expected**. No
changes to `src/gameplay/*`, `src/timing/*`, `src/chart/*`, `src/audio/*`, `src/render/*`,
`src/data/*`, `src/input/*`, `src/screens/*`.

---

## Pinned Semantics

Authority: **issue #26 AC1–AC4**, **PRD §11/§12D/§14**, the existing **App fixed-timestep loop**
(`app.cpp:58-91`), and the existing **`GameplayView`** stub-clock contract
(`gameplay_view.cpp:58-66`, `:90-103`).

### What each AC can be verified as, on this host

| Issue AC | Automatable on Linux? | In-repo deliverable | Owner-manual step |
|----------|----------------------|---------------------|-------------------|
| AC1 fresh clone → native binaries (Win/macOS/Linux) | **Linux: yes.** Win/macOS: no | `scripts/fresh-clone-check.sh` (Linux), `CMakePresets.json` (Win/macOS/Clang templates), `docs/BUILDING.md` | Run `windows-msvc`/`macos-clang` presets on those hosts |
| AC2 stable 60 fps, no hitches | **Partial:** headless CPU-budget proxy only (no GPU/vsync) | `FrameStats` + `--perf-report`, `tests/perf_loop_test.cpp` | Run `--perf-report` on real GPU over a full song; compare p99/max to vsync period |
| AC3 arcade loop no dead ends | **Logic-level: yes.** Physical pad: no | `screen_manager_test` arcade-loop contract case + existing screen tests | Traverse loop on pad hardware |
| AC4 OpenITG side-by-side feel | **No** | `docs/CROSS_PLATFORM_VERIFICATION.md` checklist | Pad hardware feel test (PRD §12D) |

### Frame-time collector (`FrameStats`) — the only shared primitive

Pure, header-only, wall-clock-free (it stores supplied samples, computes statistics). Fed by
`App`'s already-computed `frame_dt` (presentation measurement only — never the gameplay/judgment
path, which remains the `MusicClock`) and by the headless benchmark.

```cpp
// src/app/frame_stats.hpp — <vector>/<algorithm>/<cmath>/<cstddef>/<cstdint> only; no SDL/GL/audio.
namespace blaze4k {

// Descriptive per-frame timing samples in milliseconds. Sampling and statistics only;
// it never generates time and is never consulted by the judgment path.
class FrameStats {
public:
    void reset();
    void add(double milliseconds);        // ignores non-finite and negative samples
    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] bool empty() const;     // count() == 0
    [[nodiscard]] double total_ms() const;
    [[nodiscard]] double min_ms() const;  // 0 when empty
    [[nodiscard]] double mean_ms() const; // 0 when empty
    [[nodiscard]] double median_ms() const;
    [[nodiscard]] double max_ms() const;
    [[nodiscard]] double percentile_ms(double p) const; // p clamped to [0,100]; nearest-rank
    [[nodiscard]] std::size_t over_budget(double budget_ms) const; // samples > budget_ms

private:
    std::vector<double> samples_ms_;
};
}
```

Percentile definition is pinned (nearest-rank, `index = ceil(p/100 * n) - 1`, clamped): a
deterministic, testable rule; the exact method is documented in the test.

### App instrumentation contract

- `AppConfig` gains `bool perf_report = false;` and `double perf_budget_ms = 1000.0/60.0;`.
- `App` gains a `FrameStats frame_stats_`, records one sample per rendered frame using the
  `frame_dt` it already computes (`app.cpp:64-67`), and exposes
  `[[nodiscard]] const FrameStats& frame_stats() const`.
- At loop end, if `perf_report`, print one block: `frames`, `min/median/p95/p99/max ms`,
  `mean ms`, `over-budget (hitch) count`, and a PASS/FAIL verdict against `perf_budget_ms`.
  The first frame includes startup and is included as-is (documented in the report line).
- `--perf-report` enables it; `--perf-budget-ms <ms>` overrides the budget (default 16.67).

### Headless benchmark contract (`tests/perf_loop_test.cpp`)

- Fixture: `tests/fixtures/reference_pack/Blaze Pack/Aurora Borealis/Aurora Borealis.sm`
  (resolved via `BLAZE4K_SOURCE_DIR` compile definition, mirroring `background_test`'s
  `BLAZE4K_ASSETS_DIR`).
- Parse → first 4-panel chart → `GameplayView::init(..., fail_enabled=false)` (stub clock in
  headless) → run `fixed_dt` steps until `outcome() != InProgress`, timing `update`+`handle_input`
  (render is a no-op into an uninitialized `GlQuadRenderer`, so the measured cost is the
  logic/note-field/judgment CPU path).
- Also times the real arcade `ScreenManager` update path (Title/Attract/Select) for N frames.
- Asserts **correctness** unconditionally: the full chart reaches `Cleared`, no crash, samples are
  finite, `count > 0`. Asserts a **generous CPU budget** (`p99 < 16.67 ms`, `max < 50 ms`) unless
  `BLAZE4K_PERF_STRICT=0` is set, and always prints the percentiles for the owner. This is a
  *necessary* condition for AC2, not a GPU FPS proof — stated in the test output.

### Fresh-clone verification contract (`scripts/fresh-clone-check.sh`)

- `set -euo pipefail`; create a temp dir, `git archive HEAD | tar -x -C "$tmp"` (or
  `git clone --depth 1 . "$tmp"`) so the tree is clean and excludes `build/`.
- Configure with `-DFETCHCONTENT_BASE_DIR=<repo>/build/_deps` by default (offline, deterministic,
  reuses the populated cache) and `-DCMAKE_BUILD_TYPE=Release`; `--online` drops the cache reuse
  to exercise a real FetchContent download. Build `-j"$(nproc)"`, then `ctest --output-on-failure`.
- Fail loudly with the captured log path if configure/build/test fails; assert **34/34**.
- This is the Linux answer to AC1 and PRD §11 "clean CMake build from a fresh clone on at least
  one OS".

### CMake preset matrix (`CMakePresets.json`)

- `linux-gcc-release` / `linux-gcc-debug` (Unix Makefiles, `CMAKE_BUILD_TYPE`) — **runnable here**.
- `linux-clang-release` (Clang if installed; owner/environment-dependent).
- `windows-msvc-release` / `-debug` (Visual Studio generator, `-A x64`, multi-config) — owner-run.
- `macos-clang-release` / `-debug` (Unix Makefiles, AppleClang) — owner-run.
- `benchmark` build preset (`RelWithDebInfo`) and a `benchmark` test/run preset that invokes
  `perf_loop_test`; plus `test` presets.
- Presets only encode commands; **no preset is presented as verified here except the Linux/GCC
  ones actually run in Task 9**.

### Static portability audit

The tree already isolates platform code: `data_paths.cpp` uses `SDL_GetBasePath` +
`std::filesystem` (no POSIX paths); `config_loader.cpp:10-14` and `high_scores.cpp:14-18` guard
`<unistd.h>`/`<process.h>` behind `_WIN32`; `window.cpp:55-64` guards the Linux display auto-detect.
The audit task re-confirms these and records them as the portability evidence; **only genuine
portability bugs are fixed** (none expected). No new global warning flags (see OQ4).

---

## Value Provenance

| Value | Source | Status |
|-------|--------|--------|
| Frame budget 16.67 ms (60 fps) | PRD §11 "Stable 60 fps (or vsync rate)"; `AppConfig::fixed_dt = 1/60` (`app.hpp:13`) | Sourced (PRD/config) |
| `--perf-report` budget default | Derived from the 60 fps target; override flag for vsync-rate hosts | Sourced (derived) |
| Percentile rule (nearest-rank) | Not authoritative — a documented, deterministic definition for the collector | **Design decision — OQ1** |
| Generous gate (p99 16.67 / max 50 ms) on a shared host | Issue silent; avoids CI-style flakiness | **Design decision — OQ2** |
| Preset naming/contents | Issue silent; mirrors CMake convention | **Design decision — OQ3** |
| Fresh-clone default = offline dep-cache reuse | Issue/PRD silent; determinism + no network dependency | **Design decision — OQ5** |

No judgment windows, DP weights, grade boundaries, life deltas, timing values, or persisted fields
are introduced or changed.

---

## Patterns to Follow

### Existing frame-period computation to feed `FrameStats` (presentation only)
```cpp
// SOURCE: src/app/app.cpp:58-91
uint64_t current_time = SDL_GetPerformanceCounter();
double frame_dt = static_cast<double>(current_time - last_time_) /
                  static_cast<double>(perf_frequency_);
last_time_ = current_time;
frame_dt = std::min(frame_dt, config_.max_frame_dt);
```

### Pure, clock-free, headless-safe model (mirror for `FrameStats`)
```cpp
// SOURCE: src/screens/results_anim.hpp:88-134 (pure model) ; src/gameplay/judgment_animator.hpp
// SOURCE: src/audio/preview_player.cpp:30-34 (fixed-dt presentation timer, never a gameplay clock)
```

### Stub-clock full-song loop (headless benchmark)
```cpp
// SOURCE: src/gameplay/gameplay_view.cpp:58-66, 90-103, 239-247
if (!audio_started_) { use_stub_ = true; /* synthetic PCM advanced by update(fixed_dt) */ }
GameplayOutcome outcome() const;   // InProgress / Cleared / Failed
```

### CLI flag + help wiring
```cpp
// SOURCE: src/main.cpp:76-142 (arg loop), :34-50 (print_help)
} else if (arg == "--no-vsync") { config.window.vsync = false; }
```

### Test idiom + fixture path + registration
```cpp
// SOURCE: tests/background_test.cpp (BLAZE4K_ASSETS_DIR define) ; tests/CMakeLists.txt:236-250
target_compile_definitions(perf_loop_test PRIVATE
    BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_executable(perf_loop_test perf_loop_test.cpp)
target_link_libraries(perf_loop_test PRIVATE blaze4k_core)
add_test(NAME perf_loop_test COMMAND perf_loop_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/app/frame_stats.hpp` | CREATE | Pure `FrameStats` collector (no SDL/GL/audio) |
| `tests/frame_stats_test.cpp` | CREATE | Deterministic percentile/edge-case tests |
| `src/app/app.hpp` | UPDATE | `perf_report`/`perf_budget_ms` config, `FrameStats` member + accessor |
| `src/app/app.cpp` | UPDATE | Record per-frame `frame_dt`; print report at loop end |
| `src/main.cpp` | UPDATE | `--perf-report`, `--perf-budget-ms`; help text |
| `tests/perf_loop_test.cpp` | CREATE | Headless full-song CPU-budget + screen-loop benchmark |
| `tests/screen_manager_test.cpp` | UPDATE | Arcade-loop "no dead ends" contract case |
| `tests/app_test.cpp` | UPDATE | Assert `frame_stats().count()` after a perf-report smoke run |
| `tests/CMakeLists.txt` | UPDATE | Register `frame_stats_test`, `perf_loop_test`; fixture-path define |
| `CMakePresets.json` | CREATE | Cross-platform build/test matrix (Linux/GCC runnable; Win/mac templates) |
| `scripts/fresh-clone-check.sh` | CREATE | Fresh-clone configure/build/ctest proof |
| `docs/BUILDING.md` | CREATE | Per-OS build instructions, dependency notes, portable layout |
| `docs/CROSS_PLATFORM_VERIFICATION.md` | CREATE | Owner checklist: native builds, FPS report, pad loop, OpenITG feel |
| `README.md` | UPDATE | Fix stale "pre-implementation" status; link build/verification docs |

Not modified: `src/gameplay/*`, `src/timing/*`, `src/chart/*`, `src/audio/*`, `src/render/*`,
`src/data/*`, `src/input/*`, `src/screens/*`, `src/app/window.*`, root `CMakeLists.txt`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure `FrameStats` collector

- **File**: `src/app/frame_stats.hpp`
- **Action**: CREATE
- **Implement**: Per **Pinned Semantics**. Header-only, `[[nodiscard]]` queries, `add` ignores
  non-finite/negative. `percentile_ms` clamps `p` to `[0,100]` and uses nearest-rank on a sorted
  copy so `add` order never matters. Empty queries return `0.0`. Keep it free of SDL/GL/audio/
  timers.
- **Mirror**: `src/screens/results_anim.hpp:88-134` (pure model).
- **Validate**: `cmake --build build -j16` (after Task 6 registers nothing new; header compiles via
  Task 2/3).

### Task 2: `frame_stats_test` (deterministic math)

- **File**: `tests/frame_stats_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK`; no timing, no SDL):
  1. empty → `count()==0`, `empty()`, all queries `0`.
  2. `add(1..100)` → `count()==100`, `min()==1`, `max()==100`, `mean()==50.5`,
     `median_ms()==50.0` (nearest-rank lower), `percentile_ms(95)==95`, `percentile_ms(99)==99`.
  3. `add` ignores `NaN`, `+inf`, `-1.0` → count unchanged.
  4. `percentile_ms(-5)`/ `percentile_ms(150)` clamp to min/max.
  5. `over_budget(b)` counts strictly greater samples; `reset()` clears.
- **Mirror**: `tests/results_anim_test.cpp` (invariant style).
- **Validate**: `./build/tests/frame_stats_test` → exit 0.

### Task 3: App instrumentation + CLI flag

- **File**: `src/app/app.hpp`, `src/app/app.cpp`, `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - `app.hpp`: `#include "app/frame_stats.hpp"`; add `bool perf_report`, `double perf_budget_ms`
    to `AppConfig`; `FrameStats frame_stats_;` + `[[nodiscard]] const FrameStats& frame_stats() const`.
  - `app.cpp`: `frame_stats_.add(frame_dt * 1000.0);` after the accumulator clamp; after the loop
    (before returning), if `perf_report` print `frames`, `min/median/p95/p99/max`, `mean`,
    `over-budget (> budget ms)` count and a PASS/FAIL verdict. `reset()` in `init()`.
  - `main.cpp`: parse `--perf-report` (sets `config.perf_report = true`) and
    `--perf-budget-ms <ms>` (finite, `>0`, else warn + keep default); add both to `print_help()`.
- **Mirror**: `src/main.cpp:76-142` flag loop; `src/app/app.cpp:64-67`.
- **Validate**: `cmake --build build -j16`.

### Task 4: `app_test` perf-report case

- **File**: `tests/app_test.cpp`
- **Action**: UPDATE
- **Implement**: After the existing smoke block, run a headless `App` with `perf_report=true`,
  `smoke_test_frames=60`; assert `!app.frame_stats().empty()` and
  `app.frame_stats().count() == 60` after `run()`; assert `mean_ms()` is finite. Keep all existing
  assertions unchanged.
- **Mirror**: `tests/app_test.cpp:81-104`.
- **Validate**: `./build/tests/app_test` → exit 0.

### Task 5: Headless full-song benchmark

- **File**: `tests/perf_loop_test.cpp`
- **Action**: CREATE
- **Implement**: Per **Headless benchmark contract**. Resolve the fixture via `BLAZE4K_SOURCE_DIR`
  (fall back to `CMAKE_SOURCE_DIR` only if needed); parse, take `charts().front()`, init
  `GameplayView` with `fail_enabled=false`. Loop `fixed_dt = 1.0/60.0` until
  `outcome() != InProgress` or a safety cap (e.g. `last_note_time + 5s` in steps); time each
  `update`+`handle_input_events` with `std::chrono::steady_clock` into a `FrameStats`. Render into
  an uninitialized `GlQuadRenderer` (no-op). Then time N (`2000`) real arcade-screen updates
  (Title/Attract/SelectPlaceholder via `ScreenManager`). Print both percentile blocks; assert the
  chart `Cleared`, samples finite, and the budget gate unless `BLAZE4K_PERF_STRICT=0`.
- **Mirror**: `tests/judgment_engine_test.cpp` (parse→play), `tests/score_keeper_test.cpp:663-704`
  (full-chart perfect-play harness), `src/app/app.cpp:58-91` (loop shape).
- **Validate**: `./build/tests/perf_loop_test` → exit 0; prints percentiles.

### Task 6: Register tests

- **File**: `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: Append `frame_stats_test` and `perf_loop_test` blocks mirroring
  `tests/CMakeLists.txt:236-250`; give `perf_loop_test` the `BLAZE4K_SOURCE_DIR` compile definition.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 7: Arcade-loop no-dead-ends contract

- **File**: `tests/screen_manager_test.cpp`
- **Action**: UPDATE
- **Implement**: Add `test_arcade_loop_no_dead_ends()`: boot at `Title`; assert `Confirm` → `Select`;
  `Select` confirm-handoff → `Gameplay` (reuse the existing real-screen/spy wiring already in the
  file); a finished gameplay result → `Results`; `Confirm`/`Back` → `Select`; assert every canonical
  screen (`Title`, `Attract`, `Select`, `Gameplay`, `Results`) is registered and has at least one
  exit edge (no dead end). Keep existing cases green.
- **Mirror**: `tests/select_screen_test.cpp:241-255`, `tests/results_screen_test.cpp` (Confirm/Back),
  `screen_manager_test.cpp:340-366`.
- **Validate**: `./build/tests/screen_manager_test` → exit 0.

### Task 8: Cross-platform preset matrix

- **File**: `CMakePresets.json`
- **Action**: CREATE
- **Implement**: Per **CMake preset matrix**; `"version": 6`; `cmakeMinimumRequired` 3.24; hidden
  `base` configure preset (`binaryDir: ${sourceDir}/build/${presetName}`); Linux GCC/Clang, Windows
  MSVC, macOS Clang configure presets; matching `buildPresets` (with `configuration` for the
  multi-config MSVC presets); `testPresets` using `--output-on-failure`. Add `linux-portability`
  (Clang/GCC) as an owner-diagnostic preset. Keep each preset minimal and commented by name.
- **Mirror**: root `CMakeLists.txt:1-3` (minimum version), AGENTS.md commands.
- **Validate**: `cmake --preset linux-gcc-release` then `cmake --build --preset linux-gcc-release`
  then `ctest --preset linux-gcc-release` (expect 34/34).

### Task 9: Fresh-clone verification script + full run

- **File**: `scripts/fresh-clone-check.sh`
- **Action**: CREATE
- **Implement**: Per **Fresh-clone verification contract**; `chmod +x`. Default offline dep-cache
  reuse (`FETCHCONTENT_BASE_DIR`); `--online` to force fetch; `--build-dir <path>`; clear
  pass/fail output and log path. Run it this session and record the result.
- **Mirror**: AGENTS.md "Build commands" + `build/_deps` layout.
- **Validate**: `./scripts/fresh-clone-check.sh` → "34/34 tests passed" and exit 0.

### Task 10: Documentation + README

- **File**: `docs/BUILDING.md`, `docs/CROSS_PLATFORM_VERIFICATION.md`, `README.md`
- **Action**: CREATE / CREATE / UPDATE
- **Implement**:
  - `docs/BUILDING.md`: prerequisites per OS (MSVC/Clang/GCC, SDL build deps incl. Linux
    ALSA/Wayland + macOS frameworks note), the exact AGENTS.md commands, preset usage, portable
    `data/` layout (PRD §4/§9), and the fresh-clone script.
  - `docs/CROSS_PLATFORM_VERIFICATION.md`: the **owner checklist** mapping each issue AC to a
    concrete step and evidence — native builds on Windows/macOS, `--perf-report` over a full song
    (record p99/max vs vsync period), full pad-traversal of the loop, side-by-side OpenITG feel
    test, and the per-OS audio-backend latency note (PRD §14). Each row states "owner-verified" and
    what artifact to capture. Clearly marks which ACs the Linux run already covers.
  - `README.md`: replace the stale "Status: pre-implementation." with the post-D4 status; add links
    to the two docs; keep the building section (optionally note presets).
- **Mirror**: README.md:31-44, `.agents/PRDs/PRD.md:250-262`.
- **Validate**: `rg -n "pre-implementation" README.md` → no matches; links resolve to existing files.

### Task 11: Full suite + warning budget

- **Action**: VERIFY
- **Implement**: configure/build, run everything, scan for new warnings, run the scope grep.
- **Validate**: see **Validation** below (`ctest` → **34/34**, no new warnings).

---

## Validation

```bash
# Configure (tests/CMake changed) and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j16

# Tests (expect 34/34: 32 existing + frame_stats_test + perf_loop_test)
ctest --test-dir build --output-on-failure

# Explicit new/updated tests
./build/tests/frame_stats_test
./build/tests/perf_loop_test
./build/tests/app_test
./build/tests/screen_manager_test

# Purity: FrameStats must stay free of SDL/GL/audio/wall-clock
rg -n "SDL_|glad|miniaudio|chrono|GetTicks|std::time|<ctime>" src/app/frame_stats.hpp
# expected: no matches

# Warning budget (no new -Wswitch / unused-parameter / conversion warnings)
cmake --build build -j16 2>&1 | rg -i "warning" ; # expected: none

# Fresh-clone proof (Linux leg of AC1)
./scripts/fresh-clone-check.sh        # expect "34/34 tests passed"

# Preset proof (Linux leg of AC1)
cmake --preset linux-gcc-release && cmake --build --preset linux-gcc-release \
  && ctest --preset linux-gcc-release

# Scope: no gameplay/scoring/timing/render/screens edits
git diff --name-only | rg "gameplay/|timing/|chart/|audio/|render/|data/|input/|screens/|window\." ; # expected: none

# Diagnostics (owner): FrameStats is the only timing source, results remain identical to C7
```

## End-to-End Verification

All automated steps are headless/non-blocking; use `--data-dir` so the developer's real `data/` is
untouched.

1. **Fresh clone builds (AC1, Linux leg)**: `./scripts/fresh-clone-check.sh` extracts a clean tree,
   configures, builds, and runs **34/34** — no reliance on the committed/ignored `build/` state.
2. **Cross-platform commands exist and are runnable where a toolchain exists (AC1)**: the Linux
   presets are executed in Task 8/Validation; the Windows/macOS/Clang presets are documented and
   listed in the owner checklist (not claimed as verified here).
3. **Full-song loop at CPU budget (AC2 proxy)**: `perf_loop_test` plays the reference chart to
   `Cleared` headlessly and prints p50/p95/p99/max update ms; the gate asserts `p99 < 16.67 ms`
   and `max < 50 ms`. Real GPU/vsync FPS remains owner-verified via `--perf-report`.
4. **Real-hardware frame report (AC2, owner)**: on a mid-range machine with a GPU, run
   ```bash
   ./build/blaze-4k --perf-report --songs <pack>   # play a full song, then exit
   # prints p50/p95/p99/max frame ms + hitch count vs 16.67 ms budget
   ```
   Expected: p99 ≈ vsync period, hitch count 0. Capture the block as evidence.
5. **Arcade loop has no dead ends (AC3, logic level)**: `screen_manager_test`'s arcade-loop case
   asserts every canonical screen is registered and has an exit edge; `--start-screen select`
   headless smoke exits cleanly:
   ```bash
   rm -rf /tmp/blaze4k-e2e-d4 && mkdir -p /tmp/blaze4k-e2e-d4
   ./build/blaze-4k --headless --smoke-test 120 --start-screen select \
     --songs tests/fixtures/reference_pack --data-dir /tmp/blaze4k-e2e-d4
   # exit 0
   ```
6. **Docs/checklist present**: `docs/BUILDING.md` and `docs/CROSS_PLATFORM_VERIFICATION.md` exist,
   README links resolve, no "pre-implementation" text remains.
7. **Regression suite**: `ctest --test-dir build --output-on-failure` → **34/34**.
8. `git status` shows new files under `src/app/`, `tests/`, `scripts/`, `docs/`, plus
   `CMakePresets.json`; edits limited to `app.{hpp,cpp}`, `main.cpp`, `README.md`,
   `tests/{app,screen_manager,CMakeLists}`.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Windows/macOS builds cannot be verified here; AC1 only partly provable | Deliver presets + per-OS docs + explicit "owner-verified" checklist; state plainly in the plan and docs that only the Linux leg is machine-checked | **Flagged** (OQ3) |
| Real FPS cannot be measured headless (no GPU/vsync) | `--perf-report` measures real frame periods on the owner's hardware; the headless test is labelled a CPU-budget proxy, not an FPS proof | **In scope** (OQ2) |
| Timing-based ctest is flaky under host load | Generous gate (`p99 < 16.67 ms`, `max < 50 ms`) plus unconditional correctness assertions; `BLAZE4K_PERF_STRICT=0` escape hatch | **In scope** (OQ2) |
| Fresh-clone build re-fetches dependencies and fails offline/slow | Default to reusing the populated `build/_deps` via `FETCHCONTENT_BASE_DIR`; `--online` opt-in; network confirmed reachable | **In scope** (OQ5) |
| `FrameStats`/`--perf-report` accidentally enters the judgment path | Wall-clock only samples the App loop; purity grep in Validation; no gameplay symbol touched | **In scope** |
| Preset generator mismatch breaks `cmake --preset` on an owner host | Linux presets verified here; MSVC/macOS presets documented with their assumed generator and marked owner-run; generator left configurable | **Flagged** (OQ3) |
| Adding CI would violate AGENTS.md | Explicitly out of scope; no `.github/workflows` created | **Out of scope** |
| Portability audit finds no bugs and adds nothing | Audit still produces documented evidence (guarded `_WIN32`, SDL base path, `std::filesystem`); no warning-flag churn | **In scope** |
| Pad/OpenITG feel test impossible here | Documented as owner-manual in `CROSS_PLATFORM_VERIFICATION.md`; not asserted by any test | **Flagged / owner-manual** |

---

## Decisions

- **Verification-first, honest about limits.** The plan produces machine-checked evidence only for
  the Linux leg and the CPU-budget proxy; every OS/hardware criterion is drafted as an owner
  checklist rather than a fake-green test.
- **One pure collector, reused twice.** `FrameStats` serves the headless benchmark and the
  real-hardware `--perf-report`, and is unit-tested deterministically (no timing in its test).
- **Presentation-only instrumentation.** `--perf-report` consumes the frame period App already
  computes; the judgment path still runs exclusively on `MusicClock`.
- **Presets + one script + two docs** are the cross-platform deliverable; no CI, no new global
  warning flags, no gameplay changes.
- **Correct the stale README** ("pre-implementation") as part of the v1 gate.

---

## Open Questions

1. **Non-blocking — percentile rule.** Proposed default: **nearest-rank** (`ceil(p/100*n)-1`,
   clamped), documented and tested. Alternative: linear interpolation. Confirm or accept.
2. **Non-blocking — hard perf gate vs advisory.** Proposed default: `perf_loop_test` hard-asserts
   `p99 < 16.67 ms` / `max < 50 ms` unless `BLAZE4K_PERF_STRICT=0`, and always prints stats. If the
   owner prefers a never-flaky suite, make the budget advisory and assert correctness only.
3. **Non-blocking — preset matrix scope.** Proposed default: ship Linux (GCC/Clang) + Windows
   (MSVC) + macOS (Clang) configure/build/test presets, with only Linux/GCC run here. Alternative:
   document plain commands only and skip presets. Confirm the owner wants preset files.
4. **Non-blocking — portability warning strictness.** Proposed default: **no new global warning
   flags**; audit only, fix real portability bugs. Alternative: add an opt-in
   `BLAZE4K_PORTABILITY_WARNINGS` (`-Wshadow -Wconversion -Wsign-conversion`, no `-Werror`) preset
   and clean up any fallout. Confirm.
5. **Non-blocking — fresh-clone dependency strategy.** Proposed default: reuse the local
   `build/_deps` cache for a deterministic offline run, with `--online` for a true FetchContent
   fetch. Alternative: always fetch. Confirm.
6. **Non-blocking — `--perf-report` output format.** Proposed default: one stdout block
   (`frames`, min/median/p95/p99/max ms, mean, hitches, PASS/FAIL). Alternative: machine-readable
   JSON to a file. Confirm.
7. **Non-blocking — README status wording.** Proposed default: replace "pre-implementation" with
   the v1-complete status. Confirm exact wording if the owner has a preference.

---

## Acceptance Criteria

- [ ] **AC1 (Linux leg):** `scripts/fresh-clone-check.sh` and `linux-gcc-*` presets configure,
      build, and pass **34/34** from a clean tree; Windows/macOS presets + `docs/BUILDING.md`
      exist for the owner (Tasks 8–10; E2E 1/2)
- [ ] **AC2 (proxy):** `perf_loop_test` plays the reference chart to `Cleared` and prints
      p50/p95/p99/max under the 16.67 ms budget; `--perf-report` supplies the real-hardware
      measurement procedure (Tasks 1/3/5; E2E 3/4)
- [ ] **AC3 (logic level):** arcade-loop contract test asserts all canonical screens have exit
      edges; headless `--start-screen select` smoke exits 0 (Task 7; E2E 5)
- [ ] **AC4:** `docs/CROSS_PLATFORM_VERIFICATION.md` carries the owner checklist for native
      Windows/macOS builds, real FPS, pad traversal, and the OpenITG side-by-side feel test, each
      marked owner-verified (Task 10)
- [ ] `ctest --test-dir build --output-on-failure` → **34/34**; `app_test`,
      `screen_manager_test`, `frame_stats_test`, `perf_loop_test` green (Tasks 2/4/5/6/7/11;
      E2E 7)
- [ ] `src/app/frame_stats.hpp` stays SDL/GL/audio/wall-clock-free; zero new warnings under
      `-Wall -Wextra -Wpedantic` (Validation)
- [ ] No gameplay/scoring/timing/render/screens/persistence changes; wall-clock is presentation
      only (Validation scope grep)
- [ ] README status corrected and docs linked (Task 10)
- [ ] Open Questions OQ1–OQ7 confirmed or defaults accepted
