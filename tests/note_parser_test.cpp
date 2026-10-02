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
        "#ARTIST:Blaze Beats;\n"
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

    blaze4k::SimfileParser parser;
    TEST_CHECK(parser.parse_string(sm_content, ".sm"));
    TEST_CHECK(parser.charts().size() == 1);

    const blaze4k::Chart& chart = parser.charts()[0];
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
        if (note.type == blaze4k::NoteType::HoldHead) {
            found_hold = true;
            TEST_CHECK(note.column == 1);
            TEST_CHECK(std::abs(note.beat - 1.0) < 1e-4);
            TEST_CHECK(std::abs(note.hold_length_beats - 3.0) < 1e-4);
            // 120 BPM: 1 beat = 0.5s. Hold head at 0.5s, tail at 2.0s
            TEST_CHECK(std::abs(note.time_seconds - 0.5) < 1e-4);
            TEST_CHECK(std::abs(note.hold_end_time_seconds - 2.0) < 1e-4);
        } else if (note.type == blaze4k::NoteType::RollHead) {
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

    blaze4k::SimfileParser multi_parser;
    TEST_CHECK(multi_parser.parse_string(multi_diff_sm, ".sm"));
    TEST_CHECK(multi_parser.charts().size() == 2);
    TEST_CHECK(multi_parser.charts()[0].difficulty == "Beginner");
    TEST_CHECK(multi_parser.charts()[0].meter == 2);
    TEST_CHECK(multi_parser.charts()[1].difficulty == "Expert");
    TEST_CHECK(multi_parser.charts()[1].meter == 9);
    // MercifulBeginner detection (#67) on parsed labels.
    TEST_CHECK(multi_parser.charts()[0].is_beginner());
    TEST_CHECK(!multi_parser.charts()[1].is_beginner());
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

    blaze4k::SimfileParser double_parser;
    TEST_CHECK(double_parser.parse_string(double_sm, ".sm"));
    TEST_CHECK(double_parser.charts().empty()); // dance-double rejected gracefully
    std::cout << "  - Non-4-panel (dance-double) rejected gracefully without crash.\n";

    // 4. Test Exotic Timing Rejection
    blaze4k::TimingData negative_bpm_timing;
    negative_bpm_timing.add_bpm(0.0, -120.0);
    auto exotic_res = blaze4k::NoteParser::parse_4panel_notedata(
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

    blaze4k::SimfileParser ssc_parser;
    TEST_CHECK(ssc_parser.parse_string(ssc_content, ".ssc"));
    TEST_CHECK(ssc_parser.charts().size() == 1);
    TEST_CHECK(ssc_parser.charts()[0].difficulty == "Hard");
    TEST_CHECK(ssc_parser.charts()[0].meter == 9);
    TEST_CHECK(ssc_parser.charts()[0].tap_count == 4);
    std::cout << "  - SSC chart block parsed successfully.\n";

    // 6. OpenITG beat-subdivision buckets (ITG note colors) from a row index.
    TEST_CHECK(blaze4k::quantization_for_row(0) == blaze4k::NoteQuantization::Fourth);
    TEST_CHECK(blaze4k::quantization_for_row(48) == blaze4k::NoteQuantization::Fourth);
    TEST_CHECK(blaze4k::quantization_for_row(24) == blaze4k::NoteQuantization::Eighth);
    TEST_CHECK(blaze4k::quantization_for_row(16) == blaze4k::NoteQuantization::Twelfth);
    TEST_CHECK(blaze4k::quantization_for_row(12) == blaze4k::NoteQuantization::Sixteenth);
    TEST_CHECK(blaze4k::quantization_for_row(8) == blaze4k::NoteQuantization::TwentyFourth);
    TEST_CHECK(blaze4k::quantization_for_row(6) == blaze4k::NoteQuantization::ThirtySecond);
    TEST_CHECK(blaze4k::quantization_for_row(4) == blaze4k::NoteQuantization::FortyEighth);
    TEST_CHECK(blaze4k::quantization_for_row(3) == blaze4k::NoteQuantization::SixtyFourth);
    TEST_CHECK(blaze4k::quantization_for_row(1) == blaze4k::NoteQuantization::OneNinetySecond);
    std::cout << "  - Quantization buckets match OpenITG GetNoteType.\n";

    // 7. Quantization is attached per note from its row within the measure.
    std::string quant_sm =
        "#TITLE:Quant;\n"
        "#BPMS:0.0=120.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Hard:\n"
        "     7:\n"
        "     ::::\n"
        "1000\n" // beat 0.0 -> 4th
        "0100\n" // beat 0.5 -> 8th
        "0010\n" // beat 1.0 -> 4th
        "0001\n" // beat 1.5 -> 8th
        "1000\n" // beat 2.0 -> 4th
        "0100\n" // beat 2.5 -> 8th
        "0010\n" // beat 3.0 -> 4th
        "0001\n" // beat 3.5 -> 8th
        ";\n";

    blaze4k::SimfileParser quant_parser;
    TEST_CHECK(quant_parser.parse_string(quant_sm, ".sm"));
    TEST_CHECK(quant_parser.charts().size() == 1);
    const blaze4k::Chart& quant_chart = quant_parser.charts()[0];
    TEST_CHECK(quant_chart.notes.size() == 8);
    TEST_CHECK(quant_chart.notes[0].quantization == blaze4k::NoteQuantization::Fourth);
    TEST_CHECK(quant_chart.notes[1].quantization == blaze4k::NoteQuantization::Eighth);
    TEST_CHECK(quant_chart.notes[2].quantization == blaze4k::NoteQuantization::Fourth);
    TEST_CHECK(quant_chart.notes[3].quantization == blaze4k::NoteQuantization::Eighth);
    std::cout << "  - Notes carry the quantization of their measure row.\n";

    // 8. Beginner detection mirrors OpenITG StringToDifficulty
    //    (Difficulty.cpp:22-44) plus Steps::TidyUpData (Steps.cpp:130-141):
    //    label, then description, then meter 1 => Beginner.
    using blaze4k::StepsDifficulty;
    TEST_CHECK(blaze4k::string_to_difficulty("beginner") == StepsDifficulty::Beginner);
    TEST_CHECK(blaze4k::string_to_difficulty("Beginner") == StepsDifficulty::Beginner);
    TEST_CHECK(blaze4k::string_to_difficulty("BEGINNER") == StepsDifficulty::Beginner);
    TEST_CHECK(blaze4k::string_to_difficulty("Basic") == StepsDifficulty::Easy);
    TEST_CHECK(blaze4k::string_to_difficulty("Trick") == StepsDifficulty::Medium);
    TEST_CHECK(blaze4k::string_to_difficulty("Maniac") == StepsDifficulty::Hard);
    TEST_CHECK(blaze4k::string_to_difficulty("Expert") == StepsDifficulty::Challenge);
    TEST_CHECK(blaze4k::string_to_difficulty("Edit") == StepsDifficulty::Edit);
    TEST_CHECK(blaze4k::string_to_difficulty("Novice") == StepsDifficulty::Invalid);
    TEST_CHECK(blaze4k::string_to_difficulty("") == StepsDifficulty::Invalid);
    TEST_CHECK(blaze4k::string_to_difficulty("beginners") == StepsDifficulty::Invalid);
    // A recognized label wins regardless of description or meter.
    TEST_CHECK(blaze4k::resolve_difficulty("Easy", "Beginner", 1) == StepsDifficulty::Easy);
    TEST_CHECK(blaze4k::resolve_difficulty("Expert", "", 1) == StepsDifficulty::Challenge);
    TEST_CHECK(blaze4k::resolve_difficulty("Beginner", "", 9) == StepsDifficulty::Beginner);
    // Unrecognized label -> description.
    TEST_CHECK(blaze4k::resolve_difficulty("Novice", "Beginner", 5) == StepsDifficulty::Beginner);
    TEST_CHECK(blaze4k::resolve_difficulty("Novice", "Hard", 1) == StepsDifficulty::Hard);
    // Both unrecognized -> meter.
    TEST_CHECK(blaze4k::resolve_difficulty("Novice", "", 1) == StepsDifficulty::Beginner);
    TEST_CHECK(blaze4k::resolve_difficulty("Novice", "", 2) == StepsDifficulty::Easy);
    TEST_CHECK(blaze4k::resolve_difficulty("", "", 0) == StepsDifficulty::Easy);
    TEST_CHECK(blaze4k::resolve_difficulty("", "", 3) == StepsDifficulty::Easy);
    TEST_CHECK(blaze4k::resolve_difficulty("", "", 6) == StepsDifficulty::Medium);
    TEST_CHECK(blaze4k::resolve_difficulty("", "", 7) == StepsDifficulty::Hard);
    TEST_CHECK(blaze4k::Chart{}.difficulty.empty());
    TEST_CHECK(blaze4k::Chart{}.meter == 0);
    TEST_CHECK(!blaze4k::Chart{}.is_beginner());
    {
        const std::string fallback_sm =
            "#TITLE:Fallback;\n"
            "#BPMS:0.0=120.0;\n"
            "#NOTES:\n     dance-single:\n     :\n     Novice:\n     1:\n     ::::\n"
            "1000\n0000\n0000\n0000\n;\n"
            "#NOTES:\n     dance-single:\n     :\n     Novice:\n     2:\n     ::::\n"
            "1000\n0000\n0000\n0000\n;\n"
            "#NOTES:\n     dance-single:\n     Beginner:\n     Mystery:\n     8:\n     ::::\n"
            "1000\n0000\n0000\n0000\n;\n";
        blaze4k::SimfileParser fallback_parser;
        TEST_CHECK(fallback_parser.parse_string(fallback_sm, ".sm"));
        TEST_CHECK(fallback_parser.charts().size() == 3);
        TEST_CHECK(fallback_parser.charts()[0].is_beginner());
        TEST_CHECK(!fallback_parser.charts()[1].is_beginner());
        TEST_CHECK(fallback_parser.charts()[2].is_beginner());
        // Displayed label stays passthrough.
        TEST_CHECK(fallback_parser.charts()[0].difficulty == "Novice");
        TEST_CHECK(fallback_parser.charts()[2].difficulty == "Mystery");
    }
    std::cout << "  - Beginner label detection matches OpenITG.\n";

    std::cout << "[note_parser_test] All note parser tests passed successfully!\n";
    return 0;
}
