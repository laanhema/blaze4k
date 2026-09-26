#include "gameplay/gameplay_options.hpp"

#include <iostream>

#include "data/config.hpp"

namespace td {

GameplayOptions gameplay_options_from_config(const GameConfig& config) {
    GameplayOptions options;

    if (!parse_speed_mod(config.gameplay.speed_mod, options.speed)) {
        std::cerr << "[GameplayOptions] Invalid speed_mod '" << config.gameplay.speed_mod
                  << "'; defaulting to X-mod 1x\n";
        options.speed = SpeedMod{};
    }

    options.scroll =
        (config.gameplay.scroll == "down") ? ScrollDirection::Down : ScrollDirection::Up;
    options.global_offset_seconds = config.offset.global_offset_seconds;
    options.fail_enabled = config.gameplay.fail_enabled;
    return options;
}

} // namespace td
