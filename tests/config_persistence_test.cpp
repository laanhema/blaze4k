#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "data/config.hpp"
#include "data/config_loader.hpp"
#include "data/data_paths.hpp"
#include "data/high_scores.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

constexpr double kEps = 1e-12;

bool nearly(double a, double b) {
    return std::fabs(a - b) <= kEps;
}

void write_file(const fs::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::map<std::string, std::vector<std::string>> to_map(
    const std::vector<td::InputBinding>& bindings) {
    std::map<std::string, std::vector<std::string>> result;
    for (const td::InputBinding& binding : bindings) {
        result[binding.first] = binding.second;
    }
    return result;
}

} // namespace

int main() {
    std::cout << "[config_persistence_test] Running config/high-score persistence tests...\n";

    const fs::path temp_dir = fs::temp_directory_path() / "td_config_persistence_test";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string message;
    td::ConfigLoadStatus status = td::ConfigLoadStatus::UsedDefaults;
    td::ScoresLoadStatus scores_status = td::ScoresLoadStatus::UsedDefaults;

    // 1. Documented defaults + pure validation.
    {
        td::GameConfig defaults;
        TEST_CHECK(defaults.version == td::kConfigVersion);
        TEST_CHECK(defaults.video.width == 1280);
        TEST_CHECK(defaults.video.height == 720);
        TEST_CHECK(defaults.video.vsync);
        TEST_CHECK(!defaults.video.fullscreen);
        TEST_CHECK(nearly(defaults.audio.master_volume, 1.0));
        TEST_CHECK(nearly(defaults.audio.music_volume, 1.0));
        TEST_CHECK(nearly(defaults.audio.preview_volume, 0.8));
        TEST_CHECK(nearly(defaults.audio.ui_volume, 1.0));
        TEST_CHECK(nearly(defaults.offset.global_offset_seconds, 0.0));
        TEST_CHECK(defaults.gameplay.speed_mod == "1x");
        TEST_CHECK(defaults.gameplay.scroll == "up");
        TEST_CHECK(defaults.gameplay.fail_enabled);
        TEST_CHECK(!defaults.gameplay.assist_tick);
        TEST_CHECK(defaults.input.key_bindings.size() == 7);
        TEST_CHECK(defaults.input.gamepad_bindings.size() == 7);

        // C6: the persisted defaults are the single binding authority and now
        // carry the full runtime set (Tab -> Options; pad-Back -> Back).
        const auto default_keys = to_map(td::default_key_bindings());
        TEST_CHECK(default_keys.count("Options") == 1);
        TEST_CHECK(default_keys.at("Options") == std::vector<std::string>{"Tab"});
        const auto default_pads = to_map(td::default_gamepad_bindings());
        TEST_CHECK(default_pads.count("Back") == 1);
        TEST_CHECK(default_pads.at("Back") == std::vector<std::string>{"back"});

        std::string error;
        TEST_CHECK(td::validate_game_config(defaults, &error));

        td::GameConfig bad = defaults;
        bad.audio.master_volume = 1.5;
        TEST_CHECK(!td::validate_game_config(bad, &error));
        bad = defaults;
        bad.video.width = 100;
        TEST_CHECK(!td::validate_game_config(bad, &error));
        bad = defaults;
        bad.gameplay.scroll = "sideways";
        TEST_CHECK(!td::validate_game_config(bad, &error));
        bad = defaults;
        bad.gameplay.speed_mod.clear();
        TEST_CHECK(!td::validate_game_config(bad, &error));
        std::cout << "  - 1. defaults and validation ok.\n";
    }

    // 2. Full round-trip (including both binding maps and nested dir creation).
    {
        const fs::path config_path = temp_dir / "roundtrip" / "config.json";
        td::GameConfig config;
        config.video.width = 1024;
        config.video.height = 600;
        config.video.vsync = false;
        config.video.fullscreen = true;
        config.audio.master_volume = 0.5;
        config.audio.music_volume = 0.25;
        config.audio.preview_volume = 0.0;
        config.audio.ui_volume = 0.75;
        config.offset.global_offset_seconds = -0.123;
        config.gameplay.speed_mod = "C400";
        config.gameplay.scroll = "down";
        config.gameplay.fail_enabled = false;
        config.gameplay.assist_tick = true;
        config.input.key_bindings = {{"Left", {"A", "Left"}}, {"Confirm", {"Space"}}};
        config.input.gamepad_bindings = {{"Confirm", {"South", "North"}}};

        TEST_CHECK(td::save_config(config_path, config, &message));
        TEST_CHECK(fs::exists(config_path));
        TEST_CHECK(!fs::exists(fs::path(config_path.string() + ".tmp")));

        td::GameConfig loaded = td::load_config(config_path, &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::LoadedFromFile);
        TEST_CHECK(loaded.video.width == 1024);
        TEST_CHECK(loaded.video.height == 600);
        TEST_CHECK(!loaded.video.vsync);
        TEST_CHECK(loaded.video.fullscreen);
        TEST_CHECK(nearly(loaded.audio.master_volume, 0.5));
        TEST_CHECK(nearly(loaded.audio.music_volume, 0.25));
        TEST_CHECK(nearly(loaded.audio.preview_volume, 0.0));
        TEST_CHECK(nearly(loaded.audio.ui_volume, 0.75));
        TEST_CHECK(nearly(loaded.offset.global_offset_seconds, -0.123));
        TEST_CHECK(loaded.gameplay.speed_mod == "C400");
        TEST_CHECK(loaded.gameplay.scroll == "down");
        TEST_CHECK(!loaded.gameplay.fail_enabled);
        TEST_CHECK(loaded.gameplay.assist_tick);
        TEST_CHECK(to_map(loaded.input.key_bindings) == to_map(config.input.key_bindings));
        TEST_CHECK(to_map(loaded.input.gamepad_bindings) == to_map(config.input.gamepad_bindings));
        std::cout << "  - 2. config round-trip ok.\n";
    }

    // 3. Missing file -> defaults + warning.
    {
        td::GameConfig missing = td::load_config(temp_dir / "does_not_exist.json", &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::UsedDefaults);
        TEST_CHECK(missing.video.width == 1280);
        TEST_CHECK(message.find("[Config]") != std::string::npos);
        std::cout << "  - 3. missing config falls back: " << message << "\n";
    }

    // 4. Corrupt JSON -> defaults + warning, never a crash.
    {
        const fs::path corrupt = temp_dir / "corrupt.json";
        write_file(corrupt, "{ not json");
        td::GameConfig loaded = td::load_config(corrupt, &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::UsedDefaults);
        TEST_CHECK(nearly(loaded.audio.master_volume, 1.0));
        TEST_CHECK(message.find("[Config]") != std::string::npos);
        std::cout << "  - 4. corrupt config falls back with warning.\n";
    }

    // 5. Per-field tolerance: one bad field falls back while valid fields load,
    //    and out-of-range numerics are clamped.
    {
        const fs::path tolerant = temp_dir / "tolerant.json";
        write_file(tolerant,
                   R"({"video":{"width":"wide","height":600},"audio":{"master_volume":0.25,"music_volume":5.0}})");
        td::GameConfig loaded = td::load_config(tolerant, &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::LoadedFromFile);
        TEST_CHECK(loaded.video.width == 1280); // wrong type -> default
        TEST_CHECK(loaded.video.height == 600); // valid neighbour survives
        TEST_CHECK(nearly(loaded.audio.master_volume, 0.25));
        TEST_CHECK(nearly(loaded.audio.music_volume, 1.0)); // clamped
        TEST_CHECK(message.find("warning") != std::string::npos);
        std::cout << "  - 5. per-field tolerance + clamping ok.\n";
    }

    // 5b. Invalid gameplay enums fall back to defaults on load (and are not
    //     re-persisted), honoring the "invalid field keeps its default" contract.
    {
        const fs::path enums = temp_dir / "invalid_enums.json";
        write_file(enums, R"({"gameplay":{"speed_mod":"","scroll":"sideways","fail_enabled":false}})");
        td::GameConfig loaded = td::load_config(enums, &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::LoadedFromFile);
        TEST_CHECK(loaded.gameplay.speed_mod == "1x"); // empty -> default
        TEST_CHECK(loaded.gameplay.scroll == "up");    // invalid -> default
        TEST_CHECK(!loaded.gameplay.fail_enabled);     // valid neighbour survives
        TEST_CHECK(message.find("warning") != std::string::npos);
        TEST_CHECK(td::validate_game_config(loaded, nullptr));

        // Re-saving must persist the corrected defaults, not the bad values.
        const fs::path enums_out = temp_dir / "invalid_enums_out.json";
        TEST_CHECK(td::save_config(enums_out, loaded, &message));
        const std::string persisted = read_file(enums_out);
        TEST_CHECK(persisted.find("sideways") == std::string::npos);
        std::cout << "  - 5b. invalid enums fall back to defaults ok.\n";
    }

    // 6. Size cap.
    {
        const fs::path huge = temp_dir / "huge.json";
        write_file(huge, std::string((1u << 20) + 16, 'x'));
        td::GameConfig loaded = td::load_config(huge, &message, &status);
        TEST_CHECK(status == td::ConfigLoadStatus::UsedDefaults);
        TEST_CHECK(message.find("cap") != std::string::npos);
        std::cout << "  - 6. oversized config rejected by size cap.\n";
    }

    // 7. Atomic save: success leaves no temp file; a failed save leaves the prior
    //    file untouched and cleans up its temp file.
    {
        const fs::path keep = temp_dir / "keep.json";
        td::GameConfig keep_cfg;
        keep_cfg.video.width = 1024;
        TEST_CHECK(td::save_config(keep, keep_cfg, &message));
        const std::string before = read_file(keep);
        TEST_CHECK(!before.empty());

        const fs::path dir_target = temp_dir / "dir_target";
        fs::create_directories(dir_target);
        std::string fail_message;
        TEST_CHECK(!td::save_config(dir_target, keep_cfg, &fail_message));
        TEST_CHECK(!fail_message.empty());
        TEST_CHECK(fs::is_directory(dir_target));
        TEST_CHECK(!fs::exists(fs::path(dir_target.string() + ".tmp")));
        TEST_CHECK(read_file(keep) == before); // original untouched on failure

        keep_cfg.video.width = 1280;
        TEST_CHECK(td::save_config(keep, keep_cfg, &message));
        TEST_CHECK(!fs::exists(fs::path(keep.string() + ".tmp")));
        TEST_CHECK(read_file(keep) != before); // successful save replaced it
        std::cout << "  - 7. atomic save + failure cleanup ok.\n";
    }

    // 8. Stable chart key.
    {
        td::Song song;
        song.simfile_path = "/packs/PackA/Song/Song.sm";
        song.metadata.title = "Tundra Anthem";
        song.metadata.artist = "Composer";
        td::Chart chart;
        chart.steps_type = "dance-single";
        chart.difficulty = "Challenge";
        chart.meter = 9;
        chart.notes = {
            td::Note{0, 0.0, 0.0, td::NoteType::Tap, 0.0, 0.0},
            td::Note{1, 0.5, 0.5, td::NoteType::Tap, 0.0, 0.0},
            td::Note{2, 1.0, 1.0, td::NoteType::HoldHead, 1.0, 1.5},
        };

        const std::string key = td::make_chart_key(song, chart);
        TEST_CHECK(key.size() == 16);
        TEST_CHECK(td::make_chart_key(song, chart) == key);

        td::Chart difficulty_changed = chart;
        difficulty_changed.difficulty = "Hard";
        TEST_CHECK(td::make_chart_key(song, difficulty_changed) != key);

        td::Chart note_changed = chart;
        note_changed.notes[1].beat = 0.75;
        TEST_CHECK(td::make_chart_key(song, note_changed) != key);

        td::Song moved = song;
        moved.simfile_path = "/elsewhere/PackZ/Song/Song.sm";
        TEST_CHECK(td::make_chart_key(moved, chart) == key);

        td::Song upper = song;
        upper.metadata.title = "TUNDRA ANTHEM";
        TEST_CHECK(td::make_chart_key(upper, chart) == key);
        std::cout << "  - 8. chart key stability ok (moved-folder survival).\n";
    }

    // 9. Best-score logic.
    {
        td::HighScores scores;
        TEST_CHECK(td::submit_high_score(scores, "k", td::ScoreRecord{"S+", 0.9, 100, 111}));
        TEST_CHECK(td::find_high_score(scores, "k") != nullptr);
        TEST_CHECK(td::find_high_score(scores, "k")->percent == 0.9);

        TEST_CHECK(!td::submit_high_score(scores, "k", td::ScoreRecord{"A", 0.5, 50, 222}));
        TEST_CHECK(td::find_high_score(scores, "k")->percent == 0.9);
        TEST_CHECK(td::find_high_score(scores, "k")->grade == "S+");

        TEST_CHECK(!td::submit_high_score(scores, "k", td::ScoreRecord{"A", 0.9, 90, 333}));
        TEST_CHECK(td::find_high_score(scores, "k")->dance_points == 100);

        TEST_CHECK(td::submit_high_score(scores, "k", td::ScoreRecord{"quad_star", 1.0, 123, 444}));
        TEST_CHECK(td::find_high_score(scores, "k")->grade == "quad_star");
        TEST_CHECK(td::find_high_score(scores, "unknown") == nullptr);
        std::cout << "  - 9. best-score replacement semantics ok.\n";
    }

    // 10. High-score round-trip + corruption fallback.
    {
        const fs::path scores_path = temp_dir / "scores" / "scores.json";
        td::HighScores scores;
        TEST_CHECK(td::submit_high_score(scores, "abc123", td::ScoreRecord{"S+", 0.94, 88, 1700000000}));
        TEST_CHECK(
            td::submit_high_score(scores, "def456", td::ScoreRecord{"quad_star", 1.0, 123, 1700000001}));

        TEST_CHECK(td::save_high_scores(scores_path, scores, &message));
        TEST_CHECK(!fs::exists(fs::path(scores_path.string() + ".tmp")));

        td::HighScores loaded = td::load_high_scores(scores_path, &message, &scores_status);
        TEST_CHECK(scores_status == td::ScoresLoadStatus::LoadedFromFile);
        TEST_CHECK(loaded.scores.size() == 2);
        const td::ScoreRecord* record = td::find_high_score(loaded, "abc123");
        TEST_CHECK(record != nullptr);
        TEST_CHECK(record->grade == "S+");
        TEST_CHECK(nearly(record->percent, 0.94));
        TEST_CHECK(record->dance_points == 88);
        TEST_CHECK(record->timestamp_unix == 1700000000);

        const fs::path corrupt = temp_dir / "corrupt_scores.json";
        write_file(corrupt, "{ nope");
        td::HighScores corrupt_scores = td::load_high_scores(corrupt, &message, &scores_status);
        TEST_CHECK(scores_status == td::ScoresLoadStatus::UsedDefaults);
        TEST_CHECK(corrupt_scores.scores.empty());
        TEST_CHECK(message.find("[Scores]") != std::string::npos);

        td::HighScores missing =
            td::load_high_scores(temp_dir / "no_scores.json", &message, &scores_status);
        TEST_CHECK(scores_status == td::ScoresLoadStatus::UsedDefaults);
        TEST_CHECK(missing.scores.empty());
        std::cout << "  - 10. high-score round-trip + corruption fallback ok.\n";
    }

    // 10b. Untrusted numeric fields: out-of-range values must never reach an
    //      unchecked integer cast (no UB / silent corruption), and an
    //      out-of-int-range dp is clamped before narrowing.
    {
        const fs::path hostile = temp_dir / "hostile_scores.json";
        write_file(hostile,
                   R"({"version":1,"scores":{)"
                   R"("huge":{"grade":"S+","percent":0.9,"dp":1e300,"timestamp":1e300},)"
                   R"("big_dp":{"grade":"A","percent":0.5,"dp":5000000000,"timestamp":1700000002},)"
                   R"("boundary_hi":{"grade":"A","percent":0.5,"dp":9223372036854775808,"timestamp":1},)"
                   R"("int64_max":{"grade":"A","percent":0.5,"dp":9223372036854775807,"timestamp":1700000004},)"
                   R"("okay":{"grade":"A","percent":0.4,"dp":12,"timestamp":1700000003}}})");
        td::HighScores hostile_scores = td::load_high_scores(hostile, &message, &scores_status);
        TEST_CHECK(scores_status == td::ScoresLoadStatus::LoadedFromFile);
        // The whole-record numeric overflow is rejected, not wrapped.
        TEST_CHECK(td::find_high_score(hostile_scores, "huge") == nullptr);
        // 2^63 is out of int64 range and must be rejected, never cast.
        TEST_CHECK(td::find_high_score(hostile_scores, "boundary_hi") == nullptr);
        // Valid INT64_MAX must survive (then clamp) without becoming INT64_MIN.
        const td::ScoreRecord* int64_max = td::find_high_score(hostile_scores, "int64_max");
        TEST_CHECK(int64_max != nullptr);
        TEST_CHECK(int64_max->dance_points == std::numeric_limits<int>::max());
        TEST_CHECK(int64_max->timestamp_unix == 1700000004);
        // dp outside int but inside int64 is clamped, never wrapped.
        const td::ScoreRecord* big = td::find_high_score(hostile_scores, "big_dp");
        TEST_CHECK(big != nullptr);
        TEST_CHECK(big->dance_points == std::numeric_limits<int>::max());
        TEST_CHECK(big->timestamp_unix == 1700000002);
        // A valid record still loads.
        TEST_CHECK(td::find_high_score(hostile_scores, "okay") != nullptr);
        TEST_CHECK(message.find("warning") != std::string::npos);

        // Re-saving the loaded table must not emit an INT64_MIN-corrupted value.
        const fs::path hostile_out = temp_dir / "hostile_out_scores.json";
        TEST_CHECK(td::save_high_scores(hostile_out, hostile_scores, &message));
        const std::string dumped = read_file(hostile_out);
        TEST_CHECK(dumped.find("-9223372036854775808") == std::string::npos);
        std::cout << "  - 10b. out-of-range score integers bounded ok.\n";
    }

    // 10c. Atomic save temp names are unique and cleaned up (no `<name>.tmp*`).
    {
        const fs::path atomic_path = temp_dir / "atomic" / "config.json";
        td::GameConfig config;
        TEST_CHECK(td::save_config(atomic_path, config, &message));
        std::size_t leftovers = 0;
        for (const auto& entry : fs::directory_iterator(atomic_path.parent_path())) {
            const std::string name = entry.path().filename().string();
            if (name.rfind("config.json.tmp", 0) == 0) {
                ++leftovers;
            }
        }
        TEST_CHECK(leftovers == 0);

        // Distinct concurrent saves must not share a temp path.
        const fs::path other = temp_dir / "atomic" / "scores.json";
        td::HighScores scores;
        TEST_CHECK(td::submit_high_score(scores, "k", td::ScoreRecord{"S+", 0.9, 100, 1}));
        TEST_CHECK(td::save_high_scores(other, scores, &message));
        for (const auto& entry : fs::directory_iterator(other.parent_path())) {
            const std::string name = entry.path().filename().string();
            TEST_CHECK(name.rfind("scores.json.tmp", 0) != 0);
        }
        std::cout << "  - 10c. unique atomic temp names + cleanup ok.\n";
    }

    // 11. Data path resolution.
    {
        const td::ResolvedDataPaths portable = td::resolve_data_paths("/opt/game", false, "", "", {});
        TEST_CHECK(portable.data_dir == fs::path("/opt/game/data"));
        TEST_CHECK(portable.config_file == portable.data_dir / "config.json");
        TEST_CHECK(portable.scores_file == portable.data_dir / "scores.json");

        const td::ResolvedDataPaths xdg =
            td::resolve_data_paths("/opt/game", true, "/x", "/home/u", {});
        TEST_CHECK(xdg.data_dir == fs::path("/x/tundra-dance"));

        const td::ResolvedDataPaths xdg_home =
            td::resolve_data_paths("/opt/game", true, "", "/home/u", {});
        TEST_CHECK(xdg_home.data_dir == fs::path("/home/u/.local/share/tundra-dance"));

        const td::ResolvedDataPaths override =
            td::resolve_data_paths("/opt/game", true, "/x", "/home/u", fs::path("/tmp/explicit"));
        TEST_CHECK(override.data_dir == fs::path("/tmp/explicit"));
        TEST_CHECK(override.config_file == fs::path("/tmp/explicit/config.json"));
        TEST_CHECK(override.scores_file == fs::path("/tmp/explicit/scores.json"));
        std::cout << "  - 11. data path resolution ok.\n";
    }

    fs::remove_all(temp_dir);

    std::cout << "[config_persistence_test] All persistence tests passed!\n";
    return 0;
}
