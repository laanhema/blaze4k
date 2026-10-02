#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "data/judgment_constants_loader.hpp"
#include "timing/judgment_constants.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

constexpr double kEps = 1e-12;

bool nearly(double a, double b) {
    return std::fabs(a - b) <= kEps;
}

bool same_weights(const blaze4k::Weights& a, const blaze4k::Weights& b) {
    return a.fantastic == b.fantastic && a.excellent == b.excellent &&
           a.great == b.great && a.decent == b.decent &&
           a.way_off == b.way_off && a.miss == b.miss &&
           a.hit_mine == b.hit_mine && a.hold_ok == b.hold_ok &&
           a.hold_ng == b.hold_ng;
}

bool same_constants(const blaze4k::JudgmentConstants& a, const blaze4k::JudgmentConstants& b) {
    if (!nearly(a.windows.fantastic, b.windows.fantastic) ||
        !nearly(a.windows.excellent, b.windows.excellent) ||
        !nearly(a.windows.great, b.windows.great) ||
        !nearly(a.windows.decent, b.windows.decent) ||
        !nearly(a.windows.way_off, b.windows.way_off) ||
        !nearly(a.windows.hit_mine, b.windows.hit_mine) ||
        !nearly(a.windows.hold_ok, b.windows.hold_ok) ||
        !nearly(a.windows.hold_roll, b.windows.hold_roll) ||
        !nearly(a.windows.pad_stick, b.windows.pad_stick) ||
        !nearly(a.windows.judge_window_scale, b.windows.judge_window_scale) ||
        !nearly(a.windows.judge_window_add, b.windows.judge_window_add)) {
        return false;
    }
    if (!same_weights(a.dp_weights, b.dp_weights) ||
        !same_weights(a.grade_weights, b.grade_weights)) {
        return false;
    }
    if (!nearly(a.life.fantastic, b.life.fantastic) ||
        !nearly(a.life.excellent, b.life.excellent) ||
        !nearly(a.life.great, b.life.great) ||
        !nearly(a.life.decent, b.life.decent) ||
        !nearly(a.life.way_off, b.life.way_off) ||
        !nearly(a.life.miss, b.life.miss) ||
        !nearly(a.life.hit_mine, b.life.hit_mine) ||
        !nearly(a.life.hold_ok, b.life.hold_ok) ||
        !nearly(a.life.hold_ng, b.life.hold_ng) ||
        a.life.merciful_drain != b.life.merciful_drain ||
        !nearly(a.life.hot_downgrade, b.life.hot_downgrade) ||
        a.life.regen_combo_after_miss != b.life.regen_combo_after_miss ||
        a.life.regen_combo_after_fail != b.life.regen_combo_after_fail ||
        a.life.max_regen_combo_after_miss != b.life.max_regen_combo_after_miss ||
        a.life.max_regen_combo_after_fail != b.life.max_regen_combo_after_fail) {
        return false;
    }
    for (std::size_t i = 0; i < a.grade_tiers.size(); ++i) {
        if (!nearly(a.grade_tiers[i].min_percent, b.grade_tiers[i].min_percent)) {
            return false;
        }
        if (std::strcmp(a.grade_tiers[i].label, b.grade_tiers[i].label) != 0) {
            return false;
        }
    }
    return true;
}

void write_file(const fs::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
}

fs::path find_seed_file() {
    const char* candidates[] = {
        "assets/data/judgment_constants.json",
        "../assets/data/judgment_constants.json",
        "../../assets/data/judgment_constants.json",
    };
    for (const char* candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

} // namespace

int main() {
    std::cout << "[judgment_constants_test] Starting judgment constants tests...\n";

    const blaze4k::JudgmentConstants& defaults = blaze4k::JudgmentConstants::compiled_defaults();

    // 1. Defaults parity vs OpenITG source commit.
    TEST_CHECK(nearly(defaults.windows.fantastic, 0.0215));
    TEST_CHECK(nearly(defaults.windows.excellent, 0.0430));
    TEST_CHECK(nearly(defaults.windows.great, 0.1020));
    TEST_CHECK(nearly(defaults.windows.decent, 0.1350));
    TEST_CHECK(nearly(defaults.windows.way_off, 0.1800));
    TEST_CHECK(nearly(defaults.windows.hit_mine, 0.0700));
    TEST_CHECK(nearly(defaults.windows.hold_ok, 0.3200));
    TEST_CHECK(nearly(defaults.windows.hold_roll, 0.3500));
    // PadStickSeconds=0.05 (arcade metrics.ini:103; #56).
    TEST_CHECK(nearly(defaults.windows.pad_stick, 0.05));
    TEST_CHECK(nearly(defaults.windows.judge_window_scale, 1.0));
    // RoXoR/OpenITG dedicated-cabinet JudgeWindowAdd (metrics.ini:262,
    // [Preferences-cabinet]; selected by assets/arcade-patch/start-3.sh:17).
    TEST_CHECK(nearly(defaults.windows.judge_window_add, 0.0015));
    TEST_CHECK(same_weights(defaults.dp_weights, blaze4k::Weights{5, 4, 2, 0, -6, -12, -6, 5, 0}));
    TEST_CHECK(same_weights(defaults.grade_weights, blaze4k::Weights{5, 4, 2, 0, -6, -12, -6, 5, 0}));
    TEST_CHECK(nearly(defaults.life.fantastic, 0.008));
    TEST_CHECK(nearly(defaults.life.excellent, 0.008));
    TEST_CHECK(nearly(defaults.life.great, 0.004));
    TEST_CHECK(nearly(defaults.life.decent, 0.0));
    TEST_CHECK(nearly(defaults.life.way_off, -0.050));
    TEST_CHECK(nearly(defaults.life.miss, -0.100));
    TEST_CHECK(nearly(defaults.life.hit_mine, -0.050));
    TEST_CHECK(nearly(defaults.life.hold_ok, 0.008));
    TEST_CHECK(nearly(defaults.life.hold_ng, -0.080));
    TEST_CHECK(!defaults.life.merciful_drain);
    TEST_CHECK(nearly(defaults.life.hot_downgrade, -0.10));
    TEST_CHECK(defaults.life.regen_combo_after_miss == 5);
    TEST_CHECK(defaults.life.regen_combo_after_fail == 10);
    TEST_CHECK(defaults.life.max_regen_combo_after_miss == 10);
    TEST_CHECK(defaults.life.max_regen_combo_after_fail == 10);
    const double expected_tiers[] = {1.00, 0.99, 0.98, 0.96, 0.94, 0.92, 0.89,
                                     0.86, 0.83, 0.80, 0.76, 0.72, 0.68, 0.64, 0.60, 0.55, -1000};
    TEST_CHECK(defaults.grade_tiers.size() == 17);
    for (std::size_t i = 0; i < defaults.grade_tiers.size(); ++i) {
        TEST_CHECK(nearly(defaults.grade_tiers[i].min_percent, expected_tiers[i]));
    }
    std::string validation_error;
    TEST_CHECK(defaults.validate(&validation_error));
    std::cout << "  - 1. compiled defaults match pinned OpenITG values.\n";

    // 1b. Effective windows = base * scale + add (OpenITG AdjustedWindowTap/Hold,
    //     src/Player.cpp:34-74). pad_stick is never adjusted.
    const blaze4k::TimingWindows w = defaults.effective_windows();
    TEST_CHECK(nearly(w.fantastic, 0.0230));
    TEST_CHECK(nearly(w.excellent, 0.0445));
    TEST_CHECK(nearly(w.great, 0.1035));
    TEST_CHECK(nearly(w.decent, 0.1365));
    TEST_CHECK(nearly(w.way_off, 0.1815));
    TEST_CHECK(nearly(w.hit_mine, 0.0715));
    TEST_CHECK(nearly(w.hold_ok, 0.3215));
    TEST_CHECK(nearly(w.hold_roll, 0.3515));
    TEST_CHECK(w.pad_stick == 0.05);
    TEST_CHECK(w.judge_window_scale == 1.0);
    TEST_CHECK(w.judge_window_add == 0.0);
    {
        // Re-adjusting an already effective set is a no-op.
        blaze4k::JudgmentConstants twice = defaults;
        twice.windows = w;
        const blaze4k::TimingWindows w2 = twice.effective_windows();
        TEST_CHECK(w2.fantastic == w.fantastic);
        TEST_CHECK(w2.hold_roll == w.hold_roll);
    }
    {
        // Scale is applied before add (Player.cpp:49-50).
        blaze4k::JudgmentConstants scaled = defaults;
        scaled.windows.judge_window_scale = 2.0;
        scaled.windows.judge_window_add = 0.01;
        TEST_CHECK(scaled.validate());
        const blaze4k::TimingWindows sw = scaled.effective_windows();
        TEST_CHECK(nearly(sw.fantastic, 0.0215 * 2.0 + 0.01));
        TEST_CHECK(nearly(sw.way_off, 0.18 * 2.0 + 0.01));
        TEST_CHECK(nearly(sw.hit_mine, 0.07 * 2.0 + 0.01));
        TEST_CHECK(nearly(sw.hold_ok, 0.32 * 2.0 + 0.01));
        TEST_CHECK(nearly(sw.hold_roll, 0.35 * 2.0 + 0.01));
        TEST_CHECK(sw.pad_stick == 0.05);
        TEST_CHECK(scaled.classify_tap(0.0215 * 2.0 + 0.01) == blaze4k::TapJudgment::Fantastic);
        TEST_CHECK(scaled.classify_tap(0.0215 * 2.0 + 0.011) == blaze4k::TapJudgment::Excellent);
    }
    std::cout << "  - 1b. effective windows apply base * scale + add (pad_stick unadjusted).\n";

    // 2. classify_tap boundaries on the effective windows: every edge is
    //    inclusive and symmetric (OpenITG `<=`, Player.cpp:935,957-961), NaN -> Miss.
    {
        const double edges[] = {w.fantastic, w.excellent, w.great, w.decent, w.way_off};
        const blaze4k::TapJudgment tiers[] = {
            blaze4k::TapJudgment::Fantastic, blaze4k::TapJudgment::Excellent,
            blaze4k::TapJudgment::Great, blaze4k::TapJudgment::Decent,
            blaze4k::TapJudgment::WayOff, blaze4k::TapJudgment::Miss,
        };
        for (std::size_t i = 0; i < 5; ++i) {
            const double e = edges[i];
            TEST_CHECK(defaults.classify_tap(e) == tiers[i]);
            TEST_CHECK(defaults.classify_tap(-e) == tiers[i]);
            TEST_CHECK(defaults.classify_tap(std::nextafter(e, 1.0)) == tiers[i + 1]);
            TEST_CHECK(defaults.classify_tap(-std::nextafter(e, 1.0)) == tiers[i + 1]);
        }
    }
    // The base Fantastic edge (21.5 ms) is now inside the effective 23.0 ms window.
    TEST_CHECK(defaults.classify_tap(0.0215 + kEps) == blaze4k::TapJudgment::Fantastic);
    TEST_CHECK(defaults.classify_tap(0.0) == blaze4k::TapJudgment::Fantastic);
    TEST_CHECK(defaults.classify_tap(std::nan("")) == blaze4k::TapJudgment::Miss);
    std::cout << "  - 2. classify_tap boundary and NaN behavior correct.\n";

    // 3. Combo semantics (Decent breaks combo).
    TEST_CHECK(defaults.continues_combo(blaze4k::TapJudgment::Fantastic));
    TEST_CHECK(defaults.continues_combo(blaze4k::TapJudgment::Excellent));
    TEST_CHECK(defaults.continues_combo(blaze4k::TapJudgment::Great));
    TEST_CHECK(defaults.continues_combo(blaze4k::TapJudgment::HitMine));
    TEST_CHECK(!defaults.continues_combo(blaze4k::TapJudgment::Decent));
    TEST_CHECK(!defaults.continues_combo(blaze4k::TapJudgment::WayOff));
    TEST_CHECK(!defaults.continues_combo(blaze4k::TapJudgment::Miss));
    std::cout << "  - 3. combo continuation semantics correct.\n";

    // 4. Grade tier lookup.
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(1.0).label, "quad_star") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.99).label, "triple_star") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.98).label, "double_star") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.96).label, "single_star") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.94).label, "S+") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.55).label, "C-") == 0);
    TEST_CHECK(std::strcmp(defaults.grade_for_percent(0.54).label, "D") == 0);
    std::cout << "  - 4. grade tier lookup correct.\n";

    fs::path temp_dir = fs::temp_directory_path() / "td_judgment_constants_test";
    fs::create_directories(temp_dir);

    // 5. Configurable without recompiling (partial override).
    fs::path override_path = temp_dir / "override.json";
    write_file(override_path,
               "{\"windows_seconds\": {\"great\": 0.050}, \"dp_weights\": {\"fantastic\": 9}, "
               "\"life_deltas\": {\"hot_downgrade\": -0.2, \"regen_combo_after_miss\": 7, "
               "\"regen_combo_after_fail\": 8, \"max_regen_combo_after_miss\": 9, "
               "\"max_regen_combo_after_fail\": 11}}");
    std::string override_message;
    blaze4k::JudgmentConstants overridden = blaze4k::load_judgment_constants(override_path, &override_message);
    TEST_CHECK(nearly(overridden.windows.great, 0.050));
    TEST_CHECK(overridden.dp_weights.fantastic == 9);
    TEST_CHECK(overridden.dp_weights.excellent == 4);
    TEST_CHECK(nearly(overridden.windows.fantastic, defaults.windows.fantastic));
    TEST_CHECK(overridden.classify_tap(0.048) == blaze4k::TapJudgment::Great);
    TEST_CHECK(overridden.classify_tap(0.020) == blaze4k::TapJudgment::Fantastic);
    TEST_CHECK(nearly(overridden.life.hot_downgrade, -0.2));
    TEST_CHECK(overridden.life.regen_combo_after_miss == 7);
    TEST_CHECK(overridden.life.regen_combo_after_fail == 8);
    TEST_CHECK(overridden.life.max_regen_combo_after_miss == 9);
    TEST_CHECK(overridden.life.max_regen_combo_after_fail == 11);
    TEST_CHECK(nearly(overridden.life.miss, defaults.life.miss));
    std::cout << "  - 5. configurable via JSON without recompiling.\n";

    // 5b. pad_stick is overridable and 0 is legal (OpenITG IsButtonDown branch).
    fs::path pad_stick_path = temp_dir / "pad_stick.json";
    write_file(pad_stick_path, "{\"windows_seconds\": {\"pad_stick\": 0.0}}");
    std::string pad_stick_message;
    blaze4k::JudgmentConstants pad_stick_zero =
        blaze4k::load_judgment_constants(pad_stick_path, &pad_stick_message);
    TEST_CHECK(nearly(pad_stick_zero.windows.pad_stick, 0.0));
    TEST_CHECK(pad_stick_zero.validate());
    TEST_CHECK(nearly(pad_stick_zero.windows.hit_mine, defaults.windows.hit_mine));
    std::cout << "  - 5b. pad_stick override (0 allowed) loads.\n";

    // 6. Seed file parity.
    fs::path seed_path = find_seed_file();
    TEST_CHECK(!seed_path.empty());
    std::string seed_message;
    blaze4k::JudgmentConstants seeded = blaze4k::load_judgment_constants(seed_path, &seed_message);
    TEST_CHECK(same_constants(seeded, defaults));
    std::cout << "  - 6. shipped seed JSON deep-equals compiled defaults.\n";

    // 7. Missing file fallback.
    std::string missing_message;
    blaze4k::JudgmentConstants missing = blaze4k::load_judgment_constants(temp_dir / "does_not_exist.json",
                                                               &missing_message);
    TEST_CHECK(same_constants(missing, defaults));
    TEST_CHECK(!missing_message.empty());
    std::cout << "  - 7. missing file falls back with warning: " << missing_message << "\n";

    // 8. Malformed JSON fallback.
    fs::path malformed_path = temp_dir / "malformed.json";
    write_file(malformed_path, "{ not json");
    std::string malformed_message;
    blaze4k::JudgmentConstants malformed = blaze4k::load_judgment_constants(malformed_path, &malformed_message);
    TEST_CHECK(same_constants(malformed, defaults));
    TEST_CHECK(!malformed_message.empty());
    std::cout << "  - 8. malformed JSON falls back with warning.\n";

    // 9a. Invalid values fallback.
    fs::path invalid_path = temp_dir / "invalid.json";
    write_file(invalid_path, "{\"windows_seconds\": {\"great\": -1}}");
    std::string invalid_message;
    blaze4k::JudgmentConstants invalid = blaze4k::load_judgment_constants(invalid_path, &invalid_message);
    TEST_CHECK(same_constants(invalid, defaults));
    TEST_CHECK(!invalid_message.empty());

    // 9b. Non-monotonic windows fallback.
    fs::path nonmono_path = temp_dir / "nonmono.json";
    write_file(nonmono_path, "{\"windows_seconds\": {\"fantastic\": 0.5}}");
    std::string nonmono_message;
    blaze4k::JudgmentConstants nonmono = blaze4k::load_judgment_constants(nonmono_path, &nonmono_message);
    TEST_CHECK(same_constants(nonmono, defaults));
    TEST_CHECK(!nonmono_message.empty());

    // 9b2. Negative pad_stick is rejected and falls back.
    fs::path bad_pad_stick_path = temp_dir / "bad_pad_stick.json";
    write_file(bad_pad_stick_path, "{\"windows_seconds\": {\"pad_stick\": -0.01}}");
    std::string bad_pad_stick_message;
    blaze4k::JudgmentConstants bad_pad_stick =
        blaze4k::load_judgment_constants(bad_pad_stick_path, &bad_pad_stick_message);
    TEST_CHECK(same_constants(bad_pad_stick, defaults));
    TEST_CHECK(!bad_pad_stick_message.empty());
    {
        blaze4k::JudgmentConstants negative = defaults;
        negative.windows.pad_stick = -0.01;
        std::string reason;
        TEST_CHECK(!negative.validate(&reason));
        TEST_CHECK(reason.find("pad_stick") != std::string::npos);
    }

    // 9b3. judge_window_scale must be > 0; the effective windows must stay > 0.
    {
        blaze4k::JudgmentConstants zero_scale = defaults;
        zero_scale.windows.judge_window_scale = 0.0;
        std::string reason;
        TEST_CHECK(!zero_scale.validate(&reason));
        TEST_CHECK(reason.find("judge_window_scale") != std::string::npos);

        blaze4k::JudgmentConstants negative_add = defaults;
        negative_add.windows.judge_window_add = -0.03;
        reason.clear();
        TEST_CHECK(!negative_add.validate(&reason));
        TEST_CHECK(reason.find("effective") != std::string::npos);
    }
    fs::path zero_scale_path = temp_dir / "zero_scale.json";
    write_file(zero_scale_path, "{\"windows_seconds\": {\"judge_window_scale\": 0.0}}");
    std::string zero_scale_message;
    blaze4k::JudgmentConstants zero_scale_loaded =
        blaze4k::load_judgment_constants(zero_scale_path, &zero_scale_message);
    TEST_CHECK(same_constants(zero_scale_loaded, defaults));
    TEST_CHECK(!zero_scale_message.empty());

    fs::path negative_add_path = temp_dir / "negative_add.json";
    write_file(negative_add_path, "{\"windows_seconds\": {\"judge_window_add\": -0.03}}");
    std::string negative_add_message;
    blaze4k::JudgmentConstants negative_add_loaded =
        blaze4k::load_judgment_constants(negative_add_path, &negative_add_message);
    TEST_CHECK(same_constants(negative_add_loaded, defaults));
    TEST_CHECK(!negative_add_message.empty());

    // 9b4. judge_window_add = 0 restores the home [Preferences] timing (metrics.ini:91).
    fs::path home_add_path = temp_dir / "home_add.json";
    write_file(home_add_path, "{\"windows_seconds\": {\"judge_window_add\": 0.0}}");
    std::string home_add_message;
    blaze4k::JudgmentConstants home_add = blaze4k::load_judgment_constants(home_add_path, &home_add_message);
    TEST_CHECK(home_add.windows.judge_window_add == 0.0);
    TEST_CHECK(home_add.validate());
    TEST_CHECK(home_add.effective_windows().fantastic == 0.0215);
    TEST_CHECK(home_add.classify_tap(0.0215 + kEps) == blaze4k::TapJudgment::Excellent);
    std::cout << "  - 9b. bad judge_window_scale/add rejected; add 0 gives home timing.\n";

    // 9c. Partial file: absent keys keep compiled defaults.
    fs::path partial_path = temp_dir / "partial.json";
    write_file(partial_path, "{\"dp_weights\": {\"miss\": -20}}");
    std::string partial_message;
    blaze4k::JudgmentConstants partial = blaze4k::load_judgment_constants(partial_path, &partial_message);
    TEST_CHECK(partial.dp_weights.miss == -20);
    TEST_CHECK(partial.dp_weights.fantastic == defaults.dp_weights.fantastic);
    TEST_CHECK(nearly(partial.windows.great, defaults.windows.great));
    TEST_CHECK(same_weights(partial.grade_weights, defaults.grade_weights));
    std::cout << "  - 9. invalid values fall back; partial overrides merge over defaults.\n";

    // 10. Integer overrides must be integral and in range.
    fs::path fractional_path = temp_dir / "fractional.json";
    write_file(fractional_path, "{\"dp_weights\": {\"fantastic\": 4.9}}");
    std::string fractional_message;
    blaze4k::JudgmentConstants fractional = blaze4k::load_judgment_constants(fractional_path, &fractional_message);
    TEST_CHECK(same_constants(fractional, defaults));
    TEST_CHECK(!fractional_message.empty());

    fs::path huge_path = temp_dir / "huge.json";
    write_file(huge_path, "{\"dp_weights\": {\"fantastic\": 1e20}}");
    std::string huge_message;
    blaze4k::JudgmentConstants huge = blaze4k::load_judgment_constants(huge_path, &huge_message);
    TEST_CHECK(same_constants(huge, defaults));
    TEST_CHECK(!huge_message.empty());
    std::cout << "  - 10. non-integral/out-of-range integer weights fall back.\n";

    // 11. Candidate precedence: the first existing file wins; status reports a file load.
    fs::path candidate_a = temp_dir / "candidate_a.json";
    fs::path candidate_b = temp_dir / "candidate_b.json";
    write_file(candidate_a, "{\"dp_weights\": {\"fantastic\": 7}}");
    write_file(candidate_b, "{\"dp_weights\": {\"fantastic\": 8}}");
    std::string candidates_message;
    blaze4k::ConstantsLoadStatus candidates_status = blaze4k::ConstantsLoadStatus::UsedDefaults;
    blaze4k::JudgmentConstants from_candidates = blaze4k::load_judgment_constants_from_candidates(
        {candidate_a, candidate_b}, &candidates_message, &candidates_status);
    TEST_CHECK(from_candidates.dp_weights.fantastic == 7);
    TEST_CHECK(candidates_status == blaze4k::ConstantsLoadStatus::LoadedFromFile);
    TEST_CHECK(!candidates_message.empty());

    // 12. No candidate found: compiled defaults + fallback status.
    std::string none_message;
    blaze4k::ConstantsLoadStatus none_status = blaze4k::ConstantsLoadStatus::LoadedFromFile;
    blaze4k::JudgmentConstants none = blaze4k::load_judgment_constants_from_candidates(
        {temp_dir / "missing_a.json", temp_dir / "missing_b.json"}, &none_message, &none_status);
    TEST_CHECK(same_constants(none, defaults));
    TEST_CHECK(none_status == blaze4k::ConstantsLoadStatus::UsedDefaults);
    TEST_CHECK(!none_message.empty());
    std::cout << "  - 11. candidate loading precedence and no-candidate fallback correct.\n";

    fs::remove_all(temp_dir);

    std::cout << "[judgment_constants_test] All judgment constants tests passed successfully!\n";
    return 0;
}
