#pragma once

#include <string>
#include <vector>

#include "gameplay/speed_mod.hpp"

namespace blaze4k {

struct GameConfig;

// Rows of the C4 options overlay. This enum plus the `options_menu_adjust`
// dispatch are the seam C5 (offset wizard) and C6 (input remapping) extend with
// new entries; neither is implemented here.
enum class OptionsRow : int {
    SpeedType = 0, // XMOD / CMOD / MMOD
    SpeedValue,    // numeric, type-specific
    Scroll,        // UP / DOWN
    Fail,          // ON / OFF
    AssistTick,    // ON / OFF: tick on every note row (timing practice)
    CalibrateOffset, // action row: opens the C5 calibration wizard
    RemapInput,      // action row: opens the C6 input remapping screen
    Count,
};

inline constexpr int kOptionsRowCount = static_cast<int>(OptionsRow::Count);

// Pure, SDL/GL/audio/clock-free state for the options overlay. The model keeps a
// remembered value per speed type so switching type never yields nonsense like
// "C1.5"; the active type's slot is what gameplay consumes.
struct OptionsMenu {
    SpeedModType speed_type = SpeedModType::XMod;
    double x_value = 1.0;
    double c_value = 450.0;
    double m_value = 600.0;
    bool scroll_down = false;
    bool fail_enabled = true;
    bool assist_tick = false;
    int row = static_cast<int>(OptionsRow::SpeedType);

    // Display-only mirror of the persisted offset. `options_menu_apply` must NOT
    // write this back; the C5 wizard owns the config offset field.
    double offset_seconds = 0.0;

    // Active type's slot.
    [[nodiscard]] double speed_value() const;
    // Clamps to the active type's documented range and stores in its slot.
    void set_speed_value(double value);
};

// Seeds the model from the persisted config. A malformed `speed_mod` leaves all
// three slots at their defaults (mirrors gameplay_options_from_config).
[[nodiscard]] OptionsMenu options_menu_from_config(const GameConfig& config);

// Writes the model back into the shared config, which main persists on clean
// exit. `speed_mod` is emitted in the "Nx"/"Cccc"/"Mmmm" forms parse_speed_mod
// accepts.
void options_menu_apply(const OptionsMenu& menu, GameConfig& config);

// Moves the highlighted row, clamped to [0, kOptionsRowCount-1].
void options_menu_move_row(OptionsMenu& menu, int delta);

// Advances the highlighted row's value by `delta` (row-specific). The speed
// type cycles X->C->M; the X-mod value wraps at both ends (8x <-> 1x); C/M
// values clamp to [1, 9999]; toggle rows flip.
void options_menu_adjust(OptionsMenu& menu, int delta);

// The ordered grid the menu steps through for X-mod (OpenITG menu values
// extended to 8x; see options_menu.cpp for provenance). For C/M it returns the
// parser-valid range endpoints [1, 9999]: OpenITG defines no C/M increment, so
// those rows step relative to the current value and use this only for
// clamping. Data-driven.
[[nodiscard]] std::vector<double> options_speed_values(SpeedModType type);

// Display strings for the overlay.
[[nodiscard]] std::string options_speed_type_name(SpeedModType type);
[[nodiscard]] std::string options_row_name(int row);
[[nodiscard]] std::string options_row_value_text(const OptionsMenu& menu, int row);

// Signed seconds, e.g. "+0.023 s" / "-0.011 s". Pure.
[[nodiscard]] std::string format_offset(double seconds);

} // namespace blaze4k
