#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "app/frame_stats.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

bool approx(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

void test_empty() {
    td::FrameStats stats;
    TEST_CHECK(stats.empty());
    TEST_CHECK(stats.count() == 0);
    TEST_CHECK(stats.total_ms() == 0.0);
    TEST_CHECK(stats.min_ms() == 0.0);
    TEST_CHECK(stats.mean_ms() == 0.0);
    TEST_CHECK(stats.median_ms() == 0.0);
    TEST_CHECK(stats.max_ms() == 0.0);
    TEST_CHECK(stats.percentile_ms(50.0) == 0.0);
    TEST_CHECK(stats.over_budget(10.0) == 0);
    std::cout << "  - empty collector queries are all zero ok.\n";
}

void test_basic_statistics() {
    td::FrameStats stats;
    for (int i = 1; i <= 100; ++i) {
        stats.add(static_cast<double>(i));
    }

    TEST_CHECK(!stats.empty());
    TEST_CHECK(stats.count() == 100);
    TEST_CHECK(approx(stats.total_ms(), 5050.0));
    TEST_CHECK(approx(stats.min_ms(), 1.0));
    TEST_CHECK(approx(stats.max_ms(), 100.0));
    TEST_CHECK(approx(stats.mean_ms(), 50.5));
    // Nearest-rank lower median for an even count: index = ceil(50/100*100)-1 = 49.
    TEST_CHECK(approx(stats.median_ms(), 50.0));
    TEST_CHECK(approx(stats.percentile_ms(95.0), 95.0));
    TEST_CHECK(approx(stats.percentile_ms(99.0), 99.0));
    std::cout << "  - basic min/max/mean/median/percentile ok.\n";
}

void test_percentile_nearest_rank_rule() {
    // Five samples make each rank unambiguous: index = ceil(p/100*5)-1.
    td::FrameStats stats;
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        stats.add(v);
    }
    TEST_CHECK(approx(stats.percentile_ms(1.0), 10.0));    // index 0
    TEST_CHECK(approx(stats.percentile_ms(20.0), 10.0));   // ceil(1)=1 -> 0
    TEST_CHECK(approx(stats.percentile_ms(21.0), 20.0));   // ceil(1.05)=2 -> 1
    TEST_CHECK(approx(stats.percentile_ms(50.0), 30.0));   // ceil(2.5)=3 -> 2
    TEST_CHECK(approx(stats.percentile_ms(80.0), 40.0));   // ceil(4)=4 -> 3
    TEST_CHECK(approx(stats.percentile_ms(100.0), 50.0));  // ceil(5)=5 -> 4

    // Insertion order must not matter (it sorts a copy).
    td::FrameStats reversed;
    for (double v : {50.0, 40.0, 30.0, 20.0, 10.0}) {
        reversed.add(v);
    }
    TEST_CHECK(approx(reversed.percentile_ms(50.0), stats.percentile_ms(50.0)));
    std::cout << "  - nearest-rank percentile rule + order independence ok.\n";
}

void test_invalid_samples_ignored() {
    td::FrameStats stats;
    stats.add(std::numeric_limits<double>::quiet_NaN());
    stats.add(std::numeric_limits<double>::infinity());
    stats.add(-std::numeric_limits<double>::infinity());
    stats.add(-1.0);
    TEST_CHECK(stats.empty());
    TEST_CHECK(stats.count() == 0);

    stats.add(5.0);
    TEST_CHECK(stats.count() == 1);
    stats.add(-0.5);
    TEST_CHECK(stats.count() == 1);
    std::cout << "  - non-finite and negative samples are ignored ok.\n";
}

void test_percentile_clamping() {
    td::FrameStats stats;
    stats.add(10.0);
    stats.add(20.0);
    stats.add(30.0);

    TEST_CHECK(approx(stats.percentile_ms(-5.0), stats.min_ms()));
    TEST_CHECK(approx(stats.percentile_ms(0.0), stats.min_ms()));
    TEST_CHECK(approx(stats.percentile_ms(150.0), stats.max_ms()));
    TEST_CHECK(approx(stats.percentile_ms(100.0), stats.max_ms()));
    std::cout << "  - percentile clamps to [0, 100] ok.\n";
}

void test_over_budget_and_reset() {
    td::FrameStats stats;
    stats.add(10.0);
    stats.add(16.0);
    stats.add(16.67);
    stats.add(20.0);

    // Strictly greater than the budget.
    TEST_CHECK(stats.over_budget(10.0) == 3);
    TEST_CHECK(stats.over_budget(16.0) == 2);
    TEST_CHECK(stats.over_budget(20.0) == 0);
    TEST_CHECK(stats.over_budget(0.0) == 4);

    stats.reset();
    TEST_CHECK(stats.empty());
    TEST_CHECK(stats.count() == 0);
    TEST_CHECK(stats.over_budget(0.0) == 0);
    std::cout << "  - over_budget strict-greater + reset ok.\n";
}

} // namespace

int main() {
    std::cout << "[frame_stats_test] Running FrameStats tests...\n";
    test_empty();
    test_basic_statistics();
    test_percentile_nearest_rank_rule();
    test_invalid_samples_ignored();
    test_percentile_clamping();
    test_over_budget_and_reset();
    std::cout << "[frame_stats_test] All tests passed!\n";
    return 0;
}
