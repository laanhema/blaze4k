#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "gameplay/judgment_animator.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::HoldJudgment;
using blaze4k::JudgmentAnimator;
using blaze4k::JudgmentEvent;
using blaze4k::JudgmentKind;
using blaze4k::TapJudgment;

namespace fs = std::filesystem;

const fs::path kSourceDir{BLAZE4K_SOURCE_DIR};
const fs::path kCabinet = fs::path{BLAZE4K_ASSETS_DIR} / "theme" / "cabinet";

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

JudgmentEvent make_event(JudgmentKind kind, TapJudgment window = TapJudgment::Num,
                         HoldJudgment hold = HoldJudgment::Num) {
    JudgmentEvent event;
    event.kind = kind;
    event.window = window;
    event.hold = hold;
    return event;
}

std::string label(JudgmentKind kind, TapJudgment window = TapJudgment::Num,
                  HoldJudgment hold = HoldJudgment::Num) {
    return JudgmentAnimator::judgment_label(make_event(kind, window, hold));
}

void test_label_mapping() {
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Fantastic) == "FANTASTIC");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Excellent) == "EXCELLENT");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Great) == "GREAT");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Decent) == "DECENT");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::WayOff) == "WAY OFF");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Miss) == "MISS");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::HitMine) == "MINE");
    TEST_CHECK(label(JudgmentKind::Tap, TapJudgment::Num) == "");

    TEST_CHECK(label(JudgmentKind::Miss) == "MISS");
    TEST_CHECK(label(JudgmentKind::HitMine) == "MINE");
    TEST_CHECK(label(JudgmentKind::HoldOk) == "OK");
    TEST_CHECK(label(JudgmentKind::RollOk) == "OK");
    TEST_CHECK(label(JudgmentKind::HoldNg) == "NG");
    TEST_CHECK(label(JudgmentKind::RollNg) == "NG");
    TEST_CHECK(label(JudgmentKind::AvoidedMine) == "");
    TEST_CHECK(label(JudgmentKind::RollHit) == "");
    std::cout << "  - judgment label mapping ok.\n";
}

std::string_view sprite(JudgmentKind kind, TapJudgment window = TapJudgment::Num) {
    return JudgmentAnimator::judgment_sprite(make_event(kind, window));
}

void test_sprite_mapping() {
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Fantastic) == "judgment_fantastic");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Excellent) == "judgment_excellent");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Great) == "judgment_great");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Decent) == "judgment_decent");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::WayOff) == "judgment_wayoff");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Miss) == "judgment_miss");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::HitMine) == "judgment_mine");
    TEST_CHECK(sprite(JudgmentKind::Miss) == "judgment_miss");
    TEST_CHECK(sprite(JudgmentKind::HitMine) == "judgment_mine");
    TEST_CHECK(sprite(JudgmentKind::HoldOk) == "judgment_ok");
    TEST_CHECK(sprite(JudgmentKind::RollOk) == "judgment_ok");
    TEST_CHECK(sprite(JudgmentKind::HoldNg) == "judgment_ng");
    TEST_CHECK(sprite(JudgmentKind::RollNg) == "judgment_ng");
    TEST_CHECK(sprite(JudgmentKind::Tap, TapJudgment::Num).empty());
    TEST_CHECK(sprite(JudgmentKind::AvoidedMine).empty());
    TEST_CHECK(sprite(JudgmentKind::RollHit).empty());

    // Every pop-bearing event has a sprite, every sprite is a real manifest sprite
    // whose content box pins kJudgmentContentRef (888x132 @2x = 444x66 reference px).
    const blaze4k::ThemeManifest manifest =
        blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"));
    const blaze4k::Vec2 ref = JudgmentAnimator::kJudgmentContentRef;
    int checked = 0;
    for (int k = 0; k <= static_cast<int>(JudgmentKind::RollHit); ++k) {
        for (int win = 0; win <= static_cast<int>(TapJudgment::Num); ++win) {
            const JudgmentEvent e =
                make_event(static_cast<JudgmentKind>(k), static_cast<TapJudgment>(win));
            const std::string_view name = JudgmentAnimator::judgment_sprite(e);
            TEST_CHECK(name.empty() == JudgmentAnimator::judgment_label(e).empty());
            if (name.empty()) {
                continue;
            }
            const auto it = manifest.textures.find(name);
            TEST_CHECK(it != manifest.textures.end());
            const blaze4k::ThemeEntry& entry = it->second;
            TEST_CHECK(entry.kind == blaze4k::ThemeKind::Sprite);
            TEST_CHECK(entry.content.w == 888 && entry.content.h == 132);
            TEST_CHECK(std::abs(entry.content.w / manifest.texture_scale - ref.x) < 1e-4f);
            TEST_CHECK(std::abs(entry.content.h / manifest.texture_scale - ref.y) < 1e-4f);
            ++checked;
        }
    }
    TEST_CHECK(checked > 0);

    // The armed pop carries its sprite; reset clears it.
    JudgmentAnimator animator;
    animator.consume({make_event(JudgmentKind::HoldNg)});
    TEST_CHECK(animator.popup_sprite() == "judgment_ng");
    animator.reset();
    TEST_CHECK(animator.popup_sprite().empty());
    std::cout << "  - judgment sprite mapping (real manifest) ok.\n";
}

void test_judgment_pop_rect() {
    const blaze4k::theme::LayoutScale L = blaze4k::theme::layout_scale(1280, 720);
    const blaze4k::Rect r =
        JudgmentAnimator::judgment_pop_rect(L, JudgmentAnimator::kJudgmentContentRef, 1.0f);
    TEST_CHECK(r.x == 418.0f && r.y == 296.0f && r.w == 444.0f && r.h == 66.0f);
    // Scaled about the content centre.
    const blaze4k::Rect big =
        JudgmentAnimator::judgment_pop_rect(L, JudgmentAnimator::kJudgmentContentRef, 1.25f);
    TEST_CHECK(std::abs(big.x + big.w * 0.5f - 640.0f) < 1e-3f);
    TEST_CHECK(std::abs(big.y + big.h * 0.5f - 329.0f) < 1e-3f);
    TEST_CHECK(std::abs(big.w - 555.0f) < 1e-3f);

    // At rest the drawn pop is half the full-size box, centred on the old box centre.
    const double d = JudgmentAnimator::kJudgmentPopSeconds;
    const float rest = JudgmentAnimator::judgment_draw_scale(d);
    const blaze4k::Rect half =
        JudgmentAnimator::judgment_pop_rect(L, JudgmentAnimator::kJudgmentContentRef, rest);
    TEST_CHECK(std::abs(half.x - 529.0f) < 1e-3f && std::abs(half.y - 312.5f) < 1e-3f);
    TEST_CHECK(std::abs(half.w - 222.0f) < 1e-3f && std::abs(half.h - 33.0f) < 1e-3f);
    TEST_CHECK(std::abs(half.w - r.w * 0.5f) < 1e-3f && std::abs(half.h - r.h * 0.5f) < 1e-3f);
    TEST_CHECK(std::abs(half.x + half.w * 0.5f - 640.0f) < 1e-3f);
    TEST_CHECK(std::abs(half.y + half.h * 0.5f - 329.0f) < 1e-3f);

    const blaze4k::theme::LayoutScale L2 = blaze4k::theme::layout_scale(2560, 1440);
    const blaze4k::Vec2 ref = JudgmentAnimator::kJudgmentContentRef;
    const blaze4k::Rect half2 =
        JudgmentAnimator::judgment_pop_rect(L2, blaze4k::Vec2{ref.x * L2.s, ref.y * L2.s}, rest);
    TEST_CHECK(std::abs(half2.x - 1058.0f) < 1e-3f && std::abs(half2.y - 625.0f) < 1e-3f);
    TEST_CHECK(std::abs(half2.w - 444.0f) < 1e-3f && std::abs(half2.h - 66.0f) < 1e-3f);
    TEST_CHECK(std::abs(half2.x + half2.w * 0.5f - 1280.0f) < 1e-3f);
    TEST_CHECK(std::abs(half2.y + half2.h * 0.5f - 658.0f) < 1e-3f);
    std::cout << "  - judgment pop rect ok.\n";
}

// Peak of the drawn judgment scale over the whole pop curve (1001 samples).
float peak_draw_scale() {
    const double d = JudgmentAnimator::kJudgmentPopSeconds;
    float peak = 0.0f;
    for (int i = 0; i <= 1000; ++i) {
        peak = std::max(peak, JudgmentAnimator::judgment_draw_scale(d * i / 1000.0));
    }
    return peak;
}

void test_judgment_draw_scale() {
    const float k = JudgmentAnimator::kJudgmentDisplayScale;
    TEST_CHECK(k == 0.5f);
    const double d = JudgmentAnimator::kJudgmentPopSeconds;
    TEST_CHECK(JudgmentAnimator::judgment_draw_scale(d) == 0.5f);
    TEST_CHECK(JudgmentAnimator::judgment_draw_scale(d * 3.0) == 0.5f);
    // Same curve shape, at the display size.
    for (int i = 0; i <= 1000; ++i) {
        const double e = d * i / 1000.0;
        TEST_CHECK(std::abs(JudgmentAnimator::judgment_draw_scale(e) -
                            k * JudgmentAnimator::pop_scale(e, d)) < 1e-6f);
    }
    const float peak = peak_draw_scale();
    TEST_CHECK(peak > k);
    TEST_CHECK(std::abs(peak - k * 1.25f) < 1e-3f);
    std::cout << "  - judgment draw scale (half size, same pop curve) ok.\n";
}

void test_judgment_clears_combo() {
    namespace layout = blaze4k::theme::layout;
    // Glow pad below the content box, reference px, from the real manifest (max over
    // every judgment_* sprite; 28 today).
    const blaze4k::ThemeManifest manifest =
        blaze4k::parse_theme_manifest(read_text(kCabinet / "manifest.json"));
    float pad = 0.0f;
    int sprites = 0;
    for (const auto& [name, entry] : manifest.textures) {
        if (std::string_view{name}.rfind("judgment_", 0) != 0) {
            continue;
        }
        pad = std::max(pad, static_cast<float>(entry.height - entry.content.y - entry.content.h) /
                                manifest.texture_scale);
        ++sprites;
    }
    TEST_CHECK(sprites == 9);
    TEST_CHECK(pad > 0.0f);

    const float peak = peak_draw_scale();
    const blaze4k::Vec2 ref = JudgmentAnimator::kJudgmentContentRef;
    struct Size {
        int w;
        int h;
    };
    for (const Size s : {Size{1280, 720}, Size{2560, 1440}, Size{1920, 1080}, Size{1280, 1024},
                         Size{2560, 1080}}) {
        const blaze4k::theme::LayoutScale L = blaze4k::theme::layout_scale(s.w, s.h);
        const blaze4k::Rect pop =
            JudgmentAnimator::judgment_pop_rect(L, blaze4k::Vec2{ref.x * L.s, ref.y * L.s}, peak);
        const float combo_top = L.y(layout::kComboTop);
        TEST_CHECK(pop.y + pop.h < combo_top);
        TEST_CHECK(pop.y + pop.h + pad * L.s * peak <= combo_top);
        TEST_CHECK(std::abs(pop.x + pop.w * 0.5f - L.x(layout::kRefWidth * 0.5f)) < 1e-3f);
    }
    std::cout << "  - half-size judgment clears the combo line ok.\n";
}

void test_combo_visibility() {
    JudgmentAnimator animator;
    TEST_CHECK(!animator.combo_visible());
    animator.update(0.01, 3);
    TEST_CHECK(!animator.combo_visible());
    TEST_CHECK(animator.live_combo() == 3);
    animator.update(0.01, JudgmentAnimator::kShowComboAt);
    TEST_CHECK(animator.combo_visible());
    TEST_CHECK(animator.live_combo() == 4);
    // Persistent: it does not fade with the judgment pop.
    animator.update(10.0, 4);
    TEST_CHECK(animator.combo_visible());
    animator.update(0.01, 0);
    TEST_CHECK(!animator.combo_visible());
    animator.update(0.01, -5);
    TEST_CHECK(animator.live_combo() == 0);
    animator.update(0.01, 212);
    TEST_CHECK(animator.combo_visible());
    animator.reset();
    TEST_CHECK(animator.live_combo() == 0);
    TEST_CHECK(!animator.combo_visible());
    std::cout << "  - combo line visibility (ShowComboAt=4) ok.\n";
}

void test_combo_number_color() {
    const double d = JudgmentAnimator::kComboPopSeconds;
    const blaze4k::Color gold = blaze4k::theme::color::kGold;
    const blaze4k::Color white = blaze4k::theme::color::kWhite;
    const auto eq = [](blaze4k::Color a, blaze4k::Color b) {
        return std::abs(a.r - b.r) < 1e-5f && std::abs(a.g - b.g) < 1e-5f &&
               std::abs(a.b - b.b) < 1e-5f && std::abs(a.a - b.a) < 1e-5f;
    };
    TEST_CHECK(eq(JudgmentAnimator::combo_number_color(0.0, d), gold));
    TEST_CHECK(eq(JudgmentAnimator::combo_number_color(d, d), white));
    TEST_CHECK(eq(JudgmentAnimator::combo_number_color(d * 3.0, d), white));
    TEST_CHECK(eq(JudgmentAnimator::combo_number_color(0.1, 0.0), white));
    const blaze4k::Color half = JudgmentAnimator::combo_number_color(d * 0.5, d);
    // Gold has a lower blue channel than white; halfway is strictly between.
    TEST_CHECK(half.b > gold.b && half.b < white.b);
    TEST_CHECK(std::abs(half.b - (gold.b + white.b) * 0.5f) < 1e-4f);
    std::cout << "  - combo milestone flash colour (gold -> white) ok.\n";
}

void test_pop_curves() {
    const double duration = JudgmentAnimator::kJudgmentPopSeconds;

    TEST_CHECK(JudgmentAnimator::pop_active(0.0, duration));
    TEST_CHECK(!JudgmentAnimator::pop_active(duration, duration));

    TEST_CHECK(JudgmentAnimator::pop_scale(0.0, duration) >= 1.0f);
    TEST_CHECK(JudgmentAnimator::pop_scale(duration, duration) == 1.0f);
    TEST_CHECK(JudgmentAnimator::pop_scale(0.1, duration) > 1.0f);

    TEST_CHECK(JudgmentAnimator::pop_alpha(0.0, duration) == 1.0f);
    TEST_CHECK(JudgmentAnimator::pop_alpha(duration, duration) == 0.0f);

    // Degenerate duration never divides by zero.
    TEST_CHECK(JudgmentAnimator::pop_scale(0.1, 0.0) == 1.0f);
    TEST_CHECK(JudgmentAnimator::pop_alpha(0.1, 0.0) == 0.0f);
    TEST_CHECK(!JudgmentAnimator::pop_active(0.0, 0.0));
    std::cout << "  - pop curves ok.\n";
}

void test_consume_arms_popup() {
    JudgmentAnimator animator;

    animator.consume({make_event(JudgmentKind::Tap, TapJudgment::Fantastic)});
    TEST_CHECK(animator.has_popup());
    TEST_CHECK(animator.popup_label() == "FANTASTIC");

    // Last event in a chord tick wins.
    animator.consume({make_event(JudgmentKind::Tap, TapJudgment::Fantastic),
                      make_event(JudgmentKind::Tap, TapJudgment::Great)});
    TEST_CHECK(animator.popup_label() == "GREAT");

    // Stats-only events carry no popup.
    JudgmentAnimator quiet;
    quiet.consume({make_event(JudgmentKind::AvoidedMine), make_event(JudgmentKind::RollHit)});
    TEST_CHECK(!quiet.has_popup());

    // A pop fades out after its duration.
    animator.update(JudgmentAnimator::kJudgmentPopSeconds, 0);
    TEST_CHECK(!animator.has_popup());
    std::cout << "  - consume arms/fades popups ok.\n";
}

void test_combo_milestone_dedupe() {
    JudgmentAnimator animator;

    // A non-milestone combo never pops.
    animator.update(0.1, 49);
    TEST_CHECK(!animator.has_combo_pop());

    // Milestone fires once...
    animator.update(0.6, 50);
    TEST_CHECK(animator.has_combo_pop());
    TEST_CHECK(animator.combo_value() == 50);

    // ...and does not re-fire at the same milestone (expires without rearm).
    animator.update(0.6, 50);
    TEST_CHECK(!animator.has_combo_pop());

    // A new distinct milestone fires again.
    animator.update(0.1, 100);
    TEST_CHECK(animator.has_combo_pop());
    TEST_CHECK(animator.combo_value() == 100);

    // The final (non-round) combo can be celebrated explicitly; <= 0 is ignored.
    animator.reset();
    animator.celebrate(37);
    TEST_CHECK(animator.has_combo_pop());
    TEST_CHECK(animator.combo_value() == 37);
    animator.reset();
    animator.celebrate(0);
    TEST_CHECK(!animator.has_combo_pop());

    animator.reset();
    TEST_CHECK(!animator.has_popup());
    TEST_CHECK(!animator.has_combo_pop());
    std::cout << "  - combo milestone fires once and dedupes ok.\n";
}

void test_combo_milestone_crossing_and_break_reset() {
    JudgmentAnimator animator;

    // A chord spanning a multiple (49 -> 51) must still fire the 50 pop, since
    // ScoreKeeper advances combo by the whole row size.
    animator.update(0.1, 49);
    TEST_CHECK(!animator.has_combo_pop());
    animator.update(0.1, 51);
    TEST_CHECK(animator.has_combo_pop());
    TEST_CHECK(animator.combo_value() == 51);

    // The crossing is deduped: the pop expires without rearming at the same
    // multiple.
    animator.update(JudgmentAnimator::kComboPopSeconds, 51);
    TEST_CHECK(!animator.has_combo_pop());

    // A combo break resets the tracker...
    animator.update(0.1, 0);
    TEST_CHECK(!animator.has_combo_pop());
    // ...so rebuilding to the same milestone re-fires.
    animator.update(0.1, 50);
    TEST_CHECK(animator.has_combo_pop());
    TEST_CHECK(animator.combo_value() == 50);
    std::cout << "  - chord-crossing + break-reset re-fire ok.\n";
}

void test_headless_render_and_reset() {
    blaze4k::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    JudgmentAnimator animator;

    animator.consume({make_event(JudgmentKind::Tap, TapJudgment::Excellent)});
    animator.update(0.1, 50);
    // Null services (bitmap fallback / no text) must not crash.
    animator.render_judgment(renderer, 1280, 720, nullptr);
    animator.render_combo(renderer, 1280, 720, nullptr);

    // Real headless theme + fonts (draws are no-ops without GL).
    blaze4k::ThemeTextures theme;
    TEST_CHECK(theme.load(kCabinet));
    blaze4k::TextRenderer text;
    TEST_CHECK(text.load(kSourceDir));
    text.set_window_size(1280, 720);
    animator.render_judgment(renderer, 1280, 720, &theme);
    animator.render_combo(renderer, 1280, 720, &text);
    animator.render_judgment(renderer, 0, 0, &theme);
    animator.render_combo(renderer, 0, 0, &text);
    animator.reset();
    animator.render_judgment(renderer, 1280, 720, &theme);
    animator.render_combo(renderer, 1280, 720, &text);
    text.shutdown();
    theme.shutdown();
    std::cout << "  - headless render + reset ok.\n";
}

} // namespace

int main() {
    std::cout << "[judgment_animator_test] Running judgment/combo pop tests...\n";
    test_label_mapping();
    test_sprite_mapping();
    test_judgment_pop_rect();
    test_judgment_draw_scale();
    test_judgment_clears_combo();
    test_combo_visibility();
    test_combo_number_color();
    test_pop_curves();
    test_consume_arms_popup();
    test_combo_milestone_dedupe();
    test_combo_milestone_crossing_and_break_reset();
    test_headless_render_and_reset();
    std::cout << "[judgment_animator_test] All tests passed!\n";
    return 0;
}
