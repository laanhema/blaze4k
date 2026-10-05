#include "screens/setup_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "render/gl_quad_renderer.hpp"
#include "render/theme_textures.hpp"

namespace blaze4k::setup_art {

namespace layout = theme::layout;

namespace {

// Line-box top that centres `style`'s line in a reference band [ref_top, ref_top + ref_h].
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style) {
    return L.y(ref_top) + (L.px(ref_h) - text.line_height(style)) * 0.5f;
}

float finite_or_zero(float v) {
    return std::isfinite(v) ? v : 0.0f;
}

// A binding row's key, right-aligned at value_right. Truncated only when it
// would run into the action name (never for the default bindings or <PRESS>;
// setup_art_test pins it), so the usual path does not allocate.
void draw_key(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
              const Rect& row, bool selected, std::string_view action, std::string_view key,
              const theme::TextStyle& action_style, const theme::TextStyle& key_style) {
    const float right = options_art::value_right(row, selected);
    const float budget = right - options_art::name_x(row, selected) -
                         ref_measure(text, action, action_style) - options_art::kNameValueGap;
    const float top = centred_top(text, L, row.y, row.h, key_style);
    if (ref_measure(text, key, key_style) > budget) {
        text.draw(renderer, text.truncate(key, key_style, L.px(std::max(0.0f, budget))), L.x(right),
                  top, key_style, TextAlign::Right);
        return;
    }
    text.draw(renderer, key, L.x(right), top, key_style, TextAlign::Right);
}

// Left-aligned text at name_x, centred in the row.
void draw_name(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
               const Rect& row, bool selected, std::string_view name,
               const theme::TextStyle& style) {
    text.draw(renderer, name, L.x(options_art::name_x(row, selected)),
              centred_top(text, L, row.y, row.h, style), style, TextAlign::Left);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Pure layout: remap
// ---------------------------------------------------------------------------------------------

int remap_display_count(std::span<const RemapRow> rows) {
    int count = 1; // the Reset row
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i == 0 || rows[i].device != rows[i - 1].device) {
            ++count; // a header before each run of same-device rows
        }
        ++count;
    }
    return count;
}

RemapDisplayRow remap_display_row(std::span<const RemapRow> rows, int d) {
    const int count = remap_display_count(rows);
    d = std::clamp(d, 0, count - 1);
    if (d == count - 1) {
        return RemapDisplayRow{RemapDisplayRow::Kind::Reset, -1, DeviceType::Keyboard};
    }
    int index = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i == 0 || rows[i].device != rows[i - 1].device) {
            if (index == d) {
                return RemapDisplayRow{RemapDisplayRow::Kind::Header, -1, rows[i].device};
            }
            ++index;
        }
        if (index == d) {
            return RemapDisplayRow{RemapDisplayRow::Kind::Binding, static_cast<int>(i),
                                   rows[i].device};
        }
        ++index;
    }
    return RemapDisplayRow{RemapDisplayRow::Kind::Reset, -1, DeviceType::Keyboard};
}

int remap_display_index(std::span<const RemapRow> rows, int binding) {
    if (rows.empty()) {
        return 0;
    }
    binding = std::clamp(binding, 0, static_cast<int>(rows.size()) - 1);
    int headers = 0;
    for (int i = 0; i <= binding; ++i) {
        const auto n = static_cast<std::size_t>(i);
        if (n == 0 || rows[n].device != rows[n - 1].device) {
            ++headers;
        }
    }
    return binding + headers;
}

int remap_reset_display_index(std::span<const RemapRow> rows) {
    return remap_display_count(rows) - 1;
}

Rect remap_row_rect(int slot, int selected_slot) {
    const float y = kRemapListTop + static_cast<float>(slot) * options_art::kRowPitch +
                    (slot > selected_slot
                         ? options_art::kRowSelectedHeight - options_art::kRowHeight
                         : 0.0f);
    return Rect{options_art::kRowX, y, options_art::kRowWidth,
                slot == selected_slot ? options_art::kRowSelectedHeight : options_art::kRowHeight};
}

Rect remap_chip_rect(float text_w) {
    const float w = std::max(0.0f, finite_or_zero(text_w)) + 2.0f * select_art::kChipPadX;
    return Rect{select_art::kChipRight - w, select_art::kChipTop, w, select_art::kChipHeight};
}

std::string_view remap_value_view(const InputRemapModel& model, int row) {
    if (row < 0 || row >= static_cast<int>(model.rows.size())) {
        return {};
    }
    if (model.capturing && row == model.row) {
        return kPressLabel;
    }
    return model.rows[static_cast<std::size_t>(row)].name;
}

// ---------------------------------------------------------------------------------------------
// Pure layout: calibration
// ---------------------------------------------------------------------------------------------

Rect calibration_plate_rect(int i) {
    const int n = std::clamp(i, 0, 1);
    return Rect{kCalPlateX + static_cast<float>(n) * (kCalPlateWidth + kCalPlateGap), kCalPlateTop,
                kCalPlateWidth, kCalPlateHeight};
}

float plate_centre_x(const Rect& plate, float centre_y) {
    return finite_or_zero(plate.x + plate.w * 0.5f +
                          theme::skew::kStatPanel *
                              (plate.y + plate.h * 0.5f - finite_or_zero(centre_y)));
}

std::string calibration_samples_text(int count, int min_samples, bool ready) {
    std::string out = std::to_string(count);
    if (!ready) {
        out += " / ";
        out += std::to_string(min_samples);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------------------------

void draw_chrome(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                 const theme::LayoutScale& L, int w, int h, std::string_view title) {
    if (theme != nullptr) {
        select_art::draw_backdrop(*theme, renderer, w, h);
        theme->draw_stretch_x(renderer, "bar_top", 0.0f, L.y(0.0f), static_cast<float>(w), L.s);
    }
    if (text != nullptr) {
        const theme::TextStyle& style = options_art::kTitleStyle;
        text->draw(renderer, title, L.x(kTitleX), centred_top(*text, L, 0.0f, kTitleBandHeight, style),
                   style, TextAlign::Left);
    }
}

void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w, std::span<const HintItem> items) {
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
        select_art::layout_hint_items(
            items,
            [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintKey); },
            [text](std::string_view s) { return ref_measure(*text, s, theme::text::kHintWord); }));
}

void draw_message_chip(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                       const theme::LayoutScale& L, std::string_view message) {
    if (message.empty() || text == nullptr) {
        return;
    }
    const Rect rect = remap_chip_rect(ref_measure(*text, message, kChipStyle));
    if (theme != nullptr) {
        theme->draw_slice3(renderer, "chip", L.rect(rect), theme::color::kGold);
    }
    text->draw(renderer, message, L.x(rect.x + rect.w * 0.5f),
               centred_top(*text, L, rect.y, rect.h, kChipStyle), kChipStyle, TextAlign::Centre);
}

void draw_remap_table(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const InputRemapModel& model,
                      bool reset_selected) {
    const std::span<const RemapRow> rows(model.rows);
    const int count = remap_display_count(rows);
    const int selected = reset_selected ? remap_reset_display_index(rows)
                                        : remap_display_index(rows, model.row);
    const select_art::ListWindow window = select_art::list_window(selected, count, kRemapVisibleRows);
    const int selected_slot = selected - window.first;
    auto rect_of = [&](int d) { return remap_row_rect(d - window.first, selected_slot); };

    // Art: headers and navy rows, then the gold selected bar.
    if (theme != nullptr) {
        for (int d = window.first; d <= window.last; ++d) {
            if (d == selected) {
                continue;
            }
            const bool header = remap_display_row(rows, d).kind == RemapDisplayRow::Kind::Header;
            theme->draw_slice3(renderer, header ? "wheel_pack" : "wheel_row", L.rect(rect_of(d)));
        }
        theme->draw_stretch(renderer, "wheel_row_selected", L.rect(rect_of(selected)));
    }
    if (text == nullptr) {
        return;
    }

    // Text grouped by style: headers, unselected actions, unselected keys, the
    // RESET label, then the selected pair.
    for (int d = window.first; d <= window.last; ++d) {
        const RemapDisplayRow row = remap_display_row(rows, d);
        if (row.kind == RemapDisplayRow::Kind::Header) {
            const Rect r = rect_of(d);
            text->draw(renderer, remap_device_label(row.device),
                       L.x(r.x + select_art::kWheelPackTextX),
                       centred_top(*text, L, r.y, r.h, kHeaderStyle), kHeaderStyle, TextAlign::Left);
        }
    }
    for (int d = window.first; d <= window.last; ++d) {
        const RemapDisplayRow row = remap_display_row(rows, d);
        if (d != selected && row.kind == RemapDisplayRow::Kind::Binding) {
            const GameAction action = model.rows[static_cast<std::size_t>(row.binding)].action;
            draw_name(*text, renderer, L, rect_of(d), false, remap_action_label(action),
                      kActionStyle);
        }
    }
    for (int d = window.first; d <= window.last; ++d) {
        const RemapDisplayRow row = remap_display_row(rows, d);
        if (d != selected && row.kind == RemapDisplayRow::Kind::Binding) {
            const GameAction action = model.rows[static_cast<std::size_t>(row.binding)].action;
            draw_key(*text, renderer, L, rect_of(d), false, remap_action_label(action),
                     remap_value_view(model, row.binding), kActionStyle, kKeyStyle);
        }
    }
    const RemapDisplayRow sel = remap_display_row(rows, selected);
    if (sel.kind == RemapDisplayRow::Kind::Reset) {
        draw_name(*text, renderer, L, rect_of(selected), true, kResetLabel, kSelectedStyle);
    } else if (count - 1 <= window.last) {
        draw_name(*text, renderer, L, rect_of(count - 1), false, kResetLabel, kResetStyle);
    }
    if (sel.kind == RemapDisplayRow::Kind::Binding) {
        const Rect bar = rect_of(selected);
        const std::string_view action =
            remap_action_label(model.rows[static_cast<std::size_t>(sel.binding)].action);
        draw_name(*text, renderer, L, bar, true, action, kSelectedStyle);
        draw_key(*text, renderer, L, bar, true, action, remap_value_view(model, sel.binding),
                 kSelectedStyle, kSelectedStyle);
    }
}

void draw_calibration(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const CalibrationView& view) {
    if (theme != nullptr) {
        for (int i = 0; i < 2; ++i) {
            theme->draw_slice3(renderer, "stat_panel", L.rect(calibration_plate_rect(i)));
        }
    }
    if (text == nullptr) {
        return;
    }

    text->draw(renderer, view.phase_word, L.x(kPhaseCentreX),
               centred_top(*text, L, kPhaseTop, kPhaseHeight, kPhaseStyle), kPhaseStyle,
               TextAlign::Centre);

    for (int i = 0; i < 2; ++i) {
        const Rect plate = calibration_plate_rect(i);
        const float centre_y =
            plate.y + results_art::kStatLabelTop + results_art::kStatLabelCapCentre;
        text->draw(renderer, kCalPlateLabels[static_cast<std::size_t>(i)],
                   L.x(plate_centre_x(plate, centre_y)), L.y(plate.y + results_art::kStatLabelTop),
                   kPlateLabelStyle, TextAlign::Centre);
    }

    const std::string_view values[2] = {view.samples, view.offset};
    const theme::TextStyle* styles[2] = {&kPlateValueStyle,
                                         view.offset_ready ? &kPlateValueStyle : &kPlatePendingStyle};
    for (int i = 0; i < 2; ++i) {
        const Rect plate = calibration_plate_rect(i);
        const float baseline = plate.y + results_art::kValueBaseline;
        const theme::TextStyle& style = *styles[i];
        text->draw(renderer, values[i], L.x(plate_centre_x(plate, baseline - kValueCapHalf)),
                   L.y(baseline) - text->ascent(style), style, TextAlign::Centre);
    }

    // Synthetic takes priority: it can never save anyway.
    const std::string_view notice =
        view.synthetic ? kNoAudioNotice : (view.out_of_range ? kOutOfRangeNotice : std::string_view{});
    if (!notice.empty()) {
        text->draw(renderer, notice, L.x(kPhaseCentreX),
                   centred_top(*text, L, kNoticeTop, kNoticeHeight, kNoticeStyle), kNoticeStyle,
                   TextAlign::Centre);
    }
}

} // namespace blaze4k::setup_art
