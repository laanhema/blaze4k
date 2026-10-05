// #97: Cabinet Input Remap + Calibration art (setup_art). Pins the remap display
// rows (KEYBOARD / PAD headers, binding rows, the trailing RESET row) and their
// round trip to the model's rows, the remap row layout and room budget (9
// visible rows at 720p, exact 2x at 1440p), the message chip box, the
// allocation-free labels against input_remap's display helpers, the calibration
// plate layout and samples text, the five legends, text fit with the real fonts,
// that every style shares a pre-baked atlas, that every texture name is in the
// real manifest, and renders InputRemapScreen and CalibrationScreen through every
// state with the real headless theme and text services (and with null services)
// at five window sizes.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "audio/metronome.hpp"
#include "data/config.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/calibration_screen.hpp"
#include "screens/input_remap.hpp"
#include "screens/input_remap_screen.hpp"
#include "screens/options_art.hpp"
#include "screens/options_menu.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_art.hpp"
#include "screens/setup_art.hpp"
#include "timing/music_clock.hpp"
#include "timing/offset_calibration.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

namespace fs = std::filesystem;
namespace theme = blaze4k::theme;
namespace art = blaze4k::select_art;
namespace opt = blaze4k::options_art;
namespace setup = blaze4k::setup_art;

using blaze4k::CalibrationConfig;
using blaze4k::DeviceType;
using blaze4k::GameAction;
using blaze4k::InputEvent;
using blaze4k::InputRemapModel;
using blaze4k::Rect;
using blaze4k::RemapRow;
using blaze4k::ScreenId;
using Kind = blaze4k::setup_art::RemapDisplayRow::Kind;

const fs::path kSourceDir{BLAZE4K_SOURCE_DIR};
const fs::path kAssets{BLAZE4K_ASSETS_DIR};
const fs::path kCabinet = kAssets / "theme" / "cabinet";

struct Size {
    int w, h;
};
constexpr Size kSizes[] = {{1280, 720}, {2560, 1440}, {3440, 1440}, {1920, 1200}, {0, 0}};

bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

bool rect_eq(const Rect& r, float x, float y, float w, float h) {
    return approx(r.x, x) && approx(r.y, y) && approx(r.w, w) && approx(r.h, h);
}

InputEvent press(GameAction action, std::uint64_t ts_ns = 0) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    event.timestamp_ns = ts_ns;
    return event;
}

blaze4k::ThemeTextures& loaded_theme() {
    static blaze4k::ThemeTextures theme;
    static const bool loaded = theme.load(kCabinet);
    TEST_CHECK(loaded);
    return theme;
}

blaze4k::TextRenderer& loaded_text() {
    static blaze4k::TextRenderer text;
    static const bool loaded = text.load(kSourceDir);
    TEST_CHECK(loaded);
    text.set_window_size(1280, 720);
    return text;
}

// In-memory IAudioStream (calibration_screen_test's fake): no device is opened.
class FakeAudioStream : public blaze4k::IAudioStream {
public:
    bool load(const std::string& /*filepath*/) override {
        playing = false;
        return load_result;
    }
    void stop() override { playing = false; }
    bool play() override {
        playing = true;
        return true;
    }
    bool seek_seconds(double seconds) override {
        position = seconds;
        return true;
    }
    [[nodiscard]] double get_position_seconds() const override { return position; }
    [[nodiscard]] bool is_playing() const override { return playing; }
    void set_volume(float /*volume*/) override {}

    bool load_result = true;
    bool playing = false;
    double position = 0.0;
};

InputRemapModel default_model() {
    return blaze4k::input_remap_from_config(blaze4k::GameConfig{});
}

void test_remap_display_rows() {
    const InputRemapModel model = default_model();
    const std::span<const RemapRow> rows(model.rows);
    TEST_CHECK(rows.size() == 24);
    TEST_CHECK(setup::remap_display_count(rows) == 27);

    const auto header0 = setup::remap_display_row(rows, 0);
    TEST_CHECK(header0.kind == Kind::Header && header0.device == DeviceType::Keyboard);
    const auto header13 = setup::remap_display_row(rows, 13);
    TEST_CHECK(header13.kind == Kind::Header && header13.device == DeviceType::Gamepad);
    TEST_CHECK(setup::remap_display_row(rows, 26).kind == Kind::Reset);
    TEST_CHECK(setup::remap_display_index(rows, 0) == 1);
    TEST_CHECK(setup::remap_display_index(rows, 11) == 12);
    TEST_CHECK(setup::remap_display_index(rows, 12) == 14);
    TEST_CHECK(setup::remap_display_index(rows, 23) == 25);
    TEST_CHECK(setup::remap_reset_display_index(rows) == 26);

    // Round trip over every binding row.
    for (int b = 0; b < static_cast<int>(rows.size()); ++b) {
        const auto row = setup::remap_display_row(rows, setup::remap_display_index(rows, b));
        TEST_CHECK(row.kind == Kind::Binding && row.binding == b);
        TEST_CHECK(row.device == rows[static_cast<std::size_t>(b)].device);
    }
    // Every display row is exactly one of: header, binding (each once), reset.
    int headers = 0;
    int bindings = 0;
    for (int d = 0; d < 27; ++d) {
        const auto row = setup::remap_display_row(rows, d);
        headers += row.kind == Kind::Header ? 1 : 0;
        bindings += row.kind == Kind::Binding ? 1 : 0;
    }
    TEST_CHECK(headers == 2 && bindings == 24);

    // Out-of-range inputs clamp.
    TEST_CHECK(setup::remap_display_row(rows, -5).kind == Kind::Header);
    TEST_CHECK(setup::remap_display_row(rows, 99).kind == Kind::Reset);
    TEST_CHECK(setup::remap_display_index(rows, -1) == 1);
    TEST_CHECK(setup::remap_display_index(rows, 99) == 25);

    // Keyboard-only rows: one header.
    const std::vector<RemapRow> keyboard = {
        {GameAction::Left, DeviceType::Keyboard, "Left"},
        {GameAction::Down, DeviceType::Keyboard, "Down"},
    };
    TEST_CHECK(setup::remap_display_count(keyboard) == 4);
    TEST_CHECK(setup::remap_display_row(keyboard, 0).kind == Kind::Header);
    TEST_CHECK(setup::remap_display_index(keyboard, 1) == 2);
    TEST_CHECK(setup::remap_reset_display_index(keyboard) == 3);

    // Interleaved devices: a header before each run.
    const std::vector<RemapRow> mixed = {
        {GameAction::Left, DeviceType::Keyboard, "Left"},
        {GameAction::Left, DeviceType::Gamepad, "dpleft"},
        {GameAction::Down, DeviceType::Keyboard, "Down"},
    };
    TEST_CHECK(setup::remap_display_count(mixed) == 7);
    TEST_CHECK(setup::remap_display_index(mixed, 2) == 5);
    TEST_CHECK(setup::remap_display_row(mixed, 4).kind == Kind::Header);
    TEST_CHECK(setup::remap_display_row(mixed, 4).device == DeviceType::Keyboard);

    // No rows: RESET only.
    const std::vector<RemapRow> empty;
    TEST_CHECK(setup::remap_display_count(empty) == 1);
    TEST_CHECK(setup::remap_display_index(empty, 0) == 0);
    TEST_CHECK(setup::remap_reset_display_index(empty) == 0);
    TEST_CHECK(setup::remap_display_row(empty, 0).kind == Kind::Reset);
    TEST_CHECK(setup::remap_display_row(empty, 3).kind == Kind::Reset);
    std::cout << "  - remap display rows ok.\n";
}

void test_remap_layout() {
    TEST_CHECK(setup::kRemapListTop == 90.0f && setup::kRemapListBottom == 642.0f);
    TEST_CHECK(setup::kRemapVisibleRows == art::visible_rows(90.0f, 642.0f, 58.0f, 80.0f));
    TEST_CHECK(setup::kRemapVisibleRows == 9);
    TEST_CHECK(rect_eq(setup::remap_row_rect(0, 0), 360, 90, 560, 80));
    TEST_CHECK(rect_eq(setup::remap_row_rect(1, 0), 360, 180, 560, 48));
    TEST_CHECK(rect_eq(setup::remap_row_rect(8, 8), 360, 554, 560, 80));

    for (int sel = 0; sel < setup::kRemapVisibleRows; ++sel) {
        int tall = 0;
        for (int i = 0; i < setup::kRemapVisibleRows; ++i) {
            const Rect r = setup::remap_row_rect(i, sel);
            TEST_CHECK(r.y >= 90.0f - 1e-3f);
            TEST_CHECK(r.y + r.h <= 642.0f + 1e-3f);
            if (i + 1 < setup::kRemapVisibleRows) {
                TEST_CHECK(r.y + r.h < setup::remap_row_rect(i + 1, sel).y);
            }
            TEST_CHECK(r.h == (i == sel ? 80.0f : 48.0f));
            tall += r.h == 80.0f ? 1 : 0;
        }
        TEST_CHECK(tall == 1);
    }
    // Under the top bar, above the hint-bar rule.
    TEST_CHECK(setup::kRemapListTop >= theme::layout::kTopBarHeight + 2.0f);
    TEST_CHECK(setup::kRemapListBottom <= art::kHintBarTop);

    // Exact 2x at 1440p.
    TEST_CHECK(rect_eq(theme::layout_scale(2560, 1440).rect(setup::remap_row_rect(0, 0)), 720, 180,
                       1120, 160));

    // The chip ends at x 1240 and starts after the widest title (40 + 355).
    const Rect chip = setup::remap_chip_rect(185.0f);
    TEST_CHECK(rect_eq(chip, 1240.0f - 229.0f, 12, 229, 40));
    TEST_CHECK(approx(chip.x + chip.w, 1240.0f));
    TEST_CHECK(chip.x > 395.0f);
    TEST_CHECK(rect_eq(setup::remap_chip_rect(-5.0f), 1196, 12, 44, 40));
    TEST_CHECK(rect_eq(setup::remap_chip_rect(NAN), 1196, 12, 44, 40));

    // The window keeps the selection visible at the top, middle and RESET.
    const InputRemapModel model = default_model();
    const int count = setup::remap_display_count(model.rows);
    for (const int sel : {0, 1, 13, 14, 25, 26}) {
        const art::ListWindow win = art::list_window(sel, count, setup::kRemapVisibleRows);
        TEST_CHECK(win.first <= sel && sel <= win.last);
        TEST_CHECK(win.last - win.first + 1 == setup::kRemapVisibleRows);
    }
    std::cout << "  - remap layout ok.\n";
}

void test_remap_labels() {
    for (const GameAction action :
         {GameAction::Left, GameAction::Down, GameAction::Up, GameAction::Right,
          GameAction::Confirm, GameAction::Back, GameAction::Options, GameAction::None}) {
        TEST_CHECK(blaze4k::remap_action_label(action) == blaze4k::remap_action_name(action));
    }
    for (const DeviceType device : {DeviceType::Keyboard, DeviceType::Gamepad}) {
        TEST_CHECK(blaze4k::remap_device_label(device) == blaze4k::remap_device_name(device));
    }

    InputRemapModel model = default_model();
    auto check_all = [&model] {
        for (int i = -1; i <= static_cast<int>(model.rows.size()); ++i) {
            TEST_CHECK(setup::remap_value_view(model, i) == blaze4k::remap_row_value_text(model, i));
        }
    };
    check_all();
    for (const int row : {0, 23}) {
        blaze4k::input_remap_set_row(model, row);
        model.capturing = true;
        check_all();
        TEST_CHECK(setup::remap_value_view(model, row) == "<PRESS>");
        model.capturing = false;
        check_all();
    }
    std::cout << "  - remap labels match input_remap's display helpers ok.\n";
}

void test_calibration_layout() {
    TEST_CHECK(rect_eq(setup::calibration_plate_rect(0), 270, 300, 360, 111));
    TEST_CHECK(rect_eq(setup::calibration_plate_rect(1), 650, 300, 360, 111));
    TEST_CHECK(rect_eq(setup::calibration_plate_rect(-3), 270, 300, 360, 111));
    TEST_CHECK(rect_eq(setup::calibration_plate_rect(7), 650, 300, 360, 111));
    const Rect a = setup::calibration_plate_rect(0);
    const Rect b = setup::calibration_plate_rect(1);
    TEST_CHECK(approx((a.x + b.x + b.w) * 0.5f, 640.0f));
    TEST_CHECK(approx(b.x - (a.x + a.w), setup::kCalPlateGap));
    for (const Rect& r : {a, b}) {
        TEST_CHECK(r.y >= theme::layout::kTopBarHeight + 2.0f);
        TEST_CHECK(r.y + r.h <= art::kHintBarTop);
        TEST_CHECK(r.y + r.h <= setup::kNoticeTop);
        TEST_CHECK(r.y >= setup::kPhaseTop + setup::kPhaseHeight);
    }
    TEST_CHECK(setup::kNoticeTop + setup::kNoticeHeight <= art::kHintBarTop);

    // Slant correction: the plate centre at mid-height, shifted by kStatPanel * dy.
    const float mid = a.y + a.h * 0.5f;
    TEST_CHECK(approx(setup::plate_centre_x(a, mid), a.x + a.w * 0.5f));
    TEST_CHECK(approx(setup::plate_centre_x(a, mid - 20.0f),
                      a.x + a.w * 0.5f + theme::skew::kStatPanel * 20.0f));
    TEST_CHECK(approx(setup::plate_centre_x(a, mid + 10.0f),
                      a.x + a.w * 0.5f - theme::skew::kStatPanel * 10.0f));

    TEST_CHECK(setup::calibration_samples_text(3, 8, false) == "3 / 8");
    TEST_CHECK(setup::calibration_samples_text(12, 8, true) == "12");
    TEST_CHECK(setup::calibration_samples_text(0, 8, false) == "0 / 8");
    std::cout << "  - calibration layout ok.\n";
}

void test_legends() {
    blaze4k::TextRenderer& text = loaded_text();
    const art::HintMeasure key = [&text](std::string_view s) {
        return blaze4k::ref_measure(text, s, theme::text::kHintKey);
    };
    const art::HintMeasure word = [&text](std::string_view s) {
        return blaze4k::ref_measure(text, s, theme::text::kHintWord);
    };
    using P = art::HintPiece::Kind;
    struct Expect {
        std::span<const art::HintItem> items;
        std::vector<P> kinds;
        std::vector<std::string_view> texts; // "" for arrows
    };
    const std::vector<Expect> legends = {
        {setup::kRemapBrowseHint,
         {P::Arrow, P::Arrow, P::Word, P::Key, P::Word, P::Key, P::Word},
         {"", "", "SELECT", "ENTER", "REBIND", "ESC", "BACK"}},
        {setup::kRemapResetHint,
         {P::Arrow, P::Arrow, P::Word, P::Key, P::Word, P::Key, P::Word},
         {"", "", "SELECT", "ENTER", "RESET", "ESC", "BACK"}},
        {setup::kRemapCaptureHint,
         {P::Word, P::Key, P::Word},
         {"PRESS A KEY OR PAD BUTTON", "ESC", "CANCEL"}},
        {setup::kCalibrateHint,
         {P::Arrow, P::Arrow, P::Arrow, P::Arrow, P::Word, P::Key, P::Word, P::Key, P::Word},
         {"", "", "", "", "TAP", "ENTER", "SAVE", "ESC", "CANCEL"}},
        {setup::kCalibrateNoAudioHint,
         {P::Arrow, P::Arrow, P::Arrow, P::Arrow, P::Word, P::Key, P::Word},
         {"", "", "", "", "TAP", "ESC", "CANCEL"}},
    };
    const std::array<int, 5> expected_counts = {7, 7, 3, 9, 7};
    for (std::size_t n = 0; n < legends.size(); ++n) {
        const Expect& e = legends[n];
        const art::HintLine line = art::layout_hint_items(e.items, key, word);
        TEST_CHECK(line.count == expected_counts[n]);
        TEST_CHECK(line.count == static_cast<int>(e.kinds.size()));
        for (int i = 0; i < line.count; ++i) {
            const art::HintPiece& p = line.pieces[static_cast<std::size_t>(i)];
            TEST_CHECK(p.kind == e.kinds[static_cast<std::size_t>(i)]);
            if (p.kind != P::Arrow) {
                TEST_CHECK(p.text == e.texts[static_cast<std::size_t>(i)]);
            }
        }
        const art::HintPiece& first = line.pieces[0];
        const art::HintPiece& last = line.pieces[static_cast<std::size_t>(line.count - 1)];
        TEST_CHECK(approx(first.x + line.width * 0.5f, 640.0f));
        TEST_CHECK(approx(last.x + last.width, first.x + line.width)); // no trailing gap
        TEST_CHECK(line.width < 1280.0f - 2.0f * 40.0f);
    }
    // The capture line ends on a Word after a Key (the #96 trailing-gap fix).
    const art::HintLine capture = art::layout_hint_items(setup::kRemapCaptureHint, key, word);
    TEST_CHECK(approx(capture.pieces[2].x, capture.pieces[1].x + capture.pieces[1].width + 8.0f));
    std::cout << "  - legends ok.\n";
}

void test_text_fits() {
    blaze4k::TextRenderer& text = loaded_text();
    auto measure = [&text](std::string_view s, const theme::TextStyle& style) {
        return blaze4k::ref_measure(text, s, style);
    };

    // The widest message the model can set.
    float widest_message = measure("DEFAULTS RESTORED", setup::kChipStyle);
    for (const GameAction action :
         {GameAction::Left, GameAction::Down, GameAction::Up, GameAction::Right,
          GameAction::Confirm, GameAction::Back, GameAction::Options}) {
        const std::string name = blaze4k::remap_action_name(action);
        widest_message = std::max(widest_message, measure("BOUND TO " + name, setup::kChipStyle));
        widest_message = std::max(widest_message, measure("IN USE: " + name, setup::kChipStyle));
    }
    const float chip_x = setup::remap_chip_rect(widest_message).x;
    for (const std::string_view title : {setup::kRemapTitle, setup::kCalibrateTitle}) {
        const float w = measure(title, opt::kTitleStyle);
        TEST_CHECK(setup::kTitleX + w < chip_x - 24.0f);
    }
    // The chip text's line box fits the chip.
    TEST_CHECK(text.line_height(setup::kChipStyle) <= art::kChipHeight);
    // The title line (~70px, taller than the 64px band, as kSongTitle's is):
    // centred in the band, the baseline and one em above it stay inside it.
    const float line_top =
        (setup::kTitleBandHeight - text.line_height(opt::kTitleStyle)) * 0.5f;
    const float baseline = line_top + text.ascent(opt::kTitleStyle);
    TEST_CHECK(baseline <= setup::kTitleBandHeight);
    TEST_CHECK(baseline - opt::kTitleStyle.size_px >= 0.0f);

    // Phase words.
    for (const std::string_view phase : {"GET READY", "TAP ON THE BEAT", "DONE"}) {
        TEST_CHECK(measure(phase, setup::kPhaseStyle) < 1280.0f - 2.0f * 40.0f);
    }

    // Every default binding (and <PRESS>), selected and not, keeps the gap to its action.
    const InputRemapModel model = default_model();
    const Rect plain = setup::remap_row_rect(1, 0);
    const Rect bar = setup::remap_row_rect(0, 0);
    std::vector<std::string> keys;
    for (const RemapRow& row : model.rows) {
        keys.push_back(row.name);
    }
    keys.emplace_back(setup::kPressLabel);
    for (const RemapRow& row : model.rows) {
        const std::string_view action = blaze4k::remap_action_label(row.action);
        for (const std::string& key : keys) {
            for (const bool selected : {false, true}) {
                const Rect& r = selected ? bar : plain;
                const theme::TextStyle& as = selected ? setup::kSelectedStyle : setup::kActionStyle;
                const theme::TextStyle& ks = selected ? setup::kSelectedStyle : setup::kKeyStyle;
                const float name_end =
                    opt::name_x(r, selected) + measure(action, as) + opt::kNameValueGap;
                const float key_start = opt::value_right(r, selected) - measure(key, ks);
                if (!(name_end <= key_start)) {
                    std::cerr << "    " << action << " / " << key << " selected=" << selected
                              << ": " << name_end << " > " << key_start << "\n";
                }
                TEST_CHECK(name_end <= key_start);
                TEST_CHECK(text.line_height(as) <= r.h && text.line_height(ks) <= r.h);
            }
        }
    }
    // RESET TO DEFAULTS fits both styles.
    TEST_CHECK(opt::name_x(plain, false) + measure(setup::kResetLabel, setup::kResetStyle) <=
               opt::value_right(plain, false));
    TEST_CHECK(opt::name_x(bar, true) + measure(setup::kResetLabel, setup::kSelectedStyle) <=
               opt::value_right(bar, true));
    // KEYBOARD / PAD fit the pack row.
    for (const std::string_view label : {"KEYBOARD", "PAD"}) {
        TEST_CHECK(plain.x + art::kWheelPackTextX + measure(label, setup::kHeaderStyle) <=
                   opt::value_right(plain, false));
        TEST_CHECK(text.line_height(setup::kHeaderStyle) <= plain.h);
    }
    // Plate values and labels fit the plate with 20px each side.
    const float plate_budget = setup::kCalPlateWidth - 2.0f * 20.0f;
    TEST_CHECK(blaze4k::format_offset(-3600.0) == "-3600.000 s");
    for (const std::string& value :
         {blaze4k::format_offset(-3600.0), blaze4k::format_offset(3600.0), std::string("+0.023 s"),
          std::string("12 / 8"), std::string("999"), setup::calibration_samples_text(64, 8, false)}) {
        TEST_CHECK(measure(value, setup::kPlateValueStyle) < plate_budget);
    }
    TEST_CHECK(measure(setup::kOffsetPending, setup::kPlatePendingStyle) < plate_budget);
    for (const std::string_view label : setup::kCalPlateLabels) {
        TEST_CHECK(measure(label, setup::kPlateLabelStyle) < plate_budget);
    }
    // The notice.
    TEST_CHECK(measure(setup::kNoAudioNotice, setup::kNoticeStyle) < 1280.0f - 2.0f * 40.0f);
    // Its line box (~44px) fills the 44px band; centred in the band it clears
    // the plates above and the hint-bar rule below.
    const float notice_top = setup::kNoticeTop +
                             (setup::kNoticeHeight - text.line_height(setup::kNoticeStyle)) * 0.5f;
    TEST_CHECK(notice_top >= setup::kCalPlateTop + setup::kCalPlateHeight);
    TEST_CHECK(notice_top + text.line_height(setup::kNoticeStyle) <= art::kHintBarTop);
    std::cout << "  - titles, rows, plates and notice fit ok.\n";
}

void test_styles_prebaked() {
    auto prebaked = [](const theme::TextStyle& style) {
        for (const theme::TextStyle& s : theme::text::kAllStyles) {
            if (s.font == style.font && s.size_px == style.size_px) {
                return true;
            }
        }
        return false;
    };
    for (const theme::TextStyle& style :
         {setup::kPhaseStyle, opt::kTitleStyle, theme::text::kWheelRow, theme::text::kWheelPack,
          theme::text::kWheelSelected, theme::text::kChip, theme::text::kStatLabel,
          theme::text::kComboNumber, theme::text::kHintKey, theme::text::kHintWord,
          setup::kHeaderStyle, setup::kActionStyle, setup::kKeyStyle, setup::kSelectedStyle,
          setup::kResetStyle, setup::kChipStyle, setup::kPlateLabelStyle, setup::kPlateValueStyle,
          setup::kPlatePendingStyle, setup::kNoticeStyle}) {
        TEST_CHECK(prebaked(style));
    }
    TEST_CHECK(theme::text::kAllStyles.size() == 30);
    std::cout << "  - styles share pre-baked atlases ok.\n";
}

void test_texture_names_exist() {
    const blaze4k::ThemeTextures& t = loaded_theme();
    for (const char* name : {"bg_select", "bar_top", "bar_hint", "wheel_row", "wheel_pack",
                             "wheel_row_selected", "stat_panel", "chip", "scanlines"}) {
        if (t.entry(name) == nullptr) {
            std::cerr << "    missing texture: " << name << "\n";
        }
        TEST_CHECK(t.entry(name) != nullptr);
    }
    std::cout << "  - every setup texture name is in the manifest ok.\n";
}

void render_all(blaze4k::ScreenManager& manager, blaze4k::GlQuadRenderer& renderer, bool with_text) {
    blaze4k::TextRenderer& text = loaded_text();
    for (const Size& size : kSizes) {
        if (with_text && size.w > 0) {
            text.set_window_size(size.w, size.h);
        }
        manager.render(renderer, size.w, size.h);
    }
    text.set_window_size(1280, 720);
}

void test_render_smoke() {
    blaze4k::GlQuadRenderer renderer; // uninitialised: draws are no-ops

    for (const bool services : {true, false}) {
        // Remap: every row as the selection, RESET, the message chip, capture.
        {
            blaze4k::GameConfig config;
            blaze4k::ScreenManager manager(0.0);
            auto owner = std::make_unique<blaze4k::InputRemapScreen>();
            blaze4k::InputRemapScreen* remap = owner.get();
            manager.add_screen(std::move(owner));
            manager.context().config = &config;
            if (services) {
                manager.context().theme = &loaded_theme();
                manager.context().text = &loaded_text();
            }
            manager.start(ScreenId::InputRemap);
            render_all(manager, renderer, services);
            for (int i = 0; i < 30; ++i) {
                manager.update(1.0 / 60.0, {press(GameAction::Down)});
                render_all(manager, renderer, services);
            }
            TEST_CHECK(remap->reset_selected());
            manager.update(1.0 / 60.0, {press(GameAction::Up)});
            TEST_CHECK(!remap->reset_selected() && remap->model().row == 23);
            render_all(manager, renderer, services);
            manager.update(1.0 / 60.0, {press(GameAction::Down)});
            TEST_CHECK(remap->reset_selected());
            manager.update(1.0 / 60.0, {press(GameAction::Confirm)});
            TEST_CHECK(remap->model().message == "DEFAULTS RESTORED");
            render_all(manager, renderer, services);
            manager.update(1.0 / 60.0, {press(GameAction::Up)});
            manager.update(1.0 / 60.0, {press(GameAction::Confirm)});
            TEST_CHECK(remap->capturing());
            render_all(manager, renderer, services);
            TEST_CHECK(remap->handle_back(manager.context()));
            TEST_CHECK(!remap->capturing());
            render_all(manager, renderer, services);
        }
        // Remap: a long custom binding takes the key-truncation path.
        {
            blaze4k::GameConfig config;
            config.input.key_bindings = {
                {"Left", {"A Very Long Custom Key Name That Cannot Possibly Fit In The Row"}}};
            blaze4k::ScreenManager manager(0.0);
            manager.add_screen(std::make_unique<blaze4k::InputRemapScreen>());
            manager.context().config = &config;
            if (services) {
                manager.context().theme = &loaded_theme();
                manager.context().text = &loaded_text();
            }
            manager.start(ScreenId::InputRemap);
            render_all(manager, renderer, services);
            manager.update(1.0 / 60.0, {press(GameAction::Down)});
            render_all(manager, renderer, services);
        }

        // Calibration: CountIn, Sampling, Ready (offset shown).
        {
            CalibrationConfig ccfg;
            FakeAudioStream stream;
            std::uint64_t frames = 0;
            constexpr std::uint32_t kRate = 48000;
            blaze4k::GameConfig config;
            blaze4k::ScreenManager manager(0.0);
            auto owner = std::make_unique<blaze4k::CalibrationScreen>(
                stream, [&frames] { return blaze4k::SamplePosition{frames, kRate}; }, ccfg);
            blaze4k::CalibrationScreen* cal = owner.get();
            manager.add_screen(std::move(owner));
            manager.context().config = &config;
            if (services) {
                manager.context().theme = &loaded_theme();
                manager.context().text = &loaded_text();
            }
            manager.start(ScreenId::Calibration);
            TEST_CHECK(cal->phase() == blaze4k::CalibrationPhase::CountIn);
            render_all(manager, renderer, services);

            auto set_time = [&frames](double seconds) {
                frames = static_cast<std::uint64_t>(std::llround(seconds * kRate));
            };
            set_time(ccfg.lead_in_seconds + 0.1);
            manager.update(0.0, {});
            TEST_CHECK(cal->phase() == blaze4k::CalibrationPhase::Sampling);
            render_all(manager, renderer, services);

            for (int i = 0; i < ccfg.min_samples; ++i) {
                set_time(ccfg.beat_time(i + 1) + 0.02);
                manager.update(0.0, {press(GameAction::Left)});
                render_all(manager, renderer, services);
            }
            TEST_CHECK(cal->phase() == blaze4k::CalibrationPhase::Ready);
            TEST_CHECK(cal->audio_available() && cal->result().ready);
            render_all(manager, renderer, services);
        }
        // Calibration without audio: the notice and the no-audio legend.
        {
            FakeAudioStream failing;
            failing.load_result = false;
            blaze4k::ScreenManager manager(0.0);
            auto owner = std::make_unique<blaze4k::CalibrationScreen>(
                failing, blaze4k::MusicClock::Source{}, CalibrationConfig{});
            blaze4k::CalibrationScreen* cal = owner.get();
            manager.add_screen(std::move(owner));
            if (services) {
                manager.context().theme = &loaded_theme();
                manager.context().text = &loaded_text();
            }
            manager.start(ScreenId::Calibration);
            TEST_CHECK(!cal->audio_available());
            render_all(manager, renderer, services);
            for (int i = 0; i < 12; ++i) {
                manager.update(0.5, {press(GameAction::Left)});
                render_all(manager, renderer, services);
            }
            TEST_CHECK(cal->result().ready);
            render_all(manager, renderer, services);
        }
    }
    std::cout << "  - InputRemapScreen and CalibrationScreen render with real/null services at "
                 "every size ok.\n";
}

} // namespace

int main() {
    std::cout << "setup_art_test\n";
    test_remap_display_rows();
    test_remap_layout();
    test_remap_labels();
    test_calibration_layout();
    test_legends();
    test_text_fits();
    test_styles_prebaked();
    test_texture_names_exist();
    test_render_smoke();
    std::cout << "setup_art_test: all passed\n";
    return 0;
}
