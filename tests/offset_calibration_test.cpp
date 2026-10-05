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

using blaze4k::CalibrationConfig;
using blaze4k::CalibrationResult;
using blaze4k::OffsetCalibration;

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

void test_schedule() {
    CalibrationConfig config;
    TEST_CHECK(near(config.beat_time(0), config.lead_in_seconds));
    TEST_CHECK(near(config.beat_time(3), config.lead_in_seconds + 1.5));

    // #74: the most recent beat no more than max_early after the tap.
    TEST_CHECK(config.matching_beat_index(config.lead_in_seconds) == 0);
    TEST_CHECK(config.matching_beat_index(config.lead_in_seconds - 0.04) == 0);
    TEST_CHECK(config.matching_beat_index(config.lead_in_seconds + 0.3) == 0); // was 1 (the bug)
    TEST_CHECK(config.matching_beat_index(config.lead_in_seconds + 0.44) == 0);
    TEST_CHECK(config.matching_beat_index(config.lead_in_seconds + 0.47) == 1);
    TEST_CHECK(config.matching_beat_index(-100.0) == 0); // clamped below
    TEST_CHECK(config.matching_beat_index(1e9) == config.max_beats - 1); // clamped above
    TEST_CHECK(config.matching_beat_index(std::nan("")) == 0);

    std::cout << "  - beat schedule + matching index ok.\n";
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

// Feeds one tap `delta` after beat i + 1 exactly as the screen does: match the
// hit to its beat via matching_beat_index, then add (beat, hit).
bool feed(OffsetCalibration& calib, const CalibrationConfig& config, int i, double delta) {
    const double hit = config.beat_time(i + 1) + delta;
    const double beat = config.beat_time(config.matching_beat_index(hit));
    return calib.add_sample(beat, hit);
}

void test_config_invariants() {
    const CalibrationConfig config;
    TEST_CHECK(config.max_early_seconds > 0.0);
    TEST_CHECK(config.max_late_seconds > 0.0);
    TEST_CHECK(config.max_early_seconds + config.max_late_seconds < config.beat_period_seconds);
    std::cout << "  - matching window invariants ok.\n";
}

void test_large_late_delays() {
    for (const double delay : {0.28, 0.40}) {
        CalibrationConfig config;
        OffsetCalibration calib(config);
        for (int i = 0; i < config.min_samples; ++i) {
            TEST_CHECK(feed(calib, config, i, delay));
        }
        const CalibrationResult result = calib.result();
        TEST_CHECK(result.ready);
        TEST_CHECK(!result.out_of_range);
        TEST_CHECK(near(result.offset_seconds, -delay));
        TEST_CHECK(result.rejected_wild == 0);
    }
    std::cout << "  - +0.28 / +0.40 s delays -> -0.28 / -0.40 s offsets ok.\n";
}

void test_low_latency_unchanged() {
    for (const double delay : {0.02, -0.03}) {
        CalibrationConfig config;
        OffsetCalibration calib(config);
        for (int i = 0; i < config.min_samples; ++i) {
            TEST_CHECK(feed(calib, config, i, delay));
        }
        const CalibrationResult result = calib.result();
        TEST_CHECK(result.ready);
        TEST_CHECK(!result.out_of_range);
        TEST_CHECK(near(result.offset_seconds, -delay));
    }
    std::cout << "  - low latency + early taps still count ok.\n";
}

void test_cluster_straddles_slot_edge() {
    {
        CalibrationConfig config;
        OffsetCalibration calib(config);
        for (int i = 0; i < config.min_samples; ++i) {
            TEST_CHECK(feed(calib, config, i, (i % 2 == 0) ? 0.38 : 0.41));
        }
        const CalibrationResult result = calib.result();
        TEST_CHECK(result.ready);
        TEST_CHECK(!result.out_of_range);
        TEST_CHECK(near(result.mean_delta_seconds, 0.395));
        TEST_CHECK(result.rejected_outlier == 0);
    }
    {
        // The -0.06 taps match the previous beat (+0.44) and are unwrapped back.
        CalibrationConfig config;
        OffsetCalibration calib(config);
        for (int i = 0; i < config.min_samples; ++i) {
            TEST_CHECK(feed(calib, config, i, (i % 2 == 0) ? -0.02 : -0.06));
        }
        const CalibrationResult result = calib.result();
        TEST_CHECK(result.ready);
        TEST_CHECK(!result.out_of_range);
        TEST_CHECK(near(result.offset_seconds, 0.04));
        TEST_CHECK(result.rejected_outlier == 0);
    }
    std::cout << "  - cluster straddling the slot edge is unwrapped ok.\n";
}

void test_out_of_range() {
    const double patterns[3][2] = {{0.44, 0.44}, {0.43, 0.46}, {-0.07, -0.07}};
    for (const auto& pattern : patterns) {
        CalibrationConfig config;
        OffsetCalibration calib(config);
        for (int i = 0; i < config.min_samples; ++i) {
            TEST_CHECK(feed(calib, config, i, pattern[i % 2]));
        }
        const CalibrationResult result = calib.result();
        TEST_CHECK(result.out_of_range);
        TEST_CHECK(!result.ready);
        TEST_CHECK(calib.ready()); // sample gate met; the result is just not savable
        TEST_CHECK(result.accepted == 8);
        TEST_CHECK(std::isfinite(result.mean_delta_seconds));
        TEST_CHECK(std::isfinite(result.offset_seconds));
        TEST_CHECK(std::isfinite(result.spread_seconds));
    }
    std::cout << "  - out-of-range delays flagged + not ready ok.\n";
}

void test_early_alias_documented_limit() {
    // Documented limit (docs/AUDIO_LATENCY.md, "Limit more than about 80 ms
    // early"): with a 0.5 s click, a net delay of -0.10 s is indistinguishable
    // from +0.40 s one beat later. It is saved as the aliased -0.40 s offset,
    // not flagged out_of_range. Only about -80..-50 ms is refused.
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < config.min_samples; ++i) {
        TEST_CHECK(feed(calib, config, i, -0.10));
    }
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(!result.out_of_range);
    TEST_CHECK(near(result.mean_delta_seconds, 0.40, 1e-9));
    TEST_CHECK(near(result.offset_seconds, -0.40, 1e-9));
    std::cout << "  - -0.10 s early delay aliases to -0.40 s offset (documented limit) ok.\n";
}

void test_mistap_far_from_cluster() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    for (int i = 0; i < 8; ++i) {
        TEST_CHECK(feed(calib, config, i, 0.02));
    }
    TEST_CHECK(feed(calib, config, 8, 0.30)); // accepted per tap, pruned by MAD
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.ready);
    TEST_CHECK(near(result.mean_delta_seconds, 0.02));
    TEST_CHECK(result.rejected_outlier == 1);
    TEST_CHECK(result.rejected_wild == 0);
    std::cout << "  - far mistap accepted then MAD-pruned ok.\n";
}

void test_unwrap_is_identity_in_range() {
    CalibrationConfig config;
    OffsetCalibration calib(config);
    // Spans [-0.04, 0.40], every delta within half a period of the cluster
    // centre (0.18), so the unwrap must shift none of them.
    const double deltas[] = {-0.04, 0.04, 0.08, 0.13, 0.18, 0.23, 0.28, 0.32, 0.40};
    double sum = 0.0;
    for (int i = 0; i < 9; ++i) {
        const double beat = config.beat_time(i);
        const double hit = beat + deltas[i];
        TEST_CHECK(calib.add_sample(beat, hit));
        sum += hit - beat;
    }
    const CalibrationResult result = calib.result();
    TEST_CHECK(result.rejected_outlier == 0);
    TEST_CHECK(near(result.mean_delta_seconds, sum / 9.0, 1e-12));
    std::cout << "  - in-range deltas are not shifted by the unwrap ok.\n";
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
    test_config_invariants();
    test_large_late_delays();
    test_low_latency_unchanged();
    test_cluster_straddles_slot_edge();
    test_out_of_range();
    test_early_alias_documented_limit();
    test_mistap_far_from_cluster();
    test_unwrap_is_identity_in_range();
    std::cout << "[offset_calibration_test] All tests passed!\n";
    return 0;
}