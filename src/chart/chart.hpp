#pragma once

#include <string>
#include <vector>
#include "chart/note.hpp"
#include "chart/timing_data.hpp"

namespace td {

struct Chart {
    std::string steps_type = "dance-single";
    std::string description;
    std::string difficulty = "Beginner"; // Beginner, Easy, Medium, Hard, Challenge, Edit
    int meter = 1;                       // Foot rating

    std::vector<Note> notes;
    TimingData timing;

    int tap_count = 0;
    int hold_count = 0;
    int roll_count = 0;
    int mine_count = 0;

    [[nodiscard]] int total_stream_notes() const {
        return tap_count + hold_count + roll_count;
    }
};

} // namespace td
