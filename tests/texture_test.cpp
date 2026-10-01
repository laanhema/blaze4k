#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

} // namespace

int main() {
    std::cout << "[texture_test] Running Texture header-hardening tests...\n";
    test_oversized_header_rejected_before_decode();
    test_dimension_cap_boundary();
    test_missing_file_and_empty_path();
    std::cout << "[texture_test] All tests passed!\n";
    return 0;
}
