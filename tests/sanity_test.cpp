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

int main() {
    std::cout << "[sanity_test] Running C++20 and library checks...\n";

    // Test C++20 concept
    assert(add(10, 20) == 30);
    assert(add(1.5, 2.5) == 4.0);

    // Test std::span (C++20)
    std::vector<int> nums = {1, 2, 3, 4};
    std::span<int> sp(nums);
    assert(sp.size() == 4);
    assert(sp[0] == 1);

    // Test nlohmann::json
    nlohmann::json j;
    j["title"] = "Tundra Dance";
    j["panels"] = 4;
    j["fps"] = 60;
    
    std::string serialized = j.dump();
    auto parsed = nlohmann::json::parse(serialized);
    assert(parsed["title"] == "Tundra Dance");
    assert(parsed["panels"] == 4);
    assert(parsed["fps"] == 60);

    std::cout << "[sanity_test] All checks passed successfully!\n";
    return 0;
}
