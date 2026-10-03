# Plan: Import the Cabinet Theme Pack into the Repository (#87)

## Summary

The Cabinet theme pack is in the untracked folder `blaze4k-cabinet-theme/` (80 files, about 8 MB). This change moves its contents into the tracked repo layout, so the follow-up engine and screen issues (#89, #90, #91…) can build on committed files:

| From (`blaze4k-cabinet-theme/…`) | To (repo root) |
|---|---|
| `assets/fonts/*` (4 `.ttf` + 2 OFL `.txt`) | `assets/fonts/` |
| `assets/theme/cabinet/*` (63 texture PNGs + 2 digit-atlas PNGs + `manifest.json`) | `assets/theme/cabinet/` |
| `src/render/theme.hpp` | `src/render/theme.hpp` |
| `README.md`, `IMPLEMENTATION_PLAN.md`, `reference/` (5 images) | `docs/cabinet-theme/` |

The files are moved byte for byte, and SHA-256 checksums are compared before and after. Then the empty pack folder is removed. No engine code uses the assets yet: texture loading is #89 and fonts are #90. To meet the "compiles as part of the build" AC, the plan adds one test, `tests/theme_test.cpp`. It includes `render/theme.hpp` (with a few `static_assert`s) and checks the committed pack is complete: every font in `theme::kFontFiles` and both OFL texts exist, `manifest.json` parses with 63 textures and 2 bitmap fonts, every referenced PNG exists, its header matches `size_px` and passes the loader's own cap (`probe_image_header`), there are no orphan PNGs, and both digit atlases cover `0-9 . % /` and space. The existing POST_BUILD step (`CMakeLists.txt:161-167`) already copies all of `assets/` next to the binary, so `CMakeLists.txt` does not change. The top-level README gets the two OFL font licence lines and a docs link. `docs/BUILDING.md` lists the new asset folders. The moved pack README's "copy into the repo root" paragraph is reworded to say where each part now lives.

## User Story

As the developer of Blaze 4k
I want the Cabinet theme's fonts, textures, `theme.hpp` and design docs committed in the repo layout
So that the follow-up theme issues build on tracked, verified files and a fresh clone has everything it needs

## Metadata

| Field | Value |
|-------|-------|
| Type | REFACTOR (asset/infrastructure import; no runtime behaviour change) |
| Complexity | LOW |
| Systems Affected | `assets/fonts/` (new), `assets/theme/cabinet/` (new), `src/render/theme.hpp` (new), `docs/cabinet-theme/` (new), `tests/theme_test.cpp` + `tests/CMakeLists.txt`, `README.md`, `docs/BUILDING.md` |
| GitHub Issue | #87 (blocks #89, #90, #91) |
| Branch suggestion | `feature/039-import-cabinet-theme-pack` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is already configured (Release, host GCC). Build incrementally with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 (`/usr/bin/g++`) | C++20 with `-Wall -Wextra -Wpedantic` (`CMakeLists.txt:13`) and no `-Werror`. The change must add **no new warnings** |
| Baseline tests | **41/41 pass** | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` printed "100% tests passed out of 41" (0.41 s) on `main` @ `caac129`. Start green and stay green. The count becomes **42** after `theme_test` is added |
| Sandbox requirement | — | `audio_test` opens the real audio device, so **always** run ctest through the `bwrap` command above |
| `theme.hpp` compiles standalone | probed | `g++ -std=c++20 -Wall -Wextra -Wpedantic -I src` on a TU that includes it plus a `static_assert` compiled with **zero warnings**. Its only project include is `render/geometry.hpp` (`Color`, `Rect`), which already exists |
| Pack inventory | 80 files | 6 in `assets/fonts/`, 66 in `assets/theme/cabinet/` (65 PNG + `manifest.json`), 1 `theme.hpp`, 2 `.md`, 5 in `reference/` |
| Manifest | `assets/theme/cabinet/manifest.json` | Top-level keys: `name, version, reference_resolution, texture_scale, about, textures, bitmap_fonts`. `textures` has **63** entries and `bitmap_fonts` has **2** (`digits_chrome`, `digits_white`). Every entry has `file` and `kind`. Kinds: `frame, fullscreen, slice3, slice9, sprite, stretch, stretch_x, tile`. `size_px` is `[w,h]` on all 63 textures but a **number** (font px size) on the bitmap fonts. One texture (the `tile` one) has no `content_px` |
| Manifest ↔ files | probed | All 65 referenced PNGs exist, there are 0 unreferenced PNGs, and every texture's PNG IHDR equals its `size_px` (0 mismatches) |
| Loader caps | `src/render/texture.cpp:18-19` | `kMaxImageBytes` = 16 MiB and `kMaxImageDimension` = 4096. The largest pack PNG is 2560×1440 at about 1 MB, so all pass. `probe_image_header` is public (`src/render/texture.hpp:11-22`) |
| Digit glyphs | manifest | Both atlases have the glyph keys `' ' % . / 0-9` (14) |
| TTF magic | probed | All four `.ttf` start with `00 01 00 00` (TrueType sfnt) |
| `.gitignore` | checked | `git check-ignore` matches **none** of the new paths. There is no `.gitattributes`, and git auto-detects PNG/TTF as binary |
| Asset copy | `CMakeLists.txt:161-167` | POST_BUILD `copy_directory assets → $<TARGET_FILE_DIR:blaze-4k>/assets`. It runs **only when `blaze-4k` relinks** (see Risks) |
| Offline "fresh clone" build | probed | Export the tracked and untracked-not-ignored files to scratch, then configure with `FETCHCONTENT_SOURCE_DIR_{SDL3,GLAD,NLOHMANN_JSON,MINIAUDIO,STB}` pointing at `build/_deps/*-src`. With `--unshare-net` this configured successfully (28.7 s), so the fresh-clone AC can be checked offline |
| TODO tracking | `TODO.md:17` | This is the umbrella entry for #87–#98, so **do not tick it** for this issue. `.agents/stories/todo-stories.md` has unrelated uncommitted owner edits and is **off-limits** (do not stage, revert or edit it) |

---

## Patterns to Follow

### Tests locate committed assets via a compile definition (out-of-tree ctest cwd)

```cmake
# SOURCE: tests/CMakeLists.txt:266-280
add_executable(background_test
    background_test.cpp
)

target_link_libraries(background_test PRIVATE
    blaze4k_core
)

# Locate the committed fallback asset from out-of-tree test runs (cwd is the
# tests build dir), mirroring main's executable-relative candidate.
target_compile_definitions(background_test PRIVATE
    BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

add_test(NAME background_test COMMAND background_test)
```

```cpp
// SOURCE: tests/background_test.cpp:108-119
void test_committed_fallback_asset() {
    const fs::path asset = blaze4k::resolve_first_existing({
        fs::path("assets") / "backgrounds" / "fallback.png",
        fs::path(BLAZE4K_ASSETS_DIR) / "backgrounds" / "fallback.png",
    });
    TEST_CHECK(!asset.empty());
    TEST_CHECK(fs::exists(asset));
```

`theme_test` only needs the `BLAZE4K_ASSETS_DIR` path (the source tree is authoritative here), so it uses `fs::path(BLAZE4K_ASSETS_DIR) / ...` directly.

### Test file shape (plain executable, `TEST_CHECK` abort macro, one print line per case, explicit call list in `main`)

```cpp
// SOURCE: tests/background_test.cpp:15-22, 132-143
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)
...
int main() {
    std::cout << "[background_test] Running BackgroundRenderer tests...\n";
    test_cover_uv_equal_aspect();
    ...
    std::cout << "[background_test] All tests passed!\n";
    return 0;
}
```

Helpers go in an anonymous `namespace { namespace fs = std::filesystem; ... }`. Each case ends with `std::cout << "  - <what> ok.\n";`.

### Reusing the loader's header probe (no raw stb in tests)

```cpp
// SOURCE: src/render/texture.hpp:11-22
struct ImageHeader {
    // True only when the header parsed and the dimensions are positive and
    // within the untrusted-input cap (<= 4096px).
    ...
};
[[nodiscard]] ImageHeader probe_image_header(const std::string& path);
```

Use `blaze4k::probe_image_header(path.string())` and check `.ok`, `.width` and `.height`. This also proves `Texture::from_file` will accept every pack PNG later in #89.

### Third-party asset licence notes in README

```markdown
<!-- SOURCE: README.md:89-92 -->
Bundled third-party assets keep their own licenses:

- The Cel noteskin (`assets/noteskins/cel/`) is public domain under the [Unlicense](assets/noteskins/cel/LICENSE).
- `assets/noteskins/cel/explosions/Fallback HitMine Explosion.png` is from StepMania and is distributed under [StepMania's license](assets/noteskins/cel/explosions/LICENSE-StepMania.txt).
```

### Error handling

There is no new runtime error path, because nothing in `src/` loads these files yet. In the test, a malformed manifest makes `nlohmann::json::parse` throw. The test lets it propagate, so it terminates non-zero, the same way the other tests fail.

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `assets/fonts/{Audiowide-Regular,SairaCondensed-Medium,SairaCondensed-Bold,SairaCondensed-ExtraBold}.ttf`, `assets/fonts/OFL-{Audiowide,SairaCondensed}.txt` | CREATE (move) | Runtime fonts plus their OFL licences, kept next to them |
| `assets/theme/cabinet/` (63 textures, `digits_chrome.png`, `digits_white.png`, `manifest.json`) | CREATE (move) | Baked theme textures and their manifest |
| `src/render/theme.hpp` | CREATE (move, unchanged) | Theme palette, text styles and layout constants |
| `docs/cabinet-theme/README.md` | CREATE (move + small edit) | Pack overview. Reword the "copy into the repo root" intro and the tree to the new locations |
| `docs/cabinet-theme/IMPLEMENTATION_PLAN.md` | CREATE (move, unchanged) | Design/engine plan referenced by #88–#98. Its `reference/` mentions stay valid because `reference/` sits next to it |
| `docs/cabinet-theme/reference/` (4 `cabinet-v3-*.png` + `textures-overview.jpg`) | CREATE (move) | Target mock-ups |
| `tests/theme_test.cpp` | CREATE | Compiles `theme.hpp` and checks the committed pack is complete |
| `tests/CMakeLists.txt` | UPDATE | Register `theme_test` with `BLAZE4K_ASSETS_DIR` |
| `README.md` | UPDATE | OFL licence lines for both fonts, plus a docs link to the Cabinet theme pack |
| `docs/BUILDING.md` | UPDATE | Line 147: the list of what `assets/` holds gains fonts and Cabinet theme textures |
| `blaze4k-cabinet-theme/` | DELETE (untracked) | Removed once it is empty, after the checksum check |

`CMakeLists.txt` does **not** change (AC: "no CMake change"). `TODO.md` and `.agents/stories/todo-stories.md` do **not** change.

---

## Tasks

Execute in order. Each task is atomic and verifiable. Let `SCR=/tmp/claude-1000/-home-lauri-github-blaze4k/<session>/scratchpad` (use the session's scratchpad directory, never the repo).

### Task 1: Snapshot checksums of the pack

- **File**: none (scratch only)
- **Action**: From `/home/lauri/github/blaze4k/blaze4k-cabinet-theme`, run `find . -type f -print0 | sort -z | xargs -0 sha256sum > $SCR/pack-before.sha256`, then `wc -l` it. The expected count is **80**.
- **Validate**: `wc -l $SCR/pack-before.sha256` prints `80`.

### Task 2: Move the files into the repo layout

- **File**: the new paths in the table above
- **Action**: CREATE (move with plain `mv`, since the source is untracked and `git mv` does not apply)
- **Implement** (from the repo root):
  ```bash
  mkdir -p assets/fonts assets/theme docs/cabinet-theme
  mv blaze4k-cabinet-theme/assets/fonts/* assets/fonts/
  mv blaze4k-cabinet-theme/assets/theme/cabinet assets/theme/cabinet
  mv blaze4k-cabinet-theme/src/render/theme.hpp src/render/theme.hpp
  mv blaze4k-cabinet-theme/README.md blaze4k-cabinet-theme/IMPLEMENTATION_PLAN.md blaze4k-cabinet-theme/reference docs/cabinet-theme/
  ```
  Before the moves, confirm the targets do not exist: `assets/fonts/`, `assets/theme/` and `docs/cabinet-theme/` are absent on `main`, and `src/render/theme.hpp` is absent.
- **Validate**: Build a map from old path to new path and re-hash the new locations. Then compare against `pack-before.sha256` with the path prefixes rewritten. A simple way is a small `sed` over the "before" file:
  - `./assets/` → `assets/`
  - `./src/render/theme.hpp` → `src/render/theme.hpp`
  - `./README.md`, `./IMPLEMENTATION_PLAN.md` and `./reference/` → `docs/cabinet-theme/…`

  Then run `sha256sum -c` from the repo root. Expect all 80 `OK`, **before** Task 5 edits `docs/cabinet-theme/README.md`.

### Task 3: Remove the emptied pack folder

- **File**: `blaze4k-cabinet-theme/`
- **Action**: DELETE
- **Implement**: `find blaze4k-cabinet-theme -type f` must print **nothing**. Only then run `find blaze4k-cabinet-theme -depth -type d -empty -delete`. Do **not** use `rm -rf` blindly: if a file is left behind, stop and investigate.
- **Validate**: `test ! -e blaze4k-cabinet-theme && echo gone`.

### Task 4: Add `theme_test` (compiles `theme.hpp`, checks the pack)

- **File**: `tests/theme_test.cpp`
- **Action**: CREATE
- **Implement**:
  - Includes: `<cstdlib> <filesystem> <fstream> <iostream> <set> <string>`, `<nlohmann/json.hpp>`, `"render/texture.hpp"` (for `probe_image_header`) and `"render/theme.hpp"`.
  - Add the `TEST_CHECK` macro exactly as in `tests/background_test.cpp:15-22`.
  - Add file-scope `static_assert`s (compile-time proof that the header is usable). Do not add more than these:
    - `blaze4k::theme::hex(0xFF0000).r == 1.0f && blaze4k::theme::hex(0xFF0000).g == 0.0f`
    - `blaze4k::theme::hex(0x000000, 0.72f).a == 0.72f`
    - `blaze4k::theme::kFontFiles.size() == 4`
    - `blaze4k::theme::layout::kRefWidth == 1280.0f && blaze4k::theme::layout::kRefHeight == 720.0f`
  - Use an anonymous namespace with `namespace fs = std::filesystem;` and `const fs::path kAssets{BLAZE4K_ASSETS_DIR};`.
  - `test_font_files_present()`:
    - For each `const char* f : theme::kFontFiles`, set `p = kAssets / fs::path(f).lexically_relative("assets")`.
    - Check `fs::is_regular_file(p)` and `fs::file_size(p) > 0`.
    - Read the first 4 bytes and check they are `00 01 00 00` (TrueType sfnt).
    - Also check that `kAssets/"fonts"/"OFL-Audiowide.txt"` and `kAssets/"fonts"/"OFL-SairaCondensed.txt"` are regular files.
    - Print `"  - theme font files and OFL licences present ok.\n"`.
  - `nlohmann::json load_manifest()`:
    - Open an `std::ifstream` on `kAssets/"theme"/"cabinet"/"manifest.json"` and `TEST_CHECK(in)`.
    - Return `nlohmann::json::parse(in)`.
  - `test_manifest_textures()`:
    - Check that `m["textures"].is_object()` and `m["textures"].size() == 63`.
    - For each entry, check `file` is a string, `kind` is in `{frame, fullscreen, slice3, slice9, sprite, stretch, stretch_x, tile}`, the file is a regular file, and `size_px` is an array of 2.
    - Then run `probe_image_header(path.string())` and check `.ok` and `.width == size_px[0]`, `.height == size_px[1]`.
    - Print `"  - 63 manifest textures exist and match their PNG headers ok.\n"`.
  - `test_manifest_bitmap_fonts()`:
    - Check `m["bitmap_fonts"].size() == 2` and that it contains `digits_chrome` and `digits_white`.
    - For each: the file exists and `probe_image_header(...).ok`.
    - For each `char c : std::string("0123456789.%/ ")`, check `glyphs.contains(std::string(1, c))`.
    - Do not compare the numeric `size_px` against image dims, because it is the font size.
    - Print `"  - digit atlases exist and cover 0-9 . % / and space ok.\n"`.
  - `test_no_orphan_pngs()`:
    - Collect every `file` from `textures` and `bitmap_fonts` into a `std::set<std::string>`.
    - Iterate `fs::directory_iterator(kAssets/"theme"/"cabinet")`. Check every `.png` is in the set, and count them: expect **65**.
    - Print `"  - every cabinet PNG is referenced by the manifest ok.\n"`.
  - `main` prints `[theme_test] Running Cabinet theme pack tests...`, calls the four tests (sharing one `load_manifest()` result is fine) and prints `[theme_test] All tests passed!`.
- **Mirror**: `tests/background_test.cpp:1-31, 108-143`
- **Validate**: `cmake --build build -j$(nproc)` builds `theme_test` with no warnings from `theme_test.cpp` or `theme.hpp`.

### Task 5: Register the test in CMake

- **File**: `tests/CMakeLists.txt`
- **Action**: UPDATE (append at the end, after `assist_tick_test`)
- **Implement**:
  ```cmake

  add_executable(theme_test
      theme_test.cpp
  )

  target_link_libraries(theme_test PRIVATE
      blaze4k_core
  )

  # Compiles render/theme.hpp and checks the committed Cabinet theme pack
  # (fonts, manifest, textures) from out-of-tree test runs, mirroring
  # background_test's BLAZE4K_ASSETS_DIR seam.
  target_compile_definitions(theme_test PRIVATE
      BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
  )

  add_test(NAME theme_test COMMAND theme_test)
  ```
  `blaze4k_core` publicly provides the `src/` include dir, nlohmann_json and the `probe_image_header` definition.
- **Mirror**: `tests/CMakeLists.txt:266-280`
- **Validate**: `cmake --build build -j$(nproc)`, then run `theme_test` sandboxed. The full ctest gives **42/42**.

### Task 6: Point the moved pack README at the new locations

- **File**: `docs/cabinet-theme/README.md`
- **Action**: UPDATE (minimal; the rest of the file is unchanged)
- **Implement**: Replace lines 3-14 (the "folder layout mirrors the repository … copied straight into the repo root" sentence and the code-block tree) with the same tree written as repo paths, plus one sentence saying the pack now lives in the repo:
  ```
  Everything needed to make the game look like the **Cabinet v3** mock-ups. The pack has been
  imported into the repository (#87):

  ```
  assets/fonts/                     Audiowide + Saira Condensed (Medium, Bold, ExtraBold) .ttf, with OFL licences
  assets/theme/cabinet/             63 baked textures (@2x) + manifest.json + 2 bitmap digit fonts
  src/render/theme.hpp              colours, text styles, layout metrics and skews as constexpr
  docs/cabinet-theme/reference/     the four target screens rendered at 1280x720
  docs/cabinet-theme/IMPLEMENTATION_PLAN.md   how to wire it into the engine, step by step
  ```
  ```
  Keep the next paragraph ("`CMakeLists.txt` already copies the whole `assets/` folder…") as it is, because it is still true. Do **not** edit `IMPLEMENTATION_PLAN.md`: its `reference/` mentions resolve relative to `docs/cabinet-theme/`.
- **Validate**: `grep -n "copied straight into the repo root" docs/cabinet-theme/README.md` returns nothing.

### Task 7: README and BUILDING notes

- **File**: `README.md`
- **Action**: UPDATE
- **Implement**:
  - In "## License", after the StepMania bullet (line 92), add:
    ```markdown
    - The Audiowide font (`assets/fonts/Audiowide-Regular.ttf`) is distributed under the [SIL Open Font License 1.1](assets/fonts/OFL-Audiowide.txt).
    - The Saira Condensed fonts (`assets/fonts/SairaCondensed-*.ttf`) are distributed under the [SIL Open Font License 1.1](assets/fonts/OFL-SairaCondensed.txt).
    ```
  - In "## Documentation" (line 78), add after the Cross-platform bullet (line 82):
    `- [Cabinet theme](docs/cabinet-theme/README.md) — theme pack overview, implementation plan, and reference mock-ups`
- **File**: `docs/BUILDING.md`
- **Action**: UPDATE line 147. Change `` `assets/` (fallback background, bundled judgment constants, UI sounds) `` to `` `assets/` (fallback background, bundled judgment constants, UI sounds, fonts, Cabinet theme textures) ``. Keep the line wrap tidy.
- **Validate**: The links resolve: `test -f assets/fonts/OFL-Audiowide.txt && test -f assets/fonts/OFL-SairaCondensed.txt && test -f docs/cabinet-theme/README.md`.

### Task 8: Full validation and post-build copy check

- **Action**: Run the Validation and End-to-End sections below. Stage **only** the files listed in "Files to Change". Use `git add assets/fonts assets/theme src/render/theme.hpp docs/cabinet-theme tests/theme_test.cpp tests/CMakeLists.txt README.md docs/BUILDING.md`, and never `git add -A`, because `.agents/stories/todo-stories.md` must stay unstaged.

---

## Validation

```bash
# Build (host, Release; existing build/ is host-native)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# Lint: none configured. The bar is no new compiler warnings from theme_test.cpp / theme.hpp:
cmake --build build -j$(nproc) 2>&1 | grep -E "warning:.*(theme_test|theme\.hpp)" && echo "NEW WARNINGS" || echo "no theme warnings"

# Tests (MUST be sandboxed: audio_test opens real hardware). Expect 100% of 42.
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Pack layout sanity
ls assets/fonts | wc -l                                  # 6
ls assets/theme/cabinet/*.png | wc -l                    # 65 (63 textures + 2 digit atlases)
test -f assets/theme/cabinet/manifest.json && test -f src/render/theme.hpp && echo ok
ls docs/cabinet-theme docs/cabinet-theme/reference       # README.md IMPLEMENTATION_PLAN.md reference/ ; 5 images
test ! -e blaze4k-cabinet-theme && echo "pack folder removed"
git status --porcelain                                   # todo-stories.md still " M" (unstaged, untouched)
```

## End-to-End Verification

1. **Post-build copy (AC: "a build copies them next to the binary with no CMake change").** The POST_BUILD step only runs when `blaze-4k` relinks, and this change does not touch any `blaze-4k` source. So force a relink without editing source: `touch src/main.cpp && cmake --build build --target blaze-4k -j$(nproc)`. Then:
   ```bash
   ls build/assets/fonts | wc -l                       # 6
   ls build/assets/theme/cabinet/*.png | wc -l         # 65
   cmp assets/theme/cabinet/manifest.json build/assets/theme/cabinet/manifest.json && echo copied
   git diff --quiet -- CMakeLists.txt && echo "CMakeLists.txt unchanged"
   ```
2. **Fresh clone, offline (AC: "Fresh clone + cmake -B build && cmake --build build && ctest passes").** Export exactly what a commit of this branch would contain into scratch, then build it with the already-fetched dependency sources (no network). This configure was probed to work. Expect about 3-6 min for the full SDL build.
   ```bash
   S=$SCR/fresh; R=/home/lauri/github/blaze4k; D=$R/build/_deps
   rm -rf $S && mkdir -p $S/src
   git -C $R ls-files -z --cached --others --exclude-standard -- . ':!blaze4k-cabinet-theme' \
     | tar -C $R --null -T - -cf - | tar -C $S/src -xf -
   bwrap --dev-bind / / --unshare-net cmake -S $S/src -B $S/build -DCMAKE_BUILD_TYPE=Release \
     -DFETCHCONTENT_SOURCE_DIR_SDL3=$D/sdl3-src -DFETCHCONTENT_SOURCE_DIR_GLAD=$D/glad-src \
     -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=$D/nlohmann_json-src \
     -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO=$D/miniaudio-src -DFETCHCONTENT_SOURCE_DIR_STB=$D/stb-src
   cmake --build $S/build -j$(nproc)
   ls $S/build/assets/theme/cabinet/*.png | wc -l      # 65: copy works on a clean build
   bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir $S/build --output-on-failure   # 42/42
   rm -rf $S
   ```
   The `':!blaze4k-cabinet-theme'` pathspec is a guard: if a stray untracked copy of the pack were left, it could not hide a missing tracked file.
3. **No behaviour change.** No `src/` TU other than `theme.hpp` changes and nothing loads the new assets yet, so there is no need to launch the game. Do **not** launch `blaze-4k` (it opens a window and audio on the user's desktop).

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| A byte gets corrupted or lost during the move (binary PNG/TTF) | Task 1 checksums plus the Task 2 `sha256sum -c`, and `theme_test` checks the PNG headers against the manifest | In scope |
| `rm -rf` of the pack deletes files that were never moved | Task 3 deletes only empty directories after `find -type f` prints nothing | In scope |
| Accidentally staging or reverting `.agents/stories/todo-stories.md` | Stage with explicit paths only (Task 8). Never use `git add -A`, `git checkout .` or `git stash` | In scope |
| POST_BUILD copy is skipped on incremental builds when `blaze-4k` does not relink, so `build/assets` misses the new files | Verify with the forced relink and the fresh build (E2E 1-2). Making the copy dependency-driven (for example a custom target with `DEPENDS` on the asset files) is a pre-existing behaviour and touches CMake, which this AC forbids. Flag it for #89 | Out of scope (flag only) |
| `copy_directory` never removes stale files from `build/assets` | Pre-existing and harmless here, since nothing is removed from `assets/` | Out of scope |
| About 8 MB of binaries added to git history (5.7 MB textures, 2.1 MB reference mock-ups) | This is acceptable for a solo repo with no LFS and is what the issue asks for. The reference mock-ups could be dropped or compressed later if size matters (see Open Questions) | Out of scope (flag only) |
| `theme.hpp` warnings under MSVC `/W4` (Windows) cannot be checked on this Linux host | GCC `-Wall -Wextra -Wpedantic` is clean. The header is pure `constexpr` float maths with explicit `static_cast`s, so MSVC risk is low. Cross-platform verification follows `docs/CROSS_PLATFORM_VERIFICATION.md` when the owner next builds on Windows/macOS | Out of scope (flag only) |
| `theme_test` hard-codes 63 / 65 / 2 counts, so later theme issues that add textures must update it | This is intended as a completeness guard for this AC. The test messages name the counts, so updating them is a one-line change | In scope |
| `.agents/stories/todo-stories.md` (the TODO-15…26 sources) and GitHub issue bodies still mention `blaze4k-cabinet-theme/…` paths | The file is off-limits in this change. The issue comment already says later issues use `docs/cabinet-theme/`. Leave a follow-up note for the owner | Out of scope (flag only) |

---

## Open Questions

- **Licence of the baked textures and mock-ups.** The pack ships no licence for `assets/theme/cabinet/*.png` or `docs/cabinet-theme/reference/*`. *Proposed default:* treat them as the owner's own work under the repo's MIT licence and add no README note, the same way `assets/backgrounds/fallback.png` has none. The owner should confirm this if the art came from a third party.
- **Keep the 2.1 MB `reference/` mock-ups in git?** The issue says yes ("`reference/` mock-ups are kept under `docs/`"). *Proposed default:* keep them as they are. This is noted only because they are the largest non-runtime files.
- **Where `theme.hpp` is compiled.** The AC allows "included by at least one TU or a compile-only test". *Proposed default:* a dedicated `theme_test`, which also guards pack completeness. This avoids adding a dead include to a production TU before #89/#90 use it.

---

## Acceptance Criteria

- [ ] `assets/fonts/` holds `Audiowide-Regular.ttf`, `SairaCondensed-{Medium,Bold,ExtraBold}.ttf`, `OFL-Audiowide.txt` and `OFL-SairaCondensed.txt`
- [ ] `assets/theme/cabinet/` holds 63 textures, `manifest.json`, `digits_chrome.png` and `digits_white.png`. A build copies them to `build/assets/theme/cabinet/`, and `CMakeLists.txt` is unchanged
- [ ] `src/render/theme.hpp` is tracked and compiled by `tests/theme_test.cpp`, with no new warnings
- [ ] `docs/cabinet-theme/` holds `README.md` (intro updated), `IMPLEMENTATION_PLAN.md` and `reference/` (5 images). `blaze4k-cabinet-theme/` no longer exists
- [ ] All 80 moved files are byte-identical to the originals (checksum check), apart from the intentional README intro edit made afterwards
- [ ] README lists both OFL font licences and links the Cabinet theme docs. `docs/BUILDING.md` lists the new asset folders
- [ ] Sandboxed ctest passes **42/42** on `build/` and on the offline fresh-tree build
- [ ] `.agents/stories/todo-stories.md` and `TODO.md` are untouched and unstaged
