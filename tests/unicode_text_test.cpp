#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

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

using blaze4k::fold_to_ascii;
using blaze4k::is_zero_width;
using blaze4k::kReplacementChar;
using blaze4k::next_code_point;

// Decodes all of `text`, asserting the progress postcondition on every step.
std::vector<char32_t> decode_all(std::string_view text) {
    std::vector<char32_t> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t before = pos;
        out.push_back(next_code_point(text, pos));
        TEST_CHECK(pos > before);
        TEST_CHECK(pos <= text.size());
    }
    TEST_CHECK(pos == text.size());
    TEST_CHECK(out.size() <= text.size());
    return out;
}

std::vector<std::string> random_strings(std::size_t count) {
    std::mt19937 rng{77};
    std::uniform_int_distribution<int> length_dist(0, 64);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::vector<std::string> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string s(static_cast<std::size_t>(length_dist(rng)), '\0');
        for (char& c : s) {
            c = static_cast<char>(byte_dist(rng));
        }
        out.push_back(std::move(s));
    }
    return out;
}

constexpr char32_t R = kReplacementChar;

void test_ascii_and_multibyte_decode() {
    const std::string_view dont = "Don't";
    const std::vector<char32_t> decoded = decode_all(dont);
    TEST_CHECK(decoded.size() == 5);
    for (std::size_t i = 0; i < dont.size(); ++i) {
        TEST_CHECK(decoded[i] == static_cast<char32_t>(dont[i]));
    }
    TEST_CHECK(decode_all("\xC2\xB3") == std::vector<char32_t>{0x00B3});
    TEST_CHECK(decode_all("\xE2\x98\xBA") == std::vector<char32_t>{0x263A});
    TEST_CHECK(decode_all("\xF0\x9F\x8E\xB5") == std::vector<char32_t>{0x1F3B5});
    const std::vector<char32_t> vertex = decode_all("VerTex\xC2\xB3");
    TEST_CHECK(vertex.size() == 7);
    TEST_CHECK(vertex.back() == 0x00B3);
    // Boundary code points of each length decode exactly.
    TEST_CHECK(decode_all("\xC2\x80") == std::vector<char32_t>{0x0080});
    TEST_CHECK(decode_all("\xE0\xA0\x80") == std::vector<char32_t>{0x0800});
    TEST_CHECK(decode_all("\xEF\xBF\xBF") == std::vector<char32_t>{0xFFFF});
    TEST_CHECK(decode_all("\xF4\x8F\xBF\xBF") == std::vector<char32_t>{0x10FFFF});
    std::cout << "  - ASCII and multi-byte decode ok.\n";
}

void test_malformed_resync() {
    TEST_CHECK(decode_all("\x80") == std::vector<char32_t>{R});
    TEST_CHECK(decode_all("a\x80" "b") == (std::vector<char32_t>{U'a', R, U'b'}));
    TEST_CHECK(decode_all("\xFF") == std::vector<char32_t>{R});
    // Overlong '/' : C0 is an invalid lead, AF a stray continuation.
    TEST_CHECK(decode_all("\xC0\xAF") == (std::vector<char32_t>{R, R}));
    // Overlong 3-byte (E0 needs A0-BF second byte).
    TEST_CHECK(decode_all("\xE0\x80\x80") == (std::vector<char32_t>{R, R, R}));
    // Surrogate U+D800: ED only allows 80-9F, then two strays.
    TEST_CHECK(decode_all("\xED\xA0\x80") == (std::vector<char32_t>{R, R, R}));
    // Above U+10FFFF: F4 only allows 80-8F, then three strays.
    TEST_CHECK(decode_all("\xF4\x90\x80\x80") == (std::vector<char32_t>{R, R, R, R}));

    // Truncation at the end advances to size().
    {
        const std::string_view truncated = "\xE2\x98";
        std::size_t pos = 0;
        TEST_CHECK(next_code_point(truncated, pos) == R);
        TEST_CHECK(pos == 2);
    }
    TEST_CHECK(decode_all("\xF0\x9F\x8E") == std::vector<char32_t>{R});

    // A missing continuation mid-string resyncs at the offending byte.
    TEST_CHECK(decode_all("\xE2(") == (std::vector<char32_t>{R, U'('}));
    TEST_CHECK(decode_all("Bad\xC3\x28\xFF Title") ==
               (std::vector<char32_t>{U'B', U'a', U'd', R, U'(', R, U' ', U'T', U'i', U't', U'l',
                                      U'e'}));

    // Precondition violation is clamped instead of reading out of bounds.
    {
        std::size_t pos = 10;
        TEST_CHECK(next_code_point("abc", pos) == R);
        TEST_CHECK(pos == 3);
    }
    std::cout << "  - malformed input -> U+FFFD with resync ok.\n";
}

void test_fuzz_no_hang() {
    for (const std::string& s : random_strings(10000)) {
        (void)decode_all(s);
    }
    // Exhaustive 1- and 2-byte inputs.
    for (int a = 0; a < 256; ++a) {
        const std::string one(1, static_cast<char>(a));
        TEST_CHECK(decode_all(one).size() == 1);
        for (int b = 0; b < 256; ++b) {
            std::string two;
            two.push_back(static_cast<char>(a));
            two.push_back(static_cast<char>(b));
            const std::vector<char32_t> decoded = decode_all(two);
            TEST_CHECK(!decoded.empty() && decoded.size() <= 2);
            for (char32_t cp : decoded) {
                TEST_CHECK(cp <= 0x10FFFF);
                TEST_CHECK(cp < 0xD800 || cp > 0xDFFF);
            }
        }
    }
    std::cout << "  - 10k seeded fuzz + exhaustive 1/2-byte decode ok.\n";
}

void test_zero_width() {
    TEST_CHECK(is_zero_width(0x0301));
    TEST_CHECK(is_zero_width(0x200D));
    TEST_CHECK(is_zero_width(0xFE0F));
    TEST_CHECK(is_zero_width(0xFEFF));
    TEST_CHECK(is_zero_width(0x200B));
    TEST_CHECK(is_zero_width(0x2060));
    TEST_CHECK(is_zero_width(0x3099)); // combining kana voiced mark (NFD Japanese)
    TEST_CHECK(is_zero_width(0x309A)); // combining kana semi-voiced mark
    TEST_CHECK(is_zero_width(0x20D0)); // combining marks for symbols: range ends
    TEST_CHECK(is_zero_width(0x20FF));
    TEST_CHECK(is_zero_width(0xFE20)); // combining half marks: range ends
    TEST_CHECK(is_zero_width(0xFE2F));
    TEST_CHECK(is_zero_width(0x1F3FB)); // emoji skin-tone modifiers: range ends
    TEST_CHECK(is_zero_width(0x1F3FF));
    TEST_CHECK(is_zero_width(0xE0020)); // tag characters: range ends
    TEST_CHECK(is_zero_width(0xE007F));
    // Neighbours just outside the new ranges still take a cell.
    TEST_CHECK(!is_zero_width(0x20CF));
    TEST_CHECK(!is_zero_width(0x2100));
    TEST_CHECK(!is_zero_width(0x3098));
    TEST_CHECK(!is_zero_width(0x309B)); // spacing voiced mark
    TEST_CHECK(!is_zero_width(0xFE1F));
    TEST_CHECK(!is_zero_width(0xFE30));
    TEST_CHECK(!is_zero_width(0x1F3FA));
    TEST_CHECK(!is_zero_width(0x1F400));
    TEST_CHECK(!is_zero_width(0xE001F));
    TEST_CHECK(!is_zero_width(0xE0080));
    TEST_CHECK(!is_zero_width(U'a'));
    TEST_CHECK(!is_zero_width(0x00E9));
    TEST_CHECK(!is_zero_width(0x263A));
    TEST_CHECK(!is_zero_width(kReplacementChar));
    std::cout << "  - zero-width set ok.\n";
}

void test_fold() {
    TEST_CHECK(fold_to_ascii(0x00E9) == 'e'); // é
    TEST_CHECK(fold_to_ascii(0x00D1) == 'N'); // Ñ
    TEST_CHECK(fold_to_ascii(0x0142) == 'l'); // ł
    TEST_CHECK(fold_to_ascii(0x017D) == 'Z'); // Ž
    TEST_CHECK(fold_to_ascii(0x00C0) == 'A'); // À (first Latin-1 row)
    TEST_CHECK(fold_to_ascii(0x00FF) == 'y'); // ÿ (last Latin-1 row)
    TEST_CHECK(fold_to_ascii(0x00D7) == 'x'); // ×
    TEST_CHECK(fold_to_ascii(0x0100) == 'A'); // Ā (first Ext-A row)
    TEST_CHECK(fold_to_ascii(0x015A) == 'S'); // Ś
    TEST_CHECK(fold_to_ascii(0x017F) == 's'); // ſ (last Ext-A row)
    TEST_CHECK(fold_to_ascii(0x2019) == '\'');
    TEST_CHECK(fold_to_ascii(0x201C) == '"');
    TEST_CHECK(fold_to_ascii(0x2014) == '-');
    TEST_CHECK(fold_to_ascii(0xFF21) == 'A');
    TEST_CHECK(fold_to_ascii(0xFF01) == '!');
    TEST_CHECK(fold_to_ascii(0xFF5E) == '~');
    TEST_CHECK(fold_to_ascii(0x3000) == ' ');
    TEST_CHECK(fold_to_ascii(0x00B3) == '3');
    TEST_CHECK(fold_to_ascii(0x00B2) == '2');
    TEST_CHECK(fold_to_ascii(0x2606) == '*');
    TEST_CHECK(fold_to_ascii(0x2026) == '.');
    TEST_CHECK(fold_to_ascii(0x301C) == '~');
    TEST_CHECK(fold_to_ascii(0x0009) == ' ');

    // No fold: ligatures/multi-letter characters, symbols, CJK, plain ASCII.
    TEST_CHECK(fold_to_ascii(0x00C6) == '\0'); // Æ
    TEST_CHECK(fold_to_ascii(0x00DF) == '\0'); // ß
    TEST_CHECK(fold_to_ascii(0x0152) == '\0'); // Œ
    TEST_CHECK(fold_to_ascii(0x00F7) == '\0'); // ÷ ('*' in the table means no fold)
    TEST_CHECK(fold_to_ascii(0x263A) == '\0');
    TEST_CHECK(fold_to_ascii(0x65E5) == '\0');
    TEST_CHECK(fold_to_ascii(U'A') == '\0');
    TEST_CHECK(fold_to_ascii(0x001F) == '\0');
    TEST_CHECK(fold_to_ascii(kReplacementChar) == '\0');

    // Table integrity: every Latin-1 / Ext-A letter folds to 0 or an ASCII letter.
    for (char32_t cp = 0x00C0; cp <= 0x017F; ++cp) {
        const char folded = fold_to_ascii(cp);
        TEST_CHECK(folded == '\0' || (folded >= 'A' && folded <= 'Z') ||
                   (folded >= 'a' && folded <= 'z'));
    }
    std::cout << "  - ASCII folds ok.\n";
}

} // namespace

// The bitmap font's cell model: one 6-unit cell per visible code point.
float cell_measure(std::string_view text) {
    std::size_t cells = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (!is_zero_width(next_code_point(text, pos))) {
            ++cells;
        }
    }
    return static_cast<float>(cells) * 6.0f;
}

std::size_t cells(const std::string& text) {
    return static_cast<std::size_t>(cell_measure(text) / 6.0f);
}

// truncate_to_width with the cell measure and a budget of `max_cells` cells
// (exactly what truncate_to_cells wraps).
std::string truncate_cells(std::string_view text, std::size_t max_cells) {
    return blaze4k::truncate_to_width(text, 6.0f * static_cast<float>(max_cells), cell_measure);
}

bool ends_with_ellipsis(const std::string& text) {
    return text.size() >= 3 && text.compare(text.size() - 3, 3, "...") == 0;
}

// Ported from bitmap_font_test's truncate_to_cells cases (#90): every
// assertion now runs against the generic measure-based truncation.
void test_truncate_to_width_cells() {
    // Fits: returned unchanged, byte for byte.
    TEST_CHECK(truncate_cells("", 0).empty());
    TEST_CHECK(truncate_cells("", 17).empty());
    TEST_CHECK(truncate_cells("JBEAN", 17) == "JBEAN");
    TEST_CHECK(truncate_cells("JBEAN", 5) == "JBEAN");
    TEST_CHECK(truncate_cells("VerTex\xC2\xB3", 7) == "VerTex\xC2\xB3");

    // Real-data name longer than the 17-cell select budget at 1280 px.
    const std::string bagpipe = truncate_cells("mDaWg & Hatena Zubon", 17);
    TEST_CHECK(bagpipe == "mDaWg & Hatena...");
    TEST_CHECK(cell_measure(bagpipe) == 17.0f * 6.0f);

    // Multi-byte: never cuts inside a code point.
    const std::string multi = "VerTex\xC2\xB3 Edit Name"; // 16 cells
    TEST_CHECK(truncate_cells(multi, 10) == "VerTex\xC2\xB3...");
    TEST_CHECK(truncate_cells(multi, 9) == "VerTex...");
    TEST_CHECK(truncate_cells(multi, 8) == "VerTe...");
    for (std::size_t budget = 0; budget <= 20; ++budget) {
        const std::string out = truncate_cells(multi, budget);
        TEST_CHECK(cells(out) <= budget);
        const std::size_t cut = out.find("\xC2");
        if (cut != std::string::npos) {
            TEST_CHECK(cut + 1 < out.size() && out[cut + 1] == '\xB3');
        }
    }

    // A combining mark after the last kept glyph stays attached to it.
    const std::string combining = "abe\xCC\x81" "cdefg"; // e + U+0301, 8 cells
    TEST_CHECK(cells(combining) == 8);
    TEST_CHECK(truncate_cells(combining, 6) == "abe\xCC\x81" "...");
    TEST_CHECK(truncate_cells(combining, 2) == "ab");

    // Budgets below 3: a prefix with no ellipsis.
    TEST_CHECK(truncate_cells("ABCDEF", 0).empty());
    TEST_CHECK(truncate_cells("ABCDEF", 1) == "A");
    TEST_CHECK(truncate_cells("ABCDEF", 2) == "AB");
    TEST_CHECK(truncate_cells("ABCDEF", 3) == "...");
    TEST_CHECK(truncate_cells("ABCDEF", 4) == "A...");

    // Fuzz: random bytes and budgets; never exceeds the budget, never rewrites.
    std::mt19937 rng{84};
    std::uniform_int_distribution<int> length_dist(0, 64);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> budget_dist(0, 40);
    for (int i = 0; i < 10000; ++i) {
        std::string in(static_cast<std::size_t>(length_dist(rng)), '\0');
        for (char& c : in) {
            c = static_cast<char>(byte_dist(rng));
        }
        const auto budget = static_cast<std::size_t>(budget_dist(rng));
        const std::string out = truncate_cells(in, budget);
        TEST_CHECK(cells(out) <= budget);
        if (out != in) {
            if (budget >= 3) {
                TEST_CHECK(ends_with_ellipsis(out));
                TEST_CHECK(in.compare(0, out.size() - 3, out, 0, out.size() - 3) == 0);
            } else {
                TEST_CHECK(in.compare(0, out.size(), out) == 0);
            }
        }
    }
    std::cout << "  - truncate_to_width (cell measure, ported) ok.\n";
}

void test_truncate_to_width_variable() {
    // 'W' is 3 units wide, every other visible code point 1 unit.
    const auto measure = [](std::string_view text) {
        float width = 0.0f;
        std::size_t pos = 0;
        while (pos < text.size()) {
            const char32_t cp = next_code_point(text, pos);
            if (!is_zero_width(cp)) {
                width += cp == U'W' ? 3.0f : 1.0f;
            }
        }
        return width;
    };
    using blaze4k::truncate_to_width;

    // "WWWWaaaa" is 16 units. A 10-unit budget keeps 7 units of prefix: two
    // W (6) fit, a third (9) would not; the cut follows width, not count.
    TEST_CHECK(truncate_to_width("WWWWaaaa", 16.0f, measure) == "WWWWaaaa");
    TEST_CHECK(truncate_to_width("WWWWaaaa", 10.0f, measure) == "WW...");
    TEST_CHECK(truncate_to_width("aaaaWWWW", 10.0f, measure) == "aaaaW...");
    // Same code-point count, different widths, different cuts.
    TEST_CHECK(truncate_to_width("aaaaaaaaaaaa", 10.0f, measure) == "aaaaaaa...");
    // Ellipsis does not fit (3 units): plain prefix by width.
    TEST_CHECK(truncate_to_width("aWa", 2.0f, measure) == "a");
    TEST_CHECK(truncate_to_width("Waa", 2.0f, measure).empty());
    // Within the 1e-3 tolerance counts as fitting.
    TEST_CHECK(truncate_to_width("WWWWaaaa", 15.9995f, measure) == "WWWWaaaa");
    // Fractional budgets never overrun.
    for (float budget = 0.0f; budget <= 17.0f; budget += 0.25f) {
        const std::string out = truncate_to_width("aWaWaW\xCC\x81" "aaWW", budget, measure);
        TEST_CHECK(measure(out) <= budget + 1e-3f);
    }

    // Degenerate budgets: NaN, negative and 0 all count as 0.
    TEST_CHECK(truncate_to_width("abc", std::nanf(""), measure).empty());
    TEST_CHECK(truncate_to_width("abc", -5.0f, measure).empty());
    TEST_CHECK(truncate_to_width("abc", 0.0f, measure).empty());
    TEST_CHECK(truncate_to_width("", std::nanf(""), measure).empty());
    // Zero-width-only text measures 0, so it always fits unchanged.
    TEST_CHECK(truncate_to_width("\xCC\x81\xE2\x80\x8B", 0.0f, measure) == "\xCC\x81\xE2\x80\x8B");
    // Infinite budget: everything fits.
    TEST_CHECK(truncate_to_width("WWW", INFINITY, measure) == "WWW");
    std::cout << "  - truncate_to_width (variable width, degenerate budgets) ok.\n";
}

int main() {
    std::cout << "[unicode_text_test] Running UTF-8 decode/fold tests...\n";
    test_ascii_and_multibyte_decode();
    test_malformed_resync();
    test_fuzz_no_hang();
    test_zero_width();
    test_fold();
    test_truncate_to_width_cells();
    test_truncate_to_width_variable();
    std::cout << "[unicode_text_test] All tests passed.\n";
    return 0;
}
