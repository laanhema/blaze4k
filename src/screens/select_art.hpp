#pragma once

// Cabinet v3 Song Select art (#94), used by SelectScreen.

#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "chart/chart.hpp"
#include "render/geometry.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;
struct GameConfig;
class Texture;

namespace select_art {

// ---------------------------------------------------------------------------------------------
// Constants (reference px unless noted)
// ---------------------------------------------------------------------------------------------

inline constexpr float kHintBarTop = theme::layout::kRefHeight - theme::layout::kHintBarHeight - 2.0f;
inline constexpr float kHintBandTop = kHintBarTop + 2.0f;
inline constexpr float kHintBandHeight = theme::layout::kHintBarHeight;

inline constexpr float kDiffRowPitch = theme::layout::kDiffRowHeight + theme::layout::kDiffRowGap;
inline constexpr float kWheelPitch = theme::layout::kWheelRowHeight + theme::layout::kWheelRowGap;

inline constexpr int kDiffVisibleRows = 5;
inline constexpr int kWheelVisibleRows = 7;

inline constexpr double kWheelScrollSeconds = 0.08;
inline constexpr int kWheelMaxSlideRows = 2;
inline constexpr float kWheelScrollMax = static_cast<float>(kWheelMaxSlideRows) * kWheelPitch;

inline constexpr float kDiffRowWidth = 564.0f;
// Inside a difficulty row, from the row's content x (measured from the mock; the
// manifest notes say 24 / 183 / 216). The meter and ticks sit 16px right of the
// mock (#122): the slanted tab reaches x 167.5 at the cap top of a selected row.
// The name sits 6px right of the mock, 10px in the selected row (#126): the baked
// tab's fill starts at x 12.5 at the name's cap top, at 16 in the selected row.
// The best % ends 10px left of the mock, 14px in the selected row (#131): the baked
// outline's inner edge is at x 553 at mid-height, 550 in the selected row (the mock's
// is at 563), and it slants 1.5px closer at the text's baseline.
inline constexpr float kDiffNameX = 21.0f;
inline constexpr float kDiffNameSelectedX = 25.0f;
inline constexpr float kDiffMeterCentreX = 190.0f;
inline constexpr float kDiffTickX = 210.0f;
inline constexpr float kDiffBestRight = 537.0f;
inline constexpr float kDiffBestSelectedRight = 533.0f;
inline constexpr float kDiffNameBudget = 118.0f;
inline constexpr int kDiffTickCount = 10;
inline constexpr float kTickWidth = 20.0f;
inline constexpr float kTickHeight = 18.0f;
inline constexpr float kTickSelectedHeight = 20.0f;

// Code-drawn Edit row (#84; there is no diff_row_edit texture), matched to the baked
// rows' art (#130). Measured from diff_row_*.png / diff_row_*_selected.png (all five
// colours agree): the art is inset inside the 564px content box. At mid-height the
// 1px kEditEdge ring spans x 10..554; the selected row's 2px gold ring spans x 12..552
// and stays inside the content box (no outset). The tab is 150px wide inside the ring.
inline constexpr float kEditInsetX = 10.0f;
inline constexpr float kEditSelectedInsetX = 12.0f;
inline constexpr float kEditBorder = 1.0f;
inline constexpr float kEditSelectedBorder = 2.0f;
inline constexpr float kEditTabWidth = 150.0f;
inline constexpr Color kEditBody = theme::hex(0x0B1030, 0.85f);
inline constexpr Color kEditBodySelected = theme::hex(0x0B1030, 1.0f);
inline constexpr Color kEditEdge = theme::hex(0x27325A);

inline constexpr float kWheelSongTextX = 26.0f;
inline constexpr float kWheelSelectedTextX = 30.0f;
inline constexpr float kWheelPackTextX = 54.0f;
inline constexpr float kWheelMarkerX = 36.0f;
inline constexpr float kWheelMarkerHalf = 9.0f;
inline constexpr float kWheelTextRight = 1256.0f;

inline constexpr Vec2 kTitleSpritePos{theme::layout::kTopBarPadX, 9.0f};
inline constexpr float kChipTop = 12.0f;
inline constexpr float kChipHeight = 40.0f;
inline constexpr float kChipPadX = 22.0f;
inline constexpr float kChipGap = 4.0f;
inline constexpr float kChipRight = theme::layout::kRefWidth - theme::layout::kTopBarPadX;

inline constexpr float kInfoWidth = theme::layout::kBanner.w;
inline constexpr float kBpmRight = theme::layout::kBanner.x + theme::layout::kBanner.w - 4.0f;
inline constexpr float kArtistBpmGap = 16.0f;
inline constexpr float kSubtitleGap = 10.0f;

inline constexpr float kHintCentreX = theme::layout::kRefWidth * 0.5f;
inline constexpr float kHintArrowKeyGap = 10.0f;
inline constexpr float kHintTextKeyGap = 8.0f;
inline constexpr float kHintCapBand = 14.0f;
inline constexpr float kHintArrowStem = 2.0f;
inline constexpr float kHintArrowHead = 4.0f;
inline constexpr float kHintVArrowCell = 4.0f;
inline constexpr float kHintVArrowPitch = 16.0f;
inline constexpr float kHintHArrowCell = 14.0f;
inline constexpr float kHintHArrowPitch = 24.0f;
inline constexpr float kHintArrowCentreY = kHintBandTop + kHintBandHeight * 0.5f;

inline constexpr float kEmptyMessageTop = 340.0f;

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

struct ListWindow {
    int first = 0;
    int last = -1;
};
[[nodiscard]] ListWindow list_window(int selected, int count, int visible);

[[nodiscard]] int visible_rows(float list_top, float list_bottom, float row_pitch, float selected_h);

[[nodiscard]] Rect difficulty_row_rect(int slot, int selected_slot);
[[nodiscard]] float wheel_indent(int distance);
[[nodiscard]] Rect wheel_row_rect(int slot, int selected_slot);

struct WheelRow {
    enum class Kind { Pack, Song };
    Kind kind = Kind::Song;
    int pack_index = 0;
    int song_index = -1;
};
struct WheelRows {
    std::vector<WheelRow> rows;
    std::vector<int> song_row;
};
[[nodiscard]] WheelRows build_wheel_rows(std::span<const int> song_packs, int open_pack);

[[nodiscard]] float wheel_scroll_offset(float start, double elapsed);
[[nodiscard]] float wheel_scroll_start(float current_offset, int delta_first);

[[nodiscard]] ListWindow wheel_slide_range(ListWindow window, int count, float offset);
[[nodiscard]] Rect wheel_slide_rect(int slot, int selected_slot, float offset);
[[nodiscard]] bool wheel_row_in_column(const Rect& row);

// Row art and colours for a chart, classified the OpenITG way (resolve_difficulty)
// while the label stays the simfile's own. Edit and Invalid get the neutral,
// code-drawn Edit row (baked = false, empty texture names).
struct DifficultyRowStyle {
    StepsDifficulty kind = StepsDifficulty::Edit;
    std::string_view texture;
    std::string_view texture_selected;
    theme::DifficultyColors colors{};
    bool baked = false;
};
[[nodiscard]] DifficultyRowStyle difficulty_row_style(const Chart& chart);
[[nodiscard]] std::string difficulty_row_label(const Chart& chart);

[[nodiscard]] int meter_ticks_lit(int meter);
[[nodiscard]] Rect tick_rect(const Rect& row, bool selected, int n);
[[nodiscard]] float difficulty_name_x(const Rect& row, bool selected);
[[nodiscard]] float difficulty_best_right(const Rect& row, bool selected);

[[nodiscard]] std::array<Vec2, 4> skewed_quad(const Rect& rect, float skew);

[[nodiscard]] std::array<Vec2, 4> quad_band(const std::array<Vec2, 4>& q, float t0, float t1);

struct EditRowRects {
    Rect frame;
    Rect inner;
    Rect tab;
    Rect left;
    Rect right;
    float border = 0.0f;
};
[[nodiscard]] EditRowRects edit_row_rects(const Rect& row, bool selected);

[[nodiscard]] std::string speed_chip_text(const GameConfig* config);
[[nodiscard]] std::string scroll_chip_text(const GameConfig* config);

[[nodiscard]] std::array<Rect, 2> chip_rects(float speed_text_w, float scroll_text_w);

enum class HintArrow { Up, Down, Left, Right };
struct HintPiece {
    enum class Kind { Arrow, Key, Word };
    Kind kind = Kind::Word;
    HintArrow arrow = HintArrow::Up;
    std::string_view text;
    float x = 0.0f;
    float width = 0.0f;
};
inline constexpr std::size_t kHintPieceCount = 12;
struct HintLine {
    std::array<HintPiece, kHintPieceCount> pieces{};
    int count = 0;
    float width = 0.0f;
};
using HintMeasure = std::function<float(std::string_view)>;
enum class ConfirmHint { Play, Open, Close };
[[nodiscard]] std::string_view confirm_hint_word(ConfirmHint confirm);
[[nodiscard]] HintLine hint_layout(const HintMeasure& measure_key, const HintMeasure& measure_word,
                                   ConfirmHint confirm);

struct HintItem {
    enum class Kind { VArrows, HArrows, Key, Word };
    Kind kind = Kind::Word;
    std::string_view text;
};
[[nodiscard]] HintLine layout_hint_items(std::span<const HintItem> items,
                                         const HintMeasure& measure_key,
                                         const HintMeasure& measure_word);

struct ArrowQuads {
    std::array<Vec2, 4> stem{};
    std::array<Vec2, 4> head{};
};
[[nodiscard]] ArrowQuads hint_arrow_quads(HintArrow arrow, Vec2 centre);

[[nodiscard]] std::array<Vec2, 4> wheel_marker_quad(const Rect& row, bool open);
void draw_pack_marker(GlQuadRenderer& renderer, const theme::LayoutScale& L, const Rect& row,
                      bool open, Color ink);

[[nodiscard]] std::string ascii_upper(std::string_view text);

// ---------------------------------------------------------------------------------------------
// Draw helpers (no-ops for null services / an uninitialised renderer)
// ---------------------------------------------------------------------------------------------

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h);
void draw_top_bar(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                  int w);
void draw_chips(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, std::string_view speed_text,
                std::string_view scroll_text);

void draw_hint_line(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    const HintLine& line);
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w, ConfirmHint confirm);
void draw_banner(const ThemeTextures* theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                 const Texture* banner);
void draw_song_info(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    std::string_view title, std::string_view subtitle, std::string_view artist,
                    std::string_view bpm);
void draw_difficulty_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                             const theme::LayoutScale& L, const Rect& row,
                             const DifficultyRowStyle& style, bool selected);
void draw_ticks(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const Rect& row, bool selected, int meter, const theme::DifficultyColors& colors);

struct DifficultyRowView {
    const Chart* chart = nullptr;
    std::string best;
};
void draw_difficulty_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                          const theme::LayoutScale& L, std::span<const DifficultyRowView> rows,
                          int selected_slot);

enum class WheelArt { Pack, Song, Selected, SelectedPack };
void draw_wheel_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                        const theme::LayoutScale& L, int w, const Rect& row, WheelArt art);

struct WheelRowView {
    WheelArt art = WheelArt::Song;
    int slot = 0;
    std::string_view label;
    std::string_view subtitle;
    bool open = false;
};
void draw_wheel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, int w, std::span<const WheelRowView> rows,
                int selected_slot, float offset);

void draw_empty_message(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L);
void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h,
                    const theme::LayoutScale& L);

} // namespace select_art
} // namespace blaze4k
