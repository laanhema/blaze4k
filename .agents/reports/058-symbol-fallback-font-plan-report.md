# Implementation Report

**Plan**: `.agents/plans/completed/058-symbol-fallback-font-plan.md`
**Branch**: `feature/058-symbol-fallback-font`
**Status**: COMPLETE

## Summary

Symbol characters such as ☺ (Delirium's `#ARTIST`) now render as real glyphs instead of the hollow placeholder box. A merged Noto Sans Symbols 1+2 subset (`assets/fonts/NotoSansSymbols-Subset.ttf`, 101 808 B, OFL 1.1) is built reproducibly by `scripts/build-symbol-font.sh` from pinned, sha256-verified upstream files. The baked slot table grows from 319 to 975 slots (adds U+2190–21FF, U+25A0–25FF, U+2600–26FF, U+2700–27BF). The shared glyph walker resolves primary → symbol face → ASCII fold → placeholder, so `measure`, `draw`, `truncate` and `covers_text` agree. Symbol glyphs bake lazily into small per-size atlases (≤ 16 sizes × ≤ 64 glyphs), and a missing or corrupt symbol font logs one line and degrades to today's behaviour. Render-only change: no timing, audio, input, gameplay or chart code is touched.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Build script, subset font, OFL text (sha256 verified, byte-identical on re-run) | `scripts/build-symbol-font.sh`, `assets/fonts/NotoSansSymbols-Subset.ttf`, `assets/fonts/OFL-NotoSansSymbols.txt` | ✅ |
| 2 | 7-range slot table (975), `kLatinGlyphCount`, probe parameter, bake-by-slot-list | `src/render/ttf_font.{hpp,cpp}` | ✅ |
| 3 | Fallback-aware walker, `measure_text` / `for_each_text_quad` overloads, `GlyphQuad::fallback` | `src/render/ttf_font.{hpp,cpp}` | ✅ |
| 4 | `kSymbolFontFile`; TextRenderer symbol face load, lazy symbol atlases, draw/measure/covers wiring | `src/render/theme.hpp`, `src/render/ttf_font.{hpp,cpp}` | ✅ |
| 5 | Tests (fixed 319-based checks, slot pins, symbol fallback section, fuzz, Delirium display text, font-file check) | `tests/ttf_font_test.cpp`, `tests/select_art_test.cpp`, `tests/theme_test.cpp` | ✅ |
| 6 | README license credit | `README.md` | ✅ |
| 7 | TODO tick (#124) | `TODO.md` (gitignored) | ✅ |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | ✅ |
| Lint (no linter; zero compiler warnings on a rebuild after touching `ttf_font.hpp` / `theme.hpp` and the touched TUs) | ✅ 0 warnings |
| Tests (sandboxed ctest) | ✅ 51/51 passed |
| `git diff --stat -- src/timing src/audio src/input src/gameplay src/chart` | ✅ empty |
| Font is sfnt 1.0 TrueType, small | ✅ `True 101808` |
| Off-limits `todo-issues.md` not staged | ✅ `0` |
| Fresh-clone check (working-tree mode, sandboxed) | ✅ PASS, 51/51 |
| E2E `/verify` AC1: song select, Delirium artist | ✅ real ☺ glyph in ice colour (`/tmp/blaze4k-verify/124-symbols/evidence/01-select-delirium.png`, zoom `01-select-delirium-artist-zoom.png`) |
| E2E `/verify` AC2: results top bar | ✅ ☺ after "Delirium" (`03-results-delirium.png`, zoom `03-results-delirium-artist-zoom.png`); run failed at 10.16 s, `time source audio` |
| E2E Latin-1 spot-check | ✅ `VerTex³` title still draws ³ from Saira (`/tmp/blaze4k-verify/124-symbols-vertex/evidence/02-select-vertex3.png`) |
| Log check | ✅ `[TextRenderer] Loaded 4/4 fonts (+ symbols)`, no `Symbol font unavailable`, no symbol bake failure |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `scripts/build-symbol-font.sh` | CREATE | +96 |
| `assets/fonts/NotoSansSymbols-Subset.ttf` | CREATE | binary, 101 808 B |
| `assets/fonts/OFL-NotoSansSymbols.txt` | CREATE | +93 |
| `src/render/theme.hpp` | UPDATE | +4/-0 |
| `src/render/ttf_font.hpp` | UPDATE | +77/-20 |
| `src/render/ttf_font.cpp` | UPDATE | +284/-81 |
| `tests/ttf_font_test.cpp` | UPDATE | +295/-5 |
| `tests/select_art_test.cpp` | UPDATE | +18/-0 |
| `tests/theme_test.cpp` | UPDATE | +7/-2 |
| `README.md` | UPDATE | +1/-0 |
| `TODO.md` | UPDATE (gitignored, edited locally) | +1/-1 |

## Deviations from Plan

1. **`symbol_atlas_for` signature**: `symbol_atlas_for(const FontFace& primary, float size_px, std::string_view text)` rather than `(size_px, text)`. It needs the style's primary face to decide which code points resolve to the symbol face (same walker rule).
2. **Shared cap warning**: both caps (16 sizes, 64 glyphs per size) share the one `warned_symbol_cap_` flag, so there is one cap log line per renderer lifetime in total, as Pinned Semantics 7 says.
3. **Fresh-clone check mode**: ran `scripts/fresh-clone-check.sh` in its default working-tree mode, not `--committed`. Nothing is committed (project git policy), so `--committed` would only test `main`. It ran under the bwrap audio sandbox because it calls ctest.
4. **GL-only paths not unit-tested**: `TextRenderer::symbol_atlas_for` (lazy bake, the 16/64 caps, rebake on growth, the drop on scale change) needs a GL context, and the test suite is headless. The unit tests cover the pieces it uses: `FontAtlas::bake` with `only_slots` (including a 64-glyph bake at the 4K title size that fits 4096²), placeholder substitution at the same pen, and headless draws as no-ops. The `/verify` run covers the live path.
5. **Extra tests beyond the plan**: the probe error says `U+263A` for a non-ASCII probe, messy `only_slots` (negative, duplicate, out-of-range, absent) are ignored, a primary glyph in a symbol block (Saira's ←) wins over the symbol face, right alignment uses the fallback-aware width, and a fallback atlas at another size is scaled.
6. **E2E navigation**: a scratch songs root that symlinked one group showed "NO SONGS FOUND" (the scanner does not follow a symlinked group). The spot-check used the full `songs/` root instead. The first relaunch hit a window-lookup race (`window ? not found`); a stop and relaunch fixed it.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/ttf_font_test.cpp` | Slot pins for all 4 symbol blocks + bounds; `kLatinGlyphCount`/`kBakedGlyphCount` static_asserts; Saira Latin-only count; Saira has ← but not ☺; Audiowide loop to `kLatinGlyphCount`; corrupt-root copies the symbol font (1 log line); root without symbol font: load ok, exactly one `Symbol font unavailable` line, `covers_text(☺)` false, placeholder measure; new `test_symbol_fallback` (probe 'A' rejection + `U+263A` message, U+2600–26FF 256/256, symbol measure vs placeholder, no cross-face kerning, tracking, ★ beats fold, primary-in-block wins, emoji/CJK/U+FFFD placeholder, `only_slots` bake (1 glyph, ≤256², messy input), 64-glyph 4K bake within cap, fallback quad UV/x/baseline, scaled fallback atlas, placeholder at same pen when unbaked or no atlas, old overload, right alignment, TextRenderer coverage SairaBold/Audiowide/emoji/mixed, renderer measure, headless draw no-op); fuzz through the fallback path with E2 98/E2 9C biased bytes |
| `tests/select_art_test.cpp` | Delirium artist "☺" native kept with and without translit "Smiley"; bitmap rule (`nullptr`) picks the translit when present |
| `tests/theme_test.cpp` | `kSymbolFontFile` exists with an sfnt 1.0 header; `OFL-NotoSansSymbols.txt` present |
