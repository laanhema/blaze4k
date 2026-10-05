#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "gameplay/hud_renderer.hpp"
#include "gameplay/judgment_animator.hpp"
#include "gameplay/note_field.hpp"
#include "gameplay/noteskin.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::DiffBadgeLayout;
using blaze4k::JudgmentAnimator;
using blaze4k::LifeBarLayout;
using blaze4k::Rect;
using blaze4k::Vec2;
using blaze4k::layout_diff_badge;
using blaze4k::layout_life_bar;

constexpr double kGap = blaze4k::kLifeBarFieldGap;

bool near(double a, double b) {
    return std::abs(a - b) < 1e-3;
}

bool finite(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
}

bool same(const Rect& a, const Rect& b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.w, b.w) && near(a.h, b.h);
}

bool same(const Rect& a, float x, float y, float w, float h) {
    return same(a, Rect{x, y, w, h});
}

bool intersects(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// The gameplay field geometry, through the shared NoteField accessor.
double field_left_for(int screen_w) {
    blaze4k::NoteField field;
    blaze4k::NoteFieldConfig config;
    config.column_width = blaze4k::NoteSkin::kColumnWidth;
    field.set_config(config);
    return field.field_left(screen_w);
}

LifeBarLayout layout_at(double life, int w, int h) {
    return layout_life_bar(life, w, h, field_left_for(w));
}

DiffBadgeLayout badge_at(float text_w, int w, int h) {
    return layout_diff_badge(text_w, w, h, field_left_for(w));
}

struct Size {
    int w;
    int h;
};

constexpr Size kCommonSizes[] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3440, 1440},
                                 {640, 480},  {1024, 768},  {800, 600}};

void test_life_bar_matches_mock_720p() {
    const LifeBarLayout l = layout_at(1.0, 1280, 720);
    TEST_CHECK(l.visible);
    TEST_CHECK(!l.danger);
    TEST_CHECK(near(l.fraction, 1.0));
    TEST_CHECK(near(l.border, 4.0));
    TEST_CHECK(same(l.frame, 40, 120, 40, 480));
    TEST_CHECK(same(l.track, 44, 124, 32, 472));
    TEST_CHECK(same(l.fill, l.track));
    std::cout << "  - life bar matches the mock at 1280x720 ok.\n";
}

void test_life_bar_scales() {
    struct Row {
        Size size;
        Rect frame;
        Rect track;
    };
    const Row rows[] = {
        {{2560, 1440}, {80, 240, 80, 960}, {88, 248, 64, 944}},
        {{1920, 1080}, {60, 180, 60, 720}, {66, 186, 48, 708}},
        {{3440, 1440}, {520, 240, 80, 960}, {528, 248, 64, 944}},
        {{640, 480}, {20, 120, 20, 240}, {22, 122, 16, 236}},
        {{1024, 768}, {32, 192, 32, 384}, {35.2f, 195.2f, 25.6f, 377.6f}},
    };
    for (const Row& row : rows) {
        const LifeBarLayout l = layout_at(0.75, row.size.w, row.size.h);
        const blaze4k::theme::LayoutScale L = blaze4k::theme::layout_scale(row.size.w, row.size.h);
        TEST_CHECK(l.visible);
        TEST_CHECK(same(l.frame, row.frame));
        TEST_CHECK(same(l.track, row.track));
        TEST_CHECK(same(l.frame, L.rect(blaze4k::theme::layout::kLifeBar)));  // no clamp
    }
    std::cout << "  - life bar scales with the layout scale ok.\n";
}

void test_bottom_up_fill() {
    for (const double f : {0.5, 0.25, 0.9}) {
        const LifeBarLayout l = layout_at(f, 1280, 720);
        TEST_CHECK(near(l.fill.h, l.track.h * f));
        TEST_CHECK(near(l.fill.y + l.fill.h, l.track.y + l.track.h));
        TEST_CHECK(near(l.fill.x, l.track.x));
        TEST_CHECK(near(l.fill.w, l.track.w));
        const std::optional<Rect> cropped = blaze4k::fill_cropped_rect(l.track, l.fraction);
        TEST_CHECK(cropped.has_value());
        TEST_CHECK(same(l.fill, *cropped));
    }
    std::cout << "  - fill is bottom-anchored in the track and proportional ok.\n";
}

void test_clamp() {
    const LifeBarLayout low = layout_at(-0.5, 1280, 720);
    TEST_CHECK(low.visible);
    TEST_CHECK(low.fill.h == 0.0f);
    TEST_CHECK(near(low.fraction, 0.0));
    TEST_CHECK(near(low.fill.y, low.track.y + low.track.h));

    const LifeBarLayout nan = layout_at(std::nan(""), 1280, 720);
    TEST_CHECK(nan.visible);
    TEST_CHECK(nan.fill.h == 0.0f);
    TEST_CHECK(nan.danger);

    const LifeBarLayout high = layout_at(1.7, 1280, 720);
    TEST_CHECK(near(high.fraction, 1.0));
    TEST_CHECK(same(high.fill, high.track));
    std::cout << "  - life is clamped to [0,1] ok.\n";
}

void test_danger() {
    TEST_CHECK(layout_at(0.29, 1280, 720).danger);
    TEST_CHECK(!layout_at(0.3, 1280, 720).danger);
    TEST_CHECK(!layout_at(1.0, 1280, 720).danger);
    TEST_CHECK(layout_at(-1.0, 1280, 720).danger);
    std::cout << "  - danger is strict < 0.3 ok.\n";
}

void test_track_inside_frame() {
    for (const Size s : kCommonSizes) {
        const LifeBarLayout l = layout_at(0.6, s.w, s.h);
        const float b = blaze4k::theme::layout_scale(s.w, s.h).px(4.0f);
        TEST_CHECK(near(l.border, b));
        TEST_CHECK(near(l.track.x, l.frame.x + b));
        TEST_CHECK(near(l.track.y, l.frame.y + b));
        TEST_CHECK(near(l.track.w, l.frame.w - 2.0 * b));
        TEST_CHECK(near(l.track.h, l.frame.h - 2.0 * b));
    }
    std::cout << "  - track is the frame inset by the 4 px chrome border ok.\n";
}

void test_no_field_overlap_common_sizes() {
    for (const Size s : kCommonSizes) {
        const double field_left = field_left_for(s.w);
        const LifeBarLayout l = layout_at(1.0, s.w, s.h);
        TEST_CHECK(l.visible);
        TEST_CHECK(l.frame.x + l.frame.w + kGap <= field_left);
        TEST_CHECK(!intersects(l.frame, Rect{static_cast<float>(field_left), 0.0f,
                                             static_cast<float>(4 * blaze4k::NoteSkin::kColumnWidth),
                                             static_cast<float>(s.h)}));
        const DiffBadgeLayout b = badge_at(82.0f, s.w, s.h);
        TEST_CHECK(b.visible);
        TEST_CHECK(b.plate.x + b.plate.w + kGap <= field_left);
    }
    std::cout << "  - no note-field overlap at common sizes ok.\n";
}

void test_below_diff_badge() {
    for (const Size s : kCommonSizes) {
        const DiffBadgeLayout b = badge_at(82.0f, s.w, s.h);
        TEST_CHECK(layout_at(1.0, s.w, s.h).frame.y >= b.plate.y + b.plate.h);
    }
    const DiffBadgeLayout tiny = badge_at(82.0f, 320, 240);
    TEST_CHECK(layout_at(1.0, 320, 240).frame.y >= tiny.plate.y + tiny.plate.h);
    std::cout << "  - life frame stays below the difficulty badge ok.\n";
}

void test_judgment_pop_clear() {
    // Peak pop scale over the whole pop curve.
    const double d = JudgmentAnimator::kJudgmentPopSeconds;
    float peak = 0.0f;
    for (int i = 0; i <= 1000; ++i) {
        peak = std::max(peak, JudgmentAnimator::pop_scale(d * i / 1000.0, d));
    }
    TEST_CHECK(peak > 1.0f);

    for (const Size s : {Size{640, 480}, Size{500, 400}, Size{400, 400}, Size{1280, 720}}) {
        const blaze4k::theme::LayoutScale L = blaze4k::theme::layout_scale(s.w, s.h);
        const Vec2 ref = JudgmentAnimator::kJudgmentContentRef;
        const Rect pop =
            JudgmentAnimator::judgment_pop_rect(L, Vec2{ref.x * L.s, ref.y * L.s}, peak);
        const double field_left = field_left_for(s.w);
        TEST_CHECK(near(pop.x + pop.w * 0.5, s.w * 0.5));
        TEST_CHECK(near(pop.x + pop.w * 0.5, field_left + 216.0));
        const LifeBarLayout l = layout_at(1.0, s.w, s.h);
        TEST_CHECK(l.frame.x + l.frame.w + kGap <= pop.x);
    }
    std::cout << "  - judgment sprite at peak scale is centred on the field and clears the bar ok.\n";
}

void test_narrow_clamp() {
    // 500 px: field_left 34 -> the frame slides left to keep the 16 px gap (no shrink).
    const double fl500 = field_left_for(500);
    TEST_CHECK(near(fl500, 34.0));
    const LifeBarLayout n = layout_at(1.0, 500, 400);
    TEST_CHECK(n.visible);
    TEST_CHECK(near(n.frame.x, 2.375));
    TEST_CHECK(near(n.frame.w, 15.625));
    TEST_CHECK(n.frame.x + n.frame.w <= 18.0 + 1e-3);
    TEST_CHECK(near(n.border, 1.5625));
    TEST_CHECK(finite(n.frame) && finite(n.track) && finite(n.fill));

    // 400 px: field_left -16 -> no room at all; best effort at the minimum width.
    const LifeBarLayout t = layout_at(0.5, 400, 400);
    TEST_CHECK(near(field_left_for(400), -16.0));
    TEST_CHECK(t.visible);
    TEST_CHECK(near(t.frame.x, 0.0));
    TEST_CHECK(near(t.frame.w, 8.5));
    TEST_CHECK(near(t.track.w, 6.0));
    TEST_CHECK(finite(t.frame) && finite(t.track) && finite(t.fill));
    TEST_CHECK(t.track.h > 0.0f && t.fill.h >= 0.0f && t.frame.w > 0.0f && t.frame.h > 0.0f);
    std::cout << "  - very narrow windows slide/shrink the bar ok.\n";
}

void test_diff_badge_layout() {
    const DiffBadgeLayout hard = badge_at(82.0f, 1280, 720);
    TEST_CHECK(hard.visible);
    TEST_CHECK(same(hard.plate, 36, 28, 130, 40));
    TEST_CHECK(near(hard.text_x, 54.0));
    TEST_CHECK(near(hard.text_max_w, 94.0));

    const DiffBadgeLayout wide = badge_at(200.0f, 1280, 720);
    TEST_CHECK(near(wide.plate.w, 236.0));
    TEST_CHECK(near(wide.text_max_w, 200.0));

    const DiffBadgeLayout capped = badge_at(400.0f, 1280, 720);
    TEST_CHECK(near(capped.plate.w, 300.0));
    TEST_CHECK(near(capped.text_max_w, 264.0));

    const DiffBadgeLayout big = badge_at(164.0f, 2560, 1440);
    TEST_CHECK(big.visible);
    TEST_CHECK(same(big.plate, 72, 56, 260, 80));
    TEST_CHECK(near(big.text_x, 108.0));

    TEST_CHECK(!badge_at(82.0f, 500, 400).visible);
    TEST_CHECK(!layout_diff_badge(82.0f, 0, 720, 0.0).visible);
    TEST_CHECK(!layout_diff_badge(82.0f, 1280, -1, 424.0).visible);
    std::cout << "  - difficulty badge layout (mock, growth, cap, scale, hide) ok.\n";
}

void test_combo_line_layout() {
    const float number_w = 46.0f;
    const float label_w = 70.0f;
    const float number_ascent = 30.0f;
    const float label_ascent = 20.0f;
    for (const Size s : {Size{1280, 720}, Size{3440, 1440}}) {
        const blaze4k::theme::LayoutScale L = blaze4k::theme::layout_scale(s.w, s.h);
        const auto c = JudgmentAnimator::combo_line_layout(L, number_w * L.s, label_w * L.s,
                                                           number_ascent * L.s, label_ascent * L.s);
        const double total = (number_w + JudgmentAnimator::kComboGap + label_w) * L.s;
        TEST_CHECK(near(c.number.x + total * 0.5, s.w * 0.5));
        TEST_CHECK(near(c.number.y, L.y(368.0f)));
        TEST_CHECK(near(c.label.x, c.number.x + (number_w + JudgmentAnimator::kComboGap) * L.s));
        TEST_CHECK(near(c.label.y + label_ascent * L.s, c.number.y + number_ascent * L.s));
    }
    const auto c720 = JudgmentAnimator::combo_line_layout(blaze4k::theme::layout_scale(1280, 720),
                                                          number_w, label_w, number_ascent,
                                                          label_ascent);
    TEST_CHECK(near(c720.number.x, 579.0) && near(c720.label.x, 631.0) &&
               near(c720.label.y, 378.0));
    std::cout << "  - combo line is centred on the field and shares a baseline ok.\n";
}

void test_degenerate() {
    TEST_CHECK(!layout_life_bar(1.0, 0, 720, 0.0).visible);
    TEST_CHECK(!layout_life_bar(1.0, 1280, 0, 424.0).visible);
    TEST_CHECK(!layout_life_bar(1.0, -1, -1, 0.0).visible);
    std::cout << "  - non-positive screen sizes are invisible ok.\n";
}

} // namespace

int main() {
    std::cout << "[hud_renderer_test] Starting Cabinet HUD layout tests...\n";
    test_life_bar_matches_mock_720p();
    test_life_bar_scales();
    test_bottom_up_fill();
    test_clamp();
    test_danger();
    test_track_inside_frame();
    test_no_field_overlap_common_sizes();
    test_below_diff_badge();
    test_judgment_pop_clear();
    test_narrow_clamp();
    test_diff_badge_layout();
    test_combo_line_layout();
    test_degenerate();
    std::cout << "[hud_renderer_test] All Cabinet HUD layout tests passed successfully!\n";
    return 0;
}
