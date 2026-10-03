#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glad/glad.h> // GL_* enum macros only; no GL calls (headless)

#include "render/geometry.hpp"
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

// Minimal PNG: signature + an IHDR declaring the given dimensions + an empty
// IEND. stbi_info only needs the signature and IHDR, so a tiny (<1 KiB) file can
// declare huge dimensions -- exactly the decompression-bomb shape.
std::vector<unsigned char> make_png_header_only(std::uint32_t width, std::uint32_t height) {
    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto put32 = [&png](std::uint32_t value) {
        png.push_back(static_cast<unsigned char>(value >> 24));
        png.push_back(static_cast<unsigned char>(value >> 16));
        png.push_back(static_cast<unsigned char>(value >> 8));
        png.push_back(static_cast<unsigned char>(value));
    };

    put32(13); // IHDR payload length
    png.insert(png.end(), {'I', 'H', 'D', 'R'});
    put32(width);
    put32(height);
    png.push_back(8); // bit depth
    png.push_back(6); // color type: RGBA
    png.push_back(0); // compression
    png.push_back(0); // filter
    png.push_back(0); // interlace
    put32(0);         // CRC (not validated by stbi_info)

    put32(0); // empty IEND
    png.insert(png.end(), {'I', 'E', 'N', 'D'});
    put32(0);
    return png;
}

void write_file(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

void test_oversized_header_rejected_before_decode() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_texture_test";
    std::filesystem::create_directories(root);

    const std::filesystem::path huge = root / "huge.png";
    write_file(huge, make_png_header_only(16000, 16000));

    // The crafted file is a few dozen bytes: the 16 MiB read cap would not stop
    // it, so the header probe must.
    TEST_CHECK(std::filesystem::file_size(huge) < 1024);

    const blaze4k::ImageHeader header = blaze4k::probe_image_header(huge.string());
    TEST_CHECK(!header.ok);

    // from_file must reject it without allocating decoded pixels (it also
    // returns invalid because no GL context is available in this test).
    const blaze4k::Texture texture = blaze4k::Texture::from_file(huge.string());
    TEST_CHECK(!texture.valid());

    std::filesystem::remove_all(root);
    std::cout << "  - oversized IHDR rejected at the header probe ok.\n";
}

void test_dimension_cap_boundary() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_texture_test";
    std::filesystem::create_directories(root);

    const std::filesystem::path at_cap = root / "at_cap.png";
    write_file(at_cap, make_png_header_only(4096, 4096));
    TEST_CHECK(blaze4k::probe_image_header(at_cap.string()).ok);

    const std::filesystem::path over_cap = root / "over_cap.png";
    write_file(over_cap, make_png_header_only(4096, 4097));
    TEST_CHECK(!blaze4k::probe_image_header(over_cap.string()).ok);

    std::filesystem::remove_all(root);
    std::cout << "  - 4096px dimension cap boundary enforced ok.\n";
}

void test_missing_file_and_empty_path() {
    TEST_CHECK(!blaze4k::probe_image_header("does-not-exist-987654.png").ok);
    TEST_CHECK(!blaze4k::Texture::from_file("").valid());
    std::cout << "  - missing file / empty path rejected ok.\n";
}

void test_premultiply_alpha_bytes() {
    std::vector<std::uint8_t> px = {
        200, 100, 50,  255, // opaque: identity
        255, 255, 255, 0,   // white transparent texel that caused the fringe (#59)
        64,  64,  64,  0,   // grey transparent texel
        255, 255, 255, 128, // half alpha: (255*128+127)/255 = 128
        255, 0,   100, 1,   // (255+127)/255 = 1, (100+127)/255 = 0
        1,   1,   1,   1,   // (1+127)/255 = 0
    };
    blaze4k::premultiply_alpha(px);
    const std::vector<std::uint8_t> expected = {
        200, 100, 50,  255,
        0,   0,   0,   0,
        0,   0,   0,   0,
        128, 128, 128, 128,
        1,   0,   0,   1,
        0,   0,   0,   1,
    };
    TEST_CHECK(px == expected);
    // Alpha bytes are untouched.
    TEST_CHECK(px[3] == 255 && px[7] == 0 && px[11] == 0 && px[15] == 128 && px[19] == 1 &&
               px[23] == 1);
    std::cout << "  - premultiply_alpha byte math ok.\n";
}

void test_premultiply_alpha_partial_and_empty() {
    std::vector<std::uint8_t> empty;
    blaze4k::premultiply_alpha(empty);
    blaze4k::premultiply_alpha(std::span<std::uint8_t>{});
    TEST_CHECK(empty.empty());

    // Six bytes: one whole pixel plus a trailing partial pixel that must be ignored.
    std::vector<std::uint8_t> partial = {255, 255, 255, 0, 77, 99};
    blaze4k::premultiply_alpha(partial);
    TEST_CHECK((partial == std::vector<std::uint8_t>{0, 0, 0, 0, 77, 99}));
    std::cout << "  - premultiply_alpha empty / trailing partial pixel ok.\n";
}

bool approx_equal(float a, float b) {
    return std::fabs(a - b) <= 1e-6f;
}

bool color_near(blaze4k::Color a, blaze4k::Color b) {
    return approx_equal(a.r, b.r) && approx_equal(a.g, b.g) && approx_equal(a.b, b.b) &&
           approx_equal(a.a, b.a);
}

void test_premultiply_color() {
    static_assert(blaze4k::premultiply(blaze4k::Color{1.0f, 0.5f, 0.25f, 0.5f}).g == 0.25f,
                  "premultiply must be usable at compile time");

    TEST_CHECK(color_near(blaze4k::premultiply(blaze4k::Color{1.0f, 1.0f, 1.0f, 1.0f}),
                          blaze4k::Color{1.0f, 1.0f, 1.0f, 1.0f}));
    TEST_CHECK(color_near(blaze4k::premultiply(blaze4k::Color{0.5f, 1.0f, 0.2f, 0.5f}),
                          blaze4k::Color{0.25f, 0.5f, 0.1f, 0.5f}));
    // Receptor rest tint (a = 1) is unchanged, so the flash/brightness look is too.
    TEST_CHECK(color_near(blaze4k::premultiply(blaze4k::Color{0.55f, 0.55f, 0.55f, 1.0f}),
                          blaze4k::Color{0.55f, 0.55f, 0.55f, 1.0f}));
    std::cout << "  - premultiply(Color) ok.\n";
}

bool same_params(const blaze4k::SamplerParams& a, int min_filter, int mag_filter, int wrap) {
    return a.min_filter == min_filter && a.mag_filter == mag_filter && a.wrap == wrap;
}

void test_sampler_params_defaults_unchanged() {
    using blaze4k::Texture;
    // The defaults must reproduce the pre-#88 hard-coded sampler state exactly.
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(false, Texture::Wrap::Clamp, Texture::Filter::Linear),
        GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE));
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(true, Texture::Wrap::Clamp, Texture::Filter::Linear),
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE));
    std::cout << "  - default sampler params equal the old constants ok.\n";
}

void test_sampler_params_repeat_nearest() {
    using blaze4k::Texture;
    // scanlines.png: crisp tiled rows.
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(false, Texture::Wrap::Repeat, Texture::Filter::Nearest),
        GL_NEAREST, GL_NEAREST, GL_REPEAT));
    // life_stripes.png: smooth tiled stripes.
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(false, Texture::Wrap::Repeat, Texture::Filter::Linear),
        GL_LINEAR, GL_LINEAR, GL_REPEAT));
    // Nearest + mipmaps stays nearest across and within mip levels.
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(true, Texture::Wrap::Clamp, Texture::Filter::Nearest),
        GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE));
    TEST_CHECK(same_params(
        blaze4k::texture_sampler_params(true, Texture::Wrap::Repeat, Texture::Filter::Linear),
        GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT));
    std::cout << "  - Repeat / Nearest sampler params ok.\n";
}

void test_wrap_overloads_headless() {
    using blaze4k::Texture;
    TEST_CHECK(!Texture::from_file("", false, Texture::Wrap::Repeat, Texture::Filter::Nearest).valid());
    TEST_CHECK(!Texture::from_file("does-not-exist-987654.png", false, Texture::Wrap::Repeat,
                                   Texture::Filter::Nearest)
                    .valid());
    // A real committed theme texture passes the hardening but cannot upload headless.
    const std::filesystem::path scanlines =
        std::filesystem::path(BLAZE4K_ASSETS_DIR) / "theme" / "cabinet" / "scanlines.png";
    TEST_CHECK(std::filesystem::exists(scanlines));
    TEST_CHECK(blaze4k::probe_image_header(scanlines.string()).ok);
    TEST_CHECK(!Texture::from_file(scanlines.string(), false, Texture::Wrap::Repeat,
                                   Texture::Filter::Nearest)
                    .valid());
    const std::uint8_t px[4] = {255, 255, 255, 128};
    TEST_CHECK(!Texture::from_rgba(1, 1, px, false, Texture::Wrap::Repeat, Texture::Filter::Nearest)
                    .valid());
    std::cout << "  - wrap/filter overloads stay invalid headless ok.\n";
}

} // namespace

int main() {
    std::cout << "[texture_test] Running Texture header-hardening + premultiply tests...\n";
    test_oversized_header_rejected_before_decode();
    test_dimension_cap_boundary();
    test_missing_file_and_empty_path();
    test_premultiply_alpha_bytes();
    test_premultiply_alpha_partial_and_empty();
    test_premultiply_color();
    test_sampler_params_defaults_unchanged();
    test_sampler_params_repeat_nearest();
    test_wrap_overloads_headless();
    std::cout << "[texture_test] All tests passed!\n";
    return 0;
}
