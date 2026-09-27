#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "render/note_art.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::ArrowDirection;

constexpr int kSize = 64;

int alpha_count(const std::vector<std::uint8_t>& rgba) {
    int count = 0;
    for (std::size_t i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] > 0) {
            ++count;
        }
    }
    return count;
}

std::uint8_t alpha_at(const std::vector<std::uint8_t>& rgba, int x, int y) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * kSize + static_cast<std::size_t>(x)) * 4u + 3u;
    return rgba[index];
}

std::uint64_t alpha_hash(const std::vector<std::uint8_t>& rgba) {
    std::uint64_t hash = 1469598103934665603ull; // FNV-1a
    for (std::size_t i = 3; i < rgba.size(); i += 4) {
        hash ^= rgba[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

void check_buffer(const std::vector<std::uint8_t>& rgba, bool expect_center) {
    TEST_CHECK(rgba.size() == static_cast<std::size_t>(kSize) * kSize * 4u);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        TEST_CHECK(rgba[i] == 255 && rgba[i + 1] == 255 && rgba[i + 2] == 255);
    }
    if (expect_center) {
        TEST_CHECK(alpha_at(rgba, kSize / 2, kSize / 2) > 0);
    }
    // Top-left corner is outside every shape.
    TEST_CHECK(alpha_at(rgba, 0, 0) == 0);
}

void test_buffer_shape_and_channels() {
    check_buffer(td::make_arrow_rgba(kSize, ArrowDirection::Up), true);
    check_buffer(td::make_hold_head_rgba(kSize, ArrowDirection::Up), true);
    check_buffer(td::make_roll_head_rgba(kSize, ArrowDirection::Up), true);
    check_buffer(td::make_mine_rgba(kSize), true);
    check_buffer(td::make_body_rgba(kSize), true);
    const std::vector<std::uint8_t> disc = td::make_disc_rgba(kSize);
    check_buffer(disc, true);
    TEST_CHECK(alpha_at(disc, kSize / 2, kSize / 2) == 255);
    TEST_CHECK(alpha_at(disc, 0, 0) == 0); // corners lie outside the circle
    // Receptors are hollow: centre must be empty, but coverage exists.
    const std::vector<std::uint8_t> receptor = td::make_receptor_rgba(kSize, ArrowDirection::Up);
    check_buffer(receptor, false);
    TEST_CHECK(alpha_at(receptor, kSize / 2, kSize / 2) == 0);
    TEST_CHECK(alpha_count(receptor) > 0);
    std::cout << "  - buffer shape, white RGB, and coverage ok.\n";
}

void test_masks_are_distinct() {
    const std::uint64_t arrow = alpha_hash(td::make_arrow_rgba(kSize, ArrowDirection::Up));
    const std::uint64_t hold = alpha_hash(td::make_hold_head_rgba(kSize, ArrowDirection::Up));
    const std::uint64_t roll = alpha_hash(td::make_roll_head_rgba(kSize, ArrowDirection::Up));
    const std::uint64_t mine = alpha_hash(td::make_mine_rgba(kSize));
    const std::uint64_t body = alpha_hash(td::make_body_rgba(kSize));

    const std::uint64_t hashes[5] = {arrow, hold, roll, mine, body};
    for (int i = 0; i < 5; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            TEST_CHECK(hashes[i] != hashes[j]);
        }
    }
    std::cout << "  - tap/hold/roll/mine/body masks are distinct ok.\n";
}

void test_direction_rotation() {
    const std::vector<std::uint8_t> up = td::make_arrow_rgba(kSize, ArrowDirection::Up);
    const std::vector<std::uint8_t> left = td::make_arrow_rgba(kSize, ArrowDirection::Left);
    const std::vector<std::uint8_t> right = td::make_arrow_rgba(kSize, ArrowDirection::Right);
    const std::vector<std::uint8_t> down = td::make_arrow_rgba(kSize, ArrowDirection::Down);

    TEST_CHECK(up != left);
    TEST_CHECK(left != right);
    TEST_CHECK(right != down);

    // A 90-degree rotation preserves the covered-pixel count.
    const int up_count = alpha_count(up);
    TEST_CHECK(up_count > 0);
    TEST_CHECK(alpha_count(left) == up_count);
    TEST_CHECK(alpha_count(right) == up_count);
    TEST_CHECK(alpha_count(down) == up_count);
    std::cout << "  - direction rotation changes the mask and preserves coverage ok.\n";
}

void test_zero_size_is_empty() {
    TEST_CHECK(td::make_arrow_rgba(0, ArrowDirection::Up).empty());
    TEST_CHECK(td::make_hold_head_rgba(-3, ArrowDirection::Left).empty());
    TEST_CHECK(td::make_roll_head_rgba(0, ArrowDirection::Down).empty());
    TEST_CHECK(td::make_mine_rgba(0).empty());
    TEST_CHECK(td::make_receptor_rgba(-1, ArrowDirection::Right).empty());
    TEST_CHECK(td::make_body_rgba(0).empty());
    std::cout << "  - size <= 0 yields an empty buffer ok.\n";
}

} // namespace

int main() {
    std::cout << "[note_art_test] Running procedural noteskin mask tests...\n";
    test_buffer_shape_and_channels();
    test_masks_are_distinct();
    test_direction_rotation();
    test_zero_size_is_empty();
    std::cout << "[note_art_test] All tests passed!\n";
    return 0;
}
