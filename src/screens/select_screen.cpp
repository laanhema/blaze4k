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
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/ttf_font.hpp"
#include "screens/options_art.hpp"
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

void SelectScreen::rebuild_rows() {
    std::vector<int> song_packs;
    song_packs.reserve(songs_.size());
    for (const WheelEntry& entry : songs_) {
        song_packs.push_back(entry.pack_index);
    }
    select_art::WheelRows rows = select_art::build_wheel_rows(song_packs, open_pack_);
    wheel_rows_ = std::move(rows.rows);
    song_row_ = std::move(rows.song_row);
}

int SelectScreen::header_row_of(int pack_index) const {
    for (std::size_t r = 0; r < wheel_rows_.size(); ++r) {
        if (wheel_rows_[r].kind == select_art::WheelRow::Kind::Pack &&
            wheel_rows_[r].pack_index == pack_index) {
            return static_cast<int>(r);
        }
    }
    return -1;
}

void SelectScreen::update_persisted() {
    if (open_pack_ >= 0 && open_pack_ < static_cast<int>(pack_names_.size())) {
        open_pack_name_ = pack_names_[static_cast<std::size_t>(open_pack_)];
    } else {
        open_pack_name_.clear();
    }
    cursor_pack_name_.clear();
    cursor_song_key_.clear();
    if (on_pack_header()) {
        cursor_pack_name_ = selected_pack_name();
    } else if (const Song* song = selected_song(); song != nullptr) {
        cursor_song_key_ = song->simfile_path;
    }
}

void SelectScreen::rebuild(const ScreenContext& ctx) {
    // Re-find the remembered cursor target (a song by its stable simfile path, or
    // a pack header by name) and open pack in the rescanned library, so returning
    // from gameplay does not lose the player's wheel position. First entry has
    // nothing remembered: every pack starts closed with the cursor on the first
    // header, as StepMania's MusicWheel::BeginScreen does with no previous song
    // (SetOpenSection("")).
    const std::string previous_song = cursor_song_key_;
    const std::string previous_header = cursor_pack_name_;
    const std::string previous_open = open_pack_name_;
    const int previous_chart = selected_chart_;

    songs_.clear();
    pack_names_.clear();
    pack_banners_.clear();
    fallback_banner_.clear();
    pack_labels_.clear();
    wheel_rows_.clear();
    song_row_.clear();
    cursor_row_ = 0;
    open_pack_ = -1;
    selected_chart_ = 0;

    if (ctx.library == nullptr) {
        return;
    }

    fallback_banner_ = ctx.library->fallback_banner().string();
    const std::vector<SongPack>& packs = ctx.library->packs();
    for (std::size_t pack_index = 0; pack_index < packs.size(); ++pack_index) {
        pack_names_.push_back(packs[pack_index].name);
        pack_banners_.push_back(packs[pack_index].banner_path);
        pack_labels_.push_back(select_art::ascii_upper(packs[pack_index].name));
        for (const Song& song : packs[pack_index].songs) {
            if (!iequals(song.metadata.selectable, "YES")) {
                continue; // SELECTABLE:NO songs are hidden from the wheel.
            }
            songs_.push_back(WheelEntry{&song, static_cast<int>(pack_index)});
        }
    }
    if (songs_.empty()) {
        return;
    }

    auto pack_with_songs = [&](const std::string& name) {
        if (name.empty()) {
            return -1;
        }
        for (const WheelEntry& entry : songs_) {
            if (pack_names_[static_cast<std::size_t>(entry.pack_index)] == name) {
                return entry.pack_index;
            }
        }
        return -1;
    };

    if (!previous_song.empty()) {
        for (std::size_t i = 0; i < songs_.size(); ++i) {
            const Song* song = songs_[i].song;
            if (song == nullptr || song->simfile_path != previous_song) {
                continue;
            }
            open_pack_ = songs_[i].pack_index;
            rebuild_rows();
            cursor_row_ = std::max(0, song_row_[i]);
            if (!song->charts.empty()) {
                selected_chart_ =
                    std::clamp(previous_chart, 0, static_cast<int>(song->charts.size()) - 1);
            }
            update_persisted();
            return;
        }
    }

    // (b) The remembered cursor header still exists: keep it, and the open pack.
    const int header_pack = pack_with_songs(previous_header);
    if (header_pack >= 0) {
        open_pack_ = pack_with_songs(previous_open);
        rebuild_rows();
        cursor_row_ = std::max(0, header_row_of(header_pack));
        update_persisted();
        return;
    }

    open_pack_ = -1;
    rebuild_rows();
    cursor_row_ = 0;
    update_persisted();
}

int SelectScreen::selected_wheel_row() const {
    if (wheel_rows_.empty()) {
        return 0;
    }
    return std::clamp(cursor_row_, 0, static_cast<int>(wheel_rows_.size()) - 1);
}

bool SelectScreen::on_pack_header() const {
    return !wheel_rows_.empty() &&
           wheel_rows_[static_cast<std::size_t>(selected_wheel_row())].kind ==
               select_art::WheelRow::Kind::Pack;
}

int SelectScreen::selected_song_index() const {
    if (wheel_rows_.empty()) {
        return -1;
    }
    const select_art::WheelRow& row = wheel_rows_[static_cast<std::size_t>(selected_wheel_row())];
    return row.kind == select_art::WheelRow::Kind::Song ? row.song_index : -1;
}

select_art::ConfirmHint SelectScreen::confirm_hint() const {
    if (!on_pack_header()) {
        return select_art::ConfirmHint::Play;
    }
    const int pack = wheel_rows_[static_cast<std::size_t>(selected_wheel_row())].pack_index;
    return pack == open_pack_ ? select_art::ConfirmHint::Close : select_art::ConfirmHint::Open;
}

const std::string& SelectScreen::cursor_banner_path() const {
    static const std::string kNone;
    if (const Song* song = selected_song(); song != nullptr) {
        return song->resolved_banner_path;
    }
    if (!on_pack_header()) {
        return kNone;
    }
    const int pack = wheel_rows_[static_cast<std::size_t>(selected_wheel_row())].pack_index;
    if (pack >= 0 && pack < static_cast<int>(pack_banners_.size()) &&
        !pack_banners_[static_cast<std::size_t>(pack)].empty()) {
        return pack_banners_[static_cast<std::size_t>(pack)];
    }
    return fallback_banner_;
}

std::string SelectScreen::selected_pack_name() const {
    if (!on_pack_header()) {
        return {};
    }
    const int pack = wheel_rows_[static_cast<std::size_t>(selected_wheel_row())].pack_index;
    if (pack < 0 || pack >= static_cast<int>(pack_names_.size())) {
        return {};
    }
    return pack_names_[static_cast<std::size_t>(pack)];
}

const Song* SelectScreen::selected_song() const {
    const int index = selected_song_index();
    if (index < 0 || index >= static_cast<int>(songs_.size())) {
        return nullptr;
    }
    return songs_[static_cast<std::size_t>(index)].song;
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

void SelectScreen::refresh_options_values() {
    for (int i = 0; i < kOptionsRowCount; ++i) {
        options_values_[static_cast<std::size_t>(i)] = options_row_value_text(options_, i);
    }
    options_values_dirty_ = false;
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
        refresh_options_values();
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

void SelectScreen::move_row(int delta) {
    if (wheel_rows_.empty()) {
        return;
    }
    const int count = static_cast<int>(wheel_rows_.size());
    const int old_first =
        select_art::list_window(selected_wheel_row(), count, select_art::kWheelVisibleRows).first;
    cursor_row_ = ((selected_wheel_row() + delta) % count + count) % count;
    selected_chart_ = 0;

    const int new_first =
        select_art::list_window(selected_wheel_row(), count, select_art::kWheelVisibleRows).first;
    if (new_first != old_first) {
        scroll_start_ = select_art::wheel_scroll_start(wheel_scroll_offset(), new_first - old_first);
        scroll_elapsed_ = 0.0;
    }
    update_persisted();
    request_preview_for_selected();
}

void SelectScreen::toggle_pack() {
    if (!on_pack_header()) {
        return;
    }
    const int pack = wheel_rows_[static_cast<std::size_t>(selected_wheel_row())].pack_index;
    open_pack_ = open_pack_ == pack ? -1 : pack;
    rebuild_rows();
    cursor_row_ = std::max(0, header_row_of(pack));
    scroll_start_ = 0.0f;
    scroll_elapsed_ = 0.0;
    selected_chart_ = 0;
    update_persisted();
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
            move_row(-1);
            break;
        case GameAction::Down:
            move_row(+1);
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
        return GameAction::None;
    }
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
    scroll_elapsed_ += fixed_dt;

    bool options_consumed = false;
    for (const InputEvent& event : events) {
        if (!event.pressed || options_consumed) {
            continue;
        }

        if (options_open_) {
            options_values_dirty_ = true;
            if (options_.row == static_cast<int>(OptionsRow::CalibrateOffset)) {
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
                        reopen_options_on_enter_ = true;
                        ctx.manager->transition_to(ScreenId::Calibration);
                    }
                    continue;
                }
                continue;
            }

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
                options_values_dirty_ = true;
                break;
            case GameAction::Up:
                move_row(-1);
                break;
            case GameAction::Down:
                move_row(+1);
                break;
            case GameAction::Left:
                move_chart(-1);
                break;
            case GameAction::Right:
                move_chart(+1);
                break;
            case GameAction::Confirm: {
                if (on_pack_header()) {
                    toggle_pack(); // opening/closing a pack never leaves Select
                    break;
                }
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

    if (options_open_ && options_values_dirty_) {
        refresh_options_values();
    }
    refresh_chips(ctx);

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

    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;

    if (theme != nullptr) {
        select_art::draw_backdrop(*theme, renderer, w, h);
    }

    if (!songs_.empty()) {
        const Song* song = selected_song();

        const Texture* banner = texture_cache_.get(cursor_banner_path());
        select_art::draw_banner(theme, renderer, L, banner);
        if (song != nullptr && text != nullptr) {
            const std::string& title =
                song_display_title(song->metadata, text, theme::text::kSongTitle.font);
            const std::string& subtitle =
                song_display_subtitle(song->metadata, text, theme::text::kSongSubtitle.font);
            const std::string& artist =
                song_display_artist(song->metadata, text, theme::text::kArtist.font);
            select_art::draw_song_info(*text, renderer, L, title, subtitle, artist,
                                       "BPM " + format_bpm_range(song->timing));
        }

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
                view.art = r == selected_row ? select_art::WheelArt::SelectedPack
                                             : select_art::WheelArt::Pack;
                view.open = row.pack_index == open_pack_;
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
                view.subtitle = song_display_subtitle(
                    row_song->metadata, text,
                    selected ? theme::text::kWheelSelectedSubtitle.font
                             : theme::text::kWheelSubtitle.font);
            }
        }
        select_art::draw_wheel(theme, text, renderer, L, w, std::span(views.data(), n),
                               selected_row - window.first, offset);
    }

    if (theme != nullptr) {
        select_art::draw_top_bar(*theme, renderer, L, w);
    }
    select_art::draw_chips(theme, text, renderer, L, chip_speed_text_, chip_scroll_text_);
    select_art::draw_hint_bar(theme, text, renderer, L, w, confirm_hint());
    if (songs_.empty() && text != nullptr) {
        select_art::draw_empty_message(*text, renderer, L);
    }
    if (options_open_) {
        options_art::draw_overlay(theme, text, renderer, L, w, h, options_values_, options_.row);
    }
    if (theme != nullptr) {
        select_art::draw_scanlines(*theme, renderer, w, h, L);
    }
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
