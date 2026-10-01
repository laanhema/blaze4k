#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "gameplay/judgment_animator.hpp"
#include "render/gl_quad_renderer.hpp"

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

void test_color_mapping() {
    const blaze4k::Color fantastic =
        JudgmentAnimator::judgment_color(make_event(JudgmentKind::Tap, TapJudgment::Fantastic));
    const blaze4k::Color miss = JudgmentAnimator::judgment_color(make_event(JudgmentKind::Miss));
    const blaze4k::Color ok = JudgmentAnimator::judgment_color(make_event(JudgmentKind::HoldOk));
    const blaze4k::Color ng = JudgmentAnimator::judgment_color(make_event(JudgmentKind::HoldNg));
    TEST_CHECK(fantastic.r != miss.r || fantastic.g != miss.g || fantastic.b != miss.b);
    TEST_CHECK(ok.b != ng.b || ok.r != ng.r);
    std::cout << "  - judgment color mapping ok.\n";
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
    animator.render(renderer, 1280, 720); // must not crash
    animator.render(renderer, 0, 0);
    animator.reset();
    animator.render(renderer, 1280, 720);
    std::cout << "  - headless render + reset ok.\n";
}

} // namespace

int main() {
    std::cout << "[judgment_animator_test] Running judgment/combo pop tests...\n";
    test_label_mapping();
    test_color_mapping();
    test_pop_curves();
    test_consume_arms_popup();
    test_combo_milestone_dedupe();
    test_combo_milestone_crossing_and_break_reset();
    test_headless_render_and_reset();
    std::cout << "[judgment_animator_test] All tests passed!\n";
    return 0;
}
