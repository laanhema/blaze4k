#include "render/note_art.hpp"

#include <algorithm>
#include <cmath>

namespace blaze4k {

namespace {

// 3x3 supersampling with symmetric offsets (0, 0.5, 1) so a 90-degree rotation
// maps the sample grid onto itself and preserves coverage exactly.
constexpr int kSupersample = 3;

// Rotates an output-space normalized coordinate into the canonical "up" frame,
// so one up-pointing shape descends to the requested direction. Exact 90-degree
// steps only (no trig); inverse of the arrow's forward clockwise rotation.
void to_canonical(ArrowDirection dir, double x, double y, double& out_x, double& out_y) {
    switch (dir) {
        case ArrowDirection::Up:
            out_x = x;
            out_y = y;
            break;
        case ArrowDirection::Right:
            out_x = y;
            out_y = -x;
            break;
        case ArrowDirection::Down:
            out_x = -x;
            out_y = -y;
            break;
        case ArrowDirection::Left:
            out_x = -y;
            out_y = x;
            break;
    }
}

// Canonical up-pointing arrow: triangular head (tip at y=-1, base at
// `head_base`, half-width `head_half`) over a rectangular shaft.
bool up_arrow(double x, double y, double head_base, double head_half, double shaft_half) {
    if (y < -1.0 || y > 1.0) {
        return false;
    }
    double half = shaft_half;
    if (y <= head_base) {
        const double span = head_base + 1.0;
        half = span > 0.0 ? head_half * (y + 1.0) / span : head_half;
    }
    return std::abs(x) <= half;
}

template <typename Inside>
std::vector<uint8_t> make_mask(int size, Inside inside) {
    if (size <= 0) {
        return {};
    }

    const std::size_t side = static_cast<std::size_t>(size);
    std::vector<uint8_t> rgba(side * side * 4u, 255u);
    // Sample coordinates span [0, size] (offsets 0, 0.5, 1) and are symmetric
    // about size/2, so a 90-degree rotation preserves coverage exactly.
    const double center = static_cast<double>(size) * 0.5;
    const double half = static_cast<double>(size) * 0.5;
    const double denom = static_cast<double>(kSupersample - 1);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int hits = 0;
            for (int sy = 0; sy < kSupersample; ++sy) {
                for (int sx = 0; sx < kSupersample; ++sx) {
                    const double px = static_cast<double>(x) + static_cast<double>(sx) / denom;
                    const double py = static_cast<double>(y) + static_cast<double>(sy) / denom;
                    const double nx = (px - center) / half;
                    const double ny = (py - center) / half;
                    if (inside(nx, ny)) {
                        ++hits;
                    }
                }
            }
            const double coverage = static_cast<double>(hits) /
                                    static_cast<double>(kSupersample * kSupersample);
            const auto alpha = static_cast<uint8_t>(std::lround(coverage * 255.0));
            const std::size_t index =
                (static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x)) * 4u;
            rgba[index + 0u] = 255u;
            rgba[index + 1u] = 255u;
            rgba[index + 2u] = 255u;
            rgba[index + 3u] = alpha;
        }
    }
    return rgba;
}

} // namespace

std::vector<uint8_t> make_arrow_rgba(int size, ArrowDirection dir) {
    return make_mask(size, [dir](double x, double y) {
        double cx = 0.0;
        double cy = 0.0;
        to_canonical(dir, x, y, cx, cy);
        return up_arrow(cx, cy, -0.15, 0.9, 0.32);
    });
}

std::vector<uint8_t> make_hold_head_rgba(int size, ArrowDirection dir) {
    return make_mask(size, [dir](double x, double y) {
        double cx = 0.0;
        double cy = 0.0;
        to_canonical(dir, x, y, cx, cy);
        if (up_arrow(cx, cy, -0.25, 1.0, 0.38)) {
            return true;
        }
        // Square shoulder band across the waist distinguishes the hold head.
        return std::abs(cx) <= 0.9 && cy >= -0.10 && cy <= 0.30;
    });
}

std::vector<uint8_t> make_roll_head_rgba(int size, ArrowDirection dir) {
    return make_mask(size, [dir](double x, double y) {
        double cx = 0.0;
        double cy = 0.0;
        to_canonical(dir, x, y, cx, cy);
        if (!up_arrow(cx, cy, -0.15, 0.9, 0.32)) {
            return false;
        }
        // Segmented (dashed) fill: drop periodic bands along the arrow's length.
        const double phase = std::fmod(cy + 10.0, 0.5);
        return phase < 0.32;
    });
}

std::vector<uint8_t> make_mine_rgba(int size) {
    return make_mask(size, [](double x, double y) {
        const double r = std::sqrt(x * x + y * y);
        if (r > 1.0) {
            return false;
        }
        const double theta = std::atan2(y, x);
        const double boundary = 0.45 + 0.45 * std::max(0.0, std::cos(theta * 6.0));
        return r <= boundary;
    });
}

std::vector<uint8_t> make_receptor_rgba(int size, ArrowDirection dir) {
    return make_mask(size, [dir](double x, double y) {
        double cx = 0.0;
        double cy = 0.0;
        to_canonical(dir, x, y, cx, cy);
        // Hollow arrow outline (larger footprint, empty centre) so the receptor
        // reads as a target behind the note head.
        constexpr double kShrink = 1.55;
        return up_arrow(cx, cy, -0.15, 0.9, 0.32) &&
               !up_arrow(cx * kShrink, cy * kShrink, -0.15, 0.9, 0.32);
    });
}

std::vector<uint8_t> make_body_rgba(int size) {
    return make_mask(size, [](double x, double y) {
        const double ax = std::abs(x);
        const double ay = std::abs(y);
        if (ax > 0.40) {
            return false;
        }
        if (ay > 0.78) {
            const double t = (ay - 0.78) / 0.22;
            const double cap = std::sqrt(std::max(0.0, 1.0 - t * t));
            return ax <= 0.40 * cap;
        }
        return true;
    });
}

std::vector<uint8_t> make_disc_rgba(int size) {
    return make_mask(size, [](double x, double y) { return x * x + y * y <= 1.0; });
}

} // namespace blaze4k
