#include "gameplay/hud_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"

namespace blaze4k {

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

LifeBarLayout layout_life_bar(double life, int screen_w, int screen_h, double field_left) {
    LifeBarLayout layout;
    if (screen_w <= 0 || screen_h <= 0) {
        return layout;
    }

    double clamped = std::isnan(life) ? 0.0 : life;
    clamped = std::clamp(clamped, 0.0, 1.0);
    layout.fraction = static_cast<float>(clamped);
    layout.danger = clamped < theme::color::kLifeDangerThreshold;

    const theme::LayoutScale L = theme::layout_scale(screen_w, screen_h);
    const float border = L.px(kLifeFrameBorderRef);
    layout.border = border;

    // The frame starts at the Cabinet layout, then slides left and shrinks as needed so
    // it stays at least kLifeBarFieldGap left of the note field (narrow windows).
    Rect frame = L.rect(theme::layout::kLifeBar);
    const float max_right = static_cast<float>(field_left) - kLifeBarFieldGap;
    if (frame.x + frame.w > max_right) {
        frame.x = std::max(0.0f, max_right - frame.w);
        if (frame.x + frame.w > max_right) {
            const float min_w = 2.0f * border + kLifeBarMinTrack;
            frame.w = std::max(min_w, max_right - frame.x);
        }
    }
    layout.frame = frame;
    layout.track = Rect{frame.x + border, frame.y + border, frame.w - 2.0f * border,
                        frame.h - 2.0f * border};

    const std::optional<Rect> fill = fill_cropped_rect(layout.track, layout.fraction);
    layout.fill = fill ? *fill
                       : Rect{layout.track.x, layout.track.y + layout.track.h, layout.track.w, 0.0f};
    layout.visible = true;
    return layout;
}

DiffBadgeLayout layout_diff_badge(float text_w, int screen_w, int screen_h, double field_left) {
    DiffBadgeLayout layout;
    if (screen_w <= 0 || screen_h <= 0) {
        return layout;
    }
    const theme::LayoutScale L = theme::layout_scale(screen_w, screen_h);
    const float pad = L.px(kBadgeTextPadX);
    const float min_w = L.px(theme::layout::kDiffBadge.w);
    const float max_w = L.px(kBadgeMaxWidthRef);
    const float wanted = std::isfinite(text_w) ? std::max(text_w, 0.0f) + 2.0f * pad : max_w;

    Rect plate = L.rect(theme::layout::kDiffBadge);
    plate.w = std::clamp(wanted, min_w, max_w);

    bool visible = true;
    const float max_right = static_cast<float>(field_left) - kLifeBarFieldGap;
    if (plate.x + plate.w > max_right) {
        plate.w = std::max(0.0f, max_right - plate.x);
        visible = plate.w >= min_w;
    }

    layout.plate = plate;
    layout.text_x = plate.x + pad;
    layout.text_max_w = std::max(0.0f, plate.w - 2.0f * pad);
    layout.visible = visible;
    return layout;
}

void HudRenderer::render_chrome(const DifficultyBadge& badge, double life, int screen_w,
                                int screen_h, double field_left, const ThemeTextures* theme,
                                const TextRenderer* text, GlQuadRenderer& renderer) const {
    if (theme == nullptr || !renderer.is_initialized() || screen_w <= 0 || screen_h <= 0) {
        return;
    }
    const theme::LayoutScale L = theme::layout_scale(screen_w, screen_h);

    if (!badge.text.empty()) {
        const float text_w = text != nullptr ? text->measure(badge.text, theme::text::kBadge) : 0.0f;
        const DiffBadgeLayout plate = layout_diff_badge(text_w, screen_w, screen_h, field_left);
        if (plate.visible) {
            theme->draw_slice3(renderer, "diff_badge", plate.plate, badge.colors.fill);
        }
    }

    const LifeBarLayout bar = layout_life_bar(life, screen_w, screen_h, field_left);
    if (!bar.visible) {
        return;
    }
    theme->draw_slice9(renderer, "life_frame", bar.frame, L.s);
    if (bar.fill.h > 0.0f) {
        // The full texture maps to the whole track; the UVs crop to the filled part, so
        // the gradient stays put as life changes.
        theme->draw_fill_cropped(renderer, bar.danger ? "life_fill_danger" : "life_fill", bar.track,
                                 bar.fraction);
        theme->draw_tiled(renderer, "life_stripes", bar.fill, L.s, Color{}, TileAnchor::Bottom);
    }
}

void HudRenderer::render_text(const DifficultyBadge& badge, int screen_w, int screen_h,
                              double field_left, TextRenderer* text, GlQuadRenderer& renderer) {
    if (text == nullptr || badge.text.empty() || !renderer.is_initialized() || screen_w <= 0 ||
        screen_h <= 0) {
        return;
    }
    const theme::TextStyle& base = theme::text::kBadge;
    const DiffBadgeLayout layout =
        layout_diff_badge(text->measure(badge.text, base), screen_w, screen_h, field_left);
    if (!layout.visible) {
        return;
    }

    // Re-truncate only when the input changes, so steady-state frames do not allocate.
    if (cached_source_ != badge.text || cached_max_w_ != layout.text_max_w ||
        cached_scale_ != text->scale()) {
        cached_source_ = badge.text;
        cached_max_w_ = layout.text_max_w;
        cached_scale_ = text->scale();
        cached_text_ = text->truncate(badge.text, base, layout.text_max_w);
    }

    theme::TextStyle style = base;
    style.color = badge.colors.ink;
    const float top = layout.plate.y + (layout.plate.h - text->line_height(style)) * 0.5f;
    text->draw(renderer, cached_text_, layout.text_x, top, style, TextAlign::Left);
}

} // namespace blaze4k
