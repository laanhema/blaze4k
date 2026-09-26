#include "timing/judgment_constants.hpp"

#include <cmath>

namespace td {

const JudgmentConstants& JudgmentConstants::compiled_defaults() {
    // Values pinned from OpenITG commit f2c129fe65c65e4a9b3a691ff35e7717b4e8de51,
    // arcade runtime layer assets/patch-data/Themes/default/metrics.ini.
    // Hold/roll window holds JudgeWindowSecondsRoll=0.350 from the compiled
    // default (src/PrefsManager.cpp:94) - no arcade override exists.
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
        c.windows.judge_window_scale = 1.0;
        c.windows.judge_window_add = 0.0;

        // DP / grade weights (arcade PercentScoreWeight* / GradeWeight*)
        c.dp_weights = Weights{5, 4, 2, 0, -6, -12, -6, 5, 0};
        c.grade_weights = Weights{5, 4, 2, 0, -6, -12, -6, 5, 0};

        // Life deltas (arcade LifeDeltaPercentChange*)
        c.life = LifeDeltas{0.008, 0.008, 0.004, 0.0, -0.050, -0.100, -0.050, 0.008, -0.080, false};

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
    if (!std::isfinite(windows.judge_window_scale)) {
        return fail("judge_window_scale must be finite");
    }
    if (!std::isfinite(windows.judge_window_add)) {
        return fail("judge_window_add must be finite");
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
        life.miss, life.hit_mine, life.hold_ok, life.hold_ng,
    };
    for (double value : life_values) {
        if (!std::isfinite(value)) {
            return fail("life delta must be finite");
        }
    }

    return true;
}

TapJudgment JudgmentConstants::classify_tap(double delta_seconds) const {
    // NaN cannot be classified; treat it as a Miss (documented behavior).
    if (std::isnan(delta_seconds)) {
        return TapJudgment::Miss;
    }
    const double delta = std::fabs(delta_seconds);
    if (delta <= windows.fantastic) return TapJudgment::Fantastic;
    if (delta <= windows.excellent) return TapJudgment::Excellent;
    if (delta <= windows.great) return TapJudgment::Great;
    if (delta <= windows.decent) return TapJudgment::Decent;
    if (delta <= windows.way_off) return TapJudgment::WayOff;
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

} // namespace td
