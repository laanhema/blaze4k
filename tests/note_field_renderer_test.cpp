#include <cmath>
#include <cstdlib>
#include <iostream>

#include "gameplay/note_field_renderer.hpp"
#include "gameplay/noteskin.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::HoldLayout;
using blaze4k::layout_hold;

constexpr double S = blaze4k::NoteSkin::kNoteSize;                 // 96 px cap / note box
constexpr double I = S * blaze4k::kCelHoldBodyStopFromTail;         // 48 px body inset

bool near(double a, double b) {
    return std::abs(a - b) < 1e-5;
}

void test_upscroll_cel() {
    const HoldLayout l = layout_hold(100.0, 500.0, false, S, I, true);
    TEST_CHECK(near(l.body_end_y, 452.0));
    TEST_CHECK(l.has_body);
    TEST_CHECK(l.has_cap);
    TEST_CHECK(near(l.cap_near_y, 452.0));
    TEST_CHECK(near(l.cap_far_y, 548.0));
    TEST_CHECK(l.cap_v_near == 0.0f);
    TEST_CHECK(near((l.cap_near_y + l.cap_far_y) * 0.5, 500.0)); // centred on the tail
    std::cout << "  - up-scroll Cel cap centred on the tail ok.\n";
}

void test_downscroll_mirror() {
    const HoldLayout l = layout_hold(500.0, 100.0, true, S, I, true);
    TEST_CHECK(near(l.body_end_y, 148.0));
    TEST_CHECK(l.has_body);
    TEST_CHECK(l.has_cap);
    TEST_CHECK(near(l.cap_near_y, 148.0));
    TEST_CHECK(near(l.cap_far_y, 52.0));
    TEST_CHECK(l.cap_v_near == 0.0f);
    TEST_CHECK(near((l.cap_near_y + l.cap_far_y) * 0.5, 100.0));
    std::cout << "  - down-scroll mirror ok.\n";
}

void test_no_seam() {
    // Unclipped, the cap starts exactly where the body stops (same double).
    const HoldLayout up = layout_hold(100.0, 500.0, false, S, I, true);
    const HoldLayout down = layout_hold(500.0, 100.0, true, S, I, true);
    TEST_CHECK(up.cap_near_y == up.body_end_y);
    TEST_CHECK(down.cap_near_y == down.body_end_y);
    // Same for an odd, non-integer geometry.
    const HoldLayout odd = layout_hold(123.375, 777.8125, false, S, I, true);
    TEST_CHECK(odd.cap_near_y == odd.body_end_y);
    std::cout << "  - no body/cap seam ok.\n";
}

void test_visible_end_regression() {
    // The visible end overshoots the tail by half a note, not the old full note.
    const HoldLayout up = layout_hold(100.0, 500.0, false, S, I, true);
    const HoldLayout down = layout_hold(500.0, 100.0, true, S, I, true);
    TEST_CHECK(near(std::abs(up.cap_far_y - 500.0), S / 2.0));
    TEST_CHECK(near(std::abs(down.cap_far_y - 100.0), S / 2.0));
    TEST_CHECK(std::abs(up.cap_far_y - 500.0) < S);
    TEST_CHECK(std::abs(down.cap_far_y - 100.0) < S);
    TEST_CHECK(up.cap_far_y > 500.0);   // past the tail, away from the head
    TEST_CHECK(down.cap_far_y < 100.0);
    std::cout << "  - visible end half a note past the tail (regression) ok.\n";
}

void test_held_near_end_clip() {
    // Up-scroll: head clamped to the receptor at 400, tail 20 px further.
    const HoldLayout up = layout_hold(400.0, 420.0, false, S, I, true);
    TEST_CHECK(!up.has_body);
    TEST_CHECK(up.has_cap);
    TEST_CHECK(near(up.cap_near_y, 400.0));
    TEST_CHECK(near(up.cap_far_y, 468.0));
    TEST_CHECK(near(up.cap_v_near, (400.0 - 372.0) / S));

    const HoldLayout down = layout_hold(400.0, 380.0, true, S, I, true);
    TEST_CHECK(!down.has_body);
    TEST_CHECK(down.has_cap);
    TEST_CHECK(near(down.cap_near_y, 400.0));
    TEST_CHECK(near(down.cap_far_y, 332.0));
    TEST_CHECK(near(down.cap_v_near, (428.0 - 400.0) / S));
    std::cout << "  - held hold near its end clips the cap at the head ok.\n";
}

void test_exact_tail_time() {
    // head == tail == receptor: the remnant sits inside the receptor box on the
    // tail side for the scroll direction (needs the explicit `reverse`).
    const HoldLayout up = layout_hold(400.0, 400.0, false, S, I, true);
    TEST_CHECK(up.has_cap && !up.has_body);
    TEST_CHECK(near(up.cap_v_near, 0.5));
    TEST_CHECK(near(up.cap_near_y, 400.0));
    TEST_CHECK(std::abs(up.cap_far_y - 400.0) <= S / 2.0 + 1e-9);
    TEST_CHECK(up.cap_far_y > 400.0);

    const HoldLayout down = layout_hold(400.0, 400.0, true, S, I, true);
    TEST_CHECK(down.has_cap && !down.has_body);
    TEST_CHECK(near(down.cap_v_near, 0.5));
    TEST_CHECK(near(down.cap_near_y, 400.0));
    TEST_CHECK(std::abs(down.cap_far_y - 400.0) <= S / 2.0 + 1e-9);
    TEST_CHECK(down.cap_far_y < 400.0);
    std::cout << "  - exact tail time remnant inside the receptor box ok.\n";
}

void test_head_past_cap() {
    const HoldLayout up = layout_hold(600.0, 500.0, false, S, I, true);
    TEST_CHECK(!up.has_body);
    TEST_CHECK(!up.has_cap);
    const HoldLayout down = layout_hold(0.0, 100.0, true, S, I, true);
    TEST_CHECK(!down.has_body);
    TEST_CHECK(!down.has_cap);
    std::cout << "  - head past the whole cap draws nothing ok.\n";
}

void test_short_hold() {
    // Shorter than the inset: no body, cap clipped at the head centre.
    const HoldLayout l = layout_hold(100.0, 120.0, false, S, I, true);
    TEST_CHECK(!l.has_body);
    TEST_CHECK(l.has_cap);
    TEST_CHECK(near(l.cap_near_y, 100.0));
    TEST_CHECK(near(l.cap_far_y, 168.0));
    TEST_CHECK(near(l.cap_v_near, (100.0 - 72.0) / S));
    TEST_CHECK(l.cap_far_y > l.cap_near_y); // positive extent
    std::cout << "  - short hold ok.\n";
}

void test_fallback_without_cap() {
    // Procedural fallback: no cap art, the body runs exactly to the tail.
    const HoldLayout l = layout_hold(100.0, 500.0, false, S, 0.0, false);
    TEST_CHECK(l.body_end_y == 500.0);
    TEST_CHECK(l.has_body);
    TEST_CHECK(!l.has_cap);
    const HoldLayout down = layout_hold(500.0, 100.0, true, S, 0.0, false);
    TEST_CHECK(down.body_end_y == 100.0);
    TEST_CHECK(down.has_body && !down.has_cap);
    // Cap art but zero cap size: treated as cap-less, inset ignored.
    const HoldLayout zero = layout_hold(100.0, 500.0, false, 0.0, I, true);
    TEST_CHECK(!zero.has_cap);
    TEST_CHECK(zero.body_end_y == 500.0);
    // No cap art: the inset is ignored.
    const HoldLayout no_art = layout_hold(100.0, 500.0, false, S, I, false);
    TEST_CHECK(!no_art.has_cap);
    TEST_CHECK(no_art.body_end_y == 500.0);
    std::cout << "  - cap-less fallback body runs to the tail ok.\n";
}

void test_cap_uv_orientation() {
    // The quad's UVs run top to bottom on screen; the cap art is head-on-top.
    const HoldLayout up = layout_hold(100.0, 500.0, false, S, I, true);
    TEST_CHECK(up.cap_uv.u0 == 0.0f && up.cap_uv.u1 == 1.0f);
    TEST_CHECK(up.cap_uv.v0 == 0.0f && up.cap_uv.v1 == 1.0f);
    // Reverse: flipped vertically (v1 at the top edge, which is the tail side).
    const HoldLayout down = layout_hold(500.0, 100.0, true, S, I, true);
    TEST_CHECK(down.cap_uv.u0 == 0.0f && down.cap_uv.u1 == 1.0f);
    TEST_CHECK(down.cap_uv.v0 == 1.0f && down.cap_uv.v1 == 0.0f);
    // Clipped at the head: the head-side edge starts cap_v_near into the art.
    const HoldLayout up_clip = layout_hold(400.0, 420.0, false, S, I, true);
    TEST_CHECK(up_clip.cap_uv.v0 == up_clip.cap_v_near && up_clip.cap_uv.v1 == 1.0f);
    TEST_CHECK(up_clip.cap_v_near > 0.0f);
    const HoldLayout down_clip = layout_hold(400.0, 380.0, true, S, I, true);
    TEST_CHECK(down_clip.cap_uv.v0 == 1.0f && down_clip.cap_uv.v1 == down_clip.cap_v_near);
    TEST_CHECK(down_clip.cap_v_near > 0.0f);
    std::cout << "  - cap UV orientation in both directions ok.\n";
}

} // namespace

int main() {
    std::cout << "note_field_renderer_test\n";
    test_upscroll_cel();
    test_downscroll_mirror();
    test_no_seam();
    test_visible_end_regression();
    test_held_near_end_clip();
    test_exact_tail_time();
    test_head_past_cap();
    test_short_hold();
    test_fallback_without_cap();
    test_cap_uv_orientation();
    std::cout << "note_field_renderer_test passed\n";
    return 0;
}
