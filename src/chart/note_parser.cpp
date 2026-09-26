#include "chart/note_parser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>

namespace td {

namespace {

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Strip // comments from a block of text
std::string strip_comments(std::string_view text) {
    std::string clean;
    clean.reserve(text.size());

    size_t i = 0;
    const size_t len = text.size();

    while (i < len) {
        if (text[i] == '/' && i + 1 < len && text[i + 1] == '/') {
            i += 2;
            while (i < len && text[i] != '\n' && text[i] != '\r') {
                i++;
            }
            continue;
        }
        clean += text[i];
        i++;
    }
    return clean;
}

} // namespace

std::optional<Chart> NoteParser::parse_4panel_notedata(
    std::string_view steps_type,
    std::string_view description,
    std::string_view difficulty,
    int meter,
    std::string_view note_data,
    const TimingData& timing
) {
    // 1. Enforce 4-panel dance-single only
    if (!iequals(steps_type, "dance-single")) {
        std::cout << "[NoteParser] Skipping unsupported steps type '" << steps_type
                  << "' (only 'dance-single' is supported in v1).\n";
        return std::nullopt;
    }

    // 2. Reject exotic timing (negative BPMs, warps)
    if (timing.has_exotic_timing()) {
        std::cout << "[NoteParser] Rejecting chart with exotic timing (non-positive BPM or warp detected).\n";
        return std::nullopt;
    }
    for (const auto& bpm : timing.bpms()) {
        if (bpm.bpm <= 0.0) {
            std::cout << "[NoteParser] Rejecting chart with exotic timing (non-positive BPM: "
                      << bpm.bpm << ").\n";
            return std::nullopt;
        }
    }
    for (const auto& stop : timing.stops()) {
        if (stop.length_seconds < 0.0) {
            std::cout << "[NoteParser] Rejecting chart with exotic timing (warp/negative stop: "
                      << stop.length_seconds << "s).\n";
            return std::nullopt;
        }
    }

    Chart chart;
    chart.steps_type = std::string(steps_type);
    chart.description = std::string(description);
    chart.difficulty = std::string(difficulty);
    chart.meter = meter;
    chart.timing = timing;

    // Clean comments from note data
    std::string cleaned = strip_comments(note_data);

    // Split measures by ',' or ';'
    std::vector<std::string> measures;
    std::string current_measure;

    for (char c : cleaned) {
        if (c == ',' || c == ';') {
            measures.push_back(std::move(current_measure));
            current_measure.clear();
        } else {
            current_measure += c;
        }
    }
    if (!current_measure.empty()) {
        measures.push_back(std::move(current_measure));
    }

    // Per-column active hold/roll heads
    std::array<std::optional<Note>, 4> active_heads;

    double last_parsed_beat = 0.0;

    for (size_t m_idx = 0; m_idx < measures.size(); ++m_idx) {
        // Parse rows in this measure. Each row is exactly 4 panel characters.
        std::vector<std::string> rows;
        std::string current_row;

        for (char c : measures[m_idx]) {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                continue;
            }
            current_row += c;
            if (current_row.size() == 4) {
                rows.push_back(std::move(current_row));
                current_row.clear();
            }
        }

        const size_t num_rows = rows.size();
        if (num_rows == 0) {
            continue;
        }

        const double measure_start_beat = static_cast<double>(m_idx) * 4.0;

        for (size_t r_idx = 0; r_idx < num_rows; ++r_idx) {
            const double row_fraction = static_cast<double>(r_idx) / static_cast<double>(num_rows);
            const double beat = measure_start_beat + row_fraction * 4.0;
            const double time_sec = timing.beat_to_seconds(beat);
            last_parsed_beat = beat;

            const std::string& row_str = rows[r_idx];

            for (int col = 0; col < 4; ++col) {
                char ch = row_str[col];

                switch (ch) {
                    case '1': { // Tap
                        Note note;
                        note.column = col;
                        note.beat = beat;
                        note.time_seconds = time_sec;
                        note.type = NoteType::Tap;
                        chart.notes.push_back(note);
                        chart.tap_count++;
                        break;
                    }
                    case '2': { // Hold Head
                        Note head;
                        head.column = col;
                        head.beat = beat;
                        head.time_seconds = time_sec;
                        head.type = NoteType::HoldHead;
                        active_heads[col] = head;
                        chart.hold_count++;
                        break;
                    }
                    case '4': { // Roll Head
                        Note head;
                        head.column = col;
                        head.beat = beat;
                        head.time_seconds = time_sec;
                        head.type = NoteType::RollHead;
                        active_heads[col] = head;
                        chart.roll_count++;
                        break;
                    }
                    case '3': { // Tail for Hold or Roll
                        if (active_heads[col].has_value()) {
                            Note head = active_heads[col].value();
                            head.hold_length_beats = beat - head.beat;
                            head.hold_end_time_seconds = time_sec;
                            chart.notes.push_back(head);
                            active_heads[col].reset();
                        }
                        break;
                    }
                    case 'M':
                    case 'm': { // Mine
                        Note note;
                        note.column = col;
                        note.beat = beat;
                        note.time_seconds = time_sec;
                        note.type = NoteType::Mine;
                        chart.notes.push_back(note);
                        chart.mine_count++;
                        break;
                    }
                    default:
                        // '0' or unrecognized keysound/lift - ignore
                        break;
                }
            }
        }
    }

    // Close any unclosed holds/rolls gracefully
    for (int col = 0; col < 4; ++col) {
        if (active_heads[col].has_value()) {
            Note head = active_heads[col].value();
            head.hold_length_beats = last_parsed_beat - head.beat;
            head.hold_end_time_seconds = timing.beat_to_seconds(last_parsed_beat);
            chart.notes.push_back(head);
            active_heads[col].reset();
        }
    }

    // Sort notes chronologically by beat, then column
    std::sort(chart.notes.begin(), chart.notes.end(), [](const Note& a, const Note& b) {
        if (a.beat != b.beat) return a.beat < b.beat;
        return a.column < b.column;
    });

    return chart;
}

} // namespace td
