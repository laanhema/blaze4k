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

// Pure BPM display: "140" when all segments agree, else "128-175" (min-max).
// The empty-segment guard returns "?" but is defensive only: TimingData always
// seeds at least one BPM segment, so it is not reachable from a parsed simfile.
// No remapping of the underlying timing.
[[nodiscard]] std::string format_bpm_range(const TimingData& timing);

// Best stored score for a chart, or nullptr when scores are unavailable/empty.
[[nodiscard]] const ScoreRecord* best_score_for(const ScreenContext& ctx, const Song& song,
                                                const Chart& chart);

// The real Song Select wheel: a pack-grouped list of the scanned library (pack
// header rows the cursor stops on; Confirm on a header opens or closes the pack,
// and one pack is open at a time, StepMania-style) with banner art, artist/BPM, passthrough
// difficulty labels + foot ratings, and the player's best percent per chart,
// drawn in the Cabinet v3 look (#94, select_art); the options overlay is drawn
// by options_art (#96). Highlighting a song arms a
// delayed audio preview; Confirm publishes a `PlayRequest` and transitions to
// Gameplay; Back is handled centrally by the manager (Select -> Title).
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

    // Test accessors (headless, pure).
    [[nodiscard]] std::size_t song_count() const { return songs_.size(); }
    // Song index under the cursor, -1 when the cursor is on a pack header.
    [[nodiscard]] int selected_song_index() const;
    [[nodiscard]] int selected_chart_index() const { return selected_chart_; }
    [[nodiscard]] const Song* selected_song() const;
    [[nodiscard]] const Chart* selected_chart() const;
    [[nodiscard]] int chart_count() const;
    [[nodiscard]] std::size_t total_chart_count() const;
    [[nodiscard]] const PreviewPlayer& preview() const { return preview_; }
    [[nodiscard]] bool options_open() const { return options_open_; }
    [[nodiscard]] const OptionsMenu& options_menu() const { return options_; }
    // The overlay's cached value text (options_row_value_text per row).
    [[nodiscard]] const std::array<std::string, kOptionsRowCount>& options_values() const {
        return options_values_;
    }
    // Wheel display rows (pack headers + songs), the window's first row, and the
    // current slide offset in reference px (0 when the wheel is at rest).
    [[nodiscard]] std::size_t wheel_row_count() const { return wheel_rows_.size(); }
    [[nodiscard]] int wheel_first_row() const;
    [[nodiscard]] float wheel_scroll_offset() const;
    // The cursor's display row (clamped to the rows; 0 when there are none).
    [[nodiscard]] int selected_wheel_row() const;
    // The cursor is on a pack header (false on a song row and with no rows).
    [[nodiscard]] bool on_pack_header() const;
    // Path of the banner the info panel shows: the song's on a song row; on a pack
    // header the pack's banner, else the library fallback banner. Empty when there
    // is none (the theme's banner_fallback is drawn).
    [[nodiscard]] const std::string& cursor_banner_path() const;
    // Name of the pack under the cursor when it is on a header, else empty.
    [[nodiscard]] std::string selected_pack_name() const;
    // What Confirm does on the cursor row (the hint bar's word after ENTER): Play on
    // a song or with no rows, Open / Close on a closed / open pack header.
    [[nodiscard]] select_art::ConfirmHint confirm_hint() const;
    // Name of the open pack, empty when every pack is closed.
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

    // Wheel slide (select_art::wheel_scroll_offset), advanced on the fixed dt.
    float scroll_start_ = 0.0f;
    double scroll_elapsed_ = 0.0;

    // Speed/scroll chip text, rebuilt only when the config strings change.
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
    // The overlay's value text, rebuilt when it may have changed (set dirty on
    // open and on every press the overlay handles), so render() never allocates it.
    std::array<std::string, kOptionsRowCount> options_values_{};
    bool options_values_dirty_ = true;
    // Set when launching InputRemap: exit() then leaves the preview playing (and
    // update_inactive() keeps it looping) so the song continues behind the remap
    // screen. Every other exit stops the preview.
    bool keep_preview_on_exit_ = false;
    // Set when launching Calibration/InputRemap from the options overlay: the
    // next enter() reopens the overlay on the launching row, so Back from those
    // screens lands in Options rather than on the bare wheel.
    bool reopen_options_on_enter_ = false;

    // Held-direction key repeat: once a direction is held past the initial
    // delay, it re-triggers navigation at an accelerating interval. Sampled from
    // ctx.action_down, so it is inert in headless/unit tests (null callback).
    GameAction hold_action_ = GameAction::None;
    double hold_elapsed_ = 0.0;
    double repeat_interval_ = 0.0;
    bool hold_repeating_ = false;
};

} // namespace blaze4k
