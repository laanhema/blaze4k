#pragma once

#include <optional>
#include <string_view>

// The one way to turn text into a number. Nothing here throws or aborts, and
// nothing depends on the locale beyond the "C" default the game never changes.
// scripts/lint.sh fails on std::stoi / std::stod / strto* / ato* / sscanf /
// std::from_chars anywhere else under src/.
namespace blaze4k {

// Strict, for command-line arguments and config strings: the whole text must
// be one number. No leading '+' or whitespace, no trailing text, no hex, and
// "inf" / "nan" are refused. parse_int is base 10 and must fit an int.
[[nodiscard]] std::optional<double> parse_double(std::string_view text);
[[nodiscard]] std::optional<int> parse_int(std::string_view text);

// Lenient, for simfile fields, like StepMania's StringToFloat / StringToInt
// (strtof / strtol): leading whitespace is skipped and text after the number
// is ignored ("120.5abc" -> 120.5). nullopt when no number starts the text or
// it is out of range.
//
// The *_finite forms also refuse a text that spells "inf" or "nan", so the
// result can be used as it is. Use them unless the caller tells a non-finite
// value apart itself, as TimingData does to flag exotic timing.
[[nodiscard]] std::optional<double> parse_leading_finite(std::string_view text);
[[nodiscard]] std::optional<float> parse_leading_finite_float(std::string_view text);
[[nodiscard]] std::optional<int> parse_leading_int(std::string_view text);
// May be infinite or NaN.
[[nodiscard]] std::optional<double> parse_leading_double(std::string_view text);

} // namespace blaze4k
