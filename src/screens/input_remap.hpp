#pragma once

#include <string>
#include <vector>

#include "data/config.hpp"
#include "input/input_event.hpp"

namespace blaze4k {

// One remappable binding slot: a single (action, device, name) triple. The C6
// UI shows one row per slot so a default that maps two keys to one action (e.g.
// Left = "Left" + "D") keeps both bindings.
struct RemapRow {
    GameAction action = GameAction::None;
    DeviceType device = DeviceType::Keyboard;
    std::string name;
};

[[nodiscard]] inline bool operator==(const RemapRow& a, const RemapRow& b) {
    return a.action == b.action && a.device == b.device && a.name == b.name;
}

[[nodiscard]] inline bool operator!=(const RemapRow& a, const RemapRow& b) { return !(a == b); }

enum class RemapStatus {
    Bound,     // a fresh name was assigned
    Replaced,  // the selected slot's previous name was replaced
    Conflict,  // the name is already used by a different action (rejected)
    Unchanged, // the same device+name already belonged to this action
};

// Pure, SDL/GL/audio/clock-free state for the C6 remapping screen (mirrors the
// C4 OptionsMenu seam). All conflict/reset/apply logic is unit-tested headless.
struct InputRemapModel {
    std::vector<RemapRow> rows;
    int row = 0;
    bool capturing = false;
    std::string message; // transient feedback ("BOUND TO LEFT", "IN USE: CONFIRM")
};

// Builds the row list from the effective config: the compiled default lists are
// the base, and any action present in `config.input` replaces that action's
// names. Keyboard rows come first, then pad rows. Empty name lists are skipped.
[[nodiscard]] InputRemapModel input_remap_from_config(const GameConfig& config);

// Moves the highlighted row, clamped to [0, rows.size()-1].
void input_remap_move_row(InputRemapModel& model, int delta);

// Sets the highlighted row, clamped.
void input_remap_set_row(InputRemapModel& model, int row);

// Assigns `name` to the selected row's device. Returns Conflict (with a
// non-empty `message`, no change) when another row with a different action
// already holds that (device, name); Unchanged when the same action already
// holds it; Bound/Replaced otherwise.
RemapStatus input_remap_assign(InputRemapModel& model, const std::string& name);

// Rebuilds the rows from the compiled defaults, restoring all default bindings.
void input_remap_reset(InputRemapModel& model);

// Groups the rows back into the persisted `action -> [names]` shape, preserving
// per-action name order.
void input_remap_apply(const InputRemapModel& model, InputSettings& settings);

// Display helpers (pure).
[[nodiscard]] std::string remap_action_name(GameAction action);
[[nodiscard]] std::string remap_device_name(DeviceType device);
[[nodiscard]] std::string remap_row_value_text(const InputRemapModel& model, int row);

} // namespace blaze4k
