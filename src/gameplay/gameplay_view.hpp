#pragma once

#include <string>
#include <vector>
#include "chart/chart.hpp"
#include "gameplay/note_field.hpp"
#include "gameplay/note_field_renderer.hpp"
#include "gameplay/noteskin.hpp"
#include "gameplay/speed_mod.hpp"
#include "audio/sound_stream.hpp"
#include "render/gl_quad_renderer.hpp"
#include "timing/music_clock.hpp"

namespace td {

struct GameplayOptions {
    SpeedMod speed{};
    ScrollDirection scroll = ScrollDirection::Up;
    double global_offset_seconds = 0.0;
};

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

    bool init(const Chart& chart, const std::string& audio_path, const GameplayOptions& options);
    void update(double fixed_dt);
    void render(GlQuadRenderer& renderer, int screen_w, int screen_h);

    [[nodiscard]] double music_time_seconds() const { return clock_.time_seconds(); }
    [[nodiscard]] bool is_ready() const { return ready_; }

    void shutdown();

private:
    void bind_clock_source();

    Chart chart_;
    MusicClock clock_;
    SoundStream audio_;
    NoteField field_;
    NoteSkin skin_;
    NoteFieldRenderer field_renderer_;
    std::vector<NoteRenderItem> items_;
    NoteFieldConfig config_{};

    double receptor_fraction_ = 0.15;
    double stub_frames_ = 0.0;
    unsigned int stub_sample_rate_ = 48000;
    bool use_stub_ = false;
    bool audio_started_ = false;
    bool ready_ = false;
};

} // namespace td
