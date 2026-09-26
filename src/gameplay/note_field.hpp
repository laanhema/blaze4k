#pragma once

#include <vector>
#include "chart/chart.hpp"
#include "gameplay/speed_mod.hpp"

namespace td {

enum class ScrollDirection {
    Up,
    Down,
};

// Tundra-owned placeholder presentation geometry. `pixels_per_beat` is the only
// OpenITG-derived value (ARROW_SIZE = 64 px); the rest is tunable until D2.
struct NoteFieldConfig {
    double pixels_per_beat = 64.0; // OpenITG ARROW_SIZE
    double receptor_y = 0.0;       // screen pixels from the top
    double column_width = 64.0;    // screen pixels
    double note_height = 56.0;     // screen pixels; pads culling by a half-note
    ScrollDirection direction = ScrollDirection::Up;
};

// One drawable note. Offsets are measured from the receptor: a positive offset
// means the note is still ahead of the receptor (not yet hit).
struct NoteRenderItem {
    const Note* note = nullptr;
    NoteType type = NoteType::Tap;
    int column = 0;
    double head_offset = 0.0;
    double tail_offset = 0.0; // >= head_offset for holds/rolls
    bool has_body = false;
};

// Pure, deterministic layout core for the scrolling note field.
//
// This module is time-parameterized: callers pass an absolute music time (from
// `MusicClock`) rather than letting the field read a clock, so layout stays
// testable and can never consult wall-clock/frame timing. It includes no
// platform, GL, or audio headers.
class NoteField {
public:
    void set_chart(const Chart* chart);
    void set_speed_mod(const SpeedMod& mod); // resolves M-mod once against chart timing
    void set_config(const NoteFieldConfig& config);

    [[nodiscard]] const NoteFieldConfig& config() const { return config_; }
    [[nodiscard]] double effective_x_speed() const { return resolved_x_speed_; }

    [[nodiscard]] double offset_for_note(const Note& note, double music_time_seconds) const;
    [[nodiscard]] double offset_for_beat(double beat, double music_time_seconds) const;
    [[nodiscard]] double tail_offset_for(const Note& note, double music_time_seconds) const;

    // Collects notes whose drawn extent intersects [visible_top, visible_bottom]
    // (offset space, may be given in any order).
    void compute_visible(double music_time_seconds,
                         double visible_top,
                         double visible_bottom,
                         std::vector<NoteRenderItem>& out) const;

    [[nodiscard]] double column_x(int column, double field_left) const;
    [[nodiscard]] double field_width() const { return 4.0 * config_.column_width; }
    [[nodiscard]] double screen_y(double offset) const;

private:
    double time_spacing_speed() const;

    const Chart* chart_ = nullptr;
    SpeedMod speed_mod_{};
    NoteFieldConfig config_{};
    double resolved_x_speed_ = 1.0;
};

} // namespace td
