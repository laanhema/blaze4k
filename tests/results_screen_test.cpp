#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "chart/chart.hpp"
#include "chart/note.hpp"
#include "chart/song.hpp"
#include "data/config.hpp"
#include "data/high_scores.hpp"
#include "gameplay/gameplay_options.hpp"
#include "gameplay/hud_renderer.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/results.hpp"
#include "screens/results_art.hpp"
#include "screens/results_screen.hpp"
#include "screens/screen.hpp"
#include "screens/song_display_text.hpp"
#include "screens/screen_manager.hpp"
#include "timing/judgment_constants.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::Chart;
using blaze4k::GameAction;
using blaze4k::HighScores;
using blaze4k::InputEvent;
using blaze4k::Note;
using blaze4k::NoteType;
using blaze4k::ResultsSummary;
using blaze4k::ScreenContext;
using blaze4k::ScreenId;
using blaze4k::Song;

constexpr double kDt = 0.1;

const std::filesystem::path kSourceDir{BLAZE4K_SOURCE_DIR};
const std::filesystem::path kCabinet =
    std::filesystem::path{BLAZE4K_ASSETS_DIR} / "theme" / "cabinet";

// Real headless theme + text services (#95): measuring and truncation work
// without GL; draws are no-ops.
blaze4k::ThemeTextures& loaded_theme() {
    static blaze4k::ThemeTextures theme;
    static const bool loaded = theme.load(kCabinet);
    if (!loaded) {
        std::cerr << "theme failed to load from " << kCabinet << "\n";
        std::abort();
    }
    return theme;
}

blaze4k::TextRenderer& loaded_text() {
    static blaze4k::TextRenderer text;
    static const bool loaded = text.load(kSourceDir);
    if (!loaded) {
        std::cerr << "fonts failed to load from " << kSourceDir << "\n";
        std::abort();
    }
    text.set_window_size(1280, 720);
    return text;
}

const blaze4k::ThemeManifest& real_manifest() {
    static const blaze4k::ThemeManifest manifest = [] {
        std::ifstream in(kCabinet / "manifest.json", std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return blaze4k::parse_theme_manifest(buffer.str());
    }();
    return manifest;
}

bool near(float a, float b, float tol = 1e-3f) {
    return std::fabs(a - b) <= tol;
}

bool same_color(blaze4k::Color a, blaze4k::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool digits_only(std::string_view text) {
    for (const char c : text) {
        if (blaze4k::digit_glyph_index(c) < 0) {
            return false;
        }
    }
    return true;
}

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

Song make_song() {
    Song song;
    song.simfile_path = "Blaze Anthem.sm";
    song.metadata.title = "Blaze Anthem";
    song.metadata.artist = "Test Artist";
    return song;
}

Chart make_chart() {
    Chart chart;
    chart.steps_type = "dance-single";
    chart.difficulty = "Hard";
    chart.meter = 9;
    Note note;
    note.column = 0;
    note.beat = 1.0;
    note.type = NoteType::Tap;
    chart.notes.push_back(note);
    chart.tap_count = 1;
    return chart;
}

ResultsSummary make_summary(const Song& song, const Chart& chart, const std::string& grade,
                            double percent, bool failed = false) {
    ResultsSummary summary;
    summary.valid = true;
    summary.song = &song;
    summary.chart = &chart;
    summary.failed = failed;
    summary.grade_label = grade;
    summary.percent = percent;
    summary.actual_dp = 40;
    summary.possible_dp = 50;
    summary.max_combo = 7;
    return summary;
}

class SelectSpy : public blaze4k::Screen {
public:
    explicit SelectSpy(int* enters = nullptr) : enters_(enters) {}
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }
    void enter(ScreenContext& /*ctx*/) override {
        if (enters_ != nullptr) {
            ++(*enters_);
        }
    }

private:
    int* enters_ = nullptr;
};

struct ResultsFixture {
    blaze4k::GameConfig config;
    HighScores scores;
    Song song = make_song();
    Chart chart = make_chart();
    ResultsSummary summary;
    blaze4k::ScreenManager manager{0.0};
    blaze4k::ResultsScreen* results = nullptr;
    int select_enters = 0;

    ResultsFixture() : summary(make_summary(song, chart, "S+", 0.95)) {}

    void start(bool with_results = true) {
        auto owner = std::make_unique<blaze4k::ResultsScreen>();
        results = owner.get();
        manager.add_screen(std::move(owner));
        manager.add_screen(std::make_unique<SelectSpy>(&select_enters));
        manager.context().config = &config;
        manager.context().scores = &scores;
        manager.context().results = with_results ? &summary : nullptr;
        manager.start(ScreenId::Results);
    }
};

// 1. Enter + submit + NEW RECORD on a first clear.
void test_enter_submit_and_flag() {
    ResultsFixture fx;
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(fx.results->submitted());
    TEST_CHECK(fx.results->new_record());
    TEST_CHECK(fx.results->summary().grade_label == "S+");

    const std::string key = blaze4k::make_chart_key(fx.song, fx.chart);
    const blaze4k::ScoreRecord* record = blaze4k::find_high_score(fx.scores, key);
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->grade == "S+");
    TEST_CHECK(record->percent == 0.95);
    std::cout << "  - enter submits + flags the first clear ok.\n";
}

// 2. A worse-than-best run submits but is not a new record; the table is unchanged.
void test_not_a_record() {
    ResultsFixture fx;
    TEST_CHECK(blaze4k::results_submit_score(fx.scores, make_summary(fx.song, fx.chart, "quad_star", 1.0),
                                        1));
    fx.start();

    TEST_CHECK(fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());

    const std::string key = blaze4k::make_chart_key(fx.song, fx.chart);
    TEST_CHECK(blaze4k::find_high_score(fx.scores, key)->percent == 1.0);
    TEST_CHECK(blaze4k::find_high_score(fx.scores, key)->timestamp_unix == 1);
    std::cout << "  - a sub-best run does not flag a record ok.\n";
}

// 3. A failed run shows its stats, submits nothing, and never flags (OQ2/AC4).
void test_failed_run_no_submit() {
    ResultsFixture fx;
    fx.summary = make_summary(fx.song, fx.chart, "D", 0.30, true);
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(fx.results->summary().failed);
    TEST_CHECK(!fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());
    TEST_CHECK(fx.scores.scores.empty());
    std::cout << "  - failed run submits nothing ok.\n";
}

// 3b. A valid, clearing run that cannot be stored (empty grade label) must not
//     report `submitted` and must not insert a record.
void test_unstorable_run_not_submitted() {
    ResultsFixture fx;
    fx.summary = make_summary(fx.song, fx.chart, "", 0.95);
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(!fx.results->summary().failed);
    TEST_CHECK(!fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());
    TEST_CHECK(fx.scores.scores.empty());
    std::cout << "  - unstorable clear does not report submitted ok.\n";
}

// 4. Confirm skips the reveal, then returns to the song wheel (AC3).
void test_confirm_returns_to_wheel() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(!fx.results->reveal_finished());

    // First Confirm skips to the final frame without navigating (AC3).
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(fx.results->reveal_finished());

    // Second Confirm exits to the wheel (C7 behavior).
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.select_enters == 1);
    std::cout << "  - Confirm skips then exits ok.\n";
}

// 4b. Options/Right also skip while the reveal runs; only a finished reveal navigates.
void test_reveal_gating_skip_presses() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(!fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    std::cout << "  - Options skips, Right exits ok.\n";
}

// 4c. The NEW RECORD finale flag follows C7's best-score rule, and the screen's
//     single render gate (`shows_record_finale`) is false when it must not draw.
void test_new_record_finale_flag() {
    // A first clear plays the full finale.
    ResultsFixture first;
    first.start();
    TEST_CHECK(first.results->animator().new_record());
    TEST_CHECK(first.results->shows_record_finale());

    // A worse-than-best run does not, so the banner/flash is never drawn.
    ResultsFixture best;
    TEST_CHECK(blaze4k::results_submit_score(
        best.scores, make_summary(best.song, best.chart, "quad_star", 1.0), 1));
    best.start();
    TEST_CHECK(!best.results->animator().new_record());
    TEST_CHECK(!best.results->shows_record_finale());

    // A failed run never plays it (nor draws the finale).
    ResultsFixture failed;
    failed.summary = make_summary(failed.song, failed.chart, "D", 0.30, true);
    failed.start();
    TEST_CHECK(!failed.results->animator().new_record());
    TEST_CHECK(!failed.results->shows_record_finale());
    std::cout << "  - NEW RECORD finale flag gating ok.\n";
}

// 4d. An invalid (NO RESULT) summary has no reveal: a single Confirm/Options/Right
//     exits immediately (exactly C7) instead of being swallowed as a skip.
void test_invalid_summary_exits_immediately() {
    ResultsFixture fx;
    fx.start(false); // no published result -> invalid summary
    TEST_CHECK(!fx.results->valid());
    TEST_CHECK(!fx.results->shows_record_finale());
    TEST_CHECK(!fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.select_enters == 1);

    // Options and Right behave the same on a fresh invalid screen.
    ResultsFixture options;
    options.start(false);
    options.manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(options.manager.active_id() == ScreenId::Select);

    ResultsFixture right;
    right.start(false);
    right.manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(right.manager.active_id() == ScreenId::Select);
    std::cout << "  - invalid summary exits on first press ok.\n";
}

// 4e. The hint word (owner decision 6): ENTER SKIP while the reveal runs, ENTER
//     CONTINUE once it settles (skipped or played out) and on a NO RESULT screen.
void test_hint_word() {
    ResultsFixture skipped;
    skipped.start();
    TEST_CHECK(skipped.results->hint_word() == "SKIP");
    skipped.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(skipped.manager.active_id() == ScreenId::Results);
    TEST_CHECK(skipped.results->hint_word() == "CONTINUE");

    ResultsFixture played;
    played.start();
    for (int i = 0; i < 1000 && !played.results->reveal_finished(); ++i) {
        played.manager.update(0.1, {});
    }
    TEST_CHECK(played.results->reveal_finished());
    TEST_CHECK(played.results->hint_word() == "CONTINUE");

    ResultsFixture empty;
    empty.start(false);
    TEST_CHECK(!empty.results->reveal_finished());
    TEST_CHECK(empty.results->hint_word() == "CONTINUE");
    std::cout << "  - hint word SKIP -> CONTINUE ok.\n";
}

// 5. Back goes through the manager default to the wheel (AC3, no dead end).
void test_back_returns_to_wheel() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(fx.manager.back_navigates());

    fx.manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    std::cout << "  - Back -> Select ok.\n";
}

// 6. Headless render is a no-op; re-entering with no handoff is invalid + safe.
void test_render_and_reenter() {
    ResultsFixture fx;
    fx.start();

    blaze4k::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    fx.manager.render(renderer, 1280, 720);
    TEST_CHECK(fx.results->valid());

    fx.manager.context().results = nullptr;
    fx.manager.start(ScreenId::Results); // re-enter with no published result
    TEST_CHECK(!fx.results->valid());
    TEST_CHECK(!fx.results->submitted());
    fx.manager.render(renderer, 1280, 720);
    std::cout << "  - headless render + re-enter robustness ok.\n";
}

// 6b. Screen title: the baked sprite exists, with the text fallback (#95).
void test_screen_title_text() {
    TEST_CHECK(blaze4k::results_art::kScreenTitleText == "SCORE SCREEN");
    const auto it = real_manifest().textures.find("title_score_screen");
    TEST_CHECK(it != real_manifest().textures.end());
    TEST_CHECK(it->second.kind == blaze4k::ThemeKind::Sprite);
    std::cout << "  - SCORE SCREEN title sprite + fallback text ok.\n";
}

// 6c. Every compiled grade tier maps to a 680x400 @2x grade sprite; tier labels.
void test_grade_textures() {
    namespace art = blaze4k::results_art;
    const auto& tiers = blaze4k::JudgmentConstants::compiled_defaults().grade_tiers;
    for (const blaze4k::GradeTier& tier : tiers) {
        const std::string name = art::grade_texture_name(tier.label);
        const auto it = real_manifest().textures.find(name);
        TEST_CHECK(it != real_manifest().textures.end());
        TEST_CHECK(it->second.kind == blaze4k::ThemeKind::Sprite);
        TEST_CHECK(it->second.content.w == 680 && it->second.content.h == 400);
        TEST_CHECK(near(art::kGradeContentRef.x * real_manifest().texture_scale, 680.0f));
        TEST_CHECK(near(art::kGradeContentRef.y * real_manifest().texture_scale, 400.0f));
        TEST_CHECK(!art::grade_tier_text(tier.label).empty());
    }
    TEST_CHECK(art::grade_texture_name("quad_star") == "grade_quad_star");
    TEST_CHECK(art::grade_texture_name("S+") == "grade_S_plus");
    TEST_CHECK(art::grade_texture_name("A-") == "grade_A_minus");
    TEST_CHECK(art::grade_texture_name("D") == "grade_D");
    TEST_CHECK(art::grade_texture_name("").empty());

    TEST_CHECK(art::grade_tier_text("quad_star") == "FOUR STARS");
    TEST_CHECK(art::grade_tier_text("triple_star") == "THREE STARS");
    TEST_CHECK(art::grade_tier_text("double_star") == "TWO STARS");
    TEST_CHECK(art::grade_tier_text("single_star") == "ONE STAR");
    TEST_CHECK(art::grade_tier_text("S+") == "GRADE S+");
    TEST_CHECK(art::grade_tier_text("").empty());
    std::cout << "  - grade textures + tier labels ok.\n";
}

// 6d. Only 0-9 . % / and space ever reach BitmapDigits (negative DP shows 0).
void test_digit_strings() {
    namespace art = blaze4k::results_art;
    TEST_CHECK(art::digits_text(-48) == "0");
    TEST_CHECK(art::digits_text(0) == "0");
    TEST_CHECK(art::digits_text(1928) == "1928");

    ResultsFixture fx;
    fx.summary.actual_dp = -30;
    fx.summary.possible_dp = 2000;
    fx.summary.max_combo = -1;
    fx.summary.hold_counts = {12, 3};
    fx.summary.tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::HitMine)] = 4;
    fx.start();
    TEST_CHECK(fx.results->dp_text() == "0");
    TEST_CHECK(fx.results->dp_max_text() == "/ 2000");
    TEST_CHECK(fx.results->max_combo_text() == "0");
    TEST_CHECK(fx.results->hold_texts()[0] == "12");
    TEST_CHECK(fx.results->hold_texts()[1] == "3");
    TEST_CHECK(fx.results->hold_texts()[2] == "4"); // MINES = mines stepped on
    TEST_CHECK(digits_only(fx.results->dp_text()));
    TEST_CHECK(digits_only(fx.results->dp_max_text()));
    TEST_CHECK(digits_only(fx.results->max_combo_text()));
    for (const std::string& text : fx.results->hold_texts()) {
        TEST_CHECK(!text.empty());
        TEST_CHECK(digits_only(text));
    }
    for (const double p : {-2.4, 0.0, 0.9642, 1.0, 1.5}) {
        TEST_CHECK(digits_only(blaze4k::format_percent(p)));
    }
    std::cout << "  - digit strings stay inside the atlas glyph set ok.\n";
}

// 6e. Top bar group: artist, title and badge plate right-aligned at 1240.
void test_top_bar_layout() {
    namespace art = blaze4k::results_art;
    // Mock: "HARD 8" ~59, "Anubis"-like title ~118, artist ~50.
    const art::TopBarLayout mock = art::top_bar_layout(59.0f, 118.0f, 50.0f);
    TEST_CHECK(near(mock.artist_x, 1190.0f));
    TEST_CHECK(near(mock.artist_max_w, 50.0f));
    TEST_CHECK(near(mock.title_x, 1056.0f));
    TEST_CHECK(near(mock.title_max_w, 118.0f));
    TEST_CHECK(near(mock.plate.x, 957.0f) && near(mock.plate.y, 16.0f));
    TEST_CHECK(near(mock.plate.w, 83.0f) && near(mock.plate.h, 32.0f));
    TEST_CHECK(near(mock.plate.x + mock.plate.w, 1040.0f)); // mock plate ink 957..1040
    TEST_CHECK(near(mock.badge_text_x, 969.0f));
    TEST_CHECK(near(mock.badge_text_max_w, 59.0f));
    TEST_CHECK(near(mock.baseline, 40.0f));

    // No artist: the title ends at 1240.
    const art::TopBarLayout no_artist = art::top_bar_layout(59.0f, 118.0f, 0.0f);
    TEST_CHECK(near(no_artist.artist_max_w, 0.0f));
    TEST_CHECK(near(no_artist.title_x + no_artist.title_max_w, 1240.0f));

    // A 2000px title is clamped so the plate stays clear of the title sprite.
    const art::TopBarLayout long_title = art::top_bar_layout(59.0f, 2000.0f, 50.0f);
    TEST_CHECK(long_title.plate.x >= art::kBarLeftLimit - 1e-3f);
    TEST_CHECK(long_title.title_max_w < 2000.0f);
    // Long artist and badge texts are capped.
    const art::TopBarLayout long_all = art::top_bar_layout(900.0f, 2000.0f, 900.0f);
    TEST_CHECK(near(long_all.artist_max_w, art::kBarArtistMax));
    TEST_CHECK(near(long_all.badge_text_max_w, art::kBarBadgeTextMax));
    TEST_CHECK(long_all.plate.x >= art::kBarLeftLimit - 1e-3f);

    // No badge text: no plate.
    const art::TopBarLayout no_badge = art::top_bar_layout(0.0f, 118.0f, 50.0f);
    TEST_CHECK(no_badge.plate.w == 0.0f);
    TEST_CHECK(near(no_badge.title_x, 1056.0f));

    // Non-finite input never produces NaN.
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const art::TopBarLayout bad = art::top_bar_layout(nan, inf, -inf);
    for (const float v : {bad.plate.x, bad.plate.w, bad.title_x, bad.title_max_w, bad.artist_x,
                          bad.artist_max_w, bad.badge_text_x, bad.badge_text_max_w}) {
        TEST_CHECK(std::isfinite(v));
    }

    // #110: no subtitle (0, negative or non-finite) -> field-identical to the 3-width layout.
    const auto same_layout = [](const art::TopBarLayout& a, const art::TopBarLayout& b) {
        return a.plate.x == b.plate.x && a.plate.y == b.plate.y && a.plate.w == b.plate.w &&
               a.plate.h == b.plate.h && a.badge_text_x == b.badge_text_x &&
               a.badge_text_max_w == b.badge_text_max_w && a.title_x == b.title_x &&
               a.title_max_w == b.title_max_w && a.artist_x == b.artist_x &&
               a.artist_max_w == b.artist_max_w && a.baseline == b.baseline &&
               a.subtitle_x == b.subtitle_x && a.subtitle_max_w == b.subtitle_max_w;
    };
    for (const float none : {0.0f, -5.0f, nan, inf}) {
        const art::TopBarLayout same = art::top_bar_layout(59.0f, 118.0f, 50.0f, none);
        TEST_CHECK(same_layout(same, mock));
        TEST_CHECK(same.subtitle_max_w == 0.0f && same.subtitle_x == 0.0f);
        TEST_CHECK(same_layout(art::top_bar_layout(59.0f, 2000.0f, 50.0f, none), long_title));
    }
    TEST_CHECK(near(art::kBarSubtitleGap, 8.0f));

    // A subtitle: title + 8 + subtitle as one group ending at artist_x - kBarGap.
    const art::TopBarLayout sub = art::top_bar_layout(59.0f, 118.0f, 50.0f, 60.0f);
    TEST_CHECK(near(sub.title_max_w, 118.0f));
    TEST_CHECK(near(sub.subtitle_max_w, 60.0f));
    TEST_CHECK(near(sub.subtitle_x, sub.title_x + 118.0f + art::kBarSubtitleGap));
    TEST_CHECK(near(sub.subtitle_x + 60.0f, sub.artist_x - art::kBarGap));
    TEST_CHECK(near(sub.artist_x, mock.artist_x) && near(sub.artist_max_w, mock.artist_max_w));
    TEST_CHECK(near(sub.title_x, mock.title_x - 68.0f));
    TEST_CHECK(near(sub.plate.x + sub.plate.w, sub.title_x - art::kBarGap));
    TEST_CHECK(near(sub.plate.w, mock.plate.w) && near(sub.baseline, 40.0f));

    // A long title + subtitle: the plate stays clear of the sprite and the subtitle
    // keeps >= 40% of the slot (after the gap).
    const art::TopBarLayout long_sub = art::top_bar_layout(59.0f, 2000.0f, 50.0f, 300.0f);
    const float slot = (long_sub.artist_x - art::kBarGap) - long_sub.title_x;
    TEST_CHECK(long_sub.plate.x >= art::kBarLeftLimit - 1e-3f);
    TEST_CHECK(long_sub.subtitle_max_w >=
               blaze4k::kSubtitleMinShare * (slot - art::kBarSubtitleGap) - 1e-3f);
    TEST_CHECK(long_sub.title_max_w < 2000.0f);
    TEST_CHECK(near(long_sub.subtitle_x,
                    long_sub.title_x + long_sub.title_max_w + art::kBarSubtitleGap));
    TEST_CHECK(long_sub.subtitle_x + long_sub.subtitle_max_w <=
               long_sub.artist_x - art::kBarGap + 1e-3f);

    // No title: the subtitle is treated as absent.
    const art::TopBarLayout no_title = art::top_bar_layout(59.0f, 0.0f, 50.0f, 60.0f);
    TEST_CHECK(same_layout(no_title, art::top_bar_layout(59.0f, 0.0f, 50.0f)));

    // Non-finite title/badge with a subtitle never produces NaN.
    const art::TopBarLayout bad_sub = art::top_bar_layout(nan, inf, 50.0f, 60.0f);
    for (const float v : {bad_sub.plate.x, bad_sub.title_x, bad_sub.title_max_w,
                          bad_sub.subtitle_x, bad_sub.subtitle_max_w}) {
        TEST_CHECK(std::isfinite(v));
    }
    std::cout << "  - top bar layout ok.\n";
}

// 6f. Stat panels: measured rects, slanted text x, holds columns.
void test_stat_panels() {
    namespace art = blaze4k::results_art;
    const blaze4k::Rect p0 = art::stat_panel_rect(0);
    const blaze4k::Rect p1 = art::stat_panel_rect(1);
    const blaze4k::Rect p2 = art::stat_panel_rect(2);
    TEST_CHECK(near(p0.x, 44.0f) && near(p0.y, 120.0f) && near(p0.w, 360.0f) && near(p0.h, 111.0f));
    TEST_CHECK(near(p1.x, 44.0f) && near(p1.y, 245.0f) && near(p1.w, 360.0f) && near(p1.h, 111.0f));
    TEST_CHECK(near(p2.x, 44.0f) && near(p2.y, 370.0f) && near(p2.w, 360.0f) && near(p2.h, 102.0f));
    TEST_CHECK(near(art::stat_panel_rect(-3).y, p0.y));
    TEST_CHECK(near(art::stat_panel_rect(9).y, p2.y));

    // Mock label ink x 69 (line top +14, cap centre +14.5) and value ink x 64 (baseline 86).
    constexpr float kLeftMargin = 10.0f; // #127
    const float label_x = art::stat_text_x(p0, p0.y + art::kStatLabelTop + art::kStatLabelCapCentre);
    const float value_x = art::stat_text_x(p0, p0.y + art::kValueBaseline - art::kDigitCapHalfRef);
    TEST_CHECK(near(label_x, 67.807f + kLeftMargin, 0.01f));
    TEST_CHECK(near(value_x, 62.026f + kLeftMargin, 0.01f));
    TEST_CHECK(std::isfinite(art::stat_text_x(p0, std::numeric_limits<float>::quiet_NaN())));

    // Mock: HOLDS OK -> NG -> MINES ink at 69 / 192 / 246.
    const std::array<float, 3> cols = art::hold_columns({89.0f, 21.0f, 56.0f}, {59.0f, 9.0f, 33.0f});
    TEST_CHECK(near(cols[0], 0.0f) && near(cols[1], 123.0f) && near(cols[2], 178.0f));
    // A four-digit hold count wider than its label pushes the next column right.
    const std::array<float, 3> wide = art::hold_columns({89.0f, 21.0f, 56.0f}, {120.0f, 9.0f, 33.0f});
    TEST_CHECK(near(wide[1], 154.0f) && near(wide[2], 209.0f));
    const std::array<float, 3> tight = art::hold_columns({100.0f, 100.0f, 100.0f}, {});
    TEST_CHECK(near(tight[0], 0.0f) && near(tight[1], 100.0f + art::kHoldColumnMinGap) &&
               near(tight[2], 200.0f + 2.0f * art::kHoldColumnMinGap));

    const blaze4k::TextRenderer& text = loaded_text();
    const blaze4k::BitmapDigits& digits = loaded_theme().digits_white();
    constexpr std::array<std::string_view, 3> kLabels = {"HOLDS OK", "NG", "MINES"};
    using Counts = std::array<std::string_view, 3>;
    for (const Counts& counts : {Counts{"999", "999", "999"}, Counts{"148", "148", "152"}}) {
        std::array<float, 3> label_w{};
        std::array<float, 3> value_w{};
        std::array<float, 3> col{};
        for (std::size_t i = 0; i < 3; ++i) {
            label_w[i] = blaze4k::ref_measure(text, kLabels[i], blaze4k::theme::text::kStatLabel);
            value_w[i] = digits.measure(counts[i], art::kHoldScale);
            col[i] = std::max(label_w[i], value_w[i]);
        }
        const std::array<float, 3> fit = art::hold_columns(label_w, value_w);
        TEST_CHECK(fit[2] + col[2] <= art::kHoldRowWidth + 1e-3f);
        TEST_CHECK(fit[1] - col[0] >= art::kHoldColumnMinGap - 1e-3f);
        TEST_CHECK(fit[2] - fit[1] - col[1] >= art::kHoldColumnMinGap - 1e-3f);
    }
    std::cout << "  - stat panels layout ok.\n";
}

// 6g. Judgment rows: mock coordinates and proportional fills.
void test_judgment_rows() {
    namespace art = blaze4k::results_art;
    const art::JudgmentRowLayout r0 = art::judgment_row_layout(0);
    TEST_CHECK(near(r0.label_x, 878.0f) && near(r0.baseline, 145.0f));
    TEST_CHECK(near(r0.bar.x, 1018.0f) && near(r0.bar.y, 131.0f));
    TEST_CHECK(near(r0.bar.w, 154.0f) && near(r0.bar.h, 14.0f));
    TEST_CHECK(near(r0.count_right, 1238.0f));
    const art::JudgmentRowLayout r5 = art::judgment_row_layout(5);
    TEST_CHECK(near(r5.baseline, 370.0f) && near(r5.bar.y, 356.0f) && near(r5.bar.x, 1018.0f));
    TEST_CHECK(near(art::judgment_row_layout(42).baseline, r5.baseline));
    TEST_CHECK(art::kJudgmentRowLabels[0] == "FANTASTIC" && art::kJudgmentRowLabels[5] == "MISS");
    TEST_CHECK(same_color(art::judgment_label_color(5), blaze4k::theme::color::kMissLabel));
    TEST_CHECK(same_color(art::judgment_row_color(5), blaze4k::theme::color::kMiss));
    TEST_CHECK(same_color(art::judgment_label_color(0), blaze4k::theme::color::kFantastic));

    TEST_CHECK(near(art::judgment_bar_fill_width(312, 362, 154.0f), 154.0f * 312.0f / 362.0f));
    TEST_CHECK(near(art::judgment_bar_fill_width(1, 362, 154.0f), 2.0f));
    TEST_CHECK(art::judgment_bar_fill_width(0, 362, 154.0f) == 0.0f);
    TEST_CHECK(near(art::judgment_bar_fill_width(5, 3, 154.0f), 154.0f));
    TEST_CHECK(art::judgment_bar_fill_width(5, 0, 154.0f) == 0.0f);
    TEST_CHECK(art::judgment_bar_fill_width(-1, 10, 154.0f) == 0.0f);

    ResultsSummary summary;
    summary.tap_counts = {312, 40, 6, 1, 0, 2, 9}; // HitMine = 9 is excluded
    TEST_CHECK(art::judged_tap_total(summary) == 361);
    std::cout << "  - judgment rows layout + fills ok.\n";
}

// 6h. Ribbon and grade rects keep their centres when scaled.
void test_ribbon_and_grade_rects() {
    namespace art = blaze4k::results_art;
    const blaze4k::Rect& ribbon = blaze4k::theme::layout::kRecordRibbon;
    const blaze4k::Rect r1 = art::ribbon_rect(1.0f);
    TEST_CHECK(near(r1.x, ribbon.x) && near(r1.y, ribbon.y) && near(r1.w, ribbon.w) &&
               near(r1.h, ribbon.h));
    const blaze4k::Rect r2 = art::ribbon_rect(2.0f);
    TEST_CHECK(near(r2.x + r2.w * 0.5f, 640.0f) && near(r2.y + r2.h * 0.5f, 622.0f));
    TEST_CHECK(near(r2.w, 680.0f) && near(r2.h, 88.0f));

    const blaze4k::Rect g1 = art::grade_rect(art::kGradeContentRef, 1.0f);
    TEST_CHECK(near(g1.x, 470.0f) && near(g1.y, 186.0f) && near(g1.w, 340.0f) && near(g1.h, 200.0f));
    const blaze4k::Rect g24 = art::grade_rect(art::kGradeContentRef, 2.4f);
    TEST_CHECK(near(g24.x + g24.w * 0.5f, 640.0f) && near(g24.y + g24.h * 0.5f, 286.0f));
    TEST_CHECK(near(g24.w, 816.0f, 0.01f));
    const blaze4k::Rect bad = art::grade_rect(art::kGradeContentRef,
                                              std::numeric_limits<float>::quiet_NaN());
    TEST_CHECK(std::isfinite(bad.x) && bad.w == 0.0f);
    std::cout << "  - ribbon + grade rects ok.\n";
}

// 6i. enter() caches the badge (#93 rule) and the display strings; Edit charts
//     show their name, which never changes the high-score key.
void test_enter_caches_badge() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(fx.results->badge().label == "HARD");
    TEST_CHECK(fx.results->badge().meter == "9");
    TEST_CHECK(same_color(fx.results->badge().colors.fill, blaze4k::theme::difficulty::kHard.fill));
    TEST_CHECK(same_color(fx.results->badge().colors.ink, blaze4k::theme::difficulty::kHard.ink));
    TEST_CHECK(fx.results->display_title() == "Blaze Anthem");
    TEST_CHECK(fx.results->display_artist() == "Test Artist");
    TEST_CHECK(fx.results->display_subtitle().empty());
    TEST_CHECK(fx.results->grade_texture() == "grade_S_plus");
    TEST_CHECK(fx.results->tier_text() == "GRADE S+");
    TEST_CHECK(fx.results->dp_text() == "40");
    TEST_CHECK(fx.results->dp_max_text() == "/ 50");
    TEST_CHECK(fx.results->max_combo_text() == "7");
    TEST_CHECK(fx.results->count_texts()[0] == "0");

    ResultsFixture edit;
    edit.chart.difficulty = "Edit";
    edit.chart.meter = 10;
    edit.chart.description = "JBEAN";
    edit.summary = make_summary(edit.song, edit.chart, "single_star", 1.0);
    edit.start();
    TEST_CHECK(edit.results->badge().label == "JBEAN");
    TEST_CHECK(edit.results->badge().meter == "10");
    TEST_CHECK(same_color(edit.results->badge().colors.fill, blaze4k::theme::difficulty::kEdit.fill));
    TEST_CHECK(edit.results->grade_texture() == "grade_single_star");
    TEST_CHECK(edit.results->tier_text() == "ONE STAR");

    // #110: the subtitle is cached on its own; the title is unchanged.
    ResultsFixture subtitled;
    subtitled.song.metadata.subtitle = "-Hyper-";
    subtitled.start();
    TEST_CHECK(subtitled.results->display_subtitle() == "-Hyper-");
    TEST_CHECK(subtitled.results->display_title() == "Blaze Anthem");

    // An empty title gives its place to the subtitle (same rule as song select).
    ResultsFixture untitled;
    untitled.song.metadata.title.clear();
    untitled.song.metadata.subtitle = "-Hyper-";
    untitled.start();
    TEST_CHECK(untitled.results->display_title() == "-Hyper-");
    TEST_CHECK(untitled.results->display_subtitle().empty());

    // No chart / song: no badge, "UNKNOWN" title.
    ResultsFixture bare;
    bare.summary.chart = nullptr;
    bare.summary.song = nullptr;
    bare.start();
    TEST_CHECK(bare.results->badge().label.empty() && bare.results->badge().meter.empty());
    TEST_CHECK(bare.results->display_title() == "UNKNOWN");
    TEST_CHECK(bare.results->display_artist().empty());
    TEST_CHECK(bare.results->display_subtitle().empty());

    // An invalid summary clears every cached string.
    ResultsFixture invalid;
    invalid.song.metadata.subtitle = "-Hyper-";
    invalid.start(false);
    TEST_CHECK(invalid.results->display_subtitle().empty());
    TEST_CHECK(invalid.results->dp_text().empty());
    TEST_CHECK(invalid.results->tier_text().empty());
    TEST_CHECK(invalid.results->badge().label.empty());

    // The chart name is draw-only: high-score keys ignore the description.
    const Song song = make_song();
    Chart named = edit.chart;
    Chart renamed = edit.chart;
    renamed.description = "Another Name";
    TEST_CHECK(blaze4k::make_chart_key(song, named) == blaze4k::make_chart_key(song, renamed));
    std::cout << "  - enter caches badge + display strings ok.\n";
}

// 6j. Headless Cabinet render with the real theme and fonts at several window
//     sizes for every state (clear, NEW RECORD, failed, named Edit, subtitled, NO RESULT).
void render_all_sizes(ResultsFixture& fx, blaze4k::GlQuadRenderer& renderer) {
    blaze4k::TextRenderer& text = loaded_text();
    const std::array<std::array<int, 2>, 6> sizes = {
        {{1280, 720}, {2560, 1440}, {3440, 1440}, {640, 480}, {320, 240}, {0, 0}}};
    for (int step = 0; step < 30; ++step) {
        fx.manager.update(0.1, {});
        for (const auto& size : sizes) {
            text.set_window_size(size[0], size[1]);
            fx.manager.render(renderer, size[0], size[1]);
        }
    }
    text.set_window_size(1280, 720);
}

void test_render_cabinet_headless() {
    blaze4k::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    const auto attach = [](ResultsFixture& fx) {
        fx.manager.context().theme = &loaded_theme();
        fx.manager.context().text = &loaded_text();
    };

    // Normal clear (sub-best: no ribbon).
    {
        ResultsFixture fx;
        TEST_CHECK(blaze4k::results_submit_score(
            fx.scores, make_summary(fx.song, fx.chart, "quad_star", 1.0), 1));
        attach(fx);
        fx.start();
        render_all_sizes(fx, renderer);
        TEST_CHECK(fx.results->valid());
        TEST_CHECK(!fx.results->shows_record_finale());
        TEST_CHECK(fx.results->reveal_finished());
    }
    // NEW RECORD.
    {
        ResultsFixture fx;
        fx.summary.tap_counts = {312, 40, 6, 1, 0, 2, 1};
        fx.summary.hold_counts = {1234, 5};
        attach(fx);
        fx.start();
        TEST_CHECK(fx.results->shows_record_finale());
        render_all_sizes(fx, renderer);
        TEST_CHECK(fx.results->valid());
    }
    // Failed run: the earned grade + FAILED ribbon.
    {
        ResultsFixture fx;
        fx.summary = make_summary(fx.song, fx.chart, "D", 0.30, true);
        fx.summary.actual_dp = -120;
        attach(fx);
        fx.start();
        TEST_CHECK(!fx.results->shows_record_finale());
        TEST_CHECK(fx.results->grade_texture() == "grade_D");
        render_all_sizes(fx, renderer);
        TEST_CHECK(fx.results->valid());
    }
    // A named Edit chart with a long UTF-8 name.
    {
        ResultsFixture fx;
        fx.chart.difficulty = "Edit";
        fx.chart.meter = 10;
        fx.chart.description = "Caf\xC3\xA9 \xE2\x98\xBA " + std::string(100, 'W');
        fx.song.metadata.title = std::string(300, 'T');
        fx.song.metadata.artist = std::string(300, 'A');
        attach(fx);
        fx.start();
        render_all_sizes(fx, renderer);
        TEST_CHECK(fx.results->valid());
        TEST_CHECK(fx.results->summary().chart == &fx.chart);
    }
    // A subtitled song (#110): fitted with the text service, empty without.
    for (const bool services : {true, false}) {
        ResultsFixture fx;
        fx.song.metadata.title = "Disconnected";
        fx.song.metadata.subtitle = "-Hyper-";
        if (services) {
            attach(fx);
        }
        fx.start();
        TEST_CHECK(fx.results->display_subtitle() == "-Hyper-");
        if (services) {
            render_all_sizes(fx, renderer);
            fx.manager.render(renderer, 1280, 720);
            TEST_CHECK(fx.results->fitted_subtitle() == "-Hyper-");
        } else {
            for (int i = 0; i < 30; ++i) {
                fx.manager.update(0.1, {});
                fx.manager.render(renderer, 1280, 720);
            }
            TEST_CHECK(fx.results->fitted_subtitle().empty());
        }
        TEST_CHECK(fx.results->valid());
    }
    // NO RESULT.
    {
        ResultsFixture fx;
        attach(fx);
        fx.start(false);
        render_all_sizes(fx, renderer);
        TEST_CHECK(!fx.results->valid());
        TEST_CHECK(!fx.results->shows_record_finale());
    }
    std::cout << "  - headless Cabinet render (real theme + fonts, 6 sizes, 6 states) ok.\n";
}

// 6c. Headless render of a results summary for a named Edit chart.
void test_render_edit_chart() {
    ResultsFixture fx;
    fx.chart.difficulty = "Edit";
    fx.chart.meter = 10;
    fx.chart.description = "Caf\xC3\xA9 \xE2\x98\xBA " + std::string(100, 'W');
    fx.start();

    blaze4k::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    for (int i = 0; i < 40; ++i) {
        fx.manager.update(0.1, {});
        fx.manager.render(renderer, 1280, 720);
        fx.manager.render(renderer, 320, 240);
    }
    TEST_CHECK(fx.results->valid());
    TEST_CHECK(fx.results->summary().chart == &fx.chart);
    std::cout << "  - headless render with a named Edit chart ok.\n";
}

// 7. End-to-end: a real GameplayScreen run reports to Results, which submits and
//    returns to Select on Confirm (mirrors the headless completed-run pattern).
void test_gameplay_to_results_end_to_end() {
    blaze4k::GameConfig config;
    HighScores scores;
    Song song = make_song();
    Chart chart = make_chart();

    blaze4k::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options = blaze4k::GameplayOptions{};

    ResultsSummary summary;
    blaze4k::ScreenManager manager(0.0);

    auto gameplay_owner = std::make_unique<blaze4k::GameplayScreen>();
    blaze4k::GameplayScreen* gameplay = gameplay_owner.get();
    auto results_owner = std::make_unique<blaze4k::ResultsScreen>();
    blaze4k::ResultsScreen* results = results_owner.get();

    manager.add_screen(std::move(gameplay_owner));
    manager.add_screen(std::move(results_owner));
    manager.add_screen(std::make_unique<SelectSpy>());
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().play_request = &request;
    manager.context().results = &summary;
    manager.start(ScreenId::Gameplay);
    TEST_CHECK(gameplay->is_ready());

    // Hit the single tap on the first frame (its time is 0.0) so the run clears
    // with a non-negative percent; a full miss would be negative and is rejected
    // by the percent<0 submit guard.
    manager.update(1.0 / 60.0, {press(GameAction::Left)});
    for (int i = 0; i < 180 && manager.active_id() != ScreenId::Results; ++i) {
        manager.update(1.0 / 60.0, {});
    }

    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(gameplay->end_reported());
    TEST_CHECK(summary.valid);
    TEST_CHECK(summary.song == &song);
    TEST_CHECK(summary.chart == &chart);
    TEST_CHECK(results->valid());
    TEST_CHECK(results->submitted());

    const std::string key = blaze4k::make_chart_key(song, chart);
    TEST_CHECK(blaze4k::find_high_score(scores, key) != nullptr);
    TEST_CHECK(scores.scores.size() == 1);

    // Extra frames on Results must not re-enter or resubmit (exactly-once handoff).
    for (int i = 0; i < 30; ++i) {
        manager.update(1.0 / 60.0, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(scores.scores.size() == 1);
    TEST_CHECK(gameplay->end_reported());

    // The reveal is running on entry: the first Confirm skips it in place...
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(results->reveal_finished());

    // ...and the second Confirm returns to the wheel (C7 behavior).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // Frames after returning to the wheel must not resurrect the run or resubmit.
    for (int i = 0; i < 30; ++i) {
        manager.update(1.0 / 60.0, {});
    }
    TEST_CHECK(scores.scores.size() == 1);
    TEST_CHECK(gameplay->end_reported());
    std::cout << "  - Gameplay -> Results -> Select end-to-end ok.\n";
}

} // namespace

int main() {
    std::cout << "[results_screen_test] Running ResultsScreen tests...\n";
    test_enter_submit_and_flag();
    test_not_a_record();
    test_failed_run_no_submit();
    test_unstorable_run_not_submitted();
    test_confirm_returns_to_wheel();
    test_reveal_gating_skip_presses();
    test_new_record_finale_flag();
    test_invalid_summary_exits_immediately();
    test_hint_word();
    test_back_returns_to_wheel();
    test_render_and_reenter();
    test_screen_title_text();
    test_grade_textures();
    test_digit_strings();
    test_top_bar_layout();
    test_stat_panels();
    test_judgment_rows();
    test_ribbon_and_grade_rects();
    test_enter_caches_badge();
    test_render_cabinet_headless();
    test_render_edit_chart();
    test_gameplay_to_results_end_to_end();
    std::cout << "[results_screen_test] All tests passed!\n";
    return 0;
}
