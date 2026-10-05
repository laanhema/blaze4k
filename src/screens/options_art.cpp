#include "screens/options_art.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "render/gl_quad_renderer.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"

namespace blaze4k::options_art {

namespace layout = theme::layout;

namespace {

// Line-box top that centres `style`'s line in a reference band [ref_top, ref_top + ref_h].
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style) {
    return L.y(ref_top) + (L.px(ref_h) - text.line_height(style)) * 0.5f;
}

// Legend: [up down] ROW [left right] CHANGE ENTER NEXT ESC CLOSE.
using Item = select_art::HintItem;
constexpr std::array<Item, 8> kOptionsHintItems = {{
    {Item::Kind::VArrows, {}},
    {Item::Kind::Word, "ROW"},
    {Item::Kind::HArrows, {}},
    {Item::Kind::Word, "CHANGE"},
    {Item::Kind::Key, "ENTER"},
    {Item::Kind::Word, "NEXT"},
    {Item::Kind::Key, "ESC"},
    {Item::Kind::Word, "CLOSE"},
}};

// Text styles (every one shares a pre-baked (font, size) in theme::text::kAllStyles).
constexpr theme::TextStyle kNameStyle = theme::text::kWheelRow;
constexpr theme::TextStyle kValueStyle = with_color(theme::text::kWheelPack, theme::color::kGold);
constexpr theme::TextStyle kSelectedStyle = theme::text::kWheelSelected;

// A row's name at name_x. Truncated only when it would run into the value
// (never for real values; select_art_test pins it), so the usual path does not allocate.
void draw_name(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
               const Rect& row, bool selected, std::string_view name, std::string_view value,
               const theme::TextStyle& name_style, const theme::TextStyle& value_style) {
    const float nx = name_x(row, selected);
    const float budget = value_right(row, selected) - ref_measure(text, value, value_style) -
                         kNameValueGap - nx;
    const float top = centred_top(text, L, row.y, row.h, name_style);
    if (ref_measure(text, name, name_style) > budget) {
        text.draw(renderer, text.truncate(name, name_style, L.px(std::max(0.0f, budget))), L.x(nx),
                  top, name_style, TextAlign::Left);
        return;
    }
    text.draw(renderer, name, L.x(nx), top, name_style, TextAlign::Left);
}

// A row's value, right-aligned at value_right.
void draw_value(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const Rect& row, bool selected, std::string_view value,
                const theme::TextStyle& style) {
    text.draw(renderer, value, L.x(value_right(row, selected)),
              centred_top(text, L, row.y, row.h, style), style, TextAlign::Right);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

Rect panel_rect() {
    return kPanel;
}

Rect header_rect() {
    return Rect{kPanel.x, kPanel.y, kPanel.w, kHeaderHeight};
}

Rect row_rect(int row, int selected) {
    row = std::clamp(row, 0, kOptionsRowCount - 1);
    selected = std::clamp(selected, 0, kOptionsRowCount - 1);
    const float y = kRowsTop + static_cast<float>(row) * kRowPitch +
                    (row > selected ? kRowSelectedHeight - kRowHeight : 0.0f);
    return Rect{kRowX, y, kRowWidth, row == selected ? kRowSelectedHeight : kRowHeight};
}

float name_x(const Rect& row, bool selected) {
    return row.x + (selected ? select_art::kWheelSelectedTextX : select_art::kWheelSongTextX);
}

float value_right(const Rect& row, bool selected) {
    return row.x + row.w - (selected ? kValueSelectedInset : kValueInset);
}

const std::array<std::string, kOptionsRowCount>& row_names() {
    static const std::array<std::string, kOptionsRowCount> kNames = [] {
        std::array<std::string, kOptionsRowCount> names{};
        for (int i = 0; i < kOptionsRowCount; ++i) {
            names[static_cast<std::size_t>(i)] = options_row_name(i);
        }
        return names;
    }();
    return kNames;
}

select_art::HintLine hint_layout(const select_art::HintMeasure& measure_key,
                                 const select_art::HintMeasure& measure_word) {
    return select_art::layout_hint_items(kOptionsHintItems, measure_key, measure_word);
}

// ---------------------------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------------------------

void draw_panel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L) {
    // Body, then a 2px ring inside its edge (top, bottom, left, right).
    renderer.draw_quad(L.rect(kPanel), theme::color::kNavyPanel);
    const float r = kPanelRing;
    const Rect ring[4] = {
        Rect{kPanel.x, kPanel.y, kPanel.w, r},
        Rect{kPanel.x, kPanel.y + kPanel.h - r, kPanel.w, r},
        Rect{kPanel.x, kPanel.y + r, r, kPanel.h - 2.0f * r},
        Rect{kPanel.x + kPanel.w - r, kPanel.y + r, r, kPanel.h - 2.0f * r},
    };
    for (const Rect& strip : ring) {
        renderer.draw_quad(L.rect(strip), select_art::kEditEdge);
    }
    if (theme != nullptr) {
        theme->draw_stretch_x(renderer, "bar_top", L.x(kPanel.x), L.y(kPanel.y), L.px(kPanel.w),
                              L.s);
    }
    if (text != nullptr) {
        text->draw(renderer, "OPTIONS", L.x(kTitleX),
                   centred_top(*text, L, kPanel.y, kHeaderBandHeight, kTitleStyle), kTitleStyle,
                   TextAlign::Left);
    }
}

void draw_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
               const theme::LayoutScale& L, std::span<const std::string> values, int selected) {
    selected = std::clamp(selected, 0, kOptionsRowCount - 1);
    auto value_of = [values](int i) -> std::string_view {
        const auto index = static_cast<std::size_t>(i);
        return index < values.size() ? std::string_view(values[index]) : std::string_view{};
    };

    // Art: the navy rows, then the gold selected bar.
    if (theme != nullptr) {
        for (int i = 0; i < kOptionsRowCount; ++i) {
            if (i != selected) {
                theme->draw_slice3(renderer, "wheel_row", L.rect(row_rect(i, selected)));
            }
        }
        theme->draw_stretch(renderer, "wheel_row_selected", L.rect(row_rect(selected, selected)));
    }
    if (text == nullptr) {
        return;
    }

    // Text grouped by style: unselected names, unselected values, then the
    // selected pair (one style).
    const std::array<std::string, kOptionsRowCount>& names = row_names();
    for (int i = 0; i < kOptionsRowCount; ++i) {
        if (i != selected) {
            draw_name(*text, renderer, L, row_rect(i, selected), false,
                      names[static_cast<std::size_t>(i)], value_of(i), kNameStyle, kValueStyle);
        }
    }
    for (int i = 0; i < kOptionsRowCount; ++i) {
        if (i != selected) {
            draw_value(*text, renderer, L, row_rect(i, selected), false, value_of(i), kValueStyle);
        }
    }
    const Rect bar = row_rect(selected, selected);
    draw_name(*text, renderer, L, bar, true, names[static_cast<std::size_t>(selected)],
              value_of(selected), kSelectedStyle, kSelectedStyle);
    draw_value(*text, renderer, L, bar, true, value_of(selected), kSelectedStyle);
}

void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w) {
    // bar_hint is opaque, so it fully covers select's own legend.
    if (theme != nullptr) {
        const float bar_h = theme->content_size("bar_hint", L.s).y;
        theme->draw_stretch_x(renderer, "bar_hint", 0.0f, L.y(layout::kRefHeight) - bar_h,
                              static_cast<float>(w), L.s);
    }
    if (text == nullptr) {
        return;
    }
    select_art::draw_hint_line(
        *text, renderer, L,
        hint_layout(
            [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintKey); },
            [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintWord); }));
}

void draw_overlay(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                  const theme::LayoutScale& L, int w, int h, std::span<const std::string> values,
                  int selected) {
    // The whole window (letterbox bands included).
    renderer.draw_quad(Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)},
                       theme::color::kGameplayScrim);
    draw_panel(theme, text, renderer, L);
    draw_rows(theme, text, renderer, L, values, selected);
    draw_hint_bar(theme, text, renderer, L, w);
}

} // namespace blaze4k::options_art
