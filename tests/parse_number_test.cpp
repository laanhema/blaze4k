// util/parse_number.hpp: the one text -> number path (strict for CLI/config,
// lenient for simfile fields). Every expectation is written out by hand.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "util/parse_number.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

using blaze4k::parse_double;
using blaze4k::parse_int;
using blaze4k::parse_leading_double;
using blaze4k::parse_leading_finite;
using blaze4k::parse_leading_finite_float;
using blaze4k::parse_leading_int;

int main() {
    std::cout << "[parse_number_test] Starting...\n";

    // 1. Strict double: the whole text is one finite number.
    TEST_CHECK(parse_double("16.67") == 16.67);
    TEST_CHECK(parse_double("-0.5") == -0.5);
    TEST_CHECK(parse_double(".5") == 0.5);
    TEST_CHECK(parse_double("5.") == 5.0);
    TEST_CHECK(parse_double("1e3") == 1000.0);
    TEST_CHECK(parse_double("0") == 0.0);
    for (const char* bad : {"", " 1", "1 ", "+1", "16.67abc", "abc", "0x10", "1e999", "1e-999",
                            "inf", "-inf", "nan", "-nan", "Infinity", "1,5", "--1", "."}) {
        TEST_CHECK(!parse_double(bad).has_value());
    }
    TEST_CHECK(!parse_double(std::string_view("1\0" "2", 3)).has_value()); // embedded NUL
    std::cout << "  - parse_double accepts whole finite numbers only.\n";

    // 2. Strict int: base 10, whole text, fits an int.
    TEST_CHECK(parse_int("120") == 120);
    TEST_CHECK(parse_int("-7") == -7);
    TEST_CHECK(parse_int("2147483647") == std::numeric_limits<int>::max());
    TEST_CHECK(parse_int("-2147483648") == std::numeric_limits<int>::min());
    for (const char* bad : {"", "abc", "12abc", " 12", "12 ", "+12", "1.5", "0x10", "2147483648",
                            "-2147483649", "99999999999999999999"}) {
        TEST_CHECK(!parse_int(bad).has_value());
    }
    std::cout << "  - parse_int accepts whole base-10 ints only (the --smoke-test abc abort).\n";

    // 3. Lenient double (simfile fields): leading number, trailing text ignored.
    TEST_CHECK(parse_leading_double("120.5abc") == 120.5);
    TEST_CHECK(parse_leading_double("  -0.03") == -0.03);
    TEST_CHECK(parse_leading_double("+4") == 4.0);
    TEST_CHECK(parse_leading_double("150.000=x") == 150.0);
    for (const char* bad : {"", "garbage", "=", "1e999", "-1e999"}) {
        TEST_CHECK(!parse_leading_double(bad).has_value());
    }
    // It reports a spelled-out non-finite value, for callers that flag it...
    TEST_CHECK(std::isnan(parse_leading_double("NaN").value_or(0.0)));
    TEST_CHECK(std::isinf(parse_leading_double("Infinity").value_or(0.0)));
    TEST_CHECK(std::isinf(parse_leading_double("-inf").value_or(0.0)));
    // ...and the finite form refuses it.
    TEST_CHECK(parse_leading_finite("120.5abc") == 120.5);
    for (const char* bad : {"", "garbage", "1e999", "nan", "NaN", "inf", "-inf", "Infinity"}) {
        TEST_CHECK(!parse_leading_finite(bad).has_value());
    }
    std::cout << "  - parse_leading_double / _finite follow strtod and split on finiteness.\n";

    // 4. Lenient float: float precision at the edges (SM5 StringToFloat).
    TEST_CHECK(parse_leading_finite_float("0.74") == 0.74f);
    TEST_CHECK(parse_leading_finite_float("0.73999999") == 0.74f); // rounds up as a float
    for (const char* bad : {"", "garbage", "nan", "inf", "1e39" /* finite double, inf float */}) {
        TEST_CHECK(!parse_leading_finite_float(bad).has_value());
    }
    std::cout << "  - parse_leading_finite_float keeps float rounding and overflow.\n";

    // 5. Lenient int.
    TEST_CHECK(parse_leading_int("12") == 12);
    TEST_CHECK(parse_leading_int(" 9 feet") == 9);
    TEST_CHECK(parse_leading_int("-3") == -3);
    TEST_CHECK(parse_leading_int("7.9") == 7);
    for (const char* bad : {"", "x", "99999999999", "-99999999999", "99999999999999999999999"}) {
        TEST_CHECK(!parse_leading_int(bad).has_value());
    }
    std::cout << "  - parse_leading_int follows strtol and must fit an int.\n";

    std::cout << "[parse_number_test] All parse number tests passed successfully!\n";
    return 0;
}
