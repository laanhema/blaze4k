#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "data/data_paths.hpp"
#include "render/background_renderer.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/texture.hpp"

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

bool approx(float a, float b) {
    return std::fabs(a - b) < 1e-6f;
}

void test_cover_uv_equal_aspect() {
    const blaze4k::UVRect uv = blaze4k::BackgroundRenderer::cover_uv(1280, 720, 1280, 720);
    TEST_CHECK(approx(uv.u0, 0.0f) && approx(uv.u1, 1.0f));
    TEST_CHECK(approx(uv.v0, 0.0f) && approx(uv.v1, 1.0f));
    std::cout << "  - cover_uv equal aspect draws the full texture ok.\n";
}

void test_cover_uv_tall_texture() {
    // 4:3 texture on a 16:9 screen -> crop vertical, keep full width.
    const blaze4k::UVRect uv = blaze4k::BackgroundRenderer::cover_uv(1280, 720, 640, 480);
    TEST_CHECK(approx(uv.u0, 0.0f) && approx(uv.u1, 1.0f));
    TEST_CHECK(approx(uv.v0, 0.125f) && approx(uv.v1, 0.875f));
    std::cout << "  - cover_uv tall texture crops the vertical axis ok.\n";
}

void test_cover_uv_wide_texture() {
    // 16:9 texture on a 4:3 screen -> crop horizontal, keep full height.
    const blaze4k::UVRect uv = blaze4k::BackgroundRenderer::cover_uv(800, 600, 1280, 720);
    TEST_CHECK(approx(uv.v0, 0.0f) && approx(uv.v1, 1.0f));
    TEST_CHECK(approx(uv.u0, 0.125f) && approx(uv.u1, 0.875f));
    std::cout << "  - cover_uv wide texture crops the horizontal axis ok.\n";
}

void test_cover_uv_degenerate() {
    const blaze4k::UVRect full{};
    const blaze4k::UVRect cases[] = {
        blaze4k::BackgroundRenderer::cover_uv(0, 720, 640, 480),
        blaze4k::BackgroundRenderer::cover_uv(1280, 0, 640, 480),
        blaze4k::BackgroundRenderer::cover_uv(-10, 720, 640, 480),
        blaze4k::BackgroundRenderer::cover_uv(1280, 720, 0, 480),
        blaze4k::BackgroundRenderer::cover_uv(1280, 720, 640, -1),
    };
    for (const blaze4k::UVRect& uv : cases) {
        TEST_CHECK(approx(uv.u0, full.u0) && approx(uv.u1, full.u1));
        TEST_CHECK(approx(uv.v0, full.v0) && approx(uv.v1, full.v1));
    }
    std::cout << "  - cover_uv degenerate dimensions return the full texture ok.\n";
}

void test_headless_lifecycle() {
    // No window/GL: every operation must be a safe no-op.
    blaze4k::GlQuadRenderer renderer; // left uninitialized, like the other render tests
    blaze4k::BackgroundRenderer background;

    background.load("");
    TEST_CHECK(!background.has_image());

    background.load("/no/such/file-987654.png");
    TEST_CHECK(!background.has_image());

    background.render(renderer, 1280, 720);
    background.render(renderer, 0, 0);
    background.shutdown();
    TEST_CHECK(!background.has_image());
    std::cout << "  - headless load/render/shutdown are safe no-ops ok.\n";
}

void test_resolve_first_existing() {
    const fs::path root = fs::temp_directory_path() /
                          ("blaze4k_background_test_" + std::to_string(std::random_device{}()));
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path present = root / "present.png";
    {
        std::ofstream out(present);
        out << "x";
    }

    const fs::path missing = root / "missing.png";
    TEST_CHECK(blaze4k::resolve_first_existing({missing, present}).filename() == "present.png");
    TEST_CHECK(blaze4k::resolve_first_existing({missing}).empty());
    TEST_CHECK(blaze4k::resolve_first_existing({}).empty());
    fs::remove_all(root);
    std::cout << "  - resolve_first_existing honors candidate order ok.\n";
}

void test_committed_fallback_asset() {
    // Discovery order mirrors the runtime candidates: cwd-relative first, then
    // the source tree (via the compile definition) for out-of-tree test runs.
    const fs::path asset = blaze4k::resolve_first_existing({
        fs::path("assets") / "backgrounds" / "fallback.png",
        fs::path(BLAZE4K_ASSETS_DIR) / "backgrounds" / "fallback.png",
    });
    TEST_CHECK(!asset.empty());
    TEST_CHECK(fs::exists(asset));

    std::error_code ec;
    TEST_CHECK(fs::is_regular_file(asset, ec));
    TEST_CHECK(fs::file_size(asset, ec) > 0);
    TEST_CHECK(!ec);

    // Decodable image with sane (non-degenerate dims within the header cap).
    const blaze4k::ImageHeader header = blaze4k::probe_image_header(asset.string());
    TEST_CHECK(header.ok);
    TEST_CHECK(header.width > 0 && header.height > 0);
    std::cout << "  - committed fallback.png exists, is non-empty, and decodes ok.\n";
}

} // namespace

int main() {
    std::cout << "[background_test] Running BackgroundRenderer tests...\n";
    test_cover_uv_equal_aspect();
    test_cover_uv_tall_texture();
    test_cover_uv_wide_texture();
    test_cover_uv_degenerate();
    test_headless_lifecycle();
    test_resolve_first_existing();
    test_committed_fallback_asset();
    std::cout << "[background_test] All tests passed!\n";
    return 0;
}
