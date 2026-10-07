// #90: TrueType text rendering. Headless: faces load from the committed
// assets/fonts, atlases bake on the CPU, and layout runs without GL.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/ttf_font.hpp"
#include "render/unicode_text.hpp"

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
namespace theme = blaze4k::theme;
using blaze4k::FontAtlas;
using blaze4k::FontFace;
using blaze4k::GlyphQuad;
using blaze4k::TextAlign;
using blaze4k::TextLayout;
using blaze4k::TextRenderer;
using blaze4k::with_alpha;

const fs::path kSourceDir{BLAZE4K_SOURCE_DIR};
const fs::path kTempRoot = fs::temp_directory_path() / "blaze4k_ttf_font_test";

bool approx(float a, float b, float tolerance = 1e-3f) {
    return std::fabs(a - b) <= tolerance;
}

fs::path font_path(theme::Font font) {
    return kSourceDir / theme::kFontFiles[static_cast<std::size_t>(font)];
}

std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    TEST_CHECK(file.good());
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    TEST_CHECK(file.good());
}

FontFace load_face(theme::Font font) {
    std::string error;
    std::optional<FontFace> face = FontFace::from_file(font_path(font), &error);
    if (!face) {
        std::cerr << "could not load " << font_path(font) << ": " << error << "\n";
    }
    TEST_CHECK(face.has_value());
    return std::move(*face);
}

FontFace load_symbol_face() {
    std::string error;
    std::optional<FontFace> face = FontFace::from_file(kSourceDir / theme::kSymbolFontFile, &error,
                                                       blaze4k::kSymbolProbeCodePoint);
    if (!face) {
        std::cerr << "could not load the symbol font: " << error << "\n";
    }
    TEST_CHECK(face.has_value());
    return std::move(*face);
}

FontAtlas bake(const FontFace& face, float pixel_size) {
    std::string error;
    std::optional<FontAtlas> atlas = FontAtlas::bake(face, pixel_size, 4096, &error);
    if (!atlas) {
        std::cerr << "bake failed: " << error << "\n";
    }
    TEST_CHECK(atlas.has_value());
    return std::move(*atlas);
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

std::vector<GlyphQuad> collect(const FontFace& face, const FontAtlas& atlas, std::string_view text,
                               float x, float y, const TextLayout& layout) {
    std::vector<GlyphQuad> quads;
    blaze4k::for_each_text_quad(face, atlas, text, x, y, layout,
                                [&](const GlyphQuad& quad) { quads.push_back(quad); });
    return quads;
}

TextLayout plain_layout(float pixel_size) {
    TextLayout layout;
    layout.pixel_size = pixel_size;
    return layout;
}

bool same_color(blaze4k::Color a, blaze4k::Color b) {
    return approx(a.r, b.r) && approx(a.g, b.g) && approx(a.b, b.b) && approx(a.a, b.a);
}

// --- 1. validate_sfnt ----------------------------------------------------------------------

void patch_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

// Directory record index of `tag` in a real font (asserts it exists).
std::size_t record_of(const std::vector<std::uint8_t>& bytes, const char* tag) {
    const std::size_t count = static_cast<std::size_t>((bytes[4] << 8) | bytes[5]);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t record = 12 + 16 * i;
        if (std::equal(tag, tag + 4, bytes.begin() + static_cast<std::ptrdiff_t>(record))) {
            return record;
        }
    }
    TEST_CHECK(false);
    return 0;
}

void expect_invalid(const std::vector<std::uint8_t>& bytes, const char* what) {
    std::string error;
    const bool ok = blaze4k::validate_sfnt(bytes, &error);
    if (ok || error.empty()) {
        std::cerr << "expected validate_sfnt to reject: " << what << "\n";
    }
    TEST_CHECK(!ok);
    TEST_CHECK(!error.empty());
    // Never handed to stb either.
    TEST_CHECK(!FontFace::from_bytes(bytes, nullptr).has_value());
}

void test_validate_sfnt() {
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        const std::vector<std::uint8_t> bytes = read_bytes(font_path(static_cast<theme::Font>(i)));
        std::string error;
        TEST_CHECK(blaze4k::validate_sfnt(bytes, &error));
        TEST_CHECK(error.empty());
    }
    TEST_CHECK(!blaze4k::validate_sfnt({}, nullptr)); // null error is fine

    const std::vector<std::uint8_t> real = read_bytes(font_path(theme::Font::SairaBold));
    expect_invalid({}, "empty");
    expect_invalid(std::vector<std::uint8_t>(11, 0), "11 bytes");

    std::mt19937 rng{90};
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::vector<std::uint8_t> random(4096);
    for (auto& byte : random) {
        byte = static_cast<std::uint8_t>(byte_dist(rng));
    }
    expect_invalid(random, "random 4 KiB");

    std::vector<std::uint8_t> otto = real;
    patch_u32(otto, 0, 0x4F54544Fu); // 'OTTO'
    expect_invalid(otto, "OTTO");
    std::vector<std::uint8_t> ttcf = real;
    patch_u32(ttcf, 0, 0x74746366u); // 'ttcf'
    expect_invalid(ttcf, "ttcf");

    std::vector<std::uint8_t> zero_tables = real;
    zero_tables[4] = 0;
    zero_tables[5] = 0;
    expect_invalid(zero_tables, "numTables 0");
    std::vector<std::uint8_t> many_tables = real;
    many_tables[4] = 1000 >> 8;
    many_tables[5] = 1000 & 0xFF;
    expect_invalid(many_tables, "numTables 1000");

    std::vector<std::uint8_t> past_eof = real;
    patch_u32(past_eof, record_of(past_eof, "hmtx") + 8, 0xFFFFFF00u); // offset
    expect_invalid(past_eof, "table offset past EOF");
    std::vector<std::uint8_t> long_table = real;
    patch_u32(long_table, record_of(long_table, "cmap") + 12, 0xFFFFFFFFu); // length (64-bit math)
    expect_invalid(long_table, "table length overflow");

    std::vector<std::uint8_t> no_glyf = real;
    no_glyf[record_of(no_glyf, "glyf") + 3] = 'X'; // "glyX"
    expect_invalid(no_glyf, "missing glyf");

    std::vector<std::uint8_t> bad_upem = real;
    const std::size_t head_record = record_of(bad_upem, "head");
    const std::size_t head_offset =
        (static_cast<std::size_t>(bad_upem[head_record + 8]) << 24) |
        (static_cast<std::size_t>(bad_upem[head_record + 9]) << 16) |
        (static_cast<std::size_t>(bad_upem[head_record + 10]) << 8) |
        static_cast<std::size_t>(bad_upem[head_record + 11]);
    bad_upem[head_offset + 18] = 0;
    bad_upem[head_offset + 19] = 3;
    expect_invalid(bad_upem, "unitsPerEm 3");

    // A second 'head' record (stb reads the first, so a duplicate would go
    // unchecked): copy the head record over the optional 'post' record.
    std::vector<std::uint8_t> dup_head = real;
    const std::size_t post_record = record_of(dup_head, "post");
    std::copy_n(real.begin() + static_cast<std::ptrdiff_t>(head_record), 16,
                dup_head.begin() + static_cast<std::ptrdiff_t>(post_record));
    expect_invalid(dup_head, "duplicate head");
    {
        std::string error;
        TEST_CHECK(!blaze4k::validate_sfnt(dup_head, &error));
        TEST_CHECK(error.find("duplicate table 'head'") != std::string::npos);
    }

    expect_invalid(std::vector<std::uint8_t>(real.begin(), real.begin() + 1024), "1 KiB prefix");
    expect_invalid(std::vector<std::uint8_t>(real.begin(),
                                             real.begin() + static_cast<std::ptrdiff_t>(real.size() / 2)),
                   "50% prefix");
    std::cout << "  - validate_sfnt accepts the bundled fonts and rejects corrupt ones ok.\n";
}

// --- 2. Corrupt or missing files ----------------------------------------------------------

void test_corrupt_and_missing_files() {
    fs::remove_all(kTempRoot);
    fs::create_directories(kTempRoot);

    std::string error;
    TEST_CHECK(!FontFace::from_file(kTempRoot / "no-such-font.ttf", &error).has_value());
    TEST_CHECK(!error.empty());

    write_bytes(kTempRoot / "empty.ttf", {});
    error.clear();
    TEST_CHECK(!FontFace::from_file(kTempRoot / "empty.ttf", &error).has_value());
    TEST_CHECK(!error.empty());

    write_bytes(kTempRoot / "garbage.ttf", std::vector<std::uint8_t>(5000, 0xAB));
    error.clear();
    TEST_CHECK(!FontFace::from_file(kTempRoot / "garbage.ttf", &error).has_value());
    TEST_CHECK(!error.empty());

    const std::vector<std::uint8_t> real = read_bytes(font_path(theme::Font::SairaBold));
    write_bytes(kTempRoot / "truncated.ttf",
                std::vector<std::uint8_t>(real.begin(), real.begin() + 30000));
    error.clear();
    TEST_CHECK(!FontFace::from_file(kTempRoot / "truncated.ttf", &error).has_value());
    TEST_CHECK(!error.empty());

    // A pack root where SairaCondensed-Bold.ttf is corrupt.
    const fs::path root = kTempRoot / "pack";
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        const fs::path rel = theme::kFontFiles[i];
        fs::create_directories((root / rel).parent_path());
        fs::copy_file(kSourceDir / rel, root / rel, fs::copy_options::overwrite_existing);
    }
    fs::copy_file(kSourceDir / theme::kSymbolFontFile, root / theme::kSymbolFontFile,
                  fs::copy_options::overwrite_existing);
    write_bytes(root / theme::kFontFiles[static_cast<std::size_t>(theme::Font::SairaBold)],
                std::vector<std::uint8_t>(real.begin(), real.begin() + 1024));

    TextRenderer renderer;
    {
        CerrCapture capture;
        TEST_CHECK(renderer.load(root));
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "\n") == 1);
        TEST_CHECK(count_of(log, "SairaCondensed-Bold.ttf") == 1);
        TEST_CHECK(count_of(log, "Font unavailable") == 1);
    }
    TEST_CHECK(!renderer.font_available(theme::Font::SairaBold));
    TEST_CHECK(renderer.font_available(theme::Font::Audiowide));
    TEST_CHECK(renderer.font_available(theme::Font::SairaMedium));
    TEST_CHECK(renderer.font_available(theme::Font::SairaExtraBold));

    // Bitmap-fallback measure: visible * (6 * P / 10 + tracking), s = 1.
    const theme::TextStyle& footer = theme::text::kFooter; // SairaBold 18, tracking 4
    TEST_CHECK(approx(renderer.measure("ABC", footer), 3.0f * (6.0f * 18.0f / 10.0f + 4.0f)));
    TEST_CHECK(approx(renderer.measure("e\xCC\x81", footer), 6.0f * 1.8f + 4.0f));
    TEST_CHECK(approx(renderer.line_height(footer), 1.2f * 18.0f));
    // Fallback rows end on the reported ascent: centred in the 1.2em box with no
    // face, on the face ascent when the face loaded but its atlas did not.
    TEST_CHECK(approx(renderer.ascent(footer), 0.95f * 18.0f));
    TEST_CHECK(approx(blaze4k::bitmap_fallback_baseline(nullptr, 18.0f), 0.95f * 18.0f));
    const FontFace loaded = load_face(theme::Font::SairaExtraBold);
    const float title_px = theme::text::kSongTitle.size_px;
    TEST_CHECK(approx(blaze4k::bitmap_fallback_baseline(&loaded, title_px),
                      renderer.ascent(theme::text::kSongTitle)));
    TEST_CHECK(approx(blaze4k::bitmap_fallback_baseline(&loaded, title_px),
                      static_cast<float>(loaded.ascent()) * loaded.em_scale(title_px)));
    TEST_CHECK(blaze4k::bitmap_fallback_baseline(&loaded, 0.0f) == 0.0f);
    TEST_CHECK(blaze4k::bitmap_fallback_baseline(nullptr, std::nanf("")) == 0.0f);
    // Truncation stays consistent with the fallback measure.
    const std::string cut = renderer.truncate("ABCDEFGHIJ", footer, 60.0f);
    TEST_CHECK(renderer.measure(cut, footer) <= 60.0f + 1e-3f);
    TEST_CHECK(cut == "A...");
    // Coverage for a missing font answers for the bitmap fallback.
    TEST_CHECK(renderer.covers_text("Plain ASCII", theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("Caf\xC3\xA9", theme::Font::SairaBold));

    blaze4k::GlQuadRenderer quads; // uninitialized: draws are no-ops
    {
        CerrCapture capture;
        renderer.draw(quads, "HELLO", 10.0f, 10.0f, footer);
        renderer.draw(quads, "AGAIN", 10.0f, 40.0f, theme::text::kArtist, TextAlign::Right);
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "\n") == 1);
        TEST_CHECK(count_of(log, "SairaBold") == 1);
    }
    {
        CerrCapture capture;
        renderer.draw(quads, "HELLO", 10.0f, 10.0f, theme::text::kSongTitle);
        renderer.draw(quads, "HELLO", 10.0f, 10.0f, theme::text::kDiffMeter, TextAlign::Centre);
        TEST_CHECK(capture.text().empty());
    }
    renderer.shutdown();
    renderer.shutdown(); // safe to repeat
    TEST_CHECK(!renderer.font_available(theme::Font::SairaExtraBold));
    TEST_CHECK(!renderer.symbol_font_available());

    // A root without the symbol font (#124): load() still succeeds, logs exactly
    // one line, and symbols fall back to the placeholder box.
    const fs::path no_symbols = kTempRoot / "no-symbols";
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        const fs::path rel = theme::kFontFiles[i];
        fs::create_directories((no_symbols / rel).parent_path());
        fs::copy_file(kSourceDir / rel, no_symbols / rel, fs::copy_options::overwrite_existing);
    }
    {
        CerrCapture capture;
        TEST_CHECK(renderer.load(no_symbols));
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "\n") == 1);
        TEST_CHECK(count_of(log, "Symbol font unavailable") == 1);
        TEST_CHECK(count_of(log, "NotoSansSymbols-Subset.ttf") == 1);
    }
    TEST_CHECK(!renderer.symbol_font_available());
    TEST_CHECK(renderer.font_available(theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("\xE2\x98\xBA", theme::Font::SairaBold));
    {
        CerrCapture capture; // swallow the one headless line
        renderer.set_window_size(1280, 720);
    }
    const theme::TextStyle& artist = theme::text::kArtist;
    TEST_CHECK(approx(renderer.measure("\xE2\x98\xBA", artist),
                      0.6f * artist.size_px * renderer.scale() +
                          artist.tracking_px * renderer.scale()));
    renderer.shutdown();
    fs::remove_all(kTempRoot);
    std::cout << "  - corrupt/missing fonts log once and fall back to the bitmap font ok.\n";
}

// --- 3. Atlas bake --------------------------------------------------------------------------

void test_atlas_bake() {
    TEST_CHECK(blaze4k::baked_glyph_slot(U' ') == 0);
    TEST_CHECK(blaze4k::baked_glyph_slot(U'~') == 94);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x7F) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x9F) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0xA0) == 95);
    TEST_CHECK(blaze4k::baked_glyph_slot(0xFF) == 190);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x100) == 191);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x17F) == 318);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x180) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0xFFFD) == -1);
    // Symbol blocks (#124).
    static_assert(blaze4k::kLatinGlyphCount == 319);
    static_assert(blaze4k::kBakedGlyphCount == 975);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x218F) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x2190) == 319);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x21FF) == 430);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x2200) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x259F) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x25A0) == 431);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x25FF) == 526);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x2600) == 527);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x263A) == 585);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x26FF) == 782);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x2700) == 783);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x27BF) == 974);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x27C0) == -1);
    TEST_CHECK(blaze4k::baked_glyph_slot(0x1F600) == -1);

    const FontFace face = load_face(theme::Font::SairaExtraBold);
    TEST_CHECK(face.units_per_em() == 1000);
    const FontAtlas atlas = bake(face, 24.0f);
    TEST_CHECK(atlas.width > 0 && atlas.width <= 4096);
    TEST_CHECK(atlas.height > 0 && atlas.height <= 4096);
    TEST_CHECK(atlas.height % 4 == 0);
    TEST_CHECK(atlas.oversample == 2);
    TEST_CHECK(atlas.coverage.size() ==
               static_cast<std::size_t>(atlas.width) * static_cast<std::size_t>(atlas.height));
    std::size_t present = 0;
    for (std::size_t slot = 0; slot < blaze4k::kBakedGlyphCount; ++slot) {
        const blaze4k::AtlasGlyph& glyph = atlas.glyphs[slot];
        if (!glyph.present) {
            continue;
        }
        if (slot < blaze4k::kLatinGlyphCount) {
            ++present;
        }
        TEST_CHECK(glyph.uv.u0 >= 0.0f && glyph.uv.u1 <= 1.0f && glyph.uv.u0 <= glyph.uv.u1);
        TEST_CHECK(glyph.uv.v0 >= 0.0f && glyph.uv.v1 <= 1.0f && glyph.uv.v0 <= glyph.uv.v1);
    }
    TEST_CHECK(present == blaze4k::kLatinGlyphCount); // Saira covers all 319 Latin slots
    // Saira also has a few symbol-range glyphs (arrows, lozenge), baked from Saira itself.
    TEST_CHECK(face.has(blaze4k::baked_glyph_slot(0x2190)));
    TEST_CHECK(atlas.glyphs[static_cast<std::size_t>(blaze4k::baked_glyph_slot(0x2190))].present);
    TEST_CHECK(!face.has(blaze4k::baked_glyph_slot(0x263A)));

    const auto& a = atlas.glyphs[static_cast<std::size_t>(blaze4k::baked_glyph_slot(U'A'))];
    const auto& e_acute = atlas.glyphs[static_cast<std::size_t>(blaze4k::baked_glyph_slot(0xE9))];
    const auto& space = atlas.glyphs[0];
    TEST_CHECK(a.present && a.x1 > a.x0 && a.y1 > a.y0);
    TEST_CHECK(e_acute.present && e_acute.x1 > e_acute.x0 && e_acute.y1 > e_acute.y0);
    TEST_CHECK(space.present && space.x1 == space.x0 && space.y1 == space.y0);
    TEST_CHECK(face.advance_units(0) > 0);

    // Placeholder: positive area, ink on its edges, hollow middle.
    const blaze4k::AtlasGlyph& box = atlas.placeholder;
    TEST_CHECK(box.present && box.x1 > box.x0 && box.y1 > box.y0);
    TEST_CHECK(box.y1 == 0.0f); // sits on the baseline
    const int bx0 = static_cast<int>(std::lround(box.uv.u0 * static_cast<float>(atlas.width)));
    const int by0 = static_cast<int>(std::lround(box.uv.v0 * static_cast<float>(atlas.height)));
    const int bx1 = static_cast<int>(std::lround(box.uv.u1 * static_cast<float>(atlas.width)));
    const int by1 = static_cast<int>(std::lround(box.uv.v1 * static_cast<float>(atlas.height)));
    const auto texel = [&](int x, int y) {
        return atlas.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas.width) +
                              static_cast<std::size_t>(x)];
    };
    TEST_CHECK(bx1 - bx0 == static_cast<int>(box.x1 - box.x0));
    for (int x = bx0; x < bx1; ++x) {
        TEST_CHECK(texel(x, by0) == 255 && texel(x, by1 - 1) == 255);
    }
    for (int y = by0; y < by1; ++y) {
        TEST_CHECK(texel(bx0, y) == 255 && texel(bx1 - 1, y) == 255);
    }
    TEST_CHECK(texel((bx0 + bx1) / 2, (by0 + by1) / 2) == 0);

    const std::vector<std::uint8_t> rgba = blaze4k::coverage_to_white_rgba(
        std::vector<std::uint8_t>{0, 128, 255});
    TEST_CHECK((rgba == std::vector<std::uint8_t>{255, 255, 255, 0, 255, 255, 255, 128, 255, 255,
                                                  255, 255}));
    TEST_CHECK(blaze4k::coverage_to_white_rgba({}).empty());

    // Audiowide lacks exactly one Latin Extended-A code point.
    const FontFace audiowide = load_face(theme::Font::Audiowide);
    const FontAtlas audiowide_atlas = bake(audiowide, 24.0f);
    std::size_t absent = 0;
    for (std::size_t slot = 191; slot < blaze4k::kLatinGlyphCount; ++slot) {
        if (!audiowide_atlas.glyphs[slot].present) {
            ++absent;
            TEST_CHECK(!audiowide.has(static_cast<int>(slot)));
        }
    }
    TEST_CHECK(absent == 1);
    for (std::size_t slot = 0; slot < 191; ++slot) {
        TEST_CHECK(audiowide_atlas.glyphs[slot].present);
    }

    std::string error;
    TEST_CHECK(!FontAtlas::bake(face, 0.0f, 4096, &error).has_value());
    TEST_CHECK(!error.empty());
    TEST_CHECK(!FontAtlas::bake(face, std::nanf(""), 4096, nullptr).has_value());
    TEST_CHECK(!FontAtlas::bake(face, 5000.0f, 4096, nullptr).has_value());
    TEST_CHECK(!FontAtlas::bake(face, INFINITY, 4096, nullptr).has_value());
    // A tiny max_dim is clamped to 256 and still fits a small size.
    TEST_CHECK(FontAtlas::bake(face, 12.0f, 1, nullptr).has_value());
    // Headless upload fails cleanly and keeps the coverage.
    FontAtlas headless = bake(face, 20.0f);
    TEST_CHECK(!headless.upload());
    TEST_CHECK(!headless.coverage.empty());
    std::cout << "  - CPU atlas bake (ranges, placeholder, white RGBA, absent glyphs) ok.\n";
}

// --- 4. Measure -----------------------------------------------------------------------------

void test_measure() {
    const FontFace face = load_face(theme::Font::SairaBold);
    constexpr float kP = 24.0f;
    const auto m = [&](std::string_view text, float tracking = 0.0f) {
        return blaze4k::measure_text(face, text, kP, tracking);
    };
    TEST_CHECK(m("") == 0.0f);
    TEST_CHECK(m("A") > 0.0f);
    TEST_CHECK(m("AV") < m("A") + m("V")); // kerning
    TEST_CHECK(approx(m("AB"), m("A") + m("B") +
                                   static_cast<float>(face.kern_units(
                                       blaze4k::baked_glyph_slot(U'A'),
                                       blaze4k::baked_glyph_slot(U'B'))) *
                                       face.em_scale(kP)));
    // Tracking: after every visible glyph (the last included); zero-width adds none.
    const std::string tracked = "Hi \xC3\xA9" "e\xCC\x81!"; // H i space é e+U+0301 ! = 6 visible
    TEST_CHECK(approx(m(tracked, 3.0f) - m(tracked), 6.0f * 3.0f));
    TEST_CHECK(approx(m("e\xCC\x81"), m("e")));
    TEST_CHECK(approx(m("e\xE2\x80\x8D\xEF\xB8\x8F"), m("e"))); // ZWJ + VS16
    // Placeholder: 0.6em, for unknown code points and malformed bytes (U+FFFD).
    TEST_CHECK(approx(m("\xE4\xB8\xAD"), 0.6f * kP));
    TEST_CHECK(approx(m("\xFF"), 0.6f * kP));
    TEST_CHECK(approx(m("\x01"), 0.6f * kP)); // C0 control
    TEST_CHECK(approx(m("\xC2\x85"), 0.6f * kP)); // C1 control
    // Folds.
    TEST_CHECK(approx(m("\xE2\x80\x99"), m("'")));
    TEST_CHECK(approx(m("\xE2\x80\x93"), m("-")));
    TEST_CHECK(approx(m("\t"), m(" ")));
    TEST_CHECK(approx(m("\xEF\xBC\xA1"), m("A"))); // fullwidth A
    // Degenerate sizes.
    TEST_CHECK(blaze4k::measure_text(face, "A", 0.0f, 0.0f) == 0.0f);
    TEST_CHECK(blaze4k::measure_text(face, "A", std::nanf(""), 0.0f) == 0.0f);
    TEST_CHECK(approx(blaze4k::measure_text(face, "A", kP, std::nanf("")), m("A")));
    // em scale: STBTT_POINT_SIZE semantics (font-size is the em size).
    TEST_CHECK(approx(face.em_scale(44.0f), 0.044f, 1e-6f));

    // TextRenderer::measure scales linearly with s.
    TextRenderer renderer;
    TEST_CHECK(renderer.load(kSourceDir));
    {
        CerrCapture capture; // swallow the one headless line
        renderer.set_window_size(1280, 720);
    }
    const float at_720 = renderer.measure("Caf\xC3\xA9 V/A", theme::text::kBpm);
    renderer.set_window_size(2560, 1440);
    TEST_CHECK(renderer.scale() == 2.0f);
    const float at_1440 = renderer.measure("Caf\xC3\xA9 V/A", theme::text::kBpm);
    TEST_CHECK(std::fabs(at_1440 - 2.0f * at_720) <= 1e-3f * at_1440);
    TEST_CHECK(approx(renderer.ascent(theme::text::kSongTitle),
                      static_cast<float>(load_face(theme::Font::SairaExtraBold).ascent()) * 0.044f *
                          2.0f));
    TEST_CHECK(renderer.line_height(theme::text::kSongTitle) >
               renderer.ascent(theme::text::kSongTitle));
    std::cout << "  - measure (kerning, tracking, zero-width, placeholder, folds, scale) ok.\n";
}

// --- 5. Layout ------------------------------------------------------------------------------

void test_layout() {
    const FontFace face = load_face(theme::Font::SairaExtraBold);
    constexpr float kP = 24.0f;
    const FontAtlas atlas = bake(face, kP);
    const float k_em = face.em_scale(kP);

    // One quad per non-space visible glyph.
    TextLayout layout = plain_layout(kP);
    TEST_CHECK(collect(face, atlas, "AB C\xCC\x81 \xE4\xB8\xAD", 0.0f, 0.0f, layout).size() == 4);
    TEST_CHECK(collect(face, atlas, "", 0.0f, 0.0f, layout).empty());
    TEST_CHECK(collect(face, atlas, "   ", 0.0f, 0.0f, layout).empty());
    TEST_CHECK(collect(face, atlas, "A", std::nanf(""), 0.0f, layout).empty());
    TEST_CHECK(collect(face, atlas, "A", 0.0f, INFINITY, layout).empty());

    // Alignment: start pens differ by exactly w/2 and w.
    const std::string text = "WAVE 123";
    layout.tracking = 2.0f;
    const float w = blaze4k::measure_text(face, text, kP, 2.0f);
    layout.align = TextAlign::Left;
    const auto left = collect(face, atlas, text, 100.0f, 0.0f, layout);
    layout.align = TextAlign::Centre;
    const auto centre = collect(face, atlas, text, 100.0f, 0.0f, layout);
    layout.align = TextAlign::Right;
    const auto right = collect(face, atlas, text, 100.0f, 0.0f, layout);
    TEST_CHECK(left.size() == 7 && centre.size() == 7 && right.size() == 7);
    TEST_CHECK(approx(left[0].corners[0].x - centre[0].corners[0].x, w * 0.5f));
    TEST_CHECK(approx(left[0].corners[0].x - right[0].corners[0].x, w));
    // The first glyph starts at the pen; the last glyph ends one advance + tracking before w.
    const auto& first = atlas.glyphs[static_cast<std::size_t>(blaze4k::baked_glyph_slot(U'W'))];
    TEST_CHECK(approx(left[0].corners[0].x, 100.0f + first.x0));
    const int three = blaze4k::baked_glyph_slot(U'3');
    const float last_pen = 100.0f + w - 2.0f - static_cast<float>(face.advance_units(three)) * k_em;
    TEST_CHECK(approx(left.back().corners[0].x,
                      last_pen + atlas.glyphs[static_cast<std::size_t>(three)].x0, 1e-2f));
    layout.tracking = 0.0f;
    layout.align = TextAlign::Left;

    // Vertical anchor: y is the line-box top; 'H' sits on y + ascent.
    const auto h = collect(face, atlas, "H", 10.0f, 50.0f, layout);
    TEST_CHECK(h.size() == 1);
    const float baseline = 50.0f + static_cast<float>(face.ascent()) * k_em;
    TEST_CHECK(std::fabs(h[0].corners[2].y - baseline) <= 1.0f);
    TEST_CHECK(std::fabs((h[0].corners[2].y - h[0].corners[0].y) -
                         static_cast<float>(face.cap_height_units()) * k_em) <= 1.5f);
    // Upright: vertical edges.
    for (const GlyphQuad& quad : collect(face, atlas, text, 0.0f, 0.0f, layout)) {
        TEST_CHECK(quad.corners[0].x == quad.corners[3].x);
        TEST_CHECK(quad.corners[1].x == quad.corners[2].x);
        TEST_CHECK(same_color(quad.color, layout.color));
    }

    // Italic shear (+ a group shear).
    const TextLayout italic =
        blaze4k::resolve_text_layout(theme::text::kComboNumber, 1.0f, TextAlign::Centre,
                                     theme::text::kComboGroupShear);
    TEST_CHECK(approx(italic.shear, theme::kItalicShear + theme::text::kComboGroupShear));
    TEST_CHECK(approx(italic.pixel_size, 34.0f));
    TEST_CHECK(approx(italic.shadow_offset, 3.0f));
    TEST_CHECK(italic.align == TextAlign::Centre);
    TEST_CHECK(approx(blaze4k::resolve_text_layout(theme::text::kSongTitle, 1.0f, TextAlign::Left,
                                                   0.0f)
                          .shear,
                      theme::kItalicShear));
    TEST_CHECK(blaze4k::resolve_text_layout(theme::text::kArtist, 1.0f, TextAlign::Left, 0.0f)
                   .shear == 0.0f);
    TextLayout sheared = plain_layout(kP);
    sheared.shear = theme::kItalicShear + theme::text::kComboGroupShear;
    const auto slanted = collect(face, atlas, "AHg", 0.0f, 0.0f, sheared);
    TEST_CHECK(slanted.size() == 3);
    for (const GlyphQuad& quad : slanted) {
        const float dy = quad.corners[3].y - quad.corners[0].y;
        TEST_CHECK(approx(quad.corners[0].x - quad.corners[3].x, sheared.shear * dy));
        TEST_CHECK(approx(quad.corners[1].x - quad.corners[2].x, sheared.shear * dy));
    }
    // A glyph's baseline point does not move: BL.x of 'H' (bottom on the baseline) unchanged.
    const auto upright_h = collect(face, atlas, "H", 0.0f, 0.0f, plain_layout(kP));
    const auto slanted_h = collect(face, atlas, "H", 0.0f, 0.0f, sheared);
    TEST_CHECK(std::fabs(upright_h[0].corners[3].x - slanted_h[0].corners[3].x) <=
               sheared.shear * 1.0f);

    // Hard3 shadow: kShadow copies first (alpha = colour alpha), offset (0, 3s).
    for (const float s : {1.0f, 2.0f}) {
        theme::TextStyle style = theme::text::kSongTitle;
        style.color = with_alpha(style.color, 0.5f);
        const TextLayout shadowed = blaze4k::resolve_text_layout(style, s, TextAlign::Left, 0.0f);
        TEST_CHECK(approx(shadowed.shadow_offset, 3.0f * s));
        const FontAtlas scaled = bake(face, shadowed.pixel_size);
        const auto quads = collect(face, scaled, "Ab", 0.0f, 0.0f, shadowed);
        TEST_CHECK(quads.size() == 4);
        const std::size_t half = quads.size() / 2;
        for (std::size_t i = 0; i < half; ++i) {
            const GlyphQuad& shadow = quads[i];
            const GlyphQuad& main = quads[i + half];
            TEST_CHECK(same_color(shadow.color, with_alpha(theme::color::kShadow, 0.5f)));
            TEST_CHECK(same_color(main.color, style.color));
            for (std::size_t c = 0; c < 4; ++c) {
                TEST_CHECK(approx(shadow.corners[c].x - main.corners[c].x, 0.0f));
                TEST_CHECK(approx(shadow.corners[c].y - main.corners[c].y, 3.0f * s));
            }
        }
    }
    const TextLayout hard2 =
        blaze4k::resolve_text_layout(theme::text::kBarSongTitle, 2.0f, TextAlign::Left, 0.0f);
    TEST_CHECK(approx(hard2.shadow_offset, 4.0f));
    TEST_CHECK(approx(hard2.tracking, 0.0f));
    TEST_CHECK(approx(blaze4k::resolve_text_layout(theme::text::kTierLabel, 2.0f,
                                                   TextAlign::Left, 0.0f)
                          .tracking,
                      12.0f));

    // Missing code points draw the in-atlas placeholder (same texture, one batch).
    const auto boxes = collect(face, atlas, "\xE4\xB8\xAD\xFF", 0.0f, 0.0f, layout);
    TEST_CHECK(boxes.size() == 2);
    for (const GlyphQuad& quad : boxes) {
        TEST_CHECK(approx(quad.uv.u0, atlas.placeholder.uv.u0));
        TEST_CHECK(approx(quad.uv.v1, atlas.placeholder.uv.v1));
    }
    TEST_CHECK(approx(boxes[1].corners[0].x - boxes[0].corners[0].x, 0.6f * kP));

    // An atlas baked at another size is scaled to the layout size.
    const FontAtlas big = bake(face, 48.0f);
    const auto from_big = collect(face, big, "H", 0.0f, 0.0f, plain_layout(kP));
    TEST_CHECK(std::fabs((from_big[0].corners[2].y - from_big[0].corners[0].y) -
                         (upright_h[0].corners[2].y - upright_h[0].corners[0].y)) <= 1.0f);
    std::cout << "  - layout (alignment, anchor, shear, shadow, placeholder) ok.\n";
}

// --- 6. Truncation with the real font ------------------------------------------------------

void test_truncation_real_font() {
    TextRenderer renderer;
    TEST_CHECK(renderer.load(kSourceDir));
    const theme::TextStyle& style = theme::text::kSongTitle;
    const std::string text =
        "Caf\xC3\xA9 A\xCC\x8Angstr\xC3\xB6m \xE2\x80\x93 \xCE\xA9mega VAWAVA e\xCC\x81\xCC\x81!";
    const float full = renderer.measure(text, style);
    TEST_CHECK(full > 0.0f);
    TEST_CHECK(renderer.truncate(text, style, full) == text);
    TEST_CHECK(renderer.truncate(text, style, full + 50.0f) == text);
    const float ellipsis = renderer.measure("...", style);
    std::size_t cut_count = 0;
    for (float budget = 0.0f; budget <= full; budget += 0.75f) {
        const std::string out = renderer.truncate(text, style, budget);
        TEST_CHECK(renderer.measure(out, style) <= budget + 1e-3f);
        if (out == text) {
            continue;
        }
        ++cut_count;
        const bool has_ellipsis = out.size() >= 3 && out.compare(out.size() - 3, 3, "...") == 0;
        if (ellipsis <= budget) {
            TEST_CHECK(has_ellipsis);
        }
        const std::size_t prefix = has_ellipsis && ellipsis <= budget ? out.size() - 3 : out.size();
        TEST_CHECK(text.compare(0, prefix, out, 0, prefix) == 0);
        if (prefix < text.size()) {
            std::size_t pos = prefix;
            TEST_CHECK(!blaze4k::is_zero_width(blaze4k::next_code_point(text, pos)));
        }
    }
    TEST_CHECK(cut_count > 10);
    // Longest prefix: one more visible glyph would not fit.
    const std::string at_200 = renderer.truncate(text, style, 200.0f);
    TEST_CHECK(at_200.size() > 3);
    TEST_CHECK(renderer.truncate("", style, 0.0f).empty());
    TEST_CHECK(renderer.truncate(text, style, std::nanf("")).empty());
    TEST_CHECK(renderer.truncate(text, style, -1.0f).empty());
    std::cout << "  - measure-based truncation with Saira ExtraBold ok.\n";
}

// --- 7. Fuzz --------------------------------------------------------------------------------

void test_fuzz() {
    TextRenderer renderer;
    TEST_CHECK(renderer.load(kSourceDir));
    const FontFace face = load_face(theme::Font::SairaBold);
    const FontAtlas atlas = bake(face, 20.0f);
    const FontFace symbol = load_symbol_face();
    const std::vector<int> symbol_slots{blaze4k::baked_glyph_slot(0x263A),
                                        blaze4k::baked_glyph_slot(0x2605)};
    std::optional<FontAtlas> symbol_atlas = FontAtlas::bake(symbol, 20.0f, 4096, nullptr,
                                                            symbol_slots);
    TEST_CHECK(symbol_atlas.has_value());
    blaze4k::GlQuadRenderer quads;

    std::mt19937 rng{90};
    std::uniform_int_distribution<int> length_dist(0, 64);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_real_distribution<float> budget_dist(-10.0f, 400.0f);
    std::uniform_int_distribution<int> align_dist(0, 2);
    std::uniform_real_distribution<float> shear_dist(-0.5f, 0.5f);
    std::uniform_int_distribution<int> shadow_dist(0, 2);
    std::uniform_int_distribution<int> style_dist(0, static_cast<int>(theme::text::kAllStyles.size()) - 1);
    for (int i = 0; i < 2000; ++i) {
        std::string in(static_cast<std::size_t>(length_dist(rng)), '\0');
        for (char& c : in) {
            c = static_cast<char>(byte_dist(rng));
        }
        // Every other string is biased toward E2 98 xx / E2 9C xx so symbol code
        // points (and truncated/malformed symbol sequences) actually occur.
        if (i % 2 == 1) {
            for (std::size_t j = 0; j + 2 < in.size(); j += 3) {
                if (byte_dist(rng) < 128) {
                    in[j] = static_cast<char>(0xE2);
                    in[j + 1] = static_cast<char>(byte_dist(rng) < 128 ? 0x98 : 0x9C);
                }
            }
        }
        const theme::TextStyle& style =
            theme::text::kAllStyles[static_cast<std::size_t>(style_dist(rng))];
        const float width = renderer.measure(in, style);
        TEST_CHECK(std::isfinite(width) && width >= 0.0f);
        const float budget = budget_dist(rng);
        const std::string out = renderer.truncate(in, style, budget);
        if (out != in) {
            TEST_CHECK(renderer.measure(out, style) <= std::max(budget, 0.0f) + 1e-3f);
        }
        (void)renderer.covers_text(in, style.font);
        renderer.draw(quads, in, 5.0f, 5.0f, style, static_cast<TextAlign>(align_dist(rng)));

        TextLayout layout = plain_layout(20.0f);
        layout.align = static_cast<TextAlign>(align_dist(rng));
        layout.shear = shear_dist(rng);
        layout.shadow_offset = static_cast<float>(shadow_dist(rng));
        layout.tracking = static_cast<float>(shadow_dist(rng));
        std::size_t count = 0;
        blaze4k::for_each_text_quad(face, atlas, in, 640.0f, 360.0f, layout,
                                    [&](const GlyphQuad& quad) {
                                        ++count;
                                        for (const blaze4k::Vec2& corner : quad.corners) {
                                            TEST_CHECK(std::isfinite(corner.x) &&
                                                       std::isfinite(corner.y));
                                        }
                                    });
        TEST_CHECK(count <= 2 * in.size());

        std::size_t fallback_count = 0;
        blaze4k::for_each_text_quad(face, atlas, &symbol, &*symbol_atlas, in, 640.0f, 360.0f,
                                    layout, [&](const GlyphQuad& quad) {
                                        ++fallback_count;
                                        for (const blaze4k::Vec2& corner : quad.corners) {
                                            TEST_CHECK(std::isfinite(corner.x) &&
                                                       std::isfinite(corner.y));
                                        }
                                        if (quad.fallback) {
                                            TEST_CHECK(quad.uv.u0 >= 0.0f && quad.uv.u1 <= 1.0f &&
                                                       quad.uv.v0 >= 0.0f && quad.uv.v1 <= 1.0f);
                                        }
                                    });
        TEST_CHECK(fallback_count <= 2 * in.size());
        const float with_symbols = blaze4k::measure_text(face, &symbol, in, 20.0f, 0.0f);
        TEST_CHECK(std::isfinite(with_symbols) && with_symbols >= 0.0f);
    }
    std::cout << "  - fuzz (2000 random byte strings) ok.\n";
}

// --- 8. Headless TextRenderer ---------------------------------------------------------------

void test_headless_renderer() {
    TEST_CHECK(blaze4k::text_layout_scale(0, 0) == 1.0f);
    TEST_CHECK(blaze4k::text_layout_scale(1280, -5) == 1.0f);
    TEST_CHECK(blaze4k::text_layout_scale(-5, 720) == 1.0f);
    TEST_CHECK(blaze4k::text_layout_scale(1280, 720) == 1.0f);
    TEST_CHECK(blaze4k::text_layout_scale(2560, 1440) == 2.0f);
    TEST_CHECK(approx(blaze4k::text_layout_scale(1920, 1080), 1.5f));
    // Fit (#91): 16:10 is width-limited, 21:9 height-limited.
    TEST_CHECK(blaze4k::text_layout_scale(1920, 1200) == 1.5f);
    TEST_CHECK(blaze4k::text_layout_scale(1280, 800) == 1.0f);
    TEST_CHECK(blaze4k::text_layout_scale(3440, 1440) == 2.0f);

    TextRenderer renderer;
    TEST_CHECK(renderer.baked_width() == -1);
    TEST_CHECK(renderer.baked_height() == -1);
    TEST_CHECK(renderer.scale() == 1.0f);
    {
        CerrCapture capture;
        TEST_CHECK(renderer.load(kSourceDir));
        TEST_CHECK(capture.text().empty());
    }
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        TEST_CHECK(renderer.font_available(static_cast<theme::Font>(i)));
    }
    {
        CerrCapture capture;
        renderer.set_window_size(2560, 1440);
        const std::string log = capture.text();
        TEST_CHECK(count_of(log, "\n") == 1);
        TEST_CHECK(count_of(log, "No GL") == 1);
    }
    TEST_CHECK(renderer.atlas_count() == 0);
    TEST_CHECK(renderer.baked_width() == 2560);
    TEST_CHECK(renderer.baked_height() == 1440);
    TEST_CHECK(renderer.scale() == 2.0f);
    {
        CerrCapture capture;
        renderer.set_window_size(2560, 1440);
        renderer.set_window_size(3440, 1440); // 21:9: same scale, size recorded
        TEST_CHECK(renderer.baked_width() == 3440);
        TEST_CHECK(renderer.scale() == 2.0f);
        renderer.set_window_size(1920, 1200); // 16:10: width-limited fit
        TEST_CHECK(renderer.scale() == 1.5f);
        renderer.set_window_size(1280, 720); // headless line is logged once only
        TEST_CHECK(capture.text().empty());
    }
    TEST_CHECK(renderer.scale() == 1.0f);
    TEST_CHECK(renderer.baked_width() == 1280);
    TEST_CHECK(renderer.baked_height() == 720);

    blaze4k::GlQuadRenderer quads; // uninitialized
    {
        CerrCapture capture;
        renderer.draw(quads, "HELLO \xE4\xB8\xAD", 10.0f, 10.0f, theme::text::kSongTitle,
                      TextAlign::Centre, theme::text::kComboGroupShear);
        renderer.draw(quads, "", 0.0f, 0.0f, theme::text::kFooter);
        TEST_CHECK(capture.text().empty());
    }
    TEST_CHECK(renderer.atlas_count() == 0);
    TEST_CHECK(renderer.measure("HELLO", theme::text::kFooter) > 0.0f);

    // Coverage: native glyphs only.
    TEST_CHECK(renderer.covers_text("", theme::Font::SairaBold));
    TEST_CHECK(renderer.covers_text("Caf\xC3\xA9 \xC5\x81\xC3\xB3" "d\xC5\xBA", theme::Font::SairaBold));
    TEST_CHECK(renderer.covers_text("e\xCC\x81", theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("\xE4\xB8\xAD", theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("\xE2\x80\x99", theme::Font::SairaBold)); // fold only
    TEST_CHECK(!renderer.covers_text("\xFF", theme::Font::SairaBold));

    // Reload is idempotent.
    TEST_CHECK(renderer.load(kSourceDir));
    TEST_CHECK(renderer.baked_height() == -1);
    renderer.shutdown();
    TEST_CHECK(!renderer.font_available(theme::Font::SairaBold));
    // A renderer with no fonts still measures (bitmap model) and never crashes.
    TEST_CHECK(renderer.measure("AB", theme::text::kArtist) > 0.0f);
    std::cout << "  - headless TextRenderer (one no-GL line, no atlases, no-op draws) ok.\n";
}

// --- 9. Symbol fallback (#124) ---------------------------------------------------------------

void test_symbol_fallback() {
    using blaze4k::baked_glyph_slot;
    using blaze4k::measure_text;
    const int smiley = baked_glyph_slot(0x263A);
    const int star = baked_glyph_slot(0x2605);

    // The symbol subset has no Latin: the default 'A' probe rejects it.
    {
        std::string error;
        TEST_CHECK(!FontFace::from_file(kSourceDir / theme::kSymbolFontFile, &error).has_value());
        TEST_CHECK(error.find("'A'") != std::string::npos);
        // A theme font lacking the symbol probe reports it as U+XXXX.
        error.clear();
        TEST_CHECK(!FontFace::from_file(font_path(theme::Font::SairaBold), &error,
                                        blaze4k::kSymbolProbeCodePoint)
                        .has_value());
        TEST_CHECK(error.find("U+263A") != std::string::npos);
    }
    const FontFace symbol = load_symbol_face();
    const FontFace saira = load_face(theme::Font::SairaBold);
    TEST_CHECK(symbol.has(smiley));
    TEST_CHECK(!saira.has(smiley));
    // The merge of Symbols 1 + 2 covers all of U+2600-26FF.
    for (char32_t cp = 0x2600; cp <= 0x26FF; ++cp) {
        TEST_CHECK(symbol.has(baked_glyph_slot(cp)));
    }
    TEST_CHECK(!symbol.has(baked_glyph_slot(U'A')));

    // Measure: the symbol face's advance, not the 0.6em placeholder.
    constexpr float kP = 24.0f;
    const auto m = [&](std::string_view text) {
        return measure_text(saira, &symbol, text, kP, 0.0f);
    };
    const float smiley_advance =
        static_cast<float>(symbol.advance_units(smiley)) * symbol.em_scale(kP);
    TEST_CHECK(smiley_advance > 0.0f);
    TEST_CHECK(approx(m("\xE2\x98\xBA"), smiley_advance));
    TEST_CHECK(!approx(m("\xE2\x98\xBA"), 0.6f * kP));
    TEST_CHECK(approx(measure_text(saira, nullptr, "\xE2\x98\xBA", kP, 0.0f), 0.6f * kP));
    TEST_CHECK(approx(measure_text(saira, "\xE2\x98\xBA", kP, 0.0f), 0.6f * kP));
    // No kerning across faces: A and V do not kern through the symbol.
    TEST_CHECK(approx(m("A\xE2\x98\xBAV"), m("A") + m("\xE2\x98\xBA") + m("V")));
    TEST_CHECK(m("AV") < m("A") + m("V")); // primary kerning still applies
    // Tracking after the symbol glyph too.
    TEST_CHECK(approx(measure_text(saira, &symbol, "A\xE2\x98\xBA", kP, 3.0f),
                      m("A\xE2\x98\xBA") + 6.0f));
    // The symbol face wins over the ASCII fold: ★ is the real star, not '*'.
    TEST_CHECK(approx(m("\xE2\x98\x85"),
                      static_cast<float>(symbol.advance_units(star)) * symbol.em_scale(kP)));
    TEST_CHECK(!approx(m("\xE2\x98\x85"), m("*")));
    TEST_CHECK(approx(measure_text(saira, nullptr, "\xE2\x98\x85", kP, 0.0f), m("*")));
    // A primary glyph in a symbol block wins over the symbol face (Saira's arrow).
    TEST_CHECK(approx(m("\xE2\x86\x90"),
                      static_cast<float>(saira.advance_units(baked_glyph_slot(0x2190))) *
                          saira.em_scale(kP)));
    // Emoji, CJK and malformed UTF-8 (U+FFFD) still draw the placeholder.
    TEST_CHECK(approx(m("\xF0\x9F\x98\x80"), 0.6f * kP));
    TEST_CHECK(approx(m("\xE4\xB8\xAD"), 0.6f * kP));
    TEST_CHECK(approx(m("\xE2\x98"), 0.6f * kP));

    // Bake only the selected slots: just ☺ and the placeholder.
    std::string error;
    const std::vector<int> only{smiley};
    std::optional<FontAtlas> symbol_atlas = FontAtlas::bake(symbol, kP, 4096, &error, only);
    TEST_CHECK(symbol_atlas.has_value());
    std::size_t present = 0;
    for (const blaze4k::AtlasGlyph& glyph : symbol_atlas->glyphs) {
        present += glyph.present ? 1u : 0u;
    }
    TEST_CHECK(present == 1);
    const blaze4k::AtlasGlyph& smiley_glyph =
        symbol_atlas->glyphs[static_cast<std::size_t>(smiley)];
    TEST_CHECK(smiley_glyph.present && smiley_glyph.x1 > smiley_glyph.x0 &&
               smiley_glyph.y1 > smiley_glyph.y0);
    TEST_CHECK(symbol_atlas->placeholder.present);
    TEST_CHECK(symbol_atlas->width <= 256 && symbol_atlas->height <= 256);
    // Out-of-range, absent and duplicate selections are ignored.
    const std::vector<int> messy{-5, smiley, smiley, 99999, baked_glyph_slot(U'A')};
    std::optional<FontAtlas> messy_atlas = FontAtlas::bake(symbol, kP, 4096, nullptr, messy);
    TEST_CHECK(messy_atlas.has_value());
    present = 0;
    for (const blaze4k::AtlasGlyph& glyph : messy_atlas->glyphs) {
        present += glyph.present ? 1u : 0u;
    }
    TEST_CHECK(present == 1);
    // The per-size cap (64 glyphs) fits the 4096 cap even at the 4K song title size.
    std::vector<int> cap_slots;
    for (char32_t cp = 0x2600; cp < 0x2640; ++cp) {
        cap_slots.push_back(baked_glyph_slot(cp));
    }
    const float title_4k = theme::text::kSongTitle.size_px * 3.0f;
    std::optional<FontAtlas> big = FontAtlas::bake(symbol, title_4k, 4096, &error, cap_slots);
    TEST_CHECK(big.has_value());
    TEST_CHECK(big->width <= 4096 && big->height <= 4096);
    TEST_CHECK(big->oversample == 2);

    // Quads: the symbol glyph comes from the fallback atlas at the pen after 'A'.
    const FontAtlas saira_atlas = bake(saira, kP);
    const float pen_after_a = measure_text(saira, &symbol, "A", kP, 0.0f);
    std::vector<GlyphQuad> quads;
    const auto gather = [&](const FontAtlas* fallback_atlas, std::string_view text) {
        quads.clear();
        blaze4k::for_each_text_quad(saira, saira_atlas, &symbol, fallback_atlas, text, 0.0f, 0.0f,
                                    plain_layout(kP),
                                    [&](const GlyphQuad& quad) { quads.push_back(quad); });
    };
    gather(&*symbol_atlas, "A\xE2\x98\xBA");
    TEST_CHECK(quads.size() == 2);
    TEST_CHECK(!quads[0].fallback);
    TEST_CHECK(quads[1].fallback);
    TEST_CHECK(quads[1].uv.u0 >= 0.0f && quads[1].uv.u1 <= 1.0f && quads[1].uv.u0 < quads[1].uv.u1);
    TEST_CHECK(quads[1].uv.v0 >= 0.0f && quads[1].uv.v1 <= 1.0f && quads[1].uv.v0 < quads[1].uv.v1);
    TEST_CHECK(quads[1].corners[0].x >= pen_after_a);
    TEST_CHECK(approx(quads[1].corners[0].x, pen_after_a + smiley_glyph.x0));
    // Sits on the primary baseline.
    const float baseline = static_cast<float>(saira.ascent()) * saira.em_scale(kP);
    TEST_CHECK(approx(quads[1].corners[2].y, baseline + smiley_glyph.y1));
    // A fallback atlas baked at another size is scaled to the layout size.
    std::optional<FontAtlas> double_atlas = FontAtlas::bake(symbol, 2.0f * kP, 4096, nullptr, only);
    TEST_CHECK(double_atlas.has_value());
    gather(&*double_atlas, "A\xE2\x98\xBA");
    TEST_CHECK(quads.size() == 2 && quads[1].fallback);
    TEST_CHECK(std::fabs((quads[1].corners[2].y - quads[1].corners[0].y) -
                         (smiley_glyph.y1 - smiley_glyph.y0)) <= 1.0f);
    // No fallback atlas (or a glyph it lacks): the placeholder at the same pen.
    for (const FontAtlas* fallback_atlas : {static_cast<const FontAtlas*>(nullptr),
                                            static_cast<const FontAtlas*>(&*symbol_atlas)}) {
        const std::string_view text =
            fallback_atlas == nullptr ? "A\xE2\x98\xBA" : "A\xE2\x98\x85"; // ★ not baked
        gather(fallback_atlas, text);
        TEST_CHECK(quads.size() == 2);
        TEST_CHECK(!quads[1].fallback);
        TEST_CHECK(approx(quads[1].uv.u0, saira_atlas.placeholder.uv.u0));
        TEST_CHECK(approx(quads[1].corners[0].x, pen_after_a + saira_atlas.placeholder.x0));
    }
    // The old overload never uses a fallback.
    quads.clear();
    blaze4k::for_each_text_quad(saira, saira_atlas, "A\xE2\x98\xBA", 0.0f, 0.0f,
                                plain_layout(kP),
                                [&](const GlyphQuad& quad) { quads.push_back(quad); });
    TEST_CHECK(quads.size() == 2 && !quads[1].fallback);
    // Right alignment uses the fallback-aware width.
    TextLayout right = plain_layout(kP);
    right.align = TextAlign::Right;
    quads.clear();
    blaze4k::for_each_text_quad(saira, saira_atlas, &symbol, &*symbol_atlas, "\xE2\x98\xBA",
                                100.0f, 0.0f, right,
                                [&](const GlyphQuad& quad) { quads.push_back(quad); });
    TEST_CHECK(quads.size() == 1);
    TEST_CHECK(approx(quads[0].corners[0].x, 100.0f - smiley_advance + smiley_glyph.x0));

    // TextRenderer: the symbol face loads from the source dir and counts for coverage.
    TextRenderer renderer;
    {
        CerrCapture capture;
        TEST_CHECK(renderer.load(kSourceDir));
        TEST_CHECK(capture.text().empty());
    }
    TEST_CHECK(renderer.symbol_font_available());
    TEST_CHECK(renderer.covers_text("\xE2\x98\xBA", theme::Font::SairaBold));
    TEST_CHECK(renderer.covers_text("\xE2\x98\xBA", theme::Font::Audiowide));
    TEST_CHECK(renderer.covers_text("KaW feat. \xE2\x98\xBA \xE2\x9C\x93", theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("\xF0\x9F\x98\x80", theme::Font::SairaBold));
    TEST_CHECK(!renderer.covers_text("\xE2\x98\xBA\xE4\xB8\xAD", theme::Font::SairaBold));
    {
        CerrCapture capture; // swallow the one headless line
        renderer.set_window_size(1280, 720);
    }
    const theme::TextStyle& artist = theme::text::kArtist;
    const float measured = renderer.measure("\xE2\x98\xBA", artist);
    TEST_CHECK(!approx(measured, 0.6f * artist.size_px));
    TEST_CHECK(approx(measured, static_cast<float>(symbol.advance_units(smiley)) *
                                    symbol.em_scale(artist.size_px) +
                                    artist.tracking_px));
    // Headless draws stay no-ops (nothing bakes, nothing logs).
    blaze4k::GlQuadRenderer no_gl;
    {
        CerrCapture capture;
        renderer.draw(no_gl, "\xE2\x98\xBA", 10.0f, 10.0f, artist);
        TEST_CHECK(capture.text().empty());
    }
    TEST_CHECK(renderer.atlas_count() == 0);
    renderer.shutdown();
    TEST_CHECK(!renderer.symbol_font_available());
    std::cout << "  - symbol fallback (slots, probe, measure, bake, quads, coverage) ok.\n";
}

// The symbol-atlas slot merge: dedup, sorted order, cap, growth detection (#124).
void test_merge_symbol_slots() {
    using blaze4k::merge_symbol_slots;
    std::vector<int> slots;

    // Empty input: nothing changes.
    auto merged = merge_symbol_slots(slots, {}, 4);
    TEST_CHECK(!merged.grew && !merged.dropped && slots.empty());

    // New slots insert sorted; duplicates within `wanted` collapse.
    const std::vector<int> first{585, 527, 585, 783};
    merged = merge_symbol_slots(slots, first, 4);
    TEST_CHECK(merged.grew && !merged.dropped);
    TEST_CHECK((slots == std::vector<int>{527, 585, 783}));

    // Already-present slots are no growth (no rebake).
    const std::vector<int> known{783, 527};
    merged = merge_symbol_slots(slots, known, 4);
    TEST_CHECK(!merged.grew && !merged.dropped);
    TEST_CHECK(slots.size() == 3);

    // Filling to the cap grows; anything new past it is dropped, known ones are not.
    const std::vector<int> over{600, 431, 585};
    merged = merge_symbol_slots(slots, over, 4);
    TEST_CHECK(merged.grew && merged.dropped);
    TEST_CHECK((slots == std::vector<int>{527, 585, 600, 783}));

    // At the cap: a new slot is only dropped, a known one changes nothing.
    const std::vector<int> full_new{431};
    merged = merge_symbol_slots(slots, full_new, 4);
    TEST_CHECK(!merged.grew && merged.dropped);
    const std::vector<int> full_known{600};
    merged = merge_symbol_slots(slots, full_known, 4);
    TEST_CHECK(!merged.grew && !merged.dropped);
    TEST_CHECK(slots.size() == 4);

    // Cap 0 never grows.
    std::vector<int> none;
    merged = merge_symbol_slots(none, first, 0);
    TEST_CHECK(!merged.grew && merged.dropped && none.empty());
    std::cout << "  - symbol slot merge (dedup, order, cap, growth) ok.\n";
}

// --- 10. Style list --------------------------------------------------------------------------

void test_all_styles() {
    TEST_CHECK(theme::text::kAllStyles.size() == 30);
    std::set<std::pair<int, float>> pairs;
    for (const theme::TextStyle& style : theme::text::kAllStyles) {
        pairs.emplace(static_cast<int>(style.font), style.size_px);
    }
    TEST_CHECK(pairs.size() == 13);
    std::cout << "  - kAllStyles: 30 styles, 13 unique (font, size) pairs ok.\n";
}

} // namespace

int main() {
    std::cout << "[ttf_font_test] Running TrueType text tests...\n";
    test_validate_sfnt();
    test_corrupt_and_missing_files();
    test_atlas_bake();
    test_measure();
    test_layout();
    test_truncation_real_font();
    test_fuzz();
    test_headless_renderer();
    test_symbol_fallback();
    test_merge_symbol_slots();
    test_all_styles();
    std::cout << "[ttf_font_test] All tests passed.\n";
    return 0;
}
