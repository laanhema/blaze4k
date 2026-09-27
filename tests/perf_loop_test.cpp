#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/frame_stats.hpp"
#include "chart/chart.hpp"
#include "chart/simfile_parser.hpp"
#include "gameplay/gameplay_view.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/attract_screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_placeholder_screen.hpp"
#include "screens/title_screen.hpp"
#include "timing/judgment_constants.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

namespace fs = std::filesystem;

constexpr double kFixedDt = 1.0 / 60.0;
constexpr double kFrameBudgetMs = 1000.0 / 60.0; // 16.67 ms at 60 fps
constexpr double kMaxBudgetMs = 50.0;

template <typename Fn>
double time_ms(Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void print_block(const char* label, const td::FrameStats& stats) {
    std::cout << "[" << label << "] frames=" << stats.count()
              << " min=" << std::fixed << std::setprecision(3) << stats.min_ms() << "ms"
              << " median=" << stats.median_ms() << "ms"
              << " p95=" << stats.percentile_ms(95.0) << "ms"
              << " p99=" << stats.percentile_ms(99.0) << "ms"
              << " max=" << stats.max_ms() << "ms"
              << " mean=" << stats.mean_ms() << "ms"
              << " hitches(>" << kFrameBudgetMs << "ms)=" << stats.over_budget(kFrameBudgetMs)
              << "\n";
}

fs::path resolve_fixture() {
    const fs::path candidates[] = {
        fs::path(TUNDRA_SOURCE_DIR) / "tests" / "fixtures" / "reference_pack" / "Tundra Pack" /
            "Aurora Borealis" / "Aurora Borealis.sm",
        fs::path("tests") / "fixtures" / "reference_pack" / "Tundra Pack" / "Aurora Borealis" /
            "Aurora Borealis.sm",
        fs::path("..") / "tests" / "fixtures" / "reference_pack" / "Tundra Pack" /
            "Aurora Borealis" / "Aurora Borealis.sm",
    };
    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

// Plays the reference chart to completion through the real GameplayView logic
// path (stub clock in headless), timing update + input handling per step. Render
// is intentionally not timed (it is a no-op into an uninitialized renderer).
td::FrameStats run_song_loop(const fs::path& fixture) {
    td::SimfileParser parser;
    TEST_CHECK(parser.parse_file(fixture.string()));
    TEST_CHECK(!parser.charts().empty());
    const td::Chart& chart = parser.charts().front();

    td::GameplayOptions options;
    options.fail_enabled = false; // keep playing so the full chart resolves

    td::GameplayView view;
    TEST_CHECK(view.init(chart, td::JudgmentConstants::compiled_defaults(), "", options));

    double last_note_time = 0.0;
    for (const td::Note& note : chart.notes) {
        last_note_time =
            std::max(last_note_time, std::max(note.time_seconds, note.hold_end_time_seconds));
    }
    const int max_steps = static_cast<int>((last_note_time + 5.0) / kFixedDt) + 1;

    const std::array<bool, 4> held{false, false, false, false};
    td::FrameStats stats;
    int steps = 0;
    while (view.outcome() == td::GameplayOutcome::InProgress && steps < max_steps) {
        stats.add(time_ms([&] {
            view.handle_input_events({}, 0);
            view.update(kFixedDt, held);
        }));
        ++steps;
    }

    std::cout << "[perf_loop_test] song '" << chart.difficulty << "' (meter " << chart.meter
              << ") resolved as "
              << (view.outcome() == td::GameplayOutcome::Cleared ? "Cleared" : "not-Cleared")
              << " in " << steps << " steps\n";
    TEST_CHECK(view.outcome() == td::GameplayOutcome::Cleared);
    TEST_CHECK(!stats.empty());
    TEST_CHECK(stats.count() == static_cast<std::size_t>(steps));
    TEST_CHECK(std::isfinite(stats.max_ms()));

    // Headless render must not crash (the renderer is uninitialized).
    td::GlQuadRenderer renderer;
    view.render(renderer, 1280, 720);
    return stats;
}

// Times N real arcade-screen updates, rotating Title -> Attract -> Select so
// every shell update path is exercised.
td::FrameStats run_screen_loop() {
    td::ScreenManager manager(0.0); // idle-attract disabled for determinism
    manager.add_screen(std::make_unique<td::TitleScreen>());
    manager.add_screen(std::make_unique<td::AttractScreen>());
    manager.add_screen(std::make_unique<td::SelectPlaceholderScreen>());
    manager.start(td::ScreenId::Title);

    constexpr int kScreenFrames = 2000;
    const td::ScreenId rotation[] = {td::ScreenId::Title, td::ScreenId::Attract,
                                     td::ScreenId::Select};
    int rotation_index = 0;

    td::FrameStats stats;
    for (int i = 0; i < kScreenFrames; ++i) {
        if (i > 0 && i % 200 == 0) {
            rotation_index = (rotation_index + 1) % 3;
            manager.transition_to(rotation[rotation_index]);
        }
        stats.add(time_ms([&] { manager.update(kFixedDt, {}); }));
    }
    TEST_CHECK(stats.count() == kScreenFrames);
    return stats;
}

} // namespace

int main() {
    std::cout << "[perf_loop_test] Headless CPU-budget benchmark (proxy for AC2; "
                 "not a GPU/vsync FPS proof).\n";

    const char* strict_env = std::getenv("TUNDRA_PERF_STRICT");
    const bool strict = !(strict_env != nullptr && std::string(strict_env) == "0");
    if (!strict) {
        std::cout << "[perf_loop_test] TUNDRA_PERF_STRICT=0: budget assertions advisory only.\n";
    }

    const fs::path fixture = resolve_fixture();
    TEST_CHECK(!fixture.empty());

    const td::FrameStats gameplay_stats = run_song_loop(fixture);
    print_block("perf_loop_test.song", gameplay_stats);

    const td::FrameStats screen_stats = run_screen_loop();
    print_block("perf_loop_test.screens", screen_stats);

    if (strict) {
        TEST_CHECK(gameplay_stats.percentile_ms(99.0) < kFrameBudgetMs);
        TEST_CHECK(gameplay_stats.max_ms() < kMaxBudgetMs);
        TEST_CHECK(screen_stats.percentile_ms(99.0) < kFrameBudgetMs);
        TEST_CHECK(screen_stats.max_ms() < kMaxBudgetMs);
    }

    std::cout << "[perf_loop_test] All tests passed!\n";
    return 0;
}
