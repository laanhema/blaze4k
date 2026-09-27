#include "screens/gameplay_screen.hpp"

#include <iostream>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "screens/play_request.hpp"
#include "screens/results.hpp"
#include "screens/screen_manager.hpp"
#include "timing/judgment_constants.hpp"

namespace td {

void GameplayScreen::enter(ScreenContext& ctx) {
    active_ = false;
    end_reported_ = false;
    held_ = {false, false, false, false};

    if (ctx.play_request == nullptr || ctx.play_request->song == nullptr ||
        ctx.play_request->chart == nullptr) {
        std::cerr << "[GameplayScreen] No play request; nothing to start\n";
        return;
    }

    const Song& song = *ctx.play_request->song;
    const Chart& chart = *ctx.play_request->chart;

    const std::string audio_path = song.resolved_music_path;
    if (audio_path.empty()) {
        std::cerr << "[GameplayScreen] Song has no resolved music path; using synthetic clock\n";
    }

    const JudgmentConstants& constants =
        ctx.constants != nullptr ? *ctx.constants : JudgmentConstants::compiled_defaults();

    active_ = view_.init(chart, constants, audio_path, ctx.play_request->options,
                         song.resolved_background_path);
    std::cout << "[GameplayScreen] started '" << song.metadata.title << "' " << chart.difficulty
              << " " << chart.meter << "\n";
}

void GameplayScreen::update(ScreenContext& ctx, double fixed_dt,
                            const std::vector<InputEvent>& events) {
    if (!active_) {
        return;
    }

    // Held notes come from the authoritative InputManager state when main wires
    // `action_down`; this is robust to a missed release (e.g. focus loss) that
    // event replay would otherwise leave stuck. Headless/unit tests leave the
    // callback null and fall back to the event stream.
    if (ctx.action_down) {
        held_[0] = ctx.action_down(GameAction::Left);
        held_[1] = ctx.action_down(GameAction::Down);
        held_[2] = ctx.action_down(GameAction::Up);
        held_[3] = ctx.action_down(GameAction::Right);
    } else {
        for (const InputEvent& event : events) {
            int column = -1;
            switch (event.action) {
                case GameAction::Left: column = 0; break;
                case GameAction::Down: column = 1; break;
                case GameAction::Up: column = 2; break;
                case GameAction::Right: column = 3; break;
                default: break;
            }
            if (column >= 0) {
                held_[static_cast<std::size_t>(column)] = event.pressed;
            }
        }
    }

    view_.handle_input_events(events, ctx.input_reference_ns);
    view_.update(fixed_dt, held_);

    if (view_.outcome() != GameplayOutcome::InProgress && ctx.manager != nullptr) {
        if (!end_reported_) {
            end_reported_ = true;
            if (ctx.results != nullptr && ctx.play_request != nullptr) {
                *ctx.results = results_summary_from(view_.score_state(), view_.has_failed(),
                                                    ctx.play_request->song, ctx.play_request->chart);
            }
            // The real app registers Results; tests/older wiring fall back to
            // Select so a run can never strand on Gameplay when Results is absent.
            ctx.manager->transition_to(ctx.manager->has_screen(ScreenId::Results)
                                           ? ScreenId::Results
                                           : ScreenId::Select);
        }
    }
}

void GameplayScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w, int h) {
    if (!active_) {
        return;
    }
    view_.render(renderer, w, h);
}

void GameplayScreen::exit(ScreenContext& /*ctx*/) {
    view_.shutdown();
    active_ = false;
}

} // namespace td
