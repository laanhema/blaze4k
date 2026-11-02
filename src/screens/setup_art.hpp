#pragma once

// Cabinet Input Remap and Calibration screen art (#97).

#include <array>
#include <span>
#include <string>
#include <string_view>

#include "input/input_event.hpp"
#include "render/geometry.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/ttf_font.hpp"
#include "screens/input_remap.hpp"
#include "screens/options_art.hpp"
#include "screens/results_art.hpp"
#include "screens/select_art.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;

namespace setup_art {

// Constants (reference px)

inline constexpr std::string_view kRemapTitle = "REMAP INPUT";
inline constexpr std::string_view kCalibrateTitle = "CALIBRATE OFFSET";
inline constexpr float kTitleX = theme::layout::kTopBarPadX;
inline constexpr float kTitleBandHeight = theme::layout::kTopBarHeight;

using HintItem = select_art::HintItem;
inline constexpr std::array<HintItem, 6> kRemapBrowseHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::Word, "SELECT"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "REBIND"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "BACK"},
}};
inline constexpr std::array<HintItem, 6> kRemapResetHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::Word, "SELECT"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "RESET"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "BACK"},
}};
inline constexpr std::array<HintItem, 3> kRemapCaptureHint = {{
    {HintItem::Kind::Word, "PRESS A KEY OR PAD BUTTON"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};
inline constexpr std::array<HintItem, 7> kCalibrateHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::HArrows, {}},
    {HintItem::Kind::Word, "TAP"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "SAVE"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};
inline constexpr std::array<HintItem, 5> kCalibrateNoAudioHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::HArrows, {}},
    {HintItem::Kind::Word, "TAP"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};

inline constexpr float kRemapListTop = theme::layout::kTopBarHeight + 2.0f + 24.0f; // 90
inline constexpr float kRemapListBottom = select_art::kHintBarTop - 24.0f;           // 642
inline constexpr int kRemapVisibleRows = 9;
inline constexpr std::string_view kResetLabel = "RESET TO DEFAULTS";
inline constexpr std::string_view kPressLabel = "<PRESS>";

inline constexpr theme::TextStyle kHeaderStyle = theme::text::kWheelPack; // pack ink on wheel_pack
inline constexpr theme::TextStyle kActionStyle = theme::text::kWheelRow;
inline constexpr theme::TextStyle kKeyStyle =
    with_color(theme::text::kWheelPack, theme::color::kGold); // the options value style
inline constexpr theme::TextStyle kSelectedStyle = theme::text::kWheelSelected;
inline constexpr theme::TextStyle kResetStyle =
    with_color(theme::text::kWheelRow, theme::color::kMissLabel);
inline constexpr theme::TextStyle kChipStyle = with_color(theme::text::kChip, theme::color::kGold);

inline constexpr theme::TextStyle kPhaseStyle{theme::Font::SairaExtraBold, 44, 6, true,
                                              theme::color::kGold, theme::Shadow::Hard3};
inline constexpr float kPhaseCentreX = theme::layout::kRefWidth * 0.5f;
inline constexpr float kPhaseTop = 200.0f;
inline constexpr float kPhaseHeight = 64.0f;
inline constexpr float kCalPlateWidth = theme::layout::kStatPanelWidth;    // 360
inline constexpr float kCalPlateHeight = results_art::kStatPanelHeights[0]; // 111
inline constexpr float kCalPlateGap = 20.0f;
inline constexpr float kCalPlateTop = 300.0f;
inline constexpr float kCalPlateX =
    (theme::layout::kRefWidth - (2.0f * kCalPlateWidth + kCalPlateGap)) * 0.5f; // 270
inline constexpr std::array<std::string_view, 2> kCalPlateLabels = {"SAMPLES", "OFFSET"};
inline constexpr float kValueCapHalf = 12.0f;
inline constexpr theme::TextStyle kPlateLabelStyle = theme::text::kStatLabel;
inline constexpr theme::TextStyle kPlateValueStyle = theme::text::kComboNumber;
inline constexpr theme::TextStyle kPlatePendingStyle =
    with_color(theme::text::kComboNumber, theme::color::kSteel); // "---"
inline constexpr std::string_view kOffsetPending = "---";
inline constexpr std::string_view kNoAudioNotice = "AUDIO UNAVAILABLE - OFFSET WILL NOT BE SAVED";
inline constexpr std::string_view kOutOfRangeNotice = "DELAY OUT OF RANGE - OFFSET WILL NOT BE SAVED";
inline constexpr float kNoticeTop = 450.0f;
inline constexpr float kNoticeHeight = 44.0f;
inline constexpr theme::TextStyle kNoticeStyle =
    with_color(theme::text::kWheelRow, theme::color::kMissLabel);

// Room budget: nine rows (one selected) end above kRemapListBottom; a tenth does not.
static_assert(kRemapListTop + static_cast<float>(kRemapVisibleRows - 1) * options_art::kRowPitch +
                  (options_art::kRowSelectedHeight - options_art::kRowHeight) +
                  options_art::kRowHeight <=
              kRemapListBottom);
static_assert(kRemapListTop + static_cast<float>(kRemapVisibleRows) * options_art::kRowPitch +
                  (options_art::kRowSelectedHeight - options_art::kRowHeight) +
                  options_art::kRowHeight >
              kRemapListBottom);
// The plate pair is centred on x 640; the plates and the notice band sit between
// the top bar and the hint-bar rule, the notice below the plates.
static_assert(kCalPlateX + (2.0f * kCalPlateWidth + kCalPlateGap) * 0.5f ==
              theme::layout::kRefWidth * 0.5f);
static_assert(kPhaseTop >= theme::layout::kTopBarHeight + 2.0f);
static_assert(kPhaseTop + kPhaseHeight <= kCalPlateTop);
static_assert(kCalPlateTop + kCalPlateHeight <= kNoticeTop);
static_assert(kNoticeTop + kNoticeHeight <= select_art::kHintBarTop);

// ---------------------------------------------------------------------------------------------
// Pure layout: remap
// ---------------------------------------------------------------------------------------------

// Remap display rows: a Header before each run of same-device binding rows
// (KEYBOARD, PAD), the binding rows, then one Reset row. Display only:
// navigation still moves over the model's rows. Nothing here allocates.
struct RemapDisplayRow {
    enum class Kind { Header, Binding, Reset };
    Kind kind = Kind::Reset;
    int binding = -1;                     // Binding only: index into the model's rows
    DeviceType device = DeviceType::Keyboard; // Header only
};
[[nodiscard]] int remap_display_count(std::span<const RemapRow> rows);
[[nodiscard]] RemapDisplayRow remap_display_row(std::span<const RemapRow> rows, int d);
[[nodiscard]] int remap_display_index(std::span<const RemapRow> rows, int binding);
[[nodiscard]] int remap_reset_display_index(std::span<const RemapRow> rows);

[[nodiscard]] Rect remap_row_rect(int slot, int selected_slot);
[[nodiscard]] Rect remap_chip_rect(float text_w);

[[nodiscard]] std::string_view remap_value_view(const InputRemapModel& model, int row);

// ---------------------------------------------------------------------------------------------
// Pure layout: calibration
// ---------------------------------------------------------------------------------------------

[[nodiscard]] Rect calibration_plate_rect(int i);
[[nodiscard]] float plate_centre_x(const Rect& plate, float centre_y);
[[nodiscard]] std::string calibration_samples_text(int count, int min_samples, bool ready);

// ---------------------------------------------------------------------------------------------
// Draw helpers (no-ops for null services / an uninitialised renderer)
// ---------------------------------------------------------------------------------------------

void draw_chrome(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                 const theme::LayoutScale& L, int w, int h, std::string_view title);
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w, std::span<const HintItem> items);
void draw_message_chip(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                       const theme::LayoutScale& L, std::string_view message);
void draw_remap_table(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const InputRemapModel& model,
                      bool reset_selected);

struct CalibrationView {
    std::string_view phase_word;
    std::string_view samples;
    std::string_view offset;
    bool offset_ready = false; // false: `offset` drawn in the pending (steel) style
    bool synthetic = false;
    bool out_of_range = false; // #74: measured delay outside the supported window
};
void draw_calibration(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const CalibrationView& view);

} // namespace setup_art
} // namespace blaze4k
