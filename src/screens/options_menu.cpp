#include "screens/options_menu.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>

#include "data/config.hpp"

namespace td {

namespace {

// ---------------------------------------------------------------------------
// Value Provenance (see the implementation report for the full table).
//
// Authority: OpenITG commit f2c129fe65c65e4a9b3a691ff35e7717b4e8de51.
//
// X-mod values: the PlayerOptions screen lists exactly these eight entries
//   assets/patch-data/Themes/default/metrics.ini:3694-3701
//     Speed,1..8 = mod,1x / 1.5x / 2x / 2.5x / 3x / 4x / 5x / 6x
// OpenITG's in-game scroll-speed code gesture walks a finer sequence
//   src/CodeDetector.cpp:220-221 (0.5,0.75,1.0,1.5,2.0,3.0,4.0,5.0,8.0);
// the option-menu values above are used here because this menu is the
// PlayerOptions menu, not the in-game code gesture.
//
// M-mod default: metrics.ini:3703 -> M600.
// C-mod default: metrics.ini:3702 -> C450.
//
// C/M step + clamp: the parser/OpenITG *range* is C/M 1..9999
//   (src/OptionRowHandler.cpp:171-172). OpenITG's PlayerOptions menu offers a
//   single C value (C450) and a single M value (M600) and defines no increment,
//   so the step is a UI affordance, not a parity value. The menu steps C/M
//   relative to the current value by kCmStep and clamps to the sourced
//   [1, 9999] range, which preserves any seeded off-grid value (e.g. C400)
//   until the player adjusts it.
// ---------------------------------------------------------------------------

constexpr double kXModValues[] = {1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0};
constexpr double kCmStep = 10.0;   // UI affordance (no OpenITG increment exists)
constexpr double kCmMin = 1.0;     // parser/OpenITG valid range
constexpr double kCmMax = 9999.0;  // parser/OpenITG valid range

constexpr SpeedModType kSpeedTypeOrder[3] = {SpeedModType::XMod, SpeedModType::CMod,
                                             SpeedModType::MMod};

std::string format_number(double value) {
    std::ostringstream out;
    if (std::fabs(value - std::round(value)) < 1e-9) {
        out << static_cast<long long>(std::llround(value));
    } else {
        out << std::fixed << std::setprecision(2) << value;
        std::string text = out.str();
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
        return text;
    }
    return out.str();
}

std::string format_speed_mod(const SpeedMod& mod) {
    switch (mod.type) {
        case SpeedModType::XMod:
            return format_number(mod.value) + "x";
        case SpeedModType::CMod:
            return "C" + format_number(mod.value);
        case SpeedModType::MMod:
            return "M" + format_number(mod.value);
    }
    return format_number(mod.value) + "x";
}

} // namespace

std::string options_speed_type_name(SpeedModType type) {
    switch (type) {
        case SpeedModType::XMod:
            return "XMOD";
        case SpeedModType::CMod:
            return "CMOD";
        case SpeedModType::MMod:
            return "MMOD";
    }
    return "XMOD";
}

std::vector<double> options_speed_values(SpeedModType type) {
    if (type == SpeedModType::XMod) {
        return std::vector<double>(std::begin(kXModValues), std::end(kXModValues));
    }
    // C/M have no OpenITG-sourced grid: the parser accepts any integer in
    // [1, 9999]. Return the valid-range endpoints so set_speed_value() can clamp
    // generically; the menu steps C/M relative to the current value.
    return {kCmMin, kCmMax};
}

double OptionsMenu::speed_value() const {
    switch (speed_type) {
        case SpeedModType::XMod:
            return x_value;
        case SpeedModType::CMod:
            return c_value;
        case SpeedModType::MMod:
            return m_value;
    }
    return x_value;
}

void OptionsMenu::set_speed_value(double value) {
    const std::vector<double> values = options_speed_values(speed_type);
    value = std::clamp(value, values.front(), values.back());
    switch (speed_type) {
        case SpeedModType::XMod:
            x_value = value;
            break;
        case SpeedModType::CMod:
            c_value = value;
            break;
        case SpeedModType::MMod:
            m_value = value;
            break;
    }
}

OptionsMenu options_menu_from_config(const GameConfig& config) {
    OptionsMenu menu;

    SpeedMod mod;
    if (parse_speed_mod(config.gameplay.speed_mod, mod)) {
        menu.speed_type = mod.type;
        switch (mod.type) {
            case SpeedModType::XMod:
                menu.x_value = mod.value;
                break;
            case SpeedModType::CMod:
                menu.c_value = mod.value;
                break;
            case SpeedModType::MMod:
                menu.m_value = mod.value;
                break;
        }
    }

    menu.scroll_down = config.gameplay.scroll == "down";
    menu.fail_enabled = config.gameplay.fail_enabled;
    menu.assist_tick = config.gameplay.assist_tick;
    menu.offset_seconds = config.offset.global_offset_seconds;
    menu.row = static_cast<int>(OptionsRow::SpeedType);
    return menu;
}

void options_menu_apply(const OptionsMenu& menu, GameConfig& config) {
    const SpeedMod mod{menu.speed_type, menu.speed_value()};
    config.gameplay.speed_mod = format_speed_mod(mod);
    config.gameplay.scroll = menu.scroll_down ? "down" : "up";
    config.gameplay.fail_enabled = menu.fail_enabled;
    config.gameplay.assist_tick = menu.assist_tick;
}

void options_menu_move_row(OptionsMenu& menu, int delta) {
    menu.row = std::clamp(menu.row + delta, 0, kOptionsRowCount - 1);
}

void options_menu_adjust(OptionsMenu& menu, int delta) {
    switch (static_cast<OptionsRow>(menu.row)) {
        case OptionsRow::SpeedType: {
            int index = 0;
            for (int i = 0; i < 3; ++i) {
                if (kSpeedTypeOrder[i] == menu.speed_type) {
                    index = i;
                    break;
                }
            }
            index = ((index + delta) % 3 + 3) % 3;
            menu.speed_type = kSpeedTypeOrder[index];
            break;
        }
        case OptionsRow::SpeedValue: {
            if (menu.speed_type == SpeedModType::XMod) {
                const std::vector<double> values = options_speed_values(SpeedModType::XMod);
                const double current = menu.speed_value();
                std::size_t nearest = 0;
                double nearest_distance = std::fabs(values.front() - current);
                for (std::size_t i = 1; i < values.size(); ++i) {
                    const double distance = std::fabs(values[i] - current);
                    if (distance < nearest_distance) {
                        nearest_distance = distance;
                        nearest = i;
                    }
                }
                const long next =
                    std::clamp(static_cast<long>(nearest) + delta, 0L,
                               static_cast<long>(values.size()) - 1L);
                menu.set_speed_value(values[static_cast<std::size_t>(next)]);
            } else {
                // C/M: no OpenITG increment exists. Step relative to the current
                // value (preserving seeded off-grid values) within the
                // parser-valid [1, 9999] range.
                menu.set_speed_value(menu.speed_value() + static_cast<double>(delta) * kCmStep);
            }
            break;
        }
        case OptionsRow::Scroll:
            menu.scroll_down = !menu.scroll_down;
            break;
        case OptionsRow::Fail:
            menu.fail_enabled = !menu.fail_enabled;
            break;
        case OptionsRow::AssistTick:
            menu.assist_tick = !menu.assist_tick;
            break;
        case OptionsRow::CalibrateOffset:
            // Action row: activation lives in SelectScreen (opens the C5 wizard).
            // Left/Right/Confirm must not mutate the menu.
            break;
        case OptionsRow::RemapInput:
            // Action row: activation lives in SelectScreen (opens the C6 remap
            // screen). Left/Right/Confirm must not mutate the menu.
            break;
        case OptionsRow::Count:
            break;
    }
}

std::string options_row_name(int row) {
    switch (static_cast<OptionsRow>(row)) {
        case OptionsRow::SpeedType:
            return "SPEED TYPE";
        case OptionsRow::SpeedValue:
            return "SPEED";
        case OptionsRow::Scroll:
            return "SCROLL";
        case OptionsRow::Fail:
            return "FAIL";
        case OptionsRow::AssistTick:
            return "ASSIST TICK";
        case OptionsRow::CalibrateOffset:
            return "CALIBRATE OFFSET";
        case OptionsRow::RemapInput:
            return "REMAP INPUT";
        case OptionsRow::Count:
            break;
    }
    return "";
}

std::string options_row_value_text(const OptionsMenu& menu, int row) {
    switch (static_cast<OptionsRow>(row)) {
        case OptionsRow::SpeedType:
            return options_speed_type_name(menu.speed_type);
        case OptionsRow::SpeedValue:
            return format_speed_mod(SpeedMod{menu.speed_type, menu.speed_value()});
        case OptionsRow::Scroll:
            return menu.scroll_down ? "DOWN" : "UP";
        case OptionsRow::Fail:
            return menu.fail_enabled ? "ON" : "OFF";
        case OptionsRow::AssistTick:
            return menu.assist_tick ? "ON" : "OFF";
        case OptionsRow::CalibrateOffset:
            return format_offset(menu.offset_seconds);
        case OptionsRow::RemapInput:
            return ">"; // action row: opens the C6 remapping screen
        case OptionsRow::Count:
            break;
    }
    return "";
}

std::string format_offset(double seconds) {
    std::ostringstream out;
    out << (seconds < 0.0 ? '-' : '+') << std::fixed << std::setprecision(3)
        << std::fabs(seconds) << " s";
    return out.str();
}

} // namespace td
