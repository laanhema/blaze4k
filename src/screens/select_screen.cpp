#include "screens/select_screen.hpp"

#include <algorithm>
#include <array>
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
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/ttf_font.hpp"
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

// Options overlay colours (bitmap font; the overlay's Cabinet restyle is #96).
constexpr Color kTitleColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kSelectedColor{1.00f, 0.92f, 0.35f, 1.0f};
constexpr Color kTextColor{0.82f, 0.87f, 0.95f, 1.0f};
constexpr Color kHintColor{0.60f, 0.66f, 0.78f, 1.0f};
constexpr Color kPlaceholderColor{0.16f, 0.20f, 0.30f, 1.0f};

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
    pack_labels_.clear();
    wheel_rows_.clear();
    song_row_index_.clear();
    selected_song_ = 0;
    selected_chart_ = 0;

    if (ctx.library == nullptr) {
        return;
    }

    const std::vector<SongPack>& packs = ctx.library->packs();
    for (std::size_t pack_index = 0; pack_index < packs.size(); ++pack_index) {
        pack_labels_.push_back(select_art::ascii_upper(packs[pack_index].name));
        for (const Song& song : packs[pack_index].songs) {
            if (!iequals(song.metadata.selectable, "YES")) {
                continue; // SELECTABLE:NO songs are hidden from the wheel.
            }
            songs_.push_back(WheelEntry{&song, static_cast<int>(pack_index)});
        }
    }

    // Wheel display rows: an inline pack header before each pack's songs.
    std::vector<int> song_packs;
    song_packs.reserve(songs_.size());
    for (const WheelEntry& entry : songs_) {
        song_packs.push_back(entry.pack_index);
    }
    select_art::WheelRows rows = select_art::build_wheel_rows(song_packs);
    wheel_rows_ = std::move(rows.rows);
    song_row_index_ = std::move(rows.song_row);

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

int SelectScreen::selected_wheel_row() const {
    if (selected_song_ < 0 || selected_song_ >= static_cast<int>(song_row_index_.size())) {
        return 0;
    }
    return song_row_index_[static_cast<std::size_t>(selected_song_)];
}

int SelectScreen::wheel_first_row() const {
    return select_art::list_window(selected_wheel_row(), static_cast<int>(wheel_rows_.size()),
                                   select_art::kWheelVisibleRows)
        .first;
}

float SelectScreen::wheel_scroll_offset() const {
    return select_art::wheel_scroll_offset(scroll_start_, scroll_elapsed_);
}

void SelectScreen::refresh_chips(const ScreenContext& ctx) {
    static const std::string kNone;
    const std::string& speed = ctx.config != nullptr ? ctx.config->gameplay.speed_mod : kNone;
    const std::string& scroll = ctx.config != nullptr ? ctx.config->gameplay.scroll : kNone;
    if (chips_valid_ && speed == chip_src_speed_ && scroll == chip_src_scroll_) {
        return;
    }
    chip_src_speed_ = speed;
    chip_src_scroll_ = scroll;
    chip_speed_text_ = select_art::speed_chip_text(ctx.config);
    chip_scroll_text_ = select_art::scroll_chip_text(ctx.config);
    chips_valid_ = true;
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
    scroll_start_ = 0.0f;
    scroll_elapsed_ = 0.0;
    chips_valid_ = false;
    refresh_chips(ctx);

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
    const int rows = static_cast<int>(wheel_rows_.size());
    const int old_first =
        select_art::list_window(selected_wheel_row(), rows, select_art::kWheelVisibleRows).first;
    selected_song_ = ((selected_song_ + delta) % count + count) % count;
    selected_chart_ = 0;

    // Slide the wheel only when its window moved (select_art::wheel_scroll_start).
    const int new_first =
        select_art::list_window(selected_wheel_row(), rows, select_art::kWheelVisibleRows).first;
    if (new_first != old_first) {
        scroll_start_ = select_art::wheel_scroll_start(wheel_scroll_offset(), new_first - old_first);
        scroll_elapsed_ = 0.0;
    }
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
    scroll_elapsed_ += fixed_dt; // wheel slide: fixed dt, never the music clock

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

    // The overlay may have changed the speed/scroll config this tick.
    refresh_chips(ctx);

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
    refresh_chips(ctx);
    return true; // consumed: do not navigate away from Select
}

void SelectScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    // Cabinet v3 select (#94), back to front. Every theme/text call is
    // null-guarded (headless and unit tests run without them).
    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;

    if (theme != nullptr) {
        select_art::draw_backdrop(*theme, renderer, w, h);
    }

    if (!songs_.empty()) {
        const Song* song = selected_song();

        // Banner (or banner_fallback) inside banner_frame, then the song info.
        const Texture* banner =
            song != nullptr ? texture_cache_.get(song->resolved_banner_path) : nullptr;
        select_art::draw_banner(theme, renderer, L, banner);
        if (song != nullptr && text != nullptr) {
            const std::string& title =
                song_display_title(song->metadata, text, theme::text::kSongTitle.font);
            const std::string& artist =
                song_display_artist(song->metadata, text, theme::text::kArtist.font);
            select_art::draw_song_info(*text, renderer, L, title, artist,
                                       "BPM " + format_bpm_range(song->timing));
        }

        // Difficulty rows: a window of at most kDiffVisibleRows around the chart cursor.
        if (song != nullptr && !song->charts.empty()) {
            const int count = static_cast<int>(song->charts.size());
            const int selected = std::clamp(selected_chart_, 0, count - 1);
            const select_art::ListWindow window =
                select_art::list_window(selected, count, select_art::kDiffVisibleRows);
            std::array<select_art::DifficultyRowView, select_art::kDiffVisibleRows> views{};
            std::size_t n = 0;
            for (int i = window.first; i <= window.last && n < views.size(); ++i, ++n) {
                const Chart& chart = song->charts[static_cast<std::size_t>(i)];
                views[n].chart = &chart;
                const ScoreRecord* record = best_score_for(ctx, *song, chart);
                views[n].best = record != nullptr ? format_percent(record->percent) : "---";
            }
            select_art::draw_difficulty_rows(theme, text, renderer, L,
                                             std::span(views.data(), n), selected - window.first);
        }

        // Wheel: a kWheelVisibleRows window over the display rows (pack headers
        // inline). While sliding, up to two extra rows fill the side the rows
        // moved away from; draw_wheel skips any row that would leave reference
        // y 0..720 (the letterbox bands), and the bars drawn next cover the rest.
        const int rows = static_cast<int>(wheel_rows_.size());
        const int selected_row = selected_wheel_row();
        const select_art::ListWindow window =
            select_art::list_window(selected_row, rows, select_art::kWheelVisibleRows);
        const float offset = wheel_scroll_offset();
        const select_art::ListWindow range = select_art::wheel_slide_range(window, rows, offset);
        std::array<select_art::WheelRowView,
                   select_art::kWheelVisibleRows + select_art::kWheelMaxSlideRows>
            views{};
        std::size_t n = 0;
        for (int r = range.first; r <= range.last && n < views.size(); ++r, ++n) {
            const select_art::WheelRow& row = wheel_rows_[static_cast<std::size_t>(r)];
            select_art::WheelRowView& view = views[n];
            view.slot = r - window.first;
            if (row.kind == select_art::WheelRow::Kind::Pack) {
                view.art = select_art::WheelArt::Pack;
                if (row.pack_index >= 0 && row.pack_index < static_cast<int>(pack_labels_.size())) {
                    view.label = pack_labels_[static_cast<std::size_t>(row.pack_index)];
                }
                continue;
            }
            const bool selected = r == selected_row;
            view.art = selected ? select_art::WheelArt::Selected : select_art::WheelArt::Song;
            const Song* row_song =
                (row.song_index >= 0 && row.song_index < static_cast<int>(songs_.size()))
                    ? songs_[static_cast<std::size_t>(row.song_index)].song
                    : nullptr;
            if (row_song != nullptr) {
                view.label = song_display_title(
                    row_song->metadata, text,
                    selected ? theme::text::kWheelSelected.font : theme::text::kWheelRow.font);
            }
        }
        select_art::draw_wheel(theme, text, renderer, L, w, std::span(views.data(), n),
                               selected_row - window.first, offset);
    }

    // Chrome over the wheel: top bar + chips, hint bar.
    if (theme != nullptr) {
        select_art::draw_top_bar(*theme, renderer, L, w);
    }
    select_art::draw_chips(theme, text, renderer, L, chip_speed_text_, chip_scroll_text_);
    select_art::draw_hint_bar(theme, text, renderer, L, w);
    if (songs_.empty() && text != nullptr) {
        select_art::draw_empty_message(*text, renderer, L);
    }
    if (theme != nullptr) {
        select_art::draw_scanlines(*theme, renderer, w, h, L);
    }

    if (!options_open_) {
        return;
    }
    // Options overlay over the Cabinet screen (bitmap font until #96).
    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);
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
