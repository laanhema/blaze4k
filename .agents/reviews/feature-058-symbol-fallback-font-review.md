# Code Review: feature/058-symbol-fallback-font

**Scope**: branch `feature/058-symbol-fallback-font` vs merge base `b95dcc1` (no commits yet; all work is uncommitted/untracked), checked against issue #124
**Recommendation**: APPROVE WITH NITS

## Summary

The branch adds a merged Noto Sans Symbols 1+2 subset as a fallback face, grows the slot table to 975 code points, and routes `measure` / `draw` / `truncate` / `covers_text` through one fallback-aware glyph walker, with symbol glyphs baked lazily into capped per-size atlases. The change is well-contained (render layer only), well-tested headlessly, and every issue #124 acceptance criterion is met with screenshot evidence. One medium lifetime bug exists in the mid-frame symbol-atlas rebake; the rest is minor. All changed files were reviewed in full. `.agents/issues/todo-issues.md` is unrelated to this branch and was excluded.

## Acceptance Criteria (#124)

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Delirium's artist shows a smiley glyph, not the placeholder box, in song select | MET | `/tmp/blaze4k-verify/124-symbols/evidence/01-select-delirium.png` + `01-select-delirium-artist-zoom.png`: a real ☺ in the ice colour under the title |
| Same artist text renders correctly on the results screen | MET | `03-results-delirium.png` + `03-results-delirium-artist-zoom.png`: ☺ after "Delirium" in the top bar; `results_screen.cpp:125` uses the same `song_display_artist` + `TextRenderer::draw` path |
| Code points missing from every font still draw the placeholder; malformed UTF-8 doesn't crash | MET | `tests/ttf_font_test.cpp` `test_symbol_fallback` (emoji, CJK, truncated `E2 98` -> 0.6em placeholder; placeholder at same pen when unbaked) + fuzz pass biased toward `E2 98 xx` / `E2 9C xx` through the fallback path |
| Unit tests cover glyph lookup and `covers_text` for at least U+263A | MET | slot pin `0x263A -> 585`, `symbol.has(smiley)`, `!saira.has(smiley)`, `renderer.covers_text("☺", SairaBold/Audiowide)`, missing-font root `!covers_text("☺")`, `select_art_test` Delirium native/translit selection |
| A song select screenshot via `/verify` shows Delirium's artist rendered | MET | `01-select-delirium.png`; `game.log` shows `Loaded 4/4 fonts (+ symbols)` and no symbol bake failures |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority

- **M1** `src/render/ttf_font.cpp:1054` — Rebaking a symbol atlas mid-frame destroys the old atlas texture (the optional move-assign runs `Texture::operator=` -> `destroy()`) while `GlQuadRenderer` may still hold unflushed quads bound to that texture id. **Why:** `GlQuadRenderer::draw_quad_points` only flushes on a texture change (`gl_quad_renderer.cpp:217-223`), so if one `draw()` ends on a symbol quad (e.g. wheel row "Foo ☆") and the next `draw()` at the same `size_px` introduces a new symbol (row "Bar ♥"), the pending batch is later flushed with a deleted texture name — `GL_INVALID_OPERATION` in a core profile, or, since `upload()` calls `glGenTextures` right after the delete and drivers commonly reuse the freed name, the old UVs sampled from the new atlas layout — giving a one-frame garbled glyph each time a new symbol appears. Primary atlases never hit this because `atlas_for` only appends, never replaces. **Fix:** flush the renderer's pending batch before replacing `entry->atlas` (pass `GlQuadRenderer&` into `symbol_atlas_for` and expose a public `flush()`; `end()` is unsuitable because it resets the blend mode), or keep the replaced `FontAtlas` alive in a retired list that is cleared at the next `set_window_size`/frame start.

### Suggestions (Low)

- **L1** `src/render/ttf_font.cpp:993` — The slot-merge/cap logic in `symbol_atlas_for` (dedup via `lower_bound`, 64-glyph cap, 16-size cap, "rebake only on growth", warn-once) runs only under GL and has no unit test (an acknowledged deviation in the implementation report). **Why:** this is the untrusted-input bound for simfile text and the place where M1 lives; a regression in the caps would be invisible to the headless suite. **Fix:** extract the merge into a pure helper (e.g. `merge_symbol_slots(std::vector<int>& slots, std::span<const int> wanted, std::size_t cap) -> {grew, dropped}`) and unit-test dedup, cap, and growth detection.

**Noted, not a finding**:
- Symbol glyphs past the 16-size / 64-glyph caps draw the placeholder while `covers_text` still reports them covered (so native text wins over a translit); this is the planned Pinned Semantics 4/7 behaviour (measure never depends on bake state).
- Noto symbols are regular weight next to Saira Bold/ExtraBold — accepted in the plan's risk table; the owner judges from the screenshots.
- Emoji/CJK remain placeholders — out of scope per the issue.
- Environment, not this branch's code: the implementer's `scripts/fresh-clone-check.sh` run (offline mode passes `-DFETCHCONTENT_BASE_DIR=$repo_root/build/_deps`, `scripts/fresh-clone-check.sh:108`, unchanged since main) regenerated `build/_deps/*-build/` Makefiles for its scratch tree, so `cmake --build build` now fails with `No rule to make target '_deps/glad-build/CMakeFiles/glad-generate-files.dir/depend'`. Re-running `cmake -B build` should repair it. The script's habit of rewriting the user's dependency build dirs is a pre-existing problem worth a follow-up issue.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | User's `build/` is broken by the fresh-clone run above (pre-existing environment state, not caused by the diff), so validation used a clean scratch build: `cmake -S . -B <scratchpad>/rev-build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_<DEP>=build/_deps/<dep>-src` (read-only reuse) + `cmake --build`; host, exit 0 |
| Warnings | NONE | Clean full build, and changed TUs (`ttf_font.cpp`, `ttf_font_test.cpp`, `theme_test.cpp`, `select_art_test.cpp`) recompiled from `compile_commands.json` to `/dev/null` with project flags (`-Wall -Wextra -Wpedantic -Wshadow -Wfloat-conversion -Wimplicit-fallthrough -Wundef`): 0 warnings. Remaining warnings are in SDL/glad under `_deps` |
| Lint | N/A | No linter or `.clang-format` configured |
| Tests | PASS | 51/51. Sandboxed: `bwrap --dev-bind / / --tmpfs /run/user/$(id -u) --tmpfs /dev/snd --unshare-net ctest --test-dir <scratch>` (sandbox confirmed: `/dev/snd` and `/run/user` empty). 48 passed in ctest; `parser_hardening_test`, `score_keeper_test`, `metronome_sync_test` failed only on cwd-relative fixture paths from the out-of-tree dir and pass (rc 0) when re-run sandboxed with `build/` as cwd. No skipped or env-guarded tests; `ttf_font_test` GL paths run headless by design (headless renderer, no-op draws), not skipped |

## What's Good

- One shared walker (`walk_glyphs`) drives measure, draw, truncation, coverage and the bitmap fallback, so layout cannot drift between them; the placeholder substitutes at the same pen/advance when a symbol isn't baked.
- Resolution order (primary -> symbol face -> ASCII fold -> placeholder) is correct and tested, including Saira's own arrows winning over Noto and ★ beating the `*` fold.
- Untrusted-input hygiene: per-size and size-count caps, out-of-range/duplicate `only_slots` filtered, fuzz extended through the fallback path.
- Graceful degradation: a missing/corrupt symbol font logs exactly one line and keeps today's behaviour (tested).
- Reproducible, sha256-pinned font build script with throwaway venv; OFL text bundled and credited in README; the game stays offline.
- Strong evidence: AC screenshots for both screens plus a Latin-1 spot-check.

## Recommendation

Merge-ready after considering M1 (a small fix: flush the pending batch, or defer destroying the old symbol texture, before a mid-frame rebake). L1 is optional hardening. Separately, repair the local `build/` with `cmake -B build`, and consider an issue for `scripts/fresh-clone-check.sh` overwriting the user's `build/_deps/*-build` dirs.
