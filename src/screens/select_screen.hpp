#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "audio/preview_player.hpp"
#include "chart/timing_data.hpp"
#include "render/texture_cache.hpp"
#include "screens/screen.hpp"

namespace td {
struct Chart;
struct Song;
struct ScoreRecord;

// Pure BPM display: "140" when all segments agree, else "128-175" (min-max).
// The empty-segment guard returns "?" but is defensive only: TimingData always
// seeds at least one BPM segment, so it is not reachable from a parsed simfile.
// No remapping of the underlying timing.
[[nodiscard]] std::string format_bpm_range(const TimingData& timing);

// Pure best-grade label: the four OpenITG star tiers render as star glyphs
// (quad_star -> four stars ... single_star -> one star); every other stored
// label ("S+", "A", ...) passes through unchanged.
[[nodiscard]] std::string grade_display_label(const std::string& grade);

// Best stored score for a chart, or nullptr when scores are unavailable/empty.
[[nodiscard]] const ScoreRecord* best_grade_for(const ScreenContext& ctx, const Song& song,
                                                const Chart& chart);

// The real Song Select wheel: a flat, pack-grouped list of the scanned library
// with banner art, artist/BPM, passthrough difficulty labels + foot ratings, and
// the player's best grade per chart. Highlighting a song arms a delayed audio
// preview; Confirm publishes a `PlayRequest` and transitions to Gameplay; Back
// is handled centrally by the manager (Select -> Title).
class SelectScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
    void exit(ScreenContext& ctx) override;

    // Test accessors (headless, pure).
    [[nodiscard]] std::size_t song_count() const { return songs_.size(); }
    [[nodiscard]] int selected_song_index() const { return selected_song_; }
    [[nodiscard]] int selected_chart_index() const { return selected_chart_; }
    [[nodiscard]] const Song* selected_song() const;
    [[nodiscard]] const Chart* selected_chart() const;
    [[nodiscard]] int chart_count() const;
    [[nodiscard]] std::size_t total_chart_count() const;
    [[nodiscard]] const PreviewPlayer& preview() const { return preview_; }

private:
    struct WheelEntry {
        const Song* song = nullptr;
        int pack_index = 0;
    };

    void rebuild(const ScreenContext& ctx);
    void move_song(int delta);
    void move_chart(int delta);
    void request_preview_for_selected();

    std::vector<WheelEntry> songs_;
    std::vector<std::string> pack_names_;
    int selected_song_ = 0;
    int selected_chart_ = 0;
    PreviewPlayer preview_;
    TextureCache texture_cache_;
};

} // namespace td
