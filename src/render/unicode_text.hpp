#pragma once

#include <cstddef>
#include <string_view>

namespace blaze4k {

// Font-agnostic text helpers shared by the bitmap font and the future TrueType
// atlas (#55). Simfile text is untrusted: every function is bounds-checked and
// never throws or allocates.

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

} // namespace blaze4k
