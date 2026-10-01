#pragma once

#include <cstdint>
#include <vector>

namespace blaze4k {

// Direction a note reads, matching `Note.column` (chart/note.hpp:26).
enum class ArrowDirection { Left = 0, Down = 1, Up = 2, Right = 3 };

// Procedural, direction-aware noteskin masks. Each maker returns a tightly
// packed RGBA8 buffer of exactly `size * size * 4` bytes: white RGB
// (255,255,255) with a shape-coverage alpha channel. `size <= 0` returns an
// empty vector. Pure module: no OpenGL, platform, or clock dependencies, so the
// art is unit-testable headless and always present (no binary asset).
[[nodiscard]] std::vector<uint8_t> make_arrow_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_hold_head_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_roll_head_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_mine_rgba(int size);
[[nodiscard]] std::vector<uint8_t> make_receptor_rgba(int size, ArrowDirection dir);
[[nodiscard]] std::vector<uint8_t> make_body_rgba(int size);
// Filled circle spanning the whole square (e.g. the Cel mine's glowing core).
[[nodiscard]] std::vector<uint8_t> make_disc_rgba(int size);

} // namespace blaze4k
