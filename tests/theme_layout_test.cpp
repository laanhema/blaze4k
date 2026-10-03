// #91: the shared 1280x720 -> window layout-scale helper (render/theme_layout.hpp).
// Owner decision: fit the whole layout, s = min(h / 720, w / 1280), with the content column
// centred on both axes (bands at the sides on 21:9, above/below on 16:10; never cropped).

#include <climits>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/ttf_font.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace theme = blaze4k::theme;
namespace layout = blaze4k::theme::layout;
using blaze4k::Rect;
using blaze4k::Vec2;

// Compile-time proof that the helper is constexpr and exact at 16:9 integer scales.
static_assert(theme::layout_scale(1280, 720).s == 1.0f);
static_assert(theme::layout_scale(1280, 720).origin.x == 0.0f &&
              theme::layout_scale(1280, 720).origin.y == 0.0f);
static_assert(theme::layout_scale(2560, 1440).s == 2.0f);
static_assert(theme::layout_scale(0, 0).s == 1.0f);
static_assert(theme::layout_scale_factor(-5, 720) == 1.0f);
static_assert(theme::layout_scale_factor(1280, -5) == 1.0f);
// 16:10 is width-limited: 1920x1200 -> s = 1.5 with a 60 px band above and below.
static_assert(theme::layout_scale(1920, 1200).s == 1.5f &&
              theme::layout_scale(1920, 1200).origin.y == 60.0f);

namespace {

bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

bool rect_eq(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool rect_approx(const Rect& a, const Rect& b, float eps = 1e-3f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.w, b.w, eps) &&
           approx(a.h, b.h, eps);
}

bool rect_finite(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
}

bool is_identity(const theme::LayoutScale& l) {
    return l.s == 1.0f && l.origin.x == 0.0f && l.origin.y == 0.0f;
}

struct Size {
    int w;
    int h;
};

// Every valid size the tests sweep: 16:9, 21:9, 16:10, 4:3 and a not-quite-16:9 laptop size.
constexpr Size kSizes[] = {{1280, 720},  {2560, 1440}, {1920, 1080}, {3840, 2160},
                           {3440, 1440}, {2560, 1080}, {1920, 1200}, {1280, 800},
                           {1024, 768},  {1366, 768},  {640, 360},   {7680, 4320}};

void test_identity_720p() {
    const theme::LayoutScale l = theme::layout_scale(1280, 720);
    TEST_CHECK(is_identity(l));
    for (const Rect& r : {layout::kBanner, layout::kLifeBar, layout::kMedallion,
                          layout::kRecordRibbon}) {
        TEST_CHECK(rect_eq(l.rect(r), r));
    }
    const Vec2 p = l.point(Vec2{layout::kInfoX, layout::kSongTitleTop});
    TEST_CHECK(p.x == layout::kInfoX && p.y == layout::kSongTitleTop);
    TEST_CHECK(l.px(44.0f) == 44.0f);
    TEST_CHECK(rect_eq(l.column(), Rect{0.0f, 0.0f, 1280.0f, 720.0f}));
    std::cout << "  - 1280x720 is the exact identity ok.\n";
}

void test_2x_1440p() {
    const theme::LayoutScale l = theme::layout_scale(2560, 1440);
    TEST_CHECK(l.s == 2.0f && l.origin.x == 0.0f && l.origin.y == 0.0f);
    TEST_CHECK(rect_eq(l.rect(layout::kBanner), Rect{96.0f, 200.0f, 1120.0f, 314.0f}));
    TEST_CHECK(l.px(layout::kTopBarHeight) == 128.0f);
    TEST_CHECK(rect_eq(l.column(), Rect{0.0f, 0.0f, 2560.0f, 1440.0f}));
    std::cout << "  - 2560x1440 is an exact 2x ok.\n";
}

void test_1080p() {
    const theme::LayoutScale l = theme::layout_scale(1920, 1080);
    TEST_CHECK(l.s == 1.5f && l.origin.x == 0.0f && l.origin.y == 0.0f);
    TEST_CHECK(rect_eq(l.column(), Rect{0.0f, 0.0f, 1920.0f, 1080.0f}));
    std::cout << "  - 1920x1080 is 1.5x with no bands ok.\n";
}

void test_ultrawide_21_9() {
    // Height-limited: the column is centred horizontally with equal side bands.
    const theme::LayoutScale a = theme::layout_scale(3440, 1440);
    TEST_CHECK(a.s == 2.0f && a.origin.x == 440.0f && a.origin.y == 0.0f);
    TEST_CHECK(rect_eq(a.column(), Rect{440.0f, 0.0f, 2560.0f, 1440.0f}));
    TEST_CHECK(a.column().x * 2.0f + a.column().w == 3440.0f);
    TEST_CHECK(a.rect(layout::kBanner).x == 440.0f + 96.0f);

    const theme::LayoutScale b = theme::layout_scale(2560, 1080);
    TEST_CHECK(b.s == 1.5f && b.origin.x == 320.0f && b.origin.y == 0.0f);
    TEST_CHECK(rect_eq(b.column(), Rect{320.0f, 0.0f, 1920.0f, 1080.0f}));
    TEST_CHECK(b.column().x * 2.0f + b.column().w == 2560.0f);
    std::cout << "  - 21:9 centres the column with side bands ok.\n";
}

void test_16_10() {
    // Width-limited: the whole layout fits, with equal bands above and below; nothing cropped.
    const theme::LayoutScale a = theme::layout_scale(1920, 1200);
    TEST_CHECK(a.s == 1.5f && a.origin.x == 0.0f && a.origin.y == 60.0f);
    TEST_CHECK(rect_eq(a.column(), Rect{0.0f, 60.0f, 1920.0f, 1080.0f}));
    TEST_CHECK(a.column().y * 2.0f + a.column().h == 1200.0f);
    // Left-edge chrome stays on screen (it would be cropped with a height-only s).
    TEST_CHECK(a.x(layout::kLifeBar.x) == 60.0f);
    TEST_CHECK(rect_eq(a.rect(layout::kBanner), Rect{72.0f, 210.0f, 840.0f, 235.5f}));

    const theme::LayoutScale b = theme::layout_scale(1280, 800); // Steam Deck
    TEST_CHECK(b.s == 1.0f && b.origin.x == 0.0f && b.origin.y == 40.0f);
    TEST_CHECK(rect_eq(b.column(), Rect{0.0f, 40.0f, 1280.0f, 720.0f}));
    TEST_CHECK(rect_eq(b.rect(layout::kBanner), Rect{48.0f, 140.0f, 560.0f, 157.0f}));

    // 4:3 for good measure.
    const theme::LayoutScale c = theme::layout_scale(1024, 768);
    TEST_CHECK(c.s == 0.8f && c.origin.x == 0.0f && approx(c.origin.y, 96.0f));
    std::cout << "  - 16:10 and 4:3 fit with bands above and below ok.\n";
}

void test_column_fits_and_fills() {
    for (const Size& sz : kSizes) {
        const theme::LayoutScale l = theme::layout_scale(sz.w, sz.h);
        const Rect c = l.column();
        const float w = static_cast<float>(sz.w);
        const float h = static_cast<float>(sz.h);
        // Never cropped: the column lies inside the window...
        TEST_CHECK(c.x >= -1e-3f && c.y >= -1e-3f);
        TEST_CHECK(c.x + c.w <= w + 1e-2f && c.y + c.h <= h + 1e-2f);
        // ...and fills it along the limiting axis.
        TEST_CHECK(approx(c.w, w, 1e-2f) || approx(c.h, h, 1e-2f));
        // Centred on both axes.
        TEST_CHECK(approx(2.0f * c.x + c.w, w, 1e-2f));
        TEST_CHECK(approx(2.0f * c.y + c.h, h, 1e-2f));
        TEST_CHECK(l.s == theme::layout_scale_factor(sz.w, sz.h));
    }
    std::cout << "  - every size fits, fills one axis and is centred ok.\n";
}

void test_field_centre_matches_note_field() {
    // NoteField::field_left(w) + field_width() / 2 == w / 2 (note_field.hpp:69-71), so reference
    // x = 640 must land at w / 2. Likewise reference y = 360 lands at h / 2.
    for (const Size& sz : kSizes) {
        const theme::LayoutScale l = theme::layout_scale(sz.w, sz.h);
        TEST_CHECK(approx(l.x(layout::kRefWidth / 2.0f), static_cast<float>(sz.w) / 2.0f, 1e-2f));
        TEST_CHECK(approx(l.y(layout::kRefHeight / 2.0f), static_cast<float>(sz.h) / 2.0f, 1e-2f));
    }
    std::cout << "  - reference centre maps to the window (note-field) centre ok.\n";
}

void test_degenerate() {
    constexpr Size kDegenerate[] = {{0, 0},     {0, 720},  {1280, 0},          {-1, 720},
                                    {1280, -1}, {INT_MIN, INT_MIN}, {-1280, -720},
                                    {INT_MAX, 0}, {0, INT_MAX}};
    for (const Size& sz : kDegenerate) {
        const theme::LayoutScale l = theme::layout_scale(sz.w, sz.h);
        TEST_CHECK(is_identity(l));
        TEST_CHECK(theme::layout_scale_factor(sz.w, sz.h) == 1.0f);
        TEST_CHECK(rect_finite(l.rect(layout::kBanner)));
        TEST_CHECK(rect_finite(l.column()));
        TEST_CHECK(rect_eq(l.column(), Rect{0.0f, 0.0f, 1280.0f, 720.0f}));
    }
    TEST_CHECK(theme::layout_scale_factor(0, 0) == 1.0f);
    TEST_CHECK(theme::layout_scale_factor(INT_MIN, INT_MIN) == 1.0f);
    std::cout << "  - degenerate sizes give the identity ok.\n";
}

void test_large_sizes() {
    const theme::LayoutScale big = theme::layout_scale(INT_MAX, INT_MAX);
    TEST_CHECK(std::isfinite(big.s) && std::isfinite(big.origin.x) && std::isfinite(big.origin.y));
    TEST_CHECK(rect_finite(big.rect(layout::kBanner)));
    TEST_CHECK(rect_finite(big.column()));
    const theme::LayoutScale wide = theme::layout_scale(INT_MAX, 720);
    TEST_CHECK(wide.s == 1.0f && wide.origin.y == 0.0f && std::isfinite(wide.origin.x));
    const theme::LayoutScale tall = theme::layout_scale(1280, INT_MAX);
    TEST_CHECK(tall.s == 1.0f && tall.origin.x == 0.0f && std::isfinite(tall.origin.y));

    const theme::LayoutScale l8k = theme::layout_scale(7680, 4320);
    TEST_CHECK(l8k.s == 6.0f && l8k.origin.x == 0.0f && l8k.origin.y == 0.0f);
    TEST_CHECK(rect_approx(l8k.rect(layout::kBanner), Rect{288.0f, 600.0f, 3360.0f, 942.0f}));
    std::cout << "  - huge sizes stay finite (8K is 6x) ok.\n";
}

void test_text_scale_agrees() {
    // One definition of s: text atlases (#90) bake at the same s as placement.
    constexpr Size kTextSizes[] = {{-5, 720},   {1280, -5},  {0, 0},      {1, 1},
                                   {1280, 720}, {1920, 1080}, {1920, 1200}, {1280, 800},
                                   {2560, 1440}, {3440, 1440}, {2560, 1080}, {3840, 2160}};
    for (const Size& sz : kTextSizes) {
        TEST_CHECK(blaze4k::text_layout_scale(sz.w, sz.h) ==
                   theme::layout_scale_factor(sz.w, sz.h));
        TEST_CHECK(blaze4k::text_layout_scale(sz.w, sz.h) == theme::layout_scale(sz.w, sz.h).s);
    }
    std::cout << "  - text_layout_scale agrees with layout_scale ok.\n";
}

} // namespace

int main() {
    std::cout << "[theme_layout_test] Running layout scale tests...\n";
    test_identity_720p();
    test_2x_1440p();
    test_1080p();
    test_ultrawide_21_9();
    test_16_10();
    test_column_fits_and_fills();
    test_field_centre_matches_note_field();
    test_degenerate();
    test_large_sizes();
    test_text_scale_agrees();
    std::cout << "[theme_layout_test] All tests passed!\n";
    return 0;
}
