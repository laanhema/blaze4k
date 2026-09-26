#include "data/judgment_constants_loader.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace td {
namespace {

using json = nlohmann::json;

void set_message(std::string* message, const std::string& text) {
    if (message != nullptr) {
        *message = text;
    }
}

constexpr std::uintmax_t kMaxConfigBytes = 1u << 20; // 1 MiB cap for untrusted config

JudgmentConstants fallback(std::string* message, ConstantsLoadStatus* status,
                           const std::string& reason) {
    set_message(message, "[JudgmentConstants] Warning: " + reason + "; using compiled defaults");
    if (status != nullptr) {
        *status = ConstantsLoadStatus::UsedDefaults;
    }
    return JudgmentConstants::compiled_defaults();
}

template <typename T>
void override_number(const json& node, const char* key, T& target) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return;
    }
    if (!it->is_number()) {
        throw std::runtime_error(std::string("field '") + key + "' must be a number");
    }
    if constexpr (std::is_integral_v<T>) {
        bool in_range = false;
        if (it->is_number_float()) {
            const double value = it->get<double>();
            in_range = std::isfinite(value) && value == std::trunc(value) &&
                       value >= static_cast<double>(std::numeric_limits<T>::lowest()) &&
                       value <= static_cast<double>(std::numeric_limits<T>::max());
            if (in_range) {
                target = static_cast<T>(value);
            }
        } else if (it->is_number_unsigned()) {
            const std::uint64_t value = it->get<std::uint64_t>();
            in_range = value <= static_cast<std::uint64_t>(std::numeric_limits<T>::max());
            if (in_range) {
                target = static_cast<T>(value);
            }
        } else {
            const std::int64_t value = it->get<std::int64_t>();
            in_range = value >= static_cast<std::int64_t>(std::numeric_limits<T>::lowest()) &&
                       value <= static_cast<std::int64_t>(std::numeric_limits<T>::max());
            if (in_range) {
                target = static_cast<T>(value);
            }
        }
        if (!in_range) {
            throw std::runtime_error(std::string("field '") + key + "' must be an in-range integer");
        }
    } else {
        target = it->get<T>();
    }
}

void override_weights(const json& node, Weights& target) {
    override_number(node, "fantastic", target.fantastic);
    override_number(node, "excellent", target.excellent);
    override_number(node, "great", target.great);
    override_number(node, "decent", target.decent);
    override_number(node, "way_off", target.way_off);
    override_number(node, "miss", target.miss);
    override_number(node, "hit_mine", target.hit_mine);
    override_number(node, "hold_ok", target.hold_ok);
    override_number(node, "hold_ng", target.hold_ng);
}

void override_windows(const json& node, TimingWindows& target) {
    override_number(node, "fantastic", target.fantastic);
    override_number(node, "excellent", target.excellent);
    override_number(node, "great", target.great);
    override_number(node, "decent", target.decent);
    override_number(node, "way_off", target.way_off);
    override_number(node, "hit_mine", target.hit_mine);
    override_number(node, "hold_ok", target.hold_ok);
    override_number(node, "hold_roll", target.hold_roll);
    override_number(node, "judge_window_scale", target.judge_window_scale);
    override_number(node, "judge_window_add", target.judge_window_add);
}

void override_life(const json& node, LifeDeltas& target) {
    override_number(node, "fantastic", target.fantastic);
    override_number(node, "excellent", target.excellent);
    override_number(node, "great", target.great);
    override_number(node, "decent", target.decent);
    override_number(node, "way_off", target.way_off);
    override_number(node, "miss", target.miss);
    override_number(node, "hit_mine", target.hit_mine);
    override_number(node, "hold_ok", target.hold_ok);
    override_number(node, "hold_ng", target.hold_ng);
    override_number(node, "hot_downgrade", target.hot_downgrade);
    override_number(node, "regen_combo_after_miss", target.regen_combo_after_miss);
    override_number(node, "regen_combo_after_fail", target.regen_combo_after_fail);
    override_number(node, "max_regen_combo_after_miss", target.max_regen_combo_after_miss);
    override_number(node, "max_regen_combo_after_fail", target.max_regen_combo_after_fail);
    auto it = node.find("merciful_drain");
    if (it != node.end() && !it->is_null()) {
        if (!it->is_boolean()) {
            throw std::runtime_error("field 'merciful_drain' must be a boolean");
        }
        target.merciful_drain = it->get<bool>();
    }
}

void override_grade_tiers(const json& node, std::array<GradeTier, 17>& target) {
    if (!node.is_array()) {
        throw std::runtime_error("field 'grade_tiers' must be an array");
    }
    if (node.size() != target.size()) {
        throw std::runtime_error("field 'grade_tiers' must contain exactly 17 entries");
    }
    for (std::size_t i = 0; i < target.size(); ++i) {
        const json& entry = node.at(i);
        if (!entry.is_object()) {
            throw std::runtime_error("grade tier entry must be an object");
        }
        auto percent_it = entry.find("min_percent");
        if (percent_it == entry.end() || !percent_it->is_number()) {
            throw std::runtime_error("grade tier 'min_percent' must be a number");
        }
        // `label` is compiled-in presentation metadata and is deliberately not
        // configurable; any `label` in JSON is ignored (see the seed file note).
        target[i].min_percent = percent_it->get<double>();
    }
}

} // namespace

JudgmentConstants load_judgment_constants(const std::filesystem::path& path, std::string* message,
                                          ConstantsLoadStatus* status) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return fallback(message, status, "file '" + path.string() + "' not found");
    }

    const std::uintmax_t file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return fallback(message, status, "could not determine size of file '" + path.string() + "'");
    }
    if (file_size > kMaxConfigBytes) {
        return fallback(message, status, "file '" + path.string() + "' exceeds the size cap");
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return fallback(message, status, "could not open file '" + path.string() + "'");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return fallback(message, status, "failed to read file '" + path.string() + "'");
    }

    json document = json::parse(buffer.str(), nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return fallback(message, status, "malformed JSON in '" + path.string() + "'");
    }

    JudgmentConstants constants = JudgmentConstants::compiled_defaults();
    try {
        auto windows_it = document.find("windows_seconds");
        if (windows_it != document.end() && !windows_it->is_null()) {
            if (!windows_it->is_object()) {
                throw std::runtime_error("field 'windows_seconds' must be an object");
            }
            override_windows(*windows_it, constants.windows);
        }
        auto dp_it = document.find("dp_weights");
        if (dp_it != document.end() && !dp_it->is_null()) {
            if (!dp_it->is_object()) {
                throw std::runtime_error("field 'dp_weights' must be an object");
            }
            override_weights(*dp_it, constants.dp_weights);
        }
        auto grade_w_it = document.find("grade_weights");
        if (grade_w_it != document.end() && !grade_w_it->is_null()) {
            if (!grade_w_it->is_object()) {
                throw std::runtime_error("field 'grade_weights' must be an object");
            }
            override_weights(*grade_w_it, constants.grade_weights);
        }
        auto life_it = document.find("life_deltas");
        if (life_it != document.end() && !life_it->is_null()) {
            if (!life_it->is_object()) {
                throw std::runtime_error("field 'life_deltas' must be an object");
            }
            override_life(*life_it, constants.life);
        }
        auto tiers_it = document.find("grade_tiers");
        if (tiers_it != document.end() && !tiers_it->is_null()) {
            override_grade_tiers(*tiers_it, constants.grade_tiers);
        }
    } catch (const std::exception& ex) {
        return fallback(message, status,
                        std::string("invalid field in '") + path.string() + "': " + ex.what());
    }

    std::string error;
    if (!constants.validate(&error)) {
        return fallback(message, status, "invalid values in '" + path.string() + "': " + error);
    }

    set_message(message, "[JudgmentConstants] Loaded '" + path.string() + "'");
    if (status != nullptr) {
        *status = ConstantsLoadStatus::LoadedFromFile;
    }
    return constants;
}

JudgmentConstants load_judgment_constants_from_candidates(
    const std::vector<std::filesystem::path>& candidates, std::string* message,
    ConstantsLoadStatus* status) {
    std::error_code ec;
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) &&
            std::filesystem::is_regular_file(candidate, ec)) {
            return load_judgment_constants(candidate, message, status);
        }
    }
    return fallback(message, status, "no constants file found among configured candidates");
}

} // namespace td
