#include "gameplay/hud_renderer.hpp"

#include <algorithm>
#include <string>

#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"

namespace blaze4k {

namespace {

// Blaze 4k's own presentation palette (unsourced; no OpenITG parity requirement).
constexpr Color kTextColor{0.95f, 0.97f, 1.0f, 1.0f};
constexpr Color kFantasticColor{0.40f, 0.90f, 1.00f, 1.0f};
constexpr Color kExcellentColor{0.55f, 1.00f, 0.45f, 1.0f};
constexpr Color kGreatColor{1.00f, 0.90f, 0.30f, 1.0f};
constexpr Color kDecentColor{1.00f, 1.00f, 1.00f, 1.0f};
constexpr Color kWayOffColor{1.00f, 0.60f, 0.20f, 1.0f};
constexpr Color kMissColor{1.00f, 0.30f, 0.30f, 1.0f};
constexpr Color kHoldOkColor{0.45f, 0.65f, 1.00f, 1.0f};
constexpr Color kHoldNgColor{1.00f, 0.30f, 0.30f, 1.0f};

// Life bar palette + geometry (Blaze 4k presentation, unsourced; no OpenITG parity
// requirement). The only semantic value is the 0.3 danger threshold.
constexpr Color kLifeBackColor{0.10f, 0.12f, 0.18f, 0.90f};
constexpr Color kLifeFrameColor{0.55f, 0.60f, 0.70f, 1.0f};
constexpr Color kLifeFillColor{0.40f, 0.90f, 1.00f, 1.0f};
constexpr Color kLifeDangerColor{1.00f, 0.30f, 0.30f, 1.0f};
constexpr double kLifeDangerThreshold = 0.3;

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

Color judgment_color(JudgmentKind kind, TapJudgment window, HoldJudgment hold) {
    (void)hold;
    switch (kind) {
        case JudgmentKind::Tap:
            switch (window) {
                case TapJudgment::Fantastic: return kFantasticColor;
                case TapJudgment::Excellent: return kExcellentColor;
                case TapJudgment::Great: return kGreatColor;
                case TapJudgment::Decent: return kDecentColor;
                case TapJudgment::WayOff: return kWayOffColor;
                case TapJudgment::Miss: return kMissColor;
                case TapJudgment::HitMine: return kMissColor;
                case TapJudgment::Num: return kTextColor;
            }
            return kTextColor;
        case JudgmentKind::Miss: return kMissColor;
        case JudgmentKind::HitMine: return kMissColor;
        case JudgmentKind::HoldOk:
        case JudgmentKind::RollOk: return kHoldOkColor;
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollNg: return kHoldNgColor;
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit: return kTextColor;
    }
    return kTextColor;
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

    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Fantastic)],
              judgment_color(JudgmentKind::Tap, TapJudgment::Fantastic, HoldJudgment::Num), chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Excellent)],
              judgment_color(JudgmentKind::Tap, TapJudgment::Excellent, HoldJudgment::Num), chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Great)],
              judgment_color(JudgmentKind::Tap, TapJudgment::Great, HoldJudgment::Num), chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Decent)],
              judgment_color(JudgmentKind::Tap, TapJudgment::Decent, HoldJudgment::Num), chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::WayOff)],
              judgment_color(JudgmentKind::Tap, TapJudgment::WayOff, HoldJudgment::Num), chip_y);
    draw_chip(state.tap_counts[static_cast<std::size_t>(TapJudgment::Miss)],
              judgment_color(JudgmentKind::Miss, TapJudgment::Miss, HoldJudgment::Num), chip_y);
    draw_chip(state.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)],
              judgment_color(JudgmentKind::HoldOk, TapJudgment::Num, HoldJudgment::Ok), chip_y);
    draw_chip(state.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)],
              judgment_color(JudgmentKind::HoldNg, TapJudgment::Num, HoldJudgment::Ng), chip_y);

    // Bottom-centre: live grade.
    if (state.grade != nullptr) {
        const std::string grade_text = format_grade(*state.grade);
        draw_text(renderer, grade_text,
                  (width - text_width(grade_text, main_pixel)) * 0.5f,
                  height - 8.0f - 7.0f * main_pixel, main_pixel, kTextColor);
    }
}

void HudRenderer::render_life(double life, int screen_w, int screen_h,
                              GlQuadRenderer& renderer) const {
    if (screen_w <= 0 || screen_h <= 0) {
        return;
    }

    double clamped = life;
    if (clamped < 0.0) {
        clamped = 0.0;
    } else if (clamped > 1.0) {
        clamped = 1.0;
    }

    const float width = static_cast<float>(screen_w);
    const float height = static_cast<float>(screen_h);
    const float bar_w = std::min(width * 0.40f, 480.0f);
    const float bar_h = 16.0f;
    const float border = 2.0f;
    const float x = (width - bar_w) * 0.5f;
    // Sit above the bottom-centre grade text.
    const float y = height - 8.0f - 21.0f - 12.0f - bar_h;

    // Frame (drawn as a slightly larger backing quad), then the filled portion.
    renderer.draw_quad(Rect{x - border, y - border, bar_w + border * 2.0f, bar_h + border * 2.0f},
                       kLifeFrameColor);
    renderer.draw_quad(Rect{x, y, bar_w, bar_h}, kLifeBackColor);

    const Color fill =
        clamped < kLifeDangerThreshold ? kLifeDangerColor : kLifeFillColor;
    renderer.draw_quad(Rect{x, y, bar_w * static_cast<float>(clamped), bar_h}, fill);
}

} // namespace blaze4k
