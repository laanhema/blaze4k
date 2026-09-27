#include <cmath>
#include <cstdlib>
#include <iostream>

#include "timing/offset_calibration.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::CalibrationConfig;
using td::CalibrationResult;
using td::OffsetCalibration;

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

void test_schedule() {
    CalibrationConfig config;
    TEST_CHECK(near(config.beat_time(0), config.lead_in_seconds));
    TEST_CHECK(near(config.beat_time(3), config.lead_in_seconds + 1.5));

    TEST_CHECK(config.nearest_beat_index(config.lead_in_seconds) == 0);
    TEST_CHECK(config.nearest_beat_index(config.lead_in_seconds + 0.3) == 1);
    TEST_CHECK(config.nearest_beat_index(config.lead_in_seconds + 0.2) == 0);
    TEST_CHECK(config.nearest_beat_index(-100.0) == 0); // clamped below
    TEST_CHECK(config.nearest_beat_index(1e9) == config.max_beats - 1); // clamped above

    std::cout << "  - beat schedule + nearest index ok.\n";
}

void test_zero_bias() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < config.min_samples; ++i) {
        const double beat = config.beat_time(i);
        TEST_CHECK(calib.add_sample(beat, beat));
    }
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(result.accepted == config.min_samples);
    TEST_CHECK(near(result.mean_delta_seconds, 0.0));
    TEST_CHECK(near(result.offset_seconds, 0.0));
    TEST_CHECK(near(result.spread_seconds, 0.0));
    std::cout << "  - on-beat samples -> zero offset ok.\n";
}

void test_late_bias_sign() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < config.min_samples; ++i) {
        const double beat = config.beat_time(i);
        TEST_CHECK(calib.add_sample(beat, beat + 0.030));
    }
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(near(result.mean_delta_seconds, 0.030, 1e-12));
    TEST_CHECK(near(result.offset_seconds, -0.030, 1e-12)); // OQ4 sign pinned
    std::cout << "  - late taps -> negative offset ok.\n";
}

void test_early_bias_sign() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < config.min_samples; ++i) {
        const double beat = config.beat_time(i);
        TEST_CHECK(calib.add_sample(beat, beat - 0.020));
    }
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(near(result.mean_delta_seconds, -0.020, 1e-12));
    TEST_CHECK(near(result.offset_seconds, 0.020, 1e-12));
    std::cout << "  - early taps -> positive offset ok.\n";
}

void test_wild_rejection() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    const double beat = config.beat_time(0);
    TEST_CHECK(!calib.add_sample(beat, beat + 0.5));
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.accepted == 0);
    TEST_CHECK(result.rejected_wild == 1);
    TEST_CHECK(!result.ready);
    std::cout << "  - wild mistap rejected + counted ok.\n";
}

void test_outlier_prune() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < 10; ++i) {
        const double beat = config.beat_time(i);
        TEST_CHECK(calib.add_sample(beat, beat + 0.020));
    }
    const double beat = config.beat_time(10);
    TEST_CHECK(calib.add_sample(beat, beat + 0.150));

    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(near(result.mean_delta_seconds, 0.020, 1e-9));
    TEST_CHECK(result.rejected_outlier >= 1);
    TEST_CHECK(result.spread_seconds < 0.01);
    std::cout << "  - MAD outlier prune ok.\n";
}

void test_readiness_and_cap() {
    CalibrationConfig config;
    config.min_samples = 8;
    config.max_samples = 12;
    OffsetCalibration calib(config);

    for (int i = 0; i < config.min_samples - 1; ++i) {
        calib.add_sample(config.beat_time(i), config.beat_time(i));
    }
    TEST_CHECK(!calib.ready());
    calib.add_sample(config.beat_time(config.min_samples - 1),
                     config.beat_time(config.min_samples - 1));
    TEST_CHECK(calib.ready());

    for (int i = 0; i < 20; ++i) {
        calib.add_sample(config.beat_time(0), config.beat_time(0));
    }
    TEST_CHECK(calib.sample_count() == config.max_samples); // hard cap
    std::cout << "  - readiness at min_samples + cap at max_samples ok.\n";
}

void test_degenerate() {
    CalibrationConfig config;
    OffsetCalibration empty(config);
    const CalibrationResult empty_result = empty.result();
    TEST_CHECK(!empty_result.ready);
    TEST_CHECK(empty_result.accepted == 0);
    TEST_CHECK(near(empty_result.offset_seconds, 0.0));
    TEST_CHECK(std::isfinite(empty_result.offset_seconds));

    OffsetCalibration all_wild(config);
    for (int i = 0; i < config.min_samples + 4; ++i) {
        all_wild.add_sample(config.beat_time(i), config.beat_time(i) + 0.9);
    }
    const CalibrationResult wild_result = all_wild.result();
    TEST_CHECK(!wild_result.ready);
    TEST_CHECK(wild_result.rejected_wild == config.min_samples + 4);
    TEST_CHECK(std::isfinite(wild_result.offset_seconds));

    // reset() clears both samples and the wild counter.
    all_wild.reset();
    TEST_CHECK(all_wild.sample_count() == 0);
    TEST_CHECK(all_wild.result().rejected_wild == 0);
    std::cout << "  - degenerate empty/all-wild cases are safe ok.\n";
}

bool same_result(const CalibrationResult& a, const CalibrationResult& b) {
    return a.ready == b.ready && a.accepted == b.accepted &&
           a.rejected_wild == b.rejected_wild && a.rejected_outlier == b.rejected_outlier &&
           near(a.mean_delta_seconds, b.mean_delta_seconds) &&
           near(a.offset_seconds, b.offset_seconds) && near(a.spread_seconds, b.spread_seconds);
}

void test_determinism() {
    CalibrationConfig config;
    const double deltas[] = {0.010, -0.005, 0.030, 0.015, -0.010, 0.020, 0.025, 0.000, 0.012};

    OffsetCalibration first(config);
    OffsetCalibration second(config);
    for (int i = 0; i < 9; ++i) {
        const double beat = config.beat_time(i);
        first.add_sample(beat, beat + deltas[i]);
        second.add_sample(beat, beat + deltas[i]);
    }
    TEST_CHECK(same_result(first.result(), second.result()));
    std::cout << "  - identical inputs -> identical result ok.\n";
}

} // namespace

int main() {
    std::cout << "[offset_calibration_test] Running OffsetCalibration tests...\n";
    test_schedule();
    test_zero_bias();
    test_late_bias_sign();
    test_early_bias_sign();
    test_wild_rejection();
    test_outlier_prune();
    test_readiness_and_cap();
    test_degenerate();
    test_determinism();
    std::cout << "[offset_calibration_test] All tests passed!\n";
    return 0;
}