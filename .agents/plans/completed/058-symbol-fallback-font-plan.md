# Plan: Render symbol characters such as ☺ through a bundled fallback font

## Summary

The ITG song "Delirium" has `#ARTIST:☺;` (U+263A, UTF-8 `E2 98 BA`) and an empty `#ARTISTTRANSLIT`. Song select and results show the hollow placeholder box because of three gaps. The atlas only bakes U+0020–017F (`src/render/ttf_font.cpp:46-50`), no bundled font has a U+2600–26FF glyph, and `fold_to_ascii` has no fold for U+263A.

This plan does four things:

1. **Bundles one subset font**, `assets/fonts/NotoSansSymbols-Subset.ttf` (~100 KB, OFL 1.1, no Reserved Font Name). It is built by a committed script from **both** Noto Sans Symbols and Noto Sans Symbols 2. Measured on this host: U+263A is **only** in Noto Sans Symbols (1), not in Symbols 2 as the issue suggests. U+2600–26FF is split between the two families, and merging them covers all 256/256.
2. **Extends the slot table** with four symbol ranges: U+2190–21FF, U+25A0–25FF, U+2600–26FF and U+2700–27BF. The slot count goes from 319 to 975, so every face knows which of these code points it has.
3. **Adds a symbol fallback step** to the shared glyph walker. Lookup order: primary glyph, then symbol-face glyph, then ASCII fold, then placeholder. `measure`, `draw`, `truncate` and `covers_text` all use it, so they agree.
4. **Bakes symbol glyphs lazily**, per (size, used code points), into a small separate atlas per text size. Only glyphs that are actually drawn get baked, with a hard per-size glyph cap. An eager bake of the whole subset would be 73 MB at 4K title size and would not fit the 4096² cap at 2x oversampling (measured below).

`song_display_artist` / `song_display_title` need no change: they already ask `TextRenderer::covers_text`, which now counts fallback glyphs. This is presentation only: no clock, judgment or input code is touched.

## User Story

As a player browsing songs from real ITG packs
I want artist and title text with common symbols (☺ ★ ♥ ♪ ✓ ■ …) to show the real glyph
So that songs like Delirium (artist "☺") read correctly instead of showing an empty box

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | MEDIUM |
| Systems Affected | `src/render/ttf_font.{hpp,cpp}`, `src/render/theme.hpp`, `assets/fonts/` (new font + OFL), `scripts/` (new build script), `tests/ttf_font_test.cpp`, `tests/select_art_test.cpp`, `tests/theme_test.cpp`, `README.md`, `TODO.md` |
| GitHub Issue | #124 ([TODO-32] Render symbol characters such as ☺ in song artist names) |
| Branch (suggested) | `feature/058-symbol-fallback-font` |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|---|---|---|
| CMake | 4.4.3 | `build/` is configured (Release, Unix Makefiles) and builds cleanly with `cmake --build build -j$(nproc)` |
| C++ compiler | GCC 16.2.1 | C++20, `-Wall -Wextra -Wpedantic`, no `-Werror`. Add **no new warnings** |
| Baseline tests | **51/51 pass** | `main` @ `b95dcc1`, with the sandboxed ctest command in Validation (2.84 s). The count stays **51** because no test executable is added |
| Sandbox requirement | — | Some tests open the real sound device. **Always** run ctest inside the `bwrap` prefix in Validation |
| fonttools (`pyftsubset`, `pyftmerge`) | **not installed** on the host (no `pyftsubset`, no `python3-fonttools` RPM, `import fontTools` fails) | `python3 -m venv` + `pip install fonttools` works (pip 26.0.1, Python 3.14). The prototype ran with **fonttools 4.66.1** in a scratch venv. The build script (Task 1) makes its own throwaway venv, so nothing is installed system-wide |
| Network (dev time only) | raw.githubusercontent.com reachable | Only the font build script uses it, once. The game stays offline (principle 4). The built `.ttf` is committed |
| Host copies of the fonts | `/usr/share/fonts/google-noto/NotoSansSymbols2-Regular.ttf` (static), `/usr/share/fonts/google-noto-vf/NotoSansSymbols[wght].ttf` (**variable**) | Do **not** use them: the Symbols 1 copy is a variable font and both are distro-versioned. Use the pinned upstream static files below |
| Delirium | `songs/In The Groove/Delirium/Delirium.sm` | `#ARTIST:` bytes `E2 98 BA` (U+263A), `#ARTISTTRANSLIT:;` empty. Challenge 12 chart; audio `Smiley - Delirium.ogg` |
| Off-limits file | `.agents/issues/todo-issues.md` | Unrelated, uncommitted owner edits. Do **not** stage, revert or edit it |

### Font source (pinned)

| File | URL (pinned commit) | sha256 | Version |
|---|---|---|---|
| Noto Sans Symbols Regular (static, unhinted) | `https://raw.githubusercontent.com/notofonts/notofonts.github.io/1b2fe62733b83bdb2018a77978be2d7aa424fd43/fonts/NotoSansSymbols/unhinted/ttf/NotoSansSymbols-Regular.ttf` | `6eea9cb4cd39269ea9f95ba5c2735f80ae74049dfc9e1a7c932a5cfc8f0c3030` | 2.003, 145 508 B |
| Noto Sans Symbols 2 Regular (static, unhinted) | `https://raw.githubusercontent.com/notofonts/notofonts.github.io/c16b117609abbe4e60b3f2bd4433bdb3d0accb2e/fonts/NotoSansSymbols2/unhinted/ttf/NotoSansSymbols2-Regular.ttf` | `c4a0a80f0041ce4be81e2478faad22776d23edb98ae3f0d19bd37044820ecf9d` | 2.008, 671 568 B |
| License | `https://raw.githubusercontent.com/notofonts/symbols/e8d979919e083f8c60d884f2616d4a15b5ee77e0/OFL.txt` | — | SIL OFL 1.1, "Copyright 2022 The Noto Project Authors (https://github.com/notofonts/symbols)". **No Reserved Font Name** is declared, so a subset may keep the "Noto Sans Symbols" name. Both families live in the same `notofonts/symbols` repo, so one OFL file covers the merged subset |

These commit SHAs are the latest commits touching each path as of 2026-10-07 (`gh api repos/notofonts/notofonts.github.io/commits?path=…`). Before committing, the implementer should re-check that the sha256 values still match what the pinned URLs return.

### Reproduction (headless, `build/libblaze4k_core.a`, scratch probe)

| Probe | Result | Meaning |
|---|---|---|
| `TextRenderer::covers_text("☺", SairaBold)` | `0` | Not drawable today (the translit choice falls back to native only because the translit is empty) |
| `baked_glyph_slot(U+263A)` | `-1` | Not in any baked range |
| `TextRenderer::measure("☺", kArtist)` | `14.4` = 0.6 × 24 | The placeholder advance, so the **box** is drawn. Reproduces the bug |
| `FontFace::from_file(<merged subset>)` | **FAIL: "font has no glyph for 'A'"** | `from_bytes` probes 'A' (`ttf_font.cpp:384-387`). The symbol subset has no Latin, so the probe must be configurable (Task 2) |
| `validate_sfnt(<merged subset>)` | pass | glyf outlines, sfnt 0x00010000, unitsPerEm 1000, required tables present |
| stb: glyph(U+263A) in subset | index 180, advance 796 u, box (51,6)–(745,700) | ≈ 0.69 em tall, close to Saira's cap height. A 48 px render has 308 inked texels |
| Subset coverage (stb) | U+2600–26FF **256/256**; U+25A0–25FF 96/96; U+2700–27BF 180/192; U+2190–21FF 23/112 | The arrow block is sparse in Noto. Saira already has U+2190–2195 natively (see below) |

### Bundled-font coverage in the new ranges (`fc-query`)

- Saira Condensed (Medium/Bold/ExtraBold) already has U+2190–2195, U+21A8 and U+25CA. Once the ranges are added to the slot table, these bake from Saira itself (primary wins), so the slot change alone fixes them.
- Audiowide has none of the four ranges.
- Neither font has anything in U+2600–26FF or U+2700–27BF. This confirms the issue's note.

### Atlas footprint: why symbol glyphs bake lazily (measured with stb on the merged subset, all 555 glyphs)

| Pixel size (`size_px * s`) | 2x oversample | 1x |
|---|---|---|
| 24 (artist @720p) | 0.73 Mtexel (2.8 MB RGBA) | 0.19 Mtexel |
| 44 (song title @720p) | 2.27 Mtexel (8.7 MB) | 0.58 Mtexel |
| 66 (title @1080p) | 4.98 Mtexel (19 MB) | 1.26 Mtexel |
| 132 (title @4K) | **19.2 Mtexel (73 MB). Over the 4096² = 16.8 Mtexel cap** | 4.9 Mtexel (18.6 MB) |

Baking the whole subset into every one of the ~20 theme atlases would multiply GPU memory and break the "keep atlas size within the existing cap" note at 4K. A lazily baked per-size symbol atlas that holds only the code points actually drawn (usually 1–5) costs a few KB.

### Forward references to #124

`grep -rn "#124\|TODO-32"` finds only `TODO.md:48` (the source line, already tagged `(#124)`, which Task 7 ticks) and the off-limits `.agents/issues/todo-issues.md`. There are no placeholders in `src/`, `tests/` or `docs/`.

---

## Pinned Semantics (the contract the tests pin)

1. **Glyph resolution order** for each visible code point `cp` (zero-width code points are still skipped first):
   1. The primary face has `baked_glyph_slot(cp)`: **primary** glyph.
   2. Otherwise, the symbol face is loaded and has that slot: **symbol** glyph. Its advance is `symbol.advance_units(slot) * symbol.em_scale(pixel_size)`.
   3. Otherwise, the ASCII fold (if any) that the primary face has: **primary** folded glyph.
   4. Otherwise: **placeholder** (0.6 em advance, in-atlas hollow box).

   Consequence: ★/☆ (U+2605/2606) used to fold to `*` and now draw the real star when the symbol font is loaded. The bitmap font and `fold_to_ascii` itself are unchanged.
2. **Kerning** applies only between two consecutive **primary** glyphs. A symbol glyph or placeholder breaks the kerning chain (same as the placeholder today).
3. **`covers_text(text, font)`** is true iff every visible code point resolves at step 1 or step 2. Folds and placeholders still do not count. With no primary face loaded, the bitmap rule is unchanged.
4. **Measure never depends on bake state.** `measure`/`truncate` use the symbol face's metrics even headless or when the symbol glyph could not be baked. If the draw cannot get a baked symbol glyph (cap reached, bake failed, headless), it draws the **placeholder box at the same pen position and advance**, so layout never shifts.
5. **Placement**: symbol glyphs sit on the primary baseline at the same em size (`layout.pixel_size`). They get the same italic shear, tracking, shadow pass and colour as their neighbours.
6. **Missing or corrupt symbol font**: one `std::cerr` line at `load()` (`[TextRenderer] Symbol font unavailable: <path> (<reason>); symbols draw as placeholder boxes`). `load()`'s return value still depends only on the theme fonts. Behaviour is otherwise exactly today's.
7. **Bounds** (simfile text is untrusted): at most `kMaxSymbolAtlases = 16` symbol atlases (one per distinct `size_px`) and at most `kMaxSymbolGlyphsPerAtlas = 64` code points per atlas. Past either cap, the extra glyphs draw the placeholder (Pinned Semantics 4), with one log line per renderer lifetime. A symbol atlas is rebaked only when a new in-cap code point is first drawn at that size. A scale change (`set_window_size`) drops all symbol atlases, and they re-bake lazily.
8. **Malformed UTF-8** still decodes to U+FFFD, which is in no baked range, so it draws the placeholder. Nothing crashes (the fuzz test covers this).

---

## Patterns to Follow

### Baked range table + static_assert (extend, do not restructure the lookup)
```cpp
// SOURCE: src/render/ttf_font.cpp:40-51
struct BakedRange {
    char32_t first;
    int count;
    int first_slot;
};
constexpr std::array<BakedRange, 3> kBakedRanges = {{
    {0x0020, 95, 0},
    {0x00A0, 96, 95},
    {0x0100, 128, 191},
}};
static_assert(95 + 96 + 128 == kBakedGlyphCount, "baked ranges must cover every slot");
```

### Error handling: `fail(error, msg)` + `std::optional`, one log line, never throw
```cpp
// SOURCE: src/render/ttf_font.cpp:380-387
if (stbtt_InitFont(&impl->info, data, 0) == 0) {
    fail(error, "stb_truetype could not parse the font");
    return std::nullopt;
}
if (stbtt_FindGlyphIndex(&impl->info, 'A') == 0) {
    fail(error, "font has no glyph for 'A'");
    return std::nullopt;
}
```

### Log-once warnings on TextRenderer
```cpp
// SOURCE: src/render/ttf_font.cpp:917-925
if (atlases_.size() >= kMaxAtlases) {
    if (!warned_atlas_cap_) {
        warned_atlas_cap_ = true;
        std::cerr << "[TextRenderer] Atlas cap (" << kMaxAtlases << ") reached; " ...
    }
    return nullptr;
}
```

### Font path resolution (cwd, then next to the executable; `load(root)` for tests)
```cpp
// SOURCE: src/render/ttf_font.cpp:770-787
const std::filesystem::path relative(theme::kFontFiles[i]);
const std::filesystem::path resolved =
    resolve_first_existing({relative, default_executable_dir() / relative});
paths[i] = resolved.empty() ? relative : resolved;
```

### The one shared glyph walker (measure and draw must never disagree)
`walk_glyphs` at `src/render/ttf_font.cpp:147-169` is the single place that resolves slots, kerning and advances. It is used by `measure_text` (677), `for_each_text_quad` (708) and `draw_bitmap_fallback` (1066). Extend it. Do not add a second walker.

### Tests (plain executable, `TEST_CHECK` abort macro, numbered sections, one `std::cout` line per section)
```cpp
// SOURCE: tests/ttf_font_test.cpp:73-91, 446-503
FontFace load_face(theme::Font font) { ... TEST_CHECK(face.has_value()); ... }
FontAtlas bake(const FontFace& face, float pixel_size) { ... FontAtlas::bake(face, pixel_size, 4096, &error) ... }
...
std::cout << "  - measure (kerning, tracking, zero-width, placeholder, folds, scale) ok.\n";
```
Log assertions use `CerrCapture` + `count_of(log, "\n")` (`tests/ttf_font_test.cpp:283-292`).

### Display-text choice tests with a real `TextRenderer`
`tests/select_art_test.cpp:380-412` (`cafe` / `cjk` / `bare` metadata, pointer-identity checks such as `&song_display_artist(m, &text, font) == &m.artist`).

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `scripts/build-symbol-font.sh` | CREATE | Reproducible build of the subset from the pinned upstream files (throwaway venv, sha256 check, subset + merge) |
| `assets/fonts/NotoSansSymbols-Subset.ttf` | CREATE (binary, script output) | The fallback face (~100 KB) |
| `assets/fonts/OFL-NotoSansSymbols.txt` | CREATE | Upstream OFL.txt verbatim (mirrors `OFL-Audiowide.txt` / `OFL-SairaCondensed.txt`) |
| `src/render/theme.hpp` | UPDATE | `kSymbolFontFile` constant next to `kFontFiles` |
| `src/render/ttf_font.hpp` | UPDATE | Slot count 975, `kLatinGlyphCount`, probe parameter, `bake(..., only_slots)`, `GlyphQuad::fallback`, fallback-aware `measure_text` / `for_each_text_quad` overloads, TextRenderer symbol members, header comment |
| `src/render/ttf_font.cpp` | UPDATE | Ranges, walker resolution, bake by slot list, symbol face load, lazy symbol atlases, draw/covers/measure wiring |
| `tests/ttf_font_test.cpp` | UPDATE | Adjust 319-based expectations; add a symbol fallback section; extend fuzz |
| `tests/select_art_test.cpp` | UPDATE | Delirium-style metadata keeps the native "☺" |
| `tests/theme_test.cpp` | UPDATE | The symbol font file exists |
| `README.md` | UPDATE | License line for the bundled Noto subset (`README.md:96-97` pattern) |
| `TODO.md` | UPDATE | Tick line 48 (#124) |

No CMake change: `assets/` is copied wholesale by the post-build `copy_directory` (`CMakeLists.txt` ~line 175), and tests read fonts from `BLAZE4K_SOURCE_DIR`.

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Build and commit the symbol subset font

- **Files**: `scripts/build-symbol-font.sh` (CREATE, `chmod +x`), `assets/fonts/NotoSansSymbols-Subset.ttf`, `assets/fonts/OFL-NotoSansSymbols.txt`
- **Implement**: a `bash` script with `set -euo pipefail` that:
  1. `work=$(mktemp -d)`, with a `trap` to remove it. Downloads go in `$work/dl/` and the venv in `$work/venv/`, keeping downloaded data and the tooling in separate dirs.
  2. `python3 -m venv "$work/venv" && "$work/venv/bin/pip" install -q 'fonttools==4.66.1'`.
  3. `curl -fsSL` the two pinned font URLs and the pinned OFL URL from **Font source (pinned)**, and verifies the two font sha256 values (`sha256sum -c`). It aborts on mismatch.
  4. Subsets each font with the same code point set:
     ```bash
     U='U+2190-21FF,U+25A0-25FF,U+2600-26FF,U+2700-27BF'
     "$work/venv/bin/pyftsubset" "$work/dl/NotoSansSymbols-Regular.ttf"  --unicodes="$U" \
         --layout-features='' --drop-tables+=GSUB,GPOS,GDEF --no-hinting --notdef-outline \
         --name-IDs='*' --output-file="$work/s1.ttf"
     # same for NotoSansSymbols2-Regular.ttf -> s2.ttf
     ```
  5. `SOURCE_DATE_EPOCH=0 "$work/venv/bin/pyftmerge" --output-file="$out" "$work/s1.ttf" "$work/s2.ttf"`. Symbols 1 goes first, so its glyphs (☺ U+263A–263B) win any overlap. `SOURCE_DATE_EPOCH` keeps `head.modified` stable so re-runs are byte-identical.
  6. Writes `assets/fonts/NotoSansSymbols-Subset.ttf` and copies the OFL to `assets/fonts/OFL-NotoSansSymbols.txt` (resolved relative to the script's repo root, `$(dirname "$0")/..`).
  7. Prints the output size and sha256.

  Add a header comment with the provenance (URLs, commits, license, "dev-time only; the game never downloads anything"). Run it once and commit the output. In the prototype (without `--drop-tables`), the merge produced 101 948 B covering U+2600–26FF 256/256.
- **Mirror**: `scripts/fresh-clone-check.sh` (usage/help header style, `set -euo pipefail`, `mktemp -d` work dir)
- **Validate**:
  ```bash
  scripts/build-symbol-font.sh
  fc-query --format='%{charset}\n' assets/fonts/NotoSansSymbols-Subset.ttf   # expect a range like 25a0-2704 (covers all of 2600-26ff, incl. 263a)
  ls -l assets/fonts/NotoSansSymbols-Subset.ttf   # roughly 80-110 KB, far under kMaxFontBytes (16 MiB)
  ```
  If `pip` / network is unavailable, stop and report. Do **not** substitute the host's `/usr/share/fonts` copies (the Symbols 1 copy is a variable font).

### Task 2: Slot table, configurable probe, and bake-by-slot-list

- **Files**: `src/render/ttf_font.hpp`, `src/render/ttf_font.cpp`
- **Implement**:
  1. `kBakedRanges` becomes 7 entries:
     `{0x2190,112,319}, {0x25A0,96,431}, {0x2600,256,527}, {0x2700,192,783}` appended to the 3 Latin ones.
     In the header: `inline constexpr std::size_t kLatinGlyphCount = 319;` and `kBakedGlyphCount = 975;`. Update the comment at `ttf_font.hpp:66` to list the ranges. Update the `static_assert` sum.
     Out-of-order ranges are fine: `baked_glyph_slot` and `slot_code_point` scan linearly.
  2. `FontFace::from_bytes(std::vector<std::uint8_t> bytes, std::string* error, char32_t required_cp = U'A')` and the same for `from_file`. The error message becomes `"font has no glyph for U+XXXX"`, or keeps `'A'` for the default so existing log/test strings do not change: format `'A'` when `required_cp == U'A'`, else `U+263A`. In the header, add `inline constexpr char32_t kSymbolProbeCodePoint = 0x263A;`.
  3. `FontAtlas::bake(const FontFace&, float pixel_size, int max_dim, std::string* error, std::span<const int> only_slots = {})`. Restructure the packing around **one list of slots to pack**: every slot with `glyph != 0`, or, when `only_slots` is non-empty, only those slots that are in range and present. Use a single `stbtt_pack_range` with `array_of_unicode_codepoints` = their code points and a parallel `std::vector<stbtt_packedchar>`. Map each packed entry back to `atlas.glyphs[slot]`. `stbtt_GetPackedQuad` takes the index in the packed list. Estimate the area from the same list. Everything else stays as-is: the placeholder, the width doubling, the 2x→1x retry and the height crop. An empty pack list (no present slot selected) still bakes just the placeholder.
  4. `FontFace::Impl` arrays size automatically from `kBakedGlyphCount`.
- **Gotchas**:
  - `test_atlas_bake` asserts `present == kBakedGlyphCount // Saira covers all 319` (`tests/ttf_font_test.cpp:377`). Saira now also has a few symbol-range slots (U+2190–2195, U+21A8, U+25CA). Task 5 changes that check to count Latin slots (`< kLatinGlyphCount`) only.
  - The Audiowide check (`tests/ttf_font_test.cpp:416-424`) loops `slot = 191 .. kBakedGlyphCount` and expects exactly 1 absent. It must loop to `kLatinGlyphCount`.
- **Mirror**: `src/render/ttf_font.cpp:512-655` (keep the retry/crop/placeholder logic byte-for-byte where possible)
- **Validate**: `cmake --build build -j$(nproc)`, then the sandboxed `ctest -R ttf_font_test`. It is expected to fail only on the two count checks above until Task 5.

### Task 3: Fallback-aware walker, measure and quad emission (pure layer)

- **Files**: `src/render/ttf_font.hpp`, `src/render/ttf_font.cpp`
- **Implement**:
  1. `walk_glyphs(face, fallback /*nullable*/, text, pixel_size, k_em, fn)`. The callback becomes `fn(cp, slot, source, kern, advance)` with `enum class GlyphSource { Primary, Symbol, Placeholder }`, where `slot` is -1 for Placeholder. Resolution follows Pinned Semantics 1. Kerning only Primary→Primary: track `previous_primary` and reset it to -1 on Symbol/Placeholder. Symbol advance uses `fallback->advance_units(slot) * fallback->em_scale(pixel_size)`.
     `resolve_slot` splits into the primary check, the symbol check and the fold check.
  2. Public overloads, with the old signatures kept as one-line wrappers that pass `nullptr` so existing callers and tests compile unchanged:
     ```cpp
     float measure_text(const FontFace& face, const FontFace* fallback, std::string_view text,
                        float pixel_size, float tracking);
     void for_each_text_quad(const FontFace& face, const FontAtlas& atlas,
                             const FontFace* fallback, const FontAtlas* fallback_atlas,
                             std::string_view text, float x, float y, const TextLayout& layout,
                             const std::function<void(const GlyphQuad&)>& emit);
     ```
  3. `GlyphQuad` gets `bool fallback = false;`. In `for_each_text_quad`:
     - A Symbol glyph whose `fallback_atlas != nullptr && fallback_atlas->glyphs[slot].present` uses that glyph with `quad.fallback = true`, scaled by `pixel_size / fallback_atlas->pixel_size`.
     - Otherwise it uses `atlas.placeholder` with `fallback = false`, at the same pen position and advance (Pinned Semantics 4).
     - Width and alignment come from the fallback-aware `measure_text`.
  4. Update the header comment block (`ttf_font.hpp:3-39`): add the symbol fallback to "Four layers" and the UTF-8 convention line ("… a code point the font lacks uses the symbol font, else its ASCII fold, else the placeholder box").
- **Mirror**: `src/render/ttf_font.cpp:147-169`, `677-744`
- **Validate**: build. The existing `test_measure` / `test_layout` must still pass unchanged (the old overloads pass `nullptr`).

### Task 4: TextRenderer: load the symbol face, lazy symbol atlases, wiring

- **Files**: `src/render/theme.hpp`, `src/render/ttf_font.hpp`, `src/render/ttf_font.cpp`
- **Implement**:
  1. `theme.hpp`, after `kFontFiles`:
     ```cpp
     // Fallback for symbols the theme fonts lack (#124): a merged Noto Sans Symbols 1+2 subset
     // (U+2190-21FF, U+25A0-25FF, U+2600-26FF, U+2700-27BF). Not a theme::Font; never styled directly.
     constexpr const char* kSymbolFontFile = "assets/fonts/NotoSansSymbols-Subset.ttf";
     ```
  2. TextRenderer members: `std::optional<FontFace> symbol_face_;`, plus
     ```cpp
     struct SymbolAtlasSlot {
         float size_px = 0.0f;              // at 720p
         std::vector<int> slots;            // sorted, unique, <= kMaxSymbolGlyphsPerAtlas
         std::optional<FontAtlas> atlas;    // nullopt: bake failed (glyphs draw the placeholder)
     };
     std::vector<SymbolAtlasSlot> symbol_atlases_;
     bool warned_symbol_cap_ = false;
     ```
     Also `kMaxSymbolAtlases = 16` and `kMaxSymbolGlyphsPerAtlas = 64` in the anonymous namespace, and public `[[nodiscard]] bool symbol_font_available() const;`.
  3. `load()` / `load(root)` resolve `kSymbolFontFile` the same way as the theme fonts. Pass it to `load_paths(paths, symbol_path)`, which loads it with `FontFace::from_file(path, &error, kSymbolProbeCodePoint)`. On failure it logs the single Pinned Semantics 6 line. The "Loaded N/4 fonts" stdout line is unchanged. You may append `" (+ symbols)"` when loaded, but that line goes to stdout and no test captures stdout, so it is optional. `shutdown()` resets `symbol_face_`, `symbol_atlases_` and `warned_symbol_cap_`.
  4. `set_window_size`: wherever `atlases_.clear()` runs on a scale change, also `symbol_atlases_.clear()`.
  5. `measure` / `truncate` / `draw_bitmap_fallback`: pass `symbol_face()` (nullable) to the fallback-aware `measure_text` / `walk_glyphs`.
  6. `covers_text`: per visible cp, `slot = baked_glyph_slot(cp)`. Covered iff `source->has(slot) || (symbol_face_ && symbol_face_->has(slot))`.
  7. New private `const FontAtlas* symbol_atlas_for(float size_px, std::string_view text)`, which needs GL and is called from `draw` after the primary atlas is found:
     - Collect the slots in `text` that resolve to `GlyphSource::Symbol` for this style's primary face. Use the walker or a tiny loop with the same rule. If there are none, return the existing atlas (or nullptr).
     - Find or create the `SymbolAtlasSlot` for `size_px`. If none exists and the count is ≥ `kMaxSymbolAtlases`, log once and return nullptr.
     - Merge in new slots up to `kMaxSymbolGlyphsPerAtlas`. Log once (`warned_symbol_cap_`) when some are dropped.
     - If the set grew, rebake: `FontAtlas::bake(*symbol_face_, size_px * scale_, max_texture_size_, &err, slots)` then `upload()`. On failure, log one `[FontAtlas] Could not bake symbols …` line and keep `atlas = nullopt`, so those glyphs draw the placeholder. Keep `slots`, so it does not rebake every frame.
  8. `draw`: pass `symbol_face()` and `symbol_atlas_for(style.size_px, text)` to `for_each_text_quad`. In the emit lambda pick `quad.fallback ? symbol_atlas->texture : texture`. Do **not** reorder quads by texture: the shadow-then-colour order must hold, and `GlQuadRenderer` flushes on a texture change (`gl_quad_renderer.cpp:218-222`), which is correct, just one extra flush per symbol.
- **Mirror**: `bake_slot` / `atlas_for` (`src/render/ttf_font.cpp:883-930`), `load_paths` (789-805)
- **Validate**: build, then the full sandboxed ctest. Only the Task 5 count checks and the `test_corrupt_and_missing_files` log-count check should be red (its temp root has no symbol font, so it gets one extra log line).

### Task 5: Tests

- **File**: `tests/ttf_font_test.cpp`
- **Implement**:
  1. **Fix existing expectations**:
     - `test_atlas_bake` (~377): count `present` only over `slot < kLatinGlyphCount` and keep `== kLatinGlyphCount`.
     - The Audiowide loop (~416-424): iterate to `kLatinGlyphCount`.
     - `test_corrupt_and_missing_files` (~274-292): also copy `theme::kSymbolFontFile` into the temp `root` so the log still has exactly one line. Add a second sub-case: a root **without** the symbol font loads (`load(root)` true) and logs exactly one `"Symbol font unavailable"` line. Then `covers_text("\xE2\x98\xBA", SairaBold)` is **false** and `measure` of it equals `0.6 * size * s` (the placeholder).
  2. **`baked_glyph_slot`** (~348-357): add `0x2190 → 319`, `0x21FF → 430`, `0x2200 → -1`, `0x25A0 → 431`, `0x2600 → 527`, `0x263A → 585`, `0x26FF → 782`, `0x2700 → 783`, `0x27BF → 974`, `0x27C0 → -1`. Pin `kBakedGlyphCount == 975`.
  3. **New section `test_symbol_fallback()`** (call from `main`):
     - `FontFace::from_file(kSourceDir / theme::kSymbolFontFile, &err)` with the default probe fails, and `err` mentions `'A'`. With `kSymbolProbeCodePoint` it succeeds. The face `has(baked_glyph_slot(0x263A))`. All 256 slots of U+2600–26FF are present (pins the merge). SairaBold does **not** have slot(U+263A).
     - `measure_text(saira, &symbol, "☺", 24, 0)` == `symbol.advance_units(slot) * symbol.em_scale(24)` and != `0.6 * 24`. With `nullptr` fallback == `0.6 * 24`.
     - No kerning across faces: `m("A☺V") == m("A") + m("☺") + m("V")` (per-glyph sums, tracking 0).
     - U+2605 ★ now measures as the symbol glyph, not as `*`.
     - Emoji U+1F600 (`F0 9F 98 80`) and CJK still measure 0.6 em. Truncated `E2 98` (U+FFFD) is 0.6 em.
     - `FontAtlas::bake(symbol, 24, 4096, &err, {slot(263A)})`: only that slot (and the placeholder) is present. The glyph has a non-zero quad, `width <= 256`, and it fits within `max_dim`.
     - `for_each_text_quad(saira, saira_atlas, &symbol, &symbol_atlas, "A☺", …)` emits 2 colour quads. The second has `fallback == true` with UVs in [0,1], and its left x is `>= pen after 'A'`. With `fallback_atlas == nullptr` the second quad is the placeholder (`fallback == false`, same x).
     - `TextRenderer` loaded from `kSourceDir`: `symbol_font_available()`, `covers_text("☺", SairaBold)` **true**, `covers_text("☺", Audiowide)` true, and `covers_text("\xF0\x9F\x98\x80", SairaBold)` false. `measure("☺", kArtist)` at 1280x720 is not the placeholder width.
     - `std::cout << "  - symbol fallback (slots, probe, measure, bake, quads, coverage) ok.\n";`
  4. **Fuzz** (`test_fuzz`, ~675): additionally run each random string through `for_each_text_quad` with the symbol face and a small symbol atlas (e.g. baked for `{slot(263A), slot(2605)}`), and assert finite quads. Also mix in random bytes biased toward the `E2 98 xx` / `E2 9C xx` prefixes so that symbol code points actually occur.
- **File**: `tests/select_art_test.cpp` (~380-412)
  - Add `delirium` metadata: `artist = "\xE2\x98\xBA"`, `artist_translit = ""`, and a variant with `artist_translit = "Smiley"`. In both, `&song_display_artist(m, &text, font) == &m.artist` (native chosen because covered). The bitmap rule (`nullptr`) still picks the translit when present (`font_covers_text("☺")` is false, `bitmap_font_test.cpp:112`).
- **File**: `tests/theme_test.cpp` (~40): also check that `kSymbolFontFile` exists under the source dir.
- **Validate**: full sandboxed ctest, **51/51**.

### Task 6: README credit

- **File**: `README.md` (after line 97)
- **Implement**: `- The Noto Sans Symbols subset (\`assets/fonts/NotoSansSymbols-Subset.ttf\`, built from Noto Sans Symbols and Noto Sans Symbols 2 by \`scripts/build-symbol-font.sh\`) is distributed under the [SIL Open Font License 1.1](assets/fonts/OFL-NotoSansSymbols.txt).`
- **Validate**: `grep -n "NotoSansSymbols" README.md`

### Task 7: TODO tick

- **File**: `TODO.md:48`
- **Implement**: `- [ ]` → `- [x]` on the line tagged `(#124)`. Do **not** touch `.agents/issues/todo-issues.md`.
- **Validate**: `git diff TODO.md` shows that one line only.

---

## Validation

```bash
# Build (host, existing Release build dir)
cmake --build build -j$(nproc)

# Lint: no linter is configured. Gate on zero new compiler warnings in touched TUs:
touch src/render/ttf_font.cpp tests/ttf_font_test.cpp tests/select_art_test.cpp tests/theme_test.cpp
cmake --build build -j$(nproc) 2>&1 | grep -iE "warning" | grep -E "ttf_font|select_art|theme_test" || echo "no new warnings"

# Tests (MUST run sandboxed: some tests open real audio hardware)
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure
```

Expected: **51/51 pass** (baseline 51/51 on `b95dcc1`; no executable is added).

Static checks:

```bash
# Timing / judgment / input / audio untouched
git diff --stat -- src/timing src/audio src/input src/gameplay src/chart   # expect empty
# The font is a real TrueType (not CFF/variable) and small
python3 -c "import sys;d=open(sys.argv[1],'rb').read();print(d[:4]==b'\x00\x01\x00\x00', len(d))" assets/fonts/NotoSansSymbols-Subset.ttf   # True, ~1e5
# The off-limits file is not staged
git diff --cached --name-only | grep -c todo-issues   # expect 0
# Fresh clone still builds and finds the font (asset is committed)
scripts/fresh-clone-check.sh --committed
```

## End-to-End Verification

1. **Automated (agent-runnable):** `ttf_font_test` pins the slot ranges, the probe, fallback measure, the lazy-bake subset and quad emission (fallback vs placeholder). `covers_text` is tested for U+263A both with and without the symbol font. `select_art_test` pins that Delirium's native artist is chosen. Fuzz covers malformed UTF-8 through the fallback path.
2. **`/verify` screenshots (AC 1, 2, 5).** Use the `verify` skill with the owner's packs (Delirium is not in the fixture pack). Optionally take the same shots on `main` first as a "before" pair.
   ```bash
   B=.claude/skills/verify/scripts/b4k.sh
   cmake --build build -j$(nproc)
   $B doctor
   R=/tmp/blaze4k-verify/124-symbols
   $B launch --run $R -- --songs "$PWD/songs"
   $B keys $R Return && $B wait-screen $R Select
   # Move Down through the "In The Groove" group until the log/screen shows Delirium highlighted
   # (check game.log for the highlighted song after each batch of Downs), then:
   $B shot $R 01-select-delirium        # artist line under the title shows a smiley, not a box
   grep -n "Symbol font\|TextRenderer\|FontAtlas" $R/game.log   # no "Symbol font unavailable", no symbol bake failure
   # Results (AC 2): Return to start the highlighted chart, press nothing, and let fail end the run
   $B keys $R Return && $B wait-screen $R Gameplay
   $B wait-screen $R Results 180 && $B keys $R wait:3
   $B shot $R 02-results-delirium       # top bar artist (kBarArtist 18px) shows the smiley
   $B stop $R
   ```
   Expected:
   - `01`: Delirium's artist renders as a ☺ glyph in the ice colour, about cap height and on the baseline, with no hollow box.
   - `02`: the same on the results top bar.
   - Other songs' text is unchanged. Spot-check a song with Latin-1 (é), which must still use the Saira glyph.

   Read every PNG you cite. Run `stop` even after a failed attempt. Long ITG Challenge charts take a while to drain life. If fail is off in options, abort with Escape and report AC 2 as covered by the unit tests plus `results_screen` using the same `song_display_artist` + `TextRenderer::draw` path.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| The issue suggests Noto Sans Symbols 2, but U+263A is **not** in it | Measured: ☺ is only in Noto Sans Symbols (1). The plan merges subsets of both (U+2600–26FF 256/256), and a test pins the full block | In scope |
| fonttools is not installed; the network is needed once | The script creates a throwaway venv and pins `fonttools==4.66.1` and the upstream commits + sha256. The built `.ttf` is committed, so builds and the game stay offline. If pip/network is unavailable, stop and report (do not use the host's variable font copy) | In scope |
| Atlas memory/cap blow-up if the whole subset is baked per style | Lazy per-size symbol atlases with only drawn code points, capped at 16 sizes × 64 glyphs. Measured: an eager bake would be 73 MB / over the cap at 4K | In scope |
| Untrusted simfile text forces many rebakes or huge atlases | Hard caps (Pinned Semantics 7). Rebake only when a new in-cap code point appears. Log once. The fuzz test is extended | In scope |
| Measure/draw drift (layout shifts when a symbol glyph is not baked) | Measure uses symbol metrics regardless of bake state. The draw substitutes the placeholder **at the same pen/advance** (Pinned Semantics 4), and a test pins this | In scope |
| `from_bytes` 'A' probe rejects the symbol face | Probe parameter (default 'A', symbol face uses U+263A). The existing error text for 'A' is unchanged | In scope |
| Saira now bakes a few extra symbol-range glyphs (←↑→↓↔↕, ◊) into every theme atlas | About 8 small glyphs; negligible memory. Existing count tests move to `kLatinGlyphCount` | In scope |
| ★/☆ change from `*` (fold) to the real star glyph | Intended (issue: "render as the real glyph"). The bitmap font still folds. Documented in Pinned Semantics 1 | In scope |
| Visual weight mismatch: Noto symbols are regular weight next to Saira Bold/ExtraBold, and wider than condensed letters | Accepted for a fallback (≈0.69 em tall ☺ matches the cap height). Owner can judge from the screenshot | Out of scope (flag) |
| Extra GL flushes from texture switches mid-string | One flush per symbol glyph, only in text that contains symbols. Negligible | In scope (accept) |
| Emoji (U+1F300+), CJK, arrows Noto lacks | Still the placeholder (or translit when present). Full CJK/emoji coverage is explicitly out of scope per the issue | Out of scope |
| Timing path touched (principle 1) | Render-only change. The `git diff --stat` check on timing/audio/input/gameplay/chart is in Validation | In scope |

---

## Decisions

1. **One merged subset font, not two faces.** One extra `FontFace`, one lookup, one license file. The Noto families share a copyright holder, repo and unitsPerEm (1000), so `pyftmerge` works (prototyped).
2. **Code point ranges:** U+2190–21FF (Arrows), U+25A0–25FF (Geometric Shapes: ■□▲●◆), U+2600–26FF (Misc Symbols, the issue's minimum), U+2700–27BF (Dingbats: ✓✗✝❤). These are the blocks the issue names ("Miscellaneous Symbols, and possibly Dingbats/arrows"), plus geometric shapes, which are common in pack titles. Slot growth (975) is bounded and cheap.
3. **Lazy per-size symbol atlases** instead of baking symbols into each theme atlas (see the footprint table).
4. **Symbol before fold** in the resolution order, so ★ draws as ★ rather than `*`.

---

## Open Questions

None blocks the work. The plan uses the proposed defaults.

1. **Commit the build script?** Default: **yes**, as `scripts/build-symbol-font.sh`, for OFL provenance and reproducible re-subsetting. The alternative is to document the commands in the README only.
2. **Range set.** Default: the four blocks above. The owner may want more (e.g. U+2000–206F punctuation such as †‡•‰, or U+2300–23FF ⌘⏩). Adding a range later is a one-line `kBakedRanges` change plus re-running the script.
3. **Symbol-before-fold for ★/☆.** Default: draw the real star. If the owner prefers the old `*` look for star-heavy titles, swap steps 2 and 3 of the resolution order for code points that have a fold.

---

## Acceptance Criteria

- [ ] Delirium's artist shows a ☺ glyph, not the placeholder box, in song select (`/verify` shot `01`)
- [ ] The same artist renders correctly on the results screen (`/verify` shot `02`, or the unit-test fallback described in E2E)
- [ ] Code points missing from every loaded font (emoji, CJK, U+FFFD from malformed UTF-8) still draw the placeholder box, and fuzzing does not crash
- [ ] Unit tests cover the glyph lookup (`baked_glyph_slot`, symbol face `has`, measure, quad emission) and `covers_text` for U+263A, with and without the symbol font
- [ ] Atlas memory stays within the existing cap: symbol glyphs bake lazily, bounded by 16 sizes × 64 glyphs
- [ ] OFL license file is bundled and the README credits the font
- [ ] Build has no new warnings. Sandboxed ctest passes 51/51
- [ ] No changes under `src/timing`, `src/audio`, `src/input`, `src/gameplay`, `src/chart`. `.agents/issues/todo-issues.md` is untouched
