#pragma once

#include <string_view>
#include "chart/timing_data.hpp"

namespace blaze4k {

// Scroll speed modifiers, mirroring OpenITG's PlayerOptions semantics.
enum class SpeedModType {
    CMod, // constant BPM: time spacing
    XMod, // constant multiplier: beat spacing
    MMod, // maximum BPM: beat spacing, resolved to an X-mod
};

struct SpeedMod {
    SpeedModType type = SpeedModType::XMod;
    double value = 1.0; // c-BPM, x multiplier, or m-BPM depending on `type`
};

// Parses "Nx", "cN", or "mN" (case-insensitive). Values must be positive and
// finite. Returns false on any malformed or non-positive input.
[[nodiscard]] bool parse_speed_mod(std::string_view text, SpeedMod& out);

// Largest finite positive BPM over the chart's timing; 0.0 if none are valid.
[[nodiscard]] double max_chart_bpm(const TimingData& timing);

// Resolves the X-mod-equivalent multiplier:
//   XMod -> value
//   MMod -> value / max_chart_bpm (falls back to 1.0 with a warning if no BPM)
//   CMod -> value (not used by the layout path, which uses time spacing directly)
[[nodiscard]] double resolve_x_speed(const SpeedMod& mod, const TimingData& timing);

} // namespace blaze4k
