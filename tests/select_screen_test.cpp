#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "chart/song_library.hpp"
#include "chart/timing_data.hpp"
#include "data/config.hpp"
#include "data/high_scores.hpp"
#include "gameplay/gameplay_options.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/select_screen.hpp"
#include "screens/screen_manager.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::GameAction;
using td::InputEvent;
using td::ScreenContext;
using td::ScreenId;

constexpr double kDt = 0.1;

void write_file(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << content;
}

std::string make_sm(const std::string& title, const std::string& sample_start,
                    const std::vector<std::pair<std::string, int>>& charts,
                    const std::string& selectable = "YES") {
    std::ostringstream out;
    out << "#TITLE:" << title << ";\n"
        << "#ARTIST:Test Artist;\n"
        << "#BANNER:banner.png;\n"
        << "#MUSIC:audio.ogg;\n"
        << "#SELECTABLE:" << selectable << ";\n"
        << "#BPMS:0.0=128.0,16.0=175.0;\n";
    if (!sample_start.empty()) {
        out << "#SAMPLESTART:" << sample_start << ";\n";
    }
    for (const auto& [difficulty, meter] : charts) {
        out << "#NOTES:\n"
            << "     dance-single:\n"
            << "     :\n"
            << "     " << difficulty << ":\n"
            << "     " << meter << ":\n"
            << "     ::::\n"
            << "1000\n0100\n0010\n0001\n;\n";
    }
    return out.str();
}

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

// Scan a synthetic pack: Alpha (3 charts, SAMPLESTART 30), Beta + Gamma (1 chart
// each), and a hidden SELECTABLE:NO song.
td::SongLibrary make_library(const std::filesystem::path& root) {
    std::filesystem::remove_all(root);
    const std::filesystem::path pack = root / "Test Pack";

    write_file(pack / "banner.png", "fake pack banner");

    write_file(pack / "Alpha" / "banner.png", "fake banner");
    write_file(pack / "Alpha" / "audio.ogg", "fake audio");
    write_file(pack / "Alpha" / "Alpha.sm",
               make_sm("Alpha", "30.0",
                       {{"Beginner", 1}, {"Medium", 5}, {"Challenge", 10}}));

    write_file(pack / "Beta" / "banner.png", "fake banner");
    write_file(pack / "Beta" / "audio.ogg", "fake audio");
    write_file(pack / "Beta" / "Beta.sm", make_sm("Beta", "", {{"Easy", 3}}));

    write_file(pack / "Gamma" / "banner.png", "fake banner");
    write_file(pack / "Gamma" / "audio.ogg", "fake audio");
    write_file(pack / "Gamma" / "Gamma.sm", make_sm("Gamma", "", {{"Hard", 8}}));

    write_file(pack / "Hidden" / "banner.png", "fake banner");
    write_file(pack / "Hidden" / "Hidden.sm",
               make_sm("Hidden", "", {{"Challenge", 12}}, "NO"));

    td::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    return library;
}

// Navigates the wheel (wrapping) until a song with `chart_count` charts is
// highlighted. Returns true when found.
bool navigate_to_chart_count(td::ScreenManager& manager, td::SelectScreen* select, int chart_count) {
    for (int i = 0; i < static_cast<int>(select->song_count()); ++i) {
        if (select->chart_count() == chart_count) {
            return true;
        }
        manager.update(kDt, {press(GameAction::Down)});
    }
    return select->chart_count() == chart_count;
}

void test_wheel_load(const td::SelectScreen* select) {
    TEST_CHECK(select->song_count() == 3); // the SELECTABLE:NO song is hidden
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->selected_song() != nullptr);
    TEST_CHECK(select->preview().requested_path().find("audio.ogg") != std::string::npos);
    std::cout << "  - wheel load + SELECTABLE filtering ok.\n";
}

void test_song_navigation(td::ScreenManager& manager, td::SelectScreen* select) {
    // Ensure a known start.
    while (select->selected_song_index() != 0) {
        manager.update(kDt, {press(GameAction::Down)});
    }
    const std::string first_title = select->selected_song()->metadata.title;

    manager.update(kDt, {press(GameAction::Up)});
    TEST_CHECK(select->selected_song_index() == static_cast<int>(select->song_count()) - 1);
    TEST_CHECK(select->selected_song()->metadata.title != first_title);

    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->selected_song()->metadata.title == first_title);
    std::cout << "  - Up/Down wrap the wheel highlight ok.\n";
}

void test_difficulty_navigation(td::ScreenManager& manager, td::SelectScreen* select) {
    TEST_CHECK(navigate_to_chart_count(manager, select, 3));
    TEST_CHECK(select->selected_chart_index() == 0);

    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(select->selected_chart_index() == 1);
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(select->selected_chart_index() == 2);
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(select->selected_chart_index() == 2); // clamped

    manager.update(kDt, {press(GameAction::Left)});
    TEST_CHECK(select->selected_chart_index() == 1);
    manager.update(kDt, {press(GameAction::Left)});
    TEST_CHECK(select->selected_chart_index() == 0);
    manager.update(kDt, {press(GameAction::Left)});
    TEST_CHECK(select->selected_chart_index() == 0); // clamped

    // A single-chart song ignores Left/Right.
    TEST_CHECK(navigate_to_chart_count(manager, select, 1));
    TEST_CHECK(select->selected_chart_index() == 0);
    manager.update(kDt, {press(GameAction::Right)});
    manager.update(kDt, {press(GameAction::Left)});
    TEST_CHECK(select->selected_chart_index() == 0);
    std::cout << "  - Left/Right clamp within difficulty list ok.\n";
}

void test_best_grade(td::ScreenManager& manager, td::SelectScreen* select, td::HighScores& scores) {
    const td::Song* song = select->selected_song();
    const td::Chart* chart = select->selected_chart();
    TEST_CHECK(song != nullptr && chart != nullptr);

    TEST_CHECK(td::best_grade_for(manager.context(), *song, *chart) == nullptr);

    td::ScoreRecord record;
    record.grade = "quad_star";
    record.percent = 0.97;
    scores.scores[td::make_chart_key(*song, *chart)] = record;

    const td::ScoreRecord* found = td::best_grade_for(manager.context(), *song, *chart);
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->grade == "quad_star");

    TEST_CHECK(td::grade_display_label("quad_star") == "★★★★");
    TEST_CHECK(td::grade_display_label("triple_star") == "★★★");
    TEST_CHECK(td::grade_display_label("double_star") == "★★");
    TEST_CHECK(td::grade_display_label("single_star") == "★");
    TEST_CHECK(td::grade_display_label("S+") == "S+");
    TEST_CHECK(td::grade_display_label("unknown") == "unknown");
    std::cout << "  - best grade lookup + star label mapping ok.\n";
}

void test_bpm_formatting() {
    td::TimingData single;
    TEST_CHECK(single.parse_bpms_string("0.0=140.0"));
    TEST_CHECK(td::format_bpm_range(single) == "140");

    td::TimingData multi;
    TEST_CHECK(multi.parse_bpms_string("0.0=128.0,16.0=175.0"));
    TEST_CHECK(td::format_bpm_range(multi) == "128-175");

    td::TimingData def; // TimingData always seeds a 120 BPM segment
    TEST_CHECK(td::format_bpm_range(def) == "120");
    std::cout << "  - BPM range formatting ok.\n";
}

void test_options_derivation() {
    td::GameConfig config;
    config.gameplay.speed_mod = "C400";
    config.gameplay.scroll = "down";
    config.gameplay.fail_enabled = false;
    config.offset.global_offset_seconds = 0.02;

    const td::GameplayOptions options = td::gameplay_options_from_config(config);
    TEST_CHECK(options.speed.type == td::SpeedModType::CMod);
    TEST_CHECK(options.speed.value == 400.0);
    TEST_CHECK(options.scroll == td::ScrollDirection::Down);
    TEST_CHECK(!options.fail_enabled);
    TEST_CHECK(options.global_offset_seconds == 0.02);

    td::GameConfig invalid;
    invalid.gameplay.speed_mod = "zzz";
    const td::GameplayOptions fallback = td::gameplay_options_from_config(invalid);
    TEST_CHECK(fallback.speed.type == td::SpeedModType::XMod);
    TEST_CHECK(fallback.speed.value == 1.0);
    std::cout << "  - config -> gameplay options derivation ok.\n";
}

void test_confirm_handoff(td::ScreenManager& manager, td::SelectScreen* select,
                          td::GameplayScreen* gameplay, td::PlayRequest& request) {
    const td::Song* expected_song = select->selected_song();
    const td::Chart* expected_chart = select->selected_chart();
    TEST_CHECK(expected_song != nullptr && expected_chart != nullptr);

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(gameplay->is_ready());
    TEST_CHECK(request.song == expected_song);
    TEST_CHECK(request.chart == expected_chart);

    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - Confirm -> Gameplay handoff and Back abort ok.\n";
}

void test_selection_preserved_on_reenter(td::ScreenManager& manager, td::SelectScreen* select) {
    // Highlight a non-first song so a reset would be observable.
    while (select->selected_song_index() != 1) {
        manager.update(kDt, {press(GameAction::Down)});
    }
    const int song_before = select->selected_song_index();
    const int chart_before = select->selected_chart_index();
    const std::string title_before = select->selected_song()->metadata.title;

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    TEST_CHECK(select->selected_song_index() == song_before);
    TEST_CHECK(select->selected_chart_index() == chart_before);
    TEST_CHECK(select->selected_song() != nullptr);
    TEST_CHECK(select->selected_song()->metadata.title == title_before);
    std::cout << "  - wheel selection (song + chart) preserved on re-enter ok.\n";
}

void test_gameplay_held_state(td::ScreenManager& manager, td::GameplayScreen* gameplay) {
    if (manager.active_id() != ScreenId::Gameplay) {
        manager.update(kDt, {press(GameAction::Confirm)});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(gameplay->is_ready());

    bool left_down = false;
    manager.context().action_down = [&left_down](GameAction action) {
        return action == GameAction::Left && left_down;
    };

    // No key event at all, but the authoritative state reports Left held.
    left_down = true;
    manager.update(kDt, {});
    TEST_CHECK(gameplay->held(0));

    // A missed release (no Up event delivered) is still observed because the
    // held-state is sampled from the callback, not replayed from events.
    left_down = false;
    manager.update(kDt, {});
    TEST_CHECK(!gameplay->held(0));

    manager.context().action_down = nullptr;
    std::cout << "  - gameplay held-state samples the authoritative callback ok.\n";
}

void test_empty_library() {
    td::SongLibrary empty_library;
    td::GameConfig config;
    td::HighScores scores;
    td::PlayRequest request;

    auto select = std::make_unique<td::SelectScreen>();
    td::SelectScreen* select_ptr = select.get();
    td::ScreenManager manager(0.0);
    manager.add_screen(std::move(select));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &empty_library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);

    TEST_CHECK(select_ptr->song_count() == 0);
    manager.update(kDt, {press(GameAction::Down)});
    manager.update(kDt, {press(GameAction::Up)});
    manager.update(kDt, {press(GameAction::Right)});
    manager.update(kDt, {press(GameAction::Left)});

    td::GlQuadRenderer renderer; // uninitialized: safe no-op
    manager.render(renderer, 1280, 720);

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(request.song == nullptr && request.chart == nullptr);
    std::cout << "  - empty library is crash-free and inert ok.\n";
}

} // namespace

int main() {
    std::cout << "[select_screen_test] Running SelectScreen tests...\n";

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "tundra_select_screen_test";
    td::SongLibrary library = make_library(root);
    TEST_CHECK(library.total_songs() == 4);

    td::GameConfig config;
    td::HighScores scores;
    td::PlayRequest request;

    auto select = std::make_unique<td::SelectScreen>();
    td::SelectScreen* select_ptr = select.get();
    auto gameplay = std::make_unique<td::GameplayScreen>();
    td::GameplayScreen* gameplay_ptr = gameplay.get();

    td::ScreenManager manager(0.0); // disable idle-attract for determinism
    manager.add_screen(std::move(select));
    manager.add_screen(std::move(gameplay));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);

    test_wheel_load(select_ptr);
    test_song_navigation(manager, select_ptr);
    test_difficulty_navigation(manager, select_ptr);
    test_best_grade(manager, select_ptr, scores);
    test_bpm_formatting();
    test_options_derivation();
    test_confirm_handoff(manager, select_ptr, gameplay_ptr, request);
    test_selection_preserved_on_reenter(manager, select_ptr);
    test_gameplay_held_state(manager, gameplay_ptr);
    test_empty_library();

    td::GlQuadRenderer renderer; // populated-screen render smoke
    manager.render(renderer, 1280, 720);

    std::filesystem::remove_all(root);

    std::cout << "[select_screen_test] All tests passed!\n";
    return 0;
}
