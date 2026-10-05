#pragma once

// Cabinet Input Remap and Calibration screen art (#97), drawn by InputRemapScreen
// and CalibrationScreen (the two setup screens reached from the options overlay).
//
// A free design (the Cabinet mock-ups have neither screen) built from select's,
// the options overlay's and the score screen's parts: bg_select, a bar_top with a
// runtime title, a per-state legend in bar_hint and the scanlines last. Remap
// shows its binding table as the options overlay's slanted rows (gold
// wheel_row_selected bar, wheel_pack KEYBOARD / PAD headers); calibration shows
// a gold phase word over two stat_panel plates (SAMPLES, OFFSET).
//
//  - Pure layout helpers in the 1280x720 reference space (theme::layout), mapped
//    to window pixels with theme::layout_scale (#91). GL-free, so setup_art_test
//    pins them headless. Every rect is a manifest *content box*.
//  - Thin draw helpers over ThemeTextures (#89) and TextRenderer (#90). Each one
//    is a no-op for a null service and on an uninitialised GlQuadRenderer.
//
// Presentation only: nothing here reads the music clock or wall time, and no
// audio / clock header is included (the calibration phase word is passed in).

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

// ---------------------------------------------------------------------------------------------
// Constants (reference px)
// ---------------------------------------------------------------------------------------------

// Runtime titles in the top bar (there are no baked title sprites for these screens).
inline constexpr std::string_view kRemapTitle = "REMAP INPUT";
inline constexpr std::string_view kCalibrateTitle = "CALIBRATE OFFSET";
// Title: x 40 (as the title sprites), line box centred in the bar's 64px band.
inline constexpr float kTitleX = theme::layout::kTopBarPadX;
inline constexpr float kTitleBandHeight = theme::layout::kTopBarHeight;

// Legends (gold arrows / keys, grey words). Today's words, [BACK] shown as ESC.
using HintItem = select_art::HintItem;
// [up down] SELECT  ENTER REBIND  ESC BACK: browsing, on a binding row.
inline constexpr std::array<HintItem, 6> kRemapBrowseHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::Word, "SELECT"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "REBIND"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "BACK"},
}};
// [up down] SELECT  ENTER RESET  ESC BACK: browsing, on the RESET row.
inline constexpr std::array<HintItem, 6> kRemapResetHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::Word, "SELECT"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "RESET"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "BACK"},
}};
// PRESS A KEY OR PAD BUTTON  ESC CANCEL: capturing.
inline constexpr std::array<HintItem, 3> kRemapCaptureHint = {{
    {HintItem::Kind::Word, "PRESS A KEY OR PAD BUTTON"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};
// [up down] [left right] TAP  ENTER SAVE  ESC CANCEL: calibration with audio.
inline constexpr std::array<HintItem, 7> kCalibrateHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::HArrows, {}},
    {HintItem::Kind::Word, "TAP"},
    {HintItem::Kind::Key, "ENTER"},
    {HintItem::Kind::Word, "SAVE"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};
// [up down] [left right] TAP  ESC CANCEL: synthetic clock (Enter cannot save).
inline constexpr std::array<HintItem, 5> kCalibrateNoAudioHint = {{
    {HintItem::Kind::VArrows, {}},
    {HintItem::Kind::HArrows, {}},
    {HintItem::Kind::Word, "TAP"},
    {HintItem::Kind::Key, "ESC"},
    {HintItem::Kind::Word, "CANCEL"},
}};

// Remap table: the options overlay's rows (x 360, 560 wide, 48 / 80 tall, pitch 58).
// The first row 24px under the top bar (64 + 2px rule); the last 24px above the
// hint-bar rule (select_art::kHintBarTop 666).
inline constexpr float kRemapListTop = theme::layout::kTopBarHeight + 2.0f + 24.0f; // 90
inline constexpr float kRemapListBottom = select_art::kHintBarTop - 24.0f;           // 642
// select_art::visible_rows(90, 642, 58, 80) (pinned in setup_art_test).
inline constexpr int kRemapVisibleRows = 9;
// "RESET TO DEFAULTS": today's distinct reset colour, now a theme colour.
inline constexpr std::string_view kResetLabel = "RESET TO DEFAULTS";
// "<PRESS>" on the row being rebound (remap_row_value_text).
inline constexpr std::string_view kPressLabel = "<PRESS>";

// Remap text styles (each shares a pre-baked (font, size) in theme::text::kAllStyles).
inline constexpr theme::TextStyle kHeaderStyle = theme::text::kWheelPack; // pack ink on wheel_pack
inline constexpr theme::TextStyle kActionStyle = theme::text::kWheelRow;
inline constexpr theme::TextStyle kKeyStyle =
    with_color(theme::text::kWheelPack, theme::color::kGold); // the options value style
inline constexpr theme::TextStyle kSelectedStyle = theme::text::kWheelSelected;
inline constexpr theme::TextStyle kResetStyle =
    with_color(theme::text::kWheelRow, theme::color::kMissLabel);
inline constexpr theme::TextStyle kChipStyle = with_color(theme::text::kChip, theme::color::kGold);

// Calibration: the phase word, SairaExtraBold 44 (the pre-baked kSongTitle atlas,
// as options_art::kTitleStyle) with headline tracking, gold. Line box centred in
// the band {200, 64} (today's word sat at 32% of the height).
inline constexpr theme::TextStyle kPhaseStyle{theme::Font::SairaExtraBold, 44, 6, true,
                                              theme::color::kGold, theme::Shadow::Hard3};
inline constexpr float kPhaseCentreX = theme::layout::kRefWidth * 0.5f;
inline constexpr float kPhaseTop = 200.0f;
inline constexpr float kPhaseHeight = 64.0f;
// The two stat_panel plates (the score screen's 360x111 size), 20 apart,
// centred on x 640: 270 + 360 + 20 + 360 = 1010.
inline constexpr float kCalPlateWidth = theme::layout::kStatPanelWidth;    // 360
inline constexpr float kCalPlateHeight = results_art::kStatPanelHeights[0]; // 111
inline constexpr float kCalPlateGap = 20.0f;
inline constexpr float kCalPlateTop = 300.0f;
inline constexpr float kCalPlateX =
    (theme::layout::kRefWidth - (2.0f * kCalPlateWidth + kCalPlateGap)) * 0.5f; // 270
inline constexpr std::array<std::string_view, 2> kCalPlateLabels = {"SAMPLES", "OFFSET"};
// Value cap centre above its baseline (SairaExtraBold 34: caps ~24 tall), for the
// slant correction of a centred value.
inline constexpr float kValueCapHalf = 12.0f;
// Calibration text styles: the score screen's label and value styles.
inline constexpr theme::TextStyle kPlateLabelStyle = theme::text::kStatLabel;
inline constexpr theme::TextStyle kPlateValueStyle = theme::text::kComboNumber;
inline constexpr theme::TextStyle kPlatePendingStyle =
    with_color(theme::text::kComboNumber, theme::color::kSteel); // "---"
// The offset readout before a usable result (and always on the synthetic clock).
inline constexpr std::string_view kOffsetPending = "---";
// No-audio notice: a centred red line in the band {450, 40} under the plates.
inline constexpr std::string_view kNoAudioNotice = "AUDIO UNAVAILABLE - OFFSET WILL NOT BE SAVED";
inline constexpr float kNoticeTop = 450.0f;
inline constexpr float kNoticeHeight = 40.0f;
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
// Display row `d` (clamped to [0, count - 1]).
[[nodiscard]] RemapDisplayRow remap_display_row(std::span<const RemapRow> rows, int d);
// The display index of binding row `binding` (clamped to the rows); 0 for no rows.
[[nodiscard]] int remap_display_index(std::span<const RemapRow> rows, int binding);
// The trailing Reset row: remap_display_count(rows) - 1.
[[nodiscard]] int remap_reset_display_index(std::span<const RemapRow> rows);

// Row `slot` of the window with the selection at `selected_slot`:
// {360, 90 + slot * 58 (+32 below the selection), 560, 80 selected / 48}.
[[nodiscard]] Rect remap_row_rect(int slot, int selected_slot);

// The message chip for a reference text width: right-aligned at x 1240, at
// select's chip top / height / padding: {1240 - (w + 44), 12, w + 44, 40}.
[[nodiscard]] Rect remap_chip_rect(float text_w);

// remap_action_name / remap_device_name / remap_row_value_text without allocating.
[[nodiscard]] std::string_view remap_action_label(GameAction action);
[[nodiscard]] std::string_view remap_device_label(DeviceType device);
[[nodiscard]] std::string_view remap_value_view(const InputRemapModel& model, int row);

// ---------------------------------------------------------------------------------------------
// Pure layout: calibration
// ---------------------------------------------------------------------------------------------

// Plate `i` (clamped to 0..1): SAMPLES {270, 300, 360, 111}, OFFSET {650, 300, 360, 111}.
[[nodiscard]] Rect calibration_plate_rect(int i);

// The x of an element centred in a -8deg plate at `centre_y`:
// plate.x + plate.w / 2 + skew::kStatPanel * (plate centre y - centre_y).
[[nodiscard]] float plate_centre_x(const Rect& plate, float centre_y);

// SAMPLES value: "n" once ready, else "n / min" (SSO-sized).
[[nodiscard]] std::string calibration_samples_text(int count, int min_samples, bool ready);

// ---------------------------------------------------------------------------------------------
// Draw helpers (no-ops for null services / an uninitialised renderer)
// ---------------------------------------------------------------------------------------------

// bg_select over the window, bar_top across it and the runtime `title`.
void draw_chrome(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                 const theme::LayoutScale& L, int w, int h, std::string_view title);

// bar_hint along the bottom, then the legend laid out from `items`.
void draw_hint_bar(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                   const theme::LayoutScale& L, int w, std::span<const HintItem> items);

// The model's transient message as a gold chip on the right of the top bar.
// Nothing for an empty message or a null text service (the width needs text).
void draw_message_chip(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                       const theme::LayoutScale& L, std::string_view message);

// The scrolling binding table: art first (wheel_pack headers, wheel_row rows,
// the gold bar), then text grouped by style. The gold bar sits on the selected
// row, and stays on the row being rebound while capturing.
void draw_remap_table(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const InputRemapModel& model,
                      bool reset_selected);

// Calibration body: the plates, the phase word, the plate labels and values,
// and the no-audio notice (synthetic only).
struct CalibrationView {
    std::string_view phase_word;
    std::string_view samples;
    std::string_view offset;
    bool offset_ready = false; // false: `offset` drawn in the pending (steel) style
    bool synthetic = false;
};
void draw_calibration(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
                      const theme::LayoutScale& L, const CalibrationView& view);

} // namespace setup_art
} // namespace blaze4k
