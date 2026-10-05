#include <cstdlib>
#include <iostream>

#include "screens/results_anim.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::ResultsAnimator;

constexpr double kReveal = ResultsAnimator::kRevealSeconds;

void test_initial_state() {
    ResultsAnimator animator;
    animator.reset(true, false, false);

    TEST_CHECK(!animator.finished());
    TEST_CHECK(animator.valid());
    TEST_CHECK(!animator.new_record());
    TEST_CHECK(!animator.failed());
    TEST_CHECK(animator.elapsed() == 0.0);

    TEST_CHECK(ResultsAnimator::title_alpha(0.0) == 0.0f);
    TEST_CHECK(ResultsAnimator::grade_alpha(0.0) == 0.0f);
    TEST_CHECK(ResultsAnimator::stats_alpha(0.0) == 0.0f);
    TEST_CHECK(ResultsAnimator::failed_alpha(0.0) == 0.0f);
    TEST_CHECK(ResultsAnimator::record_alpha(0.0) == 0.0f);
    TEST_CHECK(ResultsAnimator::record_flash(0.0) == 0.0f);
    std::cout << "  - initial state ok.\n";
}

void test_grade_slam() {
    const double delay = ResultsAnimator::kGradeDelay;
    const double pop = ResultsAnimator::kGradePopSeconds;

    // Settled (1.0) before the slam, 2.4x at the start of the slam.
    TEST_CHECK(ResultsAnimator::grade_scale(0.0) == 1.0f);
    TEST_CHECK(ResultsAnimator::grade_scale(delay) == 2.4f);
    // Ends exactly 1.0 and stays there.
    TEST_CHECK(ResultsAnimator::grade_scale(delay + pop) == 1.0f);
    TEST_CHECK(ResultsAnimator::grade_scale(kReveal) == 1.0f);

    // Undershoots below 1.0 before settling back up.
    TEST_CHECK(ResultsAnimator::grade_scale(delay + pop * 0.7) <= 0.81f);
    TEST_CHECK(ResultsAnimator::grade_scale(delay + pop * 0.9) < 1.0f);

    // Fades in as it slams, reaching full alpha by the end of the slam.
    TEST_CHECK(ResultsAnimator::grade_alpha(delay) == 0.0f);
    TEST_CHECK(ResultsAnimator::grade_alpha(delay + pop) == 1.0f);
    std::cout << "  - grade slam curve ok.\n";
}

void test_percent_count_up() {
    TEST_CHECK(ResultsAnimator::percent_progress(0.0) == 0.0);
    TEST_CHECK(ResultsAnimator::percent_progress(kReveal) == 1.0);
    TEST_CHECK(ResultsAnimator::percent_progress(ResultsAnimator::kPercentDelay) == 0.0);

    double previous = 0.0;
    for (double t = 0.0; t <= kReveal; t += 0.1) {
        const double value = ResultsAnimator::percent_progress(t);
        TEST_CHECK(value >= previous);
        TEST_CHECK(value >= 0.0 && value <= 1.0);
        previous = value;
    }
    std::cout << "  - percent count-up ok.\n";
}

void test_reveal_alphas() {
    // reveal_alpha: 0 before delay, 1 at/after end.
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.0, 0.5, 0.25) == 0.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.5, 0.5, 0.25) == 0.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.75, 0.5, 0.25) == 1.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(1.0, 0.5, 0.25) == 1.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.6, 0.5, 0.25) > 0.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.6, 0.5, 0.25) < 1.0f);

    // Title fades in first; stats/failed are later and settle to 1.
    TEST_CHECK(ResultsAnimator::title_alpha(0.10) > 0.0f);
    TEST_CHECK(ResultsAnimator::title_alpha(0.30) == 1.0f);
    TEST_CHECK(ResultsAnimator::stats_alpha(ResultsAnimator::kStatsDelay) == 0.0f);
    TEST_CHECK(ResultsAnimator::stats_alpha(kReveal) == 1.0f);
    TEST_CHECK(ResultsAnimator::failed_alpha(ResultsAnimator::kStatsDelay) == 0.0f);
    TEST_CHECK(ResultsAnimator::failed_alpha(kReveal) == 1.0f);

    // Degenerate duration returns the settled value without NaN.
    TEST_CHECK(ResultsAnimator::reveal_alpha(1.0, 0.5, 0.0) == 1.0f);
    TEST_CHECK(ResultsAnimator::reveal_alpha(0.1, 0.5, 0.0) == 0.0f);
    std::cout << "  - reveal alphas ok.\n";
}

void test_new_record_finale() {
    ResultsAnimator animator;
    animator.reset(true, true, false);

    const double delay = ResultsAnimator::kRecordDelay;
    TEST_CHECK(ResultsAnimator::record_alpha(delay) == 0.0f);
    TEST_CHECK(ResultsAnimator::record_scale(delay) == 0.0f);
    TEST_CHECK(ResultsAnimator::record_flash(delay) == 0.0f);

    // Active + oscillating pulse window.
    const float a02 = ResultsAnimator::record_alpha(delay + 0.2);
    const float a04 = ResultsAnimator::record_alpha(delay + 0.4);
    const float a06 = ResultsAnimator::record_alpha(delay + 0.6);
    TEST_CHECK(a02 > 0.0f);
    TEST_CHECK(a04 > a02);
    TEST_CHECK(a04 > a06);
    TEST_CHECK(a04 <= 1.0f);

    // Punch overshoots then settles; the finale reaches full alpha at the end.
    TEST_CHECK(ResultsAnimator::record_scale(delay + 0.175) > 1.1f);
    TEST_CHECK(ResultsAnimator::record_scale(kReveal) == 1.0f);
    TEST_CHECK(ResultsAnimator::record_alpha(kReveal) == 1.0f);

    // Flash starts near full and decays to 0.
    TEST_CHECK(ResultsAnimator::record_flash(delay + 0.01) > 0.9f);
    TEST_CHECK(ResultsAnimator::record_flash(delay + 0.35) == 0.0f);
    TEST_CHECK(ResultsAnimator::record_flash(kReveal) == 0.0f);

    // A normal clear never flags the finale (the screen gates all record draws on
    // this flag); the curves before the delay are inert regardless.
    ResultsAnimator normal;
    normal.reset(true, false, false);
    TEST_CHECK(!normal.new_record());
    TEST_CHECK(normal.record_alpha(0.0) == 0.0f);
    std::cout << "  - NEW RECORD finale ok.\n";
}

void test_skip_and_finished() {
    ResultsAnimator animator;
    animator.reset(true, true, false);
    TEST_CHECK(!animator.finished());

    animator.skip();
    TEST_CHECK(animator.finished());
    TEST_CHECK(animator.elapsed() == kReveal);
    TEST_CHECK(ResultsAnimator::record_alpha(animator.elapsed()) == 1.0f);
    TEST_CHECK(ResultsAnimator::percent_progress(animator.elapsed()) == 1.0);
    TEST_CHECK(ResultsAnimator::grade_scale(animator.elapsed()) == 1.0f);

    // Large positive updates clamp to the end; a negative dt never rewinds.
    ResultsAnimator step;
    step.reset(true, false, false);
    step.update(kReveal * 2.0);
    TEST_CHECK(step.finished());
    TEST_CHECK(step.elapsed() == kReveal);
    step.update(-1.0);
    TEST_CHECK(step.elapsed() == kReveal);
    std::cout << "  - skip + finished clamping ok.\n";
}

void test_invalid_reset_is_safe() {
    ResultsAnimator animator;
    animator.reset(false, false, false);
    TEST_CHECK(!animator.valid());
    TEST_CHECK(!animator.finished());
    TEST_CHECK(ResultsAnimator::title_alpha(animator.elapsed()) == 0.0f);
    animator.update(0.1);
    animator.skip();
    TEST_CHECK(animator.finished());
    std::cout << "  - invalid reset safe ok.\n";
}

// Score screen reveal order (#95): the bar text first, then the grade slam on the
// medallion, the percent count-up, the stats and finally the NEW RECORD ribbon;
// everything has settled by kRevealSeconds.
void test_reveal_order() {
    TEST_CHECK(ResultsAnimator::title_alpha(0.05) > 0.0f);
    TEST_CHECK(ResultsAnimator::grade_alpha(0.05) == 0.0f);
    TEST_CHECK(ResultsAnimator::kGradeDelay < ResultsAnimator::kPercentDelay);
    TEST_CHECK(ResultsAnimator::kPercentDelay < ResultsAnimator::kStatsDelay);
    TEST_CHECK(ResultsAnimator::kStatsDelay < ResultsAnimator::kRecordDelay);
    TEST_CHECK(ResultsAnimator::kGradeDelay + ResultsAnimator::kGradePopSeconds <=
               ResultsAnimator::kRecordDelay);
    TEST_CHECK(ResultsAnimator::kRecordDelay + ResultsAnimator::kRecordSeconds <= kReveal);
    TEST_CHECK(ResultsAnimator::kPercentDelay + ResultsAnimator::kPercentCountSeconds <= kReveal);
    std::cout << "  - score screen reveal order ok.\n";
}

} // namespace

int main() {
    std::cout << "[results_anim_test] Running ResultsAnimator (score screen reveal) tests...\n";
    test_initial_state();
    test_grade_slam();
    test_percent_count_up();
    test_reveal_alphas();
    test_new_record_finale();
    test_skip_and_finished();
    test_invalid_reset_is_safe();
    test_reveal_order();
    std::cout << "[results_anim_test] All tests passed!\n";
    return 0;
}
