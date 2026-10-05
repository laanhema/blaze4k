# Code Review: feature/052-song-subtitles

**Scope**: Branch `feature/052-song-subtitles` vs `main` (no commits yet; all changes uncommitted), GitHub issue #110. Reviewed 10 source files, 5 test files, the plan (`.agents/plans/completed/052-song-subtitles-plan.md`) and the implementation report (`.agents/reports/052-song-subtitles-plan-report.md`). `.agents/stories/todo-stories.md` has an unrelated change and is excluded. `TODO.md` is gitignored (local-only tick, verified at line 43).
**Recommendation**: APPROVE WITH NITS

## Summary

The branch draws `#SUBTITLE` / `#SUBTITLETRANSLIT` after the title, on the title's baseline, in three places: the select wheel rows (plain and selected), the song info panel, and the results top bar, where it fades in with the title. A new pure helper, `fit_title_subtitle`, splits the width budget between title and subtitle, and the subtitle keeps at least 40% when both are too long. The code matches the plan and SM5 semantics: no added brackets, and native/translit is chosen per field. With no subtitle, every draw site takes the old code path unchanged, and `top_bar_layout` gives a field-identical layout (tested). The build is clean with no warnings, and all 49 sandboxed tests pass. The only finding is a Low edge case for simfiles with an empty `#TITLE`.

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)

1. **An empty `#TITLE` with a non-empty `#SUBTITLE` is handled differently on select and on results.** `src/screens/results_art.cpp:62` (`has_subtitle = subtitle_w > 0.0f && title_w > 0.0f`) drops the subtitle from the results bar when the title measures 0. The select screen still draws it, offset by a stray leading gap (`src/screens/select_art.cpp:501` and `:697`, `x + measure("") + gap`). The plan assumed that `title_w == 0` "can't happen in practice", because "UNKNOWN" is used when there is no song. But the parser has no title fallback (`src/chart/simfile_parser.cpp:95`), and `SongLibrary` loads songs with an empty title, so a malformed or minimal simfile can reach this path. Nothing crashes and nothing overlaps; the only effect is an inconsistent display.
   *Recommendation*: when the display title is empty, treat the subtitle as the title in all three places, so it is drawn at the title position in the title style with no gap. Alternatively, drop the `title_w > 0` guard and skip the gap when the fitted title is empty. The SM5-faithful fix is broader: give an empty title the song folder name (SM5 `Song::TidyUpData`). That fix belongs in its own issue.

Noted, not a finding:
- `SongLibrary::find_song` (lookup by title only) and the log lines still don't tell same-title songs apart. The plan explicitly scoped this out.
- In the wheel, the title/subtitle fit is computed twice per subtitled row per frame (once in the title loop, once in the subtitle loop). The plan allowed this. The cost is a few `measure` calls on at most 9 rows.
- The visual result has not been checked yet: ice colour on plain rows, baseline alignment next to the italic 40px titles, and italic overhang into the 10px/8px gap. This is owner E2E step 3, which is still pending.

## Validation Results

| Check | Status |
|-------|--------|
| Build (`cmake --build build -j$(nproc)`, Release, GCC) | PASS (up to date; binaries newer than the sources) |
| Warnings gate (no linter configured) | PASS: recompiled all 5 changed `src/screens/*.cpp` and 5 changed `tests/*.cpp` to `/dev/null` with the targets' own `flags.make` (`-std=c++20 -Wall -Wextra -Wpedantic -O3`). Zero warnings, and the user's build tree was not touched |
| Tests | PASS: 49/49 under `bwrap --dev-bind / / --tmpfs /run/user/$UID --tmpfs /dev/snd --unshare-net ctest --test-dir build --output-on-failure` (0.66 s). Inside the sandbox I confirmed that `/dev/snd` and `/run/user/$UID` are empty |
| Skipped / env-guarded tests | None skipped. The "Font unavailable" / "No GL context" log lines come from existing headless and missing-font fallback tests and run every assertion. The new real-font subtitle tests check `font_available` and ran with the TrueType fonts (`Disconnected -Hyper-/-Mobius-/-Hardkore- fit whole`, `6 states` results render, `subtitle display choice and title/subtitle fit`) |
| Static: `git diff main -- src/data src/chart src/timing src/audio src/input src/gameplay assets` | PASS: empty (identity, timing and judgment are untouched) |
| Static: no added brackets around the subtitle | PASS |

## What's Good

- **Faithful semantics.** The subtitle uses the SM5 `GetDisplaySubTitle` rule, chosen per field and independently of the title, and no brackets are added (`GetDisplayFullTitle`), so `(Folk Mix)` and `~Speedy Mix~` are not doubled.
- **Strict "no subtitle, no change".** Every draw site branches on `subtitle.empty()` before calling the original `truncate(title, style, budget)`. `top_bar_layout` with a subtitle of 0, a negative value, NaN or inf is pinned field-identical to the 3-argument layout. The `title_gap` change from `title_max_w > 0` to `slot_w > 0` is equivalent when there is no subtitle.
- **No overlap by construction.** `fit_title_subtitle` guarantees `title + gap + subtitle <= budget`, and an invariant grid test covers it. `truncate_to_width` never returns text wider than the max, and the subtitle x comes from the *measured* fitted title, so the group always stays inside the old budget.
- **Untrusted input is safe.** Non-finite and negative widths are sanitised in both pure helpers and tested.
- **No new atlas.** The four new styles reuse existing (font, size) pairs. `kAllStyles` 30 / 13 pairs are pinned in three tests, and all four styles are checked as prebaked.
- **Good tests.** The fit is tested with real fonts at the worst wheel indent (distance 3, `kWheelIndent[3]`), on the selected bar and on the info panel. The overflow case checks the `...` and that nothing overlaps. The render smoke now has two same-title songs, and the results tests cover caching and clearing.

## Recommendation

Ready to merge after the owner's windowed check (plan E2E step 3). The single Low finding is optional. It can be fixed here with a small branch on an empty display title, or tracked with the broader SM5 "empty title → folder name" fallback.
