# Code Review: feature/039-import-cabinet-theme-pack

**Scope**: Branch `feature/039-import-cabinet-theme-pack` vs `main` (no commits yet; all changes staged), GitHub issue #87
**Recommendation**: APPROVE (with nits)

## Summary

Reviewed the import of the untracked Cabinet theme pack into the repo layout: 4 TTF fonts and 2 OFL texts in `assets/fonts/`, 65 PNGs and `manifest.json` in `assets/theme/cabinet/`, `src/render/theme.hpp`, the pack docs and mock-ups in `docs/cabinet-theme/`, a new `tests/theme_test.cpp` (plus its `tests/CMakeLists.txt` registration), and README/BUILDING doc updates. The change meets all five acceptance criteria of #87. `CMakeLists.txt` is unchanged, the untracked `blaze4k-cabinet-theme/` folder is gone, and `theme.hpp` compiles with no warnings through `theme_test`. I found only three low-severity nits, all in docs or test hardening.

Out of scope and untouched: `.agents/stories/todo-stories.md` (unrelated owner edits, still ` M`/unstaged). The untracked plan and report under `.agents/` are workflow artifacts.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **`src/render/theme.hpp:88-95`: `Font` and `kFontFiles` are parallel lists with nothing linking them at compile time.**
   The enum has 4 members and the array is sized `4` by hand. The only guard is `static_assert(kFontFiles.size() == 4)` in `tests/theme_test.cpp:25`, which does not tie the size to the enum. If #90 adds a font to one list but not the other, indexing by `static_cast<size_t>(Font::X)` goes out of bounds or loads the wrong file without any error.
   *Recommendation:* when #90 starts indexing by `Font`, add a `kFontCount` (or a trailing sentinel) and `static_assert(kFontFiles.size() == kFontCount)` in the header. This is not needed in this PR, which imports `theme.hpp` unchanged.

2. **`docs/cabinet-theme/README.md:10` and `:27`: small inaccuracies in the moved README.**
   Line 10 says `reference/` holds "the four target screens rendered at 1280x720". It also holds `textures-overview.jpg` (2400x3558), which is not a screen and is not 1280x720. Line 27 says the digit fonts are in `theme/cabinet/`. Every other path in the file is repo-rooted (`assets/theme/cabinet/`).
   *Recommendation:* mention `textures-overview.jpg` in the tree, and use `assets/theme/cabinet/` on line 27.

3. **`tests/theme_test.cpp:64-87`: the manifest geometry that #89 will rely on is not checked.**
   The test checks that each file exists and that its `size_px` matches the PNG header. It does not check that `content_px`, `hole_px`, `slice3_px` and `slice9_px` fit inside `size_px`. I checked all 63 entries by hand and they are consistent today. A guard would catch a bad re-bake before the #89 slice and frame helpers compute negative middle widths from it.
   *Recommendation:* add these bounds checks in #89, or here. Optionally, also check that the digit-atlas glyph rects fit inside their PNG.

**Noted, not a finding** (scoped out in the plan's Risks/Open Questions):
- The POST_BUILD asset copy runs only when `blaze-4k` relinks, so an existing incremental `build/` will not pick up the new assets. The plan flags this for #89, because the AC forbids CMake changes.
- About 8 MB of binaries are added to git history.
- The baked textures and mock-ups have no licence note, and the default is to treat them as the owner's work under MIT. The owner should confirm this.
- MSVC `/W4` cleanliness of `theme.hpp` is unverified on this host.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`) | PASS (`theme_test` target builds) |
| Warnings gate (`theme_test.cpp` recompiled with `-fsyntax-only` using the build's own flags from `build/tests/CMakeFiles/theme_test.dir/flags.make`) | PASS, 0 warnings |
| `theme.hpp` standalone TU, `g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow` | PASS, 0 warnings (clang not installed on host, so not checked) |
| Tests (sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build`) | PASS, 42/42 (was 41 on `main`, +1 `theme_test`). No tests skipped or env-guarded |
| Sandbox check | `/dev/snd` and `/run/user/$UID` confirmed empty inside the sandbox. `audio_test` fell back to the "NULL Playback Device" via the Null backend, and all its assertions still ran |
| Manifest invariants (ad-hoc script) | PASS: every `content_px`/`hole_px`/`slice*_px` lies inside `size_px`, and both atlases have 14 glyph keys |
| Licences | Both OFL texts carry their copyright and Reserved Font Name lines (Audiowide: Astigmatic; Saira: The Saira Project Authors). The fonts ship unmodified, so the RFN clause is satisfied |
| Fresh-clone AC | Not re-run by the reviewer. The implementation report records an offline fresh-tree build plus sandboxed ctest at 42/42. All 84 new paths are staged and none are git-ignored |

## What's Good

- The move was verified byte for byte (80/80 SHA-256 matches) before the one intended README edit. The pack folder was then removed with an empty-dir-only delete, not `rm -rf`.
- `theme_test` does more than prove the header compiles. It checks TTF sfnt magic, that the PNG headers match the manifest, that every PNG passes the loader's own untrusted-input cap (`probe_image_header`), that there are no orphan PNGs, and that the digit glyphs are covered. This gives #89 and #90 a verified base.
- It follows the existing `BLAZE4K_ASSETS_DIR` test seam and the `TEST_CHECK` and test-file conventions exactly.
- The `theme.hpp` layout constants agree with the manifest's `layout_720p` boxes (banner, diff badge, life bar, judgment, medallion, ribbon, logo/subtitle/press-start tops). The skew values match tan(8/10/12/14°).
- README licence lines and docs link match the existing style, and `CMakeLists.txt` is untouched as the AC requires.

## Recommendation

Ready to commit and open a PR. The three nits are optional. Item 2 is a two-line doc fix that could go in now. Items 1 and 3 fit naturally into #90 and #89.
