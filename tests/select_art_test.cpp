// #94: Cabinet Song Select art. Pins the select_art layout table (720p, 1440p,
// 21:9, 16:10), the list window and visible-row rules, the wheel display rows,
// slide easing and slide culling (no row leaves reference y 0..720), the
// TrueType-coverage display-text overloads, the difficulty row style/label/tick rules, the chip texts and
// boxes, the hint line and arrow geometry, that every texture name the screen
// uses is in the real manifest, and renders SelectScreen with the real headless
// theme and text services (populated, empty, options overlay, null services).
// #96: the options overlay in the Cabinet look (options_art): the panel and row
// layout and room budget, text fit with the real fonts, the options legend, the
// pre-baked styles, and a render walk over every overlay row.
// #110: subtitle display choice (TrueType coverage), the real-font title/subtitle
// fit for same-title songs, and subtitled songs (one with an empty title) in the
// render smoke.
// #122: the meter number and ticks sit 16px right; real-font clearance from the
// slanted tab and the first tick.
// #126: the difficulty name sits 6px right (10px in the selected row); gap to the
// baked tab's left edge and the unchanged right limit.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chart/chart.hpp"
#include "chart/song_library.hpp"
#include "data/config.hpp"
#include "data/high_scores.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/options_art.hpp"
#include "screens/options_menu.hpp"
#include "screens/play_request.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_art.hpp"
#include "screens/select_screen.hpp"
#include "screens/song_display_text.hpp"

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

using blaze4k::Chart;
using blaze4k::GameAction;
using blaze4k::InputEvent;
using blaze4k::Rect;
using blaze4k::ScreenId;
using blaze4k::StepsDifficulty;
using blaze4k::Vec2;

const fs::path kSourceDir{BLAZE4K_SOURCE_DIR};
const fs::path kAssets{BLAZE4K_ASSETS_DIR};
const fs::path kCabinet = kAssets / "theme" / "cabinet";

struct Size {
    int w, h;
};
constexpr Size kSizes[] = {{1280, 720}, {2560, 1440}, {3440, 1440}, {1920, 1200}};

bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

bool rect_eq(const Rect& r, float x, float y, float w, float h) {
    return approx(r.x, x) && approx(r.y, y) && approx(r.w, w) && approx(r.h, h);
}

bool vec_eq(Vec2 v, float x, float y) {
    return approx(v.x, x) && approx(v.y, y);
}

bool same_color(blaze4k::Color a, blaze4k::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

Chart make_chart(const std::string& difficulty, const std::string& description, int meter) {
    Chart chart;
    chart.difficulty = difficulty;
    chart.description = description;
    chart.meter = meter;
    return chart;
}

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
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

void test_list_window() {
    auto win = [](int sel, int count, int visible) { return art::list_window(sel, count, visible); };
    auto is = [](art::ListWindow w, int first, int last) { return w.first == first && w.last == last; };

    TEST_CHECK(is(win(0, 0, 7), 0, -1));
    TEST_CHECK(is(win(3, 0, 7), 0, -1));
    TEST_CHECK(is(win(0, 1, 7), 0, 0));
    TEST_CHECK(is(win(0, 7, 7), 0, 6));
    TEST_CHECK(is(win(6, 7, 7), 0, 6));
    TEST_CHECK(is(win(0, 8, 7), 0, 6));
    TEST_CHECK(is(win(3, 8, 7), 0, 6));
    TEST_CHECK(is(win(4, 8, 7), 1, 7));
    TEST_CHECK(is(win(7, 8, 7), 1, 7));
    TEST_CHECK(is(win(0, 30, 7), 0, 6));
    TEST_CHECK(is(win(15, 30, 7), 12, 18));
    TEST_CHECK(is(win(29, 30, 7), 23, 29));
    // Out-of-range selections clamp; a non-positive visible count shows one row.
    TEST_CHECK(is(win(-4, 30, 7), 0, 6));
    TEST_CHECK(is(win(99, 30, 7), 23, 29));
    TEST_CHECK(is(win(5, 30, 0), 5, 5));

    // The pre-#94 13-row wheel rule, reproduced (first = clamp(sel - 6, 0, n - 13)).
    TEST_CHECK(is(win(0, 30, 13), 0, 12));
    TEST_CHECK(is(win(15, 30, 13), 9, 21));
    TEST_CHECK(is(win(29, 30, 13), 17, 29));
    TEST_CHECK(is(win(4, 5, 13), 0, 4));

    // The difficulty list: 5 rows around the chart cursor.
    TEST_CHECK(is(win(3, 5, 5), 0, 4));
    TEST_CHECK(is(win(6, 9, 5), 4, 8));
    TEST_CHECK(is(win(4, 9, 5), 2, 6));
    std::cout << "  - list window ok.\n";
}

void test_visible_rows() {
    TEST_CHECK(art::kHintBarTop == 666.0f);
    TEST_CHECK(art::kDiffRowPitch == 54.0f);
    TEST_CHECK(art::kWheelPitch == 76.0f);
    TEST_CHECK(art::visible_rows(theme::layout::kDiffListTop, art::kHintBarTop, art::kDiffRowPitch,
                                 theme::layout::kDiffRowSelectedHeight) == art::kDiffVisibleRows);
    TEST_CHECK(art::visible_rows(theme::layout::kWheelTop, art::kHintBarTop, art::kWheelPitch,
                                 theme::layout::kWheelRowSelectedHeight) == art::kWheelVisibleRows);
    TEST_CHECK(art::kDiffVisibleRows == 5 && art::kWheelVisibleRows == 7);
    // Degenerate input never yields fewer than one row.
    TEST_CHECK(art::visible_rows(0.0f, 100.0f, 0.0f, 10.0f) == 1);
    TEST_CHECK(art::visible_rows(500.0f, 100.0f, 50.0f, 10.0f) == 1);
    TEST_CHECK(art::visible_rows(0.0f, NAN, 50.0f, 10.0f) == 1);
    // The last visible row ends above the hint-bar rule, whichever slot is selected.
    for (int k = 0; k < art::kDiffVisibleRows; ++k) {
        const Rect last = art::difficulty_row_rect(art::kDiffVisibleRows - 1, k);
        TEST_CHECK(last.y + last.h <= art::kHintBarTop);
    }
    for (int k = 0; k < art::kWheelVisibleRows; ++k) {
        const Rect last = art::wheel_row_rect(art::kWheelVisibleRows - 1, k);
        TEST_CHECK(last.y + last.h <= art::kHintBarTop);
    }
    std::cout << "  - visible rows from the layout ok.\n";
}

void test_difficulty_row_rects() {
    // 5 charts, selected index 3 (window 0..4, selected slot 3).
    const Rect expect[5] = {{44, 372, 564, 44}, {44, 426, 564, 44}, {44, 480, 564, 44},
                            {58, 534, 564, 52}, {44, 596, 564, 44}};
    for (int i = 0; i < 5; ++i) {
        const Rect r = art::difficulty_row_rect(i, 3);
        TEST_CHECK(rect_eq(r, expect[i].x, expect[i].y, expect[i].w, expect[i].h));
    }
    // Screen rects at the four sizes (the selected row).
    const Rect sel = art::difficulty_row_rect(3, 3);
    const Rect want[4] = {{58, 534, 564, 52}, {116, 1068, 1128, 104}, {556, 1068, 1128, 104},
                          {87, 861, 846, 78}};
    for (int i = 0; i < 4; ++i) {
        const Rect r = theme::layout_scale(kSizes[i].w, kSizes[i].h).rect(sel);
        TEST_CHECK(rect_eq(r, want[i].x, want[i].y, want[i].w, want[i].h));
    }
    // First row at 1920x1200: y = 60 + 372 * 1.5.
    TEST_CHECK(rect_eq(theme::layout_scale(1920, 1200).rect(art::difficulty_row_rect(0, 3)), 66,
                       618, 846, 66));
    std::cout << "  - difficulty row rects ok.\n";
}

void test_wheel_row_rects() {
    // The mock: pack, song, selected, song, song.
    const float ys[5] = {92, 168, 244, 350, 426};
    const float xs[5] = {752, 722, 676, 722, 752};
    for (int j = 0; j < 5; ++j) {
        const Rect r = art::wheel_row_rect(j, 2);
        TEST_CHECK(rect_eq(r, xs[j], ys[j], 640, j == 2 ? 92.0f : 62.0f));
    }
    const struct {
        float x, y, s;
    } maps[4] = {{0, 0, 1}, {0, 0, 2}, {440, 0, 2}, {0, 60, 1.5f}};
    for (int i = 0; i < 4; ++i) {
        const theme::LayoutScale L = theme::layout_scale(kSizes[i].w, kSizes[i].h);
        for (int j = 0; j < 5; ++j) {
            const Rect r = L.rect(art::wheel_row_rect(j, 2));
            TEST_CHECK(rect_eq(r, maps[i].x + xs[j] * maps[i].s, maps[i].y + ys[j] * maps[i].s,
                               640 * maps[i].s, (j == 2 ? 92.0f : 62.0f) * maps[i].s));
        }
    }
    // Indent clamps at a distance of 3; slide rows outside the window still place.
    TEST_CHECK(art::wheel_indent(0) == 0.0f && art::wheel_indent(1) == 46.0f);
    TEST_CHECK(art::wheel_indent(2) == 76.0f && art::wheel_indent(3) == 96.0f);
    TEST_CHECK(art::wheel_indent(9) == 96.0f && art::wheel_indent(-2) == 76.0f);
    TEST_CHECK(rect_eq(art::wheel_row_rect(6, 2), 772, 92 + 6 * 76 + 30, 640, 62));
    TEST_CHECK(rect_eq(art::wheel_row_rect(-1, 2), 772, 16, 640, 62));
    std::cout << "  - wheel row rects + indents ok.\n";
}

void test_build_wheel_rows() {
    using Kind = art::WheelRow::Kind;
    const std::vector<int> packs = {0, 0, 1, 1, 1};
    const art::WheelRows rows = art::build_wheel_rows(packs);
    TEST_CHECK(rows.rows.size() == 7);
    const Kind kinds[7] = {Kind::Pack, Kind::Song, Kind::Song, Kind::Pack,
                           Kind::Song, Kind::Song, Kind::Song};
    const int pack_of[7] = {0, 0, 0, 1, 1, 1, 1};
    const int song_of[7] = {-1, 0, 1, -1, 2, 3, 4};
    for (std::size_t i = 0; i < 7; ++i) {
        TEST_CHECK(rows.rows[i].kind == kinds[i]);
        TEST_CHECK(rows.rows[i].pack_index == pack_of[i]);
        TEST_CHECK(rows.rows[i].song_index == song_of[i]);
    }
    TEST_CHECK((rows.song_row == std::vector<int>{1, 2, 4, 5, 6}));
    for (std::size_t s = 0; s < rows.song_row.size(); ++s) {
        TEST_CHECK(rows.rows[static_cast<std::size_t>(rows.song_row[s])].song_index ==
                   static_cast<int>(s));
    }

    const art::WheelRows empty = art::build_wheel_rows({});
    TEST_CHECK(empty.rows.empty() && empty.song_row.empty());

    const std::vector<int> one = {3};
    const art::WheelRows single = art::build_wheel_rows(one);
    TEST_CHECK(single.rows.size() == 2 && single.song_row == std::vector<int>{1});
    TEST_CHECK(single.rows[0].kind == Kind::Pack && single.rows[0].pack_index == 3);
    std::cout << "  - wheel display rows (inline pack headers) ok.\n";
}

void test_scroll_easing() {
    TEST_CHECK(art::kWheelScrollSeconds == 0.08);
    TEST_CHECK(art::wheel_scroll_offset(76.0f, 0.0) == 76.0f);
    TEST_CHECK(approx(art::wheel_scroll_offset(76.0f, 0.04), 9.5f));
    TEST_CHECK(art::wheel_scroll_offset(76.0f, 0.08) == 0.0f);
    TEST_CHECK(art::wheel_scroll_offset(76.0f, 1.0) == 0.0f);
    TEST_CHECK(approx(art::wheel_scroll_offset(-76.0f, 0.04), -9.5f));
    TEST_CHECK(art::wheel_scroll_offset(76.0f, -1.0) == 76.0f);
    TEST_CHECK(art::wheel_scroll_offset(NAN, 0.0) == 0.0f);
    TEST_CHECK(art::wheel_scroll_offset(76.0f, NAN) == 0.0f);
    // Monotonic ease-out towards 0.
    float previous = 76.0f;
    for (int i = 1; i <= 10; ++i) {
        const float now = art::wheel_scroll_offset(76.0f, i * 0.008);
        TEST_CHECK(now <= previous && now >= 0.0f);
        previous = now;
    }

    TEST_CHECK(art::wheel_scroll_start(30.0f, 0) == 30.0f);
    TEST_CHECK(art::wheel_scroll_start(0.0f, 1) == 76.0f);
    TEST_CHECK(art::wheel_scroll_start(0.0f, -1) == -76.0f);
    TEST_CHECK(art::wheel_scroll_start(10.0f, 1) == 86.0f);
    TEST_CHECK(art::wheel_scroll_start(0.0f, 2) == 152.0f);
    TEST_CHECK(art::wheel_scroll_start(0.0f, -2) == -152.0f);
    TEST_CHECK(art::wheel_scroll_start(0.0f, 5) == 0.0f);
    TEST_CHECK(art::wheel_scroll_start(40.0f, -5) == 0.0f);
    TEST_CHECK(art::wheel_scroll_start(100.0f, 1) == 152.0f);
    TEST_CHECK(art::wheel_scroll_start(-100.0f, -1) == -152.0f);
    TEST_CHECK(art::kWheelScrollMax == 152.0f);
    std::cout << "  - wheel slide easing + start rule ok.\n";
}

// Review of #94 (Medium 1): while the wheel slides, no drawn row may leave the
// reference column's height (y 0..720), or it paints into the letterbox bands
// of a 16:10 / 4:3 window. Every offset the slide can reach is swept: [-152,
// 152] in 0.25px steps, plus the eased offsets of every start the rule gives.
void test_wheel_slide_stays_in_column() {
    TEST_CHECK(art::kWheelMaxSlideRows == 2);
    std::vector<float> offsets;
    for (int q = -608; q <= 608; ++q) {
        offsets.push_back(static_cast<float>(q) * 0.25f);
    }
    for (const float start : {-152.0f, -76.0f, 76.0f, 152.0f}) {
        for (int ms = 0; ms <= 80; ++ms) {
            offsets.push_back(art::wheel_scroll_offset(start, ms * 0.001));
        }
    }

    constexpr int kCount = 40; // a long list: windows at the top, middle and end
    for (int selected = 0; selected < kCount; ++selected) {
        const art::ListWindow window = art::list_window(selected, kCount, art::kWheelVisibleRows);
        const int selected_slot = selected - window.first;
        for (const float offset : offsets) {
            const art::ListWindow range = art::wheel_slide_range(window, kCount, offset);
            TEST_CHECK(range.first >= 0 && range.last <= kCount - 1);
            TEST_CHECK(range.last - range.first + 1 <= art::kWheelVisibleRows + art::kWheelMaxSlideRows);
            float top = 1e9f;
            float bottom = -1e9f;
            int drawn = 0;
            for (int r = range.first; r <= range.last; ++r) {
                const Rect rect = art::wheel_slide_rect(r - window.first, selected_slot, offset);
                if (!art::wheel_row_in_column(rect)) {
                    continue;
                }
                ++drawn;
                TEST_CHECK(rect.y >= 0.0f && rect.y + rect.h <= theme::layout::kRefHeight);
                top = std::min(top, rect.y);
                bottom = std::max(bottom, rect.y + rect.h);
            }
            TEST_CHECK(drawn >= art::kWheelVisibleRows - 2);
            // Culling never opens a hole where rows sit at rest: in mid-list the
            // drawn rows still reach the rest top (92) and rest bottom (640).
            if (window.first >= art::kWheelMaxSlideRows &&
                window.last + art::kWheelMaxSlideRows <= kCount - 1) {
                TEST_CHECK(top <= theme::layout::kWheelTop);
                TEST_CHECK(bottom >= art::wheel_row_rect(art::kWheelVisibleRows - 1, selected_slot).y +
                                         theme::layout::kWheelRowHeight);
            }
        }
    }

    // The leaks the review measured at 1920x1200 are now culled...
    TEST_CHECK(!art::wheel_row_in_column(art::wheel_slide_rect(-2, 3, 10.0f)));   // Down press
    TEST_CHECK(!art::wheel_row_in_column(art::wheel_slide_rect(8, 3, -40.0f)));   // Up press
    TEST_CHECK(!art::wheel_row_in_column(art::wheel_slide_rect(8, 3, -10.0f)));
    TEST_CHECK(!art::wheel_row_in_column(art::wheel_slide_rect(6, 3, 152.0f)));   // header, Down
    TEST_CHECK(!art::wheel_row_in_column(art::wheel_slide_rect(0, 3, -152.0f)));  // header, Up
    // ...while the rows that fill the exposed side are still drawn.
    TEST_CHECK(art::wheel_row_in_column(art::wheel_slide_rect(-2, 3, 152.0f)));
    TEST_CHECK(art::wheel_row_in_column(art::wheel_slide_rect(-1, 3, 76.0f)));
    TEST_CHECK(art::wheel_row_in_column(art::wheel_slide_rect(8, 3, -152.0f)));
    TEST_CHECK(art::wheel_row_in_column(art::wheel_slide_rect(7, 3, -76.0f)));
    // The gold selected bar does not slide.
    TEST_CHECK(rect_eq(art::wheel_slide_rect(3, 3, 152.0f), 676, 92 + 3 * 76, 640, 92));
    TEST_CHECK(!art::wheel_row_in_column(Rect{676, NAN, 640, 62}));

    // Extra rows only on the side the rows moved away from, clamped to the list.
    const auto is = [](art::ListWindow w, int first, int last) {
        return w.first == first && w.last == last;
    };
    TEST_CHECK(is(art::wheel_slide_range({12, 18}, 30, 0.0f), 12, 18));
    TEST_CHECK(is(art::wheel_slide_range({12, 18}, 30, 5.0f), 10, 18));
    TEST_CHECK(is(art::wheel_slide_range({12, 18}, 30, -5.0f), 12, 20));
    TEST_CHECK(is(art::wheel_slide_range({1, 7}, 9, 5.0f), 0, 7));
    TEST_CHECK(is(art::wheel_slide_range({1, 7}, 9, -5.0f), 1, 8));
    TEST_CHECK(is(art::wheel_slide_range({0, -1}, 0, -5.0f), 0, -1));
    std::cout << "  - wheel slide stays inside reference y 0..720 ok.\n";
}

// Review of #94 (Low 1): the TrueType-coverage display-text overloads with the
// real headless TextRenderer.
void test_display_text_coverage() {
    const blaze4k::TextRenderer& text = loaded_text();
    const theme::Font font = theme::text::kSongTitle.font;
    TEST_CHECK(text.font_available(font));

    blaze4k::SongMetadata cafe;
    cafe.title = "Caf\xC3\xA9";
    cafe.title_translit = "Cafe";
    cafe.artist = "Beyonc\xC3\xA9";
    cafe.artist_translit = "Beyonce";
    // Latin-1 native: the TTF covers it (the bitmap rule picked the translit).
    TEST_CHECK(&blaze4k::song_display_title(cafe, &text, font) == &cafe.title);
    TEST_CHECK(&blaze4k::song_display_artist(cafe, &text, font) == &cafe.artist);
    TEST_CHECK(&blaze4k::song_display_title(cafe) == &cafe.title_translit);

    blaze4k::SongMetadata cjk;
    cjk.title = "\xE6\x84\x9B\xE3\x81\x97\xE3\x81\xA6"; // 愛して
    cjk.title_translit = "Aishite";
    cjk.artist = "\xE5\x88\x83";                                // 刃
    cjk.artist_translit = "Yaiba";
    // CJK native with a translit: the translit is chosen.
    TEST_CHECK(!text.covers_text(cjk.title, font));
    TEST_CHECK(&blaze4k::song_display_title(cjk, &text, font) == &cjk.title_translit);
    TEST_CHECK(&blaze4k::song_display_artist(cjk, &text, font) == &cjk.artist_translit);

    // CJK native without a translit: the native text is kept (placeholder glyphs).
    blaze4k::SongMetadata bare;
    bare.title = cjk.title;
    bare.artist = cjk.artist;
    TEST_CHECK(&blaze4k::song_display_title(bare, &text, font) == &bare.title);
    TEST_CHECK(&blaze4k::song_display_artist(bare, &text, font) == &bare.artist);

    // #124: a symbol-only artist (Delirium's "☺") is covered by the symbol
    // fallback font, so the native text is kept with or without a translit.
    const theme::Font artist_font = theme::text::kArtist.font;
    blaze4k::SongMetadata delirium;
    delirium.title = "Delirium";
    delirium.artist = "\xE2\x98\xBA";
    delirium.artist_translit = "";
    TEST_CHECK(text.symbol_font_available());
    TEST_CHECK(text.covers_text(delirium.artist, artist_font));
    TEST_CHECK(&blaze4k::song_display_artist(delirium, &text, artist_font) == &delirium.artist);
    blaze4k::SongMetadata smiley = delirium;
    smiley.artist_translit = "Smiley";
    TEST_CHECK(&blaze4k::song_display_artist(smiley, &text, artist_font) == &smiley.artist);
    // The bitmap rule cannot draw it, so it still picks the translit when present.
    TEST_CHECK(&blaze4k::song_display_artist(smiley, nullptr, artist_font) ==
               &smiley.artist_translit);
    TEST_CHECK(&blaze4k::song_display_artist(delirium, nullptr, artist_font) == &delirium.artist);

    // A null renderer falls back to the bitmap rule.
    TEST_CHECK(&blaze4k::song_display_title(cafe, nullptr, font) == &cafe.title_translit);
    TEST_CHECK(&blaze4k::song_display_artist(cafe, nullptr, font) == &cafe.artist_translit);
    TEST_CHECK(&blaze4k::song_display_title(cjk, nullptr, font) == &cjk.title_translit);
    TEST_CHECK(&blaze4k::song_display_title(bare, nullptr, font) == &bare.title);

    // #110: the subtitle follows the same rule, on its own fields.
    const theme::Font sub_font = theme::text::kSongSubtitle.font;
    TEST_CHECK(text.font_available(sub_font));
    blaze4k::SongMetadata cafe_mix;
    cafe_mix.title = "Plain";
    cafe_mix.subtitle = "Caf\xC3\xA9 Mix";
    cafe_mix.subtitle_translit = "Cafe Mix";
    TEST_CHECK(&blaze4k::song_display_subtitle(cafe_mix, &text, sub_font) == &cafe_mix.subtitle);
    TEST_CHECK(&blaze4k::song_display_subtitle(cafe_mix) == &cafe_mix.subtitle_translit);
    TEST_CHECK(&blaze4k::song_display_subtitle(cafe_mix, nullptr, sub_font) ==
               &cafe_mix.subtitle_translit);
    blaze4k::SongMetadata cjk_sub;
    cjk_sub.title = "Plain";
    cjk_sub.subtitle = cjk.title;
    cjk_sub.subtitle_translit = "Aishite Mix";
    TEST_CHECK(&blaze4k::song_display_subtitle(cjk_sub, &text, sub_font) ==
               &cjk_sub.subtitle_translit);
    // Chosen independently of the title: the ASCII title stays native.
    TEST_CHECK(&blaze4k::song_display_title(cjk_sub, &text, font) == &cjk_sub.title);
    blaze4k::SongMetadata cjk_bare_sub;
    cjk_bare_sub.subtitle = cjk.title;
    TEST_CHECK(&blaze4k::song_display_subtitle(cjk_bare_sub, &text, sub_font) ==
               &cjk_bare_sub.subtitle);
    std::cout << "  - TrueType-coverage display text ok.\n";
}

// #110: the three ITG "Disconnected" songs fit whole (title and subtitle) at every
// wheel and info-panel budget with the real fonts, so the rows read differently;
// an overflowing title + subtitle truncates without overlap and the subtitle
// keeps >= 40% of the budget.
void test_disconnected_subtitles_fit() {
    blaze4k::TextRenderer& text = loaded_text(); // 1280x720: window px == reference px
    TEST_CHECK(approx(text.scale(), 1.0f));
    const float gap = art::kSubtitleGap;
    struct Site {
        const theme::TextStyle* title;
        const theme::TextStyle* sub;
        float budget;
    };
    // Worst wheel budgets: a plain row at the deepest indent, and the selected bar.
    const Rect deep = art::wheel_row_rect(3, 0);
    const Rect sel = art::wheel_row_rect(3, 3);
    TEST_CHECK(deep.x > sel.x);
    const Site sites[] = {
        {&theme::text::kWheelRow, &theme::text::kWheelSubtitle,
         art::kWheelTextRight - (deep.x + art::kWheelSongTextX)},
        {&theme::text::kWheelSelected, &theme::text::kWheelSelectedSubtitle,
         art::kWheelTextRight - (sel.x + art::kWheelSelectedTextX)},
        {&theme::text::kSongTitle, &theme::text::kSongSubtitle, art::kInfoWidth},
    };
    for (const Site& site : sites) {
        TEST_CHECK(site.budget > 0.0f);
        const float title_w = text.measure("Disconnected", *site.title);
        std::vector<std::string> fitted;
        for (const char* sub : {"-Hyper-", "-Mobius-", "-Hardkore-"}) {
            const float sub_w = text.measure(sub, *site.sub);
            const blaze4k::TitleSubtitleFit fit =
                blaze4k::fit_title_subtitle(title_w, sub_w, gap, site.budget);
            TEST_CHECK(approx(fit.title_max_w, title_w));
            TEST_CHECK(approx(fit.subtitle_max_w, sub_w));
            TEST_CHECK(text.truncate("Disconnected", *site.title, fit.title_max_w) ==
                       "Disconnected");
            fitted.push_back(text.truncate(sub, *site.sub, fit.subtitle_max_w));
            TEST_CHECK(fitted.back() == sub);
        }
        TEST_CHECK(fitted[0] != fitted[1] && fitted[1] != fitted[2] && fitted[0] != fitted[2]);

        // Overflow: a long title + the pack's longest subtitle.
        const std::string long_title =
            "A Song With Many Charts And A Very Long Title That Will Not Fit";
        const std::string long_sub = "(Two Gees Radio Edit)";
        const float lt_w = text.measure(long_title, *site.title);
        const float ls_w = text.measure(long_sub, *site.sub);
        TEST_CHECK(lt_w + gap + ls_w > site.budget);
        const blaze4k::TitleSubtitleFit fit =
            blaze4k::fit_title_subtitle(lt_w, ls_w, gap, site.budget);
        TEST_CHECK(fit.subtitle_max_w >=
                   std::min(ls_w, (site.budget - gap) * blaze4k::kSubtitleMinShare) - 1e-3f);
        const std::string t = text.truncate(long_title, *site.title, fit.title_max_w);
        const std::string sub = text.truncate(long_sub, *site.sub, fit.subtitle_max_w);
        TEST_CHECK(t.size() >= 3 && t.compare(t.size() - 3, 3, "...") == 0);
        TEST_CHECK(!sub.empty());
        // The subtitle starts after the measured fitted title + gap and ends inside the budget.
        TEST_CHECK(text.measure(t, *site.title) + gap + text.measure(sub, *site.sub) <=
                   site.budget + 1e-3f);
    }
    std::cout << "  - Disconnected -Hyper-/-Mobius-/-Hardkore- fit whole; overflow truncates ok.\n";
}

void test_row_style() {
    struct Expect {
        const char* label;
        int meter;
        StepsDifficulty kind;
        const char* texture;
        theme::DifficultyColors colors;
    };
    const Expect rows[] = {
        {"Beginner", 1, StepsDifficulty::Beginner, "diff_row_beginner", theme::difficulty::kBeginner},
        {"easy", 3, StepsDifficulty::Easy, "diff_row_easy", theme::difficulty::kEasy},
        {"MEDIUM", 5, StepsDifficulty::Medium, "diff_row_medium", theme::difficulty::kMedium},
        {"Hard", 8, StepsDifficulty::Hard, "diff_row_hard", theme::difficulty::kHard},
        {"Challenge", 11, StepsDifficulty::Challenge, "diff_row_challenge",
         theme::difficulty::kChallenge},
        {"Expert", 11, StepsDifficulty::Challenge, "diff_row_challenge",
         theme::difficulty::kChallenge},
        // OpenITG TidyUpData: an unknown label is classified by its meter.
        {"Novice", 1, StepsDifficulty::Beginner, "diff_row_beginner", theme::difficulty::kBeginner},
        {"Novice", 5, StepsDifficulty::Medium, "diff_row_medium", theme::difficulty::kMedium},
        {"Wild", 9, StepsDifficulty::Hard, "diff_row_hard", theme::difficulty::kHard},
    };
    for (const Expect& e : rows) {
        const art::DifficultyRowStyle style = art::difficulty_row_style(make_chart(e.label, "", e.meter));
        TEST_CHECK(style.kind == e.kind);
        TEST_CHECK(style.baked);
        TEST_CHECK(style.texture == e.texture);
        TEST_CHECK(style.texture_selected == std::string(e.texture) + "_selected");
        TEST_CHECK(same_color(style.colors.fill, e.colors.fill));
        TEST_CHECK(same_color(style.colors.ink, e.colors.ink));
    }
    for (const Chart& chart : {make_chart("Edit", "", 10), make_chart("edit", "JBEAN", 12),
                               make_chart("", "Edit", 4)}) {
        const art::DifficultyRowStyle style = art::difficulty_row_style(chart);
        TEST_CHECK(style.kind == StepsDifficulty::Edit);
        TEST_CHECK(!style.baked && style.texture.empty() && style.texture_selected.empty());
        TEST_CHECK(same_color(style.colors.fill, theme::difficulty::kEdit.fill));
        TEST_CHECK(same_color(style.colors.ink, theme::difficulty::kEdit.ink));
    }
    std::cout << "  - difficulty row style (OpenITG classification) ok.\n";
}

void test_labels() {
    TEST_CHECK(art::difficulty_row_label(make_chart("Hard", "Some Author", 9)) == "HARD");
    TEST_CHECK(art::difficulty_row_label(make_chart("Expert", "", 11)) == "EXPERT");
    TEST_CHECK(art::difficulty_row_label(make_chart("Novice", "", 1)) == "NOVICE");
    TEST_CHECK(art::difficulty_row_label(make_chart("Edit", "JBEAN", 10)) == "JBEAN");
    TEST_CHECK(art::difficulty_row_label(make_chart("edit", "my edit", 10)) == "my edit");
    TEST_CHECK(art::difficulty_row_label(make_chart("Edit", "", 10)) == "EDIT");
    // An empty passthrough label shows the resolved difficulty's name.
    TEST_CHECK(art::difficulty_row_label(make_chart("", "", 9)) == "HARD");
    TEST_CHECK(art::difficulty_row_label(make_chart("", "", 1)) == "BEGINNER");
    // Edit names keep their UTF-8 bytes; ascii_upper leaves non-ASCII alone.
    const std::string utf8 = "Caf\xC3\xA9 edit";
    TEST_CHECK(art::difficulty_row_label(make_chart("Edit", utf8, 10)) == utf8);
    TEST_CHECK(art::ascii_upper("caf\xC3\xA9 pack 2") == "CAF\xC3\xA9 PACK 2");

    // A long name is truncated by measured width to the tab budget.
    blaze4k::TextRenderer& text = loaded_text();
    const std::string label =
        art::difficulty_row_label(make_chart("Edit", "mDaWg & Hatena Zubon Extended Remix", 10));
    for (const theme::TextStyle& style : {theme::text::kDiffName, theme::text::kDiffNameSelected}) {
        const std::string shown = text.truncate(label, style, art::kDiffNameBudget);
        TEST_CHECK(shown != label && shown.find("...") != std::string::npos);
        TEST_CHECK(text.measure(shown, style) <= art::kDiffNameBudget);
    }
    // Every standard upper-case label fits the budget untruncated.
    for (const char* name : {"BEGINNER", "EASY", "MEDIUM", "HARD", "CHALLENGE", "EDIT"}) {
        TEST_CHECK(text.measure(name, theme::text::kDiffNameSelected) <= art::kDiffNameBudget);
    }
    std::cout << "  - difficulty row labels ok.\n";
}

void test_ticks() {
    TEST_CHECK(art::meter_ticks_lit(-1) == 0);
    TEST_CHECK(art::meter_ticks_lit(0) == 0);
    TEST_CHECK(art::meter_ticks_lit(3) == 3);
    TEST_CHECK(art::meter_ticks_lit(10) == 10);
    TEST_CHECK(art::meter_ticks_lit(15) == 10);

    const Rect row = art::difficulty_row_rect(0, 3);
    TEST_CHECK(rect_eq(art::tick_rect(row, false, 0), 254, 385, 20, 18));
    TEST_CHECK(rect_eq(art::tick_rect(row, false, 9), 416, 385, 20, 18));
    const Rect sel = art::difficulty_row_rect(3, 3);
    TEST_CHECK(rect_eq(art::tick_rect(sel, true, 0), 268, 550, 20, 20));
    TEST_CHECK(rect_eq(art::tick_rect(sel, true, 9), 430, 550, 20, 20));
    // The ticks end before the best % column.
    TEST_CHECK(art::tick_rect(row, false, 9).x + 20 < row.x + art::kDiffBestRight - 60);
    std::cout << "  - meter ticks ok.\n";
}

void test_meter_clearance() {
    // The baked tab's right edge at the cap top (diff_row_hard{,_selected}.png; the
    // tab is slanted, so this is its widest point under the digits), and the first
    // tick's ink inset (diff_tick.png opaque x at mid-height).
    constexpr float kTabTopNormal = 163.5f;
    constexpr float kTabTopSelected = 167.5f;
    constexpr float kTickInkInset = 3.0f;
    constexpr float kMinClear = 4.0f;

    // The number and ticks moved together (spacing unchanged from 194 - 174).
    TEST_CHECK(art::kDiffTickX - art::kDiffMeterCentreX == 20.0f);
    // The code-drawn Edit tab is no wider than the baked one, so the baked check covers it.
    TEST_CHECK(art::kEditTabWidth <= kTabTopNormal);

    blaze4k::TextRenderer& text = loaded_text();
    const std::pair<theme::TextStyle, float> cases[] = {
        {theme::text::kDiffMeter, kTabTopNormal},
        {theme::text::kDiffMeterSelected, kTabTopSelected},
    };
    for (const auto& [style, tab_top] : cases) {
        for (int meter = 1; meter <= 20; ++meter) {
            const float w = text.measure(std::to_string(meter), style);
            TEST_CHECK(art::kDiffMeterCentreX - w * 0.5f >= tab_top + kMinClear);
            TEST_CHECK(art::kDiffMeterCentreX + w * 0.5f <= art::kDiffTickX + kTickInkInset - kMinClear);
        }
    }
    std::cout << "  - meter clearance ok.\n";
}

void test_name_margin() {
    // The baked tab fill's left edge at the name's cap top (diff_row_hard{,_selected}.png;
    // the tab is slanted, so this is where it cuts closest to the first letter; in the
    // selected row the fill starts inside the gold frame), and the name's right limit
    // before #126 (15 + 128).
    constexpr float kTabLeftNormal = 12.5f;
    constexpr float kTabLeftSelected = 16.0f;
    constexpr float kMinGap = 8.0f;
    constexpr float kNameRight = 143.0f;
    constexpr float kMinClear = 4.0f;

    // Left margin in both row states; the selected row's is at least the normal one.
    TEST_CHECK(art::kDiffNameX - kTabLeftNormal >= kMinGap);
    TEST_CHECK(art::kDiffNameSelectedX - kTabLeftSelected >= kMinGap);
    TEST_CHECK(art::kDiffNameSelectedX - kTabLeftSelected >= art::kDiffNameX - kTabLeftNormal);
    // The draw site's x: each row state gets its own offset (row x 44, 58 selected).
    const Rect row = art::difficulty_row_rect(0, 3);
    const Rect sel = art::difficulty_row_rect(3, 3);
    TEST_CHECK(art::difficulty_name_x(row, false) == 65.0f);
    TEST_CHECK(art::difficulty_name_x(sel, true) == 83.0f);
    TEST_CHECK(art::difficulty_name_x(row, false) - row.x == art::kDiffNameX);
    TEST_CHECK(art::difficulty_name_x(sel, true) - sel.x == art::kDiffNameSelectedX);

    // The right limit did not move, so a truncated name still ends inside the tab.
    TEST_CHECK(art::kDiffNameX + art::kDiffNameBudget <= kNameRight);
    TEST_CHECK(art::kDiffNameSelectedX + art::kDiffNameBudget <= kNameRight);
    // The code-drawn Edit tab's right edge at the name's baseline (under 8px below
    // the row's centre in both rows).
    TEST_CHECK(kNameRight + kMinClear <= art::kEditTabWidth - theme::skew::kRows * 8.0f);

    // Every standard label is drawn untruncated in both row states (the draw site
    // truncates to kDiffNameBudget).
    blaze4k::TextRenderer& text = loaded_text();
    for (const theme::TextStyle& style : {theme::text::kDiffName, theme::text::kDiffNameSelected}) {
        for (const char* name : {"BEGINNER", "EASY", "MEDIUM", "HARD", "CHALLENGE", "EDIT"}) {
            TEST_CHECK(text.truncate(name, style, art::kDiffNameBudget) == name);
        }
    }
    // The limit stays clear of the widest meter number.
    for (int meter = 1; meter <= 20; ++meter) {
        const float w = text.measure(std::to_string(meter), theme::text::kDiffMeterSelected);
        TEST_CHECK(art::kDiffMeterCentreX - w * 0.5f >= kNameRight + kMinClear);
    }
    std::cout << "  - name margin ok.\n";
}

void test_chip_text() {
    TEST_CHECK(art::speed_chip_text(nullptr) == "SPEED 1x");
    TEST_CHECK(art::scroll_chip_text(nullptr) == "UPSCROLL");
    blaze4k::GameConfig config;
    TEST_CHECK(art::speed_chip_text(&config) == "SPEED 1x");
    TEST_CHECK(art::scroll_chip_text(&config) == "UPSCROLL");
    const std::pair<const char*, const char*> speeds[] = {
        {"2.5x", "SPEED 2.5x"}, {"c450", "SPEED C450"}, {"m600", "SPEED M600"},
        {"C400", "SPEED C400"}, {"garbage", "SPEED 1x"}, {"", "SPEED 1x"}, {"-2x", "SPEED 1x"}};
    for (const auto& [in, out] : speeds) {
        config.gameplay.speed_mod = in;
        TEST_CHECK(art::speed_chip_text(&config) == out);
    }
    config.gameplay.scroll = "down";
    TEST_CHECK(art::scroll_chip_text(&config) == "DOWNSCROLL");
    config.gameplay.scroll = "up";
    TEST_CHECK(art::scroll_chip_text(&config) == "UPSCROLL");
    std::cout << "  - chip texts ok.\n";
}

void test_chip_rects() {
    const auto rects = art::chip_rects(100.0f, 80.0f);
    TEST_CHECK(rect_eq(rects[1], 1240 - 124, 12, 124, 40));
    TEST_CHECK(rect_eq(rects[0], 1240 - 124 - 4 - 144, 12, 144, 40));
    TEST_CHECK(approx(rects[1].x + rects[1].w, art::kChipRight));
    TEST_CHECK(approx(rects[1].x - (rects[0].x + rects[0].w), art::kChipGap));
    // Negative widths clamp to the padding only.
    const auto tiny = art::chip_rects(-5.0f, -5.0f);
    TEST_CHECK(approx(tiny[0].w, 44.0f) && approx(tiny[1].w, 44.0f));

    // With the real chip texts, both chips sit inside the top bar, right of the title sprite.
    blaze4k::TextRenderer& text = loaded_text();
    const float speed_w = text.measure("SPEED 2.5x", theme::text::kChip);
    const float scroll_w = text.measure("DOWNSCROLL", theme::text::kChip);
    const auto real = art::chip_rects(speed_w, scroll_w);
    TEST_CHECK(real[0].x > art::kTitleSpritePos.x + 300.0f);
    TEST_CHECK(real[0].y + real[0].h <= theme::layout::kTopBarHeight);
    std::cout << "  - chip boxes ok.\n";
}

void test_hint_layout() {
    blaze4k::TextRenderer& text = loaded_text();
    const art::HintLine line = art::hint_layout(
        [&text](std::string_view s) { return text.measure(s, theme::text::kHintKey); },
        [&text](std::string_view s) { return text.measure(s, theme::text::kHintWord); });

    using Kind = art::HintPiece::Kind;
    TEST_CHECK(line.count == 12);
    const Kind kinds[12] = {Kind::Arrow, Kind::Arrow, Kind::Word, Kind::Arrow,
                            Kind::Arrow, Kind::Word,  Kind::Key,  Kind::Word,
                            Kind::Key,   Kind::Word,  Kind::Key,  Kind::Word};
    const char* texts[12] = {"", "", "SONG", "", "", "DIFFICULTY", "ENTER", "PLAY",
                             "TAB", "OPTIONS", "ESC", "TITLE"};
    for (std::size_t i = 0; i < 12; ++i) {
        TEST_CHECK(line.pieces[i].kind == kinds[i]);
        if (kinds[i] != Kind::Arrow) {
            TEST_CHECK(line.pieces[i].text == texts[i]);
        }
    }
    TEST_CHECK(line.pieces[0].arrow == art::HintArrow::Up);
    TEST_CHECK(line.pieces[1].arrow == art::HintArrow::Down);
    TEST_CHECK(line.pieces[3].arrow == art::HintArrow::Left);
    TEST_CHECK(line.pieces[4].arrow == art::HintArrow::Right);

    const auto& p = line.pieces;
    TEST_CHECK(approx(p[1].x - p[0].x, 16.0f) && approx(p[0].width, 4.0f));
    TEST_CHECK(approx(p[2].x, p[1].x + 4.0f + 10.0f));
    TEST_CHECK(approx(p[3].x, p[2].x + p[2].width + 34.0f));
    TEST_CHECK(approx(p[4].x - p[3].x, 24.0f) && approx(p[3].width, 14.0f));
    TEST_CHECK(approx(p[5].x, p[4].x + 14.0f + 10.0f));
    TEST_CHECK(approx(p[6].x, p[5].x + p[5].width + 34.0f));
    TEST_CHECK(approx(p[7].x, p[6].x + p[6].width + 8.0f));
    TEST_CHECK(approx(p[8].x, p[7].x + p[7].width + 34.0f));
    TEST_CHECK(approx(p[11].x + p[11].width, p[0].x + line.width));

    // Centred on x 640; the mock's line spans 308..970 (662 px).
    TEST_CHECK(std::fabs(p[0].x + line.width * 0.5f - 640.0f) < 0.5f);
    std::cout << "    hint line " << p[0].x << ".." << p[0].x + line.width << " (mock 308..970)\n";
    TEST_CHECK(std::fabs(line.width - 662.0f) < 12.0f);

    // Arrow geometry: a 14px cap band centred on the hint band (mock caps 687..701).
    TEST_CHECK(art::kHintArrowCentreY == 694.0f);
    const art::ArrowQuads up = art::hint_arrow_quads(art::HintArrow::Up, Vec2{100, 694});
    TEST_CHECK(vec_eq(up.stem[0], 99, 687) && vec_eq(up.stem[1], 101, 687));
    TEST_CHECK(vec_eq(up.stem[2], 101, 701) && vec_eq(up.stem[3], 99, 701));
    TEST_CHECK(vec_eq(up.head[0], 100, 687) && vec_eq(up.head[1], 100, 687));
    TEST_CHECK(vec_eq(up.head[2], 102, 691) && vec_eq(up.head[3], 98, 691));
    const art::ArrowQuads down = art::hint_arrow_quads(art::HintArrow::Down, Vec2{100, 694});
    TEST_CHECK(vec_eq(down.head[0], 98, 697) && vec_eq(down.head[1], 102, 697));
    TEST_CHECK(vec_eq(down.head[2], 100, 701) && vec_eq(down.head[3], 100, 701));
    const art::ArrowQuads right = art::hint_arrow_quads(art::HintArrow::Right, Vec2{100, 694});
    TEST_CHECK(vec_eq(right.stem[0], 93, 693) && vec_eq(right.stem[2], 107, 695));
    TEST_CHECK(vec_eq(right.head[0], 103, 692) && vec_eq(right.head[1], 107, 694));
    TEST_CHECK(vec_eq(right.head[2], 107, 694) && vec_eq(right.head[3], 103, 696));
    const art::ArrowQuads left = art::hint_arrow_quads(art::HintArrow::Left, Vec2{100, 694});
    TEST_CHECK(vec_eq(left.head[0], 93, 694) && vec_eq(left.head[1], 97, 692));
    TEST_CHECK(vec_eq(left.head[2], 97, 696) && vec_eq(left.head[3], 93, 694));
    std::cout << "  - hint line + arrow geometry ok.\n";
}

void test_skewed_quad() {
    const auto q = art::skewed_quad(Rect{44, 372, 564, 44}, theme::skew::kRows);
    const float shift = theme::skew::kRows * 22.0f;
    TEST_CHECK(vec_eq(q[0], 44 + shift, 372) && vec_eq(q[1], 608 + shift, 372));
    TEST_CHECK(vec_eq(q[2], 608 - shift, 416) && vec_eq(q[3], 44 - shift, 416));
    const auto flat = art::skewed_quad(Rect{1, 2, 3, 4}, 0.0f);
    TEST_CHECK(vec_eq(flat[0], 1, 2) && vec_eq(flat[1], 4, 2) && vec_eq(flat[2], 4, 6) &&
               vec_eq(flat[3], 1, 6));
    std::cout << "  - skewed quad ok.\n";
}

void test_chrome_layout() {
    const blaze4k::ThemeTextures& tex = loaded_theme();
    const Vec2 title[4] = {{40, 9}, {80, 18}, {520, 18}, {60, 73.5f}};
    const float bar_top_h[4] = {66, 132, 132, 99};
    const float hint_top[4] = {666, 1332, 1332, 1059};
    for (int i = 0; i < 4; ++i) {
        const theme::LayoutScale L = theme::layout_scale(kSizes[i].w, kSizes[i].h);
        TEST_CHECK(vec_eq(L.point(art::kTitleSpritePos), title[i].x, title[i].y));
        TEST_CHECK(approx(tex.content_size("bar_top", L.s).y, bar_top_h[i]));
        TEST_CHECK(approx(L.y(theme::layout::kRefHeight) - tex.content_size("bar_hint", L.s).y,
                          hint_top[i]));
    }
    TEST_CHECK(vec_eq(tex.content_size("title_select_music", 1.0f), 300, 46));
    TEST_CHECK(vec_eq(tex.content_size("banner_fallback", 1.0f), 560, 157));
    TEST_CHECK(vec_eq(tex.content_size("diff_tick", 1.0f), art::kTickWidth, art::kTickHeight));
    TEST_CHECK(vec_eq(tex.content_size("diff_row_hard", 1.0f), art::kDiffRowWidth, 44));
    TEST_CHECK(vec_eq(tex.content_size("diff_row_hard_selected", 1.0f), art::kDiffRowWidth, 52));
    TEST_CHECK(vec_eq(tex.content_size("wheel_row", 1.0f), 564, 62));
    TEST_CHECK(vec_eq(tex.content_size("wheel_row_selected", 1.0f), 640, 92));
    TEST_CHECK(art::kHintBandTop == 668.0f && art::kBpmRight == 604.0f);
    std::cout << "  - chrome layout vs manifest ok.\n";
}

void test_texture_names_exist() {
    const blaze4k::ThemeTextures& tex = loaded_theme();
    for (const char* name :
         {"bg_select", "bar_top", "bar_hint", "title_select_music", "chip", "banner_frame",
          "banner_fallback", "diff_tick", "wheel_pack", "wheel_row", "wheel_row_selected",
          "scanlines"}) {
        TEST_CHECK(tex.entry(name) != nullptr);
    }
    for (const char* label : {"Beginner", "Easy", "Medium", "Hard", "Challenge"}) {
        const art::DifficultyRowStyle style = art::difficulty_row_style(make_chart(label, "", 5));
        TEST_CHECK(tex.entry(style.texture) != nullptr);
        TEST_CHECK(tex.entry(style.texture_selected) != nullptr);
    }
    std::cout << "  - every select texture name is in the manifest ok.\n";
}

void test_options_layout() {
    constexpr int kRows = blaze4k::kOptionsRowCount;
    TEST_CHECK(kRows == 7);
    TEST_CHECK(rect_eq(opt::panel_rect(), 320, 95, 640, 542));
    TEST_CHECK(rect_eq(opt::header_rect(), 320, 95, 640, 66));
    TEST_CHECK(rect_eq(opt::row_rect(0, 0), 360, 185, 560, 80));
    TEST_CHECK(rect_eq(opt::row_rect(1, 0), 360, 275, 560, 48));
    const Rect last = opt::row_rect(6, 0);
    TEST_CHECK(approx(last.y + last.h, 613.0f));
    TEST_CHECK(rect_eq(opt::row_rect(6, 6), 360, 533, 560, 80));

    const Rect panel = opt::panel_rect();
    for (int sel = 0; sel < kRows; ++sel) {
        int tall = 0;
        for (int i = 0; i < kRows; ++i) {
            const Rect r = opt::row_rect(i, sel);
            TEST_CHECK(r.y >= opt::kRowsTop - 1e-3f);
            TEST_CHECK(r.y + r.h <= 613.0f + 1e-3f);
            TEST_CHECK(r.y + r.h <= panel.y + panel.h - opt::kPanelPadY + 1e-3f);
            if (i + 1 < kRows) {
                TEST_CHECK(r.y + r.h < opt::row_rect(i + 1, sel).y);
            }
            tall += r.h == 80.0f ? 1 : 0;
            TEST_CHECK(r.h == (i == sel ? 80.0f : 48.0f));
        }
        TEST_CHECK(tall == 1);
    }
    // Between select's top bar (66) and its hint-bar rule (666).
    TEST_CHECK(panel.y >= theme::layout::kTopBarHeight + 2.0f);
    TEST_CHECK(panel.y + panel.h <= art::kHintBarTop);
    // The stretched gold bar keeps wheel_row_selected's slant (aspect within 2%).
    TEST_CHECK(std::fabs(80.0f / 560.0f - 92.0f / 640.0f) / (92.0f / 640.0f) < 0.02f);
    TEST_CHECK(vec_eq(loaded_theme().content_size("wheel_row_selected", 1.0f), 640, 92));
    TEST_CHECK(approx(loaded_theme().content_size("bar_top", 1.0f).y, opt::kHeaderHeight));

    // Screen px: exact 2x at 1440p; centred on x 1720 at 3440x1440.
    TEST_CHECK(rect_eq(theme::layout_scale(2560, 1440).rect(opt::row_rect(0, 0)), 720, 370, 1120,
                       160));
    const Rect wide = theme::layout_scale(3440, 1440).rect(panel);
    TEST_CHECK(approx(wide.x + wide.w * 0.5f, 1720.0f));

    // Out-of-range inputs clamp.
    TEST_CHECK(rect_eq(opt::row_rect(-1, 0), 360, 185, 560, 80));
    TEST_CHECK(rect_eq(opt::row_rect(99, 0), 360, 185 + 6 * 58 + 32, 560, 48));
    TEST_CHECK(rect_eq(opt::row_rect(0, -1), 360, 185, 560, 80));
    TEST_CHECK(rect_eq(opt::row_rect(6, 99), 360, 533, 560, 80));
    TEST_CHECK(rect_eq(opt::row_rect(-5, 99), 360, 185, 560, 48));

    // Insets.
    const Rect row = opt::row_rect(1, 0);
    TEST_CHECK(approx(opt::name_x(row, false), 386.0f) && approx(opt::name_x(row, true), 390.0f));
    TEST_CHECK(approx(opt::value_right(row, false), 890.0f) &&
               approx(opt::value_right(row, true), 884.0f));

    // Row names, built once.
    const auto& names = opt::row_names();
    TEST_CHECK(&names == &opt::row_names());
    for (int i = 0; i < kRows; ++i) {
        TEST_CHECK(names[static_cast<std::size_t>(i)] == blaze4k::options_row_name(i));
    }
    std::cout << "  - options overlay layout ok.\n";
}

void test_options_text_fits() {
    blaze4k::TextRenderer& text = loaded_text();
    using blaze4k::OptionsRow;
    using blaze4k::SpeedMod;
    using blaze4k::SpeedModType;

    std::vector<std::string> speeds = {"C9999", "M9999"};
    for (double v : blaze4k::options_speed_values(SpeedModType::XMod)) {
        speeds.push_back(blaze4k::format_speed_mod(SpeedMod{SpeedModType::XMod, v}));
    }
    speeds.push_back("2.5x");
    const std::vector<std::vector<std::string>> values = {
        {"XMOD", "CMOD", "MMOD"},
        speeds,
        {"UP", "DOWN"},
        {"ON", "OFF"},
        {"ON", "OFF"},
        {blaze4k::format_offset(-3600.0), blaze4k::format_offset(3600.0), "+0.023 s"},
        {">"},
    };
    TEST_CHECK(values.size() == static_cast<std::size_t>(blaze4k::kOptionsRowCount));
    TEST_CHECK(values[5][0] == "-3600.000 s");

    const theme::TextStyle value_style =
        blaze4k::with_color(theme::text::kWheelPack, theme::color::kGold);
    for (int i = 0; i < blaze4k::kOptionsRowCount; ++i) {
        const std::string& name = opt::row_names()[static_cast<std::size_t>(i)];
        for (const std::string& value : values[static_cast<std::size_t>(i)]) {
            for (const bool selected : {false, true}) {
                const Rect row = opt::row_rect(i, selected ? i : (i == 0 ? 1 : 0));
                const theme::TextStyle& ns =
                    selected ? theme::text::kWheelSelected : theme::text::kWheelRow;
                const theme::TextStyle& vs = selected ? theme::text::kWheelSelected : value_style;
                const float name_end = opt::name_x(row, selected) +
                                       blaze4k::ref_measure(text, name, ns) + opt::kNameValueGap;
                const float value_start =
                    opt::value_right(row, selected) - blaze4k::ref_measure(text, value, vs);
                if (!(name_end <= value_start)) {
                    std::cerr << "    " << name << " / " << value << " selected=" << selected
                              << ": " << name_end << " > " << value_start << "\n";
                }
                TEST_CHECK(name_end <= value_start);
                // Each line box fits its row.
                TEST_CHECK(text.line_height(ns) <= row.h);
                TEST_CHECK(text.line_height(vs) <= row.h);
            }
        }
    }
    // The title fits the panel and the header's 64px band.
    TEST_CHECK(blaze4k::ref_measure(text, "OPTIONS", opt::kTitleStyle) < opt::kPanel.w - 48.0f);
    // Its line box (~70px: ascent 50 + descent 20) is taller than the band, as
    // kSongTitle's is; centred in the band, the baseline and one em above it
    // (caps and accents) stay inside the band.
    const float line_top = opt::kPanel.y +
                           (opt::kHeaderBandHeight - text.line_height(opt::kTitleStyle)) * 0.5f;
    const float baseline = line_top + text.ascent(opt::kTitleStyle);
    TEST_CHECK(baseline <= opt::kPanel.y + opt::kHeaderBandHeight);
    TEST_CHECK(baseline - opt::kTitleStyle.size_px >= opt::kPanel.y);
    TEST_CHECK(opt::kTitleStyle.color.r == theme::color::kWhite.r &&
               opt::kTitleStyle.shadow == theme::Shadow::Hard3 && opt::kTitleStyle.italic);
    std::cout << "  - options names and values fit their rows ok.\n";
}

void test_options_hint_layout() {
    blaze4k::TextRenderer& text = loaded_text();
    const art::HintLine line = opt::hint_layout(
        [&text](std::string_view s) { return text.measure(s, theme::text::kHintKey); },
        [&text](std::string_view s) { return text.measure(s, theme::text::kHintWord); });

    using Kind = art::HintPiece::Kind;
    TEST_CHECK(line.count == 10);
    const Kind kinds[10] = {Kind::Arrow, Kind::Arrow, Kind::Word, Kind::Arrow, Kind::Arrow,
                            Kind::Word,  Kind::Key,   Kind::Word, Kind::Key,   Kind::Word};
    const char* texts[10] = {"", "", "ROW", "", "", "CHANGE", "ENTER", "NEXT", "ESC", "CLOSE"};
    for (std::size_t i = 0; i < 10; ++i) {
        TEST_CHECK(line.pieces[i].kind == kinds[i]);
        if (kinds[i] != Kind::Arrow) {
            TEST_CHECK(line.pieces[i].text == texts[i]);
        }
    }
    TEST_CHECK(line.pieces[0].arrow == art::HintArrow::Up);
    TEST_CHECK(line.pieces[1].arrow == art::HintArrow::Down);
    TEST_CHECK(line.pieces[3].arrow == art::HintArrow::Left);
    TEST_CHECK(line.pieces[4].arrow == art::HintArrow::Right);

    // Same spacing rules as select's legend.
    const auto& p = line.pieces;
    TEST_CHECK(approx(p[1].x - p[0].x, 16.0f) && approx(p[2].x, p[1].x + 4.0f + 10.0f));
    TEST_CHECK(approx(p[3].x, p[2].x + p[2].width + 34.0f));
    TEST_CHECK(approx(p[6].x, p[5].x + p[5].width + 34.0f));
    TEST_CHECK(approx(p[7].x, p[6].x + p[6].width + 8.0f));
    TEST_CHECK(approx(p[9].x + p[9].width, p[0].x + line.width));
    // Centred on x 640, inside the 40px margins.
    TEST_CHECK(approx(p[0].x + line.width * 0.5f, 640.0f));
    TEST_CHECK(line.width < 1280.0f - 2.0f * 40.0f);

    // The shared builder stops at kHintPieceCount pieces.
    std::vector<art::HintItem> many(20, art::HintItem{art::HintItem::Kind::Word, "W"});
    const art::HintLine capped =
        art::layout_hint_items(many, [](std::string_view) { return 10.0f; },
                               [](std::string_view) { return 10.0f; });
    TEST_CHECK(capped.count == static_cast<int>(art::kHintPieceCount));
    // 12 words of 10 + 11 gaps of 34 (no trailing gap after the last one fitted).
    TEST_CHECK(approx(capped.width, 12.0f * 10.0f + 11.0f * 34.0f));
    // An arrow pair that would straddle the cap is dropped whole.
    std::vector<art::HintItem> arrows(11, art::HintItem{art::HintItem::Kind::Word, "W"});
    arrows.push_back(art::HintItem{art::HintItem::Kind::VArrows, {}});
    const art::HintLine no_split = art::layout_hint_items(arrows, nullptr, nullptr);
    TEST_CHECK(no_split.count == 11);
    // A line ending on a Key or an arrow pair gets no trailing gap either, so it
    // is still centred on x 640.
    const auto ten = [](std::string_view) { return 10.0f; };
    const std::array<art::HintItem, 2> word_key = {
        {{art::HintItem::Kind::Word, "W"}, {art::HintItem::Kind::Key, "K"}}};
    const art::HintLine ends_key = art::layout_hint_items(word_key, ten, ten);
    TEST_CHECK(ends_key.count == 2);
    TEST_CHECK(approx(ends_key.width, 10.0f + 34.0f + 10.0f));
    TEST_CHECK(approx(ends_key.pieces[0].x + ends_key.width * 0.5f, 640.0f));
    const std::array<art::HintItem, 2> word_arrows = {
        {{art::HintItem::Kind::Word, "W"}, {art::HintItem::Kind::HArrows, {}}}};
    const art::HintLine ends_arrows = art::layout_hint_items(word_arrows, ten, ten);
    TEST_CHECK(ends_arrows.count == 3);
    TEST_CHECK(approx(ends_arrows.width, 10.0f + 34.0f + 24.0f + 14.0f));
    TEST_CHECK(approx(ends_arrows.pieces[0].x + ends_arrows.width * 0.5f, 640.0f));
    // A Key and its Word straddling the cap are dropped together: no orphan Key.
    std::vector<art::HintItem> pairs(11, art::HintItem{art::HintItem::Kind::Word, "W"});
    pairs.push_back(art::HintItem{art::HintItem::Kind::Key, "K"});
    pairs.push_back(art::HintItem{art::HintItem::Kind::Word, "W"});
    const art::HintLine no_orphan = art::layout_hint_items(pairs, ten, ten);
    TEST_CHECK(no_orphan.count == 11);
    TEST_CHECK(no_orphan.pieces[10].kind == art::HintPiece::Kind::Word);
    TEST_CHECK(approx(no_orphan.width, 11.0f * 10.0f + 10.0f * 34.0f));
    std::cout << "  - options legend ok.\n";
}

void test_options_styles_prebaked() {
    auto prebaked = [](const theme::TextStyle& style) {
        for (const theme::TextStyle& s : theme::text::kAllStyles) {
            if (s.font == style.font && s.size_px == style.size_px) {
                return true;
            }
        }
        return false;
    };
    for (const theme::TextStyle& style :
         {opt::kTitleStyle, theme::text::kWheelRow, theme::text::kWheelPack,
          theme::text::kWheelSelected, theme::text::kHintKey, theme::text::kHintWord,
          theme::text::kSongSubtitle, theme::text::kWheelSubtitle,
          theme::text::kWheelSelectedSubtitle, theme::text::kBarSongSubtitle}) {
        TEST_CHECK(prebaked(style));
    }
    TEST_CHECK(theme::text::kAllStyles.size() == 30);
    std::cout << "  - options styles share pre-baked atlases ok.\n";
}

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string make_sm(const std::string& title, const std::vector<std::pair<std::string, int>>& charts,
                    const std::string& description = "", const std::string& subtitle = "") {
    std::ostringstream out;
    out << "#TITLE:" << title << ";\n";
    if (!subtitle.empty()) {
        out << "#SUBTITLE:" << subtitle << ";\n";
    }
    out << "#ARTIST:Some Artist With A Rather Long Name;\n"
        << "#MUSIC:audio.ogg;\n#BPMS:0.0=140.0,32.0=175.0;\n";
    for (const auto& [difficulty, meter] : charts) {
        out << "#NOTES:dance-single:" << (difficulty == "Edit" ? description : "") << ":"
            << difficulty << ":" << meter << ":0,0,0,0,0:\n1000\n0100\n0010\n0001\n;\n";
    }
    return out.str();
}

void test_render_smoke() {
    const fs::path root = fs::temp_directory_path() / "blaze4k_select_art_test";
    fs::remove_all(root);
    write_file(root / "Pack A" / "Many" / "audio.ogg", "fake");
    write_file(root / "Pack A" / "Many" / "Many.sm",
               make_sm("A Song With Many Charts And A Very Long Title That Will Not Fit",
                       {{"Beginner", 1}, {"Easy", 3}, {"Medium", 6}, {"Hard", 9}, {"Challenge", 12},
                        {"Edit", 13}, {"Edit", 15}},
                       "An Extremely Long Custom Edit Chart Name", "(Two Gees Radio Edit)"));
    write_file(root / "Pack A" / "Single" / "audio.ogg", "fake");
    write_file(root / "Pack A" / "Single" / "Single.sm", make_sm("Single", {{"Novice", 1}}));
    // Two same-title songs told apart by their subtitles (#110).
    for (const char* sub : {"-Hyper-", "-Mobius-"}) {
        const std::string dir = std::string{"Disconnected "} + sub;
        write_file(root / "Pack A" / dir / "audio.ogg", "fake");
        write_file(root / "Pack A" / dir / "Disconnected.sm",
                   make_sm("Disconnected", {{"Hard", 9}}, "", sub));
    }
    // An empty #TITLE with a subtitle: the subtitle takes the title's place (#110 review).
    write_file(root / "Pack A" / "Untitled" / "audio.ogg", "fake");
    write_file(root / "Pack A" / "Untitled" / "Untitled.sm",
               make_sm("", {{"Easy", 2}}, "", "-Subtitle Only-"));
    for (int i = 0; i < 9; ++i) {
        const std::string name = "Filler " + std::to_string(i);
        write_file(root / "Pack B" / name / "audio.ogg", "fake");
        write_file(root / "Pack B" / name / (name + ".sm"), make_sm(name, {{"Expert", 11}}));
    }
    blaze4k::SongLibrary library;
    TEST_CHECK(library.scan_directory(root));

    blaze4k::GameConfig config;
    config.gameplay.speed_mod = "2.5x";
    config.gameplay.scroll = "down";
    blaze4k::HighScores scores;
    blaze4k::PlayRequest request;
    blaze4k::GlQuadRenderer renderer; // uninitialised: draws are no-ops
    blaze4k::TextRenderer& text = loaded_text();

    const Size sizes[] = {{1280, 720}, {2560, 1440}, {3440, 1440}, {1920, 1200}, {640, 480}, {0, 0}};
    auto render_all = [&](blaze4k::ScreenManager& manager, bool with_text) {
        for (const Size& size : sizes) {
            if (with_text && size.w > 0) {
                text.set_window_size(size.w, size.h);
            }
            manager.render(renderer, size.w, size.h);
        }
        text.set_window_size(1280, 720);
    };

    for (const bool services : {true, false}) {
        blaze4k::ScreenManager manager(0.0);
        auto screen = std::make_unique<blaze4k::SelectScreen>();
        blaze4k::SelectScreen* select = screen.get();
        manager.add_screen(std::move(screen));
        manager.context().config = &config;
        manager.context().scores = &scores;
        manager.context().library = &library;
        manager.context().play_request = &request;
        if (services) {
            manager.context().theme = &loaded_theme();
            manager.context().text = &text;
        }
        manager.start(ScreenId::Select);
        // Pack A: 5 songs (2 subtitled "Disconnected", 1 untitled), Pack B: 9; + 2 pack headers.
        TEST_CHECK(select->song_count() == 14);
        TEST_CHECK(select->wheel_row_count() == 16);

        // Every song, every chart (the 7-chart song scrolls its difficulty list),
        // mid-slide and at rest.
        for (std::size_t s = 0; s < select->song_count(); ++s) {
            for (int c = 0; c < select->chart_count(); ++c) {
                render_all(manager, services);
                manager.update(1.0 / 60.0, {press(GameAction::Right)});
            }
            manager.update(1.0 / 60.0, {press(GameAction::Down)});
            render_all(manager, services);
        }
        // The options overlay draws over the Cabinet screen, with every row as
        // the selection (Down clamps at the last row).
        manager.update(1.0 / 60.0, {press(GameAction::Options)});
        TEST_CHECK(select->options_open());
        render_all(manager, services);
        for (int i = 0; i < 7; ++i) {
            manager.update(1.0 / 60.0, {press(GameAction::Down)});
            TEST_CHECK(select->options_open());
            TEST_CHECK(select->options_menu().row ==
                       std::min(i + 1, blaze4k::kOptionsRowCount - 1));
            render_all(manager, services);
        }
        manager.update(1.0 / 60.0, {press(GameAction::Up)});
        TEST_CHECK(select->options_menu().row == blaze4k::kOptionsRowCount - 2);
        render_all(manager, services);
        manager.update(1.0 / 60.0, {press(GameAction::Back)});
        TEST_CHECK(!select->options_open());
    }

    // Empty library.
    blaze4k::SongLibrary empty;
    blaze4k::ScreenManager manager(0.0);
    manager.add_screen(std::make_unique<blaze4k::SelectScreen>());
    manager.context().library = &empty;
    manager.context().theme = &loaded_theme();
    manager.context().text = &text;
    manager.start(ScreenId::Select);
    render_all(manager, true);
    manager.update(1.0 / 60.0, {press(GameAction::Options)});
    render_all(manager, true);

    fs::remove_all(root);
    std::cout << "  - SelectScreen renders with real/null services at every size ok.\n";
}

} // namespace

int main() {
    std::cout << "select_art_test\n";
    test_list_window();
    test_visible_rows();
    test_difficulty_row_rects();
    test_wheel_row_rects();
    test_build_wheel_rows();
    test_scroll_easing();
    test_wheel_slide_stays_in_column();
    test_display_text_coverage();
    test_disconnected_subtitles_fit();
    test_row_style();
    test_labels();
    test_ticks();
    test_meter_clearance();
    test_name_margin();
    test_chip_text();
    test_chip_rects();
    test_hint_layout();
    test_skewed_quad();
    test_chrome_layout();
    test_texture_names_exist();
    test_options_layout();
    test_options_text_fits();
    test_options_hint_layout();
    test_options_styles_prebaked();
    test_render_smoke();
    std::cout << "select_art_test: all passed\n";
    return 0;
}
