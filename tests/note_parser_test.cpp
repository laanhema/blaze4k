#include <iostream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include "chart/simfile_parser.hpp"
#include "chart/note_parser.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[note_parser_test] Starting note data and chart parser tests...\n";

    // 1. Test 4-panel note types (Taps, Holds, Rolls, Mines)
    std::string sm_content = 
        "#TITLE:Rhythm Test;\n"
        "#ARTIST:Tundra Beats;\n"
        "#OFFSET:0.000000;\n"
        "#BPMS:0.000=120.000;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     Chart Author:\n"
        "     Challenge:\n"
        "     10:\n"
        "     0.5,0.5,0.5,0.5,0.5:\n"
        "1000\n" // Beat 0: Tap col 0
        "0200\n" // Beat 1: Hold head col 1
        "0040\n" // Beat 2: Roll head col 2
        "000M\n" // Beat 3: Mine col 3
        ",\n"
        "0300\n" // Beat 4: Hold tail col 1 (length = 3 beats)
        "0030\n" // Beat 5: Roll tail col 2 (length = 3 beats)
        "0000\n" // Beat 6: Empty
        "1001\n" // Beat 7: Jump (Taps col 0 and 3)
        ";\n";

    td::SimfileParser parser;
    TEST_CHECK(parser.parse_string(sm_content, ".sm"));
    TEST_CHECK(parser.charts().size() == 1);

    const td::Chart& chart = parser.charts()[0];
    TEST_CHECK(chart.steps_type == "dance-single");
    TEST_CHECK(chart.difficulty == "Challenge");
    TEST_CHECK(chart.meter == 10);
    TEST_CHECK(chart.tap_count == 3);  // Beat 0, Beat 7 (two taps)
    TEST_CHECK(chart.hold_count == 1); // Beat 1
    TEST_CHECK(chart.roll_count == 1); // Beat 2
    TEST_CHECK(chart.mine_count == 1); // Beat 3
    TEST_CHECK(chart.total_stream_notes() == 5);

    // Verify hold details
    bool found_hold = false;
    bool found_roll = false;
    for (const auto& note : chart.notes) {
        if (note.type == td::NoteType::HoldHead) {
            found_hold = true;
            TEST_CHECK(note.column == 1);
            TEST_CHECK(std::abs(note.beat - 1.0) < 1e-4);
            TEST_CHECK(std::abs(note.hold_length_beats - 3.0) < 1e-4);
            // 120 BPM: 1 beat = 0.5s. Hold head at 0.5s, tail at 2.0s
            TEST_CHECK(std::abs(note.time_seconds - 0.5) < 1e-4);
            TEST_CHECK(std::abs(note.hold_end_time_seconds - 2.0) < 1e-4);
        } else if (note.type == td::NoteType::RollHead) {
            found_roll = true;
            TEST_CHECK(note.column == 2);
            TEST_CHECK(std::abs(note.beat - 2.0) < 1e-4);
            TEST_CHECK(std::abs(note.hold_length_beats - 3.0) < 1e-4);
            TEST_CHECK(std::abs(note.time_seconds - 1.0) < 1e-4);
            TEST_CHECK(std::abs(note.hold_end_time_seconds - 2.5) < 1e-4);
        }
    }
    TEST_CHECK(found_hold);
    TEST_CHECK(found_roll);
    std::cout << "  - 4-panel note types (Tap, Hold, Roll, Mine) parsed with correct time/length.\n";

    // 2. Test Multiple Difficulties in one file
    std::string multi_diff_sm = 
        "#TITLE:Multi Diff;\n"
        "#BPMS:0.0=120.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Beginner:\n"
        "     2:\n"
        "     ::::\n"
        "1000\n0000\n0000\n0000\n;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Expert:\n"
        "     9:\n"
        "     ::::\n"
        "1111\n1111\n1111\n1111\n;\n";

    td::SimfileParser multi_parser;
    TEST_CHECK(multi_parser.parse_string(multi_diff_sm, ".sm"));
    TEST_CHECK(multi_parser.charts().size() == 2);
    TEST_CHECK(multi_parser.charts()[0].difficulty == "Beginner");
    TEST_CHECK(multi_parser.charts()[0].meter == 2);
    TEST_CHECK(multi_parser.charts()[1].difficulty == "Expert");
    TEST_CHECK(multi_parser.charts()[1].meter == 9);
    std::cout << "  - Multiple difficulties in one file extracted correctly.\n";

    // 3. Test Graceful Rejection of non-4-panel (dance-double) charts
    std::string double_sm = 
        "#TITLE:Doubles Only;\n"
        "#BPMS:0.0=120.0;\n"
        "#NOTES:\n"
        "     dance-double:\n"
        "     :\n"
        "     Hard:\n"
        "     8:\n"
        "     ::::\n"
        "10000000\n00000000\n00000000\n00000000\n;\n";

    td::SimfileParser double_parser;
    TEST_CHECK(double_parser.parse_string(double_sm, ".sm"));
    TEST_CHECK(double_parser.charts().empty()); // dance-double rejected gracefully
    std::cout << "  - Non-4-panel (dance-double) rejected gracefully without crash.\n";

    // 4. Test Exotic Timing Rejection
    td::TimingData negative_bpm_timing;
    negative_bpm_timing.add_bpm(0.0, -120.0);
    auto exotic_res = td::NoteParser::parse_4panel_notedata(
        "dance-single", "", "Hard", 8, "1000\n0000\n0000\n0000\n", negative_bpm_timing
    );
    TEST_CHECK(!exotic_res.has_value());
    std::cout << "  - Exotic timing chart rejected gracefully.\n";

    // 5. Test SSC Note Parsing
    std::string ssc_content = 
        "#VERSION:0.83;\n"
        "#TITLE:SSC Dance;\n"
        "#BPMS:0.0=150.0;\n"
        "#NOTEDATA:;\n"
        "#STEPSTYPE:dance-single;\n"
        "#DIFFICULTY:Hard;\n"
        "#METER:9;\n"
        "#NOTES:\n"
        "1000\n0100\n0010\n0001\n;\n";

    td::SimfileParser ssc_parser;
    TEST_CHECK(ssc_parser.parse_string(ssc_content, ".ssc"));
    TEST_CHECK(ssc_parser.charts().size() == 1);
    TEST_CHECK(ssc_parser.charts()[0].difficulty == "Hard");
    TEST_CHECK(ssc_parser.charts()[0].meter == 9);
    TEST_CHECK(ssc_parser.charts()[0].tap_count == 4);
    std::cout << "  - SSC chart block parsed successfully.\n";

    std::cout << "[note_parser_test] All note parser tests passed successfully!\n";
    return 0;
}
