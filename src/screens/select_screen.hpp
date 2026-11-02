#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "audio/preview_player.hpp"
#include "chart/timing_data.hpp"
#include "render/geometry.hpp"
#include "render/texture_cache.hpp"
#include "screens/options_menu.hpp"
#include "screens/screen.hpp"
#include "screens/select_art.hpp"

namespace blaze4k {
struct Chart;
struct Song;
struct ScoreRecord;

// Pure BPM display: "140" or "128-175".
[[nodiscard]] std::string format_bpm_range(const TimingData& timing);

[[nodiscard]] const ScoreRecord* best_score_for(const ScreenContext& ctx, const Song& song,
                                                const Chart& chart);

// Cabinet v3 Song Select screen (#94, #96).
class SelectScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
    void exit(ScreenContext& ctx) override;
    void update_inactive(double fixed_dt) override;
    bool handle_back(ScreenContext& ctx) override;
    [[nodiscard]] bool back_consumed() const override { return options_open_; }

    [[nodiscard]] std::size_t song_count() const { return songs_.size(); }
    [[nodiscard]] int selected_song_index() const;
    [[nodiscard]] int selected_chart_index() const { return selected_chart_; }
    [[nodiscard]] const Song* selected_song() const;
    [[nodiscard]] const Chart* selected_chart() const;
    [[nodiscard]] int chart_count() const;
    [[nodiscard]] std::size_t total_chart_count() const;
    [[nodiscard]] const PreviewPlayer& preview() const { return preview_; }
    [[nodiscard]] bool options_open() const { return options_open_; }
    [[nodiscard]] const OptionsMenu& options_menu() const { return options_; }
    [[nodiscard]] const std::array<std::string, kOptionsRowCount>& options_values() const {
        return options_values_;
    }
    [[nodiscard]] std::size_t wheel_row_count() const { return wheel_rows_.size(); }
    [[nodiscard]] int wheel_first_row() const;
    [[nodiscard]] float wheel_scroll_offset() const;
    [[nodiscard]] int selected_wheel_row() const;
    [[nodiscard]] bool on_pack_header() const;
    [[nodiscard]] const std::string& cursor_banner_path() const;
    [[nodiscard]] std::string selected_pack_name() const;
    [[nodiscard]] select_art::ConfirmHint confirm_hint() const;
    [[nodiscard]] const std::string& open_pack_name() const { return open_pack_name_; }

private:
    struct WheelEntry {
        const Song* song = nullptr;
        int pack_index = 0;
    };

    void rebuild(const ScreenContext& ctx);
    void rebuild_rows();
    void move_row(int delta);
    void toggle_pack();
    void update_persisted();
    [[nodiscard]] int header_row_of(int pack_index) const;
    void move_chart(int delta);
    void request_preview_for_selected();
    void refresh_chips(const ScreenContext& ctx);
    void refresh_options_values();
    void apply_navigation(GameAction action);
    [[nodiscard]] GameAction held_direction(const ScreenContext& ctx) const;

    std::vector<WheelEntry> songs_;
    std::vector<std::string> pack_names_;  // pack names as scanned, by pack index
    std::vector<std::string> pack_banners_; // SongPack::banner_path by pack index ("" = none)
    std::string fallback_banner_;           // SongLibrary::fallback_banner() at the last rebuild
    std::vector<std::string> pack_labels_; // pack names in ASCII upper case (wheel headers)
    std::vector<select_art::WheelRow> wheel_rows_; // visible rows: headers + the open pack's songs
    std::vector<int> song_row_; // song index -> wheel display row, -1 hidden

    // The wheel cursor is a display row. The open pack and the cursor target are
    // remembered by name / simfile path (not index) so a re-entry after Gameplay,
    // Results, Calibration or Input remap, which rescans the rows, restores them.
    int cursor_row_ = 0;
    int open_pack_ = -1; // pack index of the one open pack, -1 = all closed
    std::string open_pack_name_;
    std::string cursor_pack_name_; // set when the cursor is on a pack header
    std::string cursor_song_key_; // set when the cursor is on a song (simfile_path)

    float scroll_start_ = 0.0f;
    double scroll_elapsed_ = 0.0;

    std::string chip_speed_text_;
    std::string chip_scroll_text_;
    std::string chip_src_speed_;
    std::string chip_src_scroll_;
    bool chips_valid_ = false;
    int selected_chart_ = 0;
    PreviewPlayer preview_;
    TextureCache texture_cache_;
    OptionsMenu options_;
    bool options_open_ = false;
    std::array<std::string, kOptionsRowCount> options_values_{};
    bool options_values_dirty_ = true;
    bool keep_preview_on_exit_ = false;
    bool reopen_options_on_enter_ = false;

    GameAction hold_action_ = GameAction::None;
    double hold_elapsed_ = 0.0;
    double repeat_interval_ = 0.0;
    bool hold_repeating_ = false;
};

} // namespace blaze4k
