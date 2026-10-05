#include "screens/select_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "data/config.hpp"
#include "gameplay/speed_mod.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/texture.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/options_menu.hpp"
#include "screens/song_display_text.hpp"
#include "screens/title_art.hpp"

namespace blaze4k::select_art {

namespace layout = theme::layout;

namespace {

// Selected rows are taller; the rows below them move down by the difference.
constexpr float kDiffSelectedExtra = layout::kDiffRowSelectedHeight - layout::kDiffRowHeight;
constexpr float kWheelSelectedExtra = layout::kWheelRowSelectedHeight - layout::kWheelRowHeight;

// Indexed by StepsDifficulty (Beginner..Challenge baked; Edit and Invalid neutral).
constexpr DifficultyRowStyle kRowStyles[] = {
    {StepsDifficulty::Beginner, "diff_row_beginner", "diff_row_beginner_selected",
     theme::difficulty::kBeginner, true},
    {StepsDifficulty::Easy, "diff_row_easy", "diff_row_easy_selected", theme::difficulty::kEasy,
     true},
    {StepsDifficulty::Medium, "diff_row_medium", "diff_row_medium_selected",
     theme::difficulty::kMedium, true},
    {StepsDifficulty::Hard, "diff_row_hard", "diff_row_hard_selected", theme::difficulty::kHard,
     true},
    {StepsDifficulty::Challenge, "diff_row_challenge", "diff_row_challenge_selected",
     theme::difficulty::kChallenge, true},
    {StepsDifficulty::Edit, "", "", theme::difficulty::kEdit, false},
    {StepsDifficulty::Invalid, "", "", theme::difficulty::kEdit, false},
};

constexpr std::string_view kDifficultyNames[] = {"BEGINNER", "EASY", "MEDIUM", "HARD",
                                                 "CHALLENGE", "EDIT", "EDIT"};

// Select's legend: [up down] SONG [left right] DIFFICULTY ENTER PLAY TAB OPTIONS ESC TITLE.
constexpr std::array<HintItem, 10> kSelectHintItems = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::Word, "SONG"},
    {HintItem::Kind::HArrows, {}},
    {HintItem::Kind::Word, "DIFFICULTY"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "PLAY"},
    {HintItem::Kind::Key, "TAB"},
    {HintItem::Kind::Word, "OPTIONS"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "TITLE"},
}};

// Line-box top that centres `style`'s line in a reference band [ref_top, ref_top + ref_h].
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style) {
    return L.y(ref_top) + (L.px(ref_h) - text.line_height(style)) * 0.5f;
}

Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return Vec2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

// The horizontal band [t0, t1] (0 = top, 1 = bottom) of a parallelogram.
std::array<Vec2, 4> quad_band(const std::array<Vec2, 4>& q, float t0, float t1) {
    return {lerp(q[0], q[3], t0), lerp(q[1], q[2], t0), lerp(q[1], q[2], t1), lerp(q[0], q[3], t1)};
}

// A solid quad from reference-space corners.
void draw_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const std::array<Vec2, 4>& ref_corners, Color color) {
    static const Texture kSolid; // id 0: the renderer substitutes its white texture
    const std::array<Vec2, 4> corners = {L.point(ref_corners[0]), L.point(ref_corners[1]),
                                         L.point(ref_corners[2]), L.point(ref_corners[3])};
    renderer.draw_quad_points(corners, kSolid, UVRect{}, {color, color, color, color});
}

// Slice3 rows run off the window's right edge, even on 21:9.
float row_width_to_edge(const theme::LayoutScale& L, int w, float ref_x) {
    return std::max(L.px(layout::kWheelWidth), static_cast<float>(w) - L.x(ref_x));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

ListWindow list_window(int selected, int count, int visible) {
    if (count <= 0) {
        return ListWindow{0, -1};
    }
    visible = std::max(1, visible);
    selected = std::clamp(selected, 0, count - 1);
    int first = selected - visible / 2;
    first = std::clamp(first, 0, std::max(0, count - visible));
    const int last = std::min(count - 1, first + visible - 1);
    return ListWindow{first, last};
}

int visible_rows(float list_top, float list_bottom, float row_pitch, float selected_h) {
    if (!(row_pitch > 0.0f)) {
        return 1;
    }
    const float spare = (list_bottom - list_top - selected_h) / row_pitch;
    if (!(spare > 0.0f)) {
        return 1;
    }
    return 1 + static_cast<int>(std::floor(std::min(spare, 10000.0f)));
}

Rect difficulty_row_rect(int slot, int selected_slot) {
    const bool selected = slot == selected_slot;
    const float y = layout::kDiffListTop + static_cast<float>(slot) * kDiffRowPitch +
                    (slot > selected_slot ? kDiffSelectedExtra : 0.0f);
    const float x = layout::kDiffListX + (selected ? layout::kDiffRowSelectedShiftX : 0.0f);
    return Rect{x, y, kDiffRowWidth,
                selected ? layout::kDiffRowSelectedHeight : layout::kDiffRowHeight};
}

float wheel_indent(int distance) {
    const int d = std::clamp(std::abs(distance), 0, static_cast<int>(layout::kWheelIndent.size()) - 1);
    return layout::kWheelIndent[static_cast<std::size_t>(d)];
}

Rect wheel_row_rect(int slot, int selected_slot) {
    const bool selected = slot == selected_slot;
    const float y = layout::kWheelTop + static_cast<float>(slot) * kWheelPitch +
                    (slot > selected_slot ? kWheelSelectedExtra : 0.0f);
    return Rect{layout::kWheelX + wheel_indent(slot - selected_slot), y, layout::kWheelWidth,
                selected ? layout::kWheelRowSelectedHeight : layout::kWheelRowHeight};
}

WheelRows build_wheel_rows(std::span<const int> song_packs) {
    WheelRows out;
    out.rows.reserve(song_packs.size() * 2);
    out.song_row.reserve(song_packs.size());
    for (std::size_t i = 0; i < song_packs.size(); ++i) {
        const int pack = song_packs[i];
        if (i == 0 || song_packs[i - 1] != pack) {
            out.rows.push_back(WheelRow{WheelRow::Kind::Pack, pack, -1});
        }
        out.song_row.push_back(static_cast<int>(out.rows.size()));
        out.rows.push_back(WheelRow{WheelRow::Kind::Song, pack, static_cast<int>(i)});
    }
    return out;
}

float wheel_scroll_offset(float start, double elapsed) {
    if (!std::isfinite(start) || !std::isfinite(elapsed)) {
        return 0.0f;
    }
    const double t = std::clamp(elapsed / kWheelScrollSeconds, 0.0, 1.0);
    const double remain = 1.0 - t;
    return static_cast<float>(static_cast<double>(start) * remain * remain * remain);
}

float wheel_scroll_start(float current_offset, int delta_first) {
    if (delta_first == 0) {
        return current_offset;
    }
    if (std::abs(delta_first) <= kWheelMaxSlideRows) {
        return std::clamp(current_offset + static_cast<float>(delta_first) * kWheelPitch,
                          -kWheelScrollMax, kWheelScrollMax);
    }
    return 0.0f;
}

ListWindow wheel_slide_range(ListWindow window, int count, float offset) {
    const int extra_above = offset > 0.0f ? kWheelMaxSlideRows : 0;
    const int extra_below = offset < 0.0f ? kWheelMaxSlideRows : 0;
    return ListWindow{std::max(0, window.first - extra_above),
                      std::min(count - 1, window.last + extra_below)};
}

Rect wheel_slide_rect(int slot, int selected_slot, float offset) {
    Rect r = wheel_row_rect(slot, selected_slot);
    if (slot != selected_slot) {
        r.y += offset;
    }
    return r;
}

bool wheel_row_in_column(const Rect& row) {
    return row.y >= 0.0f && row.y + row.h <= layout::kRefHeight;
}

DifficultyRowStyle difficulty_row_style(const Chart& chart) {
    const StepsDifficulty kind = resolve_difficulty(chart.difficulty, chart.description, chart.meter);
    const auto index = static_cast<std::size_t>(kind);
    return index < std::size(kRowStyles) ? kRowStyles[index] : kRowStyles[std::size(kRowStyles) - 1];
}

std::string difficulty_row_label(const Chart& chart) {
    const StepsDifficulty kind = resolve_difficulty(chart.difficulty, chart.description, chart.meter);
    if (kind == StepsDifficulty::Edit && !chart.description.empty()) {
        return chart_display_label(chart); // the chart's name, as written
    }
    if (chart.difficulty.empty()) {
        const auto index = static_cast<std::size_t>(kind);
        return std::string(index < std::size(kDifficultyNames) ? kDifficultyNames[index] : "EDIT");
    }
    return ascii_upper(chart.difficulty);
}

int meter_ticks_lit(int meter) {
    return std::clamp(meter, 0, kDiffTickCount);
}

Rect tick_rect(const Rect& row, bool selected, int n) {
    const float h = selected ? kTickSelectedHeight : kTickHeight;
    return Rect{row.x + kDiffTickX + static_cast<float>(n) * layout::kDiffTickPitch,
                row.y + (row.h - h) * 0.5f, kTickWidth, h};
}

std::array<Vec2, 4> skewed_quad(const Rect& r, float skew) {
    const float shift = skew * r.h * 0.5f;
    return {Vec2{r.x + shift, r.y}, Vec2{r.x + r.w + shift, r.y},
            Vec2{r.x + r.w - shift, r.y + r.h}, Vec2{r.x - shift, r.y + r.h}};
}

std::string speed_chip_text(const GameConfig* config) {
    SpeedMod mod{};
    if (config == nullptr || !parse_speed_mod(config->gameplay.speed_mod, mod)) {
        mod = SpeedMod{};
    }
    return "SPEED " + format_speed_mod(mod);
}

std::string scroll_chip_text(const GameConfig* config) {
    return (config != nullptr && config->gameplay.scroll == "down") ? "DOWNSCROLL" : "UPSCROLL";
}

std::array<Rect, 2> chip_rects(float speed_text_w, float scroll_text_w) {
    const float scroll_w = std::max(0.0f, scroll_text_w) + 2.0f * kChipPadX;
    const float speed_w = std::max(0.0f, speed_text_w) + 2.0f * kChipPadX;
    const float scroll_x = kChipRight - scroll_w;
    const float speed_x = scroll_x - kChipGap - speed_w;
    return {Rect{speed_x, kChipTop, speed_w, kChipHeight},
            Rect{scroll_x, kChipTop, scroll_w, kChipHeight}};
}

HintLine layout_hint_items(std::span<const HintItem> items, const HintMeasure& measure_key,
                           const HintMeasure& measure_word) {
    // How many items fit in kHintPieceCount pieces (an arrow pair takes two). A
    // Key and the Word right after it fit together or not at all.
    std::size_t fitted = 0;
    std::size_t pieces = 0;
    while (fitted < items.size()) {
        const HintItem& item = items[fitted];
        const bool arrows =
            item.kind == HintItem::Kind::VArrows || item.kind == HintItem::Kind::HArrows;
        const bool key_word = item.kind == HintItem::Kind::Key && fitted + 1 < items.size() &&
                              items[fitted + 1].kind == HintItem::Kind::Word;
        const std::size_t need = (arrows || key_word) ? 2 : 1;
        if (pieces + need > kHintPieceCount) {
            break;
        }
        pieces += need;
        fitted += key_word ? 2 : 1;
    }

    HintLine line;
    float x = 0.0f;
    float gap = 0.0f; // the gap after the piece placed last; dropped at the end
    auto add = [&line](HintPiece piece) {
        line.pieces[static_cast<std::size_t>(line.count++)] = piece;
    };
    auto add_arrows = [&](HintArrow a, HintArrow b, float cell, float pitch) {
        add(HintPiece{HintPiece::Kind::Arrow, a, {}, x, cell});
        add(HintPiece{HintPiece::Kind::Arrow, b, {}, x + pitch, cell});
        gap = kHintArrowKeyGap;
        x += pitch + cell + gap;
    };
    auto add_key = [&](std::string_view key) {
        const float w = measure_key ? measure_key(key) : 0.0f;
        add(HintPiece{HintPiece::Kind::Key, HintArrow::Up, key, x, w});
        gap = kHintTextKeyGap;
        x += w + gap;
    };
    auto add_word = [&](std::string_view word) {
        const float w = measure_word ? measure_word(word) : 0.0f;
        add(HintPiece{HintPiece::Kind::Word, HintArrow::Up, word, x, w});
        gap = layout::kHintGap;
        x += w + gap;
    };

    for (std::size_t i = 0; i < fitted; ++i) {
        const HintItem& item = items[i];
        switch (item.kind) {
        case HintItem::Kind::VArrows:
            add_arrows(HintArrow::Up, HintArrow::Down, kHintVArrowCell, kHintVArrowPitch);
            break;
        case HintItem::Kind::HArrows:
            add_arrows(HintArrow::Left, HintArrow::Right, kHintHArrowCell, kHintHArrowPitch);
            break;
        case HintItem::Kind::Key:
            add_key(item.text);
            break;
        case HintItem::Kind::Word:
            add_word(item.text);
            break;
        }
    }
    x -= gap; // no trailing gap, whatever kind of piece ends the line

    line.width = x;
    const float start = kHintCentreX - x * 0.5f;
    for (int i = 0; i < line.count; ++i) {
        line.pieces[static_cast<std::size_t>(i)].x += start;
    }
    return line;
}

HintLine hint_layout(const HintMeasure& measure_key, const HintMeasure& measure_word) {
    return layout_hint_items(kSelectHintItems, measure_key, measure_word);
}

ArrowQuads hint_arrow_quads(HintArrow arrow, Vec2 c) {
    const float half = kHintCapBand * 0.5f;
    const float stem = kHintArrowStem * 0.5f;
    const float head = kHintArrowHead;
    const float head_half = kHintArrowHead * 0.5f;
    ArrowQuads q;
    switch (arrow) {
    case HintArrow::Up:
        q.stem = {Vec2{c.x - stem, c.y - half}, Vec2{c.x + stem, c.y - half},
                  Vec2{c.x + stem, c.y + half}, Vec2{c.x - stem, c.y + half}};
        q.head = {Vec2{c.x, c.y - half}, Vec2{c.x, c.y - half},
                  Vec2{c.x + head_half, c.y - half + head}, Vec2{c.x - head_half, c.y - half + head}};
        break;
    case HintArrow::Down:
        q.stem = {Vec2{c.x - stem, c.y - half}, Vec2{c.x + stem, c.y - half},
                  Vec2{c.x + stem, c.y + half}, Vec2{c.x - stem, c.y + half}};
        q.head = {Vec2{c.x - head_half, c.y + half - head}, Vec2{c.x + head_half, c.y + half - head},
                  Vec2{c.x, c.y + half}, Vec2{c.x, c.y + half}};
        break;
    case HintArrow::Left:
        q.stem = {Vec2{c.x - half, c.y - stem}, Vec2{c.x + half, c.y - stem},
                  Vec2{c.x + half, c.y + stem}, Vec2{c.x - half, c.y + stem}};
        q.head = {Vec2{c.x - half, c.y}, Vec2{c.x - half + head, c.y - head_half},
                  Vec2{c.x - half + head, c.y + head_half}, Vec2{c.x - half, c.y}};
        break;
    case HintArrow::Right:
        q.stem = {Vec2{c.x - half, c.y - stem}, Vec2{c.x + half, c.y - stem},
                  Vec2{c.x + half, c.y + stem}, Vec2{c.x - half, c.y + stem}};
        q.head = {Vec2{c.x + half - head, c.y - head_half}, Vec2{c.x + half, c.y},
                  Vec2{c.x + half, c.y}, Vec2{c.x + half - head, c.y + head_half}};
        break;
    }
    return q;
}

std::string ascii_upper(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------------------------

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h) {
    theme.draw_stretch(renderer, "bg_select",
                       Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)});
}

void draw_top_bar(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                  int w) {
    theme.draw_stretch_x(renderer, "bar_top", 0.0f, L.y(0.0f), static_cast<float>(w), L.s);
    theme.draw_sprite(renderer, "title_select_music", L.point(kTitleSpritePos), L.s);
}

void draw_chips(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, std::string_view speed_text,
                std::string_view scroll_text) {
    const theme::TextStyle& style = theme::text::kChip;
    const float speed_w = text != nullptr ? ref_measure(*text, speed_text, style) : 0.0f;
    const float scroll_w = text != nullptr ? ref_measure(*text, scroll_text, style) : 0.0f;
    const std::array<Rect, 2> rects = chip_rects(speed_w, scroll_w);
    const Color tints[2] = {theme::color::kCyan, theme::color::kGreen};
    const std::string_view labels[2] = {speed_text, scroll_text};
    if (theme != nullptr) {
        for (std::size_t i = 0; i < 2; ++i) {
            theme->draw_slice3(renderer, "chip", L.rect(rects[i]), tints[i]);
        }
    }
    if (text != nullptr) {
        for (std::size_t i = 0; i < 2; ++i) {
            const theme::TextStyle tinted = with_color(style, tints[i]);
            text->draw(renderer, labels[i], L.x(rects[i].x + rects[i].w * 0.5f),
                       centred_top(*text, L, rects[i].y, rects[i].h, tinted), tinted,
                       TextAlign::Centre);
        }
    }
}

void draw_hint_line(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    const HintLine& line) {
    for (int i = 0; i < line.count; ++i) {
        const HintPiece& piece = line.pieces[static_cast<std::size_t>(i)];
        if (piece.kind != HintPiece::Kind::Arrow) {
            continue;
        }
        const ArrowQuads q =
            hint_arrow_quads(piece.arrow, Vec2{piece.x + piece.width * 0.5f, kHintArrowCentreY});
        draw_solid(renderer, L, q.stem, theme::color::kGold);
        draw_solid(renderer, L, q.head, theme::color::kGold);
    }
    // Keys and words share one font and size, so the atlas stays bound.
    for (int i = 0; i < line.count; ++i) {
        const HintPiece& piece = line.pieces[static_cast<std::size_t>(i)];
        if (piece.kind == HintPiece::Kind::Arrow) {
            continue;
        }
        const theme::TextStyle& style =
            piece.kind == HintPiece::Kind::Key ? theme::text::kHintKey : theme::text::kHintWord;
        text.draw(renderer, piece.text, L.x(piece.x),
                  centred_top(text, L, kHintBandTop, kHintBandHeight, style), style,
                  TextAlign::Left);
    }
}

void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w) {
    if (theme != nullptr) {
        const float bar_h = theme->content_size("bar_hint", L.s).y;
        theme->draw_stretch_x(renderer, "bar_hint", 0.0f, L.y(layout::kRefHeight) - bar_h,
                              static_cast<float>(w), L.s);
    }
    if (text == nullptr) {
        return;
    }
    draw_hint_line(*text, renderer, L,
                   hint_layout(
                       [text](std::string_view st) {
                           return ref_measure(*text, st, theme::text::kHintKey);
                       },
                       [text](std::string_view st) {
                           return ref_measure(*text, st, theme::text::kHintWord);
                       }));
}

void draw_banner(const ThemeTextures* theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                 const Texture* banner) {
    const Rect hole = L.rect(layout::kBanner);
    if (banner != nullptr && banner->valid()) {
        renderer.draw_textured_quad(hole, *banner, UVRect{}, Color{});
    } else if (theme != nullptr) {
        theme->draw_stretch(renderer, "banner_fallback", hole);
    } else {
        renderer.draw_quad(hole, theme::color::kNavyPanel);
    }
    if (theme != nullptr) {
        theme->draw_frame(renderer, "banner_frame", hole);
    }
}

void draw_song_info(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    std::string_view title, std::string_view artist, std::string_view bpm) {
    const theme::TextStyle& title_style = theme::text::kSongTitle;
    const theme::TextStyle& artist_style = theme::text::kArtist;
    const theme::TextStyle& bpm_style = theme::text::kBpm;

    text.draw(renderer, text.truncate(title, title_style, L.px(kInfoWidth)), L.x(layout::kInfoX),
              L.y(layout::kSongTitleTop), title_style, TextAlign::Left);

    const float bpm_w = text.measure(bpm, bpm_style);
    const float artist_budget = std::max(0.0f, L.px(kInfoWidth) - bpm_w - L.px(kArtistBpmGap));
    text.draw(renderer, text.truncate(artist, artist_style, artist_budget), L.x(layout::kInfoX),
              L.y(layout::kArtistTop), artist_style, TextAlign::Left);
    text.draw(renderer, bpm, L.x(kBpmRight), L.y(layout::kArtistTop), bpm_style, TextAlign::Right);
}

void draw_difficulty_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                             const theme::LayoutScale& L, const Rect& row,
                             const DifficultyRowStyle& style, bool selected) {
    if (style.baked) {
        theme.draw_slice3(renderer, selected ? style.texture_selected : style.texture, L.rect(row));
        return;
    }
    // Code-drawn Edit row (#84): the baked rows' slanted body, edges and tab.
    const float k = theme::skew::kRows;
    if (selected) {
        const float o = kEditSelectedOutset;
        draw_solid(renderer, L, skewed_quad(Rect{row.x - o, row.y - o, row.w + 2 * o, row.h + 2 * o}, k),
                   theme::color::kGold);
    }
    const std::array<Vec2, 4> body = skewed_quad(row, k);
    draw_solid(renderer, L, body, selected ? kEditBodySelected : kEditBody);
    if (row.h > 2.0f) {
        const float edge = 1.0f / row.h;
        draw_solid(renderer, L, quad_band(body, 0.0f, edge), kEditEdge);
        draw_solid(renderer, L, quad_band(body, 1.0f - edge, 1.0f), kEditEdge);
    }
    draw_solid(renderer, L, skewed_quad(Rect{row.x, row.y, kEditTabWidth, row.h}, k),
               style.colors.fill);
}

void draw_ticks(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const Rect& row, bool selected, int meter, const theme::DifficultyColors& colors) {
    const int lit = meter_ticks_lit(meter);
    for (int n = 0; n < kDiffTickCount; ++n) {
        theme.draw_stretch(renderer, "diff_tick", L.rect(tick_rect(row, selected, n)),
                           n < lit ? colors.fill : theme::color::kTickOff);
    }
}

void draw_difficulty_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                          const theme::LayoutScale& L, std::span<const DifficultyRowView> rows,
                          int selected_slot) {
    if (theme != nullptr) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].chart == nullptr) {
                continue;
            }
            const int slot = static_cast<int>(i);
            draw_difficulty_row_art(*theme, renderer, L, difficulty_row_rect(slot, selected_slot),
                                    difficulty_row_style(*rows[i].chart), slot == selected_slot);
        }
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].chart == nullptr) {
                continue;
            }
            const int slot = static_cast<int>(i);
            draw_ticks(*theme, renderer, L, difficulty_row_rect(slot, selected_slot),
                       slot == selected_slot, rows[i].chart->meter,
                       difficulty_row_style(*rows[i].chart).colors);
        }
    }
    if (text == nullptr) {
        return;
    }
    // Text grouped by style: names, then meters, then bests.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].chart == nullptr) {
            continue;
        }
        const int slot = static_cast<int>(i);
        const bool selected = slot == selected_slot;
        const Rect row = difficulty_row_rect(slot, selected_slot);
        const theme::TextStyle style =
            with_color(selected ? theme::text::kDiffNameSelected : theme::text::kDiffName,
                       difficulty_row_style(*rows[i].chart).colors.ink);
        text->draw(renderer,
                   text->truncate(difficulty_row_label(*rows[i].chart), style, L.px(kDiffNameBudget)),
                   L.x(row.x + kDiffNameX), centred_top(*text, L, row.y, row.h, style), style,
                   TextAlign::Left);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].chart == nullptr) {
            continue;
        }
        const int slot = static_cast<int>(i);
        const bool selected = slot == selected_slot;
        const Rect row = difficulty_row_rect(slot, selected_slot);
        const theme::TextStyle& style =
            selected ? theme::text::kDiffMeterSelected : theme::text::kDiffMeter;
        text->draw(renderer, std::to_string(rows[i].chart->meter), L.x(row.x + kDiffMeterCentreX),
                   centred_top(*text, L, row.y, row.h, style), style, TextAlign::Centre);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].chart == nullptr) {
            continue;
        }
        const int slot = static_cast<int>(i);
        const bool selected = slot == selected_slot;
        const Rect row = difficulty_row_rect(slot, selected_slot);
        const theme::TextStyle& style =
            selected ? theme::text::kDiffBestSelected : theme::text::kDiffBest;
        text->draw(renderer, rows[i].best, L.x(row.x + kDiffBestRight),
                   centred_top(*text, L, row.y, row.h, style), style, TextAlign::Right);
    }
}

void draw_wheel_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                        const theme::LayoutScale& L, int w, const Rect& row, WheelArt art) {
    const Rect dst{L.x(row.x), L.y(row.y), row_width_to_edge(L, w, row.x), L.px(row.h)};
    switch (art) {
    case WheelArt::Pack:
        theme.draw_slice3(renderer, "wheel_pack", dst);
        break;
    case WheelArt::Song:
        theme.draw_slice3(renderer, "wheel_row", dst);
        break;
    case WheelArt::Selected:
        theme.draw_stretch(renderer, "wheel_row_selected", dst);
        break;
    }
}

void draw_wheel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, int w, std::span<const WheelRowView> rows,
                int selected_slot, float offset) {
    // The gold selected bar does not slide; every other row is drawn `offset`
    // lower. Rows that slide out of reference y 0..720 are skipped (art and text).
    auto row_rect = [&](const WheelRowView& view) {
        return wheel_slide_rect(view.slot, selected_slot, offset);
    };
    auto drawn = [&](const WheelRowView& view) { return wheel_row_in_column(row_rect(view)); };
    auto draw_text_pass = [&](WheelArt art) {
        if (text == nullptr) {
            return;
        }
        const theme::TextStyle& style = art == WheelArt::Selected ? theme::text::kWheelSelected
                                        : art == WheelArt::Pack   ? theme::text::kWheelPack
                                                                  : theme::text::kWheelRow;
        const float text_dx = art == WheelArt::Selected ? kWheelSelectedTextX
                              : art == WheelArt::Pack   ? kWheelPackTextX
                                                        : kWheelSongTextX;
        for (const WheelRowView& view : rows) {
            if (view.art != art || !drawn(view)) {
                continue;
            }
            const Rect r = row_rect(view);
            const float text_x = r.x + text_dx;
            const float budget = std::max(0.0f, L.px(kWheelTextRight - text_x));
            text->draw(renderer, text->truncate(view.label, style, budget), L.x(text_x),
                       centred_top(*text, L, r.y, r.h, style), style, TextAlign::Left);
        }
    };

    // Sliding rows (art, then text), then the selected bar on top of them.
    if (theme != nullptr) {
        for (const WheelRowView& view : rows) {
            if (view.art != WheelArt::Selected && drawn(view)) {
                draw_wheel_row_art(*theme, renderer, L, w, row_rect(view), view.art);
            }
        }
    }
    draw_text_pass(WheelArt::Song);
    draw_text_pass(WheelArt::Pack);
    if (theme != nullptr) {
        for (const WheelRowView& view : rows) {
            if (view.art == WheelArt::Selected && drawn(view)) {
                draw_wheel_row_art(*theme, renderer, L, w, row_rect(view), view.art);
            }
        }
    }
    draw_text_pass(WheelArt::Selected);
}

void draw_empty_message(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L) {
    text.draw(renderer, "NO SONGS FOUND", L.x(kHintCentreX), L.y(kEmptyMessageTop),
              theme::text::kWheelRow, TextAlign::Centre);
}

void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h,
                    const theme::LayoutScale& L) {
    title_art::draw_scanlines(theme, renderer, w, h, L.s);
}

} // namespace blaze4k::select_art
