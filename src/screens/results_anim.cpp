#include "screens/results_anim.hpp"

#include <algorithm>
#include <cmath>

namespace td {

namespace {
constexpr double kTwoPi = 6.28318530717958647692;

[[nodiscard]] double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double smoothstep(double t) {
    const double c = clamp01(t);
    return c * c * (3.0 - 2.0 * c);
}
} // namespace

void ResultsAnimator::reset(bool valid, bool new_record, bool failed) {
    valid_ = valid;
    new_record_ = new_record;
    failed_ = failed;
    elapsed_ = 0.0;
}

void ResultsAnimator::update(double fixed_dt) {
    const double dt = fixed_dt > 0.0 ? fixed_dt : 0.0;
    elapsed_ = std::min(elapsed_ + dt, kRevealSeconds);
}

void ResultsAnimator::skip() {
    elapsed_ = kRevealSeconds;
}

float ResultsAnimator::reveal_alpha(double elapsed, double delay, double duration) {
    if (elapsed <= delay) {
        return 0.0f;
    }
    if (duration <= 0.0 || elapsed >= delay + duration) {
        return 1.0f;
    }
    return static_cast<float>(smoothstep((elapsed - delay) / duration));
}

float ResultsAnimator::title_alpha(double elapsed) {
    return reveal_alpha(elapsed, 0.0, kTitleFadeSeconds);
}

float ResultsAnimator::grade_scale(double elapsed) {
    if (elapsed < kGradeDelay) {
        return 1.0f;
    }
    if (elapsed >= kGradeDelay + kGradePopSeconds) {
        return 1.0f;
    }
    const double p = (elapsed - kGradeDelay) / kGradePopSeconds;
    if (p < kGradeSlamPortion) {
        return static_cast<float>(kGradeScaleStart - kGradeScaleDrop * (p / kGradeSlamPortion));
    }
    return static_cast<float>(kGradeSettleScale +
                              kGradeSettleRise *
                                  ((p - kGradeSlamPortion) / (1.0 - kGradeSlamPortion)));
}

float ResultsAnimator::grade_alpha(double elapsed) {
    return reveal_alpha(elapsed, kGradeDelay, kGradeFadeSeconds);
}

double ResultsAnimator::percent_progress(double elapsed) {
    return smoothstep((elapsed - kPercentDelay) / kPercentCountSeconds);
}

float ResultsAnimator::stats_alpha(double elapsed) {
    return reveal_alpha(elapsed, kStatsDelay, kStatsFadeSeconds);
}

float ResultsAnimator::record_alpha(double elapsed) {
    if (elapsed <= kRecordDelay) {
        return 0.0f;
    }
    if (elapsed >= kRecordDelay + kRecordSeconds) {
        return 1.0f;
    }
    const double phase = (elapsed - kRecordDelay) / kRecordPulsePeriod;
    const double pulse = kRecordPulseBase + kRecordPulseAmplitude * std::cos(kTwoPi * phase);
    return static_cast<float>(pulse) * reveal_alpha(elapsed, kRecordDelay, kRecordFadeSeconds);
}

float ResultsAnimator::record_scale(double elapsed) {
    if (elapsed <= kRecordDelay) {
        return 0.0f;
    }
    if (elapsed >= kRecordDelay + kRecordPunchSeconds) {
        return 1.0f;
    }
    const double p = (elapsed - kRecordDelay) / kRecordPunchSeconds;
    if (p < kRecordPunchPeakPortion) {
        return static_cast<float>(kRecordScaleStart +
                                  kRecordScaleRise * (p / kRecordPunchPeakPortion));
    }
    return static_cast<float>(kRecordScalePeak -
                              kRecordScaleFall * ((p - kRecordPunchPeakPortion) /
                                                  (1.0 - kRecordPunchPeakPortion)));
}

float ResultsAnimator::record_flash(double elapsed) {
    if (elapsed <= kRecordDelay) {
        return 0.0f;
    }
    const double t = (elapsed - kRecordDelay) / kRecordFlashSeconds;
    if (t >= 1.0) {
        return 0.0f;
    }
    return static_cast<float>(1.0 - t);
}

float ResultsAnimator::failed_alpha(double elapsed) {
    return reveal_alpha(elapsed, kStatsDelay, kFailedFadeSeconds);
}

} // namespace td
