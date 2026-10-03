#include "screens/select_screen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "chart/song_library.hpp"
#include "data/config.hpp"
#include "data/high_scores.hpp"
#include "gameplay/hud_renderer.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/play_request.hpp"
#include "screens/screen_manager.hpp"
#include "screens/song_display_text.hpp"

namespace blaze4k {

namespace {

constexpr double kDefaultPreviewStartSeconds = 0.0;
constexpr double kDefaultPreviewLengthSeconds = 12.0;

// Held-direction key repeat: wait kRepeatDelaySeconds before the first repeat,
// then repeat every kRepeatInitialIntervalSeconds, shrinking by kRepeatAccelFactor
// each step down to kRepeatMinIntervalSeconds. Holding an arrow therefore walks
// the wheel slowly at first, then accelerates.
constexpr double kRepeatDelaySeconds = 0.35;
constexpr double kRepeatInitialIntervalSeconds = 0.12;
constexpr double kRepeatMinIntervalSeconds = 0.04;
constexpr double kRepeatAccelFactor = 0.75;

constexpr Color kTitleColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kPackColor{0.90f, 0.75f, 0.30f, 1.0f};
constexpr Color kSelectedColor{1.00f, 0.92f, 0.35f, 1.0f};
constexpr Color kTextColor{0.82f, 0.87f, 0.95f, 1.0f};
constexpr Color kDimColor{0.55f, 0.60f, 0.72f, 1.0f};
constexpr Color kHintColor{0.60f, 0.66f, 0.78f, 1.0f};
constexpr Color kPlaceholderColor{0.16f, 0.20f, 0.30f, 1.0f};

constexpr Color kBeginnerColor{0.78f, 0.52f, 1.00f, 1.0f};
constexpr Color kEasyColor{0.40f, 0.90f, 0.45f, 1.0f};
constexpr Color kMediumColor{1.00f, 0.88f, 0.30f, 1.0f};
constexpr Color kHardColor{1.00f, 0.38f, 0.38f, 1.0f};
constexpr Color kChallengeColor{0.40f, 0.65f, 1.00f, 1.0f};

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string format_bpm_value(double bpm) {
    std::ostringstream out;
    if (std::fabs(bpm - std::round(bpm)) < 1e-9) {
        out << static_cast<long long>(std::llround(bpm));
    } else {
        out << std::fixed << std::setprecision(1) << bpm;
    }
    return out.str();
}

} // namespace

std::string format_bpm_range(const TimingData& timing) {
    const std::vector<BpmSegment>& bpms = timing.bpms();
    if (bpms.empty()) {
        return "?";
    }

    double lowest = bpms.front().bpm;
    double highest = bpms.front().bpm;
    bool all_equal = true;
    for (const BpmSegment& segment : bpms) {
        lowest = std::min(lowest, segment.bpm);
        highest = std::max(highest, segment.bpm);
        if (segment.bpm != bpms.front().bpm) {
            all_equal = false;
        }
    }

    if (all_equal) {
        return format_bpm_value(lowest);
    }
    return format_bpm_value(lowest) + "-" + format_bpm_value(highest);
}

Color difficulty_color(const std::string& difficulty) {
    if (iequals(difficulty, "Beginner") || iequals(difficulty, "Novice")) {
        return kBeginnerColor;
    }
    if (iequals(difficulty, "Easy")) {
        return kEasyColor;
    }
    if (iequals(difficulty, "Medium")) {
        return kMediumColor;
    }
    if (iequals(difficulty, "Hard")) {
        return kHardColor;
    }
    if (iequals(difficulty, "Challenge")) {
        return kChallengeColor;
    }
    return kTextColor;
}

std::string difficulty_row_text(const Chart& chart, bool selected, const std::string& best,
                                float max_row_width, float pixel) {
    const std::string prefix = selected ? "> " : "  ";
    const std::string suffix = "  [" + std::to_string(chart.meter) + "]   " + best;

    // Signed arithmetic: a narrow window must clamp, not underflow.
    const float cell_px = 6.0f * pixel;
    const float cells_f = cell_px > 0.0f ? std::floor(max_row_width / cell_px) : 0.0f;
    const int row_cells =
        cells_f > 0.0f ? static_cast<int>(std::min(cells_f, 100000.0f)) : 0;
    const int fixed = static_cast<int>(text_width(prefix, 1.0f) / 6.0f) +
                      static_cast<int>(text_width(suffix, 1.0f) / 6.0f);
    const int name_budget =
        std::max(static_cast<int>(kMinDifficultyLabelCells), row_cells - fixed);

    return prefix + chart_display_label(chart, static_cast<std::size_t>(name_budget)) + suffix;
}

const ScoreRecord* best_score_for(const ScreenContext& ctx, const Song& song, const Chart& chart) {
    if (ctx.scores == nullptr) {
        return nullptr;
    }
    return find_high_score(*ctx.scores, make_chart_key(song, chart));
}

void SelectScreen::rebuild(const ScreenContext& ctx) {
    // Remember the highlighted song (by its stable simfile path) so returning
    // from gameplay does not lose the player's wheel position. First entry has
    // no prior selection and defaults to index 0.
    std::string previous_key;
    int previous_chart = selected_chart_;
    if (selected_song_ >= 0 && selected_song_ < static_cast<int>(songs_.size())) {
        const Song* previous = songs_[static_cast<std::size_t>(selected_song_)].song;
        if (previous != nullptr) {
            previous_key = previous->simfile_path;
        }
    }

    songs_.clear();
    pack_names_.clear();
    selected_song_ = 0;
    selected_chart_ = 0;

    if (ctx.library == nullptr) {
        return;
    }

    const std::vector<SongPack>& packs = ctx.library->packs();
    for (std::size_t pack_index = 0; pack_index < packs.size(); ++pack_index) {
        pack_names_.push_back(packs[pack_index].name);
        for (const Song& song : packs[pack_index].songs) {
            if (!iequals(song.metadata.selectable, "YES")) {
                continue; // SELECTABLE:NO songs are hidden from the wheel.
            }
            songs_.push_back(WheelEntry{&song, static_cast<int>(pack_index)});
        }
    }

    if (previous_key.empty()) {
        return;
    }
    for (std::size_t i = 0; i < songs_.size(); ++i) {
        const Song* song = songs_[i].song;
        if (song != nullptr && song->simfile_path == previous_key) {
            selected_song_ = static_cast<int>(i);
            if (!song->charts.empty()) {
                selected_chart_ =
                    std::clamp(previous_chart, 0, static_cast<int>(song->charts.size()) - 1);
            }
            break;
        }
    }
}

const Song* SelectScreen::selected_song() const {
    if (songs_.empty()) {
        return nullptr;
    }
    return songs_[static_cast<std::size_t>(selected_song_)].song;
}

const Chart* SelectScreen::selected_chart() const {
    const Song* song = selected_song();
    if (song == nullptr || song->charts.empty()) {
        return nullptr;
    }
    const int index =
        std::clamp(selected_chart_, 0, static_cast<int>(song->charts.size()) - 1);
    return &song->charts[static_cast<std::size_t>(index)];
}

int SelectScreen::chart_count() const {
    const Song* song = selected_song();
    return song == nullptr ? 0 : static_cast<int>(song->charts.size());
}

std::size_t SelectScreen::total_chart_count() const {
    std::size_t total = 0;
    for (const WheelEntry& entry : songs_) {
        if (entry.song != nullptr) {
            total += entry.song->charts.size();
        }
    }
    return total;
}

void SelectScreen::request_preview_for_selected() {
    const Song* song = selected_song();
    if (song == nullptr) {
        preview_.stop();
        return;
    }

    const double start = song->metadata.sample_start > 0.0 ? song->metadata.sample_start
                                                           : kDefaultPreviewStartSeconds;
    const double length = song->metadata.sample_length > 0.0 ? song->metadata.sample_length
                                                             : kDefaultPreviewLengthSeconds;
    preview_.request(song->resolved_music_path, start, length);
}

void SelectScreen::enter(ScreenContext& ctx) {
    rebuild(ctx);
    options_open_ = false;

    // Returning from a screen launched off the options overlay (Calibration,
    // InputRemap): reopen the overlay on the same row instead of dropping to the
    // wheel. Re-seed from config so a freshly calibrated offset is displayed.
    if (reopen_options_on_enter_) {
        reopen_options_on_enter_ = false;
        const int row = options_.row;
        options_ = options_menu_from_config(ctx.config != nullptr ? *ctx.config : GameConfig{});
        options_.row = row;
        options_open_ = true;
    }

    if (ctx.config != nullptr) {
        preview_.set_volume(static_cast<float>(ctx.config->audio.preview_volume));
    }

    std::cout << "[SelectScreen] library: " << songs_.size() << " songs, "
              << total_chart_count() << " charts\n";

    // Returning from InputRemap: the preview kept playing in the background, so
    // leave it running rather than restarting the delay and sample window.
    const Song* song = selected_song();
    if (song != nullptr && preview_.state() != PreviewState::Idle &&
        preview_.requested_path() == song->resolved_music_path) {
        return;
    }
    request_preview_for_selected();
}

void SelectScreen::move_song(int delta) {
    if (songs_.empty()) {
        return;
    }
    const int count = static_cast<int>(songs_.size());
    selected_song_ = ((selected_song_ + delta) % count + count) % count;
    selected_chart_ = 0;
    request_preview_for_selected();
}

void SelectScreen::move_chart(int delta) {
    const int count = chart_count();
    if (count <= 1) {
        return; // single-chart song: Left/Right are a no-op.
    }
    selected_chart_ = std::clamp(selected_chart_ + delta, 0, count - 1);
}

void SelectScreen::apply_navigation(GameAction action) {
    switch (action) {
        case GameAction::Up:
            move_song(-1);
            break;
        case GameAction::Down:
            move_song(+1);
            break;
        case GameAction::Left:
            move_chart(-1);
            break;
        case GameAction::Right:
            move_chart(+1);
            break;
        default:
            break;
    }
}

GameAction SelectScreen::held_direction(const ScreenContext& ctx) const {
    if (!ctx.action_down) {
        return GameAction::None; // headless/tests: no authoritative held state
    }
    // Prefer the direction already repeating so adding a second key does not
    // hijack an in-progress hold.
    if (hold_action_ != GameAction::None && ctx.action_down(hold_action_)) {
        return hold_action_;
    }
    const GameAction candidates[] = {GameAction::Up, GameAction::Down, GameAction::Left,
                                     GameAction::Right};
    for (GameAction action : candidates) {
        if (ctx.action_down(action)) {
            return action;
        }
    }
    return GameAction::None;
}

void SelectScreen::update(ScreenContext& ctx, double fixed_dt,
                          const std::vector<InputEvent>& events) {
    preview_.update(fixed_dt);

    // Set once an event closes the overlay or launches a screen this tick, so a
    // second qualifying press in the same batch cannot fall through to the wheel
    // switch (e.g. an accidental Confirm right after launching the wizard).
    bool options_consumed = false;
    for (const InputEvent& event : events) {
        if (!event.pressed || options_consumed) {
            continue;
        }

        if (options_open_) {
            // Modal: the wheel is suspended and every press is routed to the
            // overlay. Changes are applied to the shared config immediately.
            // Back is intentionally not handled here: Screen::handle_back() owns
            // it so the manager can consult post-update state (see
            // ScreenManager::update).
            // C5 seam: the calibration row is an action, not a value. Confirm or
            // Right opens the wizard; Up/Down still move the highlight and every
            // other action is a no-op (so it cannot be confused with a value row).
            if (options_.row == static_cast<int>(OptionsRow::CalibrateOffset)) {
                if (event.action == GameAction::Up) {
                    options_menu_move_row(options_, -1);
                    continue;
                }
                if (event.action == GameAction::Down) {
                    options_menu_move_row(options_, +1);
                    continue;
                }
                // Options (Tab/shoulder) remains the documented overlay toggle on
                // this row too; it must not be swallowed by the catch-all below.
                if (event.action == GameAction::Options) {
                    if (ctx.config != nullptr) {
                        options_menu_apply(options_, *ctx.config);
                    }
                    options_open_ = false;
                    options_consumed = true;
                    continue;
                }
                if (event.action == GameAction::Confirm || event.action == GameAction::Right) {
                    options_open_ = false;
                    options_consumed = true;
                    if (ctx.manager != nullptr) {
                        reopen_options_on_enter_ = true;
                        ctx.manager->transition_to(ScreenId::Calibration);
                    }
                    continue;
                }
                continue; // Left and unrelated actions: no-op
            }

            // C6 seam: like the calibration row, the remap row is an action, not
            // a value. Confirm or Right opens the remapping screen.
            if (options_.row == static_cast<int>(OptionsRow::RemapInput)) {
                if (event.action == GameAction::Up) {
                    options_menu_move_row(options_, -1);
                    continue;
                }
                if (event.action == GameAction::Down) {
                    options_menu_move_row(options_, +1);
                    continue;
                }
                if (event.action == GameAction::Options) {
                    if (ctx.config != nullptr) {
                        options_menu_apply(options_, *ctx.config);
                    }
                    options_open_ = false;
                    options_consumed = true;
                    continue;
                }
                if (event.action == GameAction::Confirm || event.action == GameAction::Right) {
                    options_open_ = false;
                    options_consumed = true;
                    if (ctx.manager != nullptr) {
                        keep_preview_on_exit_ = true;
                        reopen_options_on_enter_ = true;
                        ctx.manager->transition_to(ScreenId::InputRemap);
                    }
                    continue;
                }
                continue; // Left and unrelated actions: no-op
            }

            bool changed = false;
            bool close = false;
            switch (event.action) {
                case GameAction::Options:
                    close = true;
                    break;
                case GameAction::Up:
                    options_menu_move_row(options_, -1);
                    break;
                case GameAction::Down:
                    options_menu_move_row(options_, +1);
                    break;
                case GameAction::Left:
                    options_menu_adjust(options_, -1);
                    changed = true;
                    break;
                case GameAction::Right:
                case GameAction::Confirm:
                    options_menu_adjust(options_, +1);
                    changed = true;
                    break;
                default:
                    break;
            }
            if ((changed || close) && ctx.config != nullptr) {
                options_menu_apply(options_, *ctx.config);
            }
            if (close) {
                options_open_ = false;
                options_consumed = true;
            }
            continue;
        }

        switch (event.action) {
            case GameAction::Options:
                options_ =
                    options_menu_from_config(ctx.config != nullptr ? *ctx.config : GameConfig{});
                options_open_ = true;
                break;
            case GameAction::Up:
                move_song(-1);
                break;
            case GameAction::Down:
                move_song(+1);
                break;
            case GameAction::Left:
                move_chart(-1);
                break;
            case GameAction::Right:
                move_chart(+1);
                break;
            case GameAction::Confirm: {
                const Song* song = selected_song();
                const Chart* chart = selected_chart();
                if (ctx.manager == nullptr || ctx.play_request == nullptr || song == nullptr ||
                    chart == nullptr) {
                    break;
                }
                ctx.play_request->song = song;
                ctx.play_request->chart = chart;
                ctx.play_request->options = ctx.config != nullptr
                                                ? gameplay_options_from_config(*ctx.config)
                                                : GameplayOptions{};
                ctx.manager->transition_to(ScreenId::Gameplay);
                break;
            }
            default:
                break;
        }
    }

    // Held-direction repeat: the press above moved once; if the direction stays
    // down, accelerate through the wheel. Skipped while the options overlay is up
    // (its rows are stepped one press at a time) and when no authoritative
    // held-state source is wired (headless/unit tests).
    if (options_open_ || !ctx.action_down) {
        hold_action_ = GameAction::None;
        hold_elapsed_ = 0.0;
        repeat_interval_ = 0.0;
        hold_repeating_ = false;
        return;
    }

    const GameAction held = held_direction(ctx);
    if (held == GameAction::None) {
        hold_action_ = GameAction::None;
        hold_elapsed_ = 0.0;
        repeat_interval_ = 0.0;
        hold_repeating_ = false;
        return;
    }
    if (held != hold_action_) {
        hold_action_ = held;
        hold_elapsed_ = 0.0;
        repeat_interval_ = kRepeatInitialIntervalSeconds;
        hold_repeating_ = false;
        return;
    }

    hold_elapsed_ += fixed_dt;
    if (!hold_repeating_) {
        if (hold_elapsed_ >= kRepeatDelaySeconds) {
            hold_repeating_ = true;
            hold_elapsed_ = 0.0;
            apply_navigation(held);
        }
        return;
    }
    if (hold_elapsed_ >= repeat_interval_) {
        hold_elapsed_ = 0.0;
        apply_navigation(held);
        repeat_interval_ =
            std::max(kRepeatMinIntervalSeconds, repeat_interval_ * kRepeatAccelFactor);
    }
}

bool SelectScreen::handle_back(ScreenContext& ctx) {
    if (!options_open_) {
        return false; // fall through to Select -> Title
    }
    if (ctx.config != nullptr) {
        options_menu_apply(options_, *ctx.config);
    }
    options_open_ = false;
    return true; // consumed: do not navigate away from Select
}

void SelectScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);

    if (options_open_) {
        const Color kDimOverlay{0.0f, 0.0f, 0.0f, 0.72f};
        const Color kPanelColor{0.10f, 0.13f, 0.20f, 0.98f};
        renderer.draw_quad(Rect{0.0f, 0.0f, width, height}, kDimOverlay);

        const float panel_w = width * 0.74f;
        const float panel_h = height * 0.76f;
        const float panel_x = (width - panel_w) * 0.5f;
        const float panel_y = (height - panel_h) * 0.5f;
        renderer.draw_quad(Rect{panel_x, panel_y, panel_w, panel_h}, kPanelColor);

        const float text_x = panel_x + panel_w * 0.06f;
        float row_y = panel_y + panel_h * 0.19f;
        // Rows share the band between the title and the hint line, so adding a
        // row shrinks the spacing instead of overlapping the hint.
        const float row_h = panel_h * 0.69f / static_cast<float>(kOptionsRowCount);
        const float name_pixel = std::max(2.0f, width * 0.0035f);
        const float value_pixel = std::max(2.0f, width * 0.0032f);

        draw_text(renderer, "OPTIONS", text_x, panel_y + panel_h * 0.05f,
                  std::max(2.5f, width * 0.0045f), kTitleColor);

        // Value column starts after the widest row name, so long names never
        // run into their values.
        float name_w = 0.0f;
        for (int i = 0; i < kOptionsRowCount; ++i) {
            name_w = std::max(name_w, text_width(options_row_name(i), name_pixel));
        }
        const float value_x = text_x + name_w + name_pixel * 12.0f;
        for (int i = 0; i < kOptionsRowCount; ++i) {
            const bool selected = i == options_.row;
            if (selected) {
                renderer.draw_quad(
                    Rect{text_x - 8.0f, row_y - 4.0f, panel_w * 0.88f + 16.0f, row_h * 0.9f},
                    kPlaceholderColor);
            }
            draw_text(renderer, options_row_name(i), text_x, row_y, name_pixel,
                      selected ? kSelectedColor : kTextColor);
            draw_text(renderer, options_row_value_text(options_, i), value_x, row_y, value_pixel,
                      selected ? kSelectedColor : kTextColor);
            row_y += row_h;
        }

        draw_text_centered(
            renderer,
            "[UP/DOWN] ROW  [LEFT/RIGHT] CHANGE  [ENTER] NEXT  [BACK] CLOSE",
            width * 0.5f, panel_y + panel_h * 0.93f, 2.0f, kHintColor);
        return;
    }

    const float title_pixel = std::max(2.0f, width * 0.0035f);
    draw_text(renderer, "SONG SELECT", width * 0.04f, height * 0.04f, title_pixel, kTitleColor);

    if (songs_.empty()) {
        draw_text_centered(renderer, "NO SONGS FOUND", width * 0.5f, height * 0.45f, 3.0f,
                           kHintColor);
        draw_text_centered(renderer, "[BACK] TO TITLE", width * 0.5f, height * 0.82f, 2.5f,
                           kHintColor);
        return;
    }

    const Song* song = selected_song();
    const WheelEntry& entry = songs_[static_cast<std::size_t>(selected_song_)];

    const std::string& pack_name =
        (entry.pack_index >= 0 && entry.pack_index < static_cast<int>(pack_names_.size()))
            ? pack_names_[static_cast<std::size_t>(entry.pack_index)]
            : std::string{};
    draw_text(renderer, pack_name, width * 0.04f, height * 0.11f, 2.5f, kPackColor);

    // Banner (placeholder quad when the art is missing/undecodable/headless).
    const Rect banner_rect{width * 0.04f, height * 0.16f, width * 0.42f, width * 0.42f * 0.28f};
    const Texture* banner =
        song != nullptr ? texture_cache_.get(song->resolved_banner_path) : nullptr;
    if (banner != nullptr && banner->valid()) {
        renderer.draw_textured_quad(banner_rect, *banner, UVRect{}, Color{1.0f, 1.0f, 1.0f, 1.0f});
    } else {
        renderer.draw_quad(banner_rect, kPlaceholderColor);
    }

    if (song != nullptr) {
        const float info_x = width * 0.04f;
        float info_y = banner_rect.y + banner_rect.h + height * 0.03f;
        draw_text(renderer, song_display_title(song->metadata), info_x, info_y, 3.0f,
                  kSelectedColor);
        info_y += height * 0.045f;
        draw_text(renderer, song_display_artist(song->metadata), info_x, info_y, 2.5f, kTextColor);
        info_y += height * 0.04f;
        draw_text(renderer, "BPM " + format_bpm_range(song->timing), info_x, info_y, 2.5f,
                  kDimColor);
    }

    // Fixed-size windowed wheel on the right, with a pack-change separator. The
    // window is 13 rows tall; the highlight stays centered whenever the window
    // can slide, and the window fills the full 13 rows at either end of the list.
    const int count = static_cast<int>(songs_.size());
    constexpr int kHalfRows = 6;
    const int window_rows = 2 * kHalfRows + 1;
    int first = selected_song_ - kHalfRows;
    first = std::clamp(first, 0, std::max(0, count - window_rows));
    const int last = std::min(count - 1, first + window_rows - 1);
    const float row_h = height * 0.045f;
    const float list_x = width * 0.52f;
    float row_y = height * 0.18f;
    for (int i = first; i <= last; ++i) {
        const WheelEntry& item = songs_[static_cast<std::size_t>(i)];
        const bool selected = i == selected_song_;
        if (i == first && item.pack_index >= 0 &&
            item.pack_index < static_cast<int>(pack_names_.size())) {
            draw_text(renderer, pack_names_[static_cast<std::size_t>(item.pack_index)], list_x,
                      row_y - row_h * 0.9f, 2.0f, kPackColor);
        }
        if (selected) {
            renderer.draw_quad(Rect{list_x - 6.0f, row_y - 4.0f, width * 0.44f, row_h},
                               kPlaceholderColor);
        }
        if (item.song != nullptr) {
            draw_text(renderer, song_display_title(item.song->metadata), list_x, row_y, 2.5f,
                      selected ? kSelectedColor : kTextColor);
        }
        row_y += row_h;
    }

    // Difficulty rows for the highlighted song: passthrough label (an Edit
    // chart shows its name, shortened to fit the highlight bar) + meter + best
    // percent, tinted per difficulty (Edit keeps the neutral tint). The tint carries the difficulty, so the
    // selected row is marked by a highlight bar and a cursor instead of a color.
    const std::size_t chart_total = song == nullptr ? 0 : song->charts.size();
    const float diff_x = width * 0.04f;
    const float diff_row_h = height * 0.042f;
    float diff_y = height * 0.62f;
    draw_text(renderer, "DIFFICULTY", diff_x, diff_y, 2.5f, kDimColor);
    diff_y += height * 0.05f;
    for (std::size_t i = 0; i < chart_total; ++i) {
        const Chart& chart = song->charts[i];
        const bool selected = static_cast<int>(i) == selected_chart_;

        std::string best = "---";
        if (const ScoreRecord* record = best_score_for(ctx, *song, chart); record != nullptr) {
            best = format_percent(record->percent);
        }

        if (selected) {
            renderer.draw_quad(Rect{diff_x - 6.0f, diff_y - 4.0f, width * 0.42f, diff_row_h},
                               kPlaceholderColor);
        }
        const std::string row = difficulty_row_text(chart, selected, best, width * 0.42f, 2.5f);
        draw_text(renderer, row, diff_x, diff_y, 2.5f, difficulty_color(chart.difficulty));
        diff_y += diff_row_h;
    }

    draw_text_centered(
        renderer,
        "[UP/DOWN] SONG   [LEFT/RIGHT] DIFFICULTY   [ENTER] PLAY   [TAB] OPTIONS   [BACK] TITLE",
        width * 0.5f, height * 0.93f, 2.0f, kHintColor);
}

void SelectScreen::exit(ScreenContext& /*ctx*/) {
    if (!keep_preview_on_exit_) {
        preview_.stop();
    }
    keep_preview_on_exit_ = false;
    texture_cache_.clear();
    options_open_ = false;
}

void SelectScreen::update_inactive(double fixed_dt) {
    preview_.update(fixed_dt); // no-op once stopped (Idle)
}

} // namespace blaze4k
