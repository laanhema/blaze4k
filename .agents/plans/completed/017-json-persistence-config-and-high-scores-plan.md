# Plan: JSON Persistence — Config and Per-Chart High Scores (C2)

## Summary

Add a local, offline persistence layer under `src/data/` that saves and restores (1) the player's
`config.json` (video/audio/input/offset/gameplay options) and (2) per-chart best scores in
`scores.json`. Both are plain nlohmann/json documents, loaded through a **non-throwing, defaulting,
size-capped** reader that mirrors the existing `judgment_constants_loader` pattern, and written with an
**atomic temp-file + rename** so a crash mid-write never corrupts the prior file. Data lives in a
**portable `data/` folder next to the binary** (Windows/macOS/Linux default) with an **opt-in
XDG-compliant path on Linux** per PRD §9.

C2 is the persistence seam for the rest of Phase C: C3 (song select) reads per-chart best grades and
writes the settings it changes, C4 (options) mutates config, C5 (offset calibration) writes
`global_offset_seconds`, and C7 (results) submits a completed run and flags "NEW RECORD". C2 therefore
**models** all of those settings and exposes a stable chart key, but implements **none** of those
screens or their mutations.

`ScreenContext` (added by C1) gains optional `config` / `scores` pointers so later screens consume the
shared state without a new global; `main.cpp` owns the lifecycle: resolve paths → load config before
constructing the window → load scores → attach to the shell → save both on clean exit. All new code is
platform-light (`data_paths.cpp` is the only file touching SDL, via `SDL_GetBasePath()`), so the whole
feature is testable headless.

## User Story

As a player
I want my settings, offset, and per-chart best scores saved to local JSON files
So that my records and configuration survive restarts.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | `src/data/` (NEW: config model, config loader, high scores, data paths), `src/screens/screen.hpp` (`ScreenContext` fields), `src/screens/screen_manager.hpp` (context accessor), `src/main.cpp` (boot/exit lifecycle + CLI), `CMakeLists.txt`, `tests/` |
| GitHub Issue | #17 ([C2]) |
| PRD refs | §7.6 Persistence, §9 Security & Configuration, §5 story 6, §11 success criterion "High scores, settings, and offset survive restart", §12 Phase C |
| Depends on | A1; C1 (#16) for `ScreenContext` |
| Blocks | C3, C4, C5, C7 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | build dir already configured at `/home/lauri/github/temp-5/build` |
| C++ Compiler | GCC 16.2.1 (`/usr/bin/c++`) | C++20; `-Wall -Wextra -Wpedantic` from root CMake |
| Cores | 16 | `-j16` safe |
| Dependencies | `build/_deps/` | nlohmann_json 3.11.3 already fetched and linked to `tundra_core` (`CMakeLists.txt:42-47,120`); no new dependency needed |
| Baseline tests | **16/16 pass** | `ctest --test-dir build --output-on-failure` → "100% tests passed out of 16" (0.27 s), recorded this run |
| `src/data/` | `judgment_constants_loader.{hpp,cpp}` | Existing JSON pattern to mirror: size cap, `json::parse(..., nullptr, false)`, `is_discarded()`, returns compiled defaults + `std::string* message` + status enum, **never throws** (`judgment_constants_loader.cpp:149-241`) |
| Screen seam | `src/screens/screen.hpp:21-23`, `screen_manager.hpp:40` | `ScreenContext` currently holds only `ScreenManager*`; manager owns a private `ctx_` with no accessor yet |
| App boot order | `src/app/app.cpp:17-45`, `src/main.cpp:105-109` | `App` constructor takes `AppConfig`; `window_.init()` runs inside `App::init()` **before** anything else — video settings must be applied to `AppConfig` *before* `App` is constructed |
| Window config | `src/app/window.hpp:9-16` | `WindowConfig{title,width,height,vsync,resizable,headless}` — the video persistence target |
| Gameplay/offset target | `src/gameplay/gameplay_view.hpp:22-27` | `GameplayOptions{speed, scroll, global_offset_seconds, fail_enabled}` — the gameplay/offset persistence target (applied by C3/C5, modeled by C2) |
| Chart identity inputs | `src/chart/song.hpp:11-19`, `src/chart/chart.hpp:10-26` | `Song{pack_name,song_dir,simfile_path,metadata}`, `Chart{steps_type,description,difficulty,meter,notes}` — inputs to the stable chart key |
| Test registration | `tests/CMakeLists.txt:156-164` | Add one `add_executable`/`target_link_libraries(... tundra_core)`/`add_test` block |
| Test file patterns | `tests/judgment_constants_test.cpp:13-101` | `TEST_CHECK` macro, `std::filesystem` temp files, `write_file`, seed-file candidate search |

**Start green, stay green:** 16 tests pass; this plan adds **1** test target (`config_persistence_test`)
→ **17 expected**. There are **no behavioral changes to gameplay/timing/input/render**; `main.cpp`
changes are additive (defaults preserved when no config file exists).

---

## Pinned Semantics

Authority: **PRD** for scope/format; existing `judgment_constants_loader` for the load/fallback
contract. No gameplay constants are introduced or changed.

### Data layout (PRD §9 / §7.6)

- Portable default: `<dir of executable>/data/config.json` and `<dir of executable>/data/scores.json`.
- Linux opt-in XDG: when `--xdg` (or env `TUNDRA_XDG=1`) is set, use
  `${XDG_DATA_HOME:-$HOME/.local/share}/tundra-dance/{config.json,scores.json}`.
- `--data-dir <path>` is an explicit override that wins over both (useful for tests/smoke runs).
- This matches `AGENTS.md` core principle 4: **fully offline, all state local JSON, no network**.

### Load contract (mirror `judgment_constants_loader.cpp:149-241`)

- Missing file → defaults, status `UsedDefaults`, message `"[Config] ... not found; using defaults"`.
- File larger than **1 MiB** → defaults + warning (untrusted-input cap, same as
  `judgment_constants_loader.cpp:24,160-162`).
- Unparseable / non-object document → defaults + warning; **never throws, never crashes** (issue AC 3).
- Well-formed document with one invalid field → that field keeps its default and a warning is
  appended; the remaining valid fields still load. (Deliberately *more tolerant* than the constants
  loader, which aborts the whole file on a bad field — user settings should not be all-or-nothing.)
  Unknown JSON keys are ignored.

### Save contract

- Atomic: serialize pretty JSON to `<file>.tmp` in the same directory, close, then
  `std::filesystem::rename(tmp, file, ec)`; create parent directories first. If any step fails,
  remove the temp file and return `false` with a warning — the previous file is untouched.
- Saves never throw.

### `config.json` schema (models C3/C4/C5/C7 needs, no screen implemented)

```json
{
  "version": 1,
  "video":    { "width": 1280, "height": 720, "vsync": true, "fullscreen": false },
  "audio":    { "master_volume": 1.0, "music_volume": 1.0, "preview_volume": 0.8, "ui_volume": 1.0 },
  "offset":   { "global_offset_seconds": 0.0 },
  "gameplay": { "speed_mod": "1x", "scroll": "up", "fail_enabled": true },
  "input": {
    "key_bindings":     { "Left": ["Left"], "Down": ["Down"], "Up": ["Up"], "Right": ["Right"],
                          "Confirm": ["Return"], "Back": ["Escape"] },
    "gamepad_bindings": { "Confirm": ["South"], "Back": ["East"] }
  }
}
```

- Volumes are clamped to `[0.0, 1.0]`; `width`/`height` clamped to a sane `[320, 16384]`; non-finite
  numbers rejected to the field default.
- `speed_mod` is stored as the human string accepted by `parse_speed_mod` (`"1x"`, `"C400"`, `"M600"`);
  `scroll` is `"up"`/`"down"`. Keeping these as strings keeps `src/data/config.hpp` free of
  `src/gameplay/` includes (the data module stays pure/portable like `judgment_constants.hpp`).
- `input.*_bindings` are **modeled but not applied** in C2 (remapping is a later ticket); they are
  opaque `action -> [string]` pairs so C4/C6 can consume them without a schema change.

### `scores.json` schema (PRD §7.6 / §5 story 6)

```json
{
  "version": 1,
  "scores": {
    "<chart_key>": { "grade": "quad_star", "percent": 0.9987, "dp": 123, "timestamp": 1758931200 }
  }
}
```

- `percent` is stored as a **fraction 0.0–1.0** (matching `ScoreKeeper::percent()`), displayed as `%`
  by C7. `grade` is the tier label from `GradeTier::label` (e.g. `quad_star`, `S+`). `dp` is
  `ScoreState::actual_dp`. `timestamp` is Unix seconds (`std::time(nullptr)` at submit time).
- One best record per chart key. A score is a new best iff `percent` is **strictly greater** than the
  stored one (ties do not overwrite). Missing key ⇒ new best.
- Corrupt/missing `scores.json` → empty table + warning (never crash).

### Stable chart key (issue comment: "path + chart hash … survive pack reorganization reasonably")

`std::string make_chart_key(const Song&, const Chart&)` produces a 16-hex-char FNV-1a hash over a
canonical, lowercased, `'\x1f'`-joined string of:

1. `std::filesystem::path(song.simfile_path).filename()` — the simfile file name, **not** an absolute
   path, so moving a pack folder keeps the key;
2. `song.metadata.title` and `song.metadata.artist` (normalized);
3. `chart.steps_type`, `chart.difficulty`, `chart.meter`;
4. a content fingerprint: FNV-1a over each note's `(beat, type, column)` in order.

This is deterministic (same chart ⇒ same key), distinguishes difficulties/charts in one simfile, and
does not depend on absolute-path or pack-folder name. The exact composition is flagged (OQ2).

---

## Patterns to Follow

### Non-throwing JSON load with defaults + status + message
```cpp
// SOURCE: src/data/judgment_constants_loader.cpp:149-241
JudgmentConstants load_judgment_constants(const std::filesystem::path& path, std::string* message,
                                          ConstantsLoadStatus* status);
// exists? size cap? open? parse(buffer, nullptr, false) -> is_discarded()? try fields? validate?
// set_message(...); *status = LoadedFromFile; return ...
```
```cpp
// SOURCE: src/data/judgment_constants_loader.hpp:14-29
enum class ConstantsLoadStatus { LoadedFromFile, UsedDefaults };
[[nodiscard]] JudgmentConstants load_judgment_constants(
    const std::filesystem::path& path, std::string* message = nullptr,
    ConstantsLoadStatus* status = nullptr);
```

### File size cap + guarded numeric field override
```cpp
// SOURCE: src/data/judgment_constants_loader.cpp:24,160-162
constexpr std::uintmax_t kMaxConfigBytes = 1u << 20; // 1 MiB cap for untrusted config
```
```cpp
// SOURCE: src/data/judgment_constants_loader.cpp:35-74
template <typename T> void override_number(const json& node, const char* key, T& target);
```

### Pure data model header (no platform/JSON includes)
```cpp
// SOURCE: src/timing/judgment_constants.hpp:1-19
// "This module must remain pure: it includes only <array> and <string>, with no
//  platform, audio, or filesystem/JSON dependencies. Loading lives in src/data/*."
```

### ScreenContext seam (C1, to be extended additively)
```cpp
// SOURCE: src/screens/screen.hpp:19-23
struct ScreenContext {
    ScreenManager* manager = nullptr;
};
```

### Boot order constraint (video settings before window creation)
```cpp
// SOURCE: src/main.cpp:39-43,105-109
td::AppConfig config; config.window.width = 1280; config.window.height = 720; config.window.vsync = true;
...
td::App app(config);
if (!app.init()) { ... }
```

### Test idiom
```cpp
// SOURCE: tests/judgment_constants_test.cpp:13-19,82-99
#define TEST_CHECK(expr) do { if (!(expr)) { std::cerr << "Assertion failed at " << __FILE__ << ":" \
    << __LINE__ << ": " #expr << "\n"; std::abort(); } } while (0)
void write_file(const fs::path& path, const std::string& contents);
```

### Source + test registration
```cmake
# SOURCE: CMakeLists.txt:106 / tests/CMakeLists.txt:156-164
src/data/judgment_constants_loader.cpp
add_executable(screen_manager_test screen_manager_test.cpp)
target_link_libraries(screen_manager_test PRIVATE tundra_core)
add_test(NAME screen_manager_test COMMAND screen_manager_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/data/config.hpp` | CREATE | Pure `GameConfig` model (video/audio/offset/gameplay/input defaults), `kConfigVersion` |
| `src/data/config_loader.hpp` | CREATE | `ConfigLoadStatus`, `load_config`, `save_config` declarations |
| `src/data/config_loader.cpp` | CREATE | Tolerant JSON load (cap/fallback/per-field default) + atomic save |
| `src/data/high_scores.hpp` | CREATE | `ScoreRecord`, `HighScores`, `make_chart_key`, `load_high_scores`, `save_high_scores`, `submit_high_score` |
| `src/data/high_scores.cpp` | CREATE | Chart-key FNV-1a hashing, best-score table, load/save with fallback |
| `src/data/data_paths.hpp` | CREATE | `ResolvedDataPaths`, `resolve_data_paths(...)`, `default_executable_dir()` |
| `src/data/data_paths.cpp` | CREATE | Portable vs XDG resolution; `SDL_GetBasePath()` (only SDL touchpoint) |
| `src/screens/screen.hpp` | UPDATE | Add `GameConfig* config` / `HighScores* scores` to `ScreenContext` (forward decls; additive) |
| `src/screens/screen_manager.hpp` | UPDATE | Add `ScreenContext& context()` accessor so `main.cpp` can attach shared state |
| `src/main.cpp` | UPDATE | Resolve paths, load config before window, apply video, attach context, load scores, save on exit, add `--data-dir`/`--xdg` |
| `CMakeLists.txt` | UPDATE | Add the four new `src/data/*.cpp` to `tundra_core` |
| `tests/CMakeLists.txt` | UPDATE | Register `config_persistence_test` |
| `tests/config_persistence_test.cpp` | CREATE | Round-trip, corruption fallback, size cap, atomic save, chart-key stability, best-score logic, path resolution |

`src/app/*`, `src/timing/*`, `src/input/*`, `src/gameplay/*`, `src/render/*`, and the existing
`src/data/judgment_constants_loader.*` are **not modified**. No C3/C4/C5/C7 screens are implemented.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Pure config model

- **File**: `src/data/config.hpp`
- **Action**: CREATE
- **Implement**: `namespace td` with headers `<array>`/`<string>` only (mirror `judgment_constants.hpp`'s
  purity note):
  ```cpp
  inline constexpr int kConfigVersion = 1;

  struct VideoSettings  { int width = 1280; int height = 720; bool vsync = true; bool fullscreen = false; };
  struct AudioSettings  { double master_volume = 1.0; double music_volume = 1.0;
                          double preview_volume = 0.8; double ui_volume = 1.0; };
  struct OffsetSettings { double global_offset_seconds = 0.0; };
  struct GameplaySettings { std::string speed_mod = "1x"; std::string scroll = "up"; bool fail_enabled = true; };
  struct InputSettings  { std::vector<std::pair<std::string, std::vector<std::string>>> key_bindings;
                          std::vector<std::pair<std::string, std::vector<std::string>>> gamepad_bindings; };
  struct GameConfig {
      int version = kConfigVersion;
      VideoSettings video; AudioSettings audio; OffsetSettings offset;
      GameplaySettings gameplay; InputSettings input;
  };
  ```
  Provide `InputSettings::defaults()` (the C1/`InputManager` defaults as strings: Left/Down/Up/Right,
  Return, Escape; South/East) and a free `[[nodiscard]] bool validate_game_config(const GameConfig&,
  std::string* error = nullptr)` clamping/range-checking values (volumes, window size, speed/scroll
  strings non-empty, `scroll ∈ {"up","down"}`). `validate` is pure and testable.
- **Mirror**: `src/timing/judgment_constants.hpp:1-19,83-103`.
- **Validate**: `cmake --build build -j16` (once Task 7 registers sources).

### Task 2: Config loader/saver

- **Files**: `src/data/config_loader.hpp`, `src/data/config_loader.cpp`
- **Action**: CREATE
- **Implement**:
  - Header:
    ```cpp
    enum class ConfigLoadStatus { LoadedFromFile, UsedDefaults };
    [[nodiscard]] GameConfig load_config(const std::filesystem::path& path,
                                         std::string* message = nullptr,
                                         ConfigLoadStatus* status = nullptr);
    [[nodiscard]] bool save_config(const std::filesystem::path& path, const GameConfig& config,
                                   std::string* message = nullptr);
    ```
    Document: load never throws, missing/corrupt → defaults; save is atomic and returns false on
    failure.
  - cpp: mirror `judgment_constants_loader.cpp:149-228` exactly for the file-size cap, open, read,
    `json::parse(buffer, nullptr, false)`, `is_discarded()`, `is_object()` checks and the
    `"[Config] ..."` message wording. Read each section with a `read_number`/`read_bool`/`read_string`
    helper that **leaves the field at its current (default) value and appends a warning** on wrong
    type/range instead of throwing. Unknown keys ignored. `input` bindings read from
    `{"<Action>": ["<key>", ...]}` arrays of strings; malformed entries skipped with a warning.
  - `save_config`: create parent dirs (`std::filesystem::create_directories`), serialize
    `json{{"version",...},{"video",...},...}` with `dump(2)`, write `<path>.tmp`, then
    `std::filesystem::rename(tmp, path, ec)`; on any error remove tmp, set message, return false.
- **Mirror**: `src/data/judgment_constants_loader.cpp:24,35-74,149-228`.
- **Validate**: `cmake --build build -j16`.

### Task 3: High scores + stable chart key

- **Files**: `src/data/high_scores.hpp`, `src/data/high_scores.cpp`
- **Action**: CREATE
- **Implement**:
  - Header includes `chart/song.hpp`, `chart/chart.hpp`, `<map>`, `<string>`, `<cstdint>`:
    ```cpp
    struct ScoreRecord {
        std::string grade;        // GradeTier::label, e.g. "quad_star", "S+"
        double percent = 0.0;     // fraction 0.0-1.0
        int dance_points = 0;
        std::int64_t timestamp_unix = 0;
    };
    struct HighScores {
        std::map<std::string, ScoreRecord> scores; // chart_key -> best
    };
    [[nodiscard]] std::string make_chart_key(const Song& song, const Chart& chart);
    [[nodiscard]] HighScores load_high_scores(const std::filesystem::path& path,
                                              std::string* message = nullptr,
                                              ConfigLoadStatus* status = nullptr);
    [[nodiscard]] bool save_high_scores(const std::filesystem::path& path, const HighScores& scores,
                                        std::string* message = nullptr);
    // Returns true iff `record` beat the stored best (or none existed) and stores it.
    [[nodiscard]] bool submit_high_score(HighScores& scores, const std::string& chart_key,
                                         const ScoreRecord& record);
    [[nodiscard]] const ScoreRecord* find_high_score(const HighScores& scores,
                                                     const std::string& chart_key);
    ```
  - `make_chart_key`: FNV-1a 64-bit over the canonical `'\x1f'`-joined lowercased string described in
    Pinned Semantics (filename + title + artist + steps_type + difficulty + meter + note fingerprint
    `beat|type|column`), formatted `std::snprintf("%016llx", ...)`.
  - `load_high_scores`: same defensive pattern as Task 2; missing/corrupt/oversize → empty table +
    `"[Scores] ..."` warning; per-entry invalid records skipped with a warning.
  - `save_high_scores`: atomic save as in Task 2.
  - `submit_high_score`: replace iff `record.percent > existing.percent` (strict); tie keeps existing.
- **Mirror**: `src/data/judgment_constants_loader.cpp` (load), `src/gameplay/score_keeper.cpp:186-201`
  (percent semantics).
- **Validate**: `cmake --build build -j16`.

### Task 4: Data path resolution

- **Files**: `src/data/data_paths.hpp`, `src/data/data_paths.cpp`
- **Action**: CREATE
- **Implement**:
  - Header (`<filesystem>`, `<string>`):
    ```cpp
    struct ResolvedDataPaths {
        std::filesystem::path data_dir;
        std::filesystem::path config_file;   // data_dir / "config.json"
        std::filesystem::path scores_file;   // data_dir / "scores.json"
    };
    // Pure/deterministic resolution (no env access) so it is unit-testable.
    [[nodiscard]] ResolvedDataPaths resolve_data_paths(
        const std::filesystem::path& executable_dir, bool prefer_xdg,
        const std::string& xdg_data_home, const std::string& home_dir,
        const std::filesystem::path& explicit_data_dir = {});
    // Uses SDL_GetBasePath(); returns the directory containing the binary.
    [[nodiscard]] std::filesystem::path default_executable_dir();
    ```
  - cpp: resolution order = explicit_data_dir (if non-empty) → prefer_xdg
    (`xdg_data_home` or `home_dir/.local/share`, then `/tundra-dance`) → `executable_dir/"data"`.
    `default_executable_dir()` calls `SDL_GetBasePath()` and returns its parent directory; if SDL
    returns null, fall back to `std::filesystem::current_path()/"data"`. Include `<SDL3/SDL.h>` here
    only.
- **Mirror**: candidate-list discovery in `src/data/judgment_constants_loader.cpp:230-241` /
  `src/app/app.cpp:31-33` (path candidates), and `src/chart/song_library.cpp:84-110`
  (`std::error_code` usage).
- **Validate**: `cmake --build build -j16`.

### Task 5: Extend `ScreenContext` and expose it

- **Files**: `src/screens/screen.hpp`, `src/screens/screen_manager.hpp`
- **Action**: UPDATE (additive)
- **Implement**:
  - `screen.hpp`: forward-declare `struct GameConfig; struct HighScores;` and extend
    ```cpp
    struct ScreenContext {
        ScreenManager* manager = nullptr;
        GameConfig* config = nullptr;   // C3/C4/C5 read & write; owned by main
        HighScores* scores = nullptr;   // C3/C7 read; C7 submits
    };
    ```
    No behavior change; C1 tests still compile (aggregate default-init unchanged semantics).
  - `screen_manager.hpp`: add `[[nodiscard]] ScreenContext& context() { return ctx_; }` next to the
    other accessors.
- **Mirror**: `src/screens/screen.hpp:19-23`, `src/screens/screen_manager.hpp:26-34`.
- **Validate**: `cmake --build build -j16`; `./build/tests/screen_manager_test` still passes.

### Task 6: `main.cpp` lifecycle wiring + CLI

- **File**: `src/main.cpp`
- **Action**: UPDATE
- **Implement**:
  - Parse new flags (before the existing ones): `--data-dir <path>`, `--xdg`; update `print_help()`
    (`main.cpp:21-32`). Track whether `--headless`/`--smoke-test` were given (unchanged behavior).
  - After argument parsing, **before** constructing `td::App` (`main.cpp:105`):
    1. `const auto base = td::default_executable_dir();`
    2. `const auto paths = td::resolve_data_paths(base, xdg_flag,
       getenv("XDG_DATA_HOME") ? getenv("XDG_DATA_HOME") : "",
       getenv("HOME") ? getenv("HOME") : "", data_dir_flag);`
    3. `td::GameConfig game_config = td::load_config(paths.config_file, &msg, &status);` and print the
       message (stderr for `UsedDefaults`, stdout for `LoadedFromFile`), mirroring
       `app.cpp:34-40`'s routing.
    4. Apply video to `config.window`: `width`, `height`, `vsync` from `game_config.video` (do **not**
       persist/restore `headless`; the CLI `--headless` still wins).
    5. `td::HighScores high_scores = td::load_high_scores(paths.scores_file, &msg, &status);` and print.
  - After building `shell` (`main.cpp:180-184`): `shell->context().config = &game_config;
    shell->context().scores = &high_scores;`.
  - After `app.run()` and before the existing shutdown logging (`main.cpp:212-216`): call
    `td::save_config(paths.config_file, game_config, &msg)` and
    `td::save_high_scores(paths.scores_file, high_scores, &msg)`, printing warnings on failure. Saves
    run for both the shell path and the `--gameplay-demo` path (config still persists).
  - `--gameplay-demo` keeps working; defaults (no files) produce identical video config to today.
- **Mirror**: `src/main.cpp:36-109,178-217`, message routing in `src/app/app.cpp:29-40`.
- **Validate**: `cmake --build build -j16`; headless smoke in Validation.

### Task 7: Register sources and test target

- **Files**: `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Action**: UPDATE
- **Implement**:
  - Root `tundra_core` list, after `src/data/judgment_constants_loader.cpp` (`CMakeLists.txt:106`):
    `src/data/config_loader.cpp`, `src/data/high_scores.cpp`, `src/data/data_paths.cpp`.
    (`config.hpp` is header-only.)
  - Tests: append a `config_persistence_test` block mirroring `tests/CMakeLists.txt:156-164`.
- **Mirror**: `CMakeLists.txt:80-111`, `tests/CMakeLists.txt:156-164`.
- **Validate**: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j16`.

### Task 8: Persistence test suite

- **File**: `tests/config_persistence_test.cpp`
- **Action**: CREATE
- **Implement** (`TEST_CHECK` from `tests/judgment_constants_test.cpp:13-19`; no SDL window/audio; use a
  unique temp dir under `std::filesystem::temp_directory_path()` and clean it at the end):
  1. **Config defaults** — `GameConfig{}` matches documented defaults; `validate_game_config` accepts
     them; bad volumes/sizes/scroll are rejected or clamped.
  2. **Config round-trip** — `save_config` → mutate → `load_config`; every field (including both
     binding maps) equals the saved values; status `LoadedFromFile`.
  3. **Config missing file** — defaults, `UsedDefaults`, message contains `[Config]`.
  4. **Config corrupt JSON** — write `"{ not json"`; defaults + warning, no crash (AC 3).
  5. **Config per-field tolerance** — valid file with `"video": {"width": "wide"}`; the bad field falls
     back while valid fields (e.g. `audio.master_volume`) still load; a warning is present.
  6. **Config size cap** — a >1 MiB file → defaults + warning.
  7. **Atomic save** — pre-create a valid config; force a failing save (target path is a directory) and
     assert the original file content is unchanged; assert no `*.tmp` remains on success.
  8. **Chart key stability** — same `Song`/`Chart` → identical key; changing `difficulty` or a note
     changes the key; copying the song to a different absolute folder (same filename/title/notes)
     yields the **same** key (pack-reorganization survival).
  9. **High-score best logic** — `submit` on an empty table returns true; a higher percent returns true
     and replaces; a lower or equal percent returns false and leaves the record unchanged;
     `find_high_score` returns null for unknown keys.
  10. **High-score round-trip + corruption** — save/load equality; corrupt/missing file → empty table +
     warning.
  11. **Path resolution** — `resolve_data_paths(dir, false, ...)` → `dir/data`; with `prefer_xdg=true`
      and `xdg_data_home="/x"` → `/x/tundra-dance`; with explicit override → that dir;
      `config_file`/`scores_file` are `data_dir/{config,scores}.json`.
  12. **Headless App boot smoke with temp data dir** — not required here (covered by E2E); keep the
      test free of `App`/SDL to stay fast and deterministic.
- **Mirror**: `tests/judgment_constants_test.cpp:1-108` (temp files, `TEST_CHECK`, `main()`).
- **Validate**: `ctest --test-dir build --output-on-failure` (expect **17/17**).

---

## Validation

```bash
# Configure (build dir already exists; re-run because CMake files changed)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j16

# Tests (expect 17/17: 16 existing + config_persistence_test)
ctest --test-dir build --output-on-failure

# Explicit new test
./build/tests/config_persistence_test

# Purity check: the data model must stay platform/JSON-free; only data_paths.cpp may touch SDL
rg -n "SDL_|glad|miniaudio" src/data
# expected: only src/data/data_paths.cpp

# No stray JSON include in the pure model header
rg -n "nlohmann|json" src/data/config.hpp   # (no matches)
```

## End-to-End Verification

All steps are headless (no window, no audio device) and non-blocking; each uses an isolated temp data
dir so the developer's real `data/` is untouched.

1. **Fresh boot writes config** (AC 1):
   ```bash
   rm -rf /tmp/td-e2e && ./build/tundra-dance --headless --smoke-test 120 --data-dir /tmp/td-e2e
   # exits 0, no window/GL, prints "Tundra Dance shut down cleanly."
   python3 -m json.tool /tmp/td-e2e/config.json   # valid JSON with video/audio/offset/gameplay/input
   test -f /tmp/td-e2e/scores.json && python3 -m json.tool /tmp/td-e2e/scores.json
   ```
2. **Config reload is honored** (AC 1):
   ```bash
   python3 - <<'PY'
   import json; p="/tmp/td-e2e/config.json"; d=json.load(open(p)); d["video"]["width"]=1024; d["video"]["height"]=600; json.dump(d,open(p,"w"))
   PY
   ./build/tundra-dance --headless --smoke-test 30 --data-dir /tmp/td-e2e 2>&1 | rg "\[Config\]"
   # prints "[Config] Loaded '/tmp/td-e2e/config.json'"; boot succeeds with the 1024x600 window config applied
   ```
3. **Corrupt JSON never crashes** (AC 3):
   ```bash
   printf '{ this is not json' > /tmp/td-e2e/config.json
   ./build/tundra-dance --headless --smoke-test 30 --data-dir /tmp/td-e2e; echo "exit=$?"
   # stderr shows "[Config] Warning: ... malformed JSON ... using defaults"; exit 0; config.json is rewritten valid
   python3 -m json.tool /tmp/td-e2e/config.json >/dev/null && echo "config repaired"
   ```
4. **High-score best logic + round-trip** (AC 2, exercised directly because headless cannot play a
   song): `./build/tests/config_persistence_test` prints each sub-check and exits 0; cases 8–10 prove
   stable chart keys (including across a moved pack folder), strict-best replacement, and corrupt-file
   fallback.
5. **Portable vs XDG placement** (AC 4):
   ```bash
   rm -rf /tmp/td-xdg && XDG_DATA_HOME=/tmp/td-xdg ./build/tundra-dance --headless --smoke-test 30 --xdg
   test -f /tmp/td-xdg/tundra-dance/config.json && echo "XDG path used"
   ./build/tundra-dance --headless --smoke-test 30 --data-dir /tmp/td-e2e --xdg 2>&1 | rg "/tmp/td-e2e"
   # explicit --data-dir wins over --xdg
   ```
6. **Regression**: `ctest --test-dir build --output-on-failure` → **17/17**, including the unchanged
   `screen_manager_test`, `metronome_sync_test`, and `app_test`; the existing `--gameplay-demo` harness
   still runs:
   ```bash
   ./build/tundra-dance --headless --gameplay-demo \
     "tests/fixtures/reference_pack/Tundra Pack/Tundra Anthem/Tundra Anthem.sm" --smoke-test 60
   ```
7. `git status` shows only additions under `src/data/`, the two screen-header edits, `main.cpp`, the
   CMake files, and the new test — no changes to timing/input/gameplay/render.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A crash mid-write corrupts `config.json`/`scores.json` | Atomic write to `<file>.tmp` + `rename`; original untouched on failure; test 7 | **In scope** |
| Corrupt/hostile JSON crashes boot or exhausts memory | Size cap (1 MiB), `parse(..., nullptr, false)` + `is_discarded()`, bounded object reads, per-field fallback; tests 4–6 and E2E step 3; mirrors the parser-hardening discipline | **In scope** |
| Video settings can't be applied because the window is created in `App::init()` | Load config and apply to `AppConfig` **before** constructing `App` (`main.cpp:105`); documented boot order in Pinned Semantics | **In scope** |
| Chart scores lost when packs are reorganized | Key uses simfile filename + metadata + content fingerprint, not an absolute path; test 8 covers a moved folder | **In scope** — flagged (OQ2) |
| Per-field tolerance diverges from the strict constants loader and confuses future readers | Documented explicitly in Pinned Semantics ("deliberately more tolerant") and tested (test 5) | **In scope** |
| Extending `ScreenContext` breaks C1 tests / aggregate init | Two forward-declared pointer members with `= nullptr`; C1 code never reads them; Task 5 re-runs `screen_manager_test` | **In scope** |
| `SDL_GetBasePath()` returns null or a path without a trailing slash | `default_executable_dir()` null-checks and falls back to `current_path()`; correctness of the *pure* resolver is unit-tested independently (test 11) | **In scope** |
| Writing JSON on every exit even when nothing changed | Accepted: save-on-clean-exit is idempotent and cheap; avoids dirty-tracking complexity. Flagged (OQ4) | **In scope** — flagged |
| Applying `GameplayOptions` (speed/scroll/offset/fail) or input bindings from config | **Not implemented** in C2 — fields are modeled and persisted only; C3/C4/C5/C6 apply them. `GameplayView` is untouched | **Out of scope** — flagged |
| Input key/button name ↔ `SDL_Keycode` conversion | Bindings stored as opaque strings; no conversion in C2 | **Out of scope** — flagged (OQ1) |
| C3/C4/C5/C7 screens, song wheel, options, calibration wizard, results UI | Explicitly owned by later tickets; C2 only provides the store + seam | **Out of scope** — flagged |

---

## Decisions

- **Two documents, one loader contract.** `config.json` and `scores.json` both use the
  size-capped, non-throwing, defaulting reader style established by `judgment_constants_loader`.
- **Tolerant per-field load.** Unlike the constants loader (all-or-nothing), one bad user field falls
  back to its default while valid settings survive.
- **Atomic saves.** Temp file + `rename`, so a crash never truncates the previous good file.
- **Portable by default, XDG opt-in on Linux**, matching PRD §9; `--data-dir` overrides for tests.
- **`GameConfig` is a pure struct** with string-typed `speed_mod`/`scroll`, so `src/data/` stays free
  of `src/gameplay/`/render includes (same purity rule as `judgment_constants.hpp`).
- **Stable chart key = filename + metadata + chart content fingerprint** (not absolute path), so scores
  survive moving a pack folder.
- **C2 owns the boot/exit lifecycle in `main.cpp`**, not `App`: config is loaded before the window
  exists, and both files are saved after the main loop returns.
- **`ScreenContext` gets the shared state pointers** so C3–C7 consume config/scores through the C1
  seam rather than new globals.

---

## Open Questions

1. **Non-blocking — input bindings modeling depth.** C2 stores `input.key_bindings` /
   `gamepad_bindings` as opaque `action -> [names]` and does **not** apply them. Proposed: leave
   application and `SDL_Keycode`/button-name conversion to the remapping/options ticket (C4/C6).
   Confirm this split.
2. **Non-blocking — exact chart-key composition.** Proposed
   `fnv1a(lower(filename) + title + artist + steps_type + difficulty + meter + note (beat,type,column)
   fingerprint)`, 16 hex chars. Confirm the content fingerprint is desired (vs. filename+metadata
   only), since a simfile edit then resets the record.
3. **Non-blocking — XDG trigger.** Proposed `--xdg` flag **or** `TUNDRA_XDG=1` env; `XDG_DATA_HOME`
   respected with `$HOME/.local/share` fallback. Confirm the env-var name / whether a flag alone
   suffices.
4. **Non-blocking — save policy.** Proposed unconditional save of both files on a clean exit
   (idempotent, no dirty flag). Confirm vs. dirty-tracking.
5. **Non-blocking — `percent` storage unit.** Proposed fraction `0.0–1.0` (matches
   `ScoreKeeper::percent()`), rendered as `%` by C7. Confirm.
6. **Non-blocking — whether `version` should gate migrations.** Proposed: write `"version": 1` and
   ignore it on load (unknown/newer versions still attempt a best-effort read). Confirm no migration
   logic is wanted for v1.

---

## Acceptance Criteria

- [ ] Given changed settings, `config.json` (video/audio/input/offset/gameplay) is written on exit and
      reloaded on next boot (Tasks 1/2/6; tests 1–2; E2E steps 1–2)
- [ ] Given a completed song beating the stored best, `scores.json` updates `{grade, %, DP, timestamp}`
      for the chart key (Tasks 3/6; tests 9–10; E2E step 4)
- [ ] Given corrupt JSON on disk, boot falls back to defaults with a log warning — never a crash
      (Tasks 2/3; tests 4–6/10; E2E step 3)
- [ ] Data files live in a portable `data/` folder next to the binary, with an XDG path option on
      Linux (Task 4; test 11; E2E step 5)
- [ ] Chart keys are stable across pack reorganization (Task 3; test 8)
- [ ] `ScreenContext` exposes config/scores for C3–C7 without changing C1 behavior (Task 5)
- [ ] `ctest --test-dir build --output-on-failure` → **17/17**; `--gameplay-demo` and all prior tests
      stay green (Task 8; E2E step 6)
- [ ] Zero new warnings under `-Wall -Wextra -Wpedantic`; no C3/C4/C5/C7 screens implemented
- [ ] `src/data/config.hpp` and `src/timing/` remain platform/JSON-free; only `data_paths.cpp` uses SDL
