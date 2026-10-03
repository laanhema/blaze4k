#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace blaze4k {

// Font-agnostic text helpers shared by the bitmap font and the TrueType atlas
// renderer (#55, #90). Simfile text is untrusted: every function is
// bounds-checked and never throws; only truncate_to_width allocates (its
// result and one scratch string).

// U+FFFD, returned for every malformed or rejected UTF-8 sequence.
inline constexpr char32_t kReplacementChar = 0xFFFD;

// Decodes the code point starting at text[pos] and advances `pos` past it.
// Precondition: pos < text.size(). Always advances by >= 1 byte and never past
// text.size(). Malformed or truncated sequences, overlongs, surrogates and
// values > U+10FFFF return kReplacementChar (resync at the offending byte).
[[nodiscard]] char32_t next_code_point(std::string_view text, std::size_t& pos);

// True for code points that draw nothing and take no cell (combining marks,
// zero-width joiners/spaces, variation selectors, BOM, emoji skin-tone
// modifiers, tag characters).
[[nodiscard]] bool is_zero_width(char32_t cp);

// One-to-one ASCII stand-in for a non-ASCII (or TAB) code point, or '\0' when
// there is none. Never returns a multi-character expansion. Plain ASCII is
// never folded (the font draws it natively).
[[nodiscard]] char fold_to_ascii(char32_t cp);

// Measure-based "..." truncation for any font (#90). `measure` returns the
// width of a UTF-8 string in the caller's units (pixels for TrueType, 6 per
// cell for the bitmap font); it must not decrease as a prefix grows.
// `max_width` that is NaN or negative counts as 0; widths are compared with a
// 1e-3 tolerance.
//  1. measure(text) <= max_width: `text` is returned unchanged, byte for byte.
//  2. Else, when measure("...") <= max_width: the longest prefix P with
//     measure(P + "...") <= max_width, plus ASCII "...".
//  3. Else: the longest prefix P with measure(P) <= max_width, no ellipsis.
// P is always cut on a code-point boundary right before a visible
// (non-zero-width) code point, so zero-width marks stay attached to the last
// kept glyph. Malformed bytes are never rewritten. When the text was cut,
// measure(result) <= max_width always holds. Never throws.
[[nodiscard]] std::string truncate_to_width(
    std::string_view text, float max_width,
    const std::function<float(std::string_view)>& measure);

} // namespace blaze4k
