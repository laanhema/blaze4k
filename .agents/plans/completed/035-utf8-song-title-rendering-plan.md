# Plan: Render Song Titles With Special Characters (#77)

## Summary

The 5x7 bitmap font (`src/render/bitmap_font.cpp:17-97`) has glyphs only for `0-9 A-Z a-z`, space and
`. % - + * [ ]`. `draw_text` walks the string **byte by byte** and skips unknown bytes, and
`text_width` returns `bytes * 6 * pixel`. As a result, apostrophes, `!`, `&`, `/`, `^` and similar
disappear from titles, and UTF-8 titles such as `VerTex²`, `VerTex³` or the artist `☺` turn into
gaps that are one cell wide per byte.

The fix has three layers:

1. **A font-agnostic text module** (`src/render/unicode_text.{hpp,cpp}`). It holds a bounds-checked,
   strict UTF-8 decoder (`next_code_point`). Malformed input becomes U+FFFD, and every call is
   guaranteed to make progress. The module also has `is_zero_width` (combining marks, ZWJ/ZWSP,
   variation selectors, BOM) and a one-to-one `fold_to_ascii` table (Latin-1 and Latin Extended-A
   accents, typographic quotes and dashes, fullwidth forms, superscript digits). This module carries
   over unchanged to the TrueType atlas in #55.
2. **The bitmap font.** It gains glyphs for **all printable ASCII** (0x20–0x7E) plus a hollow-box
   placeholder, and an O(1) ASCII index. `draw_text` and `text_width` iterate code points instead of
   bytes:
   - a native glyph is drawn as itself;
   - a foldable code point is drawn as its ASCII fold;
   - a zero-width mark is skipped and takes no cell;
   - anything else is drawn as the placeholder box.

   Every visible code point takes exactly one 6-pixel cell. A new `font_covers_text` reports
   whether every code point has a *native* glyph.
3. **Display-name selection** (`src/screens/song_display_text.{hpp,cpp}`). It uses
   `#TITLETRANSLIT` / `#ARTISTTRANSLIT` when the native text is not fully covered by the font and
   the translit is non-empty. Song select (info panel and wheel) and the results screen draw through
   it. High-score keys, library lookup and log lines keep the raw native title.

## User Story

As a player with real ITG/community packs
I want song titles and artists to show their punctuation and a readable form of non-ASCII characters
So that I can tell songs like "Don't Promise Me", "Sly/Fly/Badman" or "VerTex³" apart in song select

## Metadata

| Field | Value |
|-------|-------|
| Type | BUG_FIX |
| Complexity | MEDIUM |
| Systems Affected | `render/unicode_text` (new), `render/bitmap_font` (glyphs, code-point iteration, new queries), `screens/song_display_text` (new), `screens/select_screen` (3 draw sites), `screens/results_screen` (title/artist draw), `CMakeLists.txt`, tests (`unicode_text_test` and `bitmap_font_test`, both new; one `select_screen_test` case), `TODO.md` |
| GitHub Issue | #77 |

---

## Environment Findings

| Tool / Fact | Version / Path | Notes |
|-------------|----------------|-------|
| CMake | 4.4.3 (`~/.local/bin/cmake`) | `build/` already configured (Release, native GCC); `cmake --build build -j$(nproc)` is incremental |
| C++ compiler | GCC 16.2.1 (`/usr/bin/g++`) | C++20, so `char32_t`, `std::string_view` and `std::array` are all available |
| Baseline tests | **39/39 pass** | `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` gives "100% tests passed out of 39" (0.36 s) on `main` @ `3845af3` |
| Font API | `src/render/bitmap_font.hpp:13-24` | `text_width`, `draw_text` and `draw_text_centered`, all taking `const std::string&` |
| Font callers | 73 call sites (grep `draw_text\|text_width` under `src/`) | Almost all draw ASCII literals or numbers. Song text is drawn only at `select_screen.cpp:612,614,644` and `results_screen.cpp:143,146` |
| Existing font tests | `tests/screen_manager_test.cpp:388-402` (`test_font_sanity`) | Asserts `text_width("A",1)==6`, `("AB",2)==24`, and `size()*6*pixel` for ASCII. These must stay green, and they will because ASCII stays one cell per byte |
| Width consumers | `hud_renderer.cpp:46,51,144,156`, `input_remap_screen.cpp:189-191`, `select_screen.cpp:555`, `tests/hud_renderer_test.cpp:183` | All ASCII, so their widths do not change |
| Translit fields | `src/chart/song_metadata.hpp:11-13`, parsed at `src/chart/simfile_parser.cpp:82-87` | Stored but never displayed today |
| MSD escapes | `src/chart/msd_file.cpp:112-116` | `\:` and `\;` are already unescaped. For example, the fixture `Glacier Groove.sm` has `#TITLE:Glacier\:Groove\;Part 1;`, so the title contains a real `:` and `;`, which now render |
| Real data (local `songs/`, gitignored) | ITG 1/2/3 packs | `#TITLE:VerTex²;` (no translit), `#TITLE:VerTex³;` + `#TITLETRANSLIT:VerTex^3;`, `#ARTIST:☺;` + `#ARTISTTRANSLIT:Smiley;` (several songs), `Don't Promise Me`, `Hit 'N' Hide`, `Parker/Stiles`, `Eskimo & Icebird`, `Summer ~Speedy Mix~`, `!` |
| GL in tests | `gl_quad_renderer.hpp` | A default-constructed renderer is a headless no-op. Glyph choice must be tested through pure query functions, not by drawing |
| Test registration | `tests/CMakeLists.txt:187-195` | `add_executable` + `target_link_libraries(... blaze4k_core)` + `add_test` |
| Core lib sources | `CMakeLists.txt:80-135` | An explicit list. New `.cpp` files must be added there, with `src/render/bitmap_font.cpp` at line 100 |

**Start green, stay green:** 39 tests pass now. This plan adds two test targets (`unicode_text_test`
and `bitmap_font_test`) and one case to `select_screen_test`, so **41/41** are expected afterwards.

---

## Value Provenance (StepMania 5 semantics)

Reference: `stepmania/stepmania` branch `5_1-new` @ commit `825467bcd81c812b33ad684dc04dd151b2d5dec3`
(fetched with `gh api` into the scratchpad; nothing is vendored).

| Behaviour | Upstream | Blaze 4k decision |
|-----------|----------|-------------------|
| SM/SSC `#TITLE`/`#ARTIST`/`*TRANSLIT` are stored raw, with no code-page conversion | `src/NotesLoaderSM.cpp:41-63` (plain assignments). `ConvertString("utf-8,english")` is only used by the DWI, KSF and BMS loaders (`RageUtil_CharConversions.cpp:130-170`, GitHub code search) | Treat title bytes as UTF-8 and add **no** CP1252/Shift-JIS fallback, matching SM for `.sm`/`.ssc` |
| An invalid UTF-8 sequence becomes U+FFFD, and decoding resyncs at the unexpected byte | `src/RageUtil.cpp:1576-1636` (`utf8_to_wchar_ec`: a misplaced continuation byte advances 1, a missing continuation advances to the offending byte), `:1731-1745` (`utf8_sanitize` → `INVALID_CHAR`), `:1931` (`INVALID_CHAR = 0xFFFD`) | The same resync rule. We are stricter than SM (we also reject overlongs, surrogates and values above U+10FFFF), and each rejected code point → U+FFFD |
| A code point with no glyph is drawn with the font's default glyph, not skipped | `src/Font.cpp:345-380` (`GetGlyph` falls back to `FONT_DEFAULT_GLYPH`), `src/Font.h:235` (`0xF8FF`) | Draw a visible placeholder box in one cell |
| Translit display | `src/Song.cpp:1863-1879` (`GetDisplay*` returns translit only when the `ShowNativeLanguage` pref is off), `src/Song.h:212-229` (`GetTranslit*` falls back to native when translit is empty) | Blaze 4k has no such pref. Per the issue AC, translit is chosen **automatically** when the native text has a code point without a native glyph **and** the translit is non-empty. Otherwise native is used. This is issue-driven, not an SM parity value |

ASCII-folding tables and glyph bitmaps are Blaze 4k presentation choices (unsourced, with no OpenITG
parity requirement).

---

## Pinned Semantics

- **Cell rule.** Each code point that is not zero-width takes exactly **one** cell (`6 * pixel`). For
  ASCII this is the same as today, so `text_width` is unchanged for all existing UI strings.
  Zero-width code points take **zero** cells and draw nothing.
- **Resolution order per code point** (`resolve_glyph`):
  1. A zero-width code point is skipped.
  2. A code point in 0x20–0x7E draws its native glyph.
  3. Otherwise, if `fold_to_ascii(cp)` is non-zero and has a native glyph, that glyph is drawn.
  4. Otherwise the placeholder box is drawn.

  C0 controls (0x00–0x1F) and DEL fold only for TAB (→ space). The rest draw the placeholder.
- **Decoder (`next_code_point(text, pos)`).** Precondition: `pos < text.size()`. Postcondition: `pos`
  strictly increases and never passes `text.size()`. Valid sequences follow Unicode Table 3-7
  (well-formed byte sequences):

  | Lead byte | Length | Allowed 2nd byte |
  |-----------|--------|------------------|
  | `00–7F` | 1 | — |
  | `C2–DF` | 2 | `80–BF` |
  | `E0` | 3 | `A0–BF` |
  | `E1–EC`, `EE–EF` | 3 | `80–BF` |
  | `ED` | 3 | `80–9F` |
  | `F0` | 4 | `90–BF` |
  | `F1–F3` | 4 | `80–BF` |
  | `F4` | 4 | `80–8F` |

  Bytes after the 2nd are always `80–BF`.

  Errors return `kReplacementChar` (U+FFFD):
  - An invalid lead byte (`80–C1`, `F5–FF`) advances **1**.
  - A bad or missing continuation at index *i* advances to *i*, which resyncs at the offending
    byte as in SM.
  - Truncation at the end of the string advances to `text.size()`.

  There is no allocation and no exceptions.
- **Coverage.** `font_covers_text(s)` is true iff every non-zero-width code point is in 0x20–0x7E.
  Folds do **not** count as coverage, so an author-provided translit beats an automatic fold:
  `VerTex³` shows as `VerTex^3`, not `VerTex3`. `""` is covered.
- **Display choice.** `select_display_text(native, translit, native_covered)` returns `translit` iff
  `!native_covered && !translit.empty()`, else `native`. An empty native title with a translit shows
  native (empty), as SM does when the native-language pref is on.
- **Identity untouched.** `metadata.title`/`artist` stay raw. High-score keys
  (`high_scores.cpp:144-146`), `SongLibrary::find_song` (`song_library.cpp:96`) and all `std::cout`
  log lines keep the native bytes. The display helpers are used **only** for drawing.

### Fold table (`fold_to_ascii`, 0 = no fold)

| Range | Mapping |
|-------|---------|
| U+0009 TAB | `' '` |
| U+00A0 NBSP | `' '` |
| U+00A1 `¡` / U+00BF `¿` | `'!'` / `'?'` |
| U+00AB `«` / U+00BB `»` | `'<'` / `'>'` |
| U+00B4 `´` | `'\''` |
| U+00B2 / U+00B3 / U+00B9 (superscripts) | `'2'` / `'3'` / `'1'` |
| U+00C0–U+00FF | Lookup string, 16 per row, where `*` means no fold: `"AAAAAA*CEEEEIIII"`, `"DNOOOOOxOUUUUY**"`, `"aaaaaa*ceeeeiiii"`, `"dnooooo*ouuuuy*y"` (Æ, Þ, ß, æ, ÷ and þ get the placeholder) |
| U+0100–U+017F (Latin Ext-A) | `"AaAaAaCcCcCcCcDd"`, `"DdEeEeEeEeEeGgGg"`, `"GgGgHhHhIiIiIiIi"`, `"Ii**JjKkkLlLlLlL"`, `"lLlNnNnNnnNnOoOo"`, `"Oo**RrRrRrSsSsSs"`, `"SsTtTtTtUuUuUuUu"`, `"UuUuWwYyYZzZzZzs"` (Ĳ, ĳ, Œ and œ get the placeholder) |
| U+2010–U+2015 (hyphens/dashes) | `'-'` |
| U+2018–U+201B (single quotes) / U+2032 `′` | `'\''` |
| U+201C–U+201F (double quotes) / U+2033 `″` | `'"'` |
| U+2026 `…` | `'.'` (one cell; see Open Question 3) |
| U+2605 `★` / U+2606 `☆` | `'*'` |
| U+3000 ideographic space | `' '` |
| U+301C `〜` wave dash | `'~'` |
| U+FF01–U+FF5E (fullwidth ASCII) | `cp - 0xFEE0` |

In the lookup strings, implement `*` as "no fold" (0). Do **not** fold to a literal `*`.

### Zero-width set (`is_zero_width`)

U+0300–U+036F (combining diacritics, e.g. NFD `e`+U+0301), U+200B–U+200F (ZWSP, ZWNJ, ZWJ, LRM,
RLM), U+2060 (word joiner), U+FE00–U+FE0F (variation selectors, e.g. emoji VS16), U+FEFF (BOM).

### New glyphs (5x7, bit 4 = leftmost, same encoding as `kGlyphs`)

```
'!'  {0x04,0x04,0x04,0x04,0x04,0x00,0x04}   '"'  {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}
'#'  {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}   '$'  {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}
'&'  {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}   '\'' {0x0C,0x04,0x08,0x00,0x00,0x00,0x00}
'('  {0x02,0x04,0x08,0x08,0x08,0x04,0x02}   ')'  {0x08,0x04,0x02,0x02,0x02,0x04,0x08}
','  {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}   '/'  {0x00,0x01,0x02,0x04,0x08,0x10,0x00}
':'  {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}   ';'  {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}
'<'  {0x02,0x04,0x08,0x10,0x08,0x04,0x02}   '='  {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}
'>'  {0x08,0x04,0x02,0x01,0x02,0x04,0x08}   '?'  {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}
'@'  {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E}   '\\' {0x00,0x10,0x08,0x04,0x02,0x01,0x00}
'^'  {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}   '_'  {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}
'`'  {0x08,0x04,0x02,0x00,0x00,0x00,0x00}   '{'  {0x02,0x04,0x04,0x08,0x04,0x04,0x02}
'|'  {0x04,0x04,0x04,0x04,0x04,0x04,0x04}   '}'  {0x08,0x04,0x04,0x02,0x04,0x04,0x08}
'~'  {0x00,0x00,0x08,0x15,0x02,0x00,0x00}
placeholder (hollow box) {0x1F,0x11,0x11,0x11,0x11,0x11,0x1F}
```

That is 25 new glyphs. With the existing 70, all **95** printable ASCII characters are covered. The
placeholder's square corners distinguish it from `O` (0x0E rounded) and `0` (diagonal).

### Expected incidental UI changes (all improvements; mention in the PR)

Characters that were silently dropped before now render:
- `/` in the `[UP/DOWN]` and `[LEFT/RIGHT]` hints (`select_screen.cpp:574,679`, `input_remap_screen.cpp:228-229`), and in `"SAMPLES n / m"` and `"DP a/b"`;
- the `>` cursor in difficulty rows (`select_screen.cpp:671`) and the `>` action-row value (`options_menu.cpp:272`);
- `<PRESS>` (`input_remap.cpp:164`), `IN USE:` (`input_remap.cpp:82`), and `?` (`select_screen.cpp:79`).

Widths do not change, because these characters already took a cell as invisible glyphs.

---

## Patterns to Follow

### Anonymous-namespace glyph table + lookup (extend, keep format)
```cpp
// SOURCE: src/render/bitmap_font.cpp:11-17, 87-94
struct Glyph {
    char symbol;
    std::uint8_t rows[7];
};
constexpr Glyph kGlyphs[] = {
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    ...
const std::uint8_t* glyph_for(char symbol) {
    for (const Glyph& glyph : kGlyphs) {
        if (glyph.symbol == symbol) {
            return glyph.rows;
        }
    }
    return nullptr;
}
```

### Header doc-comment + `[[nodiscard]]` free functions
```cpp
// SOURCE: src/render/bitmap_font.hpp:13-20
// Pixel width of `text` at `pixel` scale (each glyph cell is 6*pixel wide).
[[nodiscard]] float text_width(const std::string& text, float pixel);

// Draws `text` with its top-left at (x, y). Unknown glyphs advance the cursor
// without drawing.
void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y, float pixel,
               Color color);
```

### Untrusted-input style (bounds checks, no exceptions, graceful degrade)
```cpp
// SOURCE: src/chart/msd_file.cpp:112-116
if (c == '\\' && i + 1 < len) {
    // Escape sequence
    current_param += content[i + 1];
    i += 2;
    continue;
}
```

### Tests (plain executable, `TEST_CHECK` abort macro, section prints)
```cpp
// SOURCE: tests/select_screen_test.cpp:30-37, 377-417
#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)
...
void test_empty_library() {
    blaze4k::SongLibrary empty_library;
    ...
    blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
    manager.render(renderer, 1280, 720);
    ...
    std::cout << "  - empty library is crash-free and inert ok.\n";
}
```

### Test registration
```cmake
# SOURCE: tests/CMakeLists.txt:187-195
add_executable(select_screen_test
    select_screen_test.cpp
)

target_link_libraries(select_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME select_screen_test COMMAND select_screen_test)
```

---

## Files to Change

| File | Action | Purpose |
|------|--------|---------|
| `src/render/unicode_text.hpp` | CREATE | Font-agnostic: `kReplacementChar`, `next_code_point`, `is_zero_width`, `fold_to_ascii` |
| `src/render/unicode_text.cpp` | CREATE | Implementations and fold tables (see Pinned Semantics) |
| `src/render/bitmap_font.hpp` | UPDATE | Update the doc comments (code points, placeholder). Add `has_glyph(char32_t)`, `glyph_rows(char32_t)` and `font_covers_text(std::string_view)` |
| `src/render/bitmap_font.cpp` | UPDATE | 25 new glyphs + placeholder, constexpr ASCII index, `resolve_glyph`, code-point `draw_text`/`text_width` |
| `src/screens/song_display_text.hpp` | CREATE | `select_display_text(...)`, `song_display_title(const SongMetadata&)`, `song_display_artist(const SongMetadata&)` |
| `src/screens/song_display_text.cpp` | CREATE | Implementations (wire `font_covers_text`) |
| `src/screens/select_screen.cpp` | UPDATE | Lines 612, 614 and 644 draw `song_display_title/artist(...)` |
| `src/screens/results_screen.cpp` | UPDATE | Lines 141-142 (render) use the display helpers. Line 71 (log) stays raw |
| `CMakeLists.txt` | UPDATE | Add `src/render/unicode_text.cpp` (next to `bitmap_font.cpp`, line 100) and `src/screens/song_display_text.cpp` (screens block) to `blaze4k_core` |
| `tests/unicode_text_test.cpp` | CREATE | Decoder, malformed/fuzz, zero-width and fold tests |
| `tests/bitmap_font_test.cpp` | CREATE | Glyph lookup, `text_width` on multi-byte input, coverage, display choice / translit fallback, headless draw smoke |
| `tests/CMakeLists.txt` | UPDATE | Register both new targets |
| `tests/select_screen_test.cpp` | UPDATE | One case: a library with punctuation, UTF-8, translit and malformed titles, navigated and rendered headlessly |
| `TODO.md` | UPDATE | Tick line 39 (`(#77)`) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Font-agnostic `unicode_text` module

- **Files**: `src/render/unicode_text.hpp`, `src/render/unicode_text.cpp` (CREATE), `CMakeLists.txt` (UPDATE)
- **Implement**:
  ```cpp
  #pragma once
  #include <cstddef>
  #include <string_view>
  namespace blaze4k {
  // Font-agnostic text helpers shared by the bitmap font and the future TrueType atlas (#55).
  // Simfile text is untrusted: every function is bounds-checked and never throws.
  inline constexpr char32_t kReplacementChar = 0xFFFD;

  // Decodes the code point starting at text[pos] and advances `pos` past it.
  // Precondition: pos < text.size(). Always advances by >= 1 byte and never past
  // text.size(). Malformed or truncated sequences, overlongs, surrogates and
  // values > U+10FFFF return kReplacementChar (resync at the offending byte).
  [[nodiscard]] char32_t next_code_point(std::string_view text, std::size_t& pos);

  // True for code points that draw nothing and take no cell (combining marks,
  // zero-width joiners/spaces, variation selectors, BOM).
  [[nodiscard]] bool is_zero_width(char32_t cp);

  // One-to-one ASCII stand-in for a non-ASCII (or TAB) code point, or '\0' when
  // there is none. Never returns a multi-character expansion.
  [[nodiscard]] char fold_to_ascii(char32_t cp);
  }
  ```
  - Decoder per the Pinned Semantics table. Implement with `static_cast<unsigned char>` reads,
    checking `pos + i < text.size()` before every continuation read. If `pos >= text.size()` (a
    precondition violation), set `pos = text.size()` and return `kReplacementChar`, a defensive
    no-hang path.
  - Fold tables are `constexpr const char*` row strings exactly as listed. Map a `*` entry to
    `'\0'`. Return `'\0'` for code points below 0x80 except TAB. Plain ASCII is never folded, since
    the font handles it natively.
  - Add `src/render/unicode_text.cpp` to `blaze4k_core` after `src/render/bitmap_font.cpp`
    (`CMakeLists.txt:100`).
- **Mirror**: `src/chart/msd_file.cpp:112-116` (bounds-checked index reads).
  `bitmap_font.hpp` (header doc style).
- **Validate**: `cmake --build build -j$(nproc)`

### Task 2: Bitmap font — full ASCII, placeholder, code-point iteration

- **Files**: `src/render/bitmap_font.hpp`, `src/render/bitmap_font.cpp`
- **Implement**:
  - Header additions (keep the existing three signatures, `const std::string&`, so the 73 call
    sites are untouched):
    ```cpp
    // True when `cp` has a native glyph (printable ASCII 0x20-0x7E).
    [[nodiscard]] bool has_glyph(char32_t cp);
    // The 7 row bitmaps draw_text uses for `cp`: native glyph, else its ASCII fold,
    // else the placeholder box. nullptr for zero-width code points (no cell).
    [[nodiscard]] const std::uint8_t* glyph_rows(char32_t cp);
    // True when every non-zero-width code point of UTF-8 `text` has a native glyph
    // (folds and placeholders do not count). Empty text is covered.
    [[nodiscard]] bool font_covers_text(std::string_view text);
    ```
    Update the `text_width`/`draw_text` comments: the text is UTF-8, each visible code point is one
    cell, and undrawable code points draw a placeholder box.
  - `.cpp`:
    1. Append the 25 glyphs from Pinned Semantics to `kGlyphs`, keeping the `{symbol, rows}`
       format. Add `constexpr std::uint8_t kPlaceholderRows[7] = {0x1F,0x11,0x11,0x11,0x11,0x11,0x1F};`.
    2. Build a constexpr ASCII index `std::array<const std::uint8_t*, 95>` (or an index array of
       `int`) from `kGlyphs` with a `consteval`/`constexpr` builder. Add a `static_assert` that all
       95 slots are filled and that `std::size(kGlyphs) == 95`, so no slot is missing or duplicated.
       Lookup becomes O(1), replacing the linear `glyph_for`.
    3. Write `resolve_glyph(char32_t)` following the resolution order in Pinned Semantics.
       `glyph_rows` exposes it.
    4. `text_width`: loop `while (pos < text.size()) { cp = next_code_point(text, pos); if
       (!is_zero_width(cp)) ++cells; }` and return `cells * 6 * pixel`.
    5. `draw_text`: the same loop. Skip zero-width code points without advancing. Otherwise draw
       `resolve_glyph(cp)` rows (the existing inner loops) and advance `6 * pixel`.
    6. `draw_text_centered` is unchanged; it already uses `text_width`.
- **Mirror**: `src/render/bitmap_font.cpp:11-125` (keep structure and naming)
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build -R "screen_manager_test|hud_renderer_test" --output-on-failure`
  (the existing ASCII width assertions stay green)

### Task 3: Display-name selection helper

- **Files**: `src/screens/song_display_text.hpp`, `src/screens/song_display_text.cpp` (CREATE),
  `CMakeLists.txt` (UPDATE: add to the screens block)
- **Implement**:
  ```cpp
  // Display text for song metadata (draw-only; identity/high-score keys stay raw).
  // Pure choice: translit when the native text is not fully drawable and a
  // translit exists; otherwise native.
  [[nodiscard]] const std::string& select_display_text(const std::string& native,
                                                       const std::string& translit,
                                                       bool native_covered);
  // TITLE / TITLETRANSLIT and ARTIST / ARTISTTRANSLIT through the active font's coverage.
  [[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata);
  [[nodiscard]] const std::string& song_display_artist(const SongMetadata& metadata);
  ```
  The returned reference aliases `metadata`, so callers must not bind it past the song's lifetime.
  All current callers use it within one draw call. `song_display_*` call
  `font_covers_text(native)`. When #55 lands, only that call changes.
- **Mirror**: free functions in `src/screens/results.hpp` / `select_screen.hpp`
  (`format_bpm_range`, `difficulty_color`)
- **Validate**: `cmake --build build -j$(nproc)`

### Task 4: Use display names in song select and results

- **Files**: `src/screens/select_screen.cpp`, `src/screens/results_screen.cpp`
- **Implement**:
  - `select_screen.cpp:612` → `song_display_title(song->metadata)`. `:614` →
    `song_display_artist(song->metadata)`. `:644` →
    `item.song != nullptr ? song_display_title(item.song->metadata) : std::string{}`. Because the
    ternary would copy, hoist the value into a local `const std::string& title = ...` with an
    empty-string static, or keep the ternary yielding a `std::string` copy. Either is fine.
  - `results_screen.cpp:141-142`: build `title`/`artist` from the display helpers, keeping the
    `"UNKNOWN"` / `""` fallbacks. **Do not** change line 71 (log) or anything in `high_scores`.
- **Mirror**: existing draw calls at those lines
- **Validate**: `cmake --build build -j$(nproc)` and the full ctest (still 39/39)

### Task 5: `unicode_text_test` (new target)

- **Files**: `tests/unicode_text_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Implement**: Use `TEST_CHECK`, one `test_*` per case, each printing `"  - ... ok."`. Add a
  helper `decode_all(std::string_view) -> std::vector<char32_t>` that loops `next_code_point` and
  asserts progress on every step.
  1. **ASCII and multi-byte decode**:
     - `"Don't"` decodes to 5 code points equal to its bytes;
     - `"\xC2\xB3"` → U+00B3;
     - `"\xE2\x98\xBA"` → U+263A;
     - `"\xF0\x9F\x8E\xB5"` → U+1F3B5;
     - `"VerTex\xC2\xB3"` → 7 code points.
  2. **Malformed → U+FFFD with correct resync**:
     - a lone continuation byte: `"\x80"` → `{FFFD}`, `"a\x80b"` → `{a, FFFD, b}`;
     - invalid lead bytes: `"\xFF"`, `"\xC0\xAF"` (overlong → 2 × FFFD, since C0 is an invalid
       lead and AF is a stray continuation);
     - a surrogate: `"\xED\xA0\x80"` → FFFD + 2 strays, i.e. 3 × FFFD (ED only allows 80–9F);
     - above U+10FFFF: `"\xF4\x90\x80\x80"` → 4 × FFFD;
     - truncation at the end: `"\xE2\x98"` → `{FFFD}` with pos == 2;
     - a missing continuation mid-string: `"\xE2(" `→ `{FFFD, '('}`, which resyncs and keeps `(`.
  3. **No hang / bounds fuzz**: seed `std::mt19937{77}`, generate 10 000 random strings of 0–64
     bytes, run `decode_all`, and assert that `pos` strictly increases, ends at `size()`, and the
     count is ≤ the byte count. Also decode every 1- and 2-byte combination (65 536 + 256 inputs).
  4. **Zero-width**: U+0301, U+200D, U+FE0F and U+FEFF are zero-width. `'a'`, U+00E9 and U+263A
     are not.
  5. **Fold**:
     - `é`→`e`, `Ñ`→`N`, `ł`→`l`, `Ž`→`Z`, U+2019→`'`, U+201C→`"`, U+2014→`-`, U+FF21→`A`,
       U+3000→`' '`, U+00B3→`'3'`, U+2606→`'*'`, TAB→`' '`;
     - no fold: `Æ`, `ß`, `Œ` (all → `'\0'`), U+263A → `'\0'`, CJK U+65E5 → `'\0'`, plain `'A'`
       → `'\0'`;
     - table integrity: for every cp in U+00C0–U+017F, the fold is `'\0'` or an ASCII letter
       (catches row-length typos).
  - Register the target in `tests/CMakeLists.txt` after `select_screen_test` (link `blaze4k_core`).
- **Mirror**: `tests/select_screen_test.cpp:30-37`, `tests/CMakeLists.txt:187-195`
- **Validate**: `cmake --build build -j$(nproc) && bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build -R unicode_text_test --output-on-failure`

### Task 6: `bitmap_font_test` (new target)

- **Files**: `tests/bitmap_font_test.cpp` (CREATE), `tests/CMakeLists.txt` (UPDATE)
- **Implement** (AC 5):
  1. **Punctuation glyph lookup**: for every cp in 0x20–0x7E, `has_glyph(cp)` holds and
     `glyph_rows(cp) != nullptr`. For each of `' ! ? & ( ) : / , " # _ = ~ ^`,
     `glyph_rows(c) != glyph_rows(kReplacementChar)` and the rows are non-blank (any bit set).
     `has_glyph(0x7F)`, `has_glyph(0x1F)` and `has_glyph(0xE9)` are false.
  2. **Fold and placeholder**: `glyph_rows(U+00E9) == glyph_rows('e')`,
     `glyph_rows(U+2019) == glyph_rows('\'')`, and
     `glyph_rows(U+263A) == glyph_rows(U+65E5) == glyph_rows(kReplacementChar)` (placeholder,
     non-null). `glyph_rows(U+0301) == nullptr` (zero-width).
  3. **`text_width` on multi-byte input**:
     - `text_width("VerTex\xC2\xB3", 1) == 7*6`;
     - `text_width("\xE2\x98\xBA", 2) == 12`, one cell and not three;
     - `text_width("e\xCC\x81", 1) == 6`, since the combining mark takes no cell;
     - `text_width("\xFF\xFE", 1) == 12`, two placeholders;
     - `text_width("Don't Promise Me", 1) == 16*6`;
     - an ASCII regression check: `text_width(s, p) == s.size()*6*p` for a few UI strings.
  4. **Coverage**: `font_covers_text` is true for `"Don't Promise Me"`, `"Sly/Fly/Badman"`,
     `"Summer ~Speedy Mix~"`, `"Glacier:Groove;Part 1"` and `""`. It is false for
     `"VerTex\xC2\xB3"`, `"\xE2\x98\xBA"`, `"Caf\xC3\xA9"` (fold is not coverage) and `"a\xFF"`.
  5. **Translit fallback** (AC 3), through `song_display_title/artist` with hand-built
     `SongMetadata`:
     - `VerTex³` + translit `VerTex^3` → `VerTex^3`;
     - `VerTex²` + empty translit → native;
     - `Don't Promise Me` + translit `X` → native, because a fully covered native wins;
     - artist `☺` + translit `Smiley` → `Smiley`;
     - artist `KaW feat. ☺` + empty translit → native;
     - `select_display_text("a","b",true) == "a"` and `select_display_text("","b",false) == "b"`.
  6. **Malformed draw smoke** (AC 4): with a headless `GlQuadRenderer`, call `draw_text` and
     `draw_text_centered` on the 10 000 seeded random byte strings from Task 5's generator, plus a
     4 KB string of `0xE2` bytes. This must return without crashing or hanging, and each `text_width`
     must be finite and ≤ `bytes * 6 * pixel`.
  - Register it after `unicode_text_test`.
- **Mirror**: `tests/screen_manager_test.cpp:388-402` (headless font calls)
- **Validate**: `... ctest --test-dir build -R bitmap_font_test --output-on-failure`

### Task 7: `select_screen_test` integration case

- **File**: `tests/select_screen_test.cpp`
- **Implement**: Add `test_special_character_titles()`, modelled on `test_empty_library`
  (`:377-417`), with its own temp root (`blaze4k_select_screen_utf8_test`) and library.
  - Write 4 songs with `make_sm`. Extend it with an optional `extra_tags` string parameter that
    defaults to empty, so existing calls are unchanged.
    - `"Don't Promise Me"`;
    - `"VerTex\xC2\xB3"` with `#TITLETRANSLIT:VerTex^3;` and `#ARTISTTRANSLIT:Smiley;`;
    - a malformed `"Bad\xC3\x28\xFF Title"`;
    - `"Glacier\\:Groove"` (an escaped colon).
  - Open `std::ofstream` in `std::ios::binary` for exact bytes. The existing `write_file` uses text
    mode, which preserves bytes on Linux, so add a binary flag to be safe on Windows.
  - Assert `library.total_songs() == 4` and that the parser preserved raw bytes: some song's
    `metadata.title == "VerTex\xC2\xB3"` and its `song_display_title` is `"VerTex^3"`.
  - Start the select screen, then `Down` × 4 with `manager.render(renderer, 1280, 720)` after each
    step (headless). Also render at `640x480`. Nothing may crash.
  - Call it from `main` after `test_empty_library()`.
- **Mirror**: `tests/select_screen_test.cpp:52-76, 377-417`
- **Validate**: `... ctest --test-dir build -R select_screen_test --output-on-failure`

### Task 8: Tick the TODO entry

- **File**: `TODO.md`. In line 39, change `- [ ]` to `- [x]` and keep `(#77)`, matching lines 30-38.

### Task 9: Full validation and smoke

- Run the Validation block. Expect a warning-free build, **41/41** tests, and a clean headless
  smoke run.

---

## Validation

```bash
# Build (incremental, Release, native GCC)
cmake --build build -j$(nproc)
# (fresh configure if needed: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc))

# Lint: no linter configured in this repo; compiler warnings from the build are the gate (no new warnings)

# Tests: must run inside the sandbox (tests open real audio hardware). Expect 100% of 41.
bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure

# Debug build sanity (optional; catches assert-only issues and UB in the decoder)
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug -j$(nproc)
```

## End-to-End Verification

1. **Automated (headless, required)**:
   - `unicode_text_test` covers decoding, malformed input and resync, a 10k-case fuzz, zero-width
     handling and folds.
   - `bitmap_font_test` covers punctuation glyphs, placeholder/fold resolution, per-code-point
     `text_width`, coverage and translit choice.
   - `select_screen_test` drives the real `SongLibrary` → `SimfileParser` → `SelectScreen::render`
     path with apostrophe, UTF-8 with translit, malformed and escaped-colon titles.
2. **App smoke (headless, required)**:
   `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ./build/blaze-4k --headless --smoke-test 10`
   exits 0. Never launch a windowed or interactive binary from the agent.
3. **Owner play-test (interactive; the agent does not run it)**, with the local ITG packs in
   `songs/`. In song select:
   - "Don't Promise Me", "Groovin' Motion" and "Hit 'N' Hide" show apostrophes. "Sly/Fly/Badman"
     and "Parker/Stiles" show slashes. "Eskimo & Icebird" shows `&`. "Summer ~Speedy Mix~" shows
     tildes. The title `!` renders.
   - "VerTex³" (ITG3) shows **`VerTex^3`** (translit). "VerTex²" (ITG2, no translit) shows
     `VerTex2` (fold).
   - Artists `☺` show **`Smiley`** where `#ARTISTTRANSLIT:Smiley;` exists. Otherwise
     (e.g. "KaW feat. ☺" with empty translit), a hollow box shows in one cell.
   - Hints now read `[UP/DOWN]` and `[LEFT/RIGHT]`, and the `>` cursor shows on the selected
     difficulty row.
   - Play any of these songs to the results screen: the title and artist show the same display
     text. Best scores recorded **before** this change are still shown for these songs, which
     confirms the high-score key is unchanged.

---

## Risks

| Risk | Mitigation | Scope |
|------|------------|-------|
| Decoder bug → OOB read or infinite loop on untrusted bytes | Every continuation read is guarded by an explicit `pos + i < size()`. The progress postcondition is asserted in tests, and there is a 10k-case seeded fuzz plus exhaustive 1- and 2-byte inputs. The Debug build is optional | In scope |
| Changing displayed titles accidentally changes high-score identity | Display helpers are draw-only. `high_scores.cpp:144-146`, `song_library.cpp:96` and the logs are untouched. The owner play-test confirms old bests still show | In scope |
| Fold table typos (wrong row length or offset) | The table-integrity test (every U+00C0–U+017F code point folds to 0 or an ASCII letter) plus spot checks across rows | In scope |
| Newly visible `/`, `>`, `:`, `<` in existing UI hints | Intended improvement. Widths are unchanged. Listed in the PR description | In scope (flag) |
| Long titles overflow the wheel or info panel (no truncation exists today) | Unchanged by this fix. Multi-byte titles actually get *narrower*. A future story can add ellipsis truncation | Out of scope (flag only) |
| The wheel is sorted or searched by raw title, so translit display can look out of order | Today's order is unchanged (scan order). Sorting by display name is a separate feature | Out of scope (flag only) |
| `#SUBTITLE`/`#SUBTITLETRANSLIT` not displayed | Subtitles are not drawn anywhere today. The helper pattern extends trivially | Out of scope |
| CJK-only titles with no translit render as a row of boxes | This matches the AC ("visible placeholder"). Native CJK rendering depends on #55's atlas coverage | Out of scope (flag) |
| Non-UTF-8 legacy files (CP1252/Shift-JIS) show placeholders | SM5 does not convert `.sm`/`.ssc` titles either (`NotesLoaderSM.cpp:41-63`). Each invalid byte → one placeholder, with no crash | Out of scope (parity) |

---

## Decisions

- **Font-agnostic split.** Decode, zero-width and fold live in `render/unicode_text` with no font
  knowledge. Coverage (`font_covers_text`) and glyph resolution live in the font. Display choice is
  a pure function plus thin song wrappers in `screens/`. #55 replaces only the font layer.
- **Full printable ASCII, not just the AC minimum.** It costs 25 bitmaps and removes a whole class
  of "silently missing character" bugs, including the hint text and the `>` cursor.
- **One-to-one folds only.** This keeps the "one code point = one cell" invariant (AC 2) and makes
  `text_width` a simple count. Multi-letter expansions (Æ→AE, ß→ss, …→...) are deliberately left as
  the placeholder or a single character.
- **Translit beats fold.** Coverage only counts native glyphs, so an author's translit
  (`VerTex^3`, `Smiley`) is preferred over an automatic fold. Folds are the fallback when there is
  no translit.
- **Keep the `const std::string&` API.** This avoids touching 73 call sites. `std::string_view`
  is used only internally and in the new functions.
- **Two new test targets.** `unicode_text_test` survives the #55 font swap unchanged, while
  `bitmap_font_test` is bitmap-specific.

---

## Open Questions

1. **Translit trigger.** Proposed: use translit only when the native text has a code point without a
   *native* glyph (folds do not count) and the translit is non-empty. The alternative is "only when
   a placeholder would be drawn", which would show `VerTex3` instead of `VerTex^3`. Default chosen
   because author-supplied text is more faithful than an automatic fold.
2. **Placeholder glyph.** Proposed: a full-height hollow box (classic "tofu"), as in SM's
   default-glyph fallback. `?` was rejected because it is a legitimate title character, for example
   the title `!` and "Why?".
3. **Ellipsis and multi-letter characters (`…`, `Æ`, `ß`, `Œ`).** Proposed: `…` → `.` (one cell), and
   the others → placeholder, to keep the one-cell invariant. If the owner prefers `...`/`AE`/`ss`,
   `text_width` would have to count fold length. That is a contained change, but it relaxes AC 2's
   wording.
4. **Glyph shapes.** The 25 new 5x7 bitmaps are unsourced presentation choices. They can be tuned
   after the owner's visual check.

---

## Acceptance Criteria

- [ ] Printable ASCII punctuation (at least `' ! ? & ( ) : / , " # _ = ~`, in fact all of 0x20–0x7E) renders in song select titles and artists ("Don't Promise Me" shows its apostrophe)
- [ ] UTF-8 is decoded per code point. Undrawable characters show a placeholder box or a one-to-one ASCII fold, and every visible code point takes exactly one cell
- [ ] When `#TITLETRANSLIT`/`#ARTISTTRANSLIT` is present and the native text has characters without a native glyph, the translit is shown (song select and results)
- [ ] Malformed UTF-8 never crashes or hangs rendering (fuzz + select-screen render test)
- [ ] Unit tests cover punctuation glyph lookup, `text_width` on multi-byte input, and the translit fallback
- [ ] High-score keys and library lookup still use the raw native title
- [ ] `cmake --build build` passes with no new warnings, and 41/41 tests pass in the bwrap sandbox
- [ ] Follows existing patterns (anonymous-namespace glyph table, `[[nodiscard]]` free functions, `TEST_CHECK` tests, explicit `blaze4k_core` source list)
