#pragma once

#include "gameplay/note_field.hpp"

namespace blaze4k {

struct GameConfig;

// Options describing one gameplay run, independent of any screen or window.
// Extracted from `GameplayView` so option mapping is testable without a view.
struct GameplayOptions {
    SpeedMod speed{};
    ScrollDirection scroll = ScrollDirection::Up;
    double global_offset_seconds = 0.0;
    bool fail_enabled = true; // false = Fail-Off (song continues to the end)
    bool assist_tick = false; // tick sound on every tap/hold/roll row (timing practice)
};

// Pure mapping from the persisted player config (C2) to gameplay options.
// An unparseable `speed_mod` falls back to X-mod 1x with a warning.
[[nodiscard]] GameplayOptions gameplay_options_from_config(const GameConfig& config);

} // namespace blaze4k
