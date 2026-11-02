# Plan: Prune comments in src/screens (#162)

## Summary

`src/screens` currently contains 1,045 comment lines across 40 files (7,182 total lines of code), many of which are redundant restatements of obvious function signatures, parameter names, or obsolete development milestone references (such as `C1` through `C7` and `D2`). This plan establishes a disciplined pruning pass across all 40 files in `src/screens` to retain only comments that provide essential context that code alone cannot express: non-obvious design rationales ("why"), layout constraints and pixel budgets next to static assertions, cited OpenITG/StepMania metrics and commit references, and bug workaround explanations. Restatements, obsolete roadmap tags, and trivial inline echoes are removed. Only comments change (with no semantic code changes), ensuring `scripts/check.sh` remains clean and all 53 unit tests pass throughout.

## User Story

As a developer maintaining Blaze 4k  
I want comments in `src/screens` pruned of noise and redundant restatements  
So that only essential domain context, design constraints, and reference citations remain.

## Metadata

| Field | Value |
|---|---|
| Type | REFACTOR |
| Complexity | MEDIUM |
| Systems Affected | `src/screens/` (40 files: 20 headers, 20 implementations), `TODO.md` |
| GitHub Issue | #162 ([TODO-50] Prune comments in src/screens) |
| PRD Phase | N/A |
| Prototype Patch | N/A |
| Plan sequence | `075`. The running sequence in `.agents/plans/completed/` ends at `074` (#158, sort song wheel). |

---

## Environment Findings

Planned on `main` @ `0f5960a`, working tree clean.

| Fact / Tool | Value | Notes |
|---|---|---|
| Baseline build & tests | `scripts/check.sh` passed, 53 of 53 tests | Linux GCC 15.2; silent null audio device |
| Current line count in `src/screens` | 7,182 lines across 40 files (20 `.hpp`, 20 `.cpp`) | Measured via `wc -l src/screens/*.{hpp,cpp}` |
| Current comment lines in `src/screens` | 1,045 comment lines (`//`, `/*`, `*`) | Measured via `grep -E '^\s*(//|/\*|\*)' src/screens/*.{cpp,hpp} \| wc -l` |
| Milestone tag occurrences (`C1`–`C7`, `D1`–`D4`) | 48 occurrences across 11 files | Measured via `grep -nE '\b(C[1-7]\|D[1-4])\b' src/screens/*.{cpp,hpp} \| wc -l` |
| License headers in `src/screens` | 0 | Checked via `grep -rnE '(License\|Copyright\|MIT\|SPDX)' src/screens/` |
| `lint-allow` comments in `src/screens` | 0 | Checked via `grep -rnE 'lint-allow' src/screens/` |
| Sibling tasks | Issue #163 ([TODO-51]), #164 ([TODO-52]), #165 ([TODO-53]) | Parts 2, 3, and 4 of comment cleanup across other folders |

---

## Pinned Semantics & Comment Retention Rules

According to the acceptance criteria of issue #162 and the technical notes:

### What MUST Stay (Essential Context)
1. **Cited OpenITG / StepMania source values and metrics**: For example, `src/screens/options_menu.cpp:20-30` (OpenITG `metrics.ini` citations, commit hashes, and documented owner deviations for 7x/8x speed mods) and `src/screens/song_display_text.hpp:18,42,51` (StepMania 5 `StepsDisplay` / `Song` method parity).
2. **Design constraints and measured layout budgets**: Layout pixel budgets and room limits next to `static_assert` statements, such as `src/screens/setup_art.hpp:162-179` and `src/screens/select_art.hpp:64-71,87-91` (documenting why baked texture insets, tab widths, and margins differ from mockups).
3. **Rationale ("Why") and workaround explanations**: Explanations for why an architectural decision was made (e.g. why `Screen::back_consumed()` must mirror `handle_back()`, why `TextRenderer*` in `ScreenContext` is non-const due to lazy glyph baking, why `action_down` is polled rather than event-replayed for held notes).
4. **API and testing contracts**: Nullability expectations for headless / unit test runs in `ScreenContext` (e.g. `action_down`, `theme`, `text`, `ui_sounds` being null in unit tests).
5. **Special comments**: Any `lint-allow(...)` exception comments and license headers must be kept (none currently exist in `src/screens`, but the rule must be respected).

### What MUST Be Pruned (Noise and Redundancy)
1. **Obsolete development milestones**: References to internal project staging phases (e.g. `C1`, `C2`, `C3`, `C4`, `C5`, `C6`, `C7`, `D2`), such as `// C3 additions` or `// opens the C5 wizard`.
2. **Function signature / parameter restatements**: Comments that merely repeat what the function name or parameter types already declare (e.g. `// Display row d (clamped to [0, count - 1])` above `remap_display_row`, or `// Lit ticks for a meter: clamp(meter, 0, 10)` above `meter_ticks_lit`).
3. **Obvious variable / enum descriptions**: Comments repeating the name or basic semantics (e.g. `// UP / DOWN` next to `Scroll`, or `// Header only` next to `DeviceType device`).
4. **Pseudo-code duplicates**: Multiline comments that duplicate the function's implementation formulas verbatim in comments above the prototype (e.g. `src/screens/song_display_text.hpp:73-80`).

---

## Patterns to Follow

### Naming
```cpp
// SOURCE: src/screens/screen.hpp:30
enum class ScreenId { Title, Attract, Select, Gameplay, Results, Calibration, InputRemap };
```

### Error Handling
```cpp
// SOURCE: src/screens/attract_screen.cpp:26-28
if (w <= 0 || h <= 0) {
    return;
}
```

### Types
```cpp
// SOURCE: src/screens/setup_art.hpp:187-192
struct RemapDisplayRow {
    enum class Kind { Header, Binding, Reset };
    Kind kind = Kind::Reset;
    int binding = -1;
    DeviceType device = DeviceType::Keyboard;
};
```

### Tests
```cpp
// SOURCE: tests/screen_manager_test.cpp:98-112
void test_boot_lifecycle() {
    blaze4k::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);

    manager.start(ScreenId::Title);
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    TEST_CHECK(title.ptr->enter_count == 1);
    TEST_CHECK(title.ptr->exit_count == 0);

    manager.start(ScreenId::Title);
    TEST_CHECK(title.ptr->exit_count == 1);
    TEST_CHECK(title.ptr->enter_count == 2);
    std::cout << "  - boot lifecycle ok.\n";
}
```

### Comments: What to Keep vs What to Remove

#### Example to KEEP (OpenITG metrics citation & owner deviation)
```cpp
// SOURCE: src/screens/options_menu.cpp:20-25
// X-mod values: OpenITG's PlayerOptions screen lists these eight entries
//   assets/patch-data/Themes/default/metrics.ini:3694-3701
//     Speed,1..8 = mod,1x / 1.5x / 2x / 2.5x / 3x / 4x / 5x / 6x
// The grid here is those menu values EXTENDED with 7x and 8x at the product
// owner's request (#61) -- a documented deviation from the OpenITG menu grid.
```

#### Example to REMOVE (Restating code & function signature)
```cpp
// SOURCE: src/screens/select_art.hpp:230-234
// Lit ticks for a meter: clamp(meter, 0, 10).
[[nodiscard]] int meter_ticks_lit(int meter);

// Tick `n` (0..9) of `row`: {row.x + 210 + n*18, centred, 20, 18 (20 selected)}.
[[nodiscard]] Rect tick_rect(const Rect& row, bool selected, int n);
```

#### Example to REMOVE (Obsolete milestone roadmap tags)
```cpp
// SOURCE: src/screens/screen.hpp:26-29
// Stable identity of every arcade screen (PRD section 6/7.3). C1 implements
// Title/Attract and a Select placeholder; C3/C4/C5/C7 replace/extend the rest.
// Calibration (C5) and InputRemap (C6) are first-class screens entered from
// Select's options menu.
```

---

## Files to Change

| File | Action | Covered by Prototype Patch? | Purpose |
|---|---|---|---|
| `src/screens/screen.hpp` | UPDATE | No | Prune obsolete milestone comments (`C1`–`C7`, `D2`), keep nullability contracts and lifecycle rationale |
| `src/screens/screen_manager.hpp` | UPDATE | No | Prune function restatements and `C4` references, keep back-navigation contract |
| `src/screens/screen_manager.cpp` | UPDATE | No | Prune internal transition and milestone comments (`D2`, `C4`), keep transition invariant comments |
| `src/screens/play_request.hpp` | UPDATE | No | Prune trivial struct restatements |
| `src/screens/select_placeholder_screen.hpp` | UPDATE | No | Prune obsolete placeholder comments |
| `src/screens/select_placeholder_screen.cpp` | UPDATE | No | Prune `C3` references |
| `src/screens/options_menu.hpp` | UPDATE | No | Prune `C4`/`C5`/`C6` milestone notes and enum restatements, keep offset persistence constraint |
| `src/screens/options_menu.cpp` | UPDATE | No | Preserve OpenITG provenance table (`:15-42`); prune inline restatements and `C5`/`C6` tags |
| `src/screens/options_art.hpp` | UPDATE | No | Prune layout restatements, keep measured mock dimensions |
| `src/screens/options_art.cpp` | UPDATE | No | Prune trivial drawing step comments |
| `src/screens/calibration_screen.hpp` | UPDATE | No | Prune `C5` milestone notes and test accessor restatements, keep synthetic clock fallback rationale |
| `src/screens/calibration_screen.cpp` | UPDATE | No | Prune obvious step comments, keep metronome delay rationale |
| `src/screens/input_remap.hpp` | UPDATE | No | Prune `C4`/`C6` milestone tags and trivial field comments, keep conflict resolution contract |
| `src/screens/input_remap.cpp` | UPDATE | No | Prune obvious helper comments |
| `src/screens/input_remap_screen.hpp` | UPDATE | No | Prune `C4`/`C5`/`C6` references |
| `src/screens/input_remap_screen.cpp` | UPDATE | No | Prune trivial input dispatch comments |
| `src/screens/setup_art.hpp` | UPDATE | No | Preserve room budget constraints and static assertion comments (`:162-179`); prune redundant function descriptions |
| `src/screens/setup_art.cpp` | UPDATE | No | Prune obvious vertex layout comments |
| `src/screens/title_screen.hpp` | UPDATE | No | Prune trivial screen description |
| `src/screens/title_screen.cpp` | UPDATE | No | Prune obvious event handler comments |
| `src/screens/title_art.hpp` | UPDATE | No | Prune function restatements, keep layout scale references |
| `src/screens/title_art.cpp` | UPDATE | No | Prune obvious draw comments |
| `src/screens/attract_screen.hpp` | UPDATE | No | Prune trivial member comments |
| `src/screens/attract_screen.cpp` | UPDATE | No | Prune variable restatements, keep Cel receptor beat lighting rationale |
| `src/screens/gameplay_screen.hpp` | UPDATE | No | Prune member restatements, keep held-column sampling rationale |
| `src/screens/gameplay_screen.cpp` | UPDATE | No | Prune trivial loop comments, keep event-to-music-time rationale |
| `src/screens/results.hpp` | UPDATE | No | Prune struct field restatements |
| `src/screens/results.cpp` | UPDATE | No | Prune trivial grade/DP formatting comments |
| `src/screens/results_anim.hpp` | UPDATE | No | Prune state machine restatements, keep reveal timing curves |
| `src/screens/results_anim.cpp` | UPDATE | No | Prune obvious animation ticks |
| `src/screens/results_art.hpp` | UPDATE | No | Prune trivial rect restatements, keep measured score-screen mock citations and stat panel heights |
| `src/screens/results_art.cpp` | UPDATE | No | Prune obvious drawing step comments |
| `src/screens/results_screen.hpp` | UPDATE | No | Prune state variable restatements, keep high-score submission rationale |
| `src/screens/results_screen.cpp` | UPDATE | No | Prune `C7` milestone tags, keep event handling edge-case rationales |
| `src/screens/song_display_text.hpp` | UPDATE | No | Preserve SM5/OpenITG citations and subtitle budget sharing rationale; prune pseudocode arithmetic restatements |
| `src/screens/song_display_text.cpp` | UPDATE | No | Prune obvious string manipulation comments |
| `src/screens/select_screen.hpp` | UPDATE | No | Prune member comments, keep wheel restoration contract |
| `src/screens/select_screen.cpp` | UPDATE | No | Prune redundant step comments and `C5`/`C6` tags, keep modal back-handling and repeat-delay rationales |
| `src/screens/select_art.hpp` | UPDATE | No | Preserve mock measurement citations and Edit row design notes; prune trivial function and constant echoes |
| `src/screens/select_art.cpp` | UPDATE | No | Prune redundant drawing and arithmetic comments |
| `TODO.md` | UPDATE | No | Note progress on line 63 (#162 part complete) |

---

## Tasks

Execute in order. Each task is atomic and verifiable.

### Task 1: Core Screen Interfaces and Seams

- **File**: `src/screens/screen.hpp`, `src/screens/screen_manager.hpp`, `src/screens/screen_manager.cpp`, `src/screens/play_request.hpp`, `src/screens/select_placeholder_screen.hpp`, `src/screens/select_placeholder_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Remove obsolete milestone comments (`C1`–`C7`, `D2`) from `ScreenContext`, `ScreenId`, and `ScreenManager`.
  - Remove redundant member comments in `ScreenContext` (e.g. `// set by main each tick`).
  - Keep nullability contracts for unit testing (`action_down`, `set_cursor_visible`, `input`, `theme`, `text`, `noteskin`).
  - Keep the rationale for why `back_consumed()` must match `handle_back()`.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/screen.hpp:111-116`
- **Validate**: `scripts/check.sh`

### Task 2: Options Menu and Calibration

- **File**: `src/screens/options_menu.hpp`, `src/screens/options_menu.cpp`, `src/screens/options_art.hpp`, `src/screens/options_art.cpp`, `src/screens/calibration_screen.hpp`, `src/screens/calibration_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Preserve the OpenITG Value Provenance table in `src/screens/options_menu.cpp:15-42` in full.
  - Prune `C4`/`C5`/`C6` milestone references from `OptionsMenu`, `OptionsRow`, and `CalibrationScreen`.
  - Prune trivial comments restating enum variants or accessor behavior.
  - Keep the constraint that `options_menu_apply` does not write back offset, and keep the synthetic clock fallback rationale in `CalibrationScreen`.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/options_menu.cpp:20-30`
- **Validate**: `scripts/check.sh`

### Task 3: Input Remapping

- **File**: `src/screens/input_remap.hpp`, `src/screens/input_remap.cpp`, `src/screens/input_remap_screen.hpp`, `src/screens/input_remap_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Prune `C4` and `C6` milestone tags.
  - Remove obvious comments on struct members and getters.
  - Keep explanations of conflict-resolution behavior and why raw capture mode isolates input.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/setup_art.hpp:187-192`
- **Validate**: `scripts/check.sh`

### Task 4: Setup Art

- **File**: `src/screens/setup_art.hpp`, `src/screens/setup_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - Preserve all room budget comments and layout constraints next to `static_assert` statements (`setup_art.hpp:162-179`).
  - Prune function signature echoes and trivial layout descriptions.
  - Keep section dividers grouping layout vs draw helpers.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/setup_art.hpp:162-170`
- **Validate**: `scripts/check.sh`

### Task 5: Title and Attract Screens

- **File**: `src/screens/title_screen.hpp`, `src/screens/title_screen.cpp`, `src/screens/title_art.hpp`, `src/screens/title_art.cpp`, `src/screens/attract_screen.hpp`, `src/screens/attract_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Prune trivial event and render comments.
  - Keep the rationale in `attract_screen.cpp` explaining Cel receptor beat lighting and procedural dimmer behavior.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/attract_screen.cpp:42-45`
- **Validate**: `scripts/check.sh`

### Task 6: Gameplay Screen

- **File**: `src/screens/gameplay_screen.hpp`, `src/screens/gameplay_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Prune restatements of member variables and lifecycle calls.
  - Keep the rationale for why held columns are sampled from `ctx.action_down` instead of replaying events.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/screen.hpp:52-56`
- **Validate**: `scripts/check.sh`

### Task 7: Results and Score Screen

- **File**: `src/screens/results.hpp`, `src/screens/results.cpp`, `src/screens/results_anim.hpp`, `src/screens/results_anim.cpp`, `src/screens/results_art.hpp`, `src/screens/results_art.cpp`, `src/screens/results_screen.hpp`, `src/screens/results_screen.cpp`
- **Action**: UPDATE
- **Implement**:
  - Preserve score-screen layout measurements and mock-up citations in `results_art.hpp` (`:42-80`).
  - Prune `C7` milestone tags from `results_screen.cpp`.
  - Prune obvious animation step and drawing comments.
  - Keep high score record persistence and grade animation rationale.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/results_art.hpp:64-70`
- **Validate**: `scripts/check.sh`

### Task 8: Song Select and Song Display Text

- **File**: `src/screens/song_display_text.hpp`, `src/screens/song_display_text.cpp`, `src/screens/select_screen.hpp`, `src/screens/select_screen.cpp`, `src/screens/select_art.hpp`, `src/screens/select_art.cpp`
- **Action**: UPDATE
- **Implement**:
  - In `song_display_text.hpp`, preserve SM5/OpenITG citations and subtitle budget sharing rationale (`:18,42,50-52,69-70,84-86`); prune pseudocode duplicate arithmetic comments (`:73-80`).
  - In `select_art.hpp`, preserve mock measurement discrepancies (#122, #126, #131) and Edit row custom-rendering rationale (#84, #130); prune trivial tick/marker/label restatements.
  - In `select_screen.cpp`, prune `C5`/`C6` milestone comments and obvious step comments; keep cursor restoration, repeat acceleration, and modal back-handling rationales.
  - In `select_art.cpp`, prune redundant math and draw comments.
  - Run `scripts/format.sh` on changed files.
- **Mirror**: `src/screens/select_art.hpp:64-71`
- **Validate**: `scripts/check.sh`

### Task 9: Formatting and Comment-Reduction Verification

- **File**: All 40 files in `src/screens/`, `TODO.md`
- **Action**: CHECK
- **Implement**:
  - Run `scripts/format.sh --check` to ensure all touched lines strictly conform to `.clang-format`.
  - Run scratch check asserting comment line count in `src/screens/*.{cpp,hpp}` is `<= 400` (down from 1,045).
  - Run scratch check asserting no milestone references (`C1`–`C7`, `D1`–`D4`) remain in `src/screens/*.{cpp,hpp}`.
  - Run `scripts/check.sh` (lint + format + build + 53 unit tests).
- **Mirror**: `scripts/check.sh`
- **Validate**: `scripts/check.sh`

---

## Validation

```bash
# Type check / Build with warnings as errors
cmake --build build

# Lint rules
scripts/lint.sh

# Unit tests
ctest --test-dir build --output-on-failure

# Full check script
scripts/check.sh

# Comment reduction verification (fails on HEAD with 1045; passes after pruning <= 400):
test $(grep -E '^\s*(//|/\*|\*)' src/screens/*.{cpp,hpp} | wc -l) -le 400

# Milestone tag purge verification (fails on HEAD with 48; passes after pruning == 0):
test $(grep -nE '\b(C[1-7]|D[1-4])\b' src/screens/*.{cpp,hpp} | wc -l) -eq 0
```

## End-to-End Verification

1. **Verify No Code Logic / Semantic Changes**:
   Run git diff filtering out comment lines and whitespace to confirm zero code alterations:
   ```bash
   git diff -U0 src/screens | grep -vE '^[+-](\s*(//|/\*|\*)|$|@@|diff|index|---|\+\+\+)' | wc -l
   ```
   The result must be 0 (meaning only comment and formatting lines were modified).

2. **Verify All 16 Screen-Related Unit Tests Pass**:
   ```bash
   ctest --test-dir build --output-on-failure -R "(screen|select_art|setup_art|results_art|options_art|title_art)"
   ```
   All tests continue to pass without changes.

3. **Verify Complete Repo Check**:
   ```bash
   scripts/check.sh
   ```
   Ensures zero compiler warnings (`-DBLAZE_WERROR=ON`), clean formatting, and 100% test pass rate.

---

## Risks

| Risk | Mitigation | In-Scope / Out-of-Scope |
|---|---|---|
| Accidental deletion of C++ code or header guards while deleting multiline comments | `scripts/check.sh` compiles with `-DBLAZE_WERROR=ON` and runs all tests after each task; `git diff` review confirms only comment lines were touched. | In-Scope |
| Inadvertent removal of essential domain constraints (layout pixel budgets, OpenITG metrics references) | Explicitly pinned in plan: `options_menu.cpp:15-42` and `setup_art.hpp:162-179` are protected by specific retention rules. | In-Scope |
| Clang-format drift on lines adjacent to removed comments | Every task runs `scripts/format.sh`, and Task 9 verifies with `scripts/format.sh --check`. | In-Scope |
| Future regression re-introducing redundant comments in `src/screens` | Enforced at code review time; out-of-scope for automated build gates. | Out-of-Scope |

---

## Open Questions

- **Question**: Should section header banners (e.g. `// --- Constants (reference px) ---` and `// Pure layout`) in headers like `select_art.hpp` and `setup_art.hpp` be kept or pruned?  
  **Proposed default**: KEEP concise section dividers that group constants, types, pure layout helpers, and draw helpers in large header files (such as `select_art.hpp` and `setup_art.hpp`) to maintain readability, but prune individual per-function restatements under them.

- **Question**: The acceptance criteria allows "plus a rename where a removed comment was explaining a name". Are there identifiers in `src/screens` that require renaming?  
  **Proposed default**: Zero renames. All identifiers in `src/screens` (`ScreenContext`, `ScreenManager`, `RemapDisplayRow`, etc.) are already idiomatic and clean. Avoiding renames prevents churn and keeps the diff 100% comment-only.

---

## Acceptance Criteria

- [ ] Only the absolutely necessary comments remain in `src/screens`
- [ ] A comment stays only when it says something the code cannot: why, a constraint, a cited OpenITG/StepMania value or source, or a workaround and what it works around; comments that restate the code are removed
- [ ] `lint-allow(…)` exception comments and licence headers are kept
- [ ] Only comments change, plus a rename where a removed comment was explaining a name; `scripts/check.sh` passes
- [ ] Total comment lines in `src/screens/*.{cpp,hpp}` reduced to `<= 400` (from 1,045)
- [ ] All 48 obsolete milestone tags (`C1`–`C7`, `D1`–`D4`) purged from `src/screens`
