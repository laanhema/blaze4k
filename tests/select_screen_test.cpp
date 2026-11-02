#include <algorithm>
#include <cmath>
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
#include "render/theme.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/calibration_screen.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/input_remap_screen.hpp"
#include "screens/options_menu.hpp"
#include "screens/play_request.hpp"
#include "screens/select_art.hpp"
#include "screens/select_screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/song_display_text.hpp"
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

using blaze4k::GameAction;
using blaze4k::InputEvent;
using blaze4k::ScreenContext;
using blaze4k::ScreenId;

constexpr double kDt = 0.1;

const std::filesystem::path kSourceDir{BLAZE4K_SOURCE_DIR};
const std::filesystem::path kCabinet =
    std::filesystem::path{BLAZE4K_ASSETS_DIR} / "theme" / "cabinet";

// Real headless theme + text services (#94): measuring and truncation work
// without GL; draws are no-ops.
blaze4k::ThemeTextures& loaded_theme() {
    static blaze4k::ThemeTextures theme;
    static const bool loaded = theme.load(kCabinet);
    if (!loaded) {
        std::cerr << "theme failed to load from " << kCabinet << "\n";
        std::abort();
    }
    return theme;
}

blaze4k::TextRenderer& loaded_text() {
    static blaze4k::TextRenderer text;
    static const bool loaded = text.load(kSourceDir);
    if (!loaded) {
        std::cerr << "fonts failed to load from " << kSourceDir << "\n";
        std::abort();
    }
    text.set_window_size(1280, 720);
    return text;
}

void write_file(const std::filesystem::path& path, std::string_view content,
                bool binary = false) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, binary ? std::ios::out | std::ios::binary : std::ios::out);
    ofs << content;
}

std::string make_sm(const std::string& title, const std::string& sample_start,
                    const std::vector<std::pair<std::string, int>>& charts,
                    const std::string& selectable = "YES",
                    const std::string& extra_tags = "") {
    std::ostringstream out;
    out << "#TITLE:" << title << ";\n"
        << "#ARTIST:Test Artist;\n"
        << extra_tags
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
blaze4k::SongLibrary make_library(const std::filesystem::path& root) {
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

    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    return library;
}

// Navigates the wheel (wrapping) until a song with `chart_count` charts is
// highlighted. Returns true when found.
bool navigate_to_chart_count(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select, int chart_count) {
    // One full lap of the visible rows (the open pack's header stop included).
    for (int i = 0; i < static_cast<int>(select->wheel_row_count()); ++i) {
        if (select->chart_count() == chart_count) {
            return true;
        }
        manager.update(kDt, {press(GameAction::Down)});
    }
    return select->chart_count() == chart_count;
}

// StepMania's MusicWheel opens no section on first entry; neither does Select. The
// fixture has one pack ("Test Pack", 3 selectable songs), so the wheel is one closed
// header. Leaves the pack open with song 0 highlighted for the tests that follow.
void test_first_entry(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
    TEST_CHECK(select->song_count() == 3);
    TEST_CHECK(select->open_pack_name().empty());
    TEST_CHECK(select->wheel_row_count() == 1);
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->selected_pack_name() == "Test Pack");
    // The info panel stays empty on a header and no preview plays.
    TEST_CHECK(select->selected_song() == nullptr && select->selected_song_index() == -1);
    TEST_CHECK(select->chart_count() == 0 && select->selected_chart() == nullptr);
    TEST_CHECK(select->preview().requested_path().empty());
    // The hint bar says what Confirm does on this closed header.
    TEST_CHECK(select->confirm_hint() == blaze4k::select_art::ConfirmHint::Open);
    // The banner area shows the pack's banner (Test Pack/banner.png), not a song's.
    const std::filesystem::path pack_banner{select->cursor_banner_path()};
    TEST_CHECK(pack_banner.filename() == "banner.png");
    TEST_CHECK(pack_banner.parent_path().filename() == "Test Pack");

    // Confirm opens it (the cursor stays on the header; Select stays active).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->open_pack_name() == "Test Pack");
    TEST_CHECK(select->wheel_row_count() == 4); // 1 header + 3 songs
    TEST_CHECK(select->on_pack_header());
    TEST_CHECK(select->confirm_hint() == blaze4k::select_art::ConfirmHint::Close);

    // Down onto song 0: Confirm now plays, the banner is the song's, a preview starts.
    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->selected_song_index() == 0 && select->selected_wheel_row() == 1);
    TEST_CHECK(select->confirm_hint() == blaze4k::select_art::ConfirmHint::Play);
    const blaze4k::Song* song = select->selected_song();
    TEST_CHECK(song != nullptr);
    TEST_CHECK(select->cursor_banner_path() == song->resolved_banner_path);
    TEST_CHECK(std::filesystem::path{select->cursor_banner_path()}.parent_path().filename() ==
               std::filesystem::path{song->song_dir}.filename());
    TEST_CHECK(!select->preview().requested_path().empty());
    std::cout
        << "  - first entry: all closed, header selected, hint OPEN/CLOSE/PLAY, banners ok.\n";
}

// A header shows its pack's banner, else the library's fallback banner (what a song
// without a banner resolves to); never a song's banner.
void test_header_banner() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_header_banner_test";
    std::filesystem::remove_all(root);
    write_file(root / "With" / "banner.png", "fake pack banner");
    write_file(root / "With" / "Song A" / "audio.ogg", "fake audio");
    write_file(root / "With" / "Song A" / "Song A.sm", make_sm("Song A", "", {{"Easy", 3}}));
    write_file(root / "With" / "Song A" / "banner.png", "fake song banner");
    write_file(root / "Without" / "Song B" / "audio.ogg", "fake audio");
    write_file(root / "Without" / "Song B" / "Song B.sm", make_sm("Song B", "", {{"Easy", 3}}));
    blaze4k::SongLibrary library;
    const std::filesystem::path fallback = root / "fallback_banner.png";
    library.set_fallback_banner(fallback);
    TEST_CHECK(library.scan_directory(root));

    blaze4k::GameConfig config;
    blaze4k::PlayRequest request;
    blaze4k::ScreenManager manager(0.0);
    auto screen = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select = screen.get();
    manager.add_screen(std::move(screen));
    manager.context().config = &config;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);

    // Visit both headers (closed): the pack with a banner shows it, the other the fallback.
    int with_banner = 0;
    int with_fallback = 0;
    for (int i = 0; i < 2; ++i) {
        TEST_CHECK(select->on_pack_header());
        const std::string& path = select->cursor_banner_path();
        if (select->selected_pack_name() == "With") {
            ++with_banner;
            TEST_CHECK(std::filesystem::path{path}.filename() == "banner.png");
            TEST_CHECK(std::filesystem::path{path}.parent_path().filename() == "With");
        } else {
            TEST_CHECK(select->selected_pack_name() == "Without");
            ++with_fallback;
            TEST_CHECK(path == fallback.string());
        }
        manager.update(kDt, {press(GameAction::Down)});
    }
    TEST_CHECK(with_banner == 1 && with_fallback == 1);

    // Onto a song of "With": its own banner, not the pack banner.
    while (!select->on_pack_header() || select->selected_pack_name() != "With") {
        manager.update(kDt, {press(GameAction::Down)});
    }
    manager.update(kDt, {press(GameAction::Confirm)}); // open "With"
    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(!select->on_pack_header());
    TEST_CHECK(std::filesystem::path{select->cursor_banner_path()}.parent_path().filename() ==
               "Song A");
    std::filesystem::remove_all(root);
    std::cout << "  - header banner: pack banner, else fallback, song row keeps its own ok.\n";
}

void test_wheel_load(const blaze4k::SelectScreen* select) {
    TEST_CHECK(select->song_count() == 3); // the SELECTABLE:NO song is hidden
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->selected_song() != nullptr);
    TEST_CHECK(select->preview().requested_path().find("audio.ogg") != std::string::npos);
    std::cout << "  - wheel load + SELECTABLE filtering ok.\n";
}

void test_song_navigation(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
    // Ensure a known start.
    while (select->selected_song_index() != 0) {
        manager.update(kDt, {press(GameAction::Down)});
    }
    const std::string first_title = select->selected_song()->metadata.title;

    // Up from the first song stops on the pack header, then wraps to the last song.
    manager.update(kDt, {press(GameAction::Up)});
    TEST_CHECK(select->on_pack_header());
    TEST_CHECK(select->selected_song_index() == -1 && select->selected_song() == nullptr);
    manager.update(kDt, {press(GameAction::Up)});
    TEST_CHECK(select->selected_song_index() == static_cast<int>(select->song_count()) - 1);
    TEST_CHECK(select->selected_song()->metadata.title != first_title);

    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->on_pack_header());
    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->selected_song()->metadata.title == first_title);
    std::cout << "  - Up/Down wrap the wheel highlight ok.\n";
}

void test_difficulty_navigation(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
    TEST_CHECK(navigate_to_chart_count(manager, select, 3));
    TEST_CHECK(select->selected_chart_index() == 0);

    // The wheel lists difficulties hardest-at-top/easiest-at-bottom: the Alpha
    // song's simfile order (Beginner 1, Medium 5, Challenge 10) is reordered to
    // descending foot rating.
    const blaze4k::Song* song = select->selected_song();
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

void test_held_navigation_repeat(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
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

void test_best_score(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select, blaze4k::HighScores& scores) {
    const blaze4k::Song* song = select->selected_song();
    const blaze4k::Chart* chart = select->selected_chart();
    TEST_CHECK(song != nullptr && chart != nullptr);

    TEST_CHECK(blaze4k::best_score_for(manager.context(), *song, *chart) == nullptr);

    blaze4k::ScoreRecord record;
    record.grade = "quad_star";
    record.percent = 0.97;
    scores.scores[blaze4k::make_chart_key(*song, *chart)] = record;

    const blaze4k::ScoreRecord* found = blaze4k::best_score_for(manager.context(), *song, *chart);
    TEST_CHECK(found != nullptr);
    TEST_CHECK(found->grade == "quad_star");
    TEST_CHECK(found->percent == 0.97);
    std::cout << "  - best score lookup ok.\n";
}

// Named Edit charts through the real SongLibrary -> SimfileParser ->
// SelectScreen::render path (#84): an .sm song (#NOTES description) and an .ssc
// song (#CHARTNAME + #DESCRIPTION, including a long UTF-8 name).
void test_named_edit_charts() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_edit_test";
    std::filesystem::remove_all(root);
    const std::filesystem::path pack = root / "Edit Pack";
    const std::string rows = "1000\n0100\n0010\n0001\n";

    write_file(pack / "SmSong" / "audio.ogg", "fake audio");
    write_file(pack / "SmSong" / "SmSong.sm",
               "#TITLE:Sm Edit Song;\n#ARTIST:Test Artist;\n#MUSIC:audio.ogg;\n"
               "#BPMS:0.0=128.0;\n"
               "#NOTES:dance-single::Hard:9:0,0,0,0,0:\n" + rows + ";\n"
               "#NOTES:dance-single:JBEAN:Edit:10:0,0,0,0,0:\n" + rows + ";\n",
               true);

    const std::string long_utf8 = "Caf\xC3\xA9 \xE2\x98\xBA Extremely Long Custom Edit Name";
    write_file(pack / "SscSong" / "audio.ogg", "fake audio");
    write_file(pack / "SscSong" / "SscSong.ssc",
               "#VERSION:0.83;\n#TITLE:Ssc Edit Song;\n#ARTIST:Test Artist;\n"
               "#MUSIC:audio.ogg;\n#BPMS:0.0=128.0;\n"
               "#NOTEDATA:;\n#STEPSTYPE:dance-single;\n#CHARTNAME:Chart Title;\n"
               "#DESCRIPTION:My Edit;\n#DIFFICULTY:Edit;\n#METER:12;\n#NOTES:\n" + rows + ";\n"
               "#NOTEDATA:;\n#STEPSTYPE:dance-single;\n#DESCRIPTION:" + long_utf8 +
               ";\n#CHARTNAME:Other Title;\n#DIFFICULTY:Edit;\n#METER:11;\n#NOTES:\n" + rows +
               ";\n"
               "#NOTEDATA:;\n#STEPSTYPE:dance-single;\n#CHARTNAME:Only Name;\n"
               "#DIFFICULTY:Edit;\n#METER:10;\n#NOTES:\n" + rows + ";\n",
               true);

    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    TEST_CHECK(library.total_songs() == 2);

    std::vector<std::string> labels;
    for (const blaze4k::SongPack& song_pack : library.packs()) {
        for (const blaze4k::Song& song : song_pack.songs) {
            for (const blaze4k::Chart& chart : song.charts) {
                labels.push_back(blaze4k::chart_display_label(chart, 1000));
            }
        }
    }
    auto has = [&labels](const std::string& label) {
        return std::find(labels.begin(), labels.end(), label) != labels.end();
    };
    TEST_CHECK(labels.size() == 5);
    TEST_CHECK(has("Hard"));
    TEST_CHECK(has("JBEAN"));
    TEST_CHECK(has("My Edit"));
    TEST_CHECK(has(long_utf8));
    TEST_CHECK(has("Edit")); // #CHARTNAME only: falls back to the label
    TEST_CHECK(!has("Chart Title") && !has("Other Title") && !has("Only Name"));

    blaze4k::GameConfig config;
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;
    auto select = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select_ptr = select.get();
    blaze4k::ScreenManager manager(0.0);
    manager.add_screen(std::move(select));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);
    TEST_CHECK(select_ptr->song_count() == 2);
    manager.update(kDt, {press(GameAction::Confirm)}); // open the only pack
    manager.update(kDt, {press(GameAction::Down)});    // onto its first song
    TEST_CHECK(select_ptr->selected_song_index() == 0);

    blaze4k::TextRenderer& text = loaded_text();
    manager.context().theme = &loaded_theme();
    manager.context().text = &text;

    blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
    std::vector<std::string> visited;
    for (int song = 0; song < 2; ++song) {
        const int charts = select_ptr->chart_count();
        for (int i = 0; i < charts; ++i) {
            const blaze4k::Chart* chart = select_ptr->selected_chart();
            TEST_CHECK(chart != nullptr);
            visited.push_back(blaze4k::chart_display_label(*chart, 1000));
            // The draw site's rule: the row label, truncated by measured width
            // to the tab's name budget (select_art::kDiffNameBudget).
            const std::string label = blaze4k::select_art::difficulty_row_label(*chart);
            for (const blaze4k::theme::TextStyle& style :
                 {blaze4k::theme::text::kDiffName, blaze4k::theme::text::kDiffNameSelected}) {
                const std::string shown =
                    text.truncate(label, style, blaze4k::select_art::kDiffNameBudget);
                TEST_CHECK(text.measure(shown, style) <= blaze4k::select_art::kDiffNameBudget);
                TEST_CHECK(!shown.empty());
            }
            manager.render(renderer, 1280, 720);
            manager.render(renderer, 640, 480);
            text.set_window_size(1280, 720);
            manager.update(kDt, {press(GameAction::Right)});
        }
        manager.update(kDt, {press(GameAction::Down)});
    }
    std::sort(visited.begin(), visited.end());
    std::sort(labels.begin(), labels.end());
    TEST_CHECK(visited == labels);
    std::filesystem::remove_all(root);
    std::cout << "  - named Edit charts (.sm + .ssc) through the real render path ok.\n";
}

void test_bpm_formatting() {
    blaze4k::TimingData single;
    TEST_CHECK(single.parse_bpms_string("0.0=140.0"));
    TEST_CHECK(blaze4k::format_bpm_range(single) == "140");

    blaze4k::TimingData multi;
    TEST_CHECK(multi.parse_bpms_string("0.0=128.0,16.0=175.0"));
    TEST_CHECK(blaze4k::format_bpm_range(multi) == "128-175");

    blaze4k::TimingData def; // TimingData always seeds a 120 BPM segment
    TEST_CHECK(blaze4k::format_bpm_range(def) == "120");
    std::cout << "  - BPM range formatting ok.\n";
}

void test_options_derivation() {
    blaze4k::GameConfig config;
    config.gameplay.speed_mod = "C400";
    config.gameplay.scroll = "down";
    config.gameplay.fail_enabled = false;
    config.gameplay.assist_tick = true;
    config.offset.global_offset_seconds = 0.02;

    const blaze4k::GameplayOptions options = blaze4k::gameplay_options_from_config(config);
    TEST_CHECK(options.speed.type == blaze4k::SpeedModType::CMod);
    TEST_CHECK(options.speed.value == 400.0);
    TEST_CHECK(options.scroll == blaze4k::ScrollDirection::Down);
    TEST_CHECK(!options.fail_enabled);
    TEST_CHECK(options.assist_tick);
    TEST_CHECK(options.global_offset_seconds == 0.02);

    blaze4k::GameConfig invalid;
    invalid.gameplay.speed_mod = "zzz";
    const blaze4k::GameplayOptions fallback = blaze4k::gameplay_options_from_config(invalid);
    TEST_CHECK(fallback.speed.type == blaze4k::SpeedModType::XMod);
    TEST_CHECK(fallback.speed.value == 1.0);
    std::cout << "  - config -> gameplay options derivation ok.\n";
}

void test_confirm_handoff(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select,
                          blaze4k::GameplayScreen* gameplay, blaze4k::PlayRequest& request) {
    const blaze4k::Song* expected_song = select->selected_song();
    const blaze4k::Chart* expected_chart = select->selected_chart();
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

void test_selection_preserved_on_reenter(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
    // Highlight a non-first song so a reset would be observable.
    while (select->selected_song_index() != 1) {
        manager.update(kDt, {press(GameAction::Down)});
    }
    const int song_before = select->selected_song_index();
    const int chart_before = select->selected_chart_index();
    const std::string title_before = select->selected_song()->metadata.title;
    const std::string open_before = select->open_pack_name();
    TEST_CHECK(open_before == "Test Pack"); // the pack holding the highlighted song is open

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->open_pack_name() == open_before);

    TEST_CHECK(select->selected_song_index() == song_before);
    TEST_CHECK(select->selected_chart_index() == chart_before);
    TEST_CHECK(select->selected_song() != nullptr);
    TEST_CHECK(select->selected_song()->metadata.title == title_before);
    std::cout << "  - wheel selection (song + chart) preserved on re-enter ok.\n";
}

void test_gameplay_held_state(blaze4k::ScreenManager& manager, blaze4k::GameplayScreen* gameplay) {
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
    blaze4k::SongLibrary empty_library;
    blaze4k::GameConfig config;
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;

    auto select = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select_ptr = select.get();
    blaze4k::ScreenManager manager(0.0);
    manager.add_screen(std::move(select));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &empty_library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);

    TEST_CHECK(select_ptr->song_count() == 0);
    TEST_CHECK(select_ptr->wheel_row_count() == 0 && !select_ptr->on_pack_header());
    TEST_CHECK(select_ptr->confirm_hint() == blaze4k::select_art::ConfirmHint::Play);
    TEST_CHECK(select_ptr->cursor_banner_path().empty());
    manager.update(kDt, {press(GameAction::Confirm)}); // nothing to open or play
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

    blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
    manager.render(renderer, 1280, 720);

    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(request.song == nullptr && request.chart == nullptr);
    std::cout << "  - empty library is crash-free and inert ok.\n";
}

// Titles with punctuation, UTF-8 (+ translit), malformed bytes and an MSD-escaped
// colon go through the real SongLibrary -> SimfileParser -> SelectScreen::render
// path headlessly (#77). The parser must keep raw bytes; only drawing changes.
void test_special_character_titles() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_utf8_test";
    std::filesystem::remove_all(root);
    const std::filesystem::path pack = root / "UTF8 Pack";

    const struct {
        const char* dir;
        std::string title;
        std::string extra_tags;
    } songs[] = {
        {"Dont", "Don't Promise Me", ""},
        {"VerTex3", "VerTex\xC2\xB3", "#TITLETRANSLIT:VerTex^3;\n#ARTISTTRANSLIT:Smiley;\n"},
        {"Bad", "Bad\xC3\x28\xFF Title", ""},
        {"Glacier", "Glacier\\:Groove", ""},
    };
    for (const auto& song : songs) {
        const std::filesystem::path dir = pack / song.dir;
        write_file(dir / "audio.ogg", "fake audio");
        write_file(dir / (std::string(song.dir) + ".sm"),
                   make_sm(song.title, "", {{"Easy", 3}}, "YES", song.extra_tags), true);
    }

    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    TEST_CHECK(library.total_songs() == 4);

    bool found_vertex = false;
    bool found_dont = false;
    bool found_glacier = false;
    bool found_bad = false;
    for (const blaze4k::SongPack& song_pack : library.packs()) {
        for (const blaze4k::Song& song : song_pack.songs) {
            const blaze4k::SongMetadata& metadata = song.metadata;
            if (metadata.title == "VerTex\xC2\xB3") {
                found_vertex = true;
                TEST_CHECK(blaze4k::song_display_title(metadata) == "VerTex^3");
                TEST_CHECK(blaze4k::song_display_artist(metadata) == "Test Artist");
            } else if (metadata.title == "Don't Promise Me") {
                found_dont = true;
                TEST_CHECK(blaze4k::song_display_title(metadata) == "Don't Promise Me");
            } else if (metadata.title == "Glacier:Groove") {
                found_glacier = true;
                TEST_CHECK(blaze4k::song_display_title(metadata) == "Glacier:Groove");
            } else if (metadata.title == "Bad\xC3\x28\xFF Title") {
                found_bad = true;
                TEST_CHECK(blaze4k::song_display_title(metadata) == metadata.title);
            }
        }
    }
    TEST_CHECK(found_vertex && found_dont && found_glacier && found_bad);
    // Identity lookup still uses the raw native title.
    TEST_CHECK(library.find_song("UTF8 Pack", "VerTex\xC2\xB3") != nullptr);

    blaze4k::GameConfig config;
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;
    auto select = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select_ptr = select.get();
    blaze4k::ScreenManager manager(0.0);
    manager.add_screen(std::move(select));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);
    TEST_CHECK(select_ptr->song_count() == 4);
    manager.update(kDt, {press(GameAction::Confirm)}); // open the only pack

    blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
    manager.render(renderer, 1280, 720);
    // One lap of the rows: the pack header plus the four songs.
    int songs_seen = 0;
    for (int i = 0; i < 5; ++i) {
        manager.update(kDt, {press(GameAction::Down)});
        if (select_ptr->selected_song() != nullptr) {
            ++songs_seen;
        } else {
            TEST_CHECK(select_ptr->on_pack_header());
        }
        manager.render(renderer, 1280, 720);
        manager.render(renderer, 640, 480);
    }
    TEST_CHECK(songs_seen == 4);
    std::filesystem::remove_all(root);
    std::cout << "  - special-character / UTF-8 / malformed titles render crash-free ok.\n";
}

// #96: the overlay's cached value text matches the model for every row.
bool options_values_match(const blaze4k::SelectScreen* select) {
    for (int i = 0; i < blaze4k::kOptionsRowCount; ++i) {
        if (select->options_values()[static_cast<std::size_t>(i)] !=
            blaze4k::options_row_value_text(select->options_menu(), i)) {
            return false;
        }
    }
    return true;
}

// Drives the C4 overlay through the real ScreenManager + shared GameConfig,
// verifying open/adjust/close, wheel suspension, gameplay application, and the
// real C2 save/load persistence path.
void test_options_overlay(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select,
                          blaze4k::GameConfig& config, blaze4k::PlayRequest& request) {
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 1. Open; the wheel must be suspended while the overlay is up.
    manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(select->options_open());
    const int song_before = select->selected_song_index();
    manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->selected_song_index() == song_before);

    // 2. Adjust SpeedType -> CMOD, Speed value 450 -> 400, Scroll -> DOWN, Fail -> OFF.
    using blaze4k::OptionsRow;
    const auto value = [select](OptionsRow row) -> const std::string& {
        return select->options_values()[static_cast<std::size_t>(row)];
    };
    TEST_CHECK(options_values_match(select)); // filled on open
    manager.update(kDt, {press(GameAction::Up)});    // row back to SpeedType
    manager.update(kDt, {press(GameAction::Right)}); // XMOD -> CMOD
    TEST_CHECK(select->options_menu().speed_type == blaze4k::SpeedModType::CMod);
    TEST_CHECK(options_values_match(select));
    TEST_CHECK(value(OptionsRow::SpeedType) == "CMOD");
    manager.update(kDt, {press(GameAction::Down)});  // row: SpeedValue
    for (int i = 0; i < 5; ++i) {
        manager.update(kDt, {press(GameAction::Left)}); // 450 -> 400
        TEST_CHECK(options_values_match(select));
    }
    TEST_CHECK(config.gameplay.speed_mod == "C400");
    TEST_CHECK(value(OptionsRow::SpeedValue) == "C400");

    manager.update(kDt, {press(GameAction::Down)});  // row: Scroll
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(config.gameplay.scroll == "down");
    TEST_CHECK(options_values_match(select));
    TEST_CHECK(value(OptionsRow::Scroll) == "DOWN");

    manager.update(kDt, {press(GameAction::Down)});  // row: Fail
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(!config.gameplay.fail_enabled);
    TEST_CHECK(options_values_match(select));
    TEST_CHECK(value(OptionsRow::Fail) == "OFF");

    // 3. Back closes the overlay without leaving Select; a second Back navigates.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select->options_open());
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 4. Confirm publishes the changed options to gameplay (AC2).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(request.options.speed.type == blaze4k::SpeedModType::CMod);
    TEST_CHECK(request.options.speed.value == 400.0);
    TEST_CHECK(request.options.scroll == blaze4k::ScrollDirection::Down);
    TEST_CHECK(!request.options.fail_enabled);

    manager.update(kDt, {press(GameAction::Back)}); // abort gameplay -> Select
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // 5. Persist through the real C2 path and confirm the fields survive.
    const std::filesystem::path config_path =
        std::filesystem::temp_directory_path() / "blaze4k_select_options_config.json";
    TEST_CHECK(blaze4k::save_config(config_path, config));
    const blaze4k::GameConfig reloaded = blaze4k::load_config(config_path);
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
void test_calibration_launch_from_options(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select,
                                          blaze4k::GameConfig& config) {
    manager.add_screen(std::make_unique<blaze4k::CalibrationScreen>());
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    const auto move_to_calibrate_row = [&] {
        for (int i = 0; i < static_cast<int>(blaze4k::OptionsRow::CalibrateOffset); ++i) {
            manager.update(kDt, {press(GameAction::Down)});
        }
        TEST_CHECK(select->options_menu().row ==
                   static_cast<int>(blaze4k::OptionsRow::CalibrateOffset));
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
    TEST_CHECK(select->options_menu().row == static_cast<int>(blaze4k::OptionsRow::CalibrateOffset));
    // #96: the reopened overlay's cached value text shows the offset on the first frame back.
    const auto offset_text = [select] {
        return select->options_values()[static_cast<std::size_t>(
            blaze4k::OptionsRow::CalibrateOffset)];
    };
    TEST_CHECK(offset_text() == blaze4k::format_offset(config.offset.global_offset_seconds));
    TEST_CHECK(options_values_match(select));

    // Right launches it as well, straight from the reopened overlay.
    manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(manager.active_id() == ScreenId::Calibration);
    // #96: stand in for an accepted calibration (the wizard writes the config
    // offset); the reopened overlay must show the fresh value.
    config.offset.global_offset_seconds = -0.011;
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->options_open());
    TEST_CHECK(offset_text() == blaze4k::format_offset(config.offset.global_offset_seconds));
    TEST_CHECK(offset_text() == "-0.011 s");
    config.offset.global_offset_seconds = offset_before;

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
void test_remap_launch_from_options(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select,
                                    blaze4k::GameConfig& config) {
    manager.add_screen(std::make_unique<blaze4k::InputRemapScreen>());
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    const auto move_to_remap_row = [&] {
        for (int i = 0; i < static_cast<int>(blaze4k::OptionsRow::RemapInput); ++i) {
            manager.update(kDt, {press(GameAction::Down)});
        }
        TEST_CHECK(select->options_menu().row == static_cast<int>(blaze4k::OptionsRow::RemapInput));
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
    TEST_CHECK(select->options_menu().row == static_cast<int>(blaze4k::OptionsRow::RemapInput));

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
void test_same_tick_options_back(blaze4k::ScreenManager& manager, blaze4k::SelectScreen* select) {
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

// 12 songs in two 6-song packs: 8 wheel rows with one pack open (its header, its 6 songs,
// the other pack's header), 2 with both closed.
blaze4k::SongLibrary make_slide_library(const std::filesystem::path& root) {
    std::filesystem::remove_all(root);
    for (const char* pack : {"Pack One", "Pack Two"}) {
        for (int i = 1; i <= 6; ++i) {
            const std::string name = std::string(pack) + " Song " + std::to_string(i);
            const std::filesystem::path dir = root / pack / name;
            write_file(dir / "audio.ogg", "fake audio");
            write_file(dir / (name + ".sm"), make_sm(name, "", {{"Easy", 3}}));
        }
    }
    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    TEST_CHECK(library.total_songs() == 12);
    return library;
}

// Index of the pack holding `song` in `library`, or -1.
int pack_of(const blaze4k::SongLibrary& library, const blaze4k::Song* song) {
    const std::vector<blaze4k::SongPack>& packs = library.packs();
    for (std::size_t p = 0; p < packs.size(); ++p) {
        for (const blaze4k::Song& candidate : packs[p].songs) {
            if (&candidate == song) {
                return static_cast<int>(p);
            }
        }
    }
    return -1;
}

struct SlideFixture {
    blaze4k::SongLibrary library;
    blaze4k::GameConfig config;
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;
    blaze4k::ScreenManager manager{0.0};
    blaze4k::SelectScreen* select = nullptr;

    // `open_first` presses Confirm on the first header (it opens that pack) and Down onto
    // its first song: the state most tests start from. False keeps the first-entry state
    // (every pack closed, the cursor on the first header).
    explicit SlideFixture(const std::filesystem::path& root, bool open_first = true)
        : library(make_slide_library(root)) {
        auto screen = std::make_unique<blaze4k::SelectScreen>();
        select = screen.get();
        manager.add_screen(std::move(screen));
        manager.context().config = &config;
        manager.context().scores = &scores;
        manager.context().library = &library;
        manager.context().play_request = &request;
        manager.start(ScreenId::Select);
        if (open_first) {
            manager.update(kDt, {press(GameAction::Confirm)});
            manager.update(kDt, {press(GameAction::Down)});
        }
    }
};

// First entry with two packs: both closed (two header rows), the cursor on the first
// header, the hint reads OPEN; the hint follows Confirm's meaning as the cursor and the
// toggles move.
void test_first_entry_two_packs() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_first_entry_test";
    SlideFixture fx(root, /*open_first=*/false);
    blaze4k::SelectScreen* select = fx.select;
    using blaze4k::select_art::ConfirmHint;
    TEST_CHECK(select->song_count() == 12);
    TEST_CHECK(select->open_pack_name().empty());
    TEST_CHECK(select->wheel_row_count() == 2);
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->selected_pack_name() == fx.library.packs().front().name);
    TEST_CHECK(select->selected_song() == nullptr);
    TEST_CHECK(select->preview().requested_path().empty());
    TEST_CHECK(select->confirm_hint() == ConfirmHint::Open);
    // No pack banner in this library and no fallback set: empty path, theme fallback drawn.
    TEST_CHECK(select->cursor_banner_path().empty());

    fx.manager.update(kDt, {press(GameAction::Confirm)}); // open the first pack
    TEST_CHECK(select->wheel_row_count() == 8 && select->confirm_hint() == ConfirmHint::Close);
    fx.manager.update(kDt, {press(GameAction::Down)}); // song row
    TEST_CHECK(select->confirm_hint() == ConfirmHint::Play);
    for (int i = 0; i < 6; ++i) {
        fx.manager.update(kDt, {press(GameAction::Down)}); // S1..S5, then the second header
    }
    // The second pack's header is closed while the first is open: OPEN again.
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 7);
    TEST_CHECK(select->open_pack_name() != select->selected_pack_name());
    TEST_CHECK(select->confirm_hint() == ConfirmHint::Open);
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)});
    fx.manager.update(kDt, {press(GameAction::Up)}); // S0 -> header (row 0)
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->confirm_hint() == ConfirmHint::Close);
    std::filesystem::remove_all(root);
    std::cout << "  - first entry with two packs + Confirm hint follows the cursor ok.\n";
}

// #94: the wheel slides (eased, 80 ms, fixed dt) only when its window moves.
void test_wheel_slide() {
    constexpr double kSlideDt = 1.0 / 60.0; // the main fixture's 0.1 s outlasts the slide
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_slide_test";
    SlideFixture fx(root);
    blaze4k::ScreenManager& manager = fx.manager;
    blaze4k::SelectScreen* select = fx.select;
    const auto down = [&] { manager.update(kSlideDt, {press(GameAction::Down)}); };
    const auto up = [&] { manager.update(kSlideDt, {press(GameAction::Up)}); };
    const auto idle = [&](int ticks) {
        for (int i = 0; i < ticks; ++i) {
            manager.update(kSlideDt, {});
        }
    };

    // The first pack is open: rows H0, S0..S5, H1 = 8; the cursor is on S0 (row 1).
    // With 7 visible rows the window is rows 0..6 (first 0) while the cursor is on
    // rows 0..3 and rows 1..7 (first 1) from row 4 on.
    TEST_CHECK(select->song_count() == 12);
    TEST_CHECK(select->wheel_row_count() == 8);
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->selected_wheel_row() == 1);
    TEST_CHECK(select->wheel_first_row() == 0);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);

    // Rows 1 -> 2 -> 3: the window stays at the top, nothing slides.
    down();
    TEST_CHECK(select->wheel_first_row() == 0 && select->wheel_scroll_offset() == 0.0f);
    down();
    TEST_CHECK(select->wheel_first_row() == 0 && select->wheel_scroll_offset() == 0.0f);

    // Row 3 -> 4: the window moves down one row; the rows start one pitch lower
    // and ease up to rest within 80 ms (5 ticks = 83 ms).
    down();
    TEST_CHECK(select->wheel_first_row() == 1);
    TEST_CHECK(select->wheel_scroll_offset() == blaze4k::select_art::kWheelPitch);
    idle(1);
    const float after_one = select->wheel_scroll_offset();
    TEST_CHECK(after_one > 0.0f && after_one < blaze4k::select_art::kWheelPitch);
    idle(4);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);

    // Up mirrors it.
    up();
    TEST_CHECK(select->wheel_first_row() == 0);
    TEST_CHECK(select->wheel_scroll_offset() == -blaze4k::select_art::kWheelPitch);
    idle(5);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);

    // Back to the first song (row 1), then onto the pack header (row 0): the window
    // stays at the top, so nothing slides.
    up();
    up();
    TEST_CHECK(select->selected_song_index() == 0);
    idle(5);
    up();
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->wheel_first_row() == 0 && select->wheel_scroll_offset() == 0.0f);
    // Wrapping up from row 0 lands on the other pack's header (row 7). On this short
    // list the window only moves one row (first 0 -> 1), so it slides one pitch.
    up();
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 7);
    TEST_CHECK(select->wheel_first_row() == 1);
    TEST_CHECK(select->wheel_scroll_offset() == blaze4k::select_art::kWheelPitch);
    idle(5);
    down();
    TEST_CHECK(select->selected_wheel_row() == 0 && select->wheel_first_row() == 0);
    TEST_CHECK(select->wheel_scroll_offset() == -blaze4k::select_art::kWheelPitch);
    idle(5);
    down();
    TEST_CHECK(select->selected_song_index() == 0);
    TEST_CHECK(select->wheel_first_row() == 0);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);

    // Held repeat (accelerating to 40 ms steps, faster than the slide) never
    // lets the offset run past two rows, and it settles once released.
    GameAction held = GameAction::Down;
    manager.context().action_down = [&held](GameAction action) { return action == held; };
    down();
    float max_offset = 0.0f;
    for (int i = 0; i < 120; ++i) {
        idle(1);
        max_offset = std::max(max_offset, std::fabs(select->wheel_scroll_offset()));
        TEST_CHECK(std::fabs(select->wheel_scroll_offset()) <=
                   blaze4k::select_art::kWheelScrollMax);
    }
    TEST_CHECK(max_offset > 0.0f); // it did slide while repeating
    held = GameAction::None;
    idle(6);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);
    manager.context().action_down = nullptr;

    // Re-entering resets the slide.
    while (select->selected_song_index() != 2) {
        down();
    }
    idle(6);
    down(); // row 3 -> 4 moves the window
    TEST_CHECK(select->wheel_scroll_offset() != 0.0f);
    manager.start(ScreenId::Select);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);

    std::filesystem::remove_all(root);
    std::cout << "  - wheel slide: window moves only, 80 ms ease, wrap snaps, repeat clamp ok.\n";
}

// One pack of `songs` selectable songs (and `hidden` SELECTABLE:NO songs) under `root`.
blaze4k::SongLibrary make_pack_library(const std::filesystem::path& root, const std::string& pack,
                                       int songs, int hidden = 0) {
    std::filesystem::remove_all(root);
    for (int i = 1; i <= songs + hidden; ++i) {
        const std::string name = pack + " Song " + std::to_string(i);
        const std::filesystem::path dir = root / pack / name;
        write_file(dir / "audio.ogg", "fake audio");
        write_file(dir / (name + ".sm"),
                   make_sm(name, "", {{"Easy", 3}}, i <= songs ? "YES" : "NO"));
    }
    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));
    return library;
}

// The name of the pack in `library` that is not `name`.
std::string other_pack(const blaze4k::SongLibrary& library, const std::string& name) {
    for (const blaze4k::SongPack& pack : library.packs()) {
        if (pack.name != name) {
            return pack.name;
        }
    }
    return {};
}

// #166: the cursor stops on pack headers; Confirm on one closes an open pack or opens a
// closed one without leaving Select, and a closed pack's songs are not reachable.
void test_pack_header_toggle() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_toggle_test";
    SlideFixture fx(root);
    blaze4k::SelectScreen* select = fx.select;
    const std::string first = fx.library.packs().front().name;

    // First entry: the first pack is open with its first song highlighted.
    TEST_CHECK(select->open_pack_name() == first);
    TEST_CHECK(select->selected_song_index() == 0 && !select->on_pack_header());

    // Up from the first song: the cursor is on the open pack's header, which has no song.
    fx.manager.update(kDt, {press(GameAction::Up)});
    TEST_CHECK(select->on_pack_header());
    TEST_CHECK(select->selected_pack_name() == first);
    TEST_CHECK(select->selected_song() == nullptr && select->selected_song_index() == -1);
    TEST_CHECK(select->chart_count() == 0 && select->selected_chart() == nullptr);
    TEST_CHECK(select->wheel_row_count() == 8);

    // Confirm closes it: only the two headers stay, the cursor stays on the header,
    // the screen does not change and nothing is published to Gameplay.
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name().empty());
    TEST_CHECK(select->wheel_row_count() == 2);
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == first);
    TEST_CHECK(select->selected_wheel_row() == 0);
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.request.song == nullptr && fx.request.chart == nullptr);
    TEST_CHECK(select->preview().requested_path().empty()); // no song on a header: no preview

    // Left/Right on a header do nothing.
    fx.manager.update(kDt, {press(GameAction::Right)});
    fx.manager.update(kDt, {press(GameAction::Left)});
    TEST_CHECK(select->selected_chart_index() == 0 && select->on_pack_header());

    // Both closed: Down and Up only visit the two headers, never a hidden song.
    const std::string second = other_pack(fx.library, first);
    fx.manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == second);
    TEST_CHECK(select->selected_wheel_row() == 1 && select->selected_song_index() == -1);
    fx.manager.update(kDt, {press(GameAction::Down)});
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == first);
    fx.manager.update(kDt, {press(GameAction::Up)});
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == second);
    TEST_CHECK(select->selected_song_index() == -1);

    // Confirm on the second header opens it: the cursor stays on that header, and the
    // first pack stays closed (one open pack at a time).
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name() == second);
    TEST_CHECK(select->wheel_row_count() == 8);
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == second);
    TEST_CHECK(select->selected_wheel_row() == 1);
    int songs_seen = 0;
    int headers_seen = 0;
    for (int i = 0; i < 8; ++i) {
        fx.manager.update(kDt, {press(GameAction::Down)});
        if (select->on_pack_header()) {
            ++headers_seen;
        } else {
            ++songs_seen;
            TEST_CHECK(select->selected_song() != nullptr);
            TEST_CHECK(
                fx.library
                    .packs()[static_cast<std::size_t>(pack_of(fx.library, select->selected_song()))]
                    .name == second);
        }
    }
    TEST_CHECK(headers_seen == 2 && songs_seen == 6);

    // Opening the other pack while this one is open closes this one.
    while (!(select->on_pack_header() && select->selected_pack_name() == first)) {
        fx.manager.update(kDt, {press(GameAction::Down)});
    }
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name() == first && select->wheel_row_count() == 8);
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == first);

    // A single pack closes and reopens, and the cursor index stays inside the rows.
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name().empty() && select->wheel_row_count() == 2);
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name() == first && select->wheel_row_count() == 8);
    TEST_CHECK(select->selected_wheel_row() < static_cast<int>(select->wheel_row_count()));

    std::filesystem::remove_all(root);
    std::cout
        << "  - Confirm on a pack header opens/closes it; one pack open; no hidden stops ok.\n";
}

// A toggle snaps the wheel: the slide offset is 0 afterwards.
void test_pack_toggle_snaps_slide() {
    constexpr double kSlideDt = 1.0 / 60.0;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_toggle_slide_test";
    SlideFixture fx(root);
    blaze4k::SelectScreen* select = fx.select;
    const auto step = [&](GameAction action) {
        fx.manager.update(kSlideDt, {press(action)});
    };
    // Row 1 -> 4 (window slides), then back up to the header at row 0 mid-slide.
    step(GameAction::Down);
    step(GameAction::Down);
    step(GameAction::Down);
    step(GameAction::Up);
    step(GameAction::Up);
    step(GameAction::Up);
    step(GameAction::Up);
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->wheel_scroll_offset() != 0.0f);
    step(GameAction::Confirm);
    TEST_CHECK(select->wheel_row_count() == 2);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f && select->wheel_first_row() == 0);
    std::filesystem::remove_all(root);
    std::cout << "  - a pack toggle snaps the wheel (no slide) ok.\n";
}

// A wrap across a long list jumps the window, so the offset snaps to 0.
void test_wheel_wrap_snaps() {
    constexpr double kSlideDt = 1.0 / 60.0;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_wrap_test";
    blaze4k::SongLibrary library = make_pack_library(root, "Long Pack", 10);
    blaze4k::GameConfig config;
    blaze4k::PlayRequest request;
    blaze4k::ScreenManager manager(0.0);
    auto screen = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select = screen.get();
    manager.add_screen(std::move(screen));
    manager.context().config = &config;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);
    // First entry of a single pack: only its header, closed, with the cursor on it.
    TEST_CHECK(select->wheel_row_count() == 1 && select->on_pack_header());
    TEST_CHECK(select->open_pack_name().empty());
    manager.update(kDt, {press(GameAction::Confirm)});
    manager.update(kDt, {press(GameAction::Down)});

    // 1 header + 10 songs = 11 rows; the cursor is on row 1 (first 0). Up to the
    // header (row 0), then up again wraps to row 10: first = clamp(10 - 3, 0, 4) = 4.
    TEST_CHECK(select->wheel_row_count() == 11 && select->selected_wheel_row() == 1);
    manager.update(kSlideDt, {press(GameAction::Up)});
    TEST_CHECK(select->on_pack_header() && select->wheel_scroll_offset() == 0.0f);
    manager.update(kSlideDt, {press(GameAction::Up)});
    TEST_CHECK(select->selected_wheel_row() == 10 && select->selected_song_index() == 9);
    TEST_CHECK(select->wheel_first_row() == 4);
    TEST_CHECK(select->wheel_scroll_offset() == 0.0f);
    manager.update(kSlideDt, {press(GameAction::Down)});
    TEST_CHECK(select->on_pack_header() && select->selected_wheel_row() == 0);
    TEST_CHECK(select->wheel_first_row() == 0 && select->wheel_scroll_offset() == 0.0f);
    std::filesystem::remove_all(root);
    std::cout << "  - wheel wrap across a long list snaps ok.\n";
}

// A library whose songs are all SELECTABLE:NO has no rows at all.
void test_all_hidden_library() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_hidden_test";
    blaze4k::SongLibrary library = make_pack_library(root, "Hidden Pack", 0, 2);
    blaze4k::GameConfig config;
    blaze4k::PlayRequest request;
    blaze4k::ScreenManager manager(0.0);
    auto screen = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select = screen.get();
    manager.add_screen(std::move(screen));
    manager.context().config = &config;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);
    TEST_CHECK(select->song_count() == 0 && select->wheel_row_count() == 0);
    TEST_CHECK(!select->on_pack_header() && select->selected_song() == nullptr);
    for (const GameAction action :
         {GameAction::Down, GameAction::Up, GameAction::Confirm, GameAction::Right}) {
        manager.update(kDt, {press(action)});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->wheel_row_count() == 0 && request.song == nullptr);
    std::filesystem::remove_all(root);
    std::cout << "  - a library with no selectable songs has no rows ok.\n";
}

// The cursor target and the open pack survive leaving Select and coming back.
void test_pack_state_survives_reenter() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_reenter_test";
    SlideFixture fx(root);
    blaze4k::SelectScreen* select = fx.select;
    fx.manager.add_screen(std::make_unique<blaze4k::TitleScreen>());
    fx.manager.add_screen(std::make_unique<blaze4k::GameplayScreen>());
    fx.manager.add_screen(std::make_unique<blaze4k::CalibrationScreen>());
    fx.manager.add_screen(std::make_unique<blaze4k::InputRemapScreen>());
    const std::string first = fx.library.packs().front().name;
    const std::string second = other_pack(fx.library, first);

    // Open the second pack and highlight its second song (a Pack-Two row, not row 0).
    while (!(select->on_pack_header() && select->selected_pack_name() == second)) {
        fx.manager.update(kDt, {press(GameAction::Down)});
    }
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(select->open_pack_name() == second);
    fx.manager.update(kDt, {press(GameAction::Down)});
    fx.manager.update(kDt, {press(GameAction::Down)});
    const blaze4k::Song* song = select->selected_song();
    TEST_CHECK(song != nullptr);
    TEST_CHECK(fx.library.packs()[static_cast<std::size_t>(pack_of(fx.library, song))].name ==
               second);
    const int song_index = select->selected_song_index();
    const int row = select->selected_wheel_row();
    const auto unchanged = [&] {
        TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
        TEST_CHECK(select->selected_song() == song);
        TEST_CHECK(select->selected_song_index() == song_index);
        TEST_CHECK(select->selected_wheel_row() == row);
        TEST_CHECK(select->open_pack_name() == second);
        TEST_CHECK(select->wheel_row_count() == 8);
    };

    // Gameplay and back.
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Gameplay);
    fx.manager.update(kDt, {press(GameAction::Back)});
    unchanged();

    // Calibration and Input remap, both launched from the options overlay.
    for (const blaze4k::OptionsRow launch :
         {blaze4k::OptionsRow::CalibrateOffset, blaze4k::OptionsRow::RemapInput}) {
        fx.manager.update(kDt, {press(GameAction::Options)});
        TEST_CHECK(select->options_open());
        while (select->options_menu().row != static_cast<int>(launch)) {
            fx.manager.update(kDt, {press(GameAction::Down)});
        }
        fx.manager.update(kDt, {press(GameAction::Confirm)});
        TEST_CHECK(fx.manager.active_id() != ScreenId::Select);
        fx.manager.update(kDt, {press(GameAction::Back)});
        TEST_CHECK(select->options_open());
        fx.manager.update(kDt, {press(GameAction::Back)}); // close the overlay
        TEST_CHECK(!select->options_open());
        unchanged();
    }

    // A pack header persists too, and so does an open pack that is not the header's.
    for (int i = 0; i < 3; ++i) {
        fx.manager.update(
            kDt, {press(GameAction::Up)}); // row 3 -> row 0: the first pack's (closed) header
    }
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == first);
    fx.manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Title);
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == first);
    TEST_CHECK(select->selected_wheel_row() == 0);
    TEST_CHECK(select->open_pack_name() == second && select->wheel_row_count() == 8);

    // A remembered target the library no longer has falls back to the first entry:
    // every pack closed, the cursor on the first header.
    const std::filesystem::path other_root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_reenter_other_test";
    blaze4k::SongLibrary other = make_pack_library(other_root, "Elsewhere", 3);
    fx.manager.context().library = &other;
    fx.manager.start(ScreenId::Select);
    TEST_CHECK(select->open_pack_name().empty());
    TEST_CHECK(select->on_pack_header() && select->selected_pack_name() == "Elsewhere");
    TEST_CHECK(select->selected_wheel_row() == 0 && select->wheel_row_count() == 1);

    std::filesystem::remove_all(root);
    std::filesystem::remove_all(other_root);
    std::cout
        << "  - open pack + cursor (song or header) survive Gameplay/Calibration/Remap/Title ok.\n";
}

// Render smoke with the real headless theme + text on a header (open and closed) and a song.
void test_pack_header_render() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_header_render_test";
    SlideFixture fx(root);
    blaze4k::SelectScreen* select = fx.select;
    blaze4k::TextRenderer& text = loaded_text();
    fx.manager.context().theme = &loaded_theme();
    fx.manager.context().text = &text;
    blaze4k::GlQuadRenderer renderer;
    const auto render = [&] {
        for (const auto& [w, h] :
             {std::pair{1280, 720}, std::pair{640, 480}, std::pair{3440, 1440}}) {
            text.set_window_size(w, h);
            fx.manager.render(renderer, w, h);
        }
        text.set_window_size(1280, 720);
    };
    render();                                           // on a song
    fx.manager.update(kDt, {press(GameAction::Up)});    // on the open header
    TEST_CHECK(select->on_pack_header());
    render();
    fx.manager.update(kDt, {press(GameAction::Confirm)}); // on a closed header
    TEST_CHECK(select->on_pack_header() && select->wheel_row_count() == 2);
    render();
    fx.manager.update(kDt, {press(GameAction::Down)});
    render();
    for (int i = 0; i < 16; ++i) {
        fx.manager.update(1.0 / 60.0, {press(GameAction::Down)});
        render();
    }
    std::filesystem::remove_all(root);
    std::cout << "  - headers (open, closed, selected) render crash-free ok.\n";
}

} // namespace

int main() {
    std::cout << "[select_screen_test] Running SelectScreen tests...\n";

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_select_screen_test";
    blaze4k::SongLibrary library = make_library(root);
    TEST_CHECK(library.total_songs() == 4);

    blaze4k::GameConfig config;
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;

    auto select = std::make_unique<blaze4k::SelectScreen>();
    blaze4k::SelectScreen* select_ptr = select.get();
    auto gameplay = std::make_unique<blaze4k::GameplayScreen>();
    blaze4k::GameplayScreen* gameplay_ptr = gameplay.get();

    blaze4k::ScreenManager manager(0.0); // disable idle-attract for determinism
    manager.add_screen(std::make_unique<blaze4k::TitleScreen>());
    manager.add_screen(std::move(select));
    manager.add_screen(std::move(gameplay));
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().library = &library;
    manager.context().play_request = &request;
    manager.start(ScreenId::Select);

    test_first_entry(manager, select_ptr);
    test_wheel_load(select_ptr);
    test_song_navigation(manager, select_ptr);
    test_difficulty_navigation(manager, select_ptr);
    test_held_navigation_repeat(manager, select_ptr);
    test_best_score(manager, select_ptr, scores);
    test_named_edit_charts();
    test_bpm_formatting();
    test_options_derivation();
    test_confirm_handoff(manager, select_ptr, gameplay_ptr, request);
    test_selection_preserved_on_reenter(manager, select_ptr);
    test_gameplay_held_state(manager, gameplay_ptr);
    test_empty_library();
    test_special_character_titles();
    test_options_overlay(manager, select_ptr, config, request);
    test_calibration_launch_from_options(manager, select_ptr, config);
    test_remap_launch_from_options(manager, select_ptr, config);
    test_same_tick_options_back(manager, select_ptr);
    test_wheel_slide();
    test_pack_header_toggle();
    test_pack_toggle_snaps_slide();
    test_wheel_wrap_snaps();
    test_all_hidden_library();
    test_pack_state_survives_reenter();
    test_pack_header_render();
    test_first_entry_two_packs();
    test_header_banner();

    blaze4k::GlQuadRenderer renderer; // populated-screen render smoke
    manager.render(renderer, 1280, 720);

    // The same populated screen with the real (headless) theme + text services,
    // with and without the options overlay, at several window sizes.
    blaze4k::TextRenderer& text = loaded_text();
    manager.context().theme = &loaded_theme();
    manager.context().text = &text;
    manager.start(ScreenId::Select);
    for (const bool overlay : {false, true}) {
        if (overlay) {
            manager.update(kDt, {press(GameAction::Options)});
            TEST_CHECK(select_ptr->options_open());
        }
        for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{2560, 1440},
                                   std::pair{3440, 1440}, std::pair{1920, 1200},
                                   std::pair{640, 480}}) {
            text.set_window_size(w, h);
            manager.render(renderer, w, h);
        }
    }
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(!select_ptr->options_open());
    text.set_window_size(1280, 720);

    std::filesystem::remove_all(root);

    std::cout << "[select_screen_test] All tests passed!\n";
    return 0;
}
