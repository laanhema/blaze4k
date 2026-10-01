#include <iostream>
#include <cassert>
#include <cmath>
#include "app/app.hpp"
#include "app/window.hpp"

// Timestep accumulator simulator for deterministic testing
class AccumulatorSimulator {
public:
    double fixed_dt;
    double max_frame_dt;
    double accumulator = 0.0;
    int tick_count = 0;

    AccumulatorSimulator(double dt, double max_dt)
        : fixed_dt(dt), max_frame_dt(max_dt) {}

    void step(double frame_dt) {
        if (frame_dt > max_frame_dt) {
            frame_dt = max_frame_dt;
        }
        accumulator += frame_dt;
        while (accumulator >= fixed_dt) {
            tick_count++;
            accumulator -= fixed_dt;
        }
    }
};

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[app_test] Running App and Window unit tests...\n";

    // 1. Test WindowConfig defaults
    blaze4k::WindowConfig win_cfg;
    TEST_CHECK(win_cfg.width == 1280);
    TEST_CHECK(win_cfg.height == 720);
    TEST_CHECK(win_cfg.vsync == true);
    TEST_CHECK(win_cfg.title == "Blaze 4k");
    std::cout << "  - WindowConfig defaults verified.\n";

    // 2. Test AppConfig defaults
    blaze4k::AppConfig app_cfg;
    TEST_CHECK(std::abs(app_cfg.fixed_dt - (1.0 / 60.0)) < 1e-6);
    TEST_CHECK(std::abs(app_cfg.max_frame_dt - 0.25) < 1e-6);
    TEST_CHECK(app_cfg.smoke_test_frames == -1);
    std::cout << "  - AppConfig defaults verified.\n";

    // 3. Test Timestep Accumulator behavior
    const double dt = 1.0 / 60.0;
    AccumulatorSimulator sim(dt, 0.25);

    // Exact frame: 1 frame = 1 tick
    sim.step(dt);
    TEST_CHECK(sim.tick_count == 1);
    TEST_CHECK(std::abs(sim.accumulator) < 1e-9);

    // Sub-frame (e.g. 120 FPS): 2 half-frames = 1 tick
    sim.step(dt * 0.5);
    TEST_CHECK(sim.tick_count == 1); // Not accumulated enough yet
    sim.step(dt * 0.5);
    TEST_CHECK(sim.tick_count == 2); // Now triggered

    // Multi-frame lag spike (e.g. 3 frames slow): 3 ticks
    sim.step(dt * 3.0);
    TEST_CHECK(sim.tick_count == 5);

    // Spiral-of-death clamp: 1.0 second lag spike clamped to 0.25s
    sim.step(1.0);
    // 0.25s / (1/60s) = 15 ticks
    TEST_CHECK(sim.tick_count == 5 + 15);
    std::cout << "  - Timestep accumulator simulation passed.\n";

    // 4. Test Headless App Smoke Test
    blaze4k::AppConfig smoke_cfg;
    smoke_cfg.window.headless = true;
    smoke_cfg.smoke_test_frames = 10;

    int update_count = 0;
    int render_count = 0;

    blaze4k::App smoke_app(smoke_cfg);
    smoke_app.set_update_callback([&](double) {
        update_count++;
    });
    smoke_app.set_render_callback([&](double) {
        render_count++;
    });

    TEST_CHECK(smoke_app.init());
    smoke_app.run();

    TEST_CHECK(render_count == 10);
    TEST_CHECK(update_count >= 0);
    TEST_CHECK(!smoke_app.is_running());
    std::cout << "  - Headless App smoke test passed (" << render_count << " render frames, "
              << update_count << " update ticks).\n";

    // 5. Perf-report instrumentation: one sample per rendered frame, and the
    //    mean is finite. Presentation-only; nothing here touches gameplay.
    blaze4k::AppConfig perf_cfg;
    perf_cfg.window.headless = true;
    perf_cfg.smoke_test_frames = 60;
    perf_cfg.perf_report = true;

    blaze4k::App perf_app(perf_cfg);
    TEST_CHECK(perf_app.init());
    perf_app.run();

    TEST_CHECK(!perf_app.frame_stats().empty());
    TEST_CHECK(perf_app.frame_stats().count() == 60);
    TEST_CHECK(std::isfinite(perf_app.frame_stats().mean_ms()));
    std::cout << "  - Perf-report smoke run recorded " << perf_app.frame_stats().count()
              << " frame samples (mean " << perf_app.frame_stats().mean_ms() << " ms).\n";

    std::cout << "[app_test] All tests passed!\n";
    return 0;
}
