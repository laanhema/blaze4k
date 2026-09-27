#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
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
#include "data/config_loader.hpp"
#include "data/high_scores.hpp"
#include "gameplay/gameplay_options.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/calibration_screen.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/input_remap_screen.hpp"
#include "screens/options_menu.hpp"
#include "screens/play_request.hpp"
#include "screens/select_screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/title_screen.hpp"

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

    // The wheel lists difficulties hardest-at-top/easiest-at-bottom: the Alpha
    // song's simfile order (Beginner 1, Medium 5, Challenge 10) is reordered to
    // descending foot rating.
    const td::Song* song = select->selected_song();
    TEST_CHECK(song != nullptr && song->charts.size() == 3);
    TEST_CHECK(song->charts[0].meter == 10 && song->charts[1].meter == 5 &&
               song->charts[2].meter == 1);

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

void test_held_navigation_repeat(td::ScreenManager& manager, td::SelectScreen* select) {
    while (select->selected_song_index() != 0) {
        manager.update(kDt, {press(GameAction::Down)});
    }

    GameAction held = GameAction::None;
    manager.context().action_down = [&held](GameAction action) { return action == held; };

    held = GameAction::Down;
    manager.update(kDt, {press(GameAction::Down)}); // initial step
    TEST_CHECK(select->selected_song_index() == 1);

    // Below the initial delay the direction is held but does not repeat yet.
    manager.update(kDt, {});
    manager.update(kDt, {});
    TEST_CHECK(select->selected_song_index() == 1);

    // Past the delay it accelerates through the wheel without further presses.
    manager.update(kDt, {});
    manager.update(kDt, {});
    TEST_CHECK(select->selected_song_index() != 1);

    // Releasing stops the repeat (sampled from the authoritative callback).
    held = GameAction::None;
    manager.update(kDt, {});
    const int after_release = select->selected_song_index();
    manager.update(kDt, {});
    TEST_CHECK(select->selected_song_index() == after_release);

    manager.context().action_down = nullptr;
    std::cout << "  - held direction repeat accelerates and stops on release ok.\n";
}

void test_best_score(td::ScreenManager& manager, td::SelectScreen* select, td::HighScores& scores) {
    const td::Song* song = select->selected_song();
    const td::Chart* chart = select->selected_chart();
    TEST_CHECK(song != nullptr && chart != nullptr);

    TEST_CHECK(td::best_score_for(manager.context(), *song, *chart) == nullptr);

    td::ScoreRecord record;
    record.grade = "quad_star";
    record.percent = 0.97;
    scores.scores[td::make_chart_key(*song, *chart)] = record;

    const td::ScoreRecord* found = td::best_score_for(manager.context(), *song, *chart);
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->grade == "quad_star");
    TEST_CHECK(found->percent == 0.97);
    std::cout << "  - best score lookup ok.\n";
}

bool same_color(td::Color a, td::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void test_difficulty_colors() {
    const td::Color beginner = td::difficulty_color("Beginner");
    const td::Color easy = td::difficulty_color("Easy");
    const td::Color medium = td::difficulty_color("Medium");
    const td::Color hard = td::difficulty_color("Hard");
    const td::Color challenge = td::difficulty_color("Challenge");
    const td::Color edit = td::difficulty_color("Edit");

    // Each canonical difficulty gets its own tint; Edit falls back to neutral.
    const td::Color all[] = {beginner, easy, medium, hard, challenge, edit};
    for (std::size_t i = 0; i < std::size(all); ++i) {
        for (std::size_t j = i + 1; j < std::size(all); ++j) {
            TEST_CHECK(!same_color(all[i], all[j]));
        }
    }
    TEST_CHECK(same_color(td::difficulty_color("unknown"), edit));

    // Labels are passthrough, so matching is case-insensitive; Novice aliases Beginner.
    TEST_CHECK(same_color(td::difficulty_color("hard"), hard));
    TEST_CHECK(same_color(td::difficulty_color("CHALLENGE"), challenge));
    TEST_CHECK(same_color(td::difficulty_color("Novice"), beginner));

    // Hue sanity: hard is red-dominant, challenge blue-dominant, easy green-dominant.
    TEST_CHECK(hard.r > hard.g && hard.r > hard.b);
    TEST_CHECK(challenge.b > challenge.r && challenge.b > challenge.g);
    TEST_CHECK(easy.g > easy.r && easy.g > easy.b);
    std::cout << "  - difficulty color mapping ok.\n";
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
    config.gameplay.assist_tick = true;
    config.offset.global_offset_seconds = 0.02;

    const td::GameplayOptions options = td::gameplay_options_from_config(config);
    TEST_CHECK(options.speed.type == td::SpeedModType::CMod);
    TEST_CHECK(options.speed.value == 400.0);
    TEST_CHECK(options.scroll == td::ScrollDirection::Down);
    TEST_CHECK(!options.fail_enabled);
    TEST_CHECK(options.assist_tick);
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

    // The options overlay is crash-free on an empty library too.
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(select_ptr->options_open());
    manager.update(kDt, {press(GameAction::Down)});
    manager.update(kDt, {press(GameAction::Right)});
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select_ptr->options_open());

    td::GlQuadRenderer renderer; // uninitialized: safe no-op
    manager.render(renderer, 1280, 720);

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(request.song == nullptr && request.chart == nullptr);
    std::cout << "  - empty library is crash-free and inert ok.\n";
}

// Drives the C4 overlay through the real ScreenManager + shared GameConfig,
// verifying open/adjust/close, wheel suspension, gameplay application, and the
// real C2 save/load persistence path.
void test_options_overlay(td::ScreenManager& manager, td::SelectScreen* select,
                          td::GameConfig& config, td::PlayRequest& request) {
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 1. Open; the wheel must be suspended while the overlay is up.
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(select->options_open());
    const int song_before = select->selected_song_index();
    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->selected_song_index() == song_before);

    // 2. Adjust SpeedType -> CMOD, Speed value 450 -> 400, Scroll -> DOWN, Fail -> OFF.
    manager.update(kDt, {press(GameAction::Up)});    // row back to SpeedType
    manager.update(kDt, {press(GameAction::Right)}); // XMOD -> CMOD
    TEST_CHECK(select->options_menu().speed_type == td::SpeedModType::CMod);
    manager.update(kDt, {press(GameAction::Down)});  // row: SpeedValue
    for (int i = 0; i < 5; ++i) {
        manager.update(kDt, {press(GameAction::Left)}); // 450 -> 400
    }
    TEST_CHECK(config.gameplay.speed_mod == "C400");

    manager.update(kDt, {press(GameAction::Down)});  // row: Scroll
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(config.gameplay.scroll == "down");

    manager.update(kDt, {press(GameAction::Down)});  // row: Fail
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(!config.gameplay.fail_enabled);

    // 3. Back closes the overlay without leaving Select; a second Back navigates.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 4. Confirm publishes the changed options to gameplay (AC2).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(request.options.speed.type == td::SpeedModType::CMod);
    TEST_CHECK(request.options.speed.value == 400.0);
    TEST_CHECK(request.options.scroll == td::ScrollDirection::Down);
    TEST_CHECK(!request.options.fail_enabled);

    manager.update(kDt, {press(GameAction::Back)}); // abort gameplay -> Select
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 5. Persist through the real C2 path and confirm the fields survive.
    const std::filesystem::path config_path =
        std::filesystem::temp_directory_path() / "tundra_select_options_config.json";
    TEST_CHECK(td::save_config(config_path, config));
    const td::GameConfig reloaded = td::load_config(config_path);
    TEST_CHECK(reloaded.gameplay.speed_mod == "C400");
    TEST_CHECK(reloaded.gameplay.scroll == "down");
    TEST_CHECK(!reloaded.gameplay.fail_enabled);
    std::filesystem::remove(config_path);

    // 6. With the overlay closed, Back still exits Select -> Title.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    std::cout << "  - options overlay open/adjust/close + gameplay + persistence ok.\n";
}

// The only production path that launches the wizard: Select's options overlay ->
// CalibrateOffset row -> Confirm/Right -> transition_to(Calibration). Also pins
// that Options (Tab) still closes the overlay while that action row is
// highlighted, and that Back aborts the wizard without touching the offset.
void test_calibration_launch_from_options(td::ScreenManager& manager, td::SelectScreen* select,
                                          td::GameConfig& config) {
    manager.add_screen(std::make_unique<td::CalibrationScreen>());
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    const auto move_to_calibrate_row = [&] {
        for (int i = 0; i < static_cast<int>(td::OptionsRow::CalibrateOffset); ++i) {
            manager.update(kDt, {press(GameAction::Down)});
        }
        TEST_CHECK(select->options_menu().row ==
                   static_cast<int>(td::OptionsRow::CalibrateOffset));
    };

    // Confirm launches the wizard.
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(select->options_open());
    move_to_calibrate_row();
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Calibration);
    // Calibration plays its own metronome: the song preview must be stopped.
    TEST_CHECK(select->preview().requested_path().empty());

    // Back aborts the wizard with the persisted offset untouched, landing back in
    // the options overlay on the calibration row (not on the bare wheel).
    const double offset_before = config.offset.global_offset_seconds;
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(config.offset.global_offset_seconds == offset_before);
    TEST_CHECK(select->options_open());
    TEST_CHECK(select->options_menu().row == static_cast<int>(td::OptionsRow::CalibrateOffset));

    // Right launches it as well, straight from the reopened overlay.
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(manager.active_id() == ScreenId::Calibration);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->options_open());

    // A second Back then closes the overlay and stays on Select.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // Options (Tab/shoulder) closes the overlay on the calibration row too.
    manager.update(kDt, {press(GameAction::Options)});
    move_to_calibrate_row();
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - options -> Calibration launch seam + row toggle ok.\n";
}

// The C6 launch seam: Select's options overlay -> RemapInput row -> Confirm/Right
// -> transition_to(InputRemap). Mirrors the calibration launch test.
void test_remap_launch_from_options(td::ScreenManager& manager, td::SelectScreen* select,
                                    td::GameConfig& config) {
    manager.add_screen(std::make_unique<td::InputRemapScreen>());
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    const auto move_to_remap_row = [&] {
        for (int i = 0; i < static_cast<int>(td::OptionsRow::RemapInput); ++i) {
            manager.update(kDt, {press(GameAction::Down)});
        }
        TEST_CHECK(select->options_menu().row == static_cast<int>(td::OptionsRow::RemapInput));
    };

    // Confirm launches the remap screen.
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(select->options_open());
    move_to_remap_row();
    const std::string preview_path = select->preview().requested_path();
    TEST_CHECK(!preview_path.empty());
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::InputRemap);
    // The song preview keeps playing behind the remap screen (not stopped on exit).
    TEST_CHECK(select->preview().requested_path() == preview_path);

    // Back returns to the options overlay on the remap row; bindings are
    // untouched by a plain exit.
    const auto key_bindings_before = config.input.key_bindings;
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(config.input.key_bindings == key_bindings_before);
    TEST_CHECK(select->preview().requested_path() == preview_path);
    TEST_CHECK(select->options_open());
    TEST_CHECK(select->options_menu().row == static_cast<int>(td::OptionsRow::RemapInput));

    // Right launches it as well, straight from the reopened overlay.
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(manager.active_id() == ScreenId::InputRemap);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->options_open());
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select->options_open());

    // Options (Tab/shoulder) closes the overlay on the remap row too.
    manager.update(kDt, {press(GameAction::Options)});
    move_to_remap_row();
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - options -> InputRemap launch seam + row toggle ok.\n";
}

// A same-tick [Options, Back] pair must not navigate on the pre-update modal
// state (the manager must not act on state update() is about to create).
void test_same_tick_options_back(td::ScreenManager& manager, td::SelectScreen* select) {
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(!select->options_open());

    manager.update(kDt, {press(GameAction::Options), press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // A lone Back with the overlay closed still navigates as before.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    std::cout << "  - same-tick Options+Back does not navigate on stale state ok.\n";
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
    manager.add_screen(std::make_unique<td::TitleScreen>());
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
    test_held_navigation_repeat(manager, select_ptr);
    test_best_score(manager, select_ptr, scores);
    test_difficulty_colors();
    test_bpm_formatting();
    test_options_derivation();
    test_confirm_handoff(manager, select_ptr, gameplay_ptr, request);
    test_selection_preserved_on_reenter(manager, select_ptr);
    test_gameplay_held_state(manager, gameplay_ptr);
    test_empty_library();
    test_options_overlay(manager, select_ptr, config, request);
    test_calibration_launch_from_options(manager, select_ptr, config);
    test_remap_launch_from_options(manager, select_ptr, config);
    test_same_tick_options_back(manager, select_ptr);

    td::GlQuadRenderer renderer; // populated-screen render smoke
    manager.render(renderer, 1280, 720);

    std::filesystem::remove_all(root);

    std::cout << "[select_screen_test] All tests passed!\n";
    return 0;
}
