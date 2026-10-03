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

    blaze4k::MsdFile msd;
    TEST_CHECK(msd.read_string(msd_sample));
    TEST_CHECK(msd.size() == 3);
    TEST_CHECK(msd.get_tag_value("TITLE") == "Aurora Borealis");
    TEST_CHECK(msd.get_tag_value("title") == "Aurora Borealis"); // Case-insensitive
    TEST_CHECK(msd.get_tag_value("ARTIST") == "Polaris");

    const blaze4k::MsdTag* complex_tag = msd.find_tag("COMPLEX");
    TEST_CHECK(complex_tag != nullptr);
    TEST_CHECK(complex_tag->params.size() == 3);
    TEST_CHECK(complex_tag->params[0] == "Param1");
    TEST_CHECK(complex_tag->params[1] == "Param2");
    TEST_CHECK(complex_tag->params[2] == "Param3");
    std::cout << "  - MSD lexer and comment stripping passed.\n";

    // 2. Test SM Parser and Metadata
    std::string sm_content = 
        "#TITLE:Blaze Groove;\n"
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

    blaze4k::SimfileParser sm_parser;
    TEST_CHECK(sm_parser.parse_string(sm_content, ".sm"));
    TEST_CHECK(!sm_parser.is_ssc());
    TEST_CHECK(sm_parser.metadata().title == "Blaze Groove");
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
    const blaze4k::TimingData& timing = sm_parser.timing();
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

    blaze4k::SimfileParser ssc_parser;
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

    blaze4k::SimfileParser malformed_parser;
    TEST_CHECK(malformed_parser.parse_string(malformed_content, ".sm"));
    TEST_CHECK(malformed_parser.metadata().title == "Corrupt Song");
    TEST_CHECK(malformed_parser.metadata().offset == 0.0); // Safe fallback
    TEST_CHECK(std::abs(malformed_parser.timing().get_bpm_at_beat(0.0) - 150.0) < 1e-6);
    std::cout << "  - Malformed and corrupt headers handled gracefully without crash.\n";

    // 6. Named Edit charts (#84): SM #NOTES description and SSC
    // #DESCRIPTION/#CHARTNAME/#VERSION precedence per SM5 NotesLoaderSSC.cpp:319-349.
    {
        const std::string rows = "0000\n0000\n0000\n0000\n";
        // SM: the 2nd #NOTES field is the description (real-data shape,
        // ITG3 Dance All Night.sm).
        const std::string sm_edit = "#TITLE:Edit Song;\n#BPMS:0.000=120.000;\n"
                                    "#NOTES:dance-single:JBEAN:Edit:10:0,0,0,0,0:\n" +
                                    rows + ";\n";
        blaze4k::SimfileParser sm_parser;
        TEST_CHECK(sm_parser.parse_string(sm_edit, ".sm"));
        TEST_CHECK(sm_parser.charts().size() == 1);
        TEST_CHECK(sm_parser.charts()[0].difficulty == "Edit");
        TEST_CHECK(sm_parser.charts()[0].description == "JBEAN");
        TEST_CHECK(sm_parser.charts()[0].meter == 10);

        const std::string ssc_header = "#TITLE:SSC Edit;\n#BPMS:0.000=120.000;\n";
        auto block = [&rows](const std::string& tags) {
            return "#NOTEDATA:;\n#STEPSTYPE:dance-single;\n" + tags +
                   "#DIFFICULTY:Edit;\n#METER:12;\n#NOTES:\n" + rows + ";\n";
        };
        auto parse_ssc = [](const std::string& content) {
            blaze4k::SimfileParser parser;
            TEST_CHECK(parser.parse_string(content, ".ssc"));
            TEST_CHECK(parser.is_ssc());
            return parser;
        };

        // Modern version: #DESCRIPTION is the description, #CHARTNAME is not.
        {
            auto p = parse_ssc("#VERSION:0.83;\n" + ssc_header +
                               block("#CHARTNAME:Chart Title;\n#DESCRIPTION:My Edit;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].difficulty == "Edit");
            TEST_CHECK(p.charts()[0].description == "My Edit");
            TEST_CHECK(p.charts()[0].meter == 12);
        }
        // Tag order reversed: the old last-wins bug would give "Chart Title".
        {
            auto p = parse_ssc("#VERSION:0.83;\n" + ssc_header +
                               block("#DESCRIPTION:My Edit;\n#CHARTNAME:Chart Title;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description == "My Edit");
        }
        // CHARTNAME only: never the description.
        {
            auto p = parse_ssc("#VERSION:0.83;\n" + ssc_header +
                               block("#CHARTNAME:Chart Title;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description.empty());
        }
        // No #VERSION at all: defaults to 0.83 (SM5 Song.h:25).
        {
            auto p = parse_ssc(ssc_header + block("#DESCRIPTION:My Edit;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description == "My Edit");
        }
        // Pre-0.74: #DESCRIPTION is the chart name, so the description stays empty.
        {
            auto p = parse_ssc("#VERSION:0.70;\n" + ssc_header +
                               block("#DESCRIPTION:Old Name;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description.empty());
        }
        // Exactly 0.74 is modern.
        {
            auto p = parse_ssc("#VERSION:0.74;\n" + ssc_header +
                               block("#DESCRIPTION:New Name;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description == "New Name");
        }
        // Unparsable / non-finite / out-of-range version -> 0.0 (strtof parity): old.
        for (const char* junk : {"garbage", "inf", "nan", "1e999", ""}) {
            auto p = parse_ssc(std::string("#VERSION:") + junk + ";\n" + ssc_header +
                               block("#DESCRIPTION:Junk Name;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description.empty());
        }
        // Float parity with SM5 strtof: "0.73999999" rounds to 0.74f (modern),
        // "1e39" overflows float to inf -> 0 (old), though both are finite doubles.
        {
            auto p = parse_ssc("#VERSION:0.73999999;\n" + ssc_header +
                               block("#DESCRIPTION:Rounded Up;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description == "Rounded Up");
        }
        {
            auto p = parse_ssc("#VERSION:1e39;\n" + ssc_header +
                               block("#DESCRIPTION:Float Overflow;\n"));
            TEST_CHECK(p.charts().size() == 1);
            TEST_CHECK(p.charts()[0].description.empty());
        }
        // A later #VERSION (steps-level) applies to the blocks after it, in file order.
        {
            auto p = parse_ssc("#VERSION:0.83;\n" + ssc_header +
                               block("#DESCRIPTION:First;\n") +
                               "#NOTEDATA:;\n#VERSION:0.70;\n#STEPSTYPE:dance-single;\n"
                               "#DESCRIPTION:Second;\n#DIFFICULTY:Edit;\n#METER:5;\n"
                               "#NOTES:\n" + rows + ";\n");
            TEST_CHECK(p.charts().size() == 2);
            TEST_CHECK(p.charts()[0].description == "First");
            TEST_CHECK(p.charts()[1].description.empty());
        }
        // Two NOTEDATA blocks: the description resets per block (no leak).
        {
            auto p = parse_ssc("#VERSION:0.83;\n" + ssc_header +
                               block("#DESCRIPTION:First;\n") + block(""));
            TEST_CHECK(p.charts().size() == 2);
            TEST_CHECK(p.charts()[0].description == "First");
            TEST_CHECK(p.charts()[1].description.empty());
        }
    }
    std::cout << "  - Named Edit charts: SM description and SSC #DESCRIPTION/#CHARTNAME/#VERSION precedence verified.\n";

    std::cout << "[parser_test] All parser and timing tests passed successfully!\n";
    return 0;
}
