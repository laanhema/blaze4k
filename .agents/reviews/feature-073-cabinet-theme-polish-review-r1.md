# Code Review: feature/073-cabinet-theme-polish (Re-review r1)

**Scope**: branch `feature/073-cabinet-theme-polish` vs merge base `65a0721` (main), checked against issue #98
**Recommendation**: APPROVE

## Summary

Re-review of branch `feature/073-cabinet-theme-polish` after commit `9cc7246` ("Fix review findings from #98"). All findings from the initial review (H1, M1, L1) have been addressed and verified. The `video.scanlines` toggle is fully persisted, tested, and honoured across all screens, all 10 screen views are machine-captured at 1280x720, 1920x1080, and 2560x1440, and performance criteria were verified over a full 117-second song per the owner's decision on issue #98.

Acceptance criteria:
1. **Persisted `video.scanlines` toggle + tests: met.** `src/data/config.hpp:29`, `config_loader.cpp:190,400`, `config_persistence_test.cpp` (default, round trip, wrong type, explicit false), `title_screen_test.cpp` `test_scanlines_toggle` via headless recorder. Real window pixel-diff proof (`720p-10-title-scanlines-off.png` vs `720p-01-title.png`, and `1440p-10-title-scanlines-off.png` vs `1440p-01-title.png`) shows over 888k (720p) and 4.01M (1440p) changed pixels between on and off states.
2. **`--perf-report` over full song shows no regression vs before theme: met (per owner decision).** The owner adopted Option A on issue #98 (comment `IC_kwDOUtKPNM8AAAABiAbAjg`), rewording the criterion to "no frame-time regression and fewer quads (34.8 vs 295.5 quads/frame)". Full-song `--perf-report` over 117 s (*Anubis* from *In The Groove*, 7,446 frames, active hardware callbacks via silent audio) demonstrated solid 16.653 ms median frame time (<1 ms jitter at p99 17.541 ms), 6.77% CPU of one core, zero audio dropouts over 11,551 callbacks, and an 88% reduction in quads per frame (`docs/CROSS_PLATFORM_VERIFICATION.md:159-164`).
3. **Cross-platform doc with theme pass and screenshots at 1280x720 and 2560x1440: met.** Documented in `docs/CROSS_PLATFORM_VERIFICATION.md:108-201`. Linux is machine-checked; Windows and macOS owner steps are listed. All 10 views were captured at 1280x720, 1920x1080, and 2560x1440 (via Xvfb on display `:98`) in `docs/cabinet-theme/screenshots/`.
4. **No screen still uses `bitmap_font` for player-facing text: met.** `bitmap_font` is documented as fallback/debug only in `src/render/bitmap_font.hpp:16-23`. Themed screens use `TextRenderer`. `SelectPlaceholderScreen` is not registered in `src/main.cpp`.

## Fix Verification

| ID | Status | Evidence |
|----|--------|----------|
| H1 | fixed | `docs/CROSS_PLATFORM_VERIFICATION.md:159-164` (owner decision comment `IC_kwDOUtKPNM8AAAABiAbAjg` reworded criterion; full 117-second song `--perf-report` measured and documented) |
| M1 | fixed | `docs/CROSS_PLATFORM_VERIFICATION.md:168-170`, `docs/cabinet-theme/screenshots/1440p-*.png` (10 views captured at 2560x1440 via Xvfb) |
| L1 | fixed | `tests/title_screen_test.cpp:25` (`#include "screens/select_art.hpp"` moved to alphabetical position) |

## Issues Found

### Critical
None

### High Priority
None

### Medium Priority
None

### Suggestions (Low)
None

**Noted, not a finding**:
- Windows and macOS verification are owner steps by design (the criterion says "owner steps listed"); they are listed in `docs/CROSS_PLATFORM_VERIFICATION.md:182-201` and marked not verified.
- Options overlay entry for the scanlines toggle is omitted by owner design (config-file only).
- Optional items from the technical notes (rotating results sunburst, 1x backgrounds) were not required and not done.

## Validation Results

| Check | Status | Notes |
|-------|--------|-------|
| Build / Type Check | PASS | `scripts/check.sh` step 4 (warnings as errors `-DBLAZE_WERROR=ON`, host GCC) |
| Warnings | NONE | Recompiled changed project sources with `-DBLAZE_WERROR=ON`; zero warnings |
| Lint | PASS | `scripts/check.sh` steps 1-2 (`lint.sh`, `format.sh --check`) |
| Tests | PASS | `scripts/check.sh` step 5: 100% of 53 passed; sanitizer preset `asan-ubsan`: 100% of 55 passed (no memory errors or UB); tests run on null audio device |
| UI verification | PASS | Implementer captures in `docs/cabinet-theme/screenshots/` (720p, 1080p, 1440p) inspected and pixel-verified; scanlines on/off pixel delta confirmed |

## What's Good

- Clean, robust implementation of the `video.scanlines` toggle across configuration, persistence, rendering, and tests.
- Rigorous follow-up on review findings: full-song `--perf-report` benchmark on real hardware audio callbacks, complete set of 2560x1440 screenshots captured via headless Xvfb, and cleaned up header includes.
- 100% test pass rate under AddressSanitizer + UndefinedBehaviorSanitizer.

## Recommendation

None — the branch is clean, verified, and ready to merge.
