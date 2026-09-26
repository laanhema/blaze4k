#include "gameplay/note_field.hpp"

#include <algorithm>

namespace td {

void NoteField::set_chart(const Chart* chart) {
    chart_ = chart;
    resolved_x_speed_ = chart_ ? resolve_x_speed(speed_mod_, chart_->timing) : 1.0;
}

void NoteField::set_speed_mod(const SpeedMod& mod) {
    speed_mod_ = mod;
    resolved_x_speed_ = chart_ ? resolve_x_speed(speed_mod_, chart_->timing) : 1.0;
}

void NoteField::set_config(const NoteFieldConfig& config) {
    config_ = config;
}

double NoteField::time_spacing_speed() const {
    // C-mod: (c_bpm / 60) beats per second, i.e. beat-spacing equivalent.
    return speed_mod_.value / 60.0;
}

double NoteField::offset_for_note(const Note& note, double music_time_seconds) const {
    if (chart_ == nullptr) {
        return 0.0;
    }

    if (speed_mod_.type == SpeedModType::CMod) {
        return (note.time_seconds - music_time_seconds) * time_spacing_speed() * config_.pixels_per_beat;
    }

    return offset_for_beat(note.beat, music_time_seconds);
}

double NoteField::offset_for_beat(double beat, double music_time_seconds) const {
    if (chart_ == nullptr) {
        return 0.0;
    }

    if (speed_mod_.type == SpeedModType::CMod) {
        const double note_seconds = chart_->timing.beat_to_seconds(beat);
        return (note_seconds - music_time_seconds) * time_spacing_speed() * config_.pixels_per_beat;
    }

    const double current_beat = chart_->timing.seconds_to_beat(music_time_seconds);
    return (beat - current_beat) * config_.pixels_per_beat * resolved_x_speed_;
}

double NoteField::tail_offset_for(const Note& note, double music_time_seconds) const {
    if (chart_ == nullptr) {
        return 0.0;
    }

    if (speed_mod_.type == SpeedModType::CMod) {
        return (note.hold_end_time_seconds - music_time_seconds) * time_spacing_speed() * config_.pixels_per_beat;
    }

    return offset_for_beat(note.beat + note.hold_length_beats, music_time_seconds);
}

void NoteField::compute_visible(double music_time_seconds,
                                double visible_top,
                                double visible_bottom,
                                std::vector<NoteRenderItem>& out) const {
    out.clear();
    if (chart_ == nullptr) {
        return;
    }

    const double margin = config_.note_height * 0.5;
    const double window_low = std::min(visible_top, visible_bottom) - margin;
    const double window_high = std::max(visible_top, visible_bottom) + margin;

    for (const Note& note : chart_->notes) {
        const double head = offset_for_note(note, music_time_seconds);

        NoteRenderItem item;
        item.note = &note;
        item.type = note.type;
        item.column = note.column;
        item.head_offset = head;

        if (note.is_hold_or_roll()) {
            item.tail_offset = tail_offset_for(note, music_time_seconds);
            item.has_body = true;

            const double body_low = std::min(head, item.tail_offset);
            const double body_high = std::max(head, item.tail_offset);
            if (body_high < window_low || body_low > window_high) {
                continue;
            }
        } else {
            item.tail_offset = head;
            if (head < window_low || head > window_high) {
                continue;
            }
        }

        out.push_back(item);
    }
}

double NoteField::column_x(int column, double field_left) const {
    return field_left + (static_cast<double>(column) + 0.5) * config_.column_width;
}

double NoteField::screen_y(double offset) const {
    if (config_.direction == ScrollDirection::Down) {
        return config_.receptor_y - offset;
    }
    return config_.receptor_y + offset;
}

} // namespace td
