// #92: Cabinet Title + Attract screens. Pins the title_art layout table (720p,
// 1440p, 21:9, 16:10), the PRESS START blink and attract pulse/blink rules, the
// CMake-sourced footer version, footer glyph coverage, the invalid-skin-texture
// guard, and renders both screens with the real (headless) theme and text
// services while checking the screen transitions are unchanged.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/attract_screen.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_placeholder_screen.hpp"
#include "screens/title_art.hpp"
#include "screens/title_screen.hpp"

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
namespace title_art = blaze4k::title_art;

using blaze4k::GameAction;
using blaze4k::InputEvent;
using blaze4k::NoteQuantization;
using blaze4k::ScreenId;
using blaze4k::Vec2;

const fs::path kSourceDir{BLAZE4K_SOURCE_DIR};
const fs::path kAssets{BLAZE4K_ASSETS_DIR};
const fs::path kCabinet = kAssets / "theme" / "cabinet";
constexpr double kDt = 1.0 / 60.0;

bool approx(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

bool vec_eq(Vec2 v, float x, float y) {
    return v.x == x && v.y == y;
}

bool vec_approx(Vec2 v, float x, float y) {
    return approx(v.x, x) && approx(v.y, y);
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

void test_arrow_layout() {
    // 1280x720 (identity) and 2560x1440 (exact 2x): exact.
    const auto l720 = theme::layout_scale(1280, 720);
    const float x720[4] = {478, 586, 694, 802};
    for (int c = 0; c < 4; ++c) {
        TEST_CHECK(vec_eq(title_art::arrow_centre(l720, c), x720[c], 450));
    }
    TEST_CHECK(title_art::arrow_box(l720) == 96.0f);

    const auto l1440 = theme::layout_scale(2560, 1440);
    const float x1440[4] = {956, 1172, 1388, 1604};
    for (int c = 0; c < 4; ++c) {
        TEST_CHECK(vec_eq(title_art::arrow_centre(l1440, c), x1440[c], 900));
    }
    TEST_CHECK(title_art::arrow_box(l1440) == 192.0f);

    // 3440x1440 (21:9, s = 2, origin 440,0) and 1920x1200 (16:10, s = 1.5, origin 0,60).
    const auto lwide = theme::layout_scale(3440, 1440);
    const float xwide[4] = {1396, 1612, 1828, 2044};
    for (int c = 0; c < 4; ++c) {
        TEST_CHECK(vec_approx(title_art::arrow_centre(lwide, c), xwide[c], 900));
    }
    TEST_CHECK(approx(title_art::arrow_box(lwide), 192.0f));

    const auto ltall = theme::layout_scale(1920, 1200);
    const float xtall[4] = {717, 879, 1041, 1203};
    for (int c = 0; c < 4; ++c) {
        TEST_CHECK(vec_approx(title_art::arrow_centre(ltall, c), xtall[c], 735));
    }
    TEST_CHECK(approx(title_art::arrow_box(ltall), 144.0f));

    TEST_CHECK(title_art::arrow_quantization(0) == NoteQuantization::Fourth);
    TEST_CHECK(title_art::arrow_quantization(1) == NoteQuantization::Eighth);
    TEST_CHECK(title_art::arrow_quantization(2) == NoteQuantization::Twelfth);
    TEST_CHECK(title_art::arrow_quantization(3) == NoteQuantization::Sixteenth);
    TEST_CHECK(title_art::arrow_quantization(-1) == NoteQuantization::Fourth);
    TEST_CHECK(title_art::arrow_quantization(4) == NoteQuantization::Fourth);
    TEST_CHECK(title_art::kArrowBeat == 0.0);
    std::cout << "  - arrow centres, box and quantization ok.\n";
}

void test_centred_sprite_pos() {
    const blaze4k::ThemeTextures& tex = loaded_theme();

    // The manifest's layout_720p sizes and the code agree.
    TEST_CHECK(vec_eq(tex.content_size("logo", 1.0f), 980, 190));
    TEST_CHECK(vec_eq(tex.content_size("subtitle", 1.0f), 520, 30));
    TEST_CHECK(vec_eq(tex.content_size("press_start", 1.0f), 440, 76));

    struct Expect {
        const char* name;
        float top;
        Vec2 at720, at1440, wide, tall;
    };
    const Expect rows[] = {
        {"logo", theme::layout::kLogoTop, {150, 168}, {300, 336}, {740, 336}, {225, 312}},
        {"subtitle", theme::layout::kSubtitleTop, {380, 362}, {760, 724}, {1200, 724}, {570, 603}},
        {"press_start", theme::layout::kPressStartTop, {420, 548}, {840, 1096}, {1280, 1096},
         {630, 882}},
    };
    const struct {
        int w, h;
    } sizes[] = {{1280, 720}, {2560, 1440}, {3440, 1440}, {1920, 1200}};
    for (const Expect& row : rows) {
        const Vec2 want[4] = {row.at720, row.at1440, row.wide, row.tall};
        for (int i = 0; i < 4; ++i) {
            const auto L = theme::layout_scale(sizes[i].w, sizes[i].h);
            const Vec2 pos = title_art::centred_sprite_pos(L, row.top, tex.content_size(row.name, L.s));
            TEST_CHECK(vec_eq(pos, want[i].x, want[i].y)); // rounded to whole px
        }
    }

    // Rounding: a half-pixel centre snaps to a whole pixel.
    const Vec2 odd = title_art::centred_sprite_pos(theme::layout_scale(1280, 720), 10.5f, {101, 0});
    TEST_CHECK(odd.x == std::round(640.0f - 50.5f) && odd.y == std::round(10.5f));
    std::cout << "  - logo/subtitle/press_start positions ok.\n";
}

void test_prompt_blink() {
    TEST_CHECK(title_art::prompt_visible(0.0));
    TEST_CHECK(title_art::prompt_visible(0.49));
    TEST_CHECK(!title_art::prompt_visible(0.5));
    TEST_CHECK(!title_art::prompt_visible(0.99));
    TEST_CHECK(title_art::prompt_visible(1.0));
    TEST_CHECK(title_art::prompt_visible(1.49));

    // At the 60 Hz fixed dt: one rising edge per second, 60 +/- 1 steps apart.
    double t = 0.0;
    bool previous = title_art::prompt_visible(t);
    int edges = 0;
    int last_edge_step = 0;
    for (int step = 1; step <= 630; ++step) {
        t += kDt;
        const bool now = title_art::prompt_visible(t);
        if (now && !previous) {
            ++edges;
            if (edges > 1) {
                const int gap = step - last_edge_step;
                TEST_CHECK(gap >= 59 && gap <= 61);
            }
            last_edge_step = step;
        }
        previous = now;
    }
    TEST_CHECK(edges == 10);
    std::cout << "  - PRESS START 1 Hz blink ok.\n";
}

void test_attract_helpers() {
    for (int i = 0; i <= 2000; ++i) {
        const float pulse = title_art::attract_pulse(i * 0.01);
        TEST_CHECK(pulse >= 0.30f - 1e-6f && pulse <= 1.00f + 1e-6f);
    }
    TEST_CHECK(approx(title_art::attract_pulse(0.0), 0.65f));
    TEST_CHECK(title_art::attract_active_receptor(0.0) == 0);
    TEST_CHECK(title_art::attract_active_receptor(0.25) == 1);
    TEST_CHECK(title_art::attract_active_receptor(0.5) == 2);
    TEST_CHECK(title_art::attract_active_receptor(0.75) == 3);
    TEST_CHECK(title_art::attract_active_receptor(1.0) == 0);
    std::cout << "  - attract pulse and receptor sequence ok.\n";
}

void test_footer_text() {
    const std::string right(title_art::footer_right_text());
    TEST_CHECK(right.rfind("BLAZE 4K v", 0) == 0);
    TEST_CHECK(right == std::string("BLAZE 4K v") + BLAZE4K_VERSION);
    TEST_CHECK(std::regex_match(right.substr(10), std::regex(R"(\d+\.\d+\.\d+)")));
    TEST_CHECK(title_art::footer_left_text() == "SINGLE \xC2\xB7 4 PANEL");

    blaze4k::TextRenderer text;
    TEST_CHECK(text.load(kSourceDir));
    text.set_window_size(1280, 720);
    TEST_CHECK(text.covers_text(title_art::footer_left_text(), theme::Font::SairaBold));
    TEST_CHECK(text.covers_text(title_art::footer_right_text(), theme::Font::SairaBold));

    const float lh = text.line_height(theme::text::kFooter);
    TEST_CHECK(lh > 0.0f && lh < 40.0f);
    TEST_CHECK(approx(title_art::footer_text_top(theme::layout_scale(1280, 720), lh),
                      680.0f + (40.0f - lh) * 0.5f));
    // 1920x1200: band 1080..1140 (origin y 60, s 1.5).
    TEST_CHECK(approx(title_art::footer_text_top(theme::layout_scale(1920, 1200), 30.0f),
                      1080.0f + (60.0f - 30.0f) * 0.5f));
    std::cout << "  - footer strings, version and glyph coverage ok.\n";
}

void test_skin_sprite_guard() {
    const blaze4k::NoteSkin skin; // never initialised: textures were never uploaded
    for (int c = 0; c < 4; ++c) {
        const auto head = skin.head(blaze4k::NoteType::Tap, c, title_art::arrow_quantization(c),
                                    title_art::kArrowBeat);
        TEST_CHECK(!blaze4k::skin_sprite_drawable(head));
        TEST_CHECK(!blaze4k::skin_sprite_drawable(skin.receptor(c, 0.0)));
    }
    TEST_CHECK(!blaze4k::skin_sprite_drawable(blaze4k::SkinSprite{}));

    blaze4k::GlQuadRenderer renderer; // uninitialised: draws are no-ops
    TEST_CHECK(blaze4k::draw_skin_sprite(renderer, blaze4k::SkinSprite{}, 100.0, 100.0, 96.0) ==
               0);
    std::cout << "  - invalid skin textures are never drawn ok.\n";
}

void test_render_smoke_with_services() {
    blaze4k::ThemeTextures& tex = loaded_theme();
    blaze4k::TextRenderer text;
    TEST_CHECK(text.load(kSourceDir));
    text.set_window_size(1280, 720);
    const blaze4k::NoteSkin skin; // uninitialised
    blaze4k::GlQuadRenderer renderer; // uninitialised

    // Idle -> Attract disabled until the Title blink phases are rendered.
    blaze4k::ScreenManager manager(0.0);
    manager.add_screen(std::make_unique<blaze4k::TitleScreen>());
    manager.add_screen(std::make_unique<blaze4k::AttractScreen>());
    manager.add_screen(std::make_unique<blaze4k::SelectPlaceholderScreen>());
    manager.context().theme = &tex;
    manager.context().text = &text;
    manager.context().noteskin = &skin;
    manager.start(ScreenId::Title);

    const struct {
        int w, h;
    } sizes[] = {{1280, 720}, {2560, 1440}, {3440, 1440}, {1920, 1200}, {0, 0}};
    auto render_all_sizes = [&] {
        for (const auto& size : sizes) {
            if (size.w > 0) {
                text.set_window_size(size.w, size.h);
            }
            manager.render(renderer, size.w, size.h);
        }
    };

    // Title (both blink phases), then Confirm -> Select.
    render_all_sizes();
    manager.update(0.6, {});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    render_all_sizes();
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    render_all_sizes();

    // Back to Title, idle past a 0.5 s attract timeout -> Attract.
    manager.set_idle_timeout_seconds(0.5);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    manager.update(1.0, {});
    TEST_CHECK(manager.active_id() == ScreenId::Attract);
    for (int i = 0; i < 4; ++i) { // every lit receptor
        render_all_sizes();
        manager.update(0.25, {});
        TEST_CHECK(manager.active_id() == ScreenId::Attract);
    }

    // Attract + Confirm -> origin (Title).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    render_all_sizes();

    // Null services still render (headless/unit-test path).
    blaze4k::ScreenManager bare(0.5);
    bare.add_screen(std::make_unique<blaze4k::TitleScreen>());
    bare.add_screen(std::make_unique<blaze4k::AttractScreen>());
    bare.start(ScreenId::Title);
    bare.render(renderer, 1280, 720);
    bare.update(1.0, {});
    TEST_CHECK(bare.active_id() == ScreenId::Attract);
    bare.render(renderer, 1280, 720);
    std::cout << "  - Title/Attract render with real services + transitions ok.\n";
}

} // namespace

int main() {
    std::cout << "title_screen_test\n";
    test_arrow_layout();
    test_centred_sprite_pos();
    test_prompt_blink();
    test_attract_helpers();
    test_footer_text();
    test_skin_sprite_guard();
    test_render_smoke_with_services();
    std::cout << "title_screen_test: all passed\n";
    return 0;
}
