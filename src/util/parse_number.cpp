#include "util/parse_number.hpp"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace blaze4k {

namespace {

// strtod and friends need a NUL-terminated buffer. `consumed` is the number of
// characters converted (0 = no number).
template <typename T, typename Convert>
std::optional<T> convert_leading(std::string_view text, Convert convert,
                                 std::size_t* consumed = nullptr) {
    const std::string buffer(text);
    char* end = nullptr;
    errno = 0;
    const T value = convert(buffer.c_str(), &end);
    if (end == buffer.c_str() || errno == ERANGE) {
        return std::nullopt;
    }
    if (consumed != nullptr) {
        *consumed = static_cast<std::size_t>(end - buffer.c_str());
    }
    return value;
}

} // namespace

std::optional<double> parse_double(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    const char first = text.front();
    const bool starts_like_a_number =
        (first >= '0' && first <= '9') || first == '-' || first == '.';
    if (!starts_like_a_number || text.find_first_of("xX") != std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t consumed = 0;
    const std::optional<double> value = convert_leading<double>(
        text, [](const char* s, char** end) { return std::strtod(s, end); }, &consumed);
    if (!value || consumed != text.size() || !std::isfinite(*value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> parse_int(std::string_view text) {
    int value = 0;
    const char* const last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(text.data(), last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parse_leading_double(std::string_view text) {
    return convert_leading<double>(
        text, [](const char* s, char** end) { return std::strtod(s, end); });
}

std::optional<double> parse_leading_finite(std::string_view text) {
    const std::optional<double> value = parse_leading_double(text);
    return value && std::isfinite(*value) ? value : std::nullopt;
}

std::optional<float> parse_leading_finite_float(std::string_view text) {
    const std::optional<float> value = convert_leading<float>(
        text, [](const char* s, char** end) { return std::strtof(s, end); });
    return value && std::isfinite(*value) ? value : std::nullopt;
}

std::optional<int> parse_leading_int(std::string_view text) {
    const std::optional<long> value = convert_leading<long>(
        text, [](const char* s, char** end) { return std::strtol(s, end, 10); });
    if (!value || *value < std::numeric_limits<int>::min() ||
        *value > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

} // namespace blaze4k
