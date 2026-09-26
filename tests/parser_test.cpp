#include <iostream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include "chart/msd_file.hpp"
#include "chart/timing_data.hpp"
#include "chart/simfile_parser.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[parser_test] Starting SM/SSC and timing parser tests...\n";

    // 1. Test MSD Tokenizer
    std::string msd_sample = 
        "// Header comment\n"
        "#TITLE:Aurora Borealis;\n"
        "#ARTIST:Polaris;\n"
        "#COMPLEX:Param1:Param2:Param3;\n"
        "// Trailing comment\n";

    td::MsdFile msd;
    TEST_CHECK(msd.read_string(msd_sample));
    TEST_CHECK(msd.size() == 3);
    TEST_CHECK(msd.get_tag_value("TITLE") == "Aurora Borealis");
    TEST_CHECK(msd.get_tag_value("title") == "Aurora Borealis"); // Case-insensitive
    TEST_CHECK(msd.get_tag_value("ARTIST") == "Polaris");

    const td::MsdTag* complex_tag = msd.find_tag("COMPLEX");
    TEST_CHECK(complex_tag != nullptr);
    TEST_CHECK(complex_tag->params.size() == 3);
    TEST_CHECK(complex_tag->params[0] == "Param1");
    TEST_CHECK(complex_tag->params[1] == "Param2");
    TEST_CHECK(complex_tag->params[2] == "Param3");
    std::cout << "  - MSD lexer and comment stripping passed.\n";

    // 2. Test SM Parser and Metadata
    std::string sm_content = 
        "#TITLE:Tundra Groove;\n"
        "#SUBTITLE:Frostbite Mix;\n"
        "#ARTIST:DJ Arctica;\n"
        "#GENRE:Hardcore;\n"
        "#BANNER:bn.png;\n"
        "#BACKGROUND:bg.png;\n"
        "#MUSIC:audio.ogg;\n"
        "#OFFSET:-0.030000;\n"
        "#SAMPLESTART:45.000000;\n"
        "#SAMPLELENGTH:12.000000;\n"
        "#BPMS:0.000=140.000,64.000=280.000;\n"
        "#STOPS:32.000=1.500;\n";

    td::SimfileParser sm_parser;
    TEST_CHECK(sm_parser.parse_string(sm_content, ".sm"));
    TEST_CHECK(!sm_parser.is_ssc());
    TEST_CHECK(sm_parser.metadata().title == "Tundra Groove");
    TEST_CHECK(sm_parser.metadata().subtitle == "Frostbite Mix");
    TEST_CHECK(sm_parser.metadata().artist == "DJ Arctica");
    TEST_CHECK(sm_parser.metadata().genre == "Hardcore");
    TEST_CHECK(sm_parser.metadata().banner_path == "bn.png");
    TEST_CHECK(sm_parser.metadata().background_path == "bg.png");
    TEST_CHECK(sm_parser.metadata().music_path == "audio.ogg");
    TEST_CHECK(std::abs(sm_parser.metadata().offset - (-0.03)) < 1e-6);
    TEST_CHECK(std::abs(sm_parser.metadata().sample_start - 45.0) < 1e-6);
    TEST_CHECK(std::abs(sm_parser.metadata().sample_length - 12.0) < 1e-6);
    std::cout << "  - SM header metadata extraction passed.\n";

    // 3. Test Timing Conversions (BPM changes + Stops)
    const td::TimingData& timing = sm_parser.timing();
    TEST_CHECK(timing.bpms().size() == 2);
    TEST_CHECK(timing.stops().size() == 1);

    // At beat 0: time is -(-0.03) = 0.030s
    double t0 = timing.beat_to_seconds(0.0);
    TEST_CHECK(std::abs(t0 - 0.030) < 1e-6);
    TEST_CHECK(std::abs(timing.seconds_to_beat(t0) - 0.0) < 1e-6);

    // At beat 16 (140 BPM, no stops yet): 16 / 140 * 60 + 0.030 = 6.88714s
    double t16 = timing.beat_to_seconds(16.0);
    double expected_t16 = (16.0 / 140.0) * 60.0 + 0.030;
    TEST_CHECK(std::abs(t16 - expected_t16) < 1e-4);
    TEST_CHECK(std::abs(timing.seconds_to_beat(t16) - 16.0) < 1e-4);

    // Stop at beat 32 with length 1.5s
    double stop_start_t = (32.0 / 140.0) * 60.0 + 0.030; // 13.74428s
    double stop_end_t = stop_start_t + 1.5;              // 15.24428s
    TEST_CHECK(std::abs(timing.beat_to_seconds(32.0) - stop_start_t) < 1e-4);
    TEST_CHECK(timing.is_in_stop(stop_start_t + 0.5));
    TEST_CHECK(std::abs(timing.seconds_to_beat(stop_start_t + 0.5) - 32.0) < 1e-6);
    TEST_CHECK(!timing.is_in_stop(stop_end_t + 0.01));

    // Beat 48 (after stop, still 140 BPM): 48 / 140 * 60 + 1.5 + 0.030
    double t48 = timing.beat_to_seconds(48.0);
    double expected_t48 = (48.0 / 140.0) * 60.0 + 1.5 + 0.030;
    TEST_CHECK(std::abs(t48 - expected_t48) < 1e-4);
    TEST_CHECK(std::abs(timing.seconds_to_beat(t48) - 48.0) < 1e-4);

    // Beat 80 (16 beats after 64 BPM transition to 280 BPM)
    double t64 = (64.0 / 140.0) * 60.0 + 1.5 + 0.030;
    double expected_t80 = t64 + (16.0 / 280.0) * 60.0;
    double t80 = timing.beat_to_seconds(80.0);
    TEST_CHECK(std::abs(t80 - expected_t80) < 1e-4);
    TEST_CHECK(std::abs(timing.seconds_to_beat(t80) - 80.0) < 1e-4);
    std::cout << "  - Beat-to-seconds and seconds-to-beat mathematical fidelity verified.\n";

    // 4. Test SSC Header Parsing
    std::string ssc_content = 
        "#VERSION:0.83;\n"
        "#TITLE:SSC Track;\n"
        "#ARTIST:Future Sound;\n"
        "#BPMS:0.000=175.000;\n";

    td::SimfileParser ssc_parser;
    TEST_CHECK(ssc_parser.parse_string(ssc_content, ".ssc"));
    TEST_CHECK(ssc_parser.is_ssc());
    TEST_CHECK(ssc_parser.metadata().title == "SSC Track");
    TEST_CHECK(ssc_parser.metadata().artist == "Future Sound");
    TEST_CHECK(std::abs(ssc_parser.timing().get_bpm_at_beat(0.0) - 175.0) < 1e-6);
    std::cout << "  - SSC header format and version detection verified.\n";

    // 5. Test Untrusted / Malformed Input Hardening
    std::string malformed_content = 
        "#TITLE:Corrupt Song\n" // Missing semicolon
        "#OFFSET:not_a_number;\n"
        "#BPMS:garbage=value,invalid,0.000=150.000,;\n"
        "#STOPS:broken=stop;\n";

    td::SimfileParser malformed_parser;
    TEST_CHECK(malformed_parser.parse_string(malformed_content, ".sm"));
    TEST_CHECK(malformed_parser.metadata().title == "Corrupt Song");
    TEST_CHECK(malformed_parser.metadata().offset == 0.0); // Safe fallback
    TEST_CHECK(std::abs(malformed_parser.timing().get_bpm_at_beat(0.0) - 150.0) < 1e-6);
    std::cout << "  - Malformed and corrupt headers handled gracefully without crash.\n";

    std::cout << "[parser_test] All parser and timing tests passed successfully!\n";
    return 0;
}
