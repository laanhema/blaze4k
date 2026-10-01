#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "audio/assist_tick_player.hpp"
#include "chart/chart.hpp"
#include "gameplay/assist_tick_schedule.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::Note;
using blaze4k::NoteType;

Note note(int column, double time, NoteType type = NoteType::Tap) {
    Note n;
    n.column = column;
    n.time_seconds = time;
    n.beat = time * 2.0;
    n.type = type;
    return n;
}

void test_schedule_rows() {
    blaze4k::Chart chart;
    // Deliberately unsorted: a jump at 1.0, a lone mine at 1.5, a hold head and a
    // roll head sharing 2.0, a tap at 0.5, a mine sharing the 3.0 tap's row.
    chart.notes = {
        note(0, 1.0), note(3, 1.0),
        note(1, 1.5, NoteType::Mine),
        note(2, 2.0, NoteType::HoldHead), note(1, 2.0, NoteType::RollHead),
        note(0, 0.5),
        note(0, 3.0, NoteType::Mine), note(2, 3.0),
    };

    blaze4k::AssistTickSchedule schedule;
    schedule.reset(chart);
    const std::vector<double> expected = {0.5, 1.0, 2.0, 3.0};
    TEST_CHECK(schedule.tick_times() == expected); // sorted, one per row, no mines

    std::vector<double> out;
    schedule.collect_due(0.4, out);
    TEST_CHECK(out.empty());
    schedule.collect_due(1.0, out); // inclusive horizon
    TEST_CHECK((out == std::vector<double>{0.5, 1.0}));
    out.clear();
    schedule.collect_due(1.0, out); // already emitted: never twice
    TEST_CHECK(out.empty());
    schedule.collect_due(100.0, out);
    TEST_CHECK((out == std::vector<double>{2.0, 3.0}));
    TEST_CHECK(schedule.emitted() == 4);

    // reset() rewinds for the next run.
    schedule.reset(chart);
    TEST_CHECK(schedule.emitted() == 0);
    out.clear();
    schedule.collect_due(0.5, out);
    TEST_CHECK((out == std::vector<double>{0.5}));

    // A mines-only chart never ticks.
    blaze4k::Chart mines;
    mines.notes = {note(0, 1.0, NoteType::Mine)};
    schedule.reset(mines);
    TEST_CHECK(schedule.tick_times().empty());
    std::cout << "  - schedule: one tick per tap/hold/roll row, mines skipped ok.\n";
}

void test_wav_and_player_fallbacks() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "blaze4k_assist_tick_test";
    std::filesystem::create_directories(dir);
    const std::filesystem::path wav = dir / "assist_tick.wav";

    TEST_CHECK(blaze4k::write_assist_tick_wav(wav));
    std::ifstream in(wav, std::ios::binary);
    char riff[4] = {};
    in.read(riff, 4);
    TEST_CHECK(std::string(riff, 4) == "RIFF");
    TEST_CHECK(std::filesystem::file_size(wav) > 44u);
    TEST_CHECK(!blaze4k::write_assist_tick_wav(std::filesystem::path{}));

    // No path: disabled, and scheduling/stopping stay safe no-ops.
    blaze4k::AssistTickPlayer player;
    TEST_CHECK(!player.init(std::filesystem::path{}));
    TEST_CHECK(!player.is_ready());
    player.play_in(0.1);
    player.stop_all();
    player.shutdown();

    // With a real device the pool loads and schedules; headless it just reports
    // unavailable. Either way nothing may crash.
    if (player.init(wav)) {
        TEST_CHECK(player.is_ready());
        for (int i = 0; i < 40; ++i) { // wraps the round-robin pool
            player.play_in(0.01 * i);
        }
        player.stop_all();
        std::cout << "    (audio device present: pool loaded + scheduled)\n";
    }
    player.shutdown();
    TEST_CHECK(!player.is_ready());

    std::filesystem::remove_all(dir);
    std::cout << "  - tick WAV synthesis + disabled player no-ops ok.\n";
}

} // namespace

int main() {
    std::cout << "[assist_tick_test] Running assist tick tests...\n";
    test_schedule_rows();
    test_wav_and_player_fallbacks();
    std::cout << "[assist_tick_test] All tests passed!\n";
    return 0;
}
