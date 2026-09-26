#include "gameplay/hud_renderer.hpp"

#include <cstdint>
#include <string>

#include "render/gl_quad_renderer.hpp"

namespace td {

namespace {

// Tundra's own presentation palette (unsourced; no OpenITG parity requirement).
constexpr Color kTextColor{0.95f, 0.97f, 1.0f, 1.0f};
constexpr Color kFantasticColor{0.40f, 0.90f, 1.00f, 1.0f};
constexpr Color kExcellentColor{0.55f, 1.00f, 0.45f, 1.0f};
constexpr Color kGreatColor{1.00f, 0.90f, 0.30f, 1.0f};
constexpr Color kDecentColor{1.00f, 1.00f, 1.00f, 1.0f};
constexpr Color kWayOffColor{1.00f, 0.60f, 0.20f, 1.0f};
constexpr Color kMissColor{1.00f, 0.30f, 0.30f, 1.0f};
constexpr Color kHoldOkColor{0.45f, 0.65f, 1.00f, 1.0f};
constexpr Color kHoldNgColor{1.00f, 0.30f, 0.30f, 1.0f};

// Self-contained 5x7 bitmap font. Each row is 5 bits; bit 4 (0x10) is leftmost.
struct Glyph {
    char symbol;
    std::uint8_t rows[7];
};

constexpr Glyph kGlyphs[] = {
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'%', {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}},
    {'x', {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'*', {0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
};

const std::uint8_t* glyph_for(char symbol) {
    for (const Glyph& glyph : kGlyphs) {
        if (glyph.symbol == symbol) {
            return glyph.rows;
        }
    }
    return nullptr;
}

float text_width(const std::string& text, float pixel) {
    return static_cast<float>(text.size()) * 6.0f * pixel;
}

void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y, float pixel,
               Color color) {
    float cursor = x;
    for (char symbol : text) {
        const std::uint8_t* rows = glyph_for(symbol);
        if (rows != nullptr) {
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if ((rows[row] >> (4 - col)) & 1u) {
                        renderer.draw_quad(
                            Rect{cursor + static_cast<float>(col) * pixel,
                                 y + static_cast<float>(row) * pixel, pixel, pixel},
                            color);
                    }
                }
            }
        }
        cursor += 6.0f * pixel;
    }
}

} // namespace

std::string format_percent(double percent) {
    // Display-clamp to [0,1] (PercentageDisplay.cpp:110-116), then the OpenITG
    // +0.000001 boost and truncation to two decimals.
    double display = percent;
    if (display < 0.0) {
        display = 0.0;
    } else if (display > 1.0) {
        display = 1.0;
    }

    const int hundredths = static_cast<int>((display + 0.000001) * 100.0 * 100.0);
    const int whole = hundredths / 100;
    const int fraction = hundredths % 100;

    std::string text = std::to_string(whole) + ".";
    if (fraction < 10) {
        text += "0";
    }
    text += std::to_string(fraction);
    text += "%";
    return text;
}

std::string format_combo(int combo) {
    return std::to_string(combo);
}

std::string format_grade(const GradeTier& grade) {
    const std::string label = grade.label;
    if (label == "quad_star") {
        return "****";
    }
    if (label == "triple_star") {
        return "***";
    }
    if (label == "double_star") {
        return "**";
    }
    if (label == "single_star") {
        return "*";
    }
    return label;
}

void HudRenderer::render(const ScoreState& state, int screen_w, int screen_h,
                         GlQuadRenderer& renderer) const {
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }

    const float width = static_cast<float>(screen_w);
    const float height = static_cast<float>(screen_h);
    const float main_pixel = 3.0f;

    // Top-left: live percent.
    draw_text(renderer, format_percent(state.percent), 8.0f, 8.0f, main_pixel, kTextColor);

    // Top-centre: live combo.
    const std::string combo_text = format_combo(state.combo) + "x";
    draw_text(renderer, combo_text, (width - text_width(combo_text, main_pixel)) * 0.5f, 8.0f,
              main_pixel, kTextColor);

    // Top-right: color-coded per-window judgment chips (tap windows + hold outcomes).
    const float chip_pixel = 2.0f;
    const float square = 10.0f;
    const float gap = 4.0f;
    const float right = width - 8.0f;
    float chip_y = 8.0f;

    const auto draw_chip = [&](int count, Color color, float& y) {
        const std::string count_text = std::to_string(count);
        const float row_total = square + gap + text_width(count_text, chip_pixel);
        const float x = right - row_total;
        renderer.draw_quad(Rect{x, y, square, square}, color);
        draw_text(renderer, count_text, x + square + gap, y, chip_pixel, kTextColor);
        y += square + gap;
    };

    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Fantastic)], kFantasticColor,
              chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Excellent)], kExcellentColor,
              chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Great)], kGreatColor, chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Decent)], kDecentColor,
              chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::WayOff)], kWayOffColor,
              chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Miss)], kMissColor, chip_y);
    draw_chip(state.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)], kHoldOkColor, chip_y);
    draw_chip(state.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)], kHoldNgColor, chip_y);

    // Bottom-centre: live grade.
    if (state.grade != nullptr) {
        const std::string grade_text = format_grade(*state.grade);
        draw_text(renderer, grade_text,
                  (width - text_width(grade_text, main_pixel)) * 0.5f,
                  height - 8.0f - 7.0f * main_pixel, main_pixel, kTextColor);
    }
}

} // namespace td
