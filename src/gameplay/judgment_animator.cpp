#include "gameplay/judgment_animator.hpp"

#include <string>

#include "gameplay/hud_renderer.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"

namespace blaze4k {

void JudgmentAnimator::reset() {
    has_pop_ = false;
    pop_label_.clear();
    pop_color_ = Color{};
    pop_elapsed_ = 0.0;
    has_combo_pop_ = false;
    combo_elapsed_ = 0.0;
    combo_value_ = 0;
    last_milestone_ = 0;
}

void JudgmentAnimator::consume(const std::vector<JudgmentEvent>& events) {
    for (const JudgmentEvent& event : events) {
        const std::string label = judgment_label(event);
        if (label.empty()) {
            continue;
        }
        has_pop_ = true;
        pop_label_ = label;
        pop_color_ = judgment_color(event);
        pop_elapsed_ = 0.0;
    }
}

void JudgmentAnimator::update(double fixed_dt, int combo) {
    if (has_pop_) {
        pop_elapsed_ += fixed_dt;
        if (!pop_active(pop_elapsed_, kJudgmentPopSeconds)) {
            has_pop_ = false;
        }
    }
    if (has_combo_pop_) {
        combo_elapsed_ += fixed_dt;
        if (!pop_active(combo_elapsed_, kComboPopSeconds)) {
            has_combo_pop_ = false;
        }
    }

    // Fire once per milestone *crossing*. Combo advances by whole chord size, so a
    // row can jump past a multiple (e.g. 49 -> 51) without ever equalling it; key
    // dedupe on the multiple instead of exact equality. A combo break resets the
    // tracker so rebuilding to the same milestone re-fires.
    if (combo == 0) {
        last_milestone_ = 0;
    } else if (combo > 0) {
        const int milestone = (combo / kComboMilestone) * kComboMilestone;
        if (milestone > last_milestone_) {
            last_milestone_ = milestone;
            combo_value_ = combo;
            combo_elapsed_ = 0.0;
            has_combo_pop_ = true;
        }
    }
}

void JudgmentAnimator::celebrate(int combo) {
    if (combo <= 0) {
        return;
    }
    combo_value_ = combo;
    combo_elapsed_ = 0.0;
    has_combo_pop_ = true;
}

float JudgmentAnimator::pop_scale(double elapsed, double duration) {
    if (duration <= 0.0 || elapsed >= duration) {
        return 1.0f;
    }
    const double t = elapsed / duration;
    if (t < 0.2) {
        return static_cast<float>(1.0 + 0.25 * (t / 0.2));
    }
    return static_cast<float>(1.25 - 0.25 * ((t - 0.2) / 0.8));
}

float JudgmentAnimator::pop_alpha(double elapsed, double duration) {
    if (duration <= 0.0 || elapsed >= duration) {
        return 0.0f;
    }
    const double t = elapsed / duration;
    if (t < 0.6) {
        return 1.0f;
    }
    return static_cast<float>(1.0 - (t - 0.6) / 0.4);
}

bool JudgmentAnimator::pop_active(double elapsed, double duration) {
    return duration > 0.0 && elapsed < duration;
}

std::string JudgmentAnimator::judgment_label(const JudgmentEvent& e) {
    switch (e.kind) {
        case JudgmentKind::Tap:
            switch (e.window) {
                case TapJudgment::Fantastic: return "FANTASTIC";
                case TapJudgment::Excellent: return "EXCELLENT";
                case TapJudgment::Great: return "GREAT";
                case TapJudgment::Decent: return "DECENT";
                case TapJudgment::WayOff: return "WAY OFF";
                case TapJudgment::Miss: return "MISS";
                case TapJudgment::HitMine: return "MINE";
                case TapJudgment::Num: return "";
            }
            return "";
        case JudgmentKind::Miss: return "MISS";
        case JudgmentKind::HitMine: return "MINE";
        case JudgmentKind::HoldOk:
        case JudgmentKind::RollOk: return "OK";
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollNg: return "NG";
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit: return "";
    }
    return "";
}

Color JudgmentAnimator::judgment_color(const JudgmentEvent& e) {
    return blaze4k::judgment_color(e.kind, e.window, e.hold);
}

void JudgmentAnimator::render(GlQuadRenderer& renderer, int w, int h) const {
    if (!renderer.is_initialized() || w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);

    if (has_pop_) {
        const float scale = pop_scale(pop_elapsed_, kJudgmentPopSeconds);
        const float alpha = pop_alpha(pop_elapsed_, kJudgmentPopSeconds);
        draw_text_centered(renderer, pop_label_, width * 0.5f,
                           static_cast<float>(static_cast<double>(h) * 0.42),
                           5.0f * scale, with_alpha(pop_color_, alpha));
    }

    if (has_combo_pop_) {
        const float scale = pop_scale(combo_elapsed_, kComboPopSeconds);
        const float alpha = pop_alpha(combo_elapsed_, kComboPopSeconds);
        draw_text_centered(renderer, std::to_string(combo_value_), width * 0.5f, 8.0f,
                           6.0f * scale, with_alpha(Color{1.0f, 1.0f, 1.0f, 1.0f}, alpha));
    }
}

} // namespace blaze4k
