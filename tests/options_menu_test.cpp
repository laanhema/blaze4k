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

using blaze4k::OptionsMenu;
using blaze4k::OptionsRow;
using blaze4k::SpeedMod;
using blaze4k::SpeedModType;

constexpr int kSpeedTypeRow = static_cast<int>(OptionsRow::SpeedType);
constexpr int kSpeedValueRow = static_cast<int>(OptionsRow::SpeedValue);
constexpr int kScrollRow = static_cast<int>(OptionsRow::Scroll);
constexpr int kFailRow = static_cast<int>(OptionsRow::Fail);
constexpr int kAssistTickRow = static_cast<int>(OptionsRow::AssistTick);
constexpr int kCalibrateRow = static_cast<int>(OptionsRow::CalibrateOffset);
constexpr int kRemapRow = static_cast<int>(OptionsRow::RemapInput);

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

void test_seeding() {
    blaze4k::GameConfig config;
    config.gameplay.speed_mod = "C400";
    config.gameplay.scroll = "down";
    config.gameplay.fail_enabled = false;
    config.gameplay.assist_tick = true;

    const OptionsMenu menu = blaze4k::options_menu_from_config(config);
    TEST_CHECK(menu.speed_type == SpeedModType::CMod);
    TEST_CHECK(near(menu.c_value, 400.0));
    TEST_CHECK(menu.scroll_down);
    TEST_CHECK(!menu.fail_enabled);
    TEST_CHECK(menu.assist_tick);
    TEST_CHECK(near(menu.x_value, 1.0));  // untouched slots stay at defaults
    TEST_CHECK(near(menu.m_value, 600.0));
    TEST_CHECK(menu.row == kSpeedTypeRow);

    // A small in-range C value is preserved verbatim (full parser range 1..9999).
    blaze4k::GameConfig small;
    small.gameplay.speed_mod = "C20";
    const OptionsMenu small_menu = blaze4k::options_menu_from_config(small);
    TEST_CHECK(small_menu.speed_type == SpeedModType::CMod);
    TEST_CHECK(near(small_menu.c_value, 20.0));
    std::cout << "  - config seeding ok.\n";
}

void test_invalid_speed() {
    blaze4k::GameConfig config;
    config.gameplay.speed_mod = "zzz";

    const OptionsMenu menu = blaze4k::options_menu_from_config(config);
    TEST_CHECK(menu.speed_type == SpeedModType::XMod);
    TEST_CHECK(near(menu.x_value, 1.0));
    TEST_CHECK(near(menu.c_value, 450.0));
    TEST_CHECK(near(menu.m_value, 600.0));
    std::cout << "  - invalid speed falls back to defaults ok.\n";
}

void test_row_navigation() {
    OptionsMenu menu;
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 1);
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 2);
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == 3);
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kAssistTickRow); // row 4
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kCalibrateRow); // row 5
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kRemapRow); // row 6
    blaze4k::options_menu_move_row(menu, +1);
    TEST_CHECK(menu.row == kRemapRow); // clamped at the new bottom
    blaze4k::options_menu_move_row(menu, -1);
    TEST_CHECK(menu.row == kCalibrateRow);
    blaze4k::options_menu_move_row(menu, -6);
    TEST_CHECK(menu.row == 0); // clamped
    std::cout << "  - row navigation clamps at both ends ok.\n";
}

void test_type_cycle_and_memory() {
    blaze4k::GameConfig config;
    config.gameplay.speed_mod = "1.5x";
    OptionsMenu menu = blaze4k::options_menu_from_config(config);
    const double remembered_x = menu.x_value;
    TEST_CHECK(near(remembered_x, 1.5));

    blaze4k::options_menu_adjust(menu, +1); // X -> C
    TEST_CHECK(menu.speed_type == SpeedModType::CMod);
    blaze4k::options_menu_adjust(menu, +1); // C -> M
    TEST_CHECK(menu.speed_type == SpeedModType::MMod);
    blaze4k::options_menu_adjust(menu, +1); // M -> X (wraps)
    TEST_CHECK(menu.speed_type == SpeedModType::XMod);
    TEST_CHECK(near(menu.x_value, remembered_x)); // per-type memory preserved

    blaze4k::options_menu_adjust(menu, -1); // X -> M (wraps backwards)
    TEST_CHECK(menu.speed_type == SpeedModType::MMod);
    std::cout << "  - speed type cycle + per-type memory ok.\n";
}

void test_speed_value_step_clamp() {
    OptionsMenu menu;
    menu.row = kSpeedValueRow;

    // X-mod: OpenITG option-menu grid {1,1.5,2,2.5,3,4,5,6}.
    menu.speed_type = SpeedModType::XMod;
    menu.set_speed_value(1.0);
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.x_value, 1.5));
    menu.set_speed_value(1.0);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.x_value, 1.0)); // floor
    menu.set_speed_value(6.0);
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.x_value, 6.0)); // ceiling

    // C/M-mod: OpenITG defines no increment, so the menu steps relative to the
    // current value and clamps to the parser-valid range [1, 9999].
    menu.speed_type = SpeedModType::CMod;
    menu.set_speed_value(450.0);
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 460.0));
    menu.set_speed_value(1.0);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.c_value, 1.0)); // floor
    menu.set_speed_value(9999.0);
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 9999.0)); // ceiling

    menu.speed_type = SpeedModType::MMod;
    menu.set_speed_value(600.0);
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.m_value, 610.0));

    // A seeded off-grid value is preserved and steps relative to itself rather
    // than snapping to a grid.
    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 403.0;
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(near(menu.c_value, 413.0));
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(near(menu.c_value, 403.0));

    // The C/M grid exposes the full parser-valid range for clamping.
    const std::vector<double> cm_values = blaze4k::options_speed_values(SpeedModType::CMod);
    TEST_CHECK(near(cm_values.front(), 1.0));
    TEST_CHECK(near(cm_values.back(), 9999.0));
    std::cout << "  - speed value step/clamp ok.\n";
}

void test_toggles() {
    OptionsMenu menu;
    menu.row = kScrollRow;
    const bool scroll_before = menu.scroll_down;
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(menu.scroll_down != scroll_before);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.scroll_down == scroll_before);

    menu.row = kFailRow;
    const bool fail_before = menu.fail_enabled;
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(menu.fail_enabled != fail_before);

    menu.row = kAssistTickRow;
    TEST_CHECK(!menu.assist_tick); // off by default
    blaze4k::options_menu_adjust(menu, +1);
    TEST_CHECK(menu.assist_tick);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(!menu.assist_tick);
    std::cout << "  - scroll/fail/assist tick toggles ok.\n";
}

void test_formatting() {
    OptionsMenu menu;

    menu.speed_type = SpeedModType::XMod;
    menu.x_value = 1.0;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedTypeRow) == "XMOD");
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedValueRow) == "1x");
    menu.x_value = 1.5;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedValueRow) == "1.5x");

    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 450.0;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedTypeRow) == "CMOD");
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedValueRow) == "C450");

    menu.speed_type = SpeedModType::MMod;
    menu.m_value = 600.0;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedTypeRow) == "MMOD");
    TEST_CHECK(blaze4k::options_row_value_text(menu, kSpeedValueRow) == "M600");

    menu.scroll_down = false;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kScrollRow) == "UP");
    menu.scroll_down = true;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kScrollRow) == "DOWN");

    menu.fail_enabled = true;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kFailRow) == "ON");
    menu.fail_enabled = false;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kFailRow) == "OFF");

    menu.assist_tick = false;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kAssistTickRow) == "OFF");
    menu.assist_tick = true;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kAssistTickRow) == "ON");
    TEST_CHECK(blaze4k::options_row_name(kAssistTickRow) == "ASSIST TICK");

    TEST_CHECK(blaze4k::options_row_name(kSpeedTypeRow) == "SPEED TYPE");
    TEST_CHECK(blaze4k::options_row_name(kFailRow) == "FAIL");
    TEST_CHECK(blaze4k::options_row_name(kCalibrateRow) == "CALIBRATE OFFSET");

    menu.offset_seconds = 0.023;
    TEST_CHECK(blaze4k::options_row_value_text(menu, kCalibrateRow) == blaze4k::format_offset(0.023));
    TEST_CHECK(blaze4k::format_offset(0.023).find("+0.023") != std::string::npos);
    TEST_CHECK(blaze4k::format_offset(-0.011).find("-0.011") != std::string::npos);
    std::cout << "  - display text ok.\n";
}

void test_calibration_row_is_action_only() {
    blaze4k::GameConfig config;
    config.offset.global_offset_seconds = 0.023;
    OptionsMenu menu = blaze4k::options_menu_from_config(config);
    TEST_CHECK(near(menu.offset_seconds, 0.023)); // seeded from config for display

    menu.row = kCalibrateRow;
    const OptionsMenu before = menu;
    blaze4k::options_menu_adjust(menu, +1);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.offset_seconds == before.offset_seconds);
    TEST_CHECK(menu.speed_type == before.speed_type);
    TEST_CHECK(menu.scroll_down == before.scroll_down);

    // options_menu_apply must never clobber the wizard-owned offset.
    blaze4k::options_menu_apply(menu, config);
    TEST_CHECK(near(config.offset.global_offset_seconds, 0.023));
    std::cout << "  - calibration row is a display-only action ok.\n";
}

void test_remap_row_is_action_only() {
    TEST_CHECK(blaze4k::options_row_name(kRemapRow) == "REMAP INPUT");
    TEST_CHECK(blaze4k::kOptionsRowCount == 7);

    blaze4k::GameConfig config;
    const std::vector<blaze4k::InputBinding> key_before = config.input.key_bindings;
    const std::vector<blaze4k::InputBinding> pad_before = config.input.gamepad_bindings;

    OptionsMenu menu = blaze4k::options_menu_from_config(config);
    menu.row = kRemapRow;
    const OptionsMenu before = menu;
    blaze4k::options_menu_adjust(menu, +1);
    blaze4k::options_menu_adjust(menu, -1);
    TEST_CHECK(menu.speed_type == before.speed_type);
    TEST_CHECK(menu.scroll_down == before.scroll_down);
    TEST_CHECK(menu.fail_enabled == before.fail_enabled);

    // The remap row must never touch the binding maps.
    blaze4k::options_menu_apply(menu, config);
    TEST_CHECK(config.input.key_bindings == key_before);
    TEST_CHECK(config.input.gamepad_bindings == pad_before);
    std::cout << "  - remap row is an action-only entry ok.\n";
}

void test_apply_and_round_trip() {
    blaze4k::GameConfig config;
    OptionsMenu menu;
    menu.speed_type = SpeedModType::CMod;
    menu.c_value = 400.0;
    menu.scroll_down = true;
    menu.fail_enabled = false;
    menu.assist_tick = true;

    blaze4k::options_menu_apply(menu, config);
    TEST_CHECK(config.gameplay.speed_mod == "C400");
    TEST_CHECK(config.gameplay.scroll == "down");
    TEST_CHECK(!config.gameplay.fail_enabled);
    TEST_CHECK(config.gameplay.assist_tick);

    SpeedMod parsed;
    TEST_CHECK(blaze4k::parse_speed_mod(config.gameplay.speed_mod, parsed));
    TEST_CHECK(parsed.type == SpeedModType::CMod);
    TEST_CHECK(near(parsed.value, 400.0));

    const OptionsMenu round_tripped = blaze4k::options_menu_from_config(config);
    TEST_CHECK(round_tripped.speed_type == menu.speed_type);
    TEST_CHECK(near(round_tripped.c_value, menu.c_value));
    TEST_CHECK(round_tripped.scroll_down == menu.scroll_down);
    TEST_CHECK(round_tripped.fail_enabled == menu.fail_enabled);
    TEST_CHECK(round_tripped.assist_tick == menu.assist_tick);

    // X and M forms round-trip through the parser too.
    blaze4k::GameConfig x_config;
    OptionsMenu x_menu;
    x_menu.speed_type = SpeedModType::XMod;
    x_menu.x_value = 1.5;
    blaze4k::options_menu_apply(x_menu, x_config);
    TEST_CHECK(x_config.gameplay.speed_mod == "1.5x");
    SpeedMod x_parsed;
    TEST_CHECK(blaze4k::parse_speed_mod(x_config.gameplay.speed_mod, x_parsed));
    TEST_CHECK(x_parsed.type == SpeedModType::XMod && near(x_parsed.value, 1.5));

    blaze4k::GameConfig m_config;
    OptionsMenu m_menu;
    m_menu.speed_type = SpeedModType::MMod;
    m_menu.m_value = 600.0;
    blaze4k::options_menu_apply(m_menu, m_config);
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
