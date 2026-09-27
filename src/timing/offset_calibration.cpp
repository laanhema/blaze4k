#include "timing/offset_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace td {

namespace {

// Median of an already-sorted sequence (average of the two middle values when
// even). The caller guarantees the sequence is non-empty.
double median_of_sorted(const std::vector<double>& sorted) {
    const std::size_t n = sorted.size();
    const std::size_t mid = n / 2;
    if (n % 2 == 0) {
        return 0.5 * (sorted[mid - 1] + sorted[mid]);
    }
    return sorted[mid];
}

} // namespace

double CalibrationConfig::beat_time(int index) const {
    return lead_in_seconds + static_cast<double>(index) * beat_period_seconds;
}

int CalibrationConfig::nearest_beat_index(double music_seconds) const {
    if (beat_period_seconds <= 0.0 || max_beats <= 0) {
        return 0;
    }
    const double raw = (music_seconds - lead_in_seconds) / beat_period_seconds;
    if (!std::isfinite(raw)) {
        return 0;
    }
    // Clamp in double before rounding: std::lround returns `long`, which is
    // 32-bit on Windows, so an extreme music time could otherwise overflow
    // before the clamp. The clamped value is always in [0, max_beats - 1].
    const double clamped = std::clamp(raw, 0.0, static_cast<double>(max_beats - 1));
    return static_cast<int>(std::lround(clamped));
}

OffsetCalibration::OffsetCalibration(CalibrationConfig config) : config_(config) {}

void OffsetCalibration::reset() {
    samples_.clear();
    rejected_wild_ = 0;
}

bool OffsetCalibration::add_sample(double beat_seconds, double hit_seconds) {
    const double delta = hit_seconds - beat_seconds;
    if (std::fabs(delta) > config_.max_abs_delta_seconds) {
        ++rejected_wild_;
        return false;
    }
    if (sample_count() >= config_.max_samples) {
        return false;
    }
    samples_.push_back(CalibrationSample{beat_seconds, hit_seconds});
    return true;
}

CalibrationResult OffsetCalibration::result() const {
    CalibrationResult out;
    out.accepted = sample_count();
    out.rejected_wild = rejected_wild_;
    if (samples_.empty()) {
        return out; // ready=false, all numeric fields zero
    }

    std::vector<double> deltas;
    deltas.reserve(samples_.size());
    for (const CalibrationSample& sample : samples_) {
        deltas.push_back(sample.hit_seconds - sample.beat_seconds);
    }
    std::sort(deltas.begin(), deltas.end());
    const double median = median_of_sorted(deltas);

    std::vector<double> deviations;
    deviations.reserve(deltas.size());
    for (const double delta : deltas) {
        deviations.push_back(std::fabs(delta - median));
    }
    std::sort(deviations.begin(), deviations.end());
    const double mad = median_of_sorted(deviations);

    const double threshold =
        std::max(config_.mad_multiplier * mad, config_.mad_floor_seconds);

    std::vector<double> inliers;
    inliers.reserve(deltas.size());
    for (const double delta : deltas) {
        if (std::fabs(delta - median) <= threshold) {
            inliers.push_back(delta);
        }
    }
    out.rejected_outlier = static_cast<int>(deltas.size() - inliers.size());

    if (inliers.empty()) {
        return out; // degenerate: non-ready, zero fields, no NaN
    }

    double sum = 0.0;
    for (const double delta : inliers) {
        sum += delta;
    }
    const double mean = sum / static_cast<double>(inliers.size());

    double variance = 0.0;
    for (const double delta : inliers) {
        const double diff = delta - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(inliers.size());

    out.mean_delta_seconds = mean;
    out.offset_seconds = -mean;
    out.spread_seconds = std::sqrt(variance);
    out.ready = ready();
    return out;
}

} // namespace td
