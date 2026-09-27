#include "chart/song_library.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
                      << ": " << #expr << std::endl; \
            std::exit(1); \
        } \
    } while (false)

namespace {

void write_file(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

} // namespace

int main() {
    std::cout << "[song_library_test] Starting song library scanner tests...\n";

    std::filesystem::path test_root = std::filesystem::temp_directory_path() / "tundra_test_songs";
    std::filesystem::remove_all(test_root);
    std::filesystem::create_directories(test_root);

    // Build Pack 1
    std::filesystem::path pack1_dir = test_root / "Tundra Pack 1";
    write_file(pack1_dir / "banner.png", "fake_pack_banner");

    // Song 1: Standard SM with exact art
    std::filesystem::path song1_dir = pack1_dir / "Song One";
    write_file(song1_dir / "banner.png", "fake_banner_1");
    write_file(song1_dir / "bg.png", "fake_bg_1");
    write_file(song1_dir / "audio.ogg", "fake_audio_1");
    write_file(song1_dir / "Song One.sm",
        "#TITLE:Song One;\n"
        "#ARTIST:Tundra Artist;\n"
        "#BANNER:banner.png;\n"
        "#BACKGROUND:bg.png;\n"
        "#MUSIC:audio.ogg;\n"
        "#BPMS:0.0=130.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Medium:\n"
        "     5:\n"
        "     ::::\n"
        "1000\n0100\n0010\n0001\n;\n"
    );

    // Song 2: SSC with case-mismatched art (simfile says BANNER.PNG, file on disk is banner.png)
    std::filesystem::path song2_dir = pack1_dir / "Song Two SSC";
    write_file(song2_dir / "banner.png", "fake_banner_2");
    write_file(song2_dir / "Song Two.ssc",
        "#VERSION:0.83;\n"
        "#TITLE:Song Two SSC;\n"
        "#ARTIST:Artist Two;\n"
        "#BANNER:BANNER.PNG;\n"
        "#BPMS:0.0=140.0;\n"
        "#NOTEDATA:;\n"
        "#STEPSTYPE:dance-single;\n"
        "#DIFFICULTY:Hard;\n"
        "#METER:8;\n"
        "#NOTES:\n"
        "1001\n0000\n0000\n0000\n;\n"
    );

    // Song 3: No art in simfile tags, but folder has fallback bn/bg files
    std::filesystem::path song3_dir = pack1_dir / "Song Three Fallback";
    write_file(song3_dir / "track_bn.png", "fallback_bn_3");
    write_file(song3_dir / "track_bg.jpg", "fallback_bg_3");
    write_file(song3_dir / "track.mp3", "fallback_audio_3");
    write_file(song3_dir / "Song Three.sm",
        "#TITLE:Song Three Fallback;\n"
        "#ARTIST:Artist Three;\n"
        "#BANNER:;\n"
        "#BACKGROUND:;\n"
        "#BPMS:0.0=150.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Challenge:\n"
        "     10:\n"
        "     ::::\n"
        "1100\n0011\n1100\n0011\n;\n"
    );

    // Song 4: Unsupported 8-panel (dance-double) only -> Should be skipped
    std::filesystem::path song4_dir = pack1_dir / "Double Only Song";
    write_file(song4_dir / "Double.sm",
        "#TITLE:Double Only;\n"
        "#ARTIST:Double Artist;\n"
        "#BPMS:0.0=120.0;\n"
        "#NOTES:\n"
        "     dance-double:\n"
        "     :\n"
        "     Hard:\n"
        "     8:\n"
        "     ::::\n"
        "10000000\n00000000\n00000000\n00000000\n;\n"
    );

    // Song 5: Corrupt simfile -> Should be skipped gracefully with log
    std::filesystem::path song5_dir = pack1_dir / "Corrupt Song";
    write_file(song5_dir / "Corrupt.sm", "INVALID MSD NOT CLOSED #TITLE:Broken");

    // Build Pack 2
    std::filesystem::path pack2_dir = test_root / "Tundra Pack 2";
    std::filesystem::path song6_dir = pack2_dir / "Song Four";
    write_file(song6_dir / "Song Four.sm",
        "#TITLE:Song Four;\n"
        "#ARTIST:Artist Four;\n"
        "#BPMS:0.0=160.0;\n"
        "#NOTES:\n"
        "     dance-single:\n"
        "     :\n"
        "     Basic:\n"
        "     3:\n"
        "     ::::\n"
        "1000\n0000\n0000\n0000\n;\n"
    );

    // Configure fallback paths
    std::filesystem::path fallback_banner = test_root / "fallback_banner.png";
    std::filesystem::path fallback_bg = test_root / "fallback_bg.png";
    write_file(fallback_banner, "global_fallback_banner");
    write_file(fallback_bg, "global_fallback_bg");

    td::SongLibrary library;
    library.set_fallback_banner(fallback_banner);
    library.set_fallback_background(fallback_bg);

    // 1. Scan directory
    bool scan_ok = library.scan_directory(test_root);
    TEST_CHECK(scan_ok);

    std::cout << "  - Scan completed. Packs found: " << library.packs().size()
              << ", Total songs: " << library.total_songs()
              << ", Total charts: " << library.total_charts() << "\n";

    // 2. Validate pack grouping
    TEST_CHECK(library.packs().size() == 2);
    TEST_CHECK(library.total_songs() == 4); // 3 from Pack 1 (Song 1, 2, 3), 1 from Pack 2 (Song 4)
    TEST_CHECK(library.total_charts() == 4);

    const td::SongPack* p1 = nullptr;
    const td::SongPack* p2 = nullptr;
    for (const auto& p : library.packs()) {
        if (p.name == "Tundra Pack 1") p1 = &p;
        if (p.name == "Tundra Pack 2") p2 = &p;
    }
    TEST_CHECK(p1 != nullptr);
    TEST_CHECK(p2 != nullptr);
    TEST_CHECK(p1->songs.size() == 3);
    TEST_CHECK(p2->songs.size() == 1);
    TEST_CHECK(!p1->banner_path.empty());

    // 3. Validate Song 1 (exact art match)
    const td::Song* s1 = library.find_song("Tundra Pack 1", "Song One");
    TEST_CHECK(s1 != nullptr);
    TEST_CHECK(s1->has_custom_banner);
    TEST_CHECK(s1->has_custom_background);
    TEST_CHECK(s1->has_custom_music);
    TEST_CHECK(std::filesystem::exists(s1->resolved_banner_path));
    TEST_CHECK(std::filesystem::exists(s1->resolved_background_path));
    TEST_CHECK(std::filesystem::exists(s1->resolved_music_path));
    std::cout << "  - Exact art resolution verified.\n";

    // 4. Validate Song 2 (case-insensitive banner match)
    const td::Song* s2 = library.find_song("Tundra Pack 1", "Song Two SSC");
    TEST_CHECK(s2 != nullptr);
    TEST_CHECK(s2->has_custom_banner);
    TEST_CHECK(std::filesystem::exists(s2->resolved_banner_path));
    std::cout << "  - Case-insensitive art resolution verified.\n";

    // 5. Validate Song 3 (fallback art keywords matched)
    const td::Song* s3 = library.find_song("Tundra Pack 1", "Song Three Fallback");
    TEST_CHECK(s3 != nullptr);
    TEST_CHECK(s3->has_custom_banner);
    TEST_CHECK(s3->has_custom_background);
    TEST_CHECK(s3->has_custom_music);
    TEST_CHECK(std::filesystem::path(s3->resolved_banner_path).filename() == "track_bn.png");
    TEST_CHECK(std::filesystem::path(s3->resolved_background_path).filename() == "track_bg.jpg");
    std::cout << "  - Fallback keyword art resolution verified.\n";

    // 6. Validate Song 4 (global fallback art applied)
    const td::Song* s4 = library.find_song("Tundra Pack 2", "Song Four");
    TEST_CHECK(s4 != nullptr);
    TEST_CHECK(!s4->has_custom_banner);
    TEST_CHECK(!s4->has_custom_background);
    TEST_CHECK(s4->resolved_banner_path == fallback_banner.string());
    TEST_CHECK(s4->resolved_background_path == fallback_bg.string());
    TEST_CHECK(std::filesystem::exists(s4->resolved_background_path));
    std::cout << "  - Global missing-art fallback verified.\n";

    // 7. Validate non-4-panel and corrupt simfiles skipped
    TEST_CHECK(library.find_song("Tundra Pack 1", "Double Only") == nullptr);
    TEST_CHECK(library.find_song("Tundra Pack 1", "Corrupt") == nullptr);
    std::cout << "  - Graceful skipping of non-4-panel and corrupt files verified.\n";

    // Clean up fixture
    std::filesystem::remove_all(test_root);

    std::cout << "[song_library_test] All song library scanner tests passed successfully!\n";
    return 0;
}
