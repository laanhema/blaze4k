#pragma once

#include <cstddef>
#include <vector>

namespace blaze4k {

// Analytic steady-metronome schedule + robust offset estimation for the C5
// calibration wizard (PRD section 7.5). Pure: standard-library <cstddef>/<vector>
// only, no SDL/GL/audio/clock/wall-clock. Testable headless (AGENTS.md principle 1).
//
// Schedule (music-clock seconds):
//   beat_time(n) = lead_in_seconds + n * beat_period_seconds
//   matching_beat_index(t) = clamp(floor((t - lead_in + max_early) / period), 0, max_beats - 1)
//   i.e. the most recent beat b with t >= b - max_early (latency only makes taps
//   late), so a tap's delta lies in [-max_early, period - max_early) (#74).
//
// Wrap-safe estimate (#74): result() first finds the tap cluster's circular
// centre and shifts each delta by whole periods to sit next to it, so jitter
// across the slot edge cannot split the cluster. A measured delay outside
// [-max_early, +max_late] is flagged `out_of_range` and is never savable.
// Invariant: 0 < max_early, 0 < max_late, max_early + max_late < beat_period.
//
// Offset sign (derived from MusicClock: `time_seconds = sample + offset`, positive
// offset = clock reads later, and a late player needs the clock to read earlier):
//   new_offset_seconds = -mean(hit_seconds - beat_seconds)   // OQ4
struct CalibrationConfig {
    double lead_in_seconds = 2.0;
    double beat_period_seconds = 0.5; // 120 BPM (OQ3)
    int max_beats = 64;
    int min_samples = 8;                 // OQ1
    int max_samples = 32;                // OQ1
    double max_early_seconds = 0.05;     // #74: supported early limit of the measured delay
    double max_late_seconds = 0.42;      // #74: supported late limit (total delay)
    double mad_multiplier = 3.0;         // OQ2
    double mad_floor_seconds = 0.05;     // OQ2

    [[nodiscard]] double beat_time(int index) const;
    [[nodiscard]] int matching_beat_index(double music_seconds) const;
};

// One accepted panel press: the matching scheduled beat and the aged hit time.
struct CalibrationSample {
    double beat_seconds = 0.0;
    double hit_seconds = 0.0;
};

struct CalibrationResult {
    bool ready = false;
    int accepted = 0;
    int rejected_wild = 0;
    int rejected_outlier = 0;
    double mean_delta_seconds = 0.0; // mean(hit - beat); positive = late
    double offset_seconds = 0.0;     // -mean_delta (Blaze 4k sign; OQ4)
    double spread_seconds = 0.0;     // stddev of inliers
    // Measured delay outside [-max_early, +max_late]; never savable (#74).
    bool out_of_range = false;
};

// Accumulates accepted samples and recomputes a robust result on demand. Never
// throws; empty/degenerate input yields a non-ready result with zero fields.
class OffsetCalibration {
public:
    explicit OffsetCalibration(CalibrationConfig config = {});

    void reset();

    // Records a sample. Returns false (and bumps rejected_wild) when the delta
    // is non-finite or outside the matching slot [-max_early, period - max_early);
    // returns false without counting when the sample cap is already reached.
    bool add_sample(double beat_seconds, double hit_seconds);

    [[nodiscard]] const CalibrationConfig& config() const { return config_; }
    [[nodiscard]] int sample_count() const { return static_cast<int>(samples_.size()); }
    // Sample gate only (enough accepted samples); a savable result is `result().ready`.
    [[nodiscard]] bool ready() const { return sample_count() >= config_.min_samples; }
    [[nodiscard]] CalibrationResult result() const; // pure, recomputed

private:
    CalibrationConfig config_;
    std::vector<CalibrationSample> samples_;
    int rejected_wild_ = 0;
};

} // namespace blaze4k
