#pragma once

// Cabinet options overlay art (#96), drawn by SelectScreen over song select.

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

inline constexpr float kHeaderHeight = theme::layout::kTopBarHeight + 2.0f;
inline constexpr float kHeaderBandHeight = theme::layout::kTopBarHeight;
inline constexpr float kRowWidth = 560.0f;
inline constexpr float kPanelPadX = 40.0f;
inline constexpr float kPanelPadY = 24.0f;
inline constexpr float kRowHeight = 48.0f;
inline constexpr float kRowSelectedHeight = 80.0f;
inline constexpr float kRowGap = 10.0f;
inline constexpr float kRowPitch = kRowHeight + kRowGap; // 58
inline constexpr float kRowsHeight = static_cast<float>(kOptionsRowCount - 1) * kRowPitch +
                                     kRowSelectedHeight; // 428
inline constexpr float kPanelWidth = kRowWidth + 2.0f * kPanelPadX;                     // 640
inline constexpr float kPanelHeight = kHeaderHeight + kPanelPadY + kRowsHeight + kPanelPadY; // 542
inline constexpr Rect kPanel{
    (theme::layout::kRefWidth - kPanelWidth) * 0.5f,
    kHeaderHeight + (select_art::kHintBarTop - kHeaderHeight - kPanelHeight) * 0.5f, kPanelWidth,
    kPanelHeight};
inline constexpr float kTitleX = kPanel.x + 24.0f; // 344
inline constexpr theme::TextStyle kTitleStyle{theme::Font::SairaExtraBold, 44, 4, true,
                                              theme::color::kWhite, theme::Shadow::Hard3};
inline constexpr float kRowX = kPanel.x + kPanelPadX;              // 360
inline constexpr float kRowsTop = kPanel.y + kHeaderHeight + kPanelPadY; // 185
inline constexpr float kValueInset = 30.0f;
inline constexpr float kValueSelectedInset = 36.0f;
inline constexpr float kNameValueGap = 24.0f;
inline constexpr float kPanelRing = 2.0f;

// Room budget: the panel sits between the two bars.
static_assert(kPanel.y >= theme::layout::kTopBarHeight + 2.0f);
static_assert(kPanel.y + kPanel.h <= select_art::kHintBarTop);
static_assert(kRowsTop + static_cast<float>(kOptionsRowCount - 1) * kRowPitch +
                      (kRowSelectedHeight - kRowHeight) + kRowHeight + kPanelPadY ==
              kPanel.y + kPanel.h);

[[nodiscard]] Rect panel_rect();
[[nodiscard]] Rect header_rect();
[[nodiscard]] Rect row_rect(int row, int selected);
[[nodiscard]] float name_x(const Rect& row, bool selected);
[[nodiscard]] float value_right(const Rect& row, bool selected);
[[nodiscard]] const std::array<std::string, kOptionsRowCount>& row_names();
[[nodiscard]] select_art::HintLine hint_layout(const select_art::HintMeasure& measure_key,
                                               const select_art::HintMeasure& measure_word);

void draw_panel(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                const theme::LayoutScale& L);
void draw_rows(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
               const theme::LayoutScale& L, std::span<const std::string> values, int selected);
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w);
void draw_overlay(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                  const theme::LayoutScale& L, int w, int h, std::span<const std::string> values,
                  int selected);

} // namespace options_art
} // namespace blaze4k
