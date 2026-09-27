#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "data/config.hpp"
#include "data/config_loader.hpp"
#include "screens/input_remap.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::DeviceType;
using td::GameAction;
using td::InputBinding;
using td::InputRemapModel;
using td::RemapRow;
using td::RemapStatus;

std::map<std::string, std::vector<std::string>> to_map(const std::vector<InputBinding>& bindings) {
    std::map<std::string, std::vector<std::string>> result;
    for (const InputBinding& binding : bindings) {
        result[binding.first] = binding.second;
    }
    return result;
}

int find_row(const InputRemapModel& model, GameAction action, DeviceType device,
             const std::string& name) {
    for (std::size_t i = 0; i < model.rows.size(); ++i) {
        const RemapRow& row = model.rows[i];
        if (row.action == action && row.device == device && row.name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// One row per binding slot, keyboard-first then gamepad.
void test_enumeration() {
    td::GameConfig config;
    const InputRemapModel model = td::input_remap_from_config(config);

    // One row per binding slot: keyboard slots first, then gamepad slots.
    std::size_t keyboard_slots = 0;
    for (const InputBinding& binding : td::default_key_bindings()) {
        keyboard_slots += binding.second.size();
    }
    std::size_t gamepad_slots = 0;
    for (const InputBinding& binding : td::default_gamepad_bindings()) {
        gamepad_slots += binding.second.size();
    }
    TEST_CHECK(model.rows.size() == keyboard_slots + gamepad_slots);

    // Spot-check the slot mapping (arrows + DFJK both preserved).
    TEST_CHECK(find_row(model, GameAction::Left, DeviceType::Keyboard, "Left") >= 0);
    TEST_CHECK(find_row(model, GameAction::Left, DeviceType::Keyboard, "D") >= 0);
    TEST_CHECK(find_row(model, GameAction::Confirm, DeviceType::Keyboard, "Keypad Enter") >= 0);
    TEST_CHECK(find_row(model, GameAction::Options, DeviceType::Keyboard, "Tab") >= 0);
    TEST_CHECK(find_row(model, GameAction::Left, DeviceType::Gamepad, "dpleft") >= 0);
    TEST_CHECK(find_row(model, GameAction::Back, DeviceType::Gamepad, "back") >= 0);
    TEST_CHECK(find_row(model, GameAction::Options, DeviceType::Gamepad, "leftshoulder") >= 0);

    // Keyboard-first ordering.
    TEST_CHECK(model.rows.front().device == DeviceType::Keyboard);
    TEST_CHECK(model.rows.back().device == DeviceType::Gamepad);

    // A config overriding only Confirm keeps every other default action.
    td::GameConfig partial;
    partial.input.key_bindings = {{"Confirm", {"Space"}}};
    const InputRemapModel partial_model = td::input_remap_from_config(partial);
    TEST_CHECK(find_row(partial_model, GameAction::Confirm, DeviceType::Keyboard, "Space") >= 0);
    TEST_CHECK(find_row(partial_model, GameAction::Confirm, DeviceType::Keyboard, "Return") < 0);
    TEST_CHECK(find_row(partial_model, GameAction::Left, DeviceType::Keyboard, "Left") >= 0);
    TEST_CHECK(find_row(partial_model, GameAction::Options, DeviceType::Keyboard, "Tab") >= 0);

    // An override with an empty name list falls back to the default slots,
    // matching apply_bindings' runtime merge (never zero rows for one action).
    td::GameConfig empty_override;
    empty_override.input.key_bindings = {{"Options", {}}};
    const InputRemapModel fallback_model = td::input_remap_from_config(empty_override);
    TEST_CHECK(find_row(fallback_model, GameAction::Options, DeviceType::Keyboard, "Tab") >= 0);
    std::cout << "  - 1. enumeration ok.\n";
}

void test_move_clamp() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    TEST_CHECK(!model.rows.empty());
    const int last = static_cast<int>(model.rows.size()) - 1;

    td::input_remap_move_row(model, -1);
    TEST_CHECK(model.row == 0); // clamped at the top
    for (int i = 0; i < last + 5; ++i) {
        td::input_remap_move_row(model, +1);
    }
    TEST_CHECK(model.row == last); // clamped at the bottom

    input_remap_set_row(model, -100);
    TEST_CHECK(model.row == 0);
    input_remap_set_row(model, last + 100);
    TEST_CHECK(model.row == last);
    std::cout << "  - 2. move/set clamp ok.\n";
}

void test_assign_replace() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    const int confirm_row = find_row(model, GameAction::Confirm, DeviceType::Keyboard, "Return");
    TEST_CHECK(confirm_row >= 0);
    td::input_remap_set_row(model, confirm_row);

    const std::size_t before = model.rows.size();
    const RemapStatus status = td::input_remap_assign(model, "Space");
    TEST_CHECK(status == RemapStatus::Replaced);
    TEST_CHECK(model.rows.size() == before);
    TEST_CHECK(model.rows[static_cast<std::size_t>(confirm_row)].name == "Space");
    // Other rows unchanged.
    TEST_CHECK(find_row(model, GameAction::Confirm, DeviceType::Keyboard, "Keypad Enter") >= 0);
    TEST_CHECK(find_row(model, GameAction::Back, DeviceType::Keyboard, "Escape") >= 0);
    std::cout << "  - 3. assign/replace ok.\n";
}

void test_conflict() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    const int left_row = find_row(model, GameAction::Left, DeviceType::Keyboard, "Left");
    TEST_CHECK(left_row >= 0);
    td::input_remap_set_row(model, left_row);

    // "Down" is already bound to the Down action.
    const std::vector<RemapRow> before = model.rows;
    const RemapStatus status = td::input_remap_assign(model, "Down");
    TEST_CHECK(status == RemapStatus::Conflict);
    TEST_CHECK(!model.message.empty());
    TEST_CHECK(model.message.find("DOWN") != std::string::npos);
    TEST_CHECK(model.rows == before); // rejected: nothing changed
    std::cout << "  - 4. conflict rejected with message ok.\n";
}

void test_duplicate_same_action() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    const int confirm_row = find_row(model, GameAction::Confirm, DeviceType::Keyboard, "Return");
    const int second_confirm = find_row(model, GameAction::Confirm, DeviceType::Keyboard,
                                        "Keypad Enter");
    TEST_CHECK(confirm_row >= 0 && second_confirm >= 0);
    td::input_remap_set_row(model, second_confirm);

    // "Return" already belongs to Confirm.
    const RemapStatus status = td::input_remap_assign(model, "Return");
    TEST_CHECK(status == RemapStatus::Unchanged);
    TEST_CHECK(model.rows[static_cast<std::size_t>(second_confirm)].name == "Keypad Enter");
    std::cout << "  - 5. duplicate on same action unchanged ok.\n";
}

void test_reset() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    const std::vector<RemapRow> defaults = model.rows;

    const int left_row = find_row(model, GameAction::Left, DeviceType::Keyboard, "Left");
    td::input_remap_set_row(model, left_row);
    TEST_CHECK(td::input_remap_assign(model, "Q") == RemapStatus::Replaced);
    TEST_CHECK(model.rows != defaults);

    td::input_remap_reset(model);
    TEST_CHECK(model.rows == defaults);
    std::cout << "  - 6. reset restores default rows ok.\n";
}

void test_apply_grouping_and_round_trip() {
    InputRemapModel model = td::input_remap_from_config(td::GameConfig{});
    const int confirm_row = find_row(model, GameAction::Confirm, DeviceType::Keyboard, "Return");
    td::input_remap_set_row(model, confirm_row);
    TEST_CHECK(td::input_remap_assign(model, "Space") == RemapStatus::Replaced);

    td::InputSettings settings;
    td::input_remap_apply(model, settings);

    const auto key_map = to_map(settings.key_bindings);
    TEST_CHECK(key_map.count("Confirm") == 1);
    const std::vector<std::string>& confirm_names = key_map.at("Confirm");
    TEST_CHECK(std::find(confirm_names.begin(), confirm_names.end(), "Space") !=
               confirm_names.end());
    TEST_CHECK(std::find(confirm_names.begin(), confirm_names.end(), "Keypad Enter") !=
               confirm_names.end());
    TEST_CHECK(!key_map.at("Left").empty());
    TEST_CHECK(!key_map.at("Options").empty());
    TEST_CHECK(to_map(settings.gamepad_bindings).count("Back") == 1);

    // Round-trip through persistence (AC3).
    const fs::path temp_dir = fs::temp_directory_path() / "td_input_remap_test";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);
    const fs::path path = temp_dir / "config.json";

    td::GameConfig config;
    config.input = settings;
    std::string message;
    TEST_CHECK(td::save_config(path, config, &message));
    td::ConfigLoadStatus status = td::ConfigLoadStatus::UsedDefaults;
    const td::GameConfig loaded = td::load_config(path, &message, &status);
    TEST_CHECK(status == td::ConfigLoadStatus::LoadedFromFile);
    const auto loaded_map = to_map(loaded.input.key_bindings);
    const std::vector<std::string>& loaded_confirm = loaded_map.at("Confirm");
    TEST_CHECK(std::find(loaded_confirm.begin(), loaded_confirm.end(), "Space") !=
               loaded_confirm.end());
    fs::remove_all(temp_dir);
    std::cout << "  - 7. apply grouping + persistence round-trip ok.\n";
}

void test_empty_config() {
    td::GameConfig defaults;
    const InputRemapModel default_model = td::input_remap_from_config(defaults);
    TEST_CHECK(!default_model.rows.empty());

    td::GameConfig empty;
    empty.input.key_bindings.clear();
    empty.input.gamepad_bindings.clear();
    const InputRemapModel empty_model = td::input_remap_from_config(empty);
    TEST_CHECK(!empty_model.rows.empty()); // defaults fill in, no crash

    InputRemapModel mutate = empty_model;
    td::input_remap_move_row(mutate, +1);
    td::input_remap_assign(mutate, "Z");
    td::InputSettings settings;
    td::input_remap_apply(mutate, settings);
    TEST_CHECK(!settings.key_bindings.empty());
    std::cout << "  - 8. empty config is safe ok.\n";
}

} // namespace

int main() {
    std::cout << "[input_remap_test] Running pure remap-model tests...\n";
    test_enumeration();
    test_move_clamp();
    test_assign_replace();
    test_conflict();
    test_duplicate_same_action();
    test_reset();
    test_apply_grouping_and_round_trip();
    test_empty_config();
    std::cout << "[input_remap_test] All tests passed!\n";
    return 0;
}
