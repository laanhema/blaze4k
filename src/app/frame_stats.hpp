#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace blaze4k {

// Descriptive per-frame timing samples in milliseconds. Sampling and statistics
// only: it never generates time and is never consulted by the judgment path.
//
// Pure and header-only (std only, no SDL/GL/audio/timers). The App feeds it the
// `frame_dt` it already computes for presentation; the headless benchmark feeds
// it measured update costs. Every query is a pure function of the stored samples.
class FrameStats {
public:
    // Drops every stored sample.
    void reset() { samples_ms_.clear(); }

    // Records one per-frame sample in milliseconds. Non-finite (NaN/Inf) and
    // negative values are ignored so a bad clock reading cannot poison a query.
    void add(double milliseconds) {
        if (!std::isfinite(milliseconds) || milliseconds < 0.0) {
            return;
        }
        samples_ms_.push_back(milliseconds);
    }

    [[nodiscard]] std::size_t count() const { return samples_ms_.size(); }
    [[nodiscard]] bool empty() const { return samples_ms_.empty(); }

    // Sum of every stored sample; 0 when empty.
    [[nodiscard]] double total_ms() const {
        double total = 0.0;
        for (double sample : samples_ms_) {
            total += sample;
        }
        return total;
    }

    // 0 when empty.
    [[nodiscard]] double min_ms() const {
        if (samples_ms_.empty()) {
            return 0.0;
        }
        return *std::min_element(samples_ms_.begin(), samples_ms_.end());
    }

    // 0 when empty.
    [[nodiscard]] double mean_ms() const {
        if (samples_ms_.empty()) {
            return 0.0;
        }
        return total_ms() / static_cast<double>(samples_ms_.size());
    }

    // Nearest-rank percentile; see percentile_ms(). 0 when empty.
    [[nodiscard]] double median_ms() const { return percentile_ms(50.0); }

    // 0 when empty.
    [[nodiscard]] double max_ms() const {
        if (samples_ms_.empty()) {
            return 0.0;
        }
        return *std::max_element(samples_ms_.begin(), samples_ms_.end());
    }

    // Nearest-rank percentile: `index = ceil(p/100 * n) - 1`, clamped to
    // [0, n-1]. `p` is clamped to [0, 100] first (so p<=0 is the min and p>=100
    // is the max). Sorts a copy, so the order in which samples were added never
    // changes the result. 0 when empty.
    [[nodiscard]] double percentile_ms(double p) const {
        if (samples_ms_.empty()) {
            return 0.0;
        }
        const double clamped_p = std::min(100.0, std::max(0.0, p));
        std::vector<double> sorted = samples_ms_;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t n = sorted.size();
        const double rank = std::ceil(clamped_p / 100.0 * static_cast<double>(n));
        std::size_t index = 0;
        if (rank >= 1.0) {
            index = static_cast<std::size_t>(rank) - 1;
        }
        if (index >= n) {
            index = n - 1;
        }
        return sorted[index];
    }

    // Number of samples strictly greater than `budget_ms` (a "hitch" count).
    [[nodiscard]] std::size_t over_budget(double budget_ms) const {
        std::size_t count = 0;
        for (double sample : samples_ms_) {
            if (sample > budget_ms) {
                ++count;
            }
        }
        return count;
    }

private:
    std::vector<double> samples_ms_;
};

} // namespace blaze4k
