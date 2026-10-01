#pragma once

#include <cstdint>
#include <string_view>

namespace blaze4k {

enum class NoteType {
    Tap,
    HoldHead,
    RollHead,
    Mine
};

// Beat-subdivision buckets, mirroring OpenITG `NoteTypes.h`. Notes are colored
// by their quantization (the ITG "denominator" note colors) so timing reads at a
// glance; the arrow *shape* carries the column direction instead.
enum class NoteQuantization {
    Fourth = 0,      // quarter note
    Eighth,          // eighth note
    Twelfth,         // quarter-note triplet
    Sixteenth,       // sixteenth note
    TwentyFourth,    // eighth-note triplet
    ThirtySecond,    // thirty-second note
    FortyEighth,     // sixteenth-note triplet
    SixtyFourth,     // sixty-fourth note
    OneNinetySecond, // anything finer (192nd)
};

// OpenITG `GetNoteType(row)` (ROWS_PER_MEASURE = 48 rows/beat * 4 beats). Only
// the row within the measure matters: every divisor used (48, 24, 16, 12, 8, 6,
// 4, 3) also divides 192, so a measure boundary never changes the bucket.
[[nodiscard]] constexpr NoteQuantization quantization_for_row(int row) {
    if (row % (192 / 4) == 0) return NoteQuantization::Fourth;
    if (row % (192 / 8) == 0) return NoteQuantization::Eighth;
    if (row % (192 / 12) == 0) return NoteQuantization::Twelfth;
    if (row % (192 / 16) == 0) return NoteQuantization::Sixteenth;
    if (row % (192 / 24) == 0) return NoteQuantization::TwentyFourth;
    if (row % (192 / 32) == 0) return NoteQuantization::ThirtySecond;
    if (row % (192 / 48) == 0) return NoteQuantization::FortyEighth;
    if (row % (192 / 64) == 0) return NoteQuantization::SixtyFourth;
    return NoteQuantization::OneNinetySecond;
}

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
    // Beat-subdivision bucket (ITG note color); kept last so existing positional
    // aggregate initializers stay valid.
    NoteQuantization quantization = NoteQuantization::Fourth;

    [[nodiscard]] bool is_hold_or_roll() const {
        return type == NoteType::HoldHead || type == NoteType::RollHead;
    }
};

} // namespace blaze4k
