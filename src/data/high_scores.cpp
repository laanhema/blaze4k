#include "data/high_scores.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
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

constexpr std::uintmax_t kMaxScoresBytes = 1u << 20; // 1 MiB cap for untrusted input

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

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

// Locale-independent double formatting for persisted chart-key tokens
// (std::to_string uses the active C locale's decimal separator).
std::string format_double(double value) {
    char buffer[64];
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}


void fnv_update(std::uint64_t& hash, const std::string& text) {
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }
}

std::string hex64(std::uint64_t value) {
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return std::string(buffer);
}

std::string to_lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

HighScores scores_fallback(std::string* message, ScoresLoadStatus* status, const std::string& reason) {
    set_message(message, "[Scores] Warning: " + reason + "; using empty score table");
    if (status != nullptr) {
        *status = ScoresLoadStatus::UsedDefaults;
    }
    return HighScores{};
}

bool read_integer(const json& node, const char* key, std::int64_t& target) {
    auto it = node.find(key);
    if (it == node.end() || it->is_null()) {
        return true; // optional; keep default
    }
    if (!it->is_number()) {
        return false;
    }
    const double value = it->get<double>();
    if (!std::isfinite(value) || value != std::trunc(value) ||
        value < static_cast<double>(std::numeric_limits<std::int64_t>::lowest()) ||
        value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }
    target = static_cast<std::int64_t>(value);
    return true;
}

} // namespace

std::string make_chart_key(const Song& song, const Chart& chart) {
    std::uint64_t note_hash = kFnvOffset;
    for (const Note& note : chart.notes) {
        const std::string token = format_double(note.beat) + "|" +
                                  std::to_string(static_cast<int>(note.type)) + "|" +
                                  std::to_string(note.column) + ";";
        fnv_update(note_hash, token);
    }

    const std::string filename =
        std::filesystem::path(song.simfile_path).filename().string();

    std::string canonical;
    canonical += to_lower(filename);
    canonical += '\x1f';
    canonical += to_lower(song.metadata.title);
    canonical += '\x1f';
    canonical += to_lower(song.metadata.artist);
    canonical += '\x1f';
    canonical += to_lower(chart.steps_type);
    canonical += '\x1f';
    canonical += to_lower(chart.difficulty);
    canonical += '\x1f';
    canonical += std::to_string(chart.meter);
    canonical += '\x1f';
    canonical += hex64(note_hash);

    std::uint64_t hash = kFnvOffset;
    fnv_update(hash, canonical);
    return hex64(hash);
}

HighScores load_high_scores(const std::filesystem::path& path, std::string* message,
                            ScoresLoadStatus* status) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return scores_fallback(message, status, "file '" + path.string() + "' not found");
    }

    const std::uintmax_t file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return scores_fallback(message, status,
                               "could not determine size of file '" + path.string() + "'");
    }
    if (file_size > kMaxScoresBytes) {
        return scores_fallback(message, status, "file '" + path.string() + "' exceeds the size cap");
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return scores_fallback(message, status, "could not open file '" + path.string() + "'");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return scores_fallback(message, status, "failed to read file '" + path.string() + "'");
    }

    json document = json::parse(buffer.str(), nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return scores_fallback(message, status, "malformed JSON in '" + path.string() + "'");
    }

    HighScores result;
    std::string warnings;

    auto scores_it = document.find("scores");
    if (scores_it == document.end() || scores_it->is_null()) {
        // Empty document is valid: no recorded scores yet.
        set_message(message, "[Scores] Loaded '" + path.string() + "' (no scores recorded)");
        if (status != nullptr) {
            *status = ScoresLoadStatus::LoadedFromFile;
        }
        return result;
    }
    if (!scores_it->is_object()) {
        return scores_fallback(message, status, "'scores' is not an object in '" + path.string() + "'");
    }

    for (auto entry = scores_it->begin(); entry != scores_it->end(); ++entry) {
        const std::string chart_key = entry.key();
        const json& value = entry.value();
        if (chart_key.empty()) {
            append_warning(warnings, "empty chart key ignored");
            continue;
        }
        if (!value.is_object()) {
            append_warning(warnings, "record '" + chart_key + "' is not an object");
            continue;
        }

        auto grade_it = value.find("grade");
        if (grade_it == value.end() || !grade_it->is_string() || grade_it->get<std::string>().empty()) {
            append_warning(warnings, "record '" + chart_key + "' has an invalid grade");
            continue;
        }
        auto percent_it = value.find("percent");
        if (percent_it == value.end() || !percent_it->is_number()) {
            append_warning(warnings, "record '" + chart_key + "' has an invalid percent");
            continue;
        }
        const double percent = percent_it->get<double>();
        if (!std::isfinite(percent) || percent < 0.0 || percent > 1.0) {
            append_warning(warnings, "record '" + chart_key + "' has an out-of-range percent");
            continue;
        }

        std::int64_t dance_points = 0;
        std::int64_t timestamp_unix = 0;
        if (!read_integer(value, "dp", dance_points) ||
            !read_integer(value, "timestamp", timestamp_unix)) {
            append_warning(warnings, "record '" + chart_key + "' has an invalid numeric field");
            continue;
        }

        ScoreRecord record;
        record.grade = grade_it->get<std::string>();
        record.percent = percent;
        const std::int64_t int_low = std::numeric_limits<int>::lowest();
        const std::int64_t int_high = std::numeric_limits<int>::max();
        if (dance_points < int_low || dance_points > int_high) {
            append_warning(warnings, "record '" + chart_key + "' has an out-of-range dp; clamped");
            dance_points = std::clamp(dance_points, int_low, int_high);
        }
        record.dance_points = static_cast<int>(dance_points);
        record.timestamp_unix = timestamp_unix;
        result.scores[chart_key] = record;
    }

    std::string text = "[Scores] Loaded '" + path.string() + "'";
    if (!warnings.empty()) {
        text += " with warnings: " + warnings;
    }
    set_message(message, text);
    if (status != nullptr) {
        *status = ScoresLoadStatus::LoadedFromFile;
    }
    return result;
}

bool save_high_scores(const std::filesystem::path& path, const HighScores& scores,
                      std::string* message) {
    json score_object = json::object();
    for (const auto& [chart_key, record] : scores.scores) {
        score_object[chart_key] = {
            {"grade", record.grade},
            {"percent", record.percent},
            {"dp", record.dance_points},
            {"timestamp", record.timestamp_unix},
        };
    }

    json document;
    document["version"] = kScoresVersion;
    document["scores"] = score_object;

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            set_message(message, "[Scores] Warning: could not create directory for '" +
                                     path.string() + "': " + ec.message());
            return false;
        }
    }

    std::filesystem::path temp_path = path;
    temp_path += ".tmp." + std::to_string(unique_suffix());

    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            set_message(message, "[Scores] Warning: could not open '" + temp_path.string() +
                                     "' for writing");
            return false;
        }
        out << document.dump(2) << "\n";
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(temp_path, ec);
            set_message(message, "[Scores] Warning: failed to write '" + temp_path.string() + "'");
            return false;
        }
    }

    // std::filesystem::rename replaces an existing regular file (on Windows the
    // MSVC implementation maps this to MoveFileEx with REPLACE_EXISTING), and
    // the unique temp name avoids collisions between concurrent writers.
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        set_message(message, "[Scores] Warning: could not replace '" + path.string() +
                                 "': " + ec.message());
        return false;
    }

    set_message(message, "[Scores] Saved '" + path.string() + "'");
    return true;
}

bool submit_high_score(HighScores& scores, const std::string& chart_key,
                       const ScoreRecord& record) {
    auto it = scores.scores.find(chart_key);
    if (it == scores.scores.end()) {
        scores.scores[chart_key] = record;
        return true;
    }
    if (record.percent > it->second.percent) {
        it->second = record;
        return true;
    }
    return false;
}

const ScoreRecord* find_high_score(const HighScores& scores, const std::string& chart_key) {
    auto it = scores.scores.find(chart_key);
    if (it == scores.scores.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace td
