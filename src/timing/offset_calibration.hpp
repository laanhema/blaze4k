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
//   nearest_beat_index(t) = clamp(round((t - lead_in) / period), 0, max_beats - 1)
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
    double max_abs_delta_seconds = 0.25; // OQ1 wild-tap cap
    double mad_multiplier = 3.0;         // OQ2
    double mad_floor_seconds = 0.05;     // OQ2

    [[nodiscard]] double beat_time(int index) const;
    [[nodiscard]] int nearest_beat_index(double music_seconds) const;
};

// One accepted panel press: the nearest scheduled beat and the aged hit time.
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
};

// Accumulates accepted samples and recomputes a robust result on demand. Never
// throws; empty/degenerate input yields a non-ready result with zero fields.
class OffsetCalibration {
public:
    explicit OffsetCalibration(CalibrationConfig config = {});

    void reset();

    // Records a sample. Returns false (and bumps rejected_wild) when the tap is
    // farther than `max_abs_delta_seconds` from its beat, or when the sample cap
    // is already reached.
    bool add_sample(double beat_seconds, double hit_seconds);

    [[nodiscard]] const CalibrationConfig& config() const { return config_; }
    [[nodiscard]] int sample_count() const { return static_cast<int>(samples_.size()); }
    [[nodiscard]] bool ready() const { return sample_count() >= config_.min_samples; }
    [[nodiscard]] CalibrationResult result() const; // pure, recomputed

private:
    CalibrationConfig config_;
    std::vector<CalibrationSample> samples_;
    int rejected_wild_ = 0;
};

} // namespace blaze4k
