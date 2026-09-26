#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include "chart/chart.hpp"
#include "chart/song.hpp"

namespace td {

// Scores-file schema version, independent of the config schema (kConfigVersion).
inline constexpr int kScoresVersion = 1;

// Explicit outcome of a score-table load (mirrors ConfigLoadStatus without
// coupling this module to the config layer).
enum class ScoresLoadStatus { LoadedFromFile, UsedDefaults };

// One best result for a chart (PRD section 7.6 / section 5 story 6).
struct ScoreRecord {
    std::string grade;              // GradeTier::label, e.g. "quad_star", "S+"
    double percent = 0.0;           // fraction 0.0-1.0 (ScoreKeeper::percent())
    int dance_points = 0;           // ScoreState::actual_dp
    std::int64_t timestamp_unix = 0; // seconds since the Unix epoch at submit time
};

// Best record per stable chart key. `std::map` keeps the on-disk order
// deterministic.
struct HighScores {
    std::map<std::string, ScoreRecord> scores;
};

// Stable, content-based chart identity: a 16-hex-char FNV-1a hash over the
// simfile file name (not its absolute path), the song title/artist, the chart's
// steps type/difficulty/meter, and an FNV-1a fingerprint of every note's
// (beat, type, column) in order. Same chart -> same key across pack/folder
// moves; editing a chart's notes changes the key.
[[nodiscard]] std::string make_chart_key(const Song& song, const Chart& chart);

// Loads the score table from `path`. Never throws. Missing/corrupt/oversize
// documents -> empty table + "[Scores] ..." warning; individual malformed
// records are skipped with a warning.
[[nodiscard]] HighScores load_high_scores(const std::filesystem::path& path,
                                          std::string* message = nullptr,
                                          ScoresLoadStatus* status = nullptr);

// Atomic save (temp file + rename), as in save_config(). Never throws.
[[nodiscard]] bool save_high_scores(const std::filesystem::path& path, const HighScores& scores,
                                    std::string* message = nullptr);

// Stores `record` iff it beats the stored best (strictly greater percent, or no
// entry existed). Returns true when the table was updated; ties keep the
// existing record.
[[nodiscard]] bool submit_high_score(HighScores& scores, const std::string& chart_key,
                                     const ScoreRecord& record);

// Returns the stored best for `chart_key`, or nullptr when none exists.
[[nodiscard]] const ScoreRecord* find_high_score(const HighScores& scores,
                                                 const std::string& chart_key);

} // namespace td
