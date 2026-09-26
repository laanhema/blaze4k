#pragma once

#include <string_view>
#include <optional>
#include "chart/chart.hpp"
#include "chart/timing_data.hpp"

namespace td {

class NoteParser {
public:
    static std::optional<Chart> parse_4panel_notedata(
        std::string_view steps_type,
        std::string_view description,
        std::string_view difficulty,
        int meter,
        std::string_view note_data,
        const TimingData& timing
    );
};

} // namespace td
