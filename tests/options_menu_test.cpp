#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "data/config.hpp"
#include "gameplay/speed_mod.hpp"
#include "screens/options_menu.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::OptionsMenu;
using td::OptionsRow;
using td::SpeedMod;
using td::SpeedModType;

constexpr int kSpeedTypeRow = static_cast<int>(OptionsRow::SpeedType);
constexpr int kSpeedValueRow = static_cast<int>(OptionsRow::SpeedValue);
constexpr int kScrollRow = static_cast<int>(OptionsRow::Scroll);
constexpr int kFailRow = static_cast<int>(OptionsRow::Fail);
constexpr int kCalibrateRow = static_cast<int>(OptionsRow::CalibrateOffset);
constexpr int kRemapRow = static_cast<int>(OptionsRow::RemapInput);

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

void test_seeding() {
    td::GameConfig config;
    config.gameplay.speed_mod = "C400";
    config.gameplay.scroll = "down";
    config.gameplay.fail_enabled = false;

    const OptionsMenu menu = td::options_menu_from_config(config);
    TEST_CHECK(menu.speed_type == SpeedModType::CMod);
    TEST_CHECK(near(menu.c_value, 400.0));
    TEST_CHECK(menu.scroll_down);
    TEST_CHECK(!menu.fail_enabled);
    TEST_CHECK(near(menu.x_value, 1.0));  // untouched slots stay at defaults
    TEST_CHECK(near(menu.m_value, 600.0));
    TEST_CHECK(menu.row == kSpeedTypeRow);

    // A small in-range C value is preserved verbatim (full parser range 1..9999).
    td::GameConfig small;
    small.gameplay.speed_mod = "C20";
    const OptionsMenu small_menu = td::options_menu_from_config(small);
    TEST_CHECK(small_menu.speed_type == SpeedModType::CMod);
    TEST_CHECK(near(small_menu.c_value, 20.0));
    std::cout << "  - config seeding ok.\n";
}

void test_invalid_speed() {
    td::GameConfig config;
    config.gameplay.speed_mod = "zzz";

    const OptionsMenu menu = td::options_menu_from_config(config);
    TEST_CHECK(menu.speed_type == SpeedModType::XMod);
    TEST_CHECK(near(menu.x_value, 1.0));
    TEST_CHECK(near(menu.c_value, 450.0));
    TEST_CHECK(near(menu.m_value, 600.0));
    std::cout << "  - invalid speed falls back to defaults ok.\n";
}

void test_row_navigation() {
    OptionsMenu menu;
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 1);
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 2);
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 3);
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kCalibrateRow); // row 4
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kRemapRow); // row 5
    td::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kRemapRow); // clamped at the new bottom
    td::options_menu_move_row(menu, -1);
    TEST_CHECK(menu.row == kCalibrateRow);
    td::options_menu_move_row(menu, -5);
    TEST_CHECK(menu.row == 0); // clamped
    std::cout << "  - row navigation clamps at both ends ok.\n";
}

void test_type_cycle_and_memory() {
    td::GameConfig config;
    config.gameplay.speed_mod = "1.5x";
    OptionsMenu menu = td::options_menu_from_config(config);
    const double remembered_x = menu.x_value;
    TEST_CHECK(near(remembered_x, 1.5));

    td::options_menu_adjust(menu, +1); // X -> C
    TEST_CHECK(menu.speed_type == SpeedModType::CMod);
    td::options_menu_adjust(menu, +1); // C -> M
    TEST_CHECK(menu.speed_type == SpeedModType::MMod);
    td::options_menu_adjust(menu, +1); // M -> X (wraps)
    TEST_CHECK(menu.speed_type == SpeedModType::XMod);
    TEST_CHECK(near(menu.x_value, remembered_x)); // per-type memory preserved

    td::options_menu_adjust(menu, -1); // X -> M (wraps backwards)
    TEST_CHECK(menu.speed_type == SpeedModType::MMod);
    std::cout << "  - speed type cycle + per-type memory ok.\n";
}

void test_speed_value_step_clamp() {
    OptionsMenu menu;
    menu.row = kSpeedValueRow;

    // X-mod: OpenITG option-menu grid {1,1.5,2,2.5,3,4,5,6}.
    menu.speed_type = SpeedModType::XMod;
    menu.set_speed_value(1.0);
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.x_value, 1.5));
    menu.set_speed_value(1.0);
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.x_value, 1.0)); // floor
    menu.set_speed_value(6.0);
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.x_value, 6.0)); // ceiling

    // C/M-mod: OpenITG defines no increment, so the menu steps relative to the
    // current value and clamps to the parser-valid range [1, 9999].
    menu.speed_type = SpeedModType::CMod;
    menu.set_speed_value(450.0);
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 460.0));
    menu.set_speed_value(1.0);
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.c_value, 1.0)); // floor
    menu.set_speed_value(9999.0);
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 9999.0)); // ceiling

    menu.speed_type = SpeedModType::MMod;
    menu.set_speed_value(600.0);
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.m_value, 610.0));

    // A seeded off-grid value is preserved and steps relative to itself rather
    // than snapping to a grid.
    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 403.0;
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 413.0));
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.c_value, 403.0));

    // The C/M grid exposes the full parser-valid range for clamping.
    const std::vector<double> cm_values = td::options_speed_values(SpeedModType::CMod);
    TEST_CHECK(near(cm_values.front(), 1.0));
    TEST_CHECK(near(cm_values.back(), 9999.0));
    std::cout << "  - speed value step/clamp ok.\n";
}

void test_toggles() {
    OptionsMenu menu;
    menu.row = kScrollRow;
    const bool scroll_before = menu.scroll_down;
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(menu.scroll_down != scroll_before);
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.scroll_down == scroll_before);

    menu.row = kFailRow;
    const bool fail_before = menu.fail_enabled;
    td::options_menu_adjust(menu, +1);
    TEST_CHECK(menu.fail_enabled != fail_before);
    std::cout << "  - scroll/fail toggles ok.\n";
}

void test_formatting() {
    OptionsMenu menu;

    menu.speed_type = SpeedModType::XMod;
    menu.x_value = 1.0;
    TEST_CHECK(td::options_row_value_text(menu, kSpeedTypeRow) == "XMOD");
    TEST_CHECK(td::options_row_value_text(menu, kSpeedValueRow) == "1x");
    menu.x_value = 1.5;
    TEST_CHECK(td::options_row_value_text(menu, kSpeedValueRow) == "1.5x");

    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 450.0;
    TEST_CHECK(td::options_row_value_text(menu, kSpeedTypeRow) == "CMOD");
    TEST_CHECK(td::options_row_value_text(menu, kSpeedValueRow) == "C450");

    menu.speed_type = SpeedModType::MMod;
    menu.m_value = 600.0;
    TEST_CHECK(td::options_row_value_text(menu, kSpeedTypeRow) == "MMOD");
    TEST_CHECK(td::options_row_value_text(menu, kSpeedValueRow) == "M600");

    menu.scroll_down = false;
    TEST_CHECK(td::options_row_value_text(menu, kScrollRow) == "UP");
    menu.scroll_down = true;
    TEST_CHECK(td::options_row_value_text(menu, kScrollRow) == "DOWN");

    menu.fail_enabled = true;
    TEST_CHECK(td::options_row_value_text(menu, kFailRow) == "ON");
    menu.fail_enabled = false;
    TEST_CHECK(td::options_row_value_text(menu, kFailRow) == "OFF");

    TEST_CHECK(td::options_row_name(kSpeedTypeRow) == "SPEED TYPE");
    TEST_CHECK(td::options_row_name(kFailRow) == "FAIL");
    TEST_CHECK(td::options_row_name(kCalibrateRow) == "CALIBRATE OFFSET");

    menu.offset_seconds = 0.023;
    TEST_CHECK(td::options_row_value_text(menu, kCalibrateRow) == td::format_offset(0.023));
    TEST_CHECK(td::format_offset(0.023).find("+0.023") != std::string::npos);
    TEST_CHECK(td::format_offset(-0.011).find("-0.011") != std::string::npos);
    std::cout << "  - display text ok.\n";
}

void test_calibration_row_is_action_only() {
    td::GameConfig config;
    config.offset.global_offset_seconds = 0.023;
    OptionsMenu menu = td::options_menu_from_config(config);
    TEST_CHECK(near(menu.offset_seconds, 0.023)); // seeded from config for display

    menu.row = kCalibrateRow;
    const OptionsMenu before = menu;
    td::options_menu_adjust(menu, +1);
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.offset_seconds == before.offset_seconds);
    TEST_CHECK(menu.speed_type == before.speed_type);
    TEST_CHECK(menu.scroll_down == before.scroll_down);

    // options_menu_apply must never clobber the wizard-owned offset.
    td::options_menu_apply(menu, config);
    TEST_CHECK(near(config.offset.global_offset_seconds, 0.023));
    std::cout << "  - calibration row is a display-only action ok.\n";
}

void test_remap_row_is_action_only() {
    TEST_CHECK(td::options_row_name(kRemapRow) == "REMAP INPUT");
    TEST_CHECK(td::kOptionsRowCount == 6);

    td::GameConfig config;
    const std::vector<td::InputBinding> key_before = config.input.key_bindings;
    const std::vector<td::InputBinding> pad_before = config.input.gamepad_bindings;

    OptionsMenu menu = td::options_menu_from_config(config);
    menu.row = kRemapRow;
    const OptionsMenu before = menu;
    td::options_menu_adjust(menu, +1);
    td::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.speed_type == before.speed_type);
    TEST_CHECK(menu.scroll_down == before.scroll_down);
    TEST_CHECK(menu.fail_enabled == before.fail_enabled);

    // The remap row must never touch the binding maps.
    td::options_menu_apply(menu, config);
    TEST_CHECK(config.input.key_bindings == key_before);
    TEST_CHECK(config.input.gamepad_bindings == pad_before);
    std::cout << "  - remap row is an action-only entry ok.\n";
}

void test_apply_and_round_trip() {
    td::GameConfig config;
    OptionsMenu menu;
    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 400.0;
    menu.scroll_down = true;
    menu.fail_enabled = false;

    td::options_menu_apply(menu, config);
    TEST_CHECK(config.gameplay.speed_mod == "C400");
    TEST_CHECK(config.gameplay.scroll == "down");
    TEST_CHECK(!config.gameplay.fail_enabled);

    SpeedMod parsed;
    TEST_CHECK(td::parse_speed_mod(config.gameplay.speed_mod, parsed));
    TEST_CHECK(parsed.type == SpeedModType::CMod);
    TEST_CHECK(near(parsed.value, 400.0));

    const OptionsMenu round_tripped = td::options_menu_from_config(config);
    TEST_CHECK(round_tripped.speed_type == menu.speed_type);
    TEST_CHECK(near(round_tripped.c_value, menu.c_value));
    TEST_CHECK(round_tripped.scroll_down == menu.scroll_down);
    TEST_CHECK(round_tripped.fail_enabled == menu.fail_enabled);

    // X and M forms round-trip through the parser too.
    td::GameConfig x_config;
    OptionsMenu x_menu;
    x_menu.speed_type = SpeedModType::XMod;
    x_menu.x_value = 1.5;
    td::options_menu_apply(x_menu, x_config);
    TEST_CHECK(x_config.gameplay.speed_mod == "1.5x");
    SpeedMod x_parsed;
    TEST_CHECK(td::parse_speed_mod(x_config.gameplay.speed_mod, x_parsed));
    TEST_CHECK(x_parsed.type == SpeedModType::XMod && near(x_parsed.value, 1.5));

    td::GameConfig m_config;
    OptionsMenu m_menu;
    m_menu.speed_type = SpeedModType::MMod;
    m_menu.m_value = 600.0;
    td::options_menu_apply(m_menu, m_config);
    TEST_CHECK(m_config.gameplay.speed_mod == "M600");
    std::cout << "  - apply + round-trip ok.\n";
}

} // namespace

int main() {
    std::cout << "[options_menu_test] Running OptionsMenu tests...\n";
    test_seeding();
    test_invalid_speed();
    test_row_navigation();
    test_type_cycle_and_memory();
    test_speed_value_step_clamp();
    test_toggles();
    test_formatting();
    test_calibration_row_is_action_only();
    test_remap_row_is_action_only();
    test_apply_and_round_trip();
    std::cout << "[options_menu_test] All tests passed!\n";
    return 0;
}
