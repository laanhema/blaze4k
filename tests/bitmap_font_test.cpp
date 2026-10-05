#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "chart/chart.hpp"
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

bool near(float a, float b) {
    return std::fabs(a - b) <= 1e-3f;
}

void test_subtitle_display_and_fit() {
    using blaze4k::fit_title_subtitle;
    using blaze4k::song_display_subtitle;
    using blaze4k::song_display_title;
    using blaze4k::TitleSubtitleFit;

    // Bitmap rule, applied to SUBTITLE / SUBTITLETRANSLIT on their own.
    blaze4k::SongMetadata star;
    star.title = "Summer";
    star.subtitle = "\xE2\x98\x86Mix";
    star.subtitle_translit = "Star Mix";
    TEST_CHECK(!font_covers_text(star.subtitle));
    TEST_CHECK(song_display_subtitle(star) == "Star Mix");
    TEST_CHECK(&song_display_subtitle(star) == &star.subtitle_translit);
    // Independent of the title: an ASCII title stays native next to a translit subtitle.
    TEST_CHECK(&song_display_title(star) == &star.title);
    TEST_CHECK(star.subtitle == "\xE2\x98\x86Mix"); // identity field stays raw

    blaze4k::SongMetadata hyper;
    hyper.title = "Disconnected";
    hyper.subtitle = "-Hyper-";
    hyper.subtitle_translit = "X";
    TEST_CHECK(&song_display_subtitle(hyper) == &hyper.subtitle); // covered native wins

    blaze4k::SongMetadata no_translit;
    no_translit.subtitle = "\xE2\x98\x86";
    TEST_CHECK(&song_display_subtitle(no_translit) == &no_translit.subtitle);

    const blaze4k::SongMetadata none;
    TEST_CHECK(song_display_subtitle(none).empty());
    TEST_CHECK(&song_display_subtitle(none) == &none.subtitle);
    // A null renderer falls back to the bitmap rule.
    TEST_CHECK(&song_display_subtitle(star, nullptr, blaze4k::theme::Font::SairaBold) ==
               &star.subtitle_translit);

    auto fits = [](TitleSubtitleFit f, float title, float sub) {
        return near(f.title_max_w, title) && near(f.subtitle_max_w, sub);
    };
    TEST_CHECK(fits(fit_title_subtitle(100, 0, 8, 400), 100, 0));   // no subtitle
    TEST_CHECK(fits(fit_title_subtitle(500, 0, 8, 400), 400, 0));   // no subtitle, long title
    TEST_CHECK(fits(fit_title_subtitle(100, 50, 8, 400), 100, 50)); // both fit
    TEST_CHECK(fits(fit_title_subtitle(100, 292, 8, 400), 100, 292)); // exactly fits
    TEST_CHECK(fits(fit_title_subtitle(100, 500, 8, 400), 100, 292)); // short title, long sub
    TEST_CHECK(fits(fit_title_subtitle(600, 100, 8, 400), 292, 100)); // long title, short sub
    TEST_CHECK(fits(fit_title_subtitle(600, 500, 8, 400), 392.0f * 0.6f, 392.0f * 0.4f));
    TEST_CHECK(fits(fit_title_subtitle(100, 50, 8, 6), 6, 0));      // budget <= gap
    TEST_CHECK(fits(fit_title_subtitle(100, 50, 8, 8), 8, 0));
    TEST_CHECK(fits(fit_title_subtitle(100, 50, 8, 0), 0, 0));      // zero budget
    TEST_CHECK(fits(fit_title_subtitle(0, 50, 8, 400), 0, 50));     // no title
    TEST_CHECK(near(blaze4k::kSubtitleMinShare, 0.4f));

    // Non-finite and negative inputs count as 0, and outputs stay finite and >= 0.
    const float nan = std::nanf("");
    const float inf = INFINITY;
    for (const float bad : {nan, inf, -inf, -5.0f}) {
        for (const TitleSubtitleFit f :
             {fit_title_subtitle(bad, 50, 8, 400), fit_title_subtitle(100, bad, 8, 400),
              fit_title_subtitle(100, 50, bad, 400), fit_title_subtitle(100, 50, 8, bad)}) {
            TEST_CHECK(std::isfinite(f.title_max_w) && f.title_max_w >= 0.0f);
            TEST_CHECK(std::isfinite(f.subtitle_max_w) && f.subtitle_max_w >= 0.0f);
        }
    }
    TEST_CHECK(fits(fit_title_subtitle(100, nan, 8, 400), 100, 0));
    TEST_CHECK(fits(fit_title_subtitle(100, 50, 8, inf), 0, 0));
    TEST_CHECK(fits(fit_title_subtitle(100, 50, -3, 400), 100, 50)); // gap -> 0

    // Invariants over a grid.
    const float values[] = {0, 1, 7.5f, 40, 100, 291.9f, 292, 399, 400, 1000, 5000};
    const float gaps[] = {0, 8, 10, 50};
    for (const float t : values) {
        for (const float s : values) {
            for (const float g : gaps) {
                for (const float b : values) {
                    const TitleSubtitleFit f = fit_title_subtitle(t, s, g, b);
                    TEST_CHECK(std::isfinite(f.title_max_w) && f.title_max_w >= 0.0f);
                    TEST_CHECK(std::isfinite(f.subtitle_max_w) && f.subtitle_max_w >= 0.0f);
                    TEST_CHECK(f.title_max_w <= t);
                    TEST_CHECK(f.subtitle_max_w <= s);
                    TEST_CHECK(f.title_max_w <= b);
                    if (f.subtitle_max_w > 0.0f) {
                        TEST_CHECK(f.title_max_w + g + f.subtitle_max_w <= b + 1e-3f);
                        // The subtitle keeps its whole width or >= 40% of the space after the gap.
                        TEST_CHECK(f.subtitle_max_w >= std::min(s, (b - g) * 0.4f) - 1e-3f);
                    }
                }
            }
        }
    }
    std::cout << "  - subtitle display choice and title/subtitle fit ok.\n";
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

std::size_t cells(const std::string& text) {
    return static_cast<std::size_t>(text_width(text, 1.0f) / 6.0f);
}

bool ends_with_ellipsis(const std::string& text) {
    return text.size() >= 3 && text.compare(text.size() - 3, 3, "...") == 0;
}

// truncate_to_cells is a thin wrapper over truncate_to_width (#90); the full
// truncation contract is pinned in unicode_text_test. This is a wrapper smoke.
void test_truncate_to_cells() {
    using blaze4k::truncate_to_cells;

    const std::string bagpipe = truncate_to_cells("mDaWg & Hatena Zubon", 17);
    TEST_CHECK(bagpipe == "mDaWg & Hatena...");
    TEST_CHECK(text_width(bagpipe, 1.0f) == 17.0f * 6.0f);
    TEST_CHECK(truncate_to_cells("mDaWg", 17) == "mDaWg");
    // Budget below 3: a prefix with no ellipsis.
    TEST_CHECK(truncate_to_cells("ABCDEF", 2) == "AB");
    TEST_CHECK(truncate_to_cells("ABCDEF", 0).empty());
    std::cout << "  - truncate_to_cells ok.\n";
}

blaze4k::Chart make_chart(const std::string& difficulty, const std::string& description,
                          int meter) {
    blaze4k::Chart chart;
    chart.difficulty = difficulty;
    chart.description = description;
    chart.meter = meter;
    return chart;
}

void test_chart_display_label() {
    using blaze4k::chart_display_label;

    TEST_CHECK(chart_display_label(make_chart("Edit", "JBEAN", 10), 17) == "JBEAN");
    TEST_CHECK(chart_display_label(make_chart("Edit", "", 10), 17) == "Edit");
    TEST_CHECK(chart_display_label(make_chart("edit", "", 10), 17) == "edit"); // passthrough
    TEST_CHECK(chart_display_label(make_chart("edit", "JBEAN", 10), 17) == "JBEAN");
    // Non-Edit charts never show the description.
    TEST_CHECK(chart_display_label(make_chart("Hard", "Some Author", 9), 17) == "Hard");
    TEST_CHECK(chart_display_label(make_chart("Challenge", "", 12), 17) == "Challenge");
    TEST_CHECK(chart_display_label(make_chart("Beginner", "x", 1), 2) == "Beginner"); // no truncation
    // Invalid label resolves through the description (SM5 IsAnEdit on the
    // resolved difficulty).
    TEST_CHECK(chart_display_label(make_chart("", "Edit", 10), 17) == "Edit");
    // Long Edit name: shortened to the budget.
    const std::string long_name = chart_display_label(
        make_chart("Edit", "A Very Long Custom Edit Chart Name", 10), 10);
    TEST_CHECK(cells(long_name) == 10);
    TEST_CHECK(ends_with_ellipsis(long_name));
    TEST_CHECK(long_name == "A Very ...");
    std::cout << "  - chart_display_label ok.\n";
}

} // namespace

int main() {
    std::cout << "[bitmap_font_test] Running bitmap font tests...\n";
    test_punctuation_glyphs();
    test_fold_and_placeholder();
    test_text_width_multibyte();
    test_coverage();
    test_translit_fallback();
    test_subtitle_display_and_fit();
    test_malformed_draw_smoke();
    test_truncate_to_cells();
    test_chart_display_label();
    std::cout << "[bitmap_font_test] All tests passed.\n";
    return 0;
}
