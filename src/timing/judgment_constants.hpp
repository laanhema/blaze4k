#pragma once

#include <array>
#include <string>

namespace blaze4k {

// Data-driven judgment/scoring constants (PRD section 6 pattern 3).
//
// Values are seeded from OpenITG tag commit
//   f2c129fe65c65e4a9b3a691ff35e7717b4e8de51
// using the theme runtime override layer
//   assets/patch-data/Themes/default/metrics.ini
// (theme metrics override the compiled StepMania-4 code defaults in
// src/PrefsManager.cpp). The base windows come from the `[Preferences]`
// section (metrics.ini:90-103), the base/home layer. Dedicated ITG cabinets
// launch with `--type=Preferences-cabinet` (assets/arcade-patch/start-3.sh:17),
// which on top of that only sets `JudgeWindowAdd=0.0015` (metrics.ini:262) and
// a hardware `GlobalOffsetSeconds` that Blaze does not adopt (it calibrates per
// user instead). Blaze ships the cabinet add as its default.
//
// This module must remain pure: it includes only <array> and <string>, with no
// platform, audio, or filesystem/JSON dependencies. Loading lives in
// src/data/judgment_constants_loader.*.

enum class TapJudgment { Fantastic, Excellent, Great, Decent, WayOff, Miss, HitMine, Num };

enum class HoldJudgment { Ok, Ng, Num };

struct TimingWindows {
    double fantastic = 0.0215;
    double excellent = 0.0430;
    double great     = 0.1020;
    double decent    = 0.1350;
    double way_off   = 0.1800;
    double hit_mine  = 0.0700;
    double hold_ok   = 0.3200;
    double hold_roll = 0.3500;
    // OpenITG `PadStickSeconds` (arcade metrics.ini:103; compiled default 0 at
    // src/PrefsManager.cpp:250). The held-over-mine crossing check lags the
    // music by this much, and a held panel only counts once it has been held at
    // least this long (src/Player.cpp:632-646, 1461-1488). Not a judge window:
    // `judge_window_scale/add` must never apply to it. 0 is legal (IsButtonDown).
    double pad_stick = 0.05;
    // OpenITG `JudgeWindowScale` / `JudgeWindowAdd`: every judge window above
    // (not pad_stick) is applied as `base * scale + add` (src/Player.cpp:34-74).
    // Use JudgmentConstants::effective_windows() instead of reading the bases.
    double judge_window_scale = 1.0;
    // RoXoR/OpenITG dedicated-cabinet JudgeWindowAdd (metrics.ini:262,
    // [Preferences-cabinet]; selected by assets/arcade-patch/start-3.sh:17).
    // 0.0 gives the home `[Preferences]` timing (metrics.ini:91).
    double judge_window_add   = 0.0015;
};

// Used for both DP (percent score) and grade weights; the two OpenITG tables
// are identical in the arcade runtime layer.
struct Weights {
    int fantastic = 5;
    int excellent = 4;
    int great     = 2;
    int decent    = 0;
    int way_off   = -6;
    int miss      = -12;
    int hit_mine  = -6;
    int hold_ok   = 5;
    int hold_ng   = 0;
};

struct LifeDeltas {
    double fantastic = 0.008;
    double excellent = 0.008;
    double great     = 0.004;
    double decent    = 0.0;
    double way_off   = -0.050;
    double miss      = -0.100;
    double hit_mine  = -0.050;
    double hold_ok   = 0.008;
    double hold_ng   = -0.080;
    bool merciful_drain = false;

    // OpenITG "hot" penalty: while the bar is full (life >= 1), a WayOff/Miss/
    // hit-mine/hold-NG is forced to this value instead of its table delta
    // (LifeMeterBar.cpp:118-119,174-175). Pinned from
    // src/PrefsManager.cpp at commit f2c129fe65c65e4a9b3a691ff35e7717b4e8de51.
    double hot_downgrade = -0.10;
    // OpenITG "combo-to-regain-life": after any life loss, the next this many
    // positive judgments grant no life (LifeMeterBar.cpp:208-227; default 5).
    int regen_combo_after_miss = 5;
    // Successive losses accumulate the regain debt by `regen_combo_after_miss`,
    // never above `max_regen_combo_after_miss` (LifeMeterBar.cpp:220-227;
    // PrefsManager.cpp:122, default 10).
    int max_regen_combo_after_miss = 10;
    // When a delta would cross the fail threshold, the debt is additionally bumped
    // by `regen_combo_after_fail`, never above `max_regen_combo_after_fail`
    // (LifeMeterBar.cpp:244-253; PrefsManager.cpp:119,121, defaults 10/10).
    int regen_combo_after_fail = 10;
    int max_regen_combo_after_fail = 10;
};

struct GradeTier {
    double min_percent = 0.0;
    const char* label = "";
};

struct JudgmentConstants {
    TimingWindows windows;
    Weights dp_weights;
    Weights grade_weights;
    std::array<GradeTier, 17> grade_tiers{}; // index 0 = highest (quad star)
    LifeDeltas life;

    // OpenITG `MercifulBeginner`: the ITG theme turns it on in `[Preferences]`
    // (assets/patch-data/Themes/default/metrics.ini:157; compiled default false
    // at src/PrefsManager.cpp:128). On a Beginner chart it widens the Way Off
    // window (see effective_windows(bool)), makes an early Way Off display-only
    // (JudgmentEngine), and clamps negative DP/grade weights to 0 (ScoreKeeper).
    // Life is unaffected (LifeMeterBar.cpp has no Beginner branch).
    bool merciful_beginner = true;

    // `if( bIsPlayingBeginner && PREFSMAN->m_bMercifulBeginner && tw==TW_Boo )
    //  fSecs += 0.5f;` (src/Player.cpp:55-56). Way Off (Boo) only.
    static constexpr double kMercifulBeginnerWayOffBonusSeconds = 0.5;

    static const JudgmentConstants& compiled_defaults();

    // True when the MercifulBeginner rules apply to a chart.
    [[nodiscard]] bool merciful_beginner_applies(bool is_beginner) const {
        return is_beginner && merciful_beginner;
    }

    [[nodiscard]] bool validate(std::string* error = nullptr) const;

    // The windows the judgment path must use. Mirrors OpenITG
    // `AdjustedWindowTap` / `AdjustedWindowHold` (src/Player.cpp:34-74): each of
    // fantastic..way_off, hit_mine, hold_ok and hold_roll becomes
    // `base * judge_window_scale + judge_window_add` (scale first, then add).
    // `pad_stick` is copied unchanged (not a judge window). The returned struct
    // has judge_window_scale = 1.0 and judge_window_add = 0.0, so adjusting it a
    // second time is a no-op.
    [[nodiscard]] TimingWindows effective_windows() const;

    // effective_windows(), plus the MercifulBeginner bonus on `way_off` only when
    // merciful_beginner_applies(is_beginner). The bonus is added after
    // `base * scale + add` (src/Player.cpp:49-56); mine, hold and roll windows are
    // never widened. Unlike the scale/add normalization, the bonus is NOT
    // idempotent: never feed the result back in as base windows (it would be
    // added twice). effective_windows(false) == effective_windows().
    [[nodiscard]] TimingWindows effective_windows(bool is_beginner) const;

    // Pure lookups (units: seconds, percent as fraction 0.0-1.0)
    // classify_tap is symmetric (|delta|) and uses effective_windows(is_beginner).
    // Suppressing an *early* Beginner Way Off is the engine's job, not this one's.
    [[nodiscard]] TapJudgment classify_tap(double delta_seconds, bool is_beginner = false) const;
    [[nodiscard]] bool continues_combo(TapJudgment j) const;
    [[nodiscard]] const GradeTier& grade_for_percent(double percent) const;
};

} // namespace blaze4k
