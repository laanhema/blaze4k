#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "chart/song_metadata.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/unicode_text.hpp"
#include "screens/song_display_text.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::font_covers_text;
using blaze4k::glyph_rows;
using blaze4k::has_glyph;
using blaze4k::kReplacementChar;
using blaze4k::text_width;

bool rows_non_blank(const std::uint8_t* rows) {
    for (int row = 0; row < 7; ++row) {
        if (rows[row] != 0) {
            return true;
        }
    }
    return false;
}

void test_punctuation_glyphs() {
    for (char32_t cp = 0x20; cp <= 0x7E; ++cp) {
        TEST_CHECK(has_glyph(cp));
        TEST_CHECK(glyph_rows(cp) != nullptr);
    }
    const std::uint8_t* placeholder = glyph_rows(kReplacementChar);
    TEST_CHECK(placeholder != nullptr);
    for (char c : std::string("'!?&():/,\"#_=~^")) {
        const std::uint8_t* rows = glyph_rows(static_cast<unsigned char>(c));
        TEST_CHECK(rows != placeholder);
        TEST_CHECK(rows_non_blank(rows));
    }
    // Distinct printable characters never share a bitmap pointer.
    for (char32_t a = 0x20; a <= 0x7E; ++a) {
        for (char32_t b = a + 1; b <= 0x7E; ++b) {
            TEST_CHECK(glyph_rows(a) != glyph_rows(b));
        }
    }
    TEST_CHECK(!has_glyph(0x7F));
    TEST_CHECK(!has_glyph(0x1F));
    TEST_CHECK(!has_glyph(0xE9));
    TEST_CHECK(!has_glyph(kReplacementChar));
    std::cout << "  - printable ASCII / punctuation glyph lookup ok.\n";
}

void test_fold_and_placeholder() {
    TEST_CHECK(glyph_rows(0x00E9) == glyph_rows(U'e'));
    TEST_CHECK(glyph_rows(0x2019) == glyph_rows(U'\''));
    TEST_CHECK(glyph_rows(0x00B3) == glyph_rows(U'3'));
    const std::uint8_t* placeholder = glyph_rows(kReplacementChar);
    TEST_CHECK(placeholder != nullptr);
    TEST_CHECK(rows_non_blank(placeholder));
    TEST_CHECK(glyph_rows(0x263A) == placeholder);
    TEST_CHECK(glyph_rows(0x65E5) == placeholder);
    TEST_CHECK(glyph_rows(0x00C6) == placeholder); // Æ: no one-to-one fold
    TEST_CHECK(glyph_rows(0x7F) == placeholder);   // DEL
    TEST_CHECK(glyph_rows(0x01) == placeholder);   // C0 control
    TEST_CHECK(glyph_rows(0x09) == glyph_rows(U' ')); // TAB folds to space
    // The placeholder is distinct from the look-alike native glyphs.
    TEST_CHECK(placeholder != glyph_rows(U'O'));
    TEST_CHECK(placeholder != glyph_rows(U'0'));
    TEST_CHECK(glyph_rows(0x0301) == nullptr); // zero-width
    TEST_CHECK(glyph_rows(0xFEFF) == nullptr);
    std::cout << "  - fold and placeholder resolution ok.\n";
}

void test_text_width_multibyte() {
    TEST_CHECK(text_width("VerTex\xC2\xB3", 1.0f) == 7.0f * 6.0f);
    TEST_CHECK(text_width("\xE2\x98\xBA", 2.0f) == 12.0f); // one cell, not three
    TEST_CHECK(text_width("e\xCC\x81", 1.0f) == 6.0f);     // combining mark: no cell
    TEST_CHECK(text_width("\xFF\xFE", 1.0f) == 12.0f);     // two placeholders
    TEST_CHECK(text_width("\xF0\x9F\x8E\xB5", 1.0f) == 6.0f);
    TEST_CHECK(text_width("Don't Promise Me", 1.0f) == 16.0f * 6.0f);
    for (const std::string s : {"PRESS START", "[UP/DOWN] DIFFICULTY", "DP 120/340",
                                "SAMPLES 3 / 8", "> EXPERT 11", "<PRESS>", "IN USE:"}) {
        TEST_CHECK(text_width(s, 2.5f) == static_cast<float>(s.size()) * 6.0f * 2.5f);
    }
    std::cout << "  - text_width per code point ok.\n";
}

void test_coverage() {
    TEST_CHECK(font_covers_text("Don't Promise Me"));
    TEST_CHECK(font_covers_text("Sly/Fly/Badman"));
    TEST_CHECK(font_covers_text("Summer ~Speedy Mix~"));
    TEST_CHECK(font_covers_text("Glacier:Groove;Part 1"));
    TEST_CHECK(font_covers_text(""));
    TEST_CHECK(font_covers_text("e\xCC\x81")); // zero-width marks do not break coverage
    TEST_CHECK(!font_covers_text("VerTex\xC2\xB3"));
    TEST_CHECK(!font_covers_text("\xE2\x98\xBA"));
    TEST_CHECK(!font_covers_text("Caf\xC3\xA9")); // fold is not coverage
    TEST_CHECK(!font_covers_text("a\xFF"));
    TEST_CHECK(!font_covers_text("tab\there"));
    std::cout << "  - native coverage ok.\n";
}

blaze4k::SongMetadata make_metadata(const std::string& title, const std::string& title_translit,
                                    const std::string& artist,
                                    const std::string& artist_translit) {
    blaze4k::SongMetadata metadata;
    metadata.title = title;
    metadata.title_translit = title_translit;
    metadata.artist = artist;
    metadata.artist_translit = artist_translit;
    return metadata;
}

void test_translit_fallback() {
    using blaze4k::select_display_text;
    using blaze4k::song_display_artist;
    using blaze4k::song_display_title;

    const auto vertex3 = make_metadata("VerTex\xC2\xB3", "VerTex^3", "\xE2\x98\xBA", "Smiley");
    TEST_CHECK(song_display_title(vertex3) == "VerTex^3");
    TEST_CHECK(song_display_artist(vertex3) == "Smiley");
    // The identity fields stay raw.
    TEST_CHECK(vertex3.title == "VerTex\xC2\xB3");
    TEST_CHECK(vertex3.artist == "\xE2\x98\xBA");

    const auto vertex2 = make_metadata("VerTex\xC2\xB2", "", "KaW feat. \xE2\x98\xBA", "");
    TEST_CHECK(song_display_title(vertex2) == "VerTex\xC2\xB2");
    TEST_CHECK(song_display_artist(vertex2) == "KaW feat. \xE2\x98\xBA");
    TEST_CHECK(text_width(song_display_title(vertex2), 1.0f) == 7.0f * 6.0f);

    const auto dont = make_metadata("Don't Promise Me", "X", "Artist", "Y");
    TEST_CHECK(song_display_title(dont) == "Don't Promise Me"); // covered native wins
    TEST_CHECK(song_display_artist(dont) == "Artist");

    // The result aliases the metadata (no copy).
    TEST_CHECK(&song_display_title(vertex3) == &vertex3.title_translit);
    TEST_CHECK(&song_display_title(dont) == &dont.title);

    const std::string a = "a";
    const std::string b = "b";
    const std::string empty;
    TEST_CHECK(select_display_text(a, b, true) == "a");
    TEST_CHECK(select_display_text(a, b, false) == "b");
    TEST_CHECK(select_display_text(empty, b, false) == "b");
    TEST_CHECK(select_display_text(a, empty, false) == "a");
    std::cout << "  - translit display fallback ok.\n";
}

void test_malformed_draw_smoke() {
    blaze4k::GlQuadRenderer renderer; // uninitialized: safe no-op
    std::mt19937 rng{77};
    std::uniform_int_distribution<int> length_dist(0, 64);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    const float pixel = 2.5f;
    for (int i = 0; i < 10000; ++i) {
        std::string s(static_cast<std::size_t>(length_dist(rng)), '\0');
        for (char& c : s) {
            c = static_cast<char>(byte_dist(rng));
        }
        const float width = text_width(s, pixel);
        TEST_CHECK(std::isfinite(width));
        TEST_CHECK(width <= static_cast<float>(s.size()) * 6.0f * pixel);
        blaze4k::draw_text(renderer, s, 0.0f, 0.0f, pixel, blaze4k::Color{});
        blaze4k::draw_text_centered(renderer, s, 640.0f, 0.0f, pixel, blaze4k::Color{});
    }
    const std::string lead_bytes(4096, '\xE2');
    const float width = text_width(lead_bytes, pixel);
    TEST_CHECK(std::isfinite(width));
    TEST_CHECK(width == 4096.0f * 6.0f * pixel); // each truncated lead is one placeholder
    blaze4k::draw_text(renderer, lead_bytes, 0.0f, 0.0f, pixel, blaze4k::Color{});
    blaze4k::draw_text_centered(renderer, lead_bytes, 640.0f, 0.0f, pixel, blaze4k::Color{});
    std::cout << "  - malformed UTF-8 draw smoke ok.\n";
}

} // namespace

int main() {
    std::cout << "[bitmap_font_test] Running bitmap font tests...\n";
    test_punctuation_glyphs();
    test_fold_and_placeholder();
    test_text_width_multibyte();
    test_coverage();
    test_translit_fallback();
    test_malformed_draw_smoke();
    std::cout << "[bitmap_font_test] All tests passed.\n";
    return 0;
}
