#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "render/texture.hpp"
#include "render/theme.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

// Compile-time proof that render/theme.hpp is usable from a TU.
static_assert(blaze4k::theme::hex(0xFF0000).r == 1.0f && blaze4k::theme::hex(0xFF0000).g == 0.0f);
static_assert(blaze4k::theme::hex(0x000000, 0.72f).a == 0.72f);
static_assert(blaze4k::theme::kFontCount == 4);
static_assert(blaze4k::theme::layout::kRefWidth == 1280.0f &&
              blaze4k::theme::layout::kRefHeight == 720.0f);

namespace {

namespace fs = std::filesystem;

const fs::path kAssets{BLAZE4K_ASSETS_DIR};

fs::path cabinet_dir() {
    return kAssets / "theme" / "cabinet";
}

void test_font_files_present() {
    for (const char* f : blaze4k::theme::kFontFiles) {
        const fs::path p = kAssets / fs::path(f).lexically_relative("assets");
        TEST_CHECK(fs::is_regular_file(p));
        TEST_CHECK(fs::file_size(p) > 0);

        std::ifstream in(p, std::ios::binary);
        TEST_CHECK(in);
        unsigned char magic[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        in.read(reinterpret_cast<char*>(magic), 4);
        TEST_CHECK(in.gcount() == 4);
        // TrueType sfnt version 1.0.
        TEST_CHECK(magic[0] == 0x00 && magic[1] == 0x01 && magic[2] == 0x00 && magic[3] == 0x00);
    }
    TEST_CHECK(fs::is_regular_file(kAssets / "fonts" / "OFL-Audiowide.txt"));
    TEST_CHECK(fs::is_regular_file(kAssets / "fonts" / "OFL-SairaCondensed.txt"));
    std::cout << "  - theme font files and OFL licences present ok.\n";
}

nlohmann::json load_manifest() {
    std::ifstream in(cabinet_dir() / "manifest.json");
    TEST_CHECK(in);
    return nlohmann::json::parse(in);
}

void test_manifest_textures(const nlohmann::json& m) {
    const std::set<std::string> kinds = {"frame",  "fullscreen", "slice3",    "slice9",
                                         "sprite", "stretch",    "stretch_x", "tile"};
    TEST_CHECK(m.contains("textures"));
    const auto& textures = m["textures"];
    TEST_CHECK(textures.is_object());
    TEST_CHECK(textures.size() == 63);
    for (const auto& [name, entry] : textures.items()) {
        TEST_CHECK(entry.contains("file") && entry["file"].is_string());
        TEST_CHECK(entry.contains("kind") && entry["kind"].is_string());
        TEST_CHECK(kinds.count(entry["kind"].get<std::string>()) == 1);

        const fs::path path = cabinet_dir() / entry["file"].get<std::string>();
        TEST_CHECK(fs::is_regular_file(path));

        TEST_CHECK(entry.contains("size_px"));
        const auto& size = entry["size_px"];
        TEST_CHECK(size.is_array() && size.size() == 2);

        const blaze4k::ImageHeader header = blaze4k::probe_image_header(path.string());
        TEST_CHECK(header.ok);
        TEST_CHECK(header.width == size[0].get<int>());
        TEST_CHECK(header.height == size[1].get<int>());
        const int w = header.width;
        const int h = header.height;

        // Geometry the slice/frame helpers rely on must lie inside the image.
        for (const char* key : {"content_px", "hole_px"}) {
            if (!entry.contains(key)) {
                continue;
            }
            const auto& box = entry[key]; // [x, y, w, h]
            TEST_CHECK(box.is_array() && box.size() == 4);
            const int bx = box[0].get<int>();
            const int by = box[1].get<int>();
            const int bw = box[2].get<int>();
            const int bh = box[3].get<int>();
            TEST_CHECK(bx >= 0 && by >= 0 && bw > 0 && bh > 0);
            TEST_CHECK(bx + bw <= w && by + bh <= h);
        }
        if (entry.contains("slice3_px")) {
            const auto& s = entry["slice3_px"];
            const int left = s.at("left").get<int>();
            const int right = s.at("right").get<int>();
            TEST_CHECK(left >= 0 && right >= 0 && left + right < w);
        }
        if (entry.contains("slice9_px")) {
            const auto& s = entry["slice9_px"];
            const int left = s.at("left").get<int>();
            const int right = s.at("right").get<int>();
            const int top = s.at("top").get<int>();
            const int bottom = s.at("bottom").get<int>();
            TEST_CHECK(left >= 0 && right >= 0 && top >= 0 && bottom >= 0);
            TEST_CHECK(left + right < w && top + bottom < h);
        }
    }
    std::cout << "  - 63 manifest textures match their PNG headers and geometry ok.\n";
}

void test_manifest_bitmap_fonts(const nlohmann::json& m) {
    TEST_CHECK(m.contains("bitmap_fonts"));
    const auto& fonts = m["bitmap_fonts"];
    TEST_CHECK(fonts.is_object());
    TEST_CHECK(fonts.size() == 2);
    TEST_CHECK(fonts.contains("digits_chrome"));
    TEST_CHECK(fonts.contains("digits_white"));
    for (const auto& [name, entry] : fonts.items()) {
        TEST_CHECK(entry.contains("file") && entry["file"].is_string());
        const fs::path path = cabinet_dir() / entry["file"].get<std::string>();
        TEST_CHECK(fs::is_regular_file(path));
        TEST_CHECK(blaze4k::probe_image_header(path.string()).ok);

        // size_px here is the font size, not image dims, so it is not compared.
        TEST_CHECK(entry.contains("glyphs") && entry["glyphs"].is_object());
        const auto& glyphs = entry["glyphs"];
        for (const char c : std::string("0123456789.%/ ")) {
            TEST_CHECK(glyphs.contains(std::string(1, c)));
        }
    }
    std::cout << "  - digit atlases exist and cover 0-9 . % / and space ok.\n";
}

void test_no_orphan_pngs(const nlohmann::json& m) {
    std::set<std::string> referenced;
    for (const auto& [name, entry] : m["textures"].items()) {
        referenced.insert(entry["file"].get<std::string>());
    }
    for (const auto& [name, entry] : m["bitmap_fonts"].items()) {
        referenced.insert(entry["file"].get<std::string>());
    }

    int png_count = 0;
    for (const auto& dirent : fs::directory_iterator(cabinet_dir())) {
        if (dirent.path().extension() != ".png") {
            continue;
        }
        ++png_count;
        TEST_CHECK(referenced.count(dirent.path().filename().string()) == 1);
    }
    TEST_CHECK(png_count == 65);
    std::cout << "  - every cabinet PNG is referenced by the manifest ok.\n";
}

} // namespace

int main() {
    std::cout << "[theme_test] Running Cabinet theme pack tests...\n";
    test_font_files_present();
    const nlohmann::json manifest = load_manifest();
    test_manifest_textures(manifest);
    test_manifest_bitmap_fonts(manifest);
    test_no_orphan_pngs(manifest);
    std::cout << "[theme_test] All tests passed!\n";
    return 0;
}
