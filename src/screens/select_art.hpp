#pragma once

// Cabinet v3 Song Select art (#94), used by SelectScreen.
//
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so select_art_test
//    pins them headless. Every rect is a manifest *content box*; glow padding
//    hangs outside it.
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
//
// Presentation only: nothing here reads the music clock or wall time; the wheel
// slide runs on the screen's fixed `dt`.

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

// Top of the hint bar's 2px rule: bar_hint is 52 + 2 px (manifest content 108 @2x).
inline constexpr float kHintBarTop = theme::layout::kRefHeight - theme::layout::kHintBarHeight - 2.0f;
// Top of the hint text band (below the rule) and its height (theme.hpp kHintBarHeight).
inline constexpr float kHintBandTop = kHintBarTop + 2.0f;
inline constexpr float kHintBandHeight = theme::layout::kHintBarHeight;

// Row pitches (theme.hpp row height + gap; the gaps were measured from the mock).
inline constexpr float kDiffRowPitch = theme::layout::kDiffRowHeight + theme::layout::kDiffRowGap;
inline constexpr float kWheelPitch = theme::layout::kWheelRowHeight + theme::layout::kWheelRowGap;

// Visible rows: the most that fit above the hint-bar rule with one selected row
// (asserted against visible_rows() in select_art_test).
inline constexpr int kDiffVisibleRows = 5;
inline constexpr int kWheelVisibleRows = 7;

// Wheel slide: ease-out cubic over 80 ms, clamped to two rows of travel.
inline constexpr double kWheelScrollSeconds = 0.08;
inline constexpr int kWheelMaxSlideRows = 2;
inline constexpr float kWheelScrollMax = static_cast<float>(kWheelMaxSlideRows) * kWheelPitch;

// Difficulty row width (manifest diff_row_* content 1128 @2x).
inline constexpr float kDiffRowWidth = 564.0f;
// Inside a difficulty row, from the row's content x (measured from the mock; the
// manifest notes say 24 / 183 / 216). The meter and ticks sit 16px right of the
// mock (#122): the slanted tab reaches x 167.5 at the cap top of a selected row.
// The name sits 6px right of the mock, 10px in the selected row (#126): the baked
// tab's fill starts at x 12.5 at the name's cap top, at 16 in the selected row.
inline constexpr float kDiffNameX = 21.0f;
inline constexpr float kDiffNameSelectedX = 25.0f;
inline constexpr float kDiffMeterCentreX = 190.0f;
inline constexpr float kDiffTickX = 210.0f;
inline constexpr float kDiffBestRight = 547.0f; // measured from mock: 17px in from the right edge
// The name ends at x 143 in the selected row (25 + 118), inside the tab (baked and
// Edit: it ends at x 161 at mid-height, 164 selected).
inline constexpr float kDiffNameBudget = 118.0f;
inline constexpr int kDiffTickCount = 10;
// diff_tick content 40x36 @2x (manifest); 20px tall in the selected row (manifest note).
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
inline constexpr float kEditTabWidth = 150.0f; // the tab fill: x 11..161 at mid-height, 14..164 selected
inline constexpr Color kEditBody = theme::hex(0x0B1030, 0.85f); // sampled from diff_row_beginner.png
inline constexpr Color kEditBodySelected = theme::hex(0x0B1030, 1.0f);
inline constexpr Color kEditEdge = theme::hex(0x27325A); // the unselected row's 1px ring (sampled)

// Wheel text x from the row's content x (manifest notes) and the right limit.
inline constexpr float kWheelSongTextX = 26.0f;
inline constexpr float kWheelSelectedTextX = 30.0f;
inline constexpr float kWheelPackTextX = 54.0f;
inline constexpr float kWheelTextRight = 1256.0f; // 24px right margin at the column edge

// Top bar: title_select_music at manifest layout_720p (40, 9).
inline constexpr Vec2 kTitleSpritePos{theme::layout::kTopBarPadX, 9.0f};
// Chips (measured from the mock): 40px boxes, 22px text padding, 4px apart,
// the last one ending at kRefWidth - kTopBarPadX.
inline constexpr float kChipTop = 12.0f;
inline constexpr float kChipHeight = 40.0f;
inline constexpr float kChipPadX = 22.0f;
inline constexpr float kChipGap = 4.0f;
inline constexpr float kChipRight = theme::layout::kRefWidth - theme::layout::kTopBarPadX;

// Song info under the banner.
inline constexpr float kInfoWidth = theme::layout::kBanner.w;                       // title/artist budget
inline constexpr float kBpmRight = theme::layout::kBanner.x + theme::layout::kBanner.w - 4.0f; // 604, mock
inline constexpr float kArtistBpmGap = 16.0f;
// Gap between a title and the subtitle drawn after it (wheel rows and info panel, #110).
inline constexpr float kSubtitleGap = 10.0f;

// Hint line (measured from the mock): key -> word gaps, arrow cells.
inline constexpr float kHintCentreX = theme::layout::kRefWidth * 0.5f;
inline constexpr float kHintArrowKeyGap = 10.0f; // after an arrow pair
inline constexpr float kHintTextKeyGap = 8.0f;   // after a text key (its measure has 2px tracking)
inline constexpr float kHintCapBand = 14.0f;     // arrow length = the cap height band
inline constexpr float kHintArrowStem = 2.0f;
inline constexpr float kHintArrowHead = 4.0f;    // head width and length
inline constexpr float kHintVArrowCell = 4.0f;   // up/down cell width
inline constexpr float kHintVArrowPitch = 16.0f; // up and down centres apart
inline constexpr float kHintHArrowCell = 14.0f;  // left/right cell width
inline constexpr float kHintHArrowPitch = 24.0f; // left and right centres apart
// The arrows' vertical centre: the hint band centre (mock caps 687..701).
inline constexpr float kHintArrowCentreY = kHintBandTop + kHintBandHeight * 0.5f;

// Empty library message.
inline constexpr float kEmptyMessageTop = 340.0f;

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

// A window of rows [first, last] around `selected`: centred while it can slide,
// clamped to fill `visible` rows at either end. count <= 0 gives {0, -1}.
struct ListWindow {
    int first = 0;
    int last = -1;
};
[[nodiscard]] ListWindow list_window(int selected, int count, int visible);

// 1 + floor((bottom - top - selected_h) / pitch), at least 1.
[[nodiscard]] int visible_rows(float list_top, float list_bottom, float row_pitch, float selected_h);

// Difficulty row `slot` of the window with the selection at `selected_slot`:
// y = 372 + slot*54 (+8 below the selection), selected rows 52 tall and 14px right.
[[nodiscard]] Rect difficulty_row_rect(int slot, int selected_slot);

// Wheel indent for a distance from the selection (clamped to [0, 3]).
[[nodiscard]] float wheel_indent(int distance);

// Wheel row `slot` (may be negative or past the window for slide rows):
// y = 92 + slot*76 (+30 below the selection), selected 92 tall, x = 676 + indent.
[[nodiscard]] Rect wheel_row_rect(int slot, int selected_slot);

// Wheel display rows: a Pack header before the first song of each pack, then
// its songs, in library order. Navigation moves over songs only.
struct WheelRow {
    enum class Kind { Pack, Song };
    Kind kind = Kind::Song;
    int pack_index = 0;
    int song_index = -1; // -1 for a Pack row
};
struct WheelRows {
    std::vector<WheelRow> rows;
    std::vector<int> song_row; // song index -> display row index
};
// `song_packs[i]` is the pack index of wheel song i (library order, so equal
// packs are adjacent).
[[nodiscard]] WheelRows build_wheel_rows(std::span<const int> song_packs);

// Slide offset at `elapsed` seconds: start * (1 - t)^3, t = clamp(elapsed / 0.08, 0, 1).
[[nodiscard]] float wheel_scroll_offset(float start, double elapsed);
// New slide start after the window's first row moved by `delta_first`:
//  0          -> `current_offset` (a running slide is left alone)
//  +/-1, +/-2 -> clamp(current_offset + delta * 76, -152, 152) (one song step
//                moves the window 2 rows when it crosses a pack header)
//  other      -> 0 (wrap or jump: snap)
[[nodiscard]] float wheel_scroll_start(float current_offset, int delta_first);

// Display rows drawn for `window` while the wheel slides by `offset`: up to
// kWheelMaxSlideRows extra rows on the side the rows moved away from (above
// for offset > 0, below for offset < 0), clamped to [0, count - 1].
[[nodiscard]] ListWindow wheel_slide_range(ListWindow window, int count, float offset);
// Wheel row `slot` drawn with the slide: `offset` px lower, except the gold
// selected bar (slot == selected_slot), which does not slide.
[[nodiscard]] Rect wheel_slide_rect(int slot, int selected_slot, float offset);
// True when a wheel row rect stays inside the reference column's height
// (y 0..720). Rows that leave it are not drawn, so a slide never paints into
// the letterbox bands of a non-16:9 window. At the top a culled row is wholly
// under the 66px top bar; at the bottom at most an 8px strip above the hint
// bar is dropped, for a few ms of the 80 ms slide.
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

// Row label: an Edit chart's name as written (chart_display_label); every other
// label in ASCII upper case ("Hard" -> "HARD"). An empty passthrough label
// shows the resolved difficulty's name.
[[nodiscard]] std::string difficulty_row_label(const Chart& chart);

// Lit ticks for a meter: clamp(meter, 0, 10).
[[nodiscard]] int meter_ticks_lit(int meter);

// Tick `n` (0..9) of `row`: {row.x + 210 + n*18, centred, 20, 18 (20 selected)}.
[[nodiscard]] Rect tick_rect(const Rect& row, bool selected, int n);

// Left edge of the difficulty name in `row`: row.x + 21 (25 selected).
[[nodiscard]] float difficulty_name_x(const Rect& row, bool selected);

// The CSS skewX parallelogram of `rect` about its vertical centre (TL, TR, BR, BL):
// the top edge shifts right by skew*h/2, the bottom edge left by the same.
[[nodiscard]] std::array<Vec2, 4> skewed_quad(const Rect& rect, float skew);

// The code-drawn Edit row's rects for `row`, before the kRows skew (reference px):
//  frame  the ring's outer edge = the baked art's opaque extent:
//         {row.x + 10, row.y, row.w - 20, row.h} (12 / 24 selected)
//  inner  `frame` inset by `border` on every side (1, 2 selected)
//  tab    the first kEditTabWidth of `inner`
//  left, right  the ring's side strips: `border` wide and as tall as `inner`, from
//         the frame's left edge and to its right edge (the top and bottom strips
//         are the first and last `border` of `frame`)
// All share the row's vertical centre, so skewed_quad() keeps their slanted sides
// parallel and `border` px apart. Sizes never go negative.
struct EditRowRects {
    Rect frame;
    Rect inner;
    Rect tab;
    Rect left;
    Rect right;
    float border = 0.0f;
};
[[nodiscard]] EditRowRects edit_row_rects(const Rect& row, bool selected);

// Chip texts from the live config: "SPEED " + format_speed_mod (a malformed
// speed_mod falls back to 1x), and "DOWNSCROLL" / "UPSCROLL". Null config ->
// "SPEED 1x" / "UPSCROLL".
[[nodiscard]] std::string speed_chip_text(const GameConfig* config);
[[nodiscard]] std::string scroll_chip_text(const GameConfig* config);

// Chip boxes for the two texts' widths, right-aligned at kChipRight (speed, scroll).
[[nodiscard]] std::array<Rect, 2> chip_rects(float speed_text_w, float scroll_text_w);

// Hint line pieces (left to right): [up down] SONG [left right] DIFFICULTY
// [ENTER] PLAY [TAB] OPTIONS [ESC] TITLE, centred on x 640.
enum class HintArrow { Up, Down, Left, Right };
struct HintPiece {
    enum class Kind { Arrow, Key, Word };
    Kind kind = Kind::Word;
    HintArrow arrow = HintArrow::Up; // Arrow only
    std::string_view text;           // Key / Word only
    float x = 0.0f;                  // left edge (reference px)
    float width = 0.0f;
};
inline constexpr std::size_t kHintPieceCount = 12;
struct HintLine {
    std::array<HintPiece, kHintPieceCount> pieces{};
    int count = 0;
    float width = 0.0f;
};
// `measure_key` / `measure_word` return reference-px widths of a key / word.
using HintMeasure = std::function<float(std::string_view)>;
[[nodiscard]] HintLine hint_layout(const HintMeasure& measure_key, const HintMeasure& measure_word);

// One legend item (shared by select's and the options overlay's legends):
// VArrows = up/down pair (cell 4, pitch 16), HArrows = left/right pair (cell 14,
// pitch 24), each followed by kHintArrowKeyGap; Key + kHintTextKeyGap; Word +
// kHintGap. The line's last piece gets no trailing gap, whatever its kind.
// `text` is used by Key / Word only.
struct HintItem {
    enum class Kind { VArrows, HArrows, Key, Word };
    Kind kind = Kind::Word;
    std::string_view text;
};
// Lays `items` out left to right and centres the line on kHintCentreX. Pieces
// past kHintPieceCount are dropped (an arrow pair counts as two pieces); an
// arrow pair, or a Key and the Word right after it, is kept or dropped whole.
[[nodiscard]] HintLine layout_hint_items(std::span<const HintItem> items,
                                         const HintMeasure& measure_key,
                                         const HintMeasure& measure_word);

// Solid arrow: a 2px stem the full 14px cap band plus a 4x4 triangle head at
// the pointing end (a quad with two coincident corners). Reference px.
struct ArrowQuads {
    std::array<Vec2, 4> stem{};
    std::array<Vec2, 4> head{};
};
[[nodiscard]] ArrowQuads hint_arrow_quads(HintArrow arrow, Vec2 centre);

// ASCII upper case (UTF-8 bytes >= 0x80 unchanged).
[[nodiscard]] std::string ascii_upper(std::string_view text);

// ---------------------------------------------------------------------------------------------
// Draw helpers (no-ops for null services / an uninitialised renderer)
// ---------------------------------------------------------------------------------------------

// bg_select over the whole window.
void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h);

// bar_top across the window + title_select_music.
void draw_top_bar(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                  int w);

// The speed (kCyan) and scroll (kGreen) chips with their text.
void draw_chips(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, std::string_view speed_text,
                std::string_view scroll_text);

// A laid-out hint line in the hint band: gold code-drawn arrows, then the keys
// (kHintKey) and words (kHintWord). No bar.
void draw_hint_line(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    const HintLine& line);

// bar_hint along the bottom, then the hint line (gold keys and arrows, grey words).
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w);

// The song banner (or banner_fallback when `banner` is null/invalid) in the
// kBanner hole, then banner_frame on top.
void draw_banner(const ThemeTextures* theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                 const Texture* banner);

// Title + subtitle on one line, sharing the 560 budget via fit_title_subtitle
// (the subtitle in kSongSubtitle on the title's baseline, kSubtitleGap after the
// fitted title; an empty subtitle draws the title alone, truncated to 560, and an
// empty title draws the subtitle alone in its place and style),
// artist (truncated to leave room for the BPM) and "BPM <range>" right-aligned
// at x 604.
void draw_song_info(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    std::string_view title, std::string_view subtitle, std::string_view artist,
                    std::string_view bpm);

// One difficulty row's art: the baked slice3, or the code-drawn Edit row (edit_row_rects).
void draw_difficulty_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                             const theme::LayoutScale& L, const Rect& row,
                             const DifficultyRowStyle& style, bool selected);

// The 10 meter ticks of one row.
void draw_ticks(const ThemeTextures& theme, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const Rect& row, bool selected, int meter, const theme::DifficultyColors& colors);

// The visible difficulty rows: art, then ticks, then text grouped by style.
struct DifficultyRowView {
    const Chart* chart = nullptr;
    std::string best; // format_percent(...) or "---"
};
void draw_difficulty_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                          const theme::LayoutScale& L, std::span<const DifficultyRowView> rows,
                          int selected_slot);

// Wheel row art: wheel_pack / wheel_row slice3 running off the window's right
// edge, or the wheel_row_selected bar (stretched only on windows wider than 16:9).
enum class WheelArt { Pack, Song, Selected };
void draw_wheel_row_art(const ThemeTextures& theme, GlQuadRenderer& renderer,
                        const theme::LayoutScale& L, int w, const Rect& row, WheelArt art);

// The wheel rows (slot relative to the window, label already chosen) drawn
// `offset` reference px lower: art first, then text grouped by style.
struct WheelRowView {
    WheelArt art = WheelArt::Song;
    int slot = 0;
    std::string_view label;
    // Song/Selected rows only (Pack rows ignore it): drawn after the label on its
    // baseline in kWheelSubtitle / kWheelSelectedSubtitle, sharing the row's
    // budget via fit_title_subtitle. Empty = the label alone, exactly as before.
    // An empty label draws the subtitle in its place (label style, no gap).
    std::string_view subtitle;
};
void draw_wheel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L, int w, std::span<const WheelRowView> rows,
                int selected_slot, float offset);

// "NO SONGS FOUND" centred at y 340.
void draw_empty_message(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L);

// The scanlines overlay (title_art::draw_scanlines).
void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h,
                    const theme::LayoutScale& L);

} // namespace select_art
} // namespace blaze4k
