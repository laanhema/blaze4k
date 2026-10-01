#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "chart/chart.hpp"
#include "chart/note.hpp"
#include "chart/song.hpp"
#include "data/high_scores.hpp"
#include "gameplay/judgment.hpp"
#include "gameplay/score_keeper.hpp"
#include "screens/results.hpp"
#include "timing/judgment_constants.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::Chart;
using blaze4k::GradeTier;
using blaze4k::HighScores;
using blaze4k::HoldJudgment;
using blaze4k::Note;
using blaze4k::NoteType;
using blaze4k::ResultsSummary;
using blaze4k::ScoreState;
using blaze4k::Song;
using blaze4k::TapJudgment;

Song make_song() {
    Song song;
    song.simfile_path = "Blaze Anthem.sm";
    song.metadata.title = "Blaze Anthem";
    song.metadata.artist = "Test Artist";
    return song;
}

Chart make_chart() {
    Chart chart;
    chart.steps_type = "dance-single";
    chart.difficulty = "Hard";
    chart.meter = 9;
    Note note;
    note.column = 0;
    note.beat = 1.0;
    note.type = NoteType::Tap;
    chart.notes.push_back(note);
    chart.tap_count = 1;
    return chart;
}

ScoreState make_state(const GradeTier* grade) {
    ScoreState state;
    state.actual_dp = 40;
    state.possible_dp = 50;
    state.combo = 4;
    state.max_combo = 7;
    state.percent = 0.8;
    state.grade = grade;
    state.tap_counts[static_cast<std::size_t>(TapJudgment::Fantastic)] = 3;
    state.tap_counts[static_cast<std::size_t>(TapJudgment::Miss)] = 1;
    state.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)] = 2;
    return state;
}

ResultsSummary make_summary(const Song& song, const Chart& chart, const std::string& grade,
                            double percent, bool failed = false) {
    ResultsSummary summary;
    summary.valid = true;
    summary.song = &song;
    summary.chart = &chart;
    summary.failed = failed;
    summary.grade_label = grade;
    summary.percent = percent;
    summary.actual_dp = 40;
    summary.possible_dp = 50;
    summary.max_combo = 7;
    return summary;
}

// 1. Snapshot copy from a live ScoreState.
void test_snapshot_copy() {
    const GradeTier tier{0.8, "A-"};
    const Song song = make_song();
    const Chart chart = make_chart();
    const ScoreState state = make_state(&tier);

    const ResultsSummary summary = blaze4k::results_summary_from(state, false, &song, &chart);
    TEST_CHECK(summary.valid);
    TEST_CHECK(summary.song == &song);
    TEST_CHECK(summary.chart == &chart);
    TEST_CHECK(!summary.failed);
    TEST_CHECK(summary.grade_label == "A-");
    TEST_CHECK(summary.percent == 0.8);
    TEST_CHECK(summary.actual_dp == 40);
    TEST_CHECK(summary.possible_dp == 50);
    TEST_CHECK(summary.max_combo == 7);
    TEST_CHECK(summary.tap_counts[static_cast<std::size_t>(TapJudgment::Fantastic)] == 3);
    TEST_CHECK(summary.tap_counts[static_cast<std::size_t>(TapJudgment::Miss)] == 1);
    TEST_CHECK(summary.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)] == 2);

    // A null grade is tolerated: empty label, still a valid snapshot, failed passes through.
    const ScoreState no_grade = make_state(nullptr);
    const ResultsSummary null_grade = blaze4k::results_summary_from(no_grade, true, &song, &chart);
    TEST_CHECK(null_grade.valid);
    TEST_CHECK(null_grade.grade_label.empty());
    TEST_CHECK(null_grade.failed);
    std::cout << "  - snapshot copies ScoreState + fail flag ok.\n";
}

// 2. First-ever record is a NEW RECORD (submit_high_score semantics, OQ1).
void test_submit_first() {
    HighScores scores;
    const Song song = make_song();
    const Chart chart = make_chart();
    const ResultsSummary summary = make_summary(song, chart, "S+", 0.95);
    const std::string key = blaze4k::make_chart_key(song, chart);

    TEST_CHECK(blaze4k::find_high_score(scores, key) == nullptr);
    TEST_CHECK(blaze4k::results_submit_score(scores, summary, 12345));

    const blaze4k::ScoreRecord* record = blaze4k::find_high_score(scores, key);
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->grade == "S+");
    TEST_CHECK(record->percent == 0.95);
    TEST_CHECK(record->dance_points == 40);
    TEST_CHECK(record->timestamp_unix == 12345);
    std::cout << "  - first-ever clear submits and flags a new record ok.\n";
}

// 3. Lower and equal runs do not replace the stored best.
void test_submit_lower_and_tie() {
    HighScores scores;
    const Song song = make_song();
    const Chart chart = make_chart();
    const std::string key = blaze4k::make_chart_key(song, chart);

    TEST_CHECK(blaze4k::results_submit_score(scores, make_summary(song, chart, "quad_star", 1.0), 1));

    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "C-", 0.50), 2));
    TEST_CHECK(blaze4k::find_high_score(scores, key)->percent == 1.0);
    TEST_CHECK(blaze4k::find_high_score(scores, key)->timestamp_unix == 1);

    ResultsSummary tie = make_summary(song, chart, "quad_star", 1.0);
    tie.actual_dp = 999; // same percent, different payload: must be ignored
    TEST_CHECK(!blaze4k::results_submit_score(scores, tie, 3));
    TEST_CHECK(blaze4k::find_high_score(scores, key)->dance_points == 40);
    TEST_CHECK(blaze4k::find_high_score(scores, key)->timestamp_unix == 1);
    std::cout << "  - lower/equal runs keep the stored best ok.\n";
}

// 4. A strictly better run replaces grade/percent/DP/timestamp.
void test_submit_higher() {
    HighScores scores;
    const Song song = make_song();
    const Chart chart = make_chart();

    TEST_CHECK(blaze4k::results_submit_score(scores, make_summary(song, chart, "C", 0.60), 10));
    ResultsSummary better = make_summary(song, chart, "A", 0.85);
    better.actual_dp = 48;
    TEST_CHECK(blaze4k::results_submit_score(scores, better, 20));

    const blaze4k::ScoreRecord* record = blaze4k::find_high_score(scores, blaze4k::make_chart_key(song, chart));
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->grade == "A");
    TEST_CHECK(record->percent == 0.85);
    TEST_CHECK(record->dance_points == 48);
    TEST_CHECK(record->timestamp_unix == 20);
    std::cout << "  - a strictly better run replaces the record ok.\n";
}

// 5. Failed / invalid / null-song / empty-grade runs never submit (OQ2).
void test_no_submit_for_failed_or_invalid() {
    HighScores scores;
    const Song song = make_song();
    const Chart chart = make_chart();

    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", 0.9, true), 1));
    TEST_CHECK(scores.scores.empty());

    ResultsSummary invalid;
    TEST_CHECK(!blaze4k::results_submit_score(scores, invalid, 1));
    TEST_CHECK(scores.scores.empty());

    ResultsSummary no_song = make_summary(song, chart, "S", 0.9);
    no_song.song = nullptr;
    TEST_CHECK(!blaze4k::results_submit_score(scores, no_song, 1));

    ResultsSummary no_chart = make_summary(song, chart, "S", 0.9);
    no_chart.chart = nullptr;
    TEST_CHECK(!blaze4k::results_submit_score(scores, no_chart, 1));

    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "", 0.9), 1));
    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", -0.1), 1));
    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", 1.5), 1));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", nan), 1));
    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", inf), 1));
    TEST_CHECK(!blaze4k::results_submit_score(scores, make_summary(song, chart, "S", -inf), 1));
    TEST_CHECK(scores.scores.empty());
    std::cout << "  - failed/invalid runs never submit ok.\n";
}

// 6. Eligibility predicate: matches the submit guard, so the Results screen's
//    `submitted` flag cannot disagree with what was actually considered. A
//    completed Fail-Off all-miss clear has a negative percent and is deliberately
//    not eligible (its record would be dropped by the loader on the next startup).
void test_submit_permitted_predicate() {
    const Song song = make_song();
    const Chart chart = make_chart();

    TEST_CHECK(blaze4k::results_submit_permitted(make_summary(song, chart, "S+", 0.95)));
    TEST_CHECK(blaze4k::results_submit_permitted(make_summary(song, chart, "quad_star", 1.0)));
    TEST_CHECK(blaze4k::results_submit_permitted(make_summary(song, chart, "D", 0.0)));

    TEST_CHECK(!blaze4k::results_submit_permitted(make_summary(song, chart, "D", -2.4))); // Fail-Off
    TEST_CHECK(!blaze4k::results_submit_permitted(make_summary(song, chart, "S", 1.001)));
    TEST_CHECK(!blaze4k::results_submit_permitted(make_summary(song, chart, "S", 0.9, true)));
    TEST_CHECK(!blaze4k::results_submit_permitted(make_summary(song, chart, "", 0.9)));

    ResultsSummary invalid;
    TEST_CHECK(!blaze4k::results_submit_permitted(invalid));
    ResultsSummary no_song = make_summary(song, chart, "S", 0.9);
    no_song.song = nullptr;
    TEST_CHECK(!blaze4k::results_submit_permitted(no_song));
    std::cout << "  - submit eligibility predicate matches the guard ok.\n";
}

// 7. Save/load round-trip preserves the submitted record (AC2 data layer).
void test_persistence_round_trip() {
    HighScores scores;
    const Song song = make_song();
    const Chart chart = make_chart();
    const std::string key = blaze4k::make_chart_key(song, chart);
    TEST_CHECK(blaze4k::results_submit_score(scores, make_summary(song, chart, "S+", 0.97), 4242));

    const fs::path dir = fs::temp_directory_path() / "td_results_test_roundtrip";
    fs::remove_all(dir);
    const fs::path path = dir / "scores.json";

    std::string message;
    TEST_CHECK(blaze4k::save_high_scores(path, scores, &message));

    blaze4k::ScoresLoadStatus status = blaze4k::ScoresLoadStatus::UsedDefaults;
    const HighScores loaded = blaze4k::load_high_scores(path, &message, &status);
    TEST_CHECK(status == blaze4k::ScoresLoadStatus::LoadedFromFile);

    const blaze4k::ScoreRecord* record = blaze4k::find_high_score(loaded, key);
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->grade == "S+");
    TEST_CHECK(record->percent == 0.97);
    TEST_CHECK(record->dance_points == 40);
    TEST_CHECK(record->timestamp_unix == 4242);

    fs::remove_all(dir);
    std::cout << "  - scores.json round-trip preserves the record ok.\n";
}

} // namespace

int main() {
    std::cout << "[results_test] Running results model tests...\n";
    test_snapshot_copy();
    test_submit_first();
    test_submit_lower_and_tie();
    test_submit_higher();
    test_no_submit_for_failed_or_invalid();
    test_submit_permitted_predicate();
    test_persistence_round_trip();
    std::cout << "[results_test] All tests passed!\n";
    return 0;
}
