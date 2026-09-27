#pragma once

#include <array>
#include <string>
#include <vector>
#include "chart/chart.hpp"
#include "gameplay/hud_renderer.hpp"
#include "gameplay/judgment_engine.hpp"
#include "gameplay/life_keeper.hpp"
#include "gameplay/note_field.hpp"
#include "gameplay/note_field_renderer.hpp"
#include "gameplay/noteskin.hpp"
#include "gameplay/score_keeper.hpp"
#include "gameplay/gameplay_options.hpp"
#include "gameplay/speed_mod.hpp"
#include "audio/sound_stream.hpp"
#include "input/input_event.hpp"
#include "render/background_renderer.hpp"
#include "render/gl_quad_renderer.hpp"
#include "timing/music_clock.hpp"

namespace td {

// Where a single song run stands. B6 exposes this so the future screen state
// machine (C1/C7) can transition out of gameplay. `Cleared` means every row/hold
// resolved without failing; `Failed` takes precedence.
enum class GameplayOutcome { InProgress, Cleared, Failed };

// Hosts a single-song gameplay field: chart copy, audio + `MusicClock`, layout,
// and rendering. Gameplay time comes exclusively from `MusicClock` (bound to the
// audio stream in production) — never wall-clock or frame delta.
//
// If audio is unavailable (e.g. headless), `init` falls back to a synthetic PCM
// source advanced by `update(fixed_dt)`. This is a demo/harness affordance only;
// production gameplay always binds the `SoundStream`.
class GameplayView {
public:
    GameplayView() = default;
    ~GameplayView();

    GameplayView(const GameplayView&) = delete;
    GameplayView& operator=(const GameplayView&) = delete;

    bool init(const Chart& chart, const JudgmentConstants& constants, const std::string& audio_path,
              const GameplayOptions& options, const std::string& background_path = "");
    void handle_input_events(const std::vector<InputEvent>& events, uint64_t reference_ns);
    void update(double fixed_dt, const std::array<bool, 4>& held_columns);
    void render(GlQuadRenderer& renderer, int screen_w, int screen_h);

    [[nodiscard]] double music_time_seconds() const { return clock_.time_seconds(); }
    [[nodiscard]] bool is_ready() const { return ready_; }

    [[nodiscard]] const std::vector<JudgmentEvent>& judgment_events() const {
        return judge_.events();
    }
    [[nodiscard]] const JudgmentEvent* latest_judgment() const { return judge_.latest_event(); }
    [[nodiscard]] bool is_note_hidden(int note_index) const { return judge_.is_note_hidden(note_index); }

    [[nodiscard]] const ScoreState& score_state() const { return score_.state(); }
    [[nodiscard]] int dance_points() const { return score_.actual_dance_points(); }
    [[nodiscard]] double score_percent() const { return score_.percent(); }

    [[nodiscard]] const LifeState& life_state() const { return life_.state(); }
    [[nodiscard]] double life() const { return life_.life(); }
    [[nodiscard]] bool has_failed() const { return life_.has_failed(); }
    [[nodiscard]] bool is_cleared() const { return score_.is_complete(); }
    [[nodiscard]] GameplayOutcome outcome() const;

    void shutdown();

private:
    void bind_clock_source();

    Chart chart_;
    MusicClock clock_;
    SoundStream audio_;
    NoteField field_;
    NoteSkin skin_;
    BackgroundRenderer background_;
    NoteFieldRenderer field_renderer_;
    JudgmentEngine judge_;
    ScoreKeeper score_;
    LifeKeeper life_;
    HudRenderer hud_;
    std::vector<JudgmentEvent> new_events_;
    std::vector<NoteRenderItem> items_;
    std::vector<NoteRenderItem> visible_items_;
    NoteFieldConfig config_{};

    double receptor_fraction_ = 0.15;
    double stub_frames_ = 0.0;
    unsigned int stub_sample_rate_ = 48000;
    bool use_stub_ = false;
    bool audio_started_ = false;
    bool exited_ = false; // fail transition already taken (gameplay ended)
    bool ready_ = false;
};

} // namespace td
