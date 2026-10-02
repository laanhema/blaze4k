#include "timing/judgment_constants.hpp"

#include <cmath>

namespace blaze4k {

const JudgmentConstants& JudgmentConstants::compiled_defaults() {
    // Values pinned from OpenITG commit f2c129fe65c65e4a9b3a691ff35e7717b4e8de51,
    // theme runtime layer assets/patch-data/Themes/default/metrics.ini: base
    // windows from [Preferences] (metrics.ini:90-103), judge_window_add from
    // [Preferences-cabinet] (metrics.ini:262), the section dedicated cabinets
    // launch with (assets/arcade-patch/start-3.sh:17).
    // Hold/roll window holds JudgeWindowSecondsRoll=0.350 from the compiled
    // default (src/PrefsManager.cpp:94) - no arcade override exists.
    // pad_stick holds PadStickSeconds=0.05 from metrics.ini:103 (compiled
    // default 0 at src/PrefsManager.cpp:250).
    static const JudgmentConstants defaults = [] {
        JudgmentConstants c;
        // Timing windows (seconds)
        c.windows.fantastic = 0.0215;
        c.windows.excellent = 0.0430;
        c.windows.great = 0.1020;
        c.windows.decent = 0.1350;
        c.windows.way_off = 0.1800;
        c.windows.hit_mine = 0.0700;
        c.windows.hold_ok = 0.3200;
        c.windows.hold_roll = 0.3500;
        c.windows.pad_stick = 0.05;
        c.windows.judge_window_scale = 1.0;
        // RoXoR/OpenITG dedicated-cabinet JudgeWindowAdd (metrics.ini:262,
        // [Preferences-cabinet]; selected by assets/arcade-patch/start-3.sh:17).
        c.windows.judge_window_add = 0.0015;

        // DP / grade weights (arcade PercentScoreWeight* / GradeWeight*)
        c.dp_weights = Weights{5, 4, 2, 0, -6, -12, -6, 5, 0};
        c.grade_weights = Weights{5, 4, 2, 0, -6, -12, -6, 5, 0};

        // Life deltas (arcade LifeDeltaPercentChange*) plus the OpenITG hot
        // downgrade and RegenComboAfter* defaults (LifeMeterBar.cpp:118-119,208-227,
        // 244-253; PrefsManager.cpp:119-122).
        c.life = LifeDeltas{0.008, 0.008, 0.004, 0.0, -0.050, -0.100, -0.050, 0.008,
                            -0.080, false, -0.10, 5, 10, 10, 10};

        // Grade tiers (arcade [Grade] Tier01..Tier17 thresholds)
        c.grade_tiers = {{
            {1.00, "quad_star"},
            {0.99, "triple_star"},
            {0.98, "double_star"},
            {0.96, "single_star"},
            {0.94, "S+"},
            {0.92, "S"},
            {0.89, "S-"},
            {0.86, "A+"},
            {0.83, "A"},
            {0.80, "A-"},
            {0.76, "B+"},
            {0.72, "B"},
            {0.68, "B-"},
            {0.64, "C+"},
            {0.60, "C"},
            {0.55, "C-"},
            {-1000.0, "D"},
        }};
        return c;
    }();
    return defaults;
}

bool JudgmentConstants::validate(std::string* error) const {
    auto fail = [error](const std::string& reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };

    const double positive_windows[] = {
        windows.fantastic, windows.excellent, windows.great, windows.decent,
        windows.way_off, windows.hit_mine, windows.hold_ok, windows.hold_roll,
    };
    for (double value : positive_windows) {
        if (!std::isfinite(value) || value <= 0.0) {
            return fail("timing window must be finite and > 0");
        }
    }
    // pad_stick is not a judge window: 0 is legal (OpenITG IsButtonDown branch).
    if (!std::isfinite(windows.pad_stick) || windows.pad_stick < 0.0) {
        return fail("pad_stick must be finite and >= 0");
    }
    if (!std::isfinite(windows.judge_window_scale)) {
        return fail("judge_window_scale must be finite");
    }
    if (!std::isfinite(windows.judge_window_add)) {
        return fail("judge_window_add must be finite");
    }
    if (windows.judge_window_scale <= 0.0) {
        return fail("judge_window_scale must be > 0");
    }
    // Base monotonicity (checked below) plus a positive scale and a common add
    // keep the effective windows monotonic, so only positivity is checked here.
    const TimingWindows effective = effective_windows();
    const double effective_windows_list[] = {
        effective.fantastic, effective.excellent, effective.great, effective.decent,
        effective.way_off, effective.hit_mine, effective.hold_ok, effective.hold_roll,
    };
    for (double value : effective_windows_list) {
        if (!std::isfinite(value) || value <= 0.0) {
            return fail("effective timing window (base * scale + add) must be > 0");
        }
    }

    if (windows.fantastic > windows.excellent ||
        windows.excellent > windows.great ||
        windows.great > windows.decent ||
        windows.decent > windows.way_off) {
        return fail("timing windows must be monotonic: fantastic <= excellent <= great <= decent <= way_off");
    }

    for (std::size_t i = 0; i < grade_tiers.size(); ++i) {
        if (!std::isfinite(grade_tiers[i].min_percent)) {
            return fail("grade tier min_percent values must be finite");
        }
        if (i > 0 && grade_tiers[i].min_percent > grade_tiers[i - 1].min_percent) {
            return fail("grade tier min_percent values must be non-increasing");
        }
    }

    const double life_values[] = {
        life.fantastic, life.excellent, life.great, life.decent, life.way_off,
        life.miss, life.hit_mine, life.hold_ok, life.hold_ng, life.hot_downgrade,
    };
    for (double value : life_values) {
        if (!std::isfinite(value)) {
            return fail("life delta must be finite");
        }
    }
    if (life.regen_combo_after_miss < 0 || life.regen_combo_after_fail < 0 ||
        life.max_regen_combo_after_miss < 0 || life.max_regen_combo_after_fail < 0) {
        return fail("regen combo thresholds must be >= 0");
    }

    return true;
}

TimingWindows JudgmentConstants::effective_windows() const {
    // OpenITG AdjustedWindowTap/AdjustedWindowHold (src/Player.cpp:34-74):
    // fSecs *= JudgeWindowScale; fSecs += JudgeWindowAdd.
    const double scale = windows.judge_window_scale;
    const double add = windows.judge_window_add;
    TimingWindows w = windows;
    w.fantastic = windows.fantastic * scale + add;
    w.excellent = windows.excellent * scale + add;
    w.great = windows.great * scale + add;
    w.decent = windows.decent * scale + add;
    w.way_off = windows.way_off * scale + add;
    w.hit_mine = windows.hit_mine * scale + add;
    w.hold_ok = windows.hold_ok * scale + add;
    w.hold_roll = windows.hold_roll * scale + add;
    // pad_stick is not a judge window: copied unchanged.
    w.judge_window_scale = 1.0;
    w.judge_window_add = 0.0;
    return w;
}

TapJudgment JudgmentConstants::classify_tap(double delta_seconds) const {
    // NaN cannot be classified; treat it as a Miss (documented behavior).
    if (std::isnan(delta_seconds)) {
        return TapJudgment::Miss;
    }
    // OpenITG compares against ADJUSTED_WINDOW_TAP(...) with `<=` on each tier
    // (src/Player.cpp:957-961), so every edge is inclusive.
    const TimingWindows w = effective_windows();
    const double delta = std::fabs(delta_seconds);
    if (delta <= w.fantastic) return TapJudgment::Fantastic;
    if (delta <= w.excellent) return TapJudgment::Excellent;
    if (delta <= w.great) return TapJudgment::Great;
    if (delta <= w.decent) return TapJudgment::Decent;
    if (delta <= w.way_off) return TapJudgment::WayOff;
    return TapJudgment::Miss;
}

bool JudgmentConstants::continues_combo(TapJudgment j) const {
    switch (j) {
        case TapJudgment::Fantastic:
        case TapJudgment::Excellent:
        case TapJudgment::Great:
        case TapJudgment::HitMine: // mines are scored but never reset combo
            return true;
        case TapJudgment::Decent:
        case TapJudgment::WayOff:
        case TapJudgment::Miss:
        case TapJudgment::Num:
            return false;
    }
    return false;
}

const GradeTier& JudgmentConstants::grade_for_percent(double percent) const {
    for (const GradeTier& tier : grade_tiers) {
        if (percent >= tier.min_percent) {
            return tier;
        }
    }
    return grade_tiers.back();
}

} // namespace blaze4k
