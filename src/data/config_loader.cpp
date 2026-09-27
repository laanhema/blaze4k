#include "data/config_loader.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

namespace td {
namespace {

using json = nlohmann::json;

constexpr std::uintmax_t kMaxConfigBytes = 1u << 20; // 1 MiB cap for untrusted config

void set_message(std::string* message, const std::string& text) {
    if (message != nullptr) {
        *message = text;
    }
}

void append_warning(std::string& warnings, const std::string& text) {
    if (!warnings.empty()) {
        warnings += "; ";
    }
    warnings += text;
}

// Locale-independent process/instance-unique suffix for atomic-save temp files.
std::uint64_t unique_suffix() {
    static std::atomic<std::uint64_t> counter{0};
#if defined(_WIN32)
    const auto pid = static_cast<std::uint64_t>(::_getpid());
#else
    const auto pid = static_cast<std::uint64_t>(::getpid());
#endif
    return (pid << 20) ^ counter.fetch_add(1, std::memory_order_relaxed);
}

GameConfig config_fallback(std::string* message, ConfigLoadStatus* status, const std::string& reason) {
    set_message(message, "[Config] Warning: " + reason + "; using defaults");
    if (status != nullptr) {
        *status = ConfigLoadStatus::UsedDefaults;
    }
    return GameConfig{};
}

// Reads a finite number and clamps it to [lo, hi]. A missing/null field is left
// at its default; a wrong type or non-finite value warns and keeps the default.
void read_double_field(const json& node, const char* key, double lo, double hi, double& target,
                       std::string& warnings, const char* section) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_number()) {
        append_warning(warnings, std::string(section) + "." + key + " is not a number");
        return;
    }
    const double value = it->get<double>();
    if (!std::isfinite(value)) {
        append_warning(warnings, std::string(section) + "." + key + " is not finite");
        return;
    }
    if (value < lo || value > hi) {
        append_warning(warnings, std::string(section) + "." + key + " out of range; clamped");
    }
    target = std::clamp(value, lo, hi);
}

// Reads an integral number and clamps it to [lo, hi]. Non-integral, non-finite,
// or wrong-typed values warn and keep the default.
void read_int_field(const json& node, const char* key, int lo, int hi, int& target,
                    std::string& warnings, const char* section) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_number()) {
        append_warning(warnings, std::string(section) + "." + key + " is not a number");
        return;
    }
    const double value = it->get<double>();
    if (!std::isfinite(value) || value != std::trunc(value)) {
        append_warning(warnings, std::string(section) + "." + key + " is not an integer");
        return;
    }
    if (value < static_cast<double>(lo) || value > static_cast<double>(hi)) {
        append_warning(warnings, std::string(section) + "." + key + " out of range; clamped");
    }
    target = static_cast<int>(std::clamp(value, static_cast<double>(lo), static_cast<double>(hi)));
}

void read_bool_field(const json& node, const char* key, bool& target, std::string& warnings,
                     const char* section) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_boolean()) {
        append_warning(warnings, std::string(section) + "." + key + " is not a boolean");
        return;
    }
    target = it->get<bool>();
}

void read_string_field(const json& node, const char* key, std::string& target, std::string& warnings,
                       const char* section) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_string()) {
        append_warning(warnings, std::string(section) + "." + key + " is not a string");
        return;
    }
    target = it->get<std::string>();
}

const json* section_object(const json& document, const char* key, std::string& warnings) {
    auto it = document.find(key);
    if (it == document.end() || it->is_null()) {
        return nullptr;
    }
    if (!it->is_object()) {
        append_warning(warnings, std::string("'") + key + "' is not an object");
        return nullptr;
    }
    return &(*it);
}

void read_bindings(const json& node, const char* key, std::vector<InputBinding>& target,
                   std::string& warnings) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_object()) {
        append_warning(warnings, std::string("input.") + key + " is not an object");
        return;
    }
    // A present bindings map fully replaces the defaults, so save/load is an
    // exact round-trip for any config (bindings are opaque to C2).
    target.clear();
    for (auto entry = it->begin(); entry != it->end(); ++entry) {
        const std::string action = entry.key();
        if (action.empty()) {
            append_warning(warnings, std::string("input.") + key + " has an empty action name");
            continue;
        }
        const json& value = entry.value();
        if (!value.is_array()) {
            append_warning(warnings,
                           std::string("input.") + key + "." + action + " is not an array");
            continue;
        }
        std::vector<std::string> names;
        for (const json& item : value) {
            if (!item.is_string() || item.get<std::string>().empty()) {
                append_warning(warnings, std::string("input.") + key + "." + action +
                                             " contains an invalid binding name");
                continue;
            }
            names.push_back(item.get<std::string>());
        }
        target.emplace_back(action, std::move(names));
    }
}

void read_video(const json& document, VideoSettings& target, std::string& warnings) {
    const json* node = section_object(document, "video", warnings);
    if (node == nullptr) {
        return;
    }
    read_int_field(*node, "width", kMinWindowDimension, kMaxWindowDimension, target.width, warnings,
                   "video");
    read_int_field(*node, "height", kMinWindowDimension, kMaxWindowDimension, target.height, warnings,
                   "video");
    read_bool_field(*node, "vsync", target.vsync, warnings, "video");
    read_bool_field(*node, "fullscreen", target.fullscreen, warnings, "video");
}

void read_audio(const json& document, AudioSettings& target, std::string& warnings) {
    const json* node = section_object(document, "audio", warnings);
    if (node == nullptr) {
        return;
    }
    read_double_field(*node, "master_volume", 0.0, 1.0, target.master_volume, warnings, "audio");
    read_double_field(*node, "music_volume", 0.0, 1.0, target.music_volume, warnings, "audio");
    read_double_field(*node, "preview_volume", 0.0, 1.0, target.preview_volume, warnings, "audio");
    read_double_field(*node, "ui_volume", 0.0, 1.0, target.ui_volume, warnings, "audio");
}

void read_offset(const json& document, OffsetSettings& target, std::string& warnings) {
    const json* node = section_object(document, "offset", warnings);
    if (node == nullptr) {
        return;
    }
    read_double_field(*node, "global_offset_seconds", -3600.0, 3600.0, target.global_offset_seconds,
                      warnings, "offset");
}

void read_gameplay(const json& document, GameplaySettings& target, std::string& warnings) {
    const json* node = section_object(document, "gameplay", warnings);
    if (node == nullptr) {
        return;
    }
    // Capture the compiled defaults so an invalid enum/string can fall back to
    // them (the documented "invalid field keeps its default" contract) instead
    // of being loaded and re-persisted.
    const std::string default_speed_mod = target.speed_mod;
    const std::string default_scroll = target.scroll;
    read_string_field(*node, "speed_mod", target.speed_mod, warnings, "gameplay");
    read_string_field(*node, "scroll", target.scroll, warnings, "gameplay");
    if (target.speed_mod.empty()) {
        append_warning(warnings, "gameplay.speed_mod is empty; keeping default");
        target.speed_mod = default_speed_mod;
    }
    if (target.scroll != "up" && target.scroll != "down") {
        append_warning(warnings, "gameplay.scroll must be 'up' or 'down'; keeping default");
        target.scroll = default_scroll;
    }
    read_bool_field(*node, "fail_enabled", target.fail_enabled, warnings, "gameplay");
}

void read_input(const json& document, InputSettings& target, std::string& warnings) {
    const json* node = section_object(document, "input", warnings);
    if (node == nullptr) {
        return;
    }
    read_bindings(*node, "key_bindings", target.key_bindings, warnings);
    read_bindings(*node, "gamepad_bindings", target.gamepad_bindings, warnings);
}

} // namespace

// C6: these lists are the single default-binding authority. The names are the
// canonical strings SDL 3.2.8 round-trips (SDL_GetKeyName/SDL_GetKeyFromName and
// SDL_GetGamepadStringForButton/SDL_GetGamepadButtonFromString), so InputManager
// derives its runtime tables from the same set it persists.
std::vector<InputBinding> default_key_bindings() {
    return {
        {"Left", {"Left", "D"}},
        {"Down", {"Down", "F"}},
        {"Up", {"Up", "J"}},
        {"Right", {"Right", "K"}},
        {"Confirm", {"Return", "Keypad Enter"}},
        {"Back", {"Escape"}},
        {"Options", {"Tab"}},
    };
}

std::vector<InputBinding> default_gamepad_bindings() {
    return {
        {"Left", {"dpleft", "x"}},
        {"Down", {"dpdown", "a"}},
        {"Up", {"dpup", "y"}},
        {"Right", {"dpright", "b"}},
        {"Confirm", {"start"}},
        {"Back", {"back"}},
        {"Options", {"leftshoulder", "rightshoulder"}},
    };
}

bool validate_game_config(const GameConfig& config, std::string* error) {
    auto fail = [error](const std::string& text) {
        if (error != nullptr) {
            *error = text;
        }
        return false;
    };

    if (config.video.width < kMinWindowDimension || config.video.width > kMaxWindowDimension) {
        return fail("video.width out of range");
    }
    if (config.video.height < kMinWindowDimension || config.video.height > kMaxWindowDimension) {
        return fail("video.height out of range");
    }
    if (!(config.audio.master_volume >= 0.0 && config.audio.master_volume <= 1.0)) {
        return fail("audio.master_volume out of range");
    }
    if (!(config.audio.music_volume >= 0.0 && config.audio.music_volume <= 1.0)) {
        return fail("audio.music_volume out of range");
    }
    if (!(config.audio.preview_volume >= 0.0 && config.audio.preview_volume <= 1.0)) {
        return fail("audio.preview_volume out of range");
    }
    if (!(config.audio.ui_volume >= 0.0 && config.audio.ui_volume <= 1.0)) {
        return fail("audio.ui_volume out of range");
    }
    if (!std::isfinite(config.offset.global_offset_seconds)) {
        return fail("offset.global_offset_seconds is not finite");
    }
    if (config.gameplay.speed_mod.empty()) {
        return fail("gameplay.speed_mod is empty");
    }
    if (config.gameplay.scroll != "up" && config.gameplay.scroll != "down") {
        return fail("gameplay.scroll must be 'up' or 'down'");
    }
    return true;
}

GameConfig load_config(const std::filesystem::path& path, std::string* message,
                       ConfigLoadStatus* status) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return config_fallback(message, status, "file '" + path.string() + "' not found");
    }

    const std::uintmax_t file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return config_fallback(message, status,
                               "could not determine size of file '" + path.string() + "'");
    }
    if (file_size > kMaxConfigBytes) {
        return config_fallback(message, status, "file '" + path.string() + "' exceeds the size cap");
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return config_fallback(message, status, "could not open file '" + path.string() + "'");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return config_fallback(message, status, "failed to read file '" + path.string() + "'");
    }

    json document = json::parse(buffer.str(), nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return config_fallback(message, status, "malformed JSON in '" + path.string() + "'");
    }

    GameConfig config; // documented defaults, including the input binding defaults
    std::string warnings;
    read_video(document, config.video, warnings);
    read_audio(document, config.audio, warnings);
    read_offset(document, config.offset, warnings);
    read_gameplay(document, config.gameplay, warnings);
    read_input(document, config.input, warnings);

    std::string error;
    if (!validate_game_config(config, &error)) {
        append_warning(warnings, error);
    }

    std::string text = "[Config] Loaded '" + path.string() + "'";
    if (!warnings.empty()) {
        text += " with warnings: " + warnings;
    }
    set_message(message, text);
    if (status != nullptr) {
        *status = ConfigLoadStatus::LoadedFromFile;
    }
    return config;
}

bool save_config(const std::filesystem::path& path, const GameConfig& config,
                 std::string* message) {
    json key_bindings = json::object();
    for (const InputBinding& binding : config.input.key_bindings) {
        key_bindings[binding.first] = binding.second;
    }
    json gamepad_bindings = json::object();
    for (const InputBinding& binding : config.input.gamepad_bindings) {
        gamepad_bindings[binding.first] = binding.second;
    }

    json document;
    document["version"] = config.version;
    document["video"] = {
        {"width", config.video.width},
        {"height", config.video.height},
        {"vsync", config.video.vsync},
        {"fullscreen", config.video.fullscreen},
    };
    document["audio"] = {
        {"master_volume", config.audio.master_volume},
        {"music_volume", config.audio.music_volume},
        {"preview_volume", config.audio.preview_volume},
        {"ui_volume", config.audio.ui_volume},
    };
    document["offset"] = {{"global_offset_seconds", config.offset.global_offset_seconds}};
    document["gameplay"] = {
        {"speed_mod", config.gameplay.speed_mod},
        {"scroll", config.gameplay.scroll},
        {"fail_enabled", config.gameplay.fail_enabled},
    };
    document["input"] = {
        {"key_bindings", key_bindings},
        {"gamepad_bindings", gamepad_bindings},
    };

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            set_message(message, "[Config] Warning: could not create directory for '" + path.string() +
                                     "': " + ec.message());
            return false;
        }
    }

    std::filesystem::path temp_path = path;
    temp_path += ".tmp." + std::to_string(unique_suffix());

    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            set_message(message, "[Config] Warning: could not open '" + temp_path.string() +
                                     "' for writing");
            return false;
        }
        out << document.dump(2) << "\n";
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(temp_path, ec);
            set_message(message, "[Config] Warning: failed to write '" + temp_path.string() + "'");
            return false;
        }
    }

    // std::filesystem::rename replaces an existing regular file (on Windows the
    // MSVC implementation maps this to MoveFileEx with REPLACE_EXISTING), and
    // the unique temp name avoids collisions between concurrent writers.
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        set_message(message, "[Config] Warning: could not replace '" + path.string() +
                                 "': " + ec.message());
        return false;
    }

    set_message(message, "[Config] Saved '" + path.string() + "'");
    return true;
}

} // namespace td
