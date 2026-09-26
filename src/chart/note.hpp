#pragma once

#include <cstdint>
#include <string_view>

namespace td {

enum class NoteType {
    Tap,
    HoldHead,
    RollHead,
    Mine
};

constexpr std::string_view note_type_to_string(NoteType type) {
    switch (type) {
        case NoteType::Tap: return "Tap";
        case NoteType::HoldHead: return "HoldHead";
        case NoteType::RollHead: return "RollHead";
        case NoteType::Mine: return "Mine";
    }
    return "Unknown";
}

struct Note {
    int column = 0; // 0=Left, 1=Down, 2=Up, 3=Right
    double beat = 0.0;
    double time_seconds = 0.0;
    NoteType type = NoteType::Tap;
    double hold_length_beats = 0.0;
    double hold_end_time_seconds = 0.0;

    [[nodiscard]] bool is_hold_or_roll() const {
        return type == NoteType::HoldHead || type == NoteType::RollHead;
    }
};

} // namespace td
