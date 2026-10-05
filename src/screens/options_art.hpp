#pragma once

// Cabinet options overlay art (#96), drawn by SelectScreen over song select.
//
// A free design (the Cabinet mock-ups have no options screen) built from select's
// parts: a 72% black scrim, a centred navy panel with a bar_top header ("OPTIONS"),
// the option rows as slanted wheel_row slices with the selection on the gold
// wheel_row_selected bar, and an options legend in the normal bar_hint.
//
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so select_art_test
//    pins them headless. Every rect is a manifest *content box*.
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
//
// Presentation only: nothing here reads the music clock or wall time.

#include <array>
#include <span>
#include <string>

#include "render/geometry.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "screens/options_menu.hpp"
#include "screens/select_art.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;

namespace options_art {

// ---------------------------------------------------------------------------------------------
// Constants (reference px)
// ---------------------------------------------------------------------------------------------

// Header: bar_top content 132 @2x (64 + 2px black rule).
inline constexpr float kHeaderHeight = theme::layout::kTopBarHeight + 2.0f;
// The title's band inside the header (the bar without its rule).
inline constexpr float kHeaderBandHeight = theme::layout::kTopBarHeight;
// Row widths: rows 560 wide, 40px panel padding on each side.
inline constexpr float kRowWidth = 560.0f;
inline constexpr float kPanelPadX = 40.0f;
// Padding between the header and the first row, and below the last row.
inline constexpr float kPanelPadY = 24.0f;
// Rows: wheel_row slice3 (uniform k keeps its slant at any height); the selected
// bar keeps wheel_row_selected's aspect (80/560 vs 92/640, 0.6% off).
inline constexpr float kRowHeight = 48.0f;
inline constexpr float kRowSelectedHeight = 80.0f;
inline constexpr float kRowGap = 10.0f;
inline constexpr float kRowPitch = kRowHeight + kRowGap; // 58
// The panel: width = rows + 2 * padding; height = header + pad + rows + pad, with
// the rows' height for kOptionsRowCount rows and one selected.
inline constexpr float kRowsHeight = static_cast<float>(kOptionsRowCount - 1) * kRowPitch +
                                     kRowSelectedHeight; // 428
inline constexpr float kPanelWidth = kRowWidth + 2.0f * kPanelPadX;                     // 640
inline constexpr float kPanelHeight = kHeaderHeight + kPanelPadY + kRowsHeight + kPanelPadY; // 542
// Centred on x 640, and vertically in the band between select's top bar (66) and
// its hint-bar rule (select_art::kHintBarTop = 666): 66 + (600 - 542) / 2 = 95.
inline constexpr Rect kPanel{
    (theme::layout::kRefWidth - kPanelWidth) * 0.5f,
    kHeaderHeight + (select_art::kHintBarTop - kHeaderHeight - kPanelHeight) * 0.5f, kPanelWidth,
    kPanelHeight};
// Title "OPTIONS": left-aligned 24px into the header.
inline constexpr float kTitleX = kPanel.x + 24.0f; // 344
// SairaExtraBold 44 (the pre-baked kSongTitle atlas) with tracking, echoing the
// wide-tracked title_select_music sprite. Tracking needs no new atlas.
inline constexpr theme::TextStyle kTitleStyle{theme::Font::SairaExtraBold, 44, 4, true,
                                              theme::color::kWhite, theme::Shadow::Hard3};
// Rows: x = panel.x + 40, the first row 24px under the header.
inline constexpr float kRowX = kPanel.x + kPanelPadX;              // 360
inline constexpr float kRowsTop = kPanel.y + kHeaderHeight + kPanelPadY; // 185
// Value right edge, in from the row's right edge: clears the right slant
// (0.249 * h / 2 <= 10px) plus the italic overhang.
inline constexpr float kValueInset = 30.0f;
inline constexpr float kValueSelectedInset = 36.0f;
// Minimum gap between a name's end and its value's start.
inline constexpr float kNameValueGap = 24.0f;
// Panel body: a code-drawn, unslanted navy plate with a 2px ring (stat_panel is
// only 96 tall; scaling it to the panel would blow up its slant and caps).
inline constexpr float kPanelRing = 2.0f;

// The room budget: the panel sits between the two bars, and the last row (with
// one row selected) ends kPanelPadY above the panel's bottom. An eighth row
// fails here until the layout is revisited.
static_assert(kPanel.y >= theme::layout::kTopBarHeight + 2.0f);
static_assert(kPanel.y + kPanel.h <= select_art::kHintBarTop);
static_assert(kRowsTop + static_cast<float>(kOptionsRowCount - 1) * kRowPitch +
                      (kRowSelectedHeight - kRowHeight) + kRowHeight + kPanelPadY ==
              kPanel.y + kPanel.h);

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

// The panel ({320, 95, 640, 542}) and its header ({320, 95, 640, 66}).
[[nodiscard]] Rect panel_rect();
[[nodiscard]] Rect header_rect();

// Option row `row` with `selected` highlighted (both clamped to [0, kOptionsRowCount - 1]):
// {360, 185 + row * 58 (+32 below the selection), 560, 80 selected / 48}.
[[nodiscard]] Rect row_rect(int row, int selected);

// Name left edge (row.x + 26, selected + 30: the wheel's insets) and value right
// edge (row right - 30, selected - 36).
[[nodiscard]] float name_x(const Rect& row, bool selected);
[[nodiscard]] float value_right(const Rect& row, bool selected);

// options_row_name(i) for every row, built once.
[[nodiscard]] const std::array<std::string, kOptionsRowCount>& row_names();

// Legend: [up down] ROW [left right] CHANGE ENTER NEXT ESC CLOSE (10 pieces),
// centred on x 640 in select's hint band.
[[nodiscard]] select_art::HintLine hint_layout(const select_art::HintMeasure& measure_key,
                                               const select_art::HintMeasure& measure_word);

// ---------------------------------------------------------------------------------------------
// Draw helpers (no-ops for null services / an uninitialised renderer)
// ---------------------------------------------------------------------------------------------

// The navy panel body + 2px ring, the bar_top header and the "OPTIONS" title.
void draw_panel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L);

// The option rows: wheel_row art, the gold wheel_row_selected bar, then names
// (left) and values (right-aligned; gold, dark ink on the selected bar) grouped
// by style. Missing values (values.size() < kOptionsRowCount) draw empty.
void draw_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
               const theme::LayoutScale& L, std::span<const std::string> values, int selected);

// bar_hint along the bottom (covering select's legend), then the options legend.
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w);

// The whole overlay: kGameplayScrim over the window, then the panel, the rows
// and the hint bar.
void draw_overlay(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                  const theme::LayoutScale& L, int w, int h, std::span<const std::string> values,
                  int selected);

} // namespace options_art
} // namespace blaze4k
