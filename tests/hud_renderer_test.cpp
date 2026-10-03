#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "gameplay/hud_renderer.hpp"
#include "gameplay/judgment_animator.hpp"
#include "gameplay/note_field.hpp"
#include "gameplay/noteskin.hpp"
#include "render/bitmap_font.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::LifeBarLayout;
using blaze4k::Rect;
using blaze4k::layout_life_bar;

constexpr double kGap = blaze4k::kLifeBarFieldGap;

// Bottom edge of the top-left percent text, from the HUD's own layout.
double percent_bottom() {
    const Rect r = blaze4k::percent_text_rect("100.00%");
    return static_cast<double>(r.y + r.h);
}

bool near(double a, double b) {
    return std::abs(a - b) < 1e-3;
}

bool finite(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
}

bool same(const Rect& a, const Rect& b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.w, b.w) && near(a.h, b.h);
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

struct Size {
    int w;
    int h;
};

constexpr Size kCommonSizes[] = {{1280, 720}, {1920, 1080}, {640, 480}, {1024, 768}, {800, 600}};

void test_vertical_left_full() {
    const LifeBarLayout l = layout_at(1.0, 1280, 720);
    TEST_CHECK(l.visible);
    TEST_CHECK(l.back.h > l.back.w);
    TEST_CHECK(l.frame.x >= 0.0f);
    TEST_CHECK(l.frame.x + l.frame.w <= 1280.0f * 0.5f);
    TEST_CHECK(same(l.fill, l.back));
    std::cout << "  - vertical bar on the left, full at life 1.0 ok.\n";
}

void test_bottom_up_fill() {
    const LifeBarLayout half = layout_at(0.5, 1280, 720);
    TEST_CHECK(near(half.fill.h, half.back.h * 0.5));
    TEST_CHECK(near(half.fill.y + half.fill.h, half.back.y + half.back.h));
    TEST_CHECK(near(half.fill.x, half.back.x));
    TEST_CHECK(near(half.fill.w, half.back.w));

    const LifeBarLayout quarter = layout_at(0.25, 1280, 720);
    TEST_CHECK(near(quarter.fill.y, quarter.back.y + quarter.back.h * 0.75));
    TEST_CHECK(near(quarter.fill.y + quarter.fill.h, quarter.back.y + quarter.back.h));
    std::cout << "  - fill is bottom-anchored and proportional ok.\n";
}

void test_clamp() {
    const LifeBarLayout low = layout_at(-0.5, 1280, 720);
    TEST_CHECK(low.visible);
    TEST_CHECK(low.fill.h == 0.0f);
    TEST_CHECK(near(low.fill.y, low.back.y + low.back.h));

    const LifeBarLayout high = layout_at(1.7, 1280, 720);
    TEST_CHECK(near(high.fill.h, high.back.h));
    TEST_CHECK(same(high.fill, high.back));
    std::cout << "  - life is clamped to [0,1] ok.\n";
}

void test_danger() {
    TEST_CHECK(layout_at(0.29, 1280, 720).danger);
    TEST_CHECK(!layout_at(0.3, 1280, 720).danger);
    TEST_CHECK(!layout_at(1.0, 1280, 720).danger);
    TEST_CHECK(layout_at(-1.0, 1280, 720).danger);
    std::cout << "  - danger is strict < 0.3 ok.\n";
}

void test_frame_wraps_back() {
    for (const Size s : kCommonSizes) {
        const LifeBarLayout l = layout_at(0.6, s.w, s.h);
        TEST_CHECK(near(l.frame.x, l.back.x - 2.0));
        TEST_CHECK(near(l.frame.y, l.back.y - 2.0));
        TEST_CHECK(near(l.frame.w, l.back.w + 4.0));
        TEST_CHECK(near(l.frame.h, l.back.h + 4.0));
    }
    std::cout << "  - frame is the back grown by 2 px on every side ok.\n";
}

void test_no_field_overlap_common_sizes() {
    for (const Size s : kCommonSizes) {
        const double field_left = field_left_for(s.w);
        const LifeBarLayout l = layout_at(1.0, s.w, s.h);
        TEST_CHECK(l.visible);
        TEST_CHECK(l.frame.x + l.frame.w + kGap <= field_left);
        TEST_CHECK(near(l.back.w, 16.0));  // no shrinking needed
        TEST_CHECK(near(l.back.x, 24.0));  // fixed left margin
        // The whole field (receptors, arrows, and the judgment pop, asserted in
        // test_judgment_pop_clear) is right of field_left, so the bar cannot overlap it.
        TEST_CHECK(!intersects(l.frame, Rect{static_cast<float>(field_left), 0.0f,
                                             static_cast<float>(4 * blaze4k::NoteSkin::kColumnWidth),
                                             static_cast<float>(s.h)}));
    }
    std::cout << "  - no note-field overlap at common sizes ok.\n";
}

void test_below_percent_text() {
    const double bottom = percent_bottom();
    for (const Size s : kCommonSizes) {
        TEST_CHECK(layout_at(1.0, s.w, s.h).frame.y >= bottom);
    }
    TEST_CHECK(layout_at(1.0, 320, 240).frame.y >= bottom);
    std::cout << "  - bar stays below the top-left percent text ok.\n";
}

void test_judgment_pop_clear() {
    using blaze4k::JudgmentAnimator;
    using blaze4k::JudgmentEvent;
    using blaze4k::JudgmentKind;
    using blaze4k::TapJudgment;

    // Peak pop scale over the whole pop curve.
    const double d = JudgmentAnimator::kJudgmentPopSeconds;
    float peak = 0.0f;
    for (int i = 0; i <= 1000; ++i) {
        peak = std::max(peak, JudgmentAnimator::pop_scale(d * i / 1000.0, d));
    }
    TEST_CHECK(peak >= 1.0f);
    const float pixel = JudgmentAnimator::kJudgmentPopPixel * peak;

    // Widest judgment label the animator can show, at peak scale.
    float pop_w = 0.0f;
    for (int k = 0; k <= static_cast<int>(JudgmentKind::RollHit); ++k) {
        for (int win = 0; win <= static_cast<int>(TapJudgment::Num); ++win) {
            JudgmentEvent e;
            e.kind = static_cast<JudgmentKind>(k);
            e.window = static_cast<TapJudgment>(win);
            pop_w = std::max(pop_w, blaze4k::text_width(JudgmentAnimator::judgment_label(e), pixel));
        }
    }
    TEST_CHECK(pop_w > 0.0f);

    // Centred on the screen (as JudgmentAnimator::render draws it) at 640x480, the
    // widest pop stays inside the field rect and so clear of the bar.
    const float field_left = static_cast<float>(field_left_for(640));
    const float field_w = static_cast<float>(4 * blaze4k::NoteSkin::kColumnWidth);
    const float pop_left = (640.0f - pop_w) * 0.5f;
    TEST_CHECK(pop_left >= field_left);
    TEST_CHECK(pop_left + pop_w <= field_left + field_w);
    const LifeBarLayout l = layout_at(1.0, 640, 480);
    TEST_CHECK(l.frame.x + l.frame.w + kGap <= pop_left);
    std::cout << "  - widest judgment pop at peak scale clears the bar ok.\n";
}

void test_narrow_clamp() {
    // 500 px: field_left 34 -> slide left and shrink to keep the 16 px gap.
    const double fl500 = field_left_for(500);
    TEST_CHECK(near(fl500, 34.0));
    const LifeBarLayout n = layout_at(1.0, 500, 400);
    TEST_CHECK(n.visible);
    TEST_CHECK(n.frame.x + n.frame.w + kGap <= fl500 + 1e-3);
    TEST_CHECK(n.back.w >= 6.0f);
    TEST_CHECK(n.back.w < 16.0f);
    TEST_CHECK(n.frame.x >= 0.0f);

    // 400 px: field_left -16 -> no room at all; best effort at the minimum width.
    const LifeBarLayout t = layout_at(0.5, 400, 400);
    TEST_CHECK(near(field_left_for(400), -16.0));
    TEST_CHECK(t.visible);
    TEST_CHECK(near(t.back.w, 6.0));
    TEST_CHECK(near(t.back.x, 2.0));
    TEST_CHECK(t.frame.x >= 0.0f);
    TEST_CHECK(finite(t.frame) && finite(t.back) && finite(t.fill));
    TEST_CHECK(t.back.h > 0.0f && t.fill.h >= 0.0f && t.frame.w > 0.0f && t.frame.h > 0.0f);
    std::cout << "  - very narrow windows slide/shrink the bar ok.\n";
}

void test_degenerate() {
    TEST_CHECK(!layout_life_bar(1.0, 0, 720, 0.0).visible);
    TEST_CHECK(!layout_life_bar(1.0, 1280, 0, 424.0).visible);
    TEST_CHECK(!layout_life_bar(1.0, -1, -1, 0.0).visible);
    std::cout << "  - non-positive screen sizes are invisible ok.\n";
}

void test_height_scales() {
    const LifeBarLayout l720 = layout_at(1.0, 1280, 720);
    const LifeBarLayout l1080 = layout_at(1.0, 1920, 1080);
    TEST_CHECK(near(l720.back.h, 432.0));
    TEST_CHECK(near(l1080.back.h, 648.0));
    TEST_CHECK(near(l720.back.y, 720.0 - (l720.back.y + l720.back.h)));
    TEST_CHECK(near(l1080.back.y, 1080.0 - (l1080.back.y + l1080.back.h)));
    std::cout << "  - bar height scales with screen height, centred ok.\n";
}

} // namespace

int main() {
    std::cout << "[hud_renderer_test] Starting life bar layout tests...\n";
    test_vertical_left_full();
    test_bottom_up_fill();
    test_clamp();
    test_danger();
    test_frame_wraps_back();
    test_no_field_overlap_common_sizes();
    test_below_percent_text();
    test_judgment_pop_clear();
    test_narrow_clamp();
    test_degenerate();
    test_height_scales();
    std::cout << "[hud_renderer_test] All life bar layout tests passed successfully!\n";
    return 0;
}
