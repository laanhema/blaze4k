# Plan: Metronome Sync-Test Chart (Permanent Regression Tool)

## Summary

Build a **permanent, deterministic sync regression harness** for the audio-sync-drift risk (PRD §14;
PRD §11 success criterion "gameplay sync holds within one judgment window over a full-length song").
The work is test-side only — **no production code changes** — and splits into three pieces:

1. `tests/fixtures/sync_test/metronome.sm` (NEW) — the permanent metronome chart: 120 BPM, one tap per
   beat for the full song, cycling columns 0→1→2→3, no stops / holds / rolls / mines, `#OFFSET:0.0`.
2. `tests/sync_test_harness.hpp` (NEW) — a small, reusable, header-only harness that drives a
   `MusicClock` from an **injected fake `SamplePosition` source** (no audio device, no window, no
   `<chrono>`), autoplays the chart against the clock, and reports per-note judgment deltas plus
   summary drift metrics.
3. `tests/metronome_sync_test.cpp` (NEW) — the runnable CTest entry point: validates the fixture is
   on-beat for its whole length (AC1), proves zero-drift sync stays within one judgment window, and
   proves deliberate offset / rate drift shows up in the judgment deltas at sub-window resolution
   (AC2). Registered in `tests/CMakeLists.txt`, so it remains in `tests/` as a regression tool (AC3).

**Key design principle:** gameplay time is `MusicClock` time = `frames/sample_rate + global_offset`
(B1). All drift is modeled as a **discrepancy between the chart's BPM-derived note times and the audio
clock's timebase** — exactly the failure mode Blaze 4k can suffer. Because the clock source is a
`std::function<SamplePosition()>` (B1), the harness injects a fake frame provider; it never opens an
audio device and never reads wall-clock or frame delta, so it is fully deterministic and non-flaky.

## User Story

As the developer
I want a permanent metronome sync-test chart with an automated harness
So that gameplay sync drift is caught as a regression within one judgment window over a full song.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY (test infrastructure) |
| Complexity | SMALL |
| Systems Affected | `tests/` (new fixture + harness header + test executable), `tests/CMakeLists.txt` |
| GitHub Issue | #15 ([B7]) |
| PRD refs | §11 success criteria, §12 Phase B, §14 risks (audio sync drift) |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | `cmake --build build -j16` verified working |
| C++ Compiler | GCC 16.2.1 | C++20; `-Wall -Wextra -Wpedantic` enabled in root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | SDL3 3.2.8, glad (GL 3.3 core), miniaudio 0.11.21, nlohmann_json 3.11.3, stb fetched |
| Baseline tests | **14/14 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 14" (0.31 s), recorded this run |
| `tests/` layout | `tests/*.cpp` + `tests/fixtures/reference_pack/` | Each functional area gets one executable registered via `add_executable`/`target_link_libraries(... blaze4k_core)`/`add_test` |
| Fixture path resolution | `tests/score_keeper_test.cpp:666-674` | Tests try `tests/...`, `../tests/...`, `../../tests/...` because CTest runs from the build tree |
| Audio availability | `tests/music_clock_test.cpp:97-130` | The reference fixture's `music.ogg` is a 15-byte placeholder whose load fails; the guarded audio block in `music_clock_test` shows the existing pattern is to **skip** when no device. B7 must not depend on a device at all |
| `MusicClock` | `src/timing/music_clock.hpp:28-54` | `Source = std::function<SamplePosition()>`; `time_seconds() = frames/rate + global_offset`; pure, no platform headers |
| `JudgmentEngine` | `src/gameplay/judgment_engine.hpp:21-69` | Time-parameterized; `handle_step(column, music_time_seconds)`, `events()`, `reset(chart, constants)`; includes no clock |
| B2 windows | `src/timing/judgment_constants.hpp:25-36` | `fantastic 0.0215`, `excellent 0.0430`, `great 0.1020`, `decent 0.1350`, `way_off 0.1800` |

**Start green, stay green:** 14 tests pass; this plan adds 1 test target (`metronome_sync_test`) → **15 expected**.
**No production source changes**, so no risk to the 14 existing tests beyond an additive CMake entry.

---

## Pinned Semantics

Authority: **B1 `MusicClock`**, **B2 `JudgmentConstants`**, **B4 `JudgmentEngine`**, and **OpenITG**
commit `f2c129fe65c65e4a9b3a691ff35e7717b4e8de51` where behavior originates upstream.

### Time model (B1) — the harness's contract

- `sample_time_seconds = frames / sample_rate`; `time_seconds = sample_time_seconds + global_offset`
  (`src/timing/music_clock.hpp:44-59`). Sign convention: **positive offset makes the clock read later**
  (documented at `music_clock.hpp:19-21`).
- The clock is pure: repeated reads with unchanged frames are byte-identical
  (`tests/music_clock_test.cpp:62-71`). The harness relies on this for determinism.
- The harness **must never** initialize `AudioEngine`; it binds a lambda returning
  `SamplePosition{frames, sample_rate}`.

### Judgment delta (B4)

- Input step carries an absolute **music** time; the engine stores
  `delta_ms = (hit_time_seconds - note_time_seconds) * 1000` and **negative = early**
  (`src/gameplay/judgment.hpp:26-36`; `tests/judgment_engine_test.cpp:101-122`).
- Tap classification is `classify_tap(delta)` over the B2 windows; a step beyond Way Off produces no
  event, Miss events come only from expiry (`judgment_engine_test.cpp:60-99`).
- Closest-note selection searches a single column within **±1.0 s** (OpenITG `StepSearchDistance`,
  `src/Player.cpp:27,791-823`). Blaze 4k's metronome cycles columns, so the **same column repeats every
  4 beats = 2.0 s** at 120 BPM, i.e. twice the radius — drift up to 1.0 s still selects the intended
  note instead of a neighbour.

### Drift modeled by the harness

Two independent, deliberately introduced discrepancies between the **real beat timeline** (chart note
times, from BPM math) and the **audio clock**:

1. **Constant offset drift** `d` — the audio is misaligned by `d` seconds. Set
   `clock.set_global_offset_seconds(d)`. A player/autoplay hitting the audible beat produces
   `delta_ms ≈ d * 1000` on **every** note.
2. **Progressive rate drift** `k` — the audio clock's timebase is `(1 + k)` times the assumed tempo,
   so the error accumulates linearly. Drive the fake provider with
   `frames = round(note_time * sample_rate * (1 + k))`; hitting the audible beat yields
   `delta_ms ≈ note_time * k * 1000` — near zero on the first note, largest on the last.

Detection resolution target: measured delta must track the injected drift to **well under one
judgment window** (frame quantization at 48 kHz is ≤ 1 frame ≈ 0.0208 ms).

### Fixture constraints (AC1)

- Single BPM `120.000` at beat 0, `#OFFSET:0.000`, **no** `#STOPS`.
- Exactly one `Tap` on every beat 0..N-1, column = `beat % 4`; no holds/rolls/mines.
- Therefore `note.time_seconds == beat * 0.5` exactly (B1/`TimingData::beat_to_seconds`,
  `src/chart/timing_data.cpp:147-190`), and the chart covers the full song length.

---

## Value Provenance

B7 introduces **no new game constants**. The only numbers it uses are the B2 timing windows, the
sample rate of the fake provider, and the fixture's own tempo/length.

| Value / Semantic | Value | Source |
|------------------|-------|--------|
| Sync pass/fail window | `fantastic = 0.0215 s` (also excellent/great/decent/way_off) | `src/timing/judgment_constants.hpp:25-36` (B2, OpenITG `metrics.ini`) |
| Clock formula | `frames/rate + global_offset` | `src/timing/music_clock.hpp:44-59` (B1) |
| Delta sign / units | `(hit - note) * 1000`, negative = early | `src/gameplay/judgment.hpp:31` (B4); OpenITG `Player.cpp:919,1105-1106` |
| Closest-note search radius | `1.0 s` | OpenITG `src/Player.cpp:27,791-823` @ `f2c129fe…` |
| Fake sample rate | `48000 Hz` | Matches B1 default (`music_clock.hpp:10-12`) and the project's audio config |
| Fixture tempo | `120 BPM`, 1 tap/beat, columns `beat % 4` | Blaze 4k test design (synthetic metronome); no upstream parity claim |
| Fixture length | 240 beats (120 s) | Blaze 4k test design; long enough that 0.5 % rate drift accumulates ~0.6 s ≫ 1 window |

No unsourced upstream numbers are introduced; fixture tempo/length are explicitly synthetic test
parameters (flagged in Decisions/Open Questions).

---

## Patterns to Follow

### Injectable clock source (B1's designed seam)
```cpp
// SOURCE: src/timing/music_clock.hpp:28-39
using Source = std::function<SamplePosition()>;
void set_source(Source source);
void set_global_offset_seconds(double offset);
```
```cpp
// SOURCE: tests/music_clock_test.cpp:64-66 (fake provider already used by B1 tests)
blaze4k::MusicClock clock([&frames] { return blaze4k::SamplePosition{frames, 44100}; });
```

### Time-parameterized judgment (never reads a clock)
```cpp
// SOURCE: src/gameplay/judgment_engine.hpp:25-33
void reset(const Chart* chart, const JudgmentConstants* constants);
void handle_step(int column, double music_time_seconds);
```

### Existing test assertion + note-construction idioms
```cpp
// SOURCE: tests/judgment_engine_test.cpp:13-36
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << ...; std::abort(); } } while (0)
blaze4k::Note make_note(int column, double time_seconds, blaze4k::NoteType type, double hold_end_time = 0.0);
```

### Fixture loading + path fallback
```cpp
// SOURCE: tests/score_keeper_test.cpp:666-677
std::filesystem::path ref = "tests/fixtures/...";
if (!std::filesystem::exists(ref)) ref = "../tests/fixtures/...";
if (!std::filesystem::exists(ref)) ref = "../../tests/fixtures/...";
blaze4k::SimfileParser parser; TEST_CHECK(parser.parse_file(ref.string()));
```

### Test registration
```cmake
# SOURCE: tests/CMakeLists.txt:125-133
add_executable(score_keeper_test score_keeper_test.cpp)
target_link_libraries(score_keeper_test PRIVATE blaze4k_core)
add_test(NAME score_keeper_test COMMAND score_keeper_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `tests/fixtures/sync_test/metronome.sm` | CREATE | Permanent metronome chart (one tap per beat, full length) |
| `tests/sync_test_harness.hpp` | CREATE | Reusable fake-clock autoplay + drift-report harness (header-only) |
| `tests/metronome_sync_test.cpp` | CREATE | CTest regression executable asserting AC1/AC2/AC3 |
| `tests/CMakeLists.txt` | UPDATE | Register `metronome_sync_test` |

**No production files change.** `src/timing/music_clock.*`, `src/gameplay/judgment_engine.*`,
`src/gameplay/judgment.hpp`, `src/chart/*`, and `CMakeLists.txt` are **not modified**.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Permanent metronome fixture

- **File**: `tests/fixtures/sync_test/metronome.sm`
- **Action**: CREATE
- **Implement**:
  - `#TITLE:Blaze Sync Test (Metronome);`, `#ARTIST:Engine;`, `#MUSIC:music.ogg;` (the file need
    not exist — the harness never loads audio), `#OFFSET:0.000;`, `#BPMS:0.000=120.000;`,
    `#STOPS:;` (empty).
  - One `dance-single` `#NOTES:` block, difficulty `Challenge`, meter `1`, with **240 note rows**
    grouped into **60 measures of 4 rows**. Row `n` (global beat `b = n`, `n = 0..239`) has a single
    `1` in column `b % 4`, all other columns `0`.
  - Generate the file with a one-shot script (the implementer's choice), then commit the file. Keep it
    exactly reproducible; the harness in Task 4 re-derives and verifies the pattern.
  - Beat 0's note time is 0 s and beat 239's is 119.5 s, so the chart spans the full 120 s song.
- **Mirror**: `tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm:12-48` (SM
  `#NOTES:` block shape) and its `#BPMS`/`#OFFSET` header style.
- **Validate**: `cmake --build build -j16` (parsing is exercised by Task 4).

### Task 2: Sync harness — fake clock + autoplay

- **File**: `tests/sync_test_harness.hpp`
- **Action**: CREATE
- **Implement** (header-only; includes only `<cmath>`, `<cstdint>`, `<vector>`, `<algorithm>`,
  `<string>`/`<iostream>` if it prints — **no** SDL/GL/audio/`<chrono>`):
  ```cpp
  namespace blaze4k::sync_test {

  // Fake audio clock: the injected SamplePosition source. Models the audio
  // timebase as real_seconds * (1 + rate_error); the MusicClock then applies
  // its own global_offset on top.
  struct FakeSampleClock {
      uint64_t frames = 0;
      uint32_t sample_rate = 48000;
      double rate_error = 0.0;
      double compute_frames(double real_seconds) const {
          return std::llround(real_seconds * sample_rate * (1.0 + rate_error));
      }
      // Advance the provider to `real_seconds` and return the clock's music time.
      static double music_time_at(double real_seconds, double rate_error, double offset);
  };

  struct NoteDelta {
      int column = 0;
      double note_seconds = 0.0;
      double hit_seconds = 0.0;
      double delta_ms = 0.0;
      TapJudgment window = TapJudgment::Num;
  };

  struct SyncReport {
      std::vector<NoteDelta> notes;   // one entry per judged note, in note order
      double max_abs_delta_ms = 0.0;
      double terminal_delta_ms = 0.0; // last note
      int fantastic = 0;
      int non_fantastic = 0;
      int misses = 0;
  };

  // Autoplays every non-mine note: at the note's *real* time, the player hits
  // the audible beat, so the input's music time is the clock reading there.
  [[nodiscard]] SyncReport run_autoplay_sync(
      const Chart& chart, const JudgmentConstants& constants,
      double rate_error, double offset);

  // Expected drift for a note, in ms: (note_seconds * rate_error + offset) * 1000.
  [[nodiscard]] double expected_drift_ms(double note_seconds, double rate_error, double offset);

  // True iff every note is a Tap exactly on an integer beat's time, one per beat.
  [[nodiscard]] bool is_pure_beats(const Chart& chart, int expected_beats, double eps);

  } // namespace blaze4k::sync_test
  ```
  - `run_autoplay_sync` builds a `MusicClock`, sets its source to a fake provider holding a frame
    counter, sets `global_offset_seconds(offset)`, `JudgmentEngine::reset(&chart, &constants)`, then
    **for each note in order**: compute `hit = music_time_at(note.time_seconds, rate_error, offset)`
    (updating the fake frames), `engine.handle_step(note.column, hit)`, and after the step read
    `engine.latest_event()` to fill a `NoteDelta` (note_time, hit_time, delta_ms, window).
  - Skip `NoteType::Mine` (none in the fixture; defensive). After the loop, no `update` is needed for
    all-hit taps, but call one `engine.update(hit_last, held_none)` defensively so any un-hit note
    would surface as a Miss rather than silently vanish.
  - Accumulate summary metrics; `terminal_delta_ms` = last entry's `delta_ms`.
- **Mirror**: `tests/music_clock_test.cpp:36-71` (fake provider), `tests/judgment_engine_test.cpp:52-99`
  (engine drive + event inspection).
- **Validate**: `cmake --build build -j16` (once included by Task 3).

### Task 3: Sync regression test executable

- **File**: `tests/metronome_sync_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK` macro copied from `tests/judgment_engine_test.cpp:13-19`; uses
  `SyncReport`/`run_autoplay_sync`; path fallback per Task Patterns; prints each sub-check):
  1. **Fixture integrity / AC1** — load `tests/fixtures/sync_test/metronome.sm`; assert parse succeeds,
     exactly 1 chart, `chart.notes.size() == 240`, `chart.mine_count == 0`, `hold_count==0`,
     `roll_count==0`; `is_pure_beats(chart, 240, 1e-9)`; each `note.time_seconds == note.beat * 0.5`
     within 1e-9; columns follow `beat % 4`; no `#STOPS`/exotic timing.
  2. **Zero-drift sync / PRD §11** — `run_autoplay_sync(chart, k, 0.0, 0.0)`: exactly 240 tap events,
     all `TapJudgment::Fantastic`, `misses == 0`, and `max_abs_delta_ms` below the frame-quantization
     bound (`1000.0 / 48000.0 ≈ 0.0208 ms`, assert `< 0.05`), i.e. well within one judgment window
     (`k.windows.fantastic * 1000 = 21.5 ms`).
  3. **Constant offset drift / AC2** — for `d = +0.010` and `d = -0.010`: every note's measured
     `delta_ms` equals `d * 1000` within `0.05 ms`; `max_abs_delta_ms ≈ 10 ms < 21.5 ms`, so the drift
     is **revealed** while still classifying Fantastic (detection is finer than one window). Also
     sweep `d` across `{0.500*fw, 0.999*fw, 1.001*fw}` (`fw = fantastic`) and assert the window flips
     from Fantastic to Excellent exactly as the B2 boundary requires — one-window resolution.
  4. **Progressive rate drift / AC2** — `k = +0.005`: `delta_ms[i] ≈ k * note_seconds[i] * 1000`
     within `0.05 ms`; assert first-note `|delta|` is sub-millisecond, monotonic non-decreasing, and
     `terminal_delta_ms ≈ k * 119.5 * 1000 = 597.5 ms` **> one window** (so the regression is
     unmistakably flagged). The measured drift matches the analytic value within one window.
  5. **Within-tolerance rate drift passes** — `k = 1e-6` (1 ppm), `offset = 0`: `max_abs_delta_ms` at
     the song end ≈ `0.12 ms`, assert `< k.windows.fantastic * 1000`; demonstrates the success
     criterion: a realistic tiny clock-rate error does **not** break sync over the full song.
  6. **Determinism** — run case 4 twice; assert the two `SyncReport`s are identical (note counts and
     every `delta_ms` bit-for-bit), proving the harness has no wall-clock/frame-delta input.
  7. **No audio device dependency** — assert by construction (harness never calls `AudioEngine`); no
     guarded audio block is present. (Enforced by the purity `rg` check in Validation.)
- **Mirror**: `tests/judgment_engine_test.cpp:54-151` (structure/output), `tests/music_clock_test.cpp:26-95`.
- **Validate**: build, then `./build/tests/metronome_sync_test`.

### Task 4: Register the test target

- **File**: `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**: append a block mirroring the `life_keeper_test` block
  (`tests/CMakeLists.txt:136-144`):
  ```cmake
  add_executable(metronome_sync_test
      metronome_sync_test.cpp
  )

  target_link_libraries(metronome_sync_test PRIVATE
      blaze4k_core
  )

  add_test(NAME metronome_sync_test COMMAND metronome_sync_test)
  ```
  The fixture is read at runtime via the relative-path fallback, so no CMake file-copy is needed.
- **Mirror**: `tests/CMakeLists.txt:136-144`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

---

## Validation

```bash
# Configure (build dir exists; re-run only if CMake files change)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 15/15: 14 existing + metronome_sync_test)
ctest --test-dir build --output-on-failure

# Explicit regression tool
./build/tests/metronome_sync_test

# Fixture sanity (one tap per beat; no holds/mines)
rg -n "M|2|3|4" "tests/fixtures/sync_test/metronome.sm" | rg -v "120.000|0.000" || true

# Purity check: harness must not touch platform/time/audio/GL headers
rg -n "SDL|glad|gl[A-Z]|ma_|AudioEngine|chrono|thread|GetTicksNS|GetPerformanceCounter|fixed_dt" \
  tests/sync_test_harness.hpp tests/metronome_sync_test.cpp
# (only the intentional greps above / no matches in harness logic)
```

## End-to-End Verification

1. `./build/tests/metronome_sync_test` prints each sub-check and exits 0; cases 1–2 prove the fixture
   is a full-length on-beat metronome and that zero-drift play stays within one window; cases 3–5 prove
   injected offset and rate drift are visible in the judgment deltas at sub-window resolution.
2. `ctest --test-dir build --output-on-failure` → **15/15**, all 14 prior tests still green (this change
   is purely additive: a fixture, two test-side files, and one CMake target).
3. The purity `rg` command finds no SDL/GL/miniaudio/`<chrono>` use in the harness/test, proving the
   regression tool needs **no audio device** and no wall-clock.
4. Manually load the fixture through the existing demo path for a human sanity play
   (`./build/blaze-4k --gameplay-demo "tests/fixtures/sync_test/metronome.sm"`); the demo's stub
   clock is a *harness affordance only* and is **not** what B7's regression asserts — B7's authority is
   `MusicClock` + fake source in the test.
5. Confirm `git status` shows only additions under `tests/` (plus the plan); no `src/` or root
   `CMakeLists.txt` diff.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Fixture hand-maintained, may drift off-beat as it is edited | Harness AC1 test deterministically re-validates beat/time/column/counts every run | **In scope** |
| Same-column notes closer than the ±1.0 s search radius would confuse closest-note selection | Fixture cycles columns so same column repeats every 4 beats = 2.0 s; assert gap > radius in case 1 | **In scope** |
| Fake clock frame quantization (1/48000 ≈ 20.8 µs) mistaken for drift | Assert zero-drift max delta `< 0.05 ms`; tolerances stated as sub-window | **In scope** |
| Large injected drift exceeds search radius → no event / Miss instead of numeric delta | Drift cases keep `|drift| < 1.0 s`; terminal rate drift 0.6 s; window-boundary case uses small values | **In scope** |
| Harness accidentally initializing an audio device (like `music_clock_test`'s guarded block) | Harness never includes/links a device path; purity `rg` check + test case 7 | **In scope** |
| Flakiness from wall-clock / frame timing | No `<chrono>`/SDL; fake source advances only when the harness says so; determinism case 6 | **In scope** |
| Test could pass trivially if drift is never actually injected | Cases 3–4 assert the *measured* delta equals the analytic drift, not merely "ran" | **In scope** |
| Fixture size in git (~300 lines) is awkward to review | It is plain text and structure-validated; length is a flagged parameter (OQ1) | **In scope** — flagged |
| Adding a real `--autoplay` demo mode to production | Explicitly out of B7; human play can use the existing `--gameplay-demo` | **Out of scope** |

---

## Decisions

- **Test-side only.** B7 adds no production code and does not modify `MusicClock`, `JudgmentEngine`,
  the parser, or the root `CMakeLists.txt`. The clock's injectable `Source` (B1) is the seam.
- **No audio device.** Drift is exercised through a fake `SamplePosition` source; the harness never
  calls `AudioEngine::instance()` and never needs `music.ogg`. This is the only way to keep the
  regression deterministic and runnable in CI-free local builds.
- **Permanent checked-in fixture.** The metronome `.sm` lives at `tests/fixtures/sync_test/metronome.sm`
  and is validated by the test, satisfying AC3 ("remains in `tests/` as a runnable regression tool").
- **Two drift models.** Constant `global_offset` drift and progressive clock-rate drift
  (`frames = real * rate * (1+k)`) are both injected and both asserted; this covers the
  audio-sync-drift risk from PRD §14.
- **Analysis is the reference, not a hardcoded tolerance.** Expected delta is computed from the
  injected drift (`note_seconds * k + offset`), and the measured delta must match within frame
  quantization; this tests the *pipeline*, not a magic number.
- **Fixture parameters are synthetic.** 120 BPM, 240 beats, columns `beat % 4`, no stops; chosen for
  determinism and drift accumulation, not upstream parity (no OpenITG chart is being reproduced).
- **Harness is a reusable header.** `tests/sync_test_harness.hpp` can be included by future timing
  tests (e.g. once real-audio integration lands), keeping the "tool" permanent rather than a one-off.

---

## Open Questions

1. **Non-blocking — fixture length.** Proposed: 240 beats (120 s) at 120 BPM. Longer amplifies rate
   drift but costs git lines; shorter is easier to review. Confirm 120 s is acceptable or specify a
   target song length.
2. **Non-blocking — fixture generation provenance.** Proposed: generate the `.sm` once with a
   throwaway script and commit the artifact; the harness validates structure at runtime. If the owner
   prefers reproducibility in-repo, add a committed generator later (out of B7 scope).
3. **Non-blocking — near-zero tolerance run.** Proposed: 1 ppm (`k = 1e-6`) as the "still in sync"
   case. Confirm whether an exact-zero run alone suffices for the PRD §11 criterion.
4. **Non-blocking — autoplay in the shipped demo.** Proposed: do **not** add an `--autoplay` demo flag;
   B7's assertion authority is the test. If manual pad play of the metronome is desired for feel
   testing, that can be a follow-up option in C1's menu work.

---

## Acceptance Criteria

- [ ] Given the sync-test chart, every note falls exactly on a metronome beat for the full song length
      (Task 1/3, case 1)
- [ ] Given an introduced audio/offset drift, the judgment deltas reveal the drift within one timing
      window — measured delta matches the injected offset/rate drift at sub-window resolution
      (Task 2/3, cases 3–4)
- [ ] The chart + harness remain in `tests/` and run via CTest (`metronome_sync_test`) (Task 1/4)
- [ ] Zero-drift full-song play stays within one judgment window (PRD §11) (case 2)
- [ ] No audio device, window, wall-clock, or frame-delta dependency; harness is deterministic
      (cases 6–7; purity `rg`)
- [ ] All tasks complete; zero new warnings under `-Wall -Wextra -Wpedantic`
- [ ] `ctest --test-dir build --output-on-failure` → **15/15** (14 prior + `metronome_sync_test`)
- [ ] No production source changed; follows existing fixture/test/CMake patterns
