#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <numbers>

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

using blaze4k::NoteQuantization;

bool near(double a, double b) {
    return std::abs(a - b) < 1e-5;
}

void test_tap_frames() {
    // Beat 0: first frame of the 4th band (rows 0-1).
    const blaze4k::UVRect first = blaze4k::cel_tap_frame(NoteQuantization::Fourth, 0.0);
    TEST_CHECK(near(first.u0, 0.0) && near(first.v0, 0.0));
    TEST_CHECK(near(first.u1, 1.0 / 16.0) && near(first.v1, 1.0 / 16.0));

    // 32 frames over 2 beats: beat 1 is frame 16, the start of the band's 2nd row.
    const blaze4k::UVRect half = blaze4k::cel_tap_frame(NoteQuantization::Eighth, 1.0);
    TEST_CHECK(near(half.u0, 0.0) && near(half.v0, 3.0 / 16.0));

    // Last frame of the cycle, then wraps back to frame 0 at beat 2.
    const blaze4k::UVRect last = blaze4k::cel_tap_frame(NoteQuantization::Fourth, 1.999);
    TEST_CHECK(near(last.u0, 15.0 / 16.0) && near(last.v0, 1.0 / 16.0));
    const blaze4k::UVRect wrapped = blaze4k::cel_tap_frame(NoteQuantization::Fourth, 2.0);
    TEST_CHECK(near(wrapped.u0, 0.0) && near(wrapped.v0, 0.0));

    // Negative beats (lead-in) stay inside the band.
    const blaze4k::UVRect lead_in = blaze4k::cel_tap_frame(NoteQuantization::Fourth, -0.5);
    TEST_CHECK(lead_in.v0 >= 0.0f && lead_in.v1 <= 2.0f / 16.0f + 1e-6f);

    // Each quantization owns its own band; 192nd shares 64th (8 bands).
    const blaze4k::UVRect sixty_fourth = blaze4k::cel_tap_frame(NoteQuantization::SixtyFourth, 0.0);
    const blaze4k::UVRect finest = blaze4k::cel_tap_frame(NoteQuantization::OneNinetySecond, 0.0);
    TEST_CHECK(near(sixty_fourth.v0, 14.0 / 16.0));
    TEST_CHECK(near(finest.v0, sixty_fourth.v0));
    TEST_CHECK(near(blaze4k::cel_tap_frame(NoteQuantization::Sixteenth, 0.0).v0, 6.0 / 16.0));
    std::cout << "  - tap sheet frames and quantization bands ok.\n";
}

void test_rotations() {
    constexpr double kPi = std::numbers::pi;
    TEST_CHECK(near(blaze4k::column_rotation(0), kPi / 2.0));  // Left
    TEST_CHECK(near(blaze4k::column_rotation(1), 0.0));        // Down (as authored)
    TEST_CHECK(near(blaze4k::column_rotation(2), kPi));        // Up
    TEST_CHECK(near(blaze4k::column_rotation(3), -kPi / 2.0)); // Right
    TEST_CHECK(near(blaze4k::column_rotation(7), 0.0));

    // One turn per 4 seconds.
    TEST_CHECK(near(blaze4k::cel_mine_rotation(0.0), 0.0));
    TEST_CHECK(near(blaze4k::cel_mine_rotation(2.0), kPi));
    TEST_CHECK(near(blaze4k::cel_mine_rotation(5.0), kPi / 2.0));
    std::cout << "  - column and mine rotations ok.\n";
}

void test_receptor_and_mine_colors() {
    TEST_CHECK(near(blaze4k::cel_receptor_brightness(4.0), 1.0));   // flash on the beat
    TEST_CHECK(blaze4k::cel_receptor_brightness(4.1) < 1.0f);
    TEST_CHECK(blaze4k::cel_receptor_brightness(4.1) > 0.55f);
    TEST_CHECK(near(blaze4k::cel_receptor_brightness(4.5), 0.55));  // settled
    TEST_CHECK(near(blaze4k::cel_receptor_brightness(-0.5), 0.55)); // lead-in

    const blaze4k::Color red = blaze4k::cel_mine_core_color(3.0);
    const blaze4k::Color brown = blaze4k::cel_mine_core_color(3.6);
    TEST_CHECK(near(red.r, 1.0) && near(red.g, 0.0));
    TEST_CHECK(brown.r < red.r && brown.g > red.g);
    std::cout << "  - receptor pulse and mine core colors ok.\n";
}

void test_explosion_tween() {
    // Start: zoom 1.1, alpha 1.2 clamped to opaque.
    const blaze4k::ExplosionTween start = blaze4k::cel_explosion_tween(0.0, blaze4k::kCelTapExplosionSeconds);
    TEST_CHECK(near(start.zoom, 1.1) && near(start.alpha, 1.0));
    // accelerate eases in: halfway is a quarter of the way to the end state.
    const blaze4k::ExplosionTween half =
        blaze4k::cel_explosion_tween(blaze4k::kCelTapExplosionSeconds * 0.5, blaze4k::kCelTapExplosionSeconds);
    TEST_CHECK(near(half.zoom, 1.075) && near(half.alpha, 0.9));
    // Finished, not started, and zero-length tweens draw nothing.
    TEST_CHECK(blaze4k::cel_explosion_tween(0.15, 0.15).alpha == 0.0f);
    TEST_CHECK(blaze4k::cel_explosion_tween(-0.01, 0.15).alpha == 0.0f);
    TEST_CHECK(blaze4k::cel_explosion_tween(0.0, 0.0).alpha == 0.0f);
    // The held flash is shorter than the tap flash.
    TEST_CHECK(blaze4k::cel_explosion_tween(0.1, blaze4k::kCelHeldExplosionSeconds).alpha == 0.0f);
    TEST_CHECK(blaze4k::cel_explosion_tween(0.1, blaze4k::kCelTapExplosionSeconds).alpha > 0.0f);
    std::cout << "  - explosion tween ok.\n";
}

void test_mine_explosion_tween() {
    constexpr double kPi = std::numbers::pi;
    const blaze4k::ExplosionTween start = blaze4k::cel_mine_explosion_tween(0.0);
    TEST_CHECK(near(start.alpha, 1.0) && near(start.rotation, 0.0) && near(start.zoom, 1.0));
    // First 0.2 s: opaque while turning to 90 degrees.
    const blaze4k::ExplosionTween quarter = blaze4k::cel_mine_explosion_tween(0.2);
    TEST_CHECK(near(quarter.alpha, 1.0) && near(quarter.rotation, kPi / 2.0));
    // Second 0.2 s: turns on toward 180 while fading.
    const blaze4k::ExplosionTween fading = blaze4k::cel_mine_explosion_tween(0.3);
    TEST_CHECK(near(fading.alpha, 0.5) && near(fading.rotation, kPi * 0.75));
    TEST_CHECK(blaze4k::cel_mine_explosion_tween(blaze4k::kCelMineExplosionSeconds).alpha == 0.0f);
    TEST_CHECK(blaze4k::cel_mine_explosion_tween(-0.01).alpha == 0.0f);
    std::cout << "  - mine explosion tween ok.\n";
}

void test_headless_fallback() {
    // Without init (no GL) the skin serves the procedural fallback's sprites:
    // quantization-tinted heads and a stretched, cap-less body.
    const blaze4k::NoteSkin skin;
    TEST_CHECK(!skin.using_cel());
    const blaze4k::SkinSprite head =
        skin.head(blaze4k::NoteType::Tap, 0, NoteQuantization::Eighth, 0.0);
    TEST_CHECK(head.texture != nullptr);
    TEST_CHECK(head.tint.b > head.tint.r); // 8th = blue
    const blaze4k::HoldSprites hold = skin.hold(blaze4k::NoteType::HoldHead, false, NoteQuantization::Fourth);
    TEST_CHECK(hold.cap == nullptr && hold.tile_scale == 0.0f);
    TEST_CHECK(skin.mine(0.0)[1].texture == nullptr);
    // Explosions are Cel-only art.
    TEST_CHECK(skin.tap_explosion(0, blaze4k::TapJudgment::Fantastic, 0.0, 0.15).texture == nullptr);
    TEST_CHECK(skin.hold_explosion(0).texture == nullptr);
    TEST_CHECK(skin.mine_explosion(0.0).texture == nullptr);

    // Initializing without a GL context fails cleanly.
    blaze4k::NoteSkin headless;
    TEST_CHECK(!headless.init(std::filesystem::path("does-not-exist")));
    std::cout << "  - headless fallback sprites ok.\n";
}

} // namespace

int main() {
    std::cout << "noteskin_test\n";
    test_tap_frames();
    test_rotations();
    test_receptor_and_mine_colors();
    test_explosion_tween();
    test_mine_explosion_tween();
    test_headless_fallback();
    std::cout << "noteskin_test passed\n";
    return 0;
}
