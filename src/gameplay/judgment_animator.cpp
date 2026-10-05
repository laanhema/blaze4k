#include "gameplay/judgment_animator.hpp"

#include <algorithm>
#include <string>

#include "gameplay/hud_renderer.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"

namespace blaze4k {

void JudgmentAnimator::reset() {
    has_pop_ = false;
    pop_label_.clear();
    pop_sprite_ = {};
    pop_elapsed_ = 0.0;
    has_combo_pop_ = false;
    combo_elapsed_ = 0.0;
    combo_value_ = 0;
    last_milestone_ = 0;
    live_combo_ = 0;
}

void JudgmentAnimator::consume(const std::vector<JudgmentEvent>& events) {
    for (const JudgmentEvent& event : events) {
        const std::string label = judgment_label(event);
        if (label.empty()) {
            continue;
        }
        has_pop_ = true;
        pop_label_ = label;
        pop_sprite_ = judgment_sprite(event);
        pop_elapsed_ = 0.0;
    }
}

void JudgmentAnimator::update(double fixed_dt, int combo) {
    live_combo_ = std::max(combo, 0);
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

float JudgmentAnimator::judgment_draw_scale(double elapsed) {
    return kJudgmentDisplayScale * pop_scale(elapsed, kJudgmentPopSeconds);
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

std::string_view JudgmentAnimator::judgment_sprite(const JudgmentEvent& e) {
    switch (e.kind) {
        case JudgmentKind::Tap:
            switch (e.window) {
                case TapJudgment::Fantastic: return "judgment_fantastic";
                case TapJudgment::Excellent: return "judgment_excellent";
                case TapJudgment::Great: return "judgment_great";
                case TapJudgment::Decent: return "judgment_decent";
                case TapJudgment::WayOff: return "judgment_wayoff";
                case TapJudgment::Miss: return "judgment_miss";
                case TapJudgment::HitMine: return "judgment_mine";
                case TapJudgment::Num: return {};
            }
            return {};
        case JudgmentKind::Miss: return "judgment_miss";
        case JudgmentKind::HitMine: return "judgment_mine";
        case JudgmentKind::HoldOk:
        case JudgmentKind::RollOk: return "judgment_ok";
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollNg: return "judgment_ng";
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit: return {};
    }
    return {};
}

Rect JudgmentAnimator::judgment_pop_rect(const theme::LayoutScale& L, Vec2 content, float scale) {
    const float cx = L.x(theme::layout::kRefWidth * 0.5f);
    const float cy = L.y(theme::layout::kJudgmentTop) + content.y * 0.5f;
    const float w = content.x * scale;
    const float h = content.y * scale;
    return Rect{cx - w * 0.5f, cy - h * 0.5f, w, h};
}

JudgmentAnimator::ComboLineLayout JudgmentAnimator::combo_line_layout(const theme::LayoutScale& L,
                                                                      float number_w, float label_w,
                                                                      float number_ascent,
                                                                      float label_ascent) {
    const float gap = L.px(kComboGap);
    const float total = number_w + gap + label_w;
    ComboLineLayout layout;
    layout.number = Vec2{L.x(theme::layout::kRefWidth * 0.5f) - total * 0.5f,
                         L.y(theme::layout::kComboTop)};
    const float baseline = layout.number.y + number_ascent;
    layout.label = Vec2{layout.number.x + number_w + gap, baseline - label_ascent};
    return layout;
}

Color JudgmentAnimator::combo_number_color(double elapsed, double duration) {
    const Color from = theme::color::kGold;
    const Color to = theme::color::kWhite;
    if (duration <= 0.0 || !(elapsed < duration)) {
        return to;
    }
    const float t = elapsed <= 0.0 ? 0.0f : static_cast<float>(elapsed / duration);
    return Color{from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
                 from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t};
}

void JudgmentAnimator::render_judgment(GlQuadRenderer& renderer, int w, int h,
                                       const ThemeTextures* theme) const {
    if (!renderer.is_initialized() || w <= 0 || h <= 0 || !has_pop_) {
        return;
    }
    const theme::LayoutScale L = theme::layout_scale(w, h);
    const float scale = judgment_draw_scale(pop_elapsed_);
    const float alpha = pop_alpha(pop_elapsed_, kJudgmentPopSeconds);

    if (theme != nullptr && !pop_sprite_.empty() && theme->entry(pop_sprite_) != nullptr) {
        // The italic slant is baked into the sprite: no extra shear here.
        const Rect rect = judgment_pop_rect(L, theme->content_size(pop_sprite_, L.s), scale);
        theme->draw_sprite(renderer, pop_sprite_, Vec2{rect.x, rect.y}, L.s * scale,
                           Color{1.0f, 1.0f, 1.0f, alpha});
        return;
    }
    // Missing theme or manifest entry: the bitmap label keeps feedback on screen.
    draw_text_centered(renderer, pop_label_, L.x(theme::layout::kRefWidth * 0.5f),
                       L.y(theme::layout::kJudgmentTop), kJudgmentPopPixel * scale,
                       Color{1.0f, 1.0f, 1.0f, alpha});
}

void JudgmentAnimator::render_combo(GlQuadRenderer& renderer, int w, int h,
                                    TextRenderer* text) const {
    if (text == nullptr || !renderer.is_initialized() || w <= 0 || h <= 0 || !combo_visible()) {
        return;
    }
    const theme::LayoutScale L = theme::layout_scale(w, h);
    // Fits the small-string buffer: no heap allocation per frame.
    const std::string number = format_combo(live_combo_);
    constexpr std::string_view kLabel = "COMBO";

    theme::TextStyle number_style = theme::text::kComboNumber;
    if (has_combo_pop_) {
        // Colour-only milestone flash: a size change would bake a new atlas mid-song.
        number_style.color = combo_number_color(combo_elapsed_, kComboPopSeconds);
    }
    const theme::TextStyle& label_style = theme::text::kComboLabel;

    const ComboLineLayout layout =
        combo_line_layout(L, text->measure(number, number_style), text->measure(kLabel, label_style),
                          text->ascent(number_style), text->ascent(label_style));
    text->draw(renderer, number, layout.number.x, layout.number.y, number_style, TextAlign::Left,
               theme::text::kComboGroupShear);
    text->draw(renderer, kLabel, layout.label.x, layout.label.y, label_style, TextAlign::Left,
               theme::text::kComboGroupShear);
}

} // namespace blaze4k
