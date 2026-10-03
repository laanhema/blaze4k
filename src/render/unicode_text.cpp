#include "render/unicode_text.hpp"

#include <cstddef>
#include <vector>

namespace blaze4k {

namespace {

// ASCII folds for U+00C0-U+00FF, 16 code points per row. '*' means "no fold"
// (the code point draws the placeholder), never a literal asterisk.
constexpr const char* kLatin1Folds[4] = {
    "AAAAAA*CEEEEIIII", // U+00C0-U+00CF
    "DNOOOOOxOUUUUY**", // U+00D0-U+00DF
    "aaaaaa*ceeeeiiii", // U+00E0-U+00EF
    "dnooooo*ouuuuy*y", // U+00F0-U+00FF
};

// ASCII folds for U+0100-U+017F (Latin Extended-A), same encoding.
constexpr const char* kLatinExtAFolds[8] = {
    "AaAaAaCcCcCcCcDd", // U+0100-U+010F
    "DdEeEeEeEeEeGgGg", // U+0110-U+011F
    "GgGgHhHhIiIiIiIi", // U+0120-U+012F
    "Ii**JjKkkLlLlLlL", // U+0130-U+013F
    "lLlNnNnNnnNnOoOo", // U+0140-U+014F
    "Oo**RrRrRrSsSsSs", // U+0150-U+015F
    "SsTtTtTtUuUuUuUu", // U+0160-U+016F
    "UuUuWwYyYZzZzZzs", // U+0170-U+017F
};

char table_fold(const char* const* rows, char32_t offset) {
    const char folded = rows[offset / 16][offset % 16];
    return folded == '*' ? '\0' : folded;
}

bool is_continuation(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

} // namespace

char32_t next_code_point(std::string_view text, std::size_t& pos) {
    const std::size_t size = text.size();
    if (pos >= size) {
        // Precondition violation: clamp and report, never hang or read OOB.
        pos = size;
        return kReplacementChar;
    }

    const auto lead = static_cast<unsigned char>(text[pos]);
    if (lead < 0x80u) {
        ++pos;
        return static_cast<char32_t>(lead);
    }

    std::size_t length = 0;
    unsigned char second_min = 0x80u;
    unsigned char second_max = 0xBFu;
    char32_t value = 0;
    if (lead >= 0xC2u && lead <= 0xDFu) {
        length = 2;
        value = lead & 0x1Fu;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
        length = 3;
        value = lead & 0x0Fu;
        if (lead == 0xE0u) {
            second_min = 0xA0u; // reject overlongs
        } else if (lead == 0xEDu) {
            second_max = 0x9Fu; // reject surrogates
        }
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
        length = 4;
        value = lead & 0x07u;
        if (lead == 0xF0u) {
            second_min = 0x90u; // reject overlongs
        } else if (lead == 0xF4u) {
            second_max = 0x8Fu; // reject > U+10FFFF
        }
    } else {
        // 0x80-0xC1 (stray continuation / overlong lead) or 0xF5-0xFF.
        ++pos;
        return kReplacementChar;
    }

    for (std::size_t i = 1; i < length; ++i) {
        if (pos + i >= size) {
            // Truncated at the end of the string.
            pos = size;
            return kReplacementChar;
        }
        const auto byte = static_cast<unsigned char>(text[pos + i]);
        const bool ok = i == 1 ? (byte >= second_min && byte <= second_max)
                               : is_continuation(byte);
        if (!ok) {
            // Resync at the offending byte (StepMania utf8_to_wchar_ec).
            pos += i;
            return kReplacementChar;
        }
        value = (value << 6) | (byte & 0x3Fu);
    }

    pos += length;
    return value;
}

bool is_zero_width(char32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||   // combining diacritical marks
           (cp >= 0x200B && cp <= 0x200F) ||   // ZWSP, ZWNJ, ZWJ, LRM, RLM
           cp == 0x2060 ||                     // word joiner
           (cp >= 0x20D0 && cp <= 0x20FF) ||   // combining marks for symbols
           cp == 0x3099 || cp == 0x309A ||     // combining kana (semi-)voiced marks
           (cp >= 0xFE00 && cp <= 0xFE0F) ||   // variation selectors
           (cp >= 0xFE20 && cp <= 0xFE2F) ||   // combining half marks
           cp == 0xFEFF ||                     // BOM / ZWNBSP
           (cp >= 0x1F3FB && cp <= 0x1F3FF) || // emoji skin-tone modifiers
           (cp >= 0xE0020 && cp <= 0xE007F);   // tag characters
}

char fold_to_ascii(char32_t cp) {
    if (cp == 0x0009) {
        return ' ';
    }
    if (cp < 0x80) {
        return '\0';
    }
    if (cp >= 0x00C0 && cp <= 0x00FF) {
        return table_fold(kLatin1Folds, cp - 0x00C0);
    }
    if (cp >= 0x0100 && cp <= 0x017F) {
        return table_fold(kLatinExtAFolds, cp - 0x0100);
    }
    if (cp >= 0xFF01 && cp <= 0xFF5E) {
        return static_cast<char>(cp - 0xFEE0); // fullwidth ASCII
    }
    if (cp >= 0x2010 && cp <= 0x2015) {
        return '-';
    }
    if ((cp >= 0x2018 && cp <= 0x201B) || cp == 0x2032) {
        return '\'';
    }
    if ((cp >= 0x201C && cp <= 0x201F) || cp == 0x2033) {
        return '"';
    }
    switch (cp) {
    case 0x00A0: // NBSP
    case 0x3000: // ideographic space
        return ' ';
    case 0x00A1: // inverted exclamation
        return '!';
    case 0x00BF: // inverted question
        return '?';
    case 0x00AB: // left guillemet
        return '<';
    case 0x00BB: // right guillemet
        return '>';
    case 0x00B4: // acute accent
        return '\'';
    case 0x00B9: // superscript one
        return '1';
    case 0x00B2: // superscript two
        return '2';
    case 0x00B3: // superscript three
        return '3';
    case 0x2026: // horizontal ellipsis (one cell)
        return '.';
    case 0x2605: // black star
    case 0x2606: // white star
        return '*';
    case 0x301C: // wave dash
        return '~';
    default:
        return '\0';
    }
}

std::string truncate_to_width(std::string_view text, float max_width,
                              const std::function<float(std::string_view)>& measure) {
    constexpr float kEpsilon = 1e-3f;
    constexpr std::string_view kEllipsis = "...";
    if (!(max_width >= 0.0f)) {
        max_width = 0.0f; // NaN or negative
    }
    const auto fits = [&](std::string_view candidate) {
        return measure(candidate) <= max_width + kEpsilon;
    };
    if (fits(text)) {
        return std::string(text);
    }

    // Candidate cut offsets: 0, every offset where a visible code point
    // starts, and the full length. Ascending, so the measured width of the
    // prefix (with or without the ellipsis) never decreases along the list.
    std::vector<std::size_t> cuts;
    cuts.push_back(0);
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        if (!is_zero_width(next_code_point(text, pos)) && start != cuts.back()) {
            cuts.push_back(start);
        }
    }
    if (cuts.back() != text.size()) {
        cuts.push_back(text.size());
    }

    const bool ellipsis = fits(kEllipsis);
    std::string scratch;
    scratch.reserve(text.size() + kEllipsis.size());
    const auto cut_fits = [&](std::size_t cut) {
        scratch.assign(text.substr(0, cut));
        if (ellipsis) {
            scratch += kEllipsis;
        }
        return fits(scratch);
    };

    // Longest fitting candidate by binary search. Every accepted index was
    // measured, so the result holds even if `measure` is not monotonic.
    std::size_t lo = 0;
    std::size_t hi = cuts.size();
    std::size_t best = 0;
    bool found = false;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (cut_fits(cuts[mid])) {
            best = mid;
            found = true;
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    std::string out(text.substr(0, found ? cuts[best] : 0));
    if (ellipsis) {
        out += kEllipsis;
    }
    if (!found && !fits(out)) {
        out.clear(); // a measure where even the shortest candidate overflows
    }
    return out;
}

} // namespace blaze4k
