#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include "chart/note.hpp"
#include "chart/timing_data.hpp"

namespace blaze4k {

// True only for a case-insensitive "beginner" label. Mirrors OpenITG
// `StringToDifficulty` (src/Difficulty.cpp:22-26: `MakeLower`, `== "beginner"`),
// which `Player::IsPlayingBeginner` keys on. "novice" is NOT Beginner in OpenITG
// (it maps to DIFFICULTY_INVALID). Labels arrive already trimmed by the MSD reader.
[[nodiscard]] inline bool is_beginner_difficulty(std::string_view label) {
    constexpr std::string_view kBeginner = "beginner";
    if (label.size() != kBeginner.size()) {
        return false;
    }
    for (std::size_t i = 0; i < label.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(label[i])) != kBeginner[i]) {
            return false;
        }
    }
    return true;
}

struct Chart {
    std::string steps_type = "dance-single";
    std::string description;
    // Beginner, Easy, Medium, Hard, Challenge, Edit (passthrough label). The
    // parser always sets it; the empty default mirrors OpenITG `Steps` starting
    // at DIFFICULTY_INVALID, which is not Beginner (so hand-built charts are
    // never MercifulBeginner charts by accident).
    std::string difficulty;
    int meter = 1;                       // Foot rating

    std::vector<Note> notes;
    TimingData timing;

    int tap_count = 0;
    int hold_count = 0;
    int roll_count = 0;
    int mine_count = 0;

    // OpenITG `Player::IsPlayingBeginner` (src/Player.cpp:1724-1737).
    [[nodiscard]] bool is_beginner() const { return is_beginner_difficulty(difficulty); }

    [[nodiscard]] int total_stream_notes() const {
        return tap_count + hold_count + roll_count;
    }
};

} // namespace blaze4k
