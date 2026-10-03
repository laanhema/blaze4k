// #89: Cabinet theme textures by name + bitmap digits. Pins the manifest parser
// against the committed pack, the content-box placement math per kind, the
// load-option policy, and the headless / missing-asset fallbacks. No GL.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "render/gl_quad_renderer.hpp"
#include "render/theme_textures.hpp"

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
using blaze4k::DigitAlign;
using blaze4k::Rect;
using blaze4k::ThemeEntry;
using blaze4k::ThemeKind;
using blaze4k::ThemeManifest;
using blaze4k::TileAnchor;
using blaze4k::UVRect;

const fs::path kAssets{BLAZE4K_ASSETS_DIR};
const fs::path kCabinet = kAssets / "theme" / "cabinet";
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

bool rect_eq(const Rect& r, float x, float y, float w, float h, float eps = 1e-4f) {
    return approx(r.x, x, eps) && approx(r.y, y, eps) && approx(r.w, w, eps) &&
           approx(r.h, h, eps);
}

bool uv_eq(const UVRect& uv, float u0, float v0, float u1, float v1, float eps = 1e-4f) {
    return approx(uv.u0, u0, eps) && approx(uv.v0, v0, eps) && approx(uv.u1, u1, eps) &&
           approx(uv.v1, v1, eps);
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

const ThemeManifest& real_manifest() {
    static const ThemeManifest manifest =
        blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"));
    return manifest;
}

const ThemeEntry& real_entry(const char* name) {
    const auto it = real_manifest().textures.find(name);
    TEST_CHECK(it != real_manifest().textures.end());
    return it->second;
}

const blaze4k::DigitFont& real_font(const char* name) {
    const auto it = real_manifest().fonts.find(name);
    TEST_CHECK(it != real_manifest().fonts.end());
    return it->second;
}

std::size_t count_of(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

// Redirects std::cerr into a buffer for the guard's lifetime.
class CerrCapture {
public:
    CerrCapture() : previous_(std::cerr.rdbuf(buffer_.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(previous_); }
    CerrCapture(const CerrCapture&) = delete;
    CerrCapture& operator=(const CerrCapture&) = delete;
    [[nodiscard]] std::string text() const { return buffer_.str(); }

private:
    std::ostringstream buffer_;
    std::streambuf* previous_;
};

void test_parse_real_manifest() {
    std::vector<std::string> warnings;
    const ThemeManifest manifest =
        blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"), &warnings);
    for (const std::string& w : warnings) {
        std::cerr << "unexpected warning: " << w << "\n";
    }
    TEST_CHECK(warnings.empty());
    TEST_CHECK(manifest.textures.size() == 63);
    TEST_CHECK(manifest.fonts.size() == 2);
    TEST_CHECK(approx(manifest.texture_scale, 2.0f));

    const ThemeEntry& logo = real_entry("logo");
    TEST_CHECK(logo.kind == ThemeKind::Sprite);
    TEST_CHECK(logo.file == "logo.png");
    TEST_CHECK(logo.content.x == 80 && logo.content.y == 80 && logo.content.w == 1960 &&
               logo.content.h == 380);

    const ThemeEntry& chip = real_entry("chip");
    TEST_CHECK(chip.kind == ThemeKind::Slice3 && chip.slice3);
    TEST_CHECK(chip.slice3->left == 44 && chip.slice3->right == 44);

    const ThemeEntry& life_frame = real_entry("life_frame");
    TEST_CHECK(life_frame.kind == ThemeKind::Slice9 && life_frame.slice9);
    TEST_CHECK(life_frame.slice9->top == 16 && life_frame.slice9->right == 8 &&
               life_frame.slice9->bottom == 16 && life_frame.slice9->left == 8);

    const ThemeEntry& banner = real_entry("banner_frame");
    TEST_CHECK(banner.kind == ThemeKind::Frame && banner.hole);
    TEST_CHECK(banner.hole->x == 8 && banner.hole->y == 8 && banner.hole->w == 1120 &&
               banner.hole->h == 314);

    const ThemeEntry& scanlines = real_entry("scanlines");
    TEST_CHECK(scanlines.kind == ThemeKind::Tile && scanlines.screen_pixel_tile);
    TEST_CHECK(scanlines.content.x == 0 && scanlines.content.y == 0 &&
               scanlines.content.w == 1 && scanlines.content.h == 3);
    TEST_CHECK(!real_entry("life_stripes").screen_pixel_tile);
    TEST_CHECK(real_entry("bar_top").kind == ThemeKind::StretchX);
    TEST_CHECK(real_entry("bg_title").kind == ThemeKind::Fullscreen);
    TEST_CHECK(real_entry("life_fill").kind == ThemeKind::Stretch);

    const blaze4k::DigitFont& white = real_font("digits_white");
    TEST_CHECK(white.file == "digits_white.png");
    const blaze4k::DigitGlyph& one =
        white.glyphs[static_cast<std::size_t>(blaze4k::digit_glyph_index('1'))];
    TEST_CHECK(one.present);
    TEST_CHECK(one.src.x == 148 && one.src.y == 0 && one.src.w == 92 && one.src.h == 164);
    TEST_CHECK(approx(one.advance, 34.03f) && approx(one.origin_x, 28.0f));
    for (const blaze4k::DigitGlyph& glyph : real_font("digits_chrome").glyphs) {
        TEST_CHECK(glyph.present);
    }
    TEST_CHECK(blaze4k::digit_glyph_index('0') == 0 && blaze4k::digit_glyph_index(' ') == 13);
    TEST_CHECK(blaze4k::digit_glyph_index('a') == -1 && blaze4k::digit_glyph_index('\0') == -1);
    std::cout << "  - real manifest: 63 textures, 2 digit fonts, no warnings ok.\n";
}

void test_parse_malformed_never_throws() {
    for (const char* text : {"", "not json", "[]", "{\"textures\": 5}",
                             "{\"textures\": {\"a\": 7}}", "{\"bitmap_fonts\": []}", "null",
                             "{\"textures\": {\"a\": {\"file\": \"a.png\"}}}"}) {
        std::vector<std::string> warnings;
        const ThemeManifest manifest = blaze4k::parse_theme_manifest(text, &warnings);
        TEST_CHECK(manifest.textures.empty() && manifest.fonts.empty());
        TEST_CHECK(!warnings.empty());
    }
    // Null warnings sink is fine too.
    TEST_CHECK(blaze4k::parse_theme_manifest("{oops").textures.empty());

    const char* mixed = R"({
      "texture_scale": 0,
      "textures": {
        "good":        {"file": "good.png", "kind": "sprite", "size_px": [10, 10], "content_px": [1, 1, 8, 8]},
        "size_string": {"file": "a.png", "kind": "sprite", "size_px": "10x10"},
        "content_oob": {"file": "a.png", "kind": "sprite", "size_px": [10, 10], "content_px": [5, 5, 6, 6]},
        "content_neg": {"file": "a.png", "kind": "sprite", "size_px": [10, 10], "content_px": [-1, 0, 4, 4]},
        "slice_wide":  {"file": "a.png", "kind": "slice3", "size_px": [10, 10], "slice3_px": {"left": 5, "right": 5}},
        "frame_hole":  {"file": "a.png", "kind": "frame", "size_px": [10, 10]},
        "dotdot":      {"file": "../x.png", "kind": "sprite", "size_px": [10, 10]},
        "slash":       {"file": "a/b.png", "kind": "sprite", "size_px": [10, 10]},
        "jpg":         {"file": "x.jpg", "kind": "sprite", "size_px": [10, 10]},
        "bad_kind":    {"file": "a.png", "kind": "hexagon", "size_px": [10, 10]},
        "huge":        {"file": "a.png", "kind": "sprite", "size_px": [100000, 10]},
        "float_size":  {"file": "a.png", "kind": "sprite", "size_px": [10.5, 10]},
        "slice9_tall": {"file": "a.png", "kind": "slice9", "size_px": [10, 10], "slice9_px": {"top": 5, "right": 1, "bottom": 5, "left": 1}}
      },
      "bitmap_fonts": {
        "digits_white": {"file": "d.png", "glyphs": {
          "1": {"x": 0, "y": 0, "w": 4, "h": 4, "origin_x": 1, "advance": 3},
          "x": {"x": 0, "y": 0, "w": 4, "h": 4, "origin_x": 1, "advance": 3},
          "2": {"x": 0, "y": 0, "w": 0, "h": 4, "origin_x": 1, "advance": 3}
        }},
        "no_glyphs": {"file": "d.png", "glyphs": {}},
        "bad_file":  {"file": "C:evil.png", "glyphs": {"1": {"x": 0, "y": 0, "w": 4, "h": 4, "origin_x": 1, "advance": 3}}}
      }
    })";
    std::vector<std::string> warnings;
    const ThemeManifest manifest = blaze4k::parse_theme_manifest(mixed, &warnings);
    TEST_CHECK(manifest.textures.size() == 1);
    TEST_CHECK(manifest.textures.contains("good"));
    // 12 bad textures + texture_scale + glyph 'x' + glyph '2' + 2 bad fonts.
    TEST_CHECK(warnings.size() == 17);
    for (const char* name : {"size_string", "content_oob", "content_neg", "slice_wide",
                             "frame_hole", "dotdot", "slash", "jpg", "bad_kind", "huge",
                             "float_size", "slice9_tall"}) {
        bool named = false;
        for (const std::string& w : warnings) {
            named = named || w.find(std::string("'") + name + "'") != std::string::npos;
        }
        TEST_CHECK(named);
    }
    TEST_CHECK(approx(manifest.texture_scale, 2.0f));
    TEST_CHECK(manifest.fonts.size() == 1);
    const blaze4k::DigitFont& font = manifest.fonts.at("digits_white");
    TEST_CHECK(font.glyphs[1].present && !font.glyphs[2].present);
    std::cout << "  - malformed manifests never throw; bad entries skipped with a warning ok.\n";
}

void test_sprite_content_box_placement() {
    const ThemeEntry& logo = real_entry("logo");
    const Rect content = blaze4k::sprite_content_rect(logo, {150.0f, 168.0f}, 0.5f);
    TEST_CHECK(rect_eq(content, 150, 168, 980, 190)); // == layout_720p
    TEST_CHECK(rect_eq(blaze4k::content_to_image_rect(logo, content), 110, 128, 1060, 270));

    const Rect content2 = blaze4k::sprite_content_rect(logo, {300.0f, 336.0f}, 1.0f);
    TEST_CHECK(rect_eq(blaze4k::content_to_image_rect(logo, content2), 220, 256, 2120, 540));

    // Degenerate scale / rect: empty.
    TEST_CHECK(rect_eq(blaze4k::sprite_content_rect(logo, {0, 0}, 0.0f), 0, 0, 0, 0));
    TEST_CHECK(rect_eq(blaze4k::sprite_content_rect(logo, {0, 0}, kNaN), 0, 0, 0, 0));
    TEST_CHECK(rect_eq(blaze4k::content_to_image_rect(logo, {0, 0, -5, 10}), 0, 0, 0, 0));

    // Fullscreen: content == image, so it just fills the rect; stretch is per-axis.
    TEST_CHECK(rect_eq(blaze4k::content_to_image_rect(real_entry("bg_title"), {0, 0, 1920, 1080}),
                       0, 0, 1920, 1080));

    blaze4k::ThemeTextures theme;
    {
        CerrCapture quiet;
        TEST_CHECK(theme.load(kCabinet));
    }
    const blaze4k::Vec2 size = theme.content_size("logo", 1.0f);
    TEST_CHECK(approx(size.x, 980.0f) && approx(size.y, 190.0f));
    {
        CerrCapture quiet;
        const blaze4k::Vec2 none = theme.content_size("no_such_texture", 1.0f);
        TEST_CHECK(none.x == 0.0f && none.y == 0.0f);
    }
    std::cout << "  - sprite places content_px, padding hangs outside ok.\n";
}

void test_slice3_rects() {
    const ThemeEntry& chip = real_entry("chip");
    const auto pieces = blaze4k::slice3_pieces(chip, {100, 50, 140, 30}); // k = 0.5
    const float iw = 296.0f;
    TEST_CHECK(rect_eq(pieces[0].dst, 96, 46, 22, 38));
    TEST_CHECK(uv_eq(pieces[0].uv, 0, 0, 44 / iw, 1));
    TEST_CHECK(rect_eq(pieces[1].dst, 118, 46, 104, 38));
    TEST_CHECK(uv_eq(pieces[1].uv, 44 / iw, 0, 252 / iw, 1));
    TEST_CHECK(rect_eq(pieces[2].dst, 222, 46, 22, 38));
    TEST_CHECK(uv_eq(pieces[2].uv, 252 / iw, 0, 1, 1));
    // Image rect = union of the pieces = {96, 46, 148, 38}.
    TEST_CHECK(approx(pieces[2].dst.x + pieces[2].dst.w - pieces[0].dst.x, 148.0f));

    // Narrow: image w = 4 + 16*0.5 = 12 < caps (44): caps shrink to 6, middle 0.
    const auto narrow = blaze4k::slice3_pieces(chip, {100, 50, 4, 30});
    TEST_CHECK(approx(narrow[0].dst.w, 6.0f) && approx(narrow[2].dst.w, 6.0f));
    TEST_CHECK(approx(narrow[1].dst.w, 0.0f));
    TEST_CHECK(approx(narrow[0].dst.x, 96.0f) && approx(narrow[2].dst.x, 102.0f));

    // Asymmetric caps measured from the image edges (padding included).
    const ThemeEntry& row = real_entry("diff_row_challenge_selected"); // content x=52, left 432
    const auto rp = blaze4k::slice3_pieces(row, {0, 0, 564, 52});      // k = 0.5
    TEST_CHECK(approx(rp[0].dst.x, -26.0f) && approx(rp[0].dst.w, 216.0f));
    TEST_CHECK(approx(rp[2].dst.w, 72.0f));

    // Degenerate input and a non-slice3 entry: all-empty pieces.
    for (const auto& piece : blaze4k::slice3_pieces(chip, {0, 0, 10, 0})) {
        TEST_CHECK(piece.dst.w == 0.0f && piece.dst.h == 0.0f);
    }
    for (const auto& piece : blaze4k::slice3_pieces(real_entry("logo"), {0, 0, 10, 10})) {
        TEST_CHECK(piece.dst.w == 0.0f && piece.dst.h == 0.0f);
    }
    std::cout << "  - slice3 caps keep aspect, middle stretches, narrow shrinks ok.\n";
}

void test_slice9_rects() {
    const ThemeEntry& frame = real_entry("life_frame");
    const auto pieces = blaze4k::slice9_pieces(frame, {40, 120, 40, 480}, 0.5f);
    TEST_CHECK(rect_eq(pieces[0].dst, 40, 120, 4, 8));
    TEST_CHECK(uv_eq(pieces[0].uv, 0, 0, 0.1f, 16.0f / 960.0f));
    TEST_CHECK(rect_eq(pieces[4].dst, 44, 128, 32, 464));
    TEST_CHECK(uv_eq(pieces[4].uv, 0.1f, 16.0f / 960.0f, 0.9f, 944.0f / 960.0f));
    TEST_CHECK(rect_eq(pieces[8].dst, 76, 592, 4, 8));
    TEST_CHECK(uv_eq(pieces[8].uv, 0.9f, 944.0f / 960.0f, 1, 1));
    TEST_CHECK(rect_eq(pieces[1].dst, 44, 120, 32, 8)); // top edge
    TEST_CHECK(rect_eq(pieces[3].dst, 40, 128, 4, 464)); // left edge

    // Shrink: h = 10 < 8 + 8, so the top/bottom borders become 5 each, centre 0.
    const auto small = blaze4k::slice9_pieces(frame, {40, 120, 40, 10}, 0.5f);
    TEST_CHECK(approx(small[0].dst.h, 5.0f) && approx(small[6].dst.h, 5.0f));
    TEST_CHECK(approx(small[4].dst.h, 0.0f));
    TEST_CHECK(approx(small[6].dst.y, 125.0f));

    for (const auto& piece : blaze4k::slice9_pieces(frame, {0, 0, 40, 480}, kNaN)) {
        TEST_CHECK(piece.dst.w == 0.0f && piece.dst.h == 0.0f);
    }
    std::cout << "  - slice9 borders scale by s/T, shrink proportionally ok.\n";
}

void test_frame_rect() {
    const ThemeEntry& banner = real_entry("banner_frame");
    const Rect image = blaze4k::frame_image_rect(banner, {48, 100, 560, 157});
    TEST_CHECK(rect_eq(image, 44, 96, 568, 165)); // == layout_720p
    const auto ring = blaze4k::frame_ring_rects(image, {48, 100, 560, 157});
    TEST_CHECK(rect_eq(ring[0], 44, 96, 568, 4));
    TEST_CHECK(rect_eq(ring[1], 44, 257, 568, 4));
    TEST_CHECK(rect_eq(ring[2], 44, 100, 4, 157));
    TEST_CHECK(rect_eq(ring[3], 608, 100, 4, 157));
    TEST_CHECK(rect_eq(blaze4k::frame_image_rect(real_entry("logo"), {0, 0, 10, 10}), 0, 0, 0, 0));
    std::cout << "  - frame places hole_px over the banner rect ok.\n";
}

void test_tile_uv() {
    const ThemeEntry& stripes = real_entry("life_stripes");
    const UVRect bottom = blaze4k::tile_uv(stripes, {44, 128, 32, 464}, 0.5f, TileAnchor::Bottom);
    TEST_CHECK(approx(bottom.u0, 0.0f) && approx(bottom.u1, 4.0f));
    TEST_CHECK(approx(bottom.v1, 1.0f));
    TEST_CHECK(approx(bottom.v1 - bottom.v0, 464.0f / 12.0f));
    const UVRect top = blaze4k::tile_uv(stripes, {44, 128, 32, 464}, 0.5f, TileAnchor::TopLeft);
    TEST_CHECK(approx(top.v0, 0.0f) && approx(top.v1, 464.0f / 12.0f));

    const ThemeEntry& scanlines = real_entry("scanlines");
    for (const float k : {1.0f, 0.5f}) { // independent of s
        const UVRect uv = blaze4k::tile_uv(scanlines, {0, 0, 2560, 1440}, k, TileAnchor::TopLeft);
        TEST_CHECK(uv_eq(uv, 0, 0, 2560, 480));
    }
    const UVRect none = blaze4k::tile_uv(stripes, {0, 0, 10, 10}, 0.0f, TileAnchor::TopLeft);
    TEST_CHECK(none.u1 == 0.0f && none.v1 == 0.0f);
    std::cout << "  - tile UVs repeat per tile, scanlines at 1 texel per pixel ok.\n";
}

void test_fill_cropped_uv() {
    const ThemeEntry& fill = real_entry("life_fill");
    const Rect bar{44, 128, 32, 464};
    const auto quarter = blaze4k::fill_cropped_piece(fill, bar, 0.25f);
    TEST_CHECK(quarter);
    TEST_CHECK(rect_eq(quarter->dst, 44, 476, 32, 116));
    TEST_CHECK(uv_eq(quarter->uv, 0, 0.75f, 1, 1));

    const auto full = blaze4k::fill_cropped_piece(fill, bar, 1.0f);
    TEST_CHECK(full && rect_eq(full->dst, 44, 128, 32, 464) && uv_eq(full->uv, 0, 0, 1, 1));
    const auto over = blaze4k::fill_cropped_piece(fill, bar, 2.0f);
    TEST_CHECK(over && rect_eq(over->dst, 44, 128, 32, 464) && uv_eq(over->uv, 0, 0, 1, 1));

    TEST_CHECK(!blaze4k::fill_cropped_piece(fill, bar, 0.0f));
    TEST_CHECK(!blaze4k::fill_cropped_piece(fill, bar, -1.0f));
    TEST_CHECK(!blaze4k::fill_cropped_piece(fill, bar, kNaN));
    TEST_CHECK(!blaze4k::fill_cropped_piece(fill, {0, 0, 0, 10}, 0.5f));

    // The gradient does not slide: v = 0.75 lands on the same screen y at any fill.
    const auto screen_y = [](const blaze4k::ThemePiece& piece, float v) {
        return piece.dst.y + (v - piece.uv.v0) / (piece.uv.v1 - piece.uv.v0) * piece.dst.h;
    };
    const auto half = blaze4k::fill_cropped_piece(fill, bar, 0.5f);
    TEST_CHECK(half);
    TEST_CHECK(approx(screen_y(*half, 0.75f), screen_y(*quarter, 0.75f)));
    TEST_CHECK(approx(screen_y(*half, 0.75f), 476.0f));
    std::cout << "  - cropped fill keeps the gradient fixed to the bar ok.\n";
}

void test_digit_layout_widths() {
    const blaze4k::DigitFont& white = real_font("digits_white");
    const blaze4k::DigitFont& chrome = real_font("digits_chrome");
    TEST_CHECK(approx(blaze4k::digits_width(white, "100%", 0.5f), 147.435f, 1e-3f));
    TEST_CHECK(approx(blaze4k::digits_width(chrome, "98.5", 0.5f), 190.795f, 1e-3f));
    TEST_CHECK(approx(blaze4k::digits_width(white, "1a2", 0.5f),
                      blaze4k::digits_width(white, "12", 0.5f)));
    TEST_CHECK(blaze4k::digits_width(white, "", 0.5f) == 0.0f);
    TEST_CHECK(blaze4k::digits_width(white, "12", kNaN) == 0.0f);

    TEST_CHECK(approx(blaze4k::digits_start_x(500, 100, DigitAlign::Right), 400.0f));
    TEST_CHECK(approx(blaze4k::digits_start_x(500, 100, DigitAlign::Centre), 450.0f));
    TEST_CHECK(approx(blaze4k::digits_start_x(500, 100, DigitAlign::Left), 500.0f));

    const blaze4k::DigitGlyph& one =
        white.glyphs[static_cast<std::size_t>(blaze4k::digit_glyph_index('1'))];
    TEST_CHECK(rect_eq(blaze4k::digit_glyph_rect(one, 10, 20, 0.5f), -4, 20, 46, 82));
    TEST_CHECK(uv_eq(blaze4k::digit_glyph_uv(one, 1722, 164), 148.0f / 1722.0f, 0,
                     240.0f / 1722.0f, 1));
    TEST_CHECK(uv_eq(blaze4k::digit_glyph_uv(one, 0, 164), 0, 0, 1, 1));

    // Through the owner: measure() uses s / texture_scale.
    blaze4k::ThemeTextures theme;
    {
        CerrCapture quiet;
        TEST_CHECK(theme.load(kCabinet));
    }
    TEST_CHECK(theme.digits_white().valid());
    TEST_CHECK(approx(theme.digits_white().measure("100%", 1.0f), 147.435f, 1e-3f));
    TEST_CHECK(approx(theme.digits_chrome().measure("98.5", 2.0f), 381.59f, 1e-3f));
    TEST_CHECK(approx(theme.digits_white().height(1.0f), 82.0f));
    std::cout << "  - digit widths, alignment and glyph quads ok.\n";
}

void test_load_options() {
    const auto options = [](const char* name) {
        return blaze4k::theme_texture_options(name, real_entry(name));
    };
    for (const char* name : {"logo", "grade_S", "grade_quad_star", "judgment_miss"}) {
        TEST_CHECK(options(name).mipmaps);
    }
    for (const char* name : {"bg_title", "chip"}) {
        TEST_CHECK(!options(name).mipmaps);
    }
    for (const char* name : {"digits_chrome", "digits_white"}) {
        TEST_CHECK(!blaze4k::theme_texture_options(name, ThemeEntry{}).mipmaps);
    }
    using Wrap = blaze4k::Texture::Wrap;
    using Filter = blaze4k::Texture::Filter;
    TEST_CHECK(options("life_stripes").wrap == Wrap::Repeat &&
               options("life_stripes").filter == Filter::Linear);
    TEST_CHECK(options("scanlines").wrap == Wrap::Repeat &&
               options("scanlines").filter == Filter::Nearest);
    TEST_CHECK(options("chip").wrap == Wrap::Clamp && options("chip").filter == Filter::Linear);

    TEST_CHECK(approx(blaze4k::theme_fallback_color(ThemeKind::Sprite, {1, 1, 1, 1}).a, 0.25f));
    const blaze4k::Color navy = blaze4k::theme_fallback_color(ThemeKind::Fullscreen, {});
    TEST_CHECK(approx(navy.a, 1.0f) && navy.r < 0.05f && navy.b < 0.05f);
    std::cout << "  - load options: mipmaps/Repeat/Nearest per texture ok.\n";
}

void exercise_all_draws(const blaze4k::ThemeTextures& theme, blaze4k::GlQuadRenderer& renderer) {
    for (const float s : {1.0f, 0.0f, -1.0f, kNaN}) {
        theme.draw_sprite(renderer, "logo", {150 * s, 168 * s}, s);
        theme.draw_stretch(renderer, "bg_title", {0, 0, 1280 * s, 720 * s});
        theme.draw_stretch_x(renderer, "bar_top", 0, 0, 1280 * s, s);
        theme.draw_slice3(renderer, "chip", {100, 50, 140 * s, 30 * s});
        theme.draw_slice9(renderer, "life_frame", {40, 120, 40 * s, 480 * s}, s);
        theme.draw_frame(renderer, "banner_frame", {48, 100, 560 * s, 157 * s});
        theme.draw_tiled(renderer, "life_stripes", {44, 128, 32 * s, 464 * s}, s,
                         blaze4k::Color{}, TileAnchor::Bottom);
        theme.draw_tiled(renderer, "scanlines", {0, 0, 1280 * s, 720 * s}, s);
        theme.draw_fill_cropped(renderer, "life_fill", {44, 128, 32, 464}, 0.5f * s);
        theme.digits_chrome().draw(renderer, "98.76%", 640, 500 * s, s, DigitAlign::Centre);
        theme.digits_white().draw(renderer, "1/2 3", 640, 500, s, DigitAlign::Right);
    }
}

void test_headless_load_and_draw() {
    blaze4k::GlQuadRenderer renderer; // uninitialized: every draw is a no-op
    blaze4k::ThemeTextures theme;
    std::string log;
    {
        CerrCapture capture;
        TEST_CHECK(theme.load(kCabinet));
        log = capture.text();
    }
    TEST_CHECK(theme.entry_count() == 63);
    TEST_CHECK(theme.loaded_texture_count() == 0);
    TEST_CHECK(theme.entry("logo") != nullptr && theme.entry("nope") == nullptr);
    TEST_CHECK(!theme.digits_chrome().has_texture() && theme.digits_chrome().valid());
    // Headless logs exactly one line (no per-PNG [Texture] spam).
    TEST_CHECK(count_of(log, "\n") == 1);
    TEST_CHECK(log.find("No GL context") != std::string::npos);
    TEST_CHECK(log.find("[Texture]") == std::string::npos);

    {
        CerrCapture capture;
        exercise_all_draws(theme, renderer);
        exercise_all_draws(theme, renderer);
        // Unsupported digit bytes are logged once per font.
        theme.digits_white().draw(renderer, "1a2", 0, 0, 1.0f, DigitAlign::Left);
        theme.digits_white().draw(renderer, "1b2", 0, 0, 1.0f, DigitAlign::Left);
        TEST_CHECK(count_of(capture.text(), "Digit font 'digits_white' has no glyph") == 1);
    }

    {
        CerrCapture capture;
        TEST_CHECK(!theme.load("/no/such/dir-89"));
        TEST_CHECK(count_of(capture.text(), "\n") == 1);
        TEST_CHECK(theme.entry_count() == 0);
        TEST_CHECK(!theme.digits_chrome().valid());
        exercise_all_draws(theme, renderer);
        exercise_all_draws(theme, renderer);
    }
    {
        CerrCapture capture;
        TEST_CHECK(!theme.load(fs::path{}));
        TEST_CHECK(count_of(capture.text(), "\n") == 1);
    }
    theme.shutdown();
    theme.shutdown();
    TEST_CHECK(theme.entry_count() == 0);
    std::cout << "  - headless: one log line, all draws are safe fallbacks ok.\n";
}

void test_missing_png_and_log_once() {
    const fs::path dir = fs::temp_directory_path() / "blaze4k_theme_textures_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    fs::copy_file(kCabinet / "scanlines.png", dir / "ok.png");
    fs::copy_file(kCabinet / "scanlines.png", dir / "wrong.png");
    {
        std::ofstream out(dir / "manifest.json", std::ios::binary);
        out << R"json({"texture_scale": 2, "textures": {
          "ok":    {"file": "ok.png", "kind": "tile", "size_px": [1, 3], "scale": "1x (screen pixels)"},
          "wrong": {"file": "wrong.png", "kind": "sprite", "size_px": [2, 2]},
          "gone":  {"file": "gone.png", "kind": "slice3", "size_px": [10, 4], "slice3_px": {"left": 2, "right": 2}}
        }})json";
    }

    blaze4k::GlQuadRenderer renderer;
    blaze4k::ThemeTextures theme;
    {
        CerrCapture capture;
        TEST_CHECK(theme.load(dir));
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "wrong.png") == 1);
        TEST_CHECK(count_of(log, "gone.png") == 1);
        TEST_CHECK(count_of(log, "ok.png") == 0);
        TEST_CHECK(count_of(log, "No GL context") == 1);
    }
    TEST_CHECK(theme.entry_count() == 3);
    TEST_CHECK(theme.entry("ok")->screen_pixel_tile);

    {
        CerrCapture capture;
        theme.draw_tiled(renderer, "ok", {0, 0, 100, 100}, 1.0f);
        theme.draw_sprite(renderer, "wrong", {0, 0}, 1.0f);
        theme.draw_slice3(renderer, "gone", {0, 0, 50, 2});
        for (int i = 0; i < 3; ++i) {
            theme.draw_sprite(renderer, "nope", {0, 0}, 1.0f);
            theme.draw_frame(renderer, "nope", {0, 0, 10, 10});
            theme.digits_chrome().draw(renderer, "12", 0, 0, 1.0f, DigitAlign::Left);
        }
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "'nope'") == 1);
        TEST_CHECK(count_of(log, "Digit font 'digits_chrome' unavailable") == 1);
        TEST_CHECK(count_of(log, "'ok'") == 0 && count_of(log, "'gone'") == 0);
    }

    fs::remove_all(dir);
    TEST_CHECK(!fs::exists(dir));
    std::cout << "  - missing/mis-sized PNG warns once at load, unknown name once at draw ok.\n";
}

} // namespace

int main() {
    std::cout << "Running theme textures tests...\n";
    test_parse_real_manifest();
    test_parse_malformed_never_throws();
    test_sprite_content_box_placement();
    test_slice3_rects();
    test_slice9_rects();
    test_frame_rect();
    test_tile_uv();
    test_fill_cropped_uv();
    test_digit_layout_widths();
    test_load_options();
    test_headless_load_and_draw();
    test_missing_png_and_log_once();
    std::cout << "All theme textures tests passed.\n";
    return 0;
}
