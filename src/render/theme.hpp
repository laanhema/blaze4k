#pragma once

// Blaze 4k "Cabinet" theme: colours, text styles and layout taken from the Cabinet v3 mock-ups.
// Layout values are in a 1280x720 reference space: multiply by (window_height / 720) and centre
// horizontally the same way the existing screens do. Baked textures live in assets/theme/cabinet/
// and are described in assets/theme/cabinet/manifest.json.

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/geometry.hpp"

namespace blaze4k::theme {

// 0xRRGGBB -> straight-alpha Color (same convention as the rest of the renderer).
[[nodiscard]] constexpr Color hex(std::uint32_t rgb, float alpha = 1.0f) {
    return Color{static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                 static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                 static_cast<float>(rgb & 0xFF) / 255.0f, alpha};
}

// ---------------------------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------------------------
namespace color {
// Grounds (the vignettes themselves are baked into bg_*.png)
constexpr Color kNavyDeep = hex(0x03040B);
constexpr Color kNavy = hex(0x0A1030);
constexpr Color kNavyPanel = hex(0x070A18);
constexpr Color kGameplayScrim = hex(0x000000, 0.72f); // over the song background

// Text
constexpr Color kWhite = hex(0xFFFFFF);
constexpr Color kText = hex(0xEAF1FF);
constexpr Color kIce = hex(0x9FD8FF);       // artist, stat labels, subtitle
constexpr Color kSteel = hex(0xA9B6CF);     // best %, footer, "/ 2000"
constexpr Color kHint = hex(0xC3CEE3);      // key-hint words
constexpr Color kWheelText = hex(0xD6E2FF); // unselected wheel rows
constexpr Color kArtistOnBar = hex(0xE3E9F5);

// Accents
constexpr Color kGold = hex(0xFFD633);      // the "this one" accent: keys, best %, BPM, PRESS START
constexpr Color kSelectedInk = hex(0x1B0D00); // text on the gold/orange selected wheel bar
constexpr Color kPackInk = hex(0x1E0B00);
constexpr Color kCyan = hex(0x2FD2FF);      // speed chip
constexpr Color kGreen = hex(0x3DFF7A);     // scroll chip
constexpr Color kShadow = hex(0x000000);    // hard 2-3px drop shadow under bright text

// Judgments (labels, bars, chips). Pops use the baked judgment_*.png instead.
constexpr Color kFantastic = hex(0x2FD2FF);
constexpr Color kExcellent = hex(0x6DFF6A);
constexpr Color kGreat = hex(0xFFD633);
constexpr Color kDecent = hex(0xFFFFFF);
constexpr Color kWayOff = hex(0xFF9A1F);
constexpr Color kMiss = hex(0xFF4A4A);
constexpr Color kMissLabel = hex(0xFF6A6A); // MISS text, a touch lighter for contrast
constexpr Color kHoldOkLabel = hex(0x6DFF9C);
constexpr Color kBarTrack = hex(0x121A3A);  // empty part of a score-screen judgment bar

// Life bar (the fill and frame are baked; these are for code-drawn fallbacks)
constexpr Color kLifeLow = hex(0x0A6FD1);
constexpr Color kLifeMid = hex(0x2FD2FF);
constexpr Color kLifeHigh = hex(0xBFF4FF);
constexpr double kLifeDangerThreshold = 0.3;

// Difficulty meter ticks
constexpr Color kTickOff = hex(0x26304F);
} // namespace color

struct DifficultyColors {
    Color fill; // tab, ticks, badge, results badge
    Color ink;  // text on the fill
};

// Order matches the select screen: Beginner/Novice, Easy, Medium, Hard, Challenge, Edit (neutral).
namespace difficulty {
constexpr DifficultyColors kBeginner{hex(0x9A6BFF), hex(0x14062E)};
constexpr DifficultyColors kEasy{hex(0x3DDC6A), hex(0x04210D)};
constexpr DifficultyColors kMedium{hex(0xFFD23A), hex(0x2A1D00)};
constexpr DifficultyColors kHard{hex(0xFF4A4A), hex(0x2A0000)};
constexpr DifficultyColors kChallenge{hex(0x3FA0FF), hex(0x001A33)};
constexpr DifficultyColors kEdit{hex(0xA9B6CF), hex(0x0A1030)};
} // namespace difficulty

// ---------------------------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------------------------
enum class Font { Audiowide, SairaMedium, SairaBold, SairaExtraBold, Count };
constexpr std::size_t kFontCount = static_cast<std::size_t>(Font::Count);

// Indexed by Font; the size is deduced so a missing or extra file fails the static_assert.
constexpr std::array kFontFiles = {
    "assets/fonts/Audiowide-Regular.ttf",
    "assets/fonts/SairaCondensed-Medium.ttf",
    "assets/fonts/SairaCondensed-Bold.ttf",
    "assets/fonts/SairaCondensed-ExtraBold.ttf",
};
static_assert(kFontFiles.size() == kFontCount, "kFontFiles must list one file per Font");

// Neither font has a real italic. The mock-ups use the browser's synthetic oblique:
// shear glyph quads by x += kItalicShear * (baseline_y - y).
constexpr float kItalicShear = 0.25f;

enum class Shadow { None, Hard2, Hard3 }; // draw the text again in kShadow, offset 2/3px down

struct TextStyle {
    Font font;
    float size_px;     // at 720p
    float tracking_px; // extra space after every glyph
    bool italic;
    Color color;
    Shadow shadow = Shadow::None;
};

namespace text {
// Title
constexpr TextStyle kFooter{Font::SairaBold, 18, 4, false, color::kSteel};
// Song select
constexpr TextStyle kSongTitle{Font::SairaExtraBold, 44, 0, true, color::kWhite, Shadow::Hard3};
constexpr TextStyle kArtist{Font::SairaBold, 24, 0, false, color::kIce};
constexpr TextStyle kBpm{Font::SairaExtraBold, 24, 2, true, color::kGold};
constexpr TextStyle kWheelRow{Font::SairaBold, 28, 0, false, color::kWheelText};
constexpr TextStyle kWheelSelected{Font::SairaExtraBold, 40, 0, true, color::kSelectedInk};
constexpr TextStyle kWheelPack{Font::SairaExtraBold, 28, 3, false, color::kPackInk};
constexpr TextStyle kDiffName{Font::SairaExtraBold, 20, 2, false, color::kWhite};   // colour = DifficultyColors::ink
constexpr TextStyle kDiffMeter{Font::SairaExtraBold, 28, 0, false, color::kWhite};
constexpr TextStyle kDiffBest{Font::SairaBold, 20, 0, false, color::kSteel};
constexpr TextStyle kDiffNameSelected{Font::SairaExtraBold, 22, 2, false, color::kWhite};
constexpr TextStyle kDiffMeterSelected{Font::SairaExtraBold, 32, 0, false, color::kWhite};
constexpr TextStyle kDiffBestSelected{Font::SairaExtraBold, 22, 0, false, color::kGold};
constexpr TextStyle kChip{Font::SairaExtraBold, 18, 3, false, color::kCyan};
constexpr TextStyle kHintWord{Font::SairaBold, 20, 2, false, color::kHint};
constexpr TextStyle kHintKey{Font::SairaBold, 20, 2, false, color::kGold};
// Gameplay
constexpr TextStyle kBadge{Font::SairaExtraBold, 24, 3, false, color::kWhite}; // colour = ink
constexpr TextStyle kComboNumber{Font::SairaExtraBold, 34, 0, true, color::kWhite, Shadow::Hard3};
constexpr TextStyle kComboLabel{Font::SairaExtraBold, 22, 3, true, color::kGold};
constexpr float kComboGroupShear = 0.176f; // the judgment + combo group is also skewed -10deg
// Score screen
constexpr TextStyle kBarSongTitle{Font::SairaExtraBold, 24, 0, true, color::kWhite, Shadow::Hard2};
constexpr TextStyle kBarArtist{Font::SairaBold, 18, 0, false, color::kArtistOnBar};
constexpr TextStyle kBarBadge{Font::SairaExtraBold, 18, 2, false, color::kWhite}; // colour = ink
constexpr TextStyle kStatLabel{Font::SairaBold, 18, 4, false, color::kIce};
constexpr TextStyle kTierLabel{Font::SairaExtraBold, 20, 6, false, color::kGold};
constexpr TextStyle kJudgmentLabel{Font::SairaExtraBold, 22, 0, false, color::kWhite}; // colour = judgment
constexpr TextStyle kJudgmentCount{Font::SairaExtraBold, 22, 0, false, color::kWhite};
// Big numbers on the score screen use the baked bitmap fonts:
//   digits_chrome (Audiowide 78px, the percentage) and digits_white (Audiowide 48px, stats).
} // namespace text

// ---------------------------------------------------------------------------------------------
// Layout (1280x720 reference)
// ---------------------------------------------------------------------------------------------
namespace layout {
constexpr float kRefWidth = 1280.0f;
constexpr float kRefHeight = 720.0f;

// Shared chrome
constexpr float kTopBarHeight = 64.0f;   // + 2px black rule (baked in bar_top.png)
constexpr float kTopBarPadX = 40.0f;
constexpr float kHintBarHeight = 52.0f;  // score screen: 44
constexpr float kHintGap = 34.0f;        // between hint pairs

// Title
constexpr float kLogoTop = 168.0f;
constexpr float kSubtitleTop = 362.0f;
constexpr float kTitleArrowsTop = 402.0f;  // four Cel tap notes, 96px, 108px pitch (same as the field)
constexpr float kPressStartTop = 548.0f;
constexpr float kFooterHeight = 40.0f;
constexpr float kFooterPadX = 36.0f;

// Song select
constexpr Rect kBanner{48.0f, 100.0f, 560.0f, 157.0f};  // banner_frame.png sits 4px outside this
constexpr float kInfoX = 48.0f;
constexpr float kSongTitleTop = 270.0f;
constexpr float kArtistTop = 326.0f;
constexpr float kDiffListX = 44.0f;
constexpr float kDiffListTop = 372.0f;
constexpr float kDiffRowHeight = 44.0f;
constexpr float kDiffRowSelectedHeight = 52.0f;
constexpr float kDiffRowGap = 8.0f;
constexpr float kDiffRowSelectedShiftX = 14.0f;
constexpr float kDiffTickPitch = 18.0f; // 14px tick + 4px gap, 10 ticks
constexpr float kWheelX = 676.0f;
constexpr float kWheelTop = 92.0f;
constexpr float kWheelWidth = 640.0f;   // runs off the right edge on purpose
constexpr float kWheelRowHeight = 62.0f;
constexpr float kWheelRowSelectedHeight = 92.0f;
constexpr float kWheelRowGap = 10.0f;
// Indent per distance from the selected row (0 = selected): fakes the curve of the wheel.
constexpr std::array<float, 4> kWheelIndent = {0.0f, 46.0f, 76.0f, 96.0f};

// Gameplay (the note field, receptors and Cel noteskin are unchanged)
constexpr Rect kDiffBadge{36.0f, 28.0f, 130.0f, 40.0f};
constexpr Rect kLifeBar{40.0f, 120.0f, 40.0f, 480.0f}; // outer frame; 4px chrome border
constexpr float kJudgmentTop = 296.0f;   // judgment sprite content box top, centred on the field
constexpr float kComboTop = 368.0f;

// Score screen
constexpr Rect kMedallion{450.0f, 110.0f, 380.0f, 380.0f};
constexpr float kTierLabelTop = 388.0f;
constexpr float kPercentTop = 500.0f;     // digits_chrome, centred
constexpr Rect kRecordRibbon{470.0f, 600.0f, 340.0f, 44.0f};
constexpr float kStatPanelX = 44.0f;
constexpr float kStatPanelTop = 120.0f;
constexpr float kStatPanelWidth = 360.0f;
constexpr float kStatPanelGap = 14.0f;
constexpr float kJudgmentBarsX = 878.0f;
constexpr float kJudgmentBarsTop = 120.0f;
constexpr float kJudgmentBarsWidth = 360.0f;
constexpr float kJudgmentBarRowPitch = 45.0f;
constexpr float kJudgmentBarHeight = 14.0f;
constexpr float kJudgmentBarLabelWidth = 130.0f;
constexpr float kJudgmentBarCountWidth = 56.0f;
} // namespace layout

// Skews used across the theme (tan of the CSS skewX angle), for code-drawn parallelograms.
namespace skew {
constexpr float kLogo = 0.176f;       // -10deg
constexpr float kRows = 0.213f;       // -12deg: difficulty rows, chips, badges, PRESS START
constexpr float kWheel = 0.249f;      // -14deg: wheel rows, NEW RECORD ribbon
constexpr float kStatPanel = 0.141f;  // -8deg
} // namespace skew

} // namespace blaze4k::theme
