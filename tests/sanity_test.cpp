#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <concepts>
#include <span>
#include <nlohmann/json.hpp>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
T add(T a, T b) {
    return a + b;
}

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[sanity_test] Running C++20 and library checks...\n";

    // Test C++20 concept
    TEST_CHECK(add(10, 20) == 30);
    TEST_CHECK(add(1.5, 2.5) == 4.0);

    // Test std::span (C++20)
    std::vector<int> nums = {1, 2, 3, 4};
    std::span<int> sp(nums);
    TEST_CHECK(sp.size() == 4);
    TEST_CHECK(sp[0] == 1);

    // Test nlohmann::json
    nlohmann::json j;
    j["title"] = "Tundra Dance";
    j["panels"] = 4;
    j["fps"] = 60;
    
    std::string serialized = j.dump();
    auto parsed = nlohmann::json::parse(serialized);
    TEST_CHECK(parsed["title"] == "Tundra Dance");
    TEST_CHECK(parsed["panels"] == 4);
    TEST_CHECK(parsed["fps"] == 60);

    std::cout << "[sanity_test] All checks passed successfully!\n";
    return 0;
}
