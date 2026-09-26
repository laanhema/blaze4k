# Plan: CMake Project Scaffold with Dependency Fetching

## Summary

Establish the CMake 3.24+ project build system for Tundra Dance using C++20. Use CMake's `FetchContent` module to fetch and configure pinned external dependencies: SDL3 (≥ 3.2), glad (OpenGL 3.3 Core), nlohmann/json, miniaudio, and stb (for stb_truetype). Create the planned directory structure (`src/`, `assets/`, `tests/`, and module subdirectories under `src/`) and a `src/main.cpp` stub that links all dependencies and verifies they can be built and run on host systems. Configure CTest testing infrastructure with an initial sanity test.

## User Story

As a developer,
I want a CMake build that fetches and links all dependencies,
So that the project compiles from a fresh clone on Windows, macOS, and Linux without manual dependency installation.

## Metadata

| Field | Value |
|-------|-------|
| Type | NEW_CAPABILITY |
| Complexity | MEDIUM |
| Systems Affected | Build system, Project structure, Dependencies |
| GitHub Issue | #1 |

---

## Environment Findings

| Tool | Version / Path | Notes |
|------|----------------|-------|
| CMake | 4.4.3 | Supported (>= 3.24 required) |
| C++ Compiler | GCC 16.2.1 | C++20 supported |
| Python | 3.14.7 | Standard library available for glad generation |
| Git | Available | FetchContent git checkout working |

---

## Pinned Dependencies

| Dependency | Repository | Pinned Tag / Commit | Purpose |
|------------|------------|---------------------|---------|
| SDL3 | `https://github.com/libsdl-org/SDL.git` | `release-3.2.8` | Windowing, input events, GL context |
| glad | `https://github.com/Dav1dde/glad.git` | `v0.1.36` | OpenGL 3.3 Core loader |
| nlohmann_json | `https://github.com/nlohmann/json.git` | `v3.11.3` | JSON configuration and score persistence |
| miniaudio | `https://github.com/mackron/miniaudio.git` | `0.11.21` | Audio playback & clock |
| stb | `https://github.com/nothings/stb.git` | `2c980bb59875b0d32144a71867fbdebb2f77cd20` | Font loading (stb_truetype) & image decoding |

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | CREATE | Root CMake configuration with FetchContent and executable definition |
| `.gitignore` | CREATE | Ignore build directories, CMake artifacts, editor files |
| `src/main.cpp` | CREATE | Application entry point stub exercising dependencies |
| `tests/CMakeLists.txt` | CREATE | Test runner setup with CTest |
| `tests/sanity_test.cpp` | CREATE | Sanity unit test asserting C++20 and library linkages |

---

## Tasks

### Task 1: Create `.gitignore`
- **File**: `.gitignore`
- **Action**: CREATE
- **Implement**: Standard C++/CMake gitignore (ignore `build/`, `build-*/`, `.cache/`, IDE configs).

### Task 2: Create root `CMakeLists.txt`
- **File**: `CMakeLists.txt`
- **Action**: CREATE
- **Implement**: 
  - `cmake_minimum_required(VERSION 3.24)`
  - `project(tundra-dance VERSION 0.1.0 LANGUAGES C CXX)`
  - Set C++20 standard (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`)
  - Set warning flags (`-Wall -Wextra -Wpedantic` on GCC/Clang, `/W4` on MSVC)
  - Configure `FetchContent` for SDL3, glad (gl=3.3 core), nlohmann_json, miniaudio, and stb
  - Expose `miniaudio` and `stb` as interface libraries
  - Define `tundra-dance` executable target
  - Add directories and enable testing with CTest (`add_subdirectory(tests)`)

### Task 3: Create directory structure and `src/main.cpp`
- **File**: `src/main.cpp`
- **Action**: CREATE
- **Implement**:
  - Stub `main(int argc, char* argv[])`
  - Include headers: `<SDL3/SDL.h>`, `<glad/glad.h>`, `<nlohmann/json.hpp>`, `<miniaudio.h>`, `<stb_truetype.h>`
  - Print version and status verification
  - Create directory layout placeholders under `src/` (`app`, `screens`, `timing`, `input`, `chart`, `audio`, `render`, `gameplay`, `data`) and `assets/`

### Task 4: Setup CTest and sanity test in `tests/`
- **File**: `tests/CMakeLists.txt`, `tests/sanity_test.cpp`
- **Action**: CREATE
- **Implement**: Simple sanity test checking JSON parsing, types, and C++20 features.

### Task 5: Configure, Build, and Validate
- Run `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Run `cmake --build build`
- Run `ctest --test-dir build --output-on-failure`
- Run `./build/tundra-dance --version` or verify binary execution

---

## Validation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/tundra-dance
```

## End-to-End Verification

1. Fresh configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release` succeeds and downloads all 5 dependencies.
2. Build: `cmake --build build` produces `build/tundra-dance` without errors or warnings.
3. Execution: `./build/tundra-dance` outputs dependency verification info and exits with code 0.
4. Test: `ctest --test-dir build --output-on-failure` passes 100%.

## Risks

| Risk | Mitigation |
|------|------------|
| FetchContent download time or network flakes | Pin exact commits/tags; shallow clones where supported (`GIT_SHALLOW TRUE`) |
| SDL3 X11 / Wayland development dependencies on Linux | SDL3 CMake detects available system backends; if any headless issue occurs in main loop, stub exits gracefully without failing build |
| glad Python generation in CMake | glad CMake invokes python3 to generate headers and C loader during build |

## Acceptance Criteria

- [ ] Fresh build with `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` succeeds and produces `tundra-dance` binary
- [ ] Pinned FetchContent for SDL3, glad, miniaudio, stb_truetype, nlohmann/json
- [ ] Builds with C++20 cleanly
- [ ] Directory layout (`src/`, `assets/`, `tests/`) and `main.cpp` stub exist
- [ ] `ctest --test-dir build` passes
