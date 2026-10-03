#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "audio/preview_player.hpp"
#include "chart/timing_data.hpp"
#include "render/geometry.hpp"
#include "render/texture_cache.hpp"
#include "screens/options_menu.hpp"
#include "screens/screen.hpp"

namespace blaze4k {
struct Chart;
struct Song;
struct ScoreRecord;

// Pure BPM display: "140" when all segments agree, else "128-175" (min-max).
// The empty-segment guard returns "?" but is defensive only: TimingData always
// seeds at least one BPM segment, so it is not reachable from a parsed simfile.
// No remapping of the underlying timing.
[[nodiscard]] std::string format_bpm_range(const TimingData& timing);

// Pure difficulty tint for the select screen's chart rows, keyed on the
// passthrough simfile label (case-insensitive): Beginner/Novice purple, Easy
// green, Medium yellow, Hard red, Challenge blue. Edit and unknown labels get the
// neutral text color.
[[nodiscard]] Color difficulty_color(const std::string& difficulty);

// Narrowest name budget for a difficulty row, in cells: the width of
// "Challenge", the longest standard label, so an edit name is never shortened
// below what a standard label already takes.
inline constexpr std::size_t kMinDifficultyLabelCells = 9;

// Pure select-screen difficulty row: cursor ("> " or "  "), the display label
// (chart_display_label: an Edit chart's name, else the passthrough label), then
// "  [meter]   " and `best`. The name is shortened so the whole row fits
// `max_row_width` pixels at `pixel` scale, but never below
// kMinDifficultyLabelCells cells.
[[nodiscard]] std::string difficulty_row_text(const Chart& chart, bool selected,
                                              const std::string& best, float max_row_width,
                                              float pixel);

// Best stored score for a chart, or nullptr when scores are unavailable/empty.
[[nodiscard]] const ScoreRecord* best_score_for(const ScreenContext& ctx, const Song& song,
                                                const Chart& chart);

// The real Song Select wheel: a flat, pack-grouped list of the scanned library
// with banner art, artist/BPM, passthrough difficulty labels + foot ratings, and
// the player's best percent per chart. Highlighting a song arms a delayed audio
// preview; Confirm publishes a `PlayRequest` and transitions to Gameplay; Back
// is handled centrally by the manager (Select -> Title).
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
    [[nodiscard]] int selected_song_index() const { return selected_song_; }
    [[nodiscard]] int selected_chart_index() const { return selected_chart_; }
    [[nodiscard]] const Song* selected_song() const;
    [[nodiscard]] const Chart* selected_chart() const;
    [[nodiscard]] int chart_count() const;
    [[nodiscard]] std::size_t total_chart_count() const;
    [[nodiscard]] const PreviewPlayer& preview() const { return preview_; }
    [[nodiscard]] bool options_open() const { return options_open_; }
    [[nodiscard]] const OptionsMenu& options_menu() const { return options_; }

private:
    struct WheelEntry {
        const Song* song = nullptr;
        int pack_index = 0;
    };

    void rebuild(const ScreenContext& ctx);
    void move_song(int delta);
    void move_chart(int delta);
    void request_preview_for_selected();
    void apply_navigation(GameAction action);
    [[nodiscard]] GameAction held_direction(const ScreenContext& ctx) const;

    std::vector<WheelEntry> songs_;
    std::vector<std::string> pack_names_;
    int selected_song_ = 0;
    int selected_chart_ = 0;
    PreviewPlayer preview_;
    TextureCache texture_cache_;
    OptionsMenu options_;
    bool options_open_ = false;
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
