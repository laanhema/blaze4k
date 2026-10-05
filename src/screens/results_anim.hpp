#pragma once

namespace blaze4k {

// Pure, SDL/GL/audio/wall-clock-free presentation model for the D3 results
// reveal (PRD section 12 Phase D), drawn by the Cabinet score screen (#95).
// Mirrors `gameplay/JudgmentAnimator`: the screen advances it with the injected
// `fixed_dt` only (presentation, never the judgment path — Results has no
// judgment path at all), and every output is a pure function of the accumulated
// `elapsed_`, so the reveal stays deterministic and headless-testable.
//
// The score screen maps the curves onto its parts: the top bar's badge, title
// and artist fade in (title_alpha); the grade sprite slams onto the medallion
// from 2.4x and settles at 1x (grade_scale / grade_alpha, which also fades the
// tier label); the chrome percentage counts up (percent_progress); the stat
// panels and judgment rows fade in (stats_alpha); the NEW RECORD ribbon punches
// in and pulses (record_scale / record_alpha) over a white full-window flash
// (record_flash); the FAILED ribbon fades in (failed_alpha). At completion every
// part is at its settled value.
//
// All constants are "Blaze 4k presentation, unsourced" (no OpenITG parity
// requirement): scale/alpha/count-up only.
class ResultsAnimator {
public:
    static constexpr double kGradeDelay = 0.15;        // s after enter
    static constexpr double kGradePopSeconds = 0.85;   // grade sprite "slam" onto the medallion, 2.4x to 1.0
    static constexpr double kPercentDelay = 0.45;
    static constexpr double kPercentCountSeconds = 0.80;
    static constexpr double kStatsDelay = 0.70;
    static constexpr double kRecordDelay = 1.15;       // after grade/stats have landed
    static constexpr double kRecordSeconds = 1.20;     // pulse window
    static constexpr double kRecordPulsePeriod = 0.40;
    static constexpr double kRevealSeconds = 2.40;     // >= the end of every element

    // Fade lengths and curve shapes. Same "Blaze 4k presentation, unsourced"
    // contract as the delays above; named so the reveal timeline is tuned in one
    // place instead of via inline literals.
    static constexpr double kTitleFadeSeconds = 0.25;
    static constexpr double kGradeFadeSeconds = 0.25;
    static constexpr double kStatsFadeSeconds = 0.35;
    static constexpr double kFailedFadeSeconds = 0.40;
    static constexpr double kRecordFadeSeconds = 0.25;   // alpha ramp into the pulse
    static constexpr double kRecordFlashSeconds = 0.35;  // white flash decay
    static constexpr double kGradeScaleStart = 2.4;      // slam start
    static constexpr double kGradeScaleDrop = 1.6;       // slam travel
    static constexpr double kGradeSlamPortion = 0.7;     // descending share of the pop
    static constexpr double kGradeSettleScale = 0.8;     // undershoot before settling
    static constexpr double kGradeSettleRise = 0.2;      // climb back to 1.0
    static constexpr double kRecordPulseBase = 0.75;     // pulse = base +/- amplitude
    static constexpr double kRecordPulseAmplitude = 0.25;
    static constexpr double kRecordPunchSeconds = 0.5;
    static constexpr double kRecordScaleStart = 0.6;
    static constexpr double kRecordScaleRise = 0.55;
    static constexpr double kRecordPunchPeakPortion = 0.35;
    static constexpr double kRecordScalePeak = 1.15;
    static constexpr double kRecordScaleFall = 0.15;

    void reset(bool valid, bool new_record, bool failed);

    // Advances by a non-negative `fixed_dt`, clamped to `kRevealSeconds`.
    void update(double fixed_dt);

    // Jumps to the final frame (used by the input skip).
    void skip();

    [[nodiscard]] bool finished() const { return elapsed_ >= kRevealSeconds; }
    // Test-only accessors: they mirror the reset() inputs and are not consulted by
    // ResultsScreen (which reads its own ResultsSummary). Kept so the pure animator
    // can be exercised standalone.
    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] bool new_record() const { return new_record_; }
    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] double elapsed() const { return elapsed_; }

    // Pure curves (unit-tested). Never NaN; 0 before an element's delay and the
    // settled value at/after its end. `reveal_alpha` guards `duration <= 0` and
    // `elapsed <= delay` (returns 0 then 1) so a degenerate call cannot divide by
    // zero.
    [[nodiscard]] static float title_alpha(double elapsed);
    [[nodiscard]] static float grade_scale(double elapsed);   // grade sprite: 2.4x -> ~0.8x -> 1.0x
    [[nodiscard]] static float grade_alpha(double elapsed);
    [[nodiscard]] static double percent_progress(double elapsed); // smoothstep 0..1 count-up
    [[nodiscard]] static float stats_alpha(double elapsed);
    [[nodiscard]] static float record_alpha(double elapsed);  // NEW RECORD ribbon: 0, then pulse 0.75..1, then 1
    [[nodiscard]] static float record_scale(double elapsed);  // NEW RECORD ribbon punch 0.6 -> 1.15 -> 1.0
    [[nodiscard]] static float record_flash(double elapsed);  // white full-window flash fade
    [[nodiscard]] static float failed_alpha(double elapsed);
    [[nodiscard]] static float reveal_alpha(double elapsed, double delay, double duration);

private:
    bool valid_ = false;
    bool new_record_ = false;
    bool failed_ = false;
    double elapsed_ = 0.0;
};

} // namespace blaze4k
