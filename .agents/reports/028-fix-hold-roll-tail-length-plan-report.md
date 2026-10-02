# Implementation Report

**Plan**: `.agents/plans/completed/028-fix-hold-roll-tail-length-plan.md`
**Branch**: `feature/028-fix-hold-roll-tail-length`
**Status**: COMPLETE (the owner's manual visual check is still pending, see below)

## Summary

Fixes #62. Hold and roll end caps were drawn entirely past the tail, so the visible end of every hold
and roll landed one full note box (96 px) after its release time. The body now stops
`HoldSprites::tail_inset_scale` note sizes before the tail and the cap starts there. For Cel that is
`kCelHoldBodyStopFromTail = 0.5`, from `metrics.ini` `StopDrawingHoldBodyOffsetFromTail=-32`, so the
cap is centred on the tail as in OpenITG/SM5 with Cel. The procedural fallback has no cap and keeps 0,
so its body still runs exactly to the tail.

The geometry now lives in a pure, GL-free `layout_hold()`. It clips the cap at the head centre
(OpenITG `DrawHoldBottomCap` and SM5 both do this) and offsets the texture v by the clipped
distance. `draw_hold` takes the scroll direction explicitly instead of inferring it from
`tail_y < head_y`, which was ambiguous at `head_y == tail_y`. Body tiling is now anchored at the
body/cap junction. That junction is one shared float edge, so the two quads meet without a seam.

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | Skin metric `kCelHoldBodyStopFromTail`, `HoldSprites::tail_inset_scale`, Cel `hold()` passes it | `src/gameplay/noteskin.hpp`, `src/gameplay/noteskin.cpp` | ✅ |
| 2 | Pure `HoldLayout` / `layout_hold()` | `src/gameplay/note_field_renderer.hpp`, `.cpp` | ✅ |
| 3 | `draw_hold` uses the layout, takes explicit `reverse`, shares the seam edge; `render()` hoists `reverse` | `src/gameplay/note_field_renderer.cpp` | ✅ |
| 4 | Headless geometry tests + registration | `tests/note_field_renderer_test.cpp`, `tests/CMakeLists.txt` | ✅ |
| 5 | Fallback `tail_inset_scale == 0`, Cel constant == 0.5 | `tests/noteskin_test.cpp` | ✅ |
| 6 | Cel README cap row | `assets/noteskins/cel/README.md` | ✅ |
| 7 | Full suite | n/a | ✅ 38/38 |

## Validation Results

| Check | Result |
|-------|--------|
| Build (`cmake --build build -j16`, `-Wall -Wextra -Wpedantic`; touched files force-recompiled) | ✅ No compiler warnings or errors. The only CMake warnings are the existing third-party configure ones (SDL alsa, glad `cmake_minimum_required`) |
| Lint | N/A: no linter is configured, so the compiler warnings above are the lint gate |
| Tests (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build --output-on-failure`) | ✅ 100% passed, 38/38 (baseline was 37, plus `note_field_renderer_test`) |
| Targeted (`note_field_renderer_test`, `noteskin_test`) | ✅ |
| App smoke (`./build/blaze-4k --headless --smoke-test 10`) | ✅ exit 0 |
| Extra: headless `--gameplay-demo "songs/In The Groove/Bend Your Mind/Bend your mind.sm" --smoke-test 120` at 1x/8x, up/down | ✅ exit 0 in all four. The headless GL renderer is a no-op, so this checks the code path, not the visuals |
| Scope guard (`git diff --name-only \| grep -E "judgment\|note_field\.cpp\|gameplay_view\|timing/"`) | ✅ no output |
| `.agents/stories/todo-stories.md` | ✅ still untracked and untouched |
| Visual check (AC 4), 1x/8x, up/down scroll, ITG songs in `songs/` | ⏳ **Pending manual owner verification**. The agent cannot run a windowed or interactive binary |

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/gameplay/noteskin.hpp` | UPDATE | +15/-5 |
| `src/gameplay/noteskin.cpp` | UPDATE | +2/-1 |
| `src/gameplay/note_field_renderer.hpp` | UPDATE | +18/-2 |
| `src/gameplay/note_field_renderer.cpp` | UPDATE | +82/-38 |
| `tests/note_field_renderer_test.cpp` | CREATE | +172 |
| `tests/CMakeLists.txt` | UPDATE | +10/-0 |
| `tests/noteskin_test.cpp` | UPDATE | +3/-1 |
| `assets/noteskins/cel/README.md` | UPDATE | +1/-1 |

## Deviations from Plan

- **Open Questions settled by the product owner.** The cap is centred, using the plan's default
  `kCelHoldBodyStopFromTail = 0.5`. No permanent hold/roll test chart was added. Both match the plan's
  proposed defaults.
- **Seam guard detail (Task 3.5).** The first body segment's tail-side edge and the unclipped cap's
  head-side edge both use one `float junction_y` converted from `layout.body_end_y`. When the cap is
  clipped there is no body (the head is past the junction), so the cap uses its own clipped
  `cap_near_y`. This is how the plan's intent was implemented, not a change to it.
- **Extra test coverage beyond the plan's list:** a non-integer no-seam case, a down-scroll
  "head past cap" case, and "cap art present but `has_cap=false` ignores the inset".
- **Visual check (E2E step 3)** is an owner step by design and is recorded as pending.

## Tests Written

| Test File | Test Cases |
|-----------|------------|
| `tests/note_field_renderer_test.cpp` | up-scroll Cel cap centred on tail; down-scroll mirror; no seam (exact `==`); visible-end regression (S/2 < old S); held hold near its end clip (up/down); exact tail time remnant inside the receptor box (up/down, explicit `reverse`); head past the whole cap; short hold; cap-less fallback / zero cap size / no cap art |
| `tests/noteskin_test.cpp` | fallback `tail_inset_scale == 0`; `kCelHoldBodyStopFromTail == 0.5` |

## Pending Manual Verification (owner)

Run `./build/blaze-4k --gameplay-demo "<ITG song with holds/rolls>.sm" --speed 1x`, then `--speed 8x`,
and repeat both with `--downscroll`. Check that:
- a hold held to the end shrinks into the receptor, with nothing sticking out past the receptor box;
- an unhit hold's rounded end passes the receptor about half a note after the release beat, not a full note;
- there is no gap or line between the body and the cap;
- rolls behave the same way.
