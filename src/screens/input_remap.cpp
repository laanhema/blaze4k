#include "screens/input_remap.hpp"

#include <algorithm>

namespace blaze4k {

namespace {

// Appends one row per non-empty name of each default action, overriding that
// action's names with the matching entry from `config_bindings` when present.
void append_rows(std::vector<RemapRow>& rows, const std::vector<InputBinding>& defaults,
                 const std::vector<InputBinding>& config_bindings, DeviceType device) {
    for (const InputBinding& def : defaults) {
        const std::vector<std::string>* names = &def.second;
        for (const InputBinding& override_binding : config_bindings) {
            if (override_binding.first == def.first && !override_binding.second.empty()) {
                names = &override_binding.second;
                break;
            }
        }
        GameAction action = GameAction::None;
        const GameAction known[] = {
            GameAction::Left,    GameAction::Down,   GameAction::Up,      GameAction::Right,
            GameAction::Confirm, GameAction::Back,   GameAction::Options,
        };
        bool found = false;
        for (GameAction candidate : known) {
            if (action_to_string(candidate) == def.first) {
                action = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }
        for (const std::string& name : *names) {
            if (name.empty()) {
                continue;
            }
            rows.push_back(RemapRow{action, device, name});
        }
    }
}

} // namespace

InputRemapModel input_remap_from_config(const GameConfig& config) {
    InputRemapModel model;
    append_rows(model.rows, default_key_bindings(), config.input.key_bindings,
                DeviceType::Keyboard);
    append_rows(model.rows, default_gamepad_bindings(), config.input.gamepad_bindings,
                DeviceType::Gamepad);
    return model;
}

void input_remap_move_row(InputRemapModel& model, int delta) {
    if (model.rows.empty()) {
        model.row = 0;
        return;
    }
    model.row = std::clamp(model.row + delta, 0, static_cast<int>(model.rows.size()) - 1);
}

void input_remap_set_row(InputRemapModel& model, int row) {
    if (model.rows.empty()) {
        model.row = 0;
        return;
    }
    model.row = std::clamp(row, 0, static_cast<int>(model.rows.size()) - 1);
}

RemapStatus input_remap_assign(InputRemapModel& model, const std::string& name) {
    if (name.empty() || model.rows.empty()) {
        return RemapStatus::Unchanged;
    }
    const RemapRow selected = model.rows[static_cast<std::size_t>(model.row)];

    for (const RemapRow& other : model.rows) {
        if (other.device == selected.device && other.name == name &&
            other.action != selected.action) {
            model.message = "IN USE: " + remap_action_name(other.action);
            return RemapStatus::Conflict;
        }
    }

    for (const RemapRow& other : model.rows) {
        if (other.device == selected.device && other.action == selected.action &&
            other.name == name) {
            return RemapStatus::Unchanged;
        }
    }

    model.rows[static_cast<std::size_t>(model.row)].name = name;
    model.message = "BOUND TO " + remap_action_name(selected.action);
    return RemapStatus::Replaced;
}

void input_remap_reset(InputRemapModel& model) {
    GameConfig defaults;
    InputRemapModel reset = input_remap_from_config(defaults);
    model.rows = std::move(reset.rows);
    input_remap_set_row(model, model.row);
    model.capturing = false;
    model.message = "DEFAULTS RESTORED";
}

void input_remap_apply(const InputRemapModel& model, InputSettings& settings) {
    settings.key_bindings.clear();
    settings.gamepad_bindings.clear();

    std::vector<InputBinding>* target = &settings.key_bindings;
    for (int pass = 0; pass < 2; ++pass) {
        const DeviceType device = pass == 0 ? DeviceType::Keyboard : DeviceType::Gamepad;
        target = pass == 0 ? &settings.key_bindings : &settings.gamepad_bindings;
        for (const RemapRow& row : model.rows) {
            if (row.device != device || row.name.empty()) {
                continue;
            }
            const std::string action_name = std::string(action_to_string(row.action));
            auto it = std::find_if(target->begin(), target->end(),
                                   [&action_name](const InputBinding& binding) {
                                       return binding.first == action_name;
                                   });
            if (it == target->end()) {
                target->emplace_back(action_name, std::vector<std::string>{row.name});
            } else {
                it->second.push_back(row.name);
            }
        }
    }
}

std::string_view remap_action_label(GameAction action) {
    switch (action) {
        case GameAction::Left:
            return "LEFT";
        case GameAction::Down:
            return "DOWN";
        case GameAction::Up:
            return "UP";
        case GameAction::Right:
            return "RIGHT";
        case GameAction::Confirm:
            return "CONFIRM";
        case GameAction::Back:
            return "BACK";
        case GameAction::Options:
            return "OPTIONS";
        default:
            return "NONE";
    }
}

std::string_view remap_device_label(DeviceType device) {
    return device == DeviceType::Keyboard ? "KEYBOARD" : "PAD";
}

std::string remap_action_name(GameAction action) {
    return std::string(remap_action_label(action));
}

std::string remap_device_name(DeviceType device) {
    return std::string(remap_device_label(device));
}

std::string remap_row_value_text(const InputRemapModel& model, int row) {
    if (row < 0 || row >= static_cast<int>(model.rows.size())) {
        return "";
    }
    if (model.capturing && row == model.row) {
        return "<PRESS>";
    }
    return model.rows[static_cast<std::size_t>(row)].name;
}

} // namespace blaze4k
