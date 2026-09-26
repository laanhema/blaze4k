#include "chart/msd_file.hpp"
#include "chart/timing_data.hpp"
#include "chart/simfile_parser.hpp"
#include "chart/note_parser.hpp"
#include "chart/song_library.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cassert>
#include <filesystem>
#include <chrono>

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
                      << ": " << #expr << std::endl; \
            std::exit(1); \
        } \
    } while (false)

namespace {

// Deterministic PRNG for repeatable fuzz testing
std::mt19937 prng(133742);

std::string generate_random_bytes(size_t len) {
    std::uniform_int_distribution<int> dist(0, 255);
    std::string s(len, '\0');
    for (size_t i = 0; i < len; ++i) {
        s[i] = static_cast<char>(dist(prng));
    }
    return s;
}

std::string mutate_string(const std::string& base) {
    std::string mutated = base;
    std::uniform_int_distribution<int> op_dist(0, 3);
    std::uniform_int_distribution<int> pos_dist(0, static_cast<int>(mutated.empty() ? 0 : mutated.size() - 1));
    std::uniform_int_distribution<int> byte_dist(0, 255);

    int num_mutations = (prng() % 5) + 1;
    for (int m = 0; m < num_mutations; ++m) {
        if (mutated.empty()) {
            mutated.push_back(static_cast<char>(byte_dist(prng)));
            continue;
        }
        int op = op_dist(prng);
        size_t pos = static_cast<size_t>(pos_dist(prng));
        if (pos >= mutated.size()) pos = mutated.size() - 1;

        switch (op) {
            case 0: // Replace byte
                mutated[pos] = static_cast<char>(byte_dist(prng));
                break;
            case 1: // Insert byte
                mutated.insert(mutated.begin() + pos, static_cast<char>(byte_dist(prng)));
                break;
            case 2: // Delete byte
                if (mutated.size() > 1) {
                    mutated.erase(mutated.begin() + pos);
                }
                break;
            case 3: // Insert delimiter
                static const char delims[] = {'#', ':', ';', '/', '\\', '\0', '\n', '\r', ',', '-'};
                mutated.insert(mutated.begin() + pos, delims[prng() % sizeof(delims)]);
                break;
        }
    }
    return mutated;
}

} // namespace

int main() {
    std::cout << "[parser_hardening_test] Starting parser hardening and fuzz tests...\n";

    // =========================================================================
    // 1. Reference Pack Verification (AC1)
    // =========================================================================
    std::cout << "--- 1. Testing Reference Pack Loading ---\n";
    std::filesystem::path ref_pack_path = "tests/fixtures/reference_pack";
    if (!std::filesystem::exists(ref_pack_path)) {
        ref_pack_path = "../tests/fixtures/reference_pack";
    }
    if (!std::filesystem::exists(ref_pack_path)) {
        ref_pack_path = "../../tests/fixtures/reference_pack";
    }
    TEST_CHECK(std::filesystem::exists(ref_pack_path));

    td::SongLibrary library;
    bool scan_res = library.scan_directory(ref_pack_path);
    TEST_CHECK(scan_res);

    TEST_CHECK(library.packs().size() == 1);
    const auto& pack = library.packs()[0];
    TEST_CHECK(pack.name == "Tundra Pack");
    TEST_CHECK(pack.songs.size() == 4);

    // Verify Song 1: Tundra Anthem
    const td::Song* anthem = library.find_song("Tundra Pack", "Tundra Anthem");
    TEST_CHECK(anthem != nullptr);
    TEST_CHECK(anthem->charts.size() == 5);
    TEST_CHECK(anthem->has_custom_banner);
    TEST_CHECK(anthem->has_custom_background);
    TEST_CHECK(anthem->has_custom_music);
    TEST_CHECK(anthem->timing.bpms().size() == 3);
    TEST_CHECK(anthem->timing.stops().size() == 2);

    // Verify Song 2: Northern Lights (SSC)
    const td::Song* lights = library.find_song("Tundra Pack", "Northern Lights");
    TEST_CHECK(lights != nullptr);
    TEST_CHECK(lights->charts.size() == 2);
    TEST_CHECK(lights->has_custom_banner);
    TEST_CHECK(lights->has_custom_background);
    TEST_CHECK(lights->has_custom_music);

    // Verify Song 3: Glacier Groove (Escaped chars & edge cases)
    const td::Song* glacier = library.find_song("Tundra Pack", "Glacier:Groove;Part 1");
    TEST_CHECK(glacier != nullptr);
    TEST_CHECK(glacier->charts.size() == 1);
    TEST_CHECK(glacier->timing.offset() == 0.010);

    // Verify Song 4: Aurora Borealis (.ssc priority over .sm)
    const td::Song* aurora = library.find_song("Tundra Pack", "Aurora Borealis SSC");
    TEST_CHECK(aurora != nullptr);
    TEST_CHECK(aurora->charts.size() == 1);
    TEST_CHECK(aurora->metadata.artist == "Tundra SSC");

    std::cout << "  - Reference pack parsed 100% of 4-panel songs with zero crashes.\n";

    // =========================================================================
    // 2. Allocation Caps and Bounds Checks (AC2)
    // =========================================================================
    std::cout << "--- 2. Testing Allocation Caps & Hardening Bounds ---\n";

    // 2a. 17 MB content (exceeds 16MB limit)
    std::string huge_content(17 * 1024 * 1024, 'A');
    td::MsdFile huge_msd;
    bool huge_res = huge_msd.read_string(huge_content);
    TEST_CHECK(!huge_res); // Must be rejected
    TEST_CHECK(huge_msd.empty());
    std::cout << "  - 17MB file rejected cleanly by 16MB allocation cap.\n";

    // 2b. Runaway tag parameters (> 1,000 colons in one tag)
    std::string runaway_tag = "#TAG";
    for (int i = 0; i < 3000; ++i) {
        runaway_tag += ":val" + std::to_string(i);
    }
    runaway_tag += ";";
    td::MsdFile params_msd;
    TEST_CHECK(params_msd.read_string(runaway_tag));
    TEST_CHECK(params_msd.size() == 1);
    TEST_CHECK(params_msd.tags()[0].params.size() <= td::MsdFile::kMaxParamsPerTag);
    std::cout << "  - Parameter count capped at kMaxParamsPerTag (1000).\n";

    // 2c. Runaway measures (> 10,000 measures)
    std::string runaway_measures;
    for (int m = 0; m < 12000; ++m) {
        runaway_measures += "1000\n0100\n0010\n0001\n,\n";
    }
    runaway_measures += ";";
    td::TimingData timing;
    auto runaway_chart = td::NoteParser::parse_4panel_notedata(
        "dance-single", "", "Challenge", 10, runaway_measures, timing
    );
    TEST_CHECK(!runaway_chart.has_value()); // Rejected due to excessive measures
    std::cout << "  - Excessive measures (>10,000) rejected gracefully.\n";

    // 2d. Runaway rows in single measure (> 1,024 rows)
    std::string runaway_rows;
    for (int r = 0; r < 2000; ++r) {
        runaway_rows += "1000\n";
    }
    runaway_rows += ";";
    auto runaway_row_chart = td::NoteParser::parse_4panel_notedata(
        "dance-single", "", "Challenge", 10, runaway_rows, timing
    );
    TEST_CHECK(!runaway_row_chart.has_value());
    std::cout << "  - Excessive rows in measure (>1,024) rejected gracefully.\n";

    // 2e. Non-finite / NaN / Inf timing values
    td::TimingData nan_timing;
    TEST_CHECK(nan_timing.parse_bpms_string("0.0=NaN,4.0=Infinity,8.0=-120"));
    TEST_CHECK(nan_timing.has_exotic_timing());
    auto nan_chart = td::NoteParser::parse_4panel_notedata(
        "dance-single", "", "Hard", 8, "1000\n0100\n0010\n0001\n;\n", nan_timing
    );
    TEST_CHECK(!nan_chart.has_value());
    std::cout << "  - Non-finite BPMs and exotic timing detected and rejected.\n";

    // =========================================================================
    // 3. StepMania Edge Cases (AC3)
    // =========================================================================
    std::cout << "--- 3. Testing StepMania Source Edge Cases ---\n";

    // 3a. Inline comments inside tags
    std::string comment_tag = "#TITLE:StepMania // comment here\n5 Source;";
    td::MsdFile comment_msd;
    TEST_CHECK(comment_msd.read_string(comment_tag));
    TEST_CHECK(comment_msd.get_tag_value("TITLE") == "StepMania 5 Source");

    // 3b. Escapes: colon, semicolon, and backslash
    std::string escape_tag = "#TAG:Escaped\\:Colon:Escaped\\;Semicolon:Double\\\\Backslash;";
    td::MsdFile escape_msd;
    TEST_CHECK(escape_msd.read_string(escape_tag));
    TEST_CHECK(escape_msd.tags()[0].params[0] == "Escaped:Colon");
    TEST_CHECK(escape_msd.tags()[0].params[1] == "Escaped;Semicolon");
    TEST_CHECK(escape_msd.tags()[0].params[2] == "Double\\Backslash");

    // 3c. Unclosed tag at EOF without semicolon
    std::string unclosed_eof = "#TITLE:End Of File Tag";
    td::MsdFile eof_msd;
    TEST_CHECK(eof_msd.read_string(unclosed_eof));
    TEST_CHECK(eof_msd.get_tag_value("TITLE") == "End Of File Tag");

    // 3d. Unclosed hold head at song end (should auto-close gracefully)
    std::string unclosed_hold = 
        "#TITLE:Unclosed Hold;\n"
        "#BPMS:0.0=120.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Hard:\n"
        "     7:\n"
        "     ::::\n"
        "2000\n0000\n0000\n0000\n;\n";
    td::SimfileParser unclosed_parser;
    TEST_CHECK(unclosed_parser.parse_string(unclosed_hold, ".sm"));
    TEST_CHECK(unclosed_parser.charts().size() == 1);
    const auto& unclosed_chart = unclosed_parser.charts()[0];
    TEST_CHECK(unclosed_chart.notes.size() == 1);
    TEST_CHECK(unclosed_chart.notes[0].hold_length_beats > 0.0);
    std::cout << "  - StepMania edge cases matched reference behavior.\n";

    // =========================================================================
    // 4. Automated Fuzzing against Malformed Files (AC2)
    // =========================================================================
    std::cout << "--- 4. Running Fuzz Mutations (Binary Garbage & Truncations) ---\n";

    std::string base_sm = 
        "#TITLE:Fuzz Song;\n"
        "#ARTIST:Fuzz Artist;\n"
        "#OFFSET:0.000;\n"
        "#BPMS:0.0=120.0,16.0=150.0;\n"
        "#STOPS:8.0=0.5;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Hard:\n"
        "     8:\n"
        "     ::::\n"
        "1000\n0100\n0010\n0001\n,\n"
        "2000\n0000\n3000\n0000\n,\n"
        "4000\n0000\n3000\n0000\n,\n"
        "000M\n0000\n0000\n0000\n;\n";

    // 4a. Truncation fuzzing: truncate at every possible byte offset
    for (size_t len = 0; len < base_sm.size(); ++len) {
        std::string truncated = base_sm.substr(0, len);
        td::SimfileParser parser;
        // Must not crash or throw unhandled exceptions
        parser.parse_string(truncated, ".sm");
    }
    std::cout << "  - Truncation fuzzing (1 to " << base_sm.size() << " bytes) passed.\n";

    // 4b. Pure binary garbage fuzzing (100 random binary byte sequences)
    for (int iter = 0; iter < 100; ++iter) {
        size_t len = (prng() % 2048) + 1;
        std::string garbage = generate_random_bytes(len);
        td::SimfileParser parser;
        parser.parse_string(garbage, (iter % 2 == 0) ? ".sm" : ".ssc");
    }
    std::cout << "  - Pure binary garbage fuzzing (100 iterations) passed.\n";

    // 4c. Genetic mutation fuzzing (1,000 mutated simfiles)
    int successful_parses = 0;
    for (int iter = 0; iter < 1000; ++iter) {
        std::string mutated = mutate_string(base_sm);
        td::SimfileParser parser;
        if (parser.parse_string(mutated, (iter % 2 == 0) ? ".sm" : ".ssc")) {
            successful_parses++;
        }
    }
    std::cout << "  - Genetic mutation fuzzing (1,000 iterations) passed. ("
              << successful_parses << " mutations produced valid syntax).\n";

    std::cout << "[parser_hardening_test] All parser hardening and fuzz tests passed successfully!\n";
    return 0;
}
