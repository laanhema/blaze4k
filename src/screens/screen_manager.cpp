#include "screens/screen_manager.hpp"

#include <iostream>

#include "render/gl_quad_renderer.hpp"

namespace td {

namespace {

// The manager's default Back-navigation set. Title/Results fall through to the
// App's Escape-quit. Shared with back_navigates() so the two cannot desync.
bool default_back_navigates(ScreenId id) {
    return id == ScreenId::Attract || id == ScreenId::Select || id == ScreenId::Gameplay;
}

} // namespace

std::string_view screen_id_name(ScreenId id) {
    switch (id) {
        case ScreenId::Title: return "Title";
        case ScreenId::Attract: return "Attract";
        case ScreenId::Select: return "Select";
        case ScreenId::Gameplay: return "Gameplay";
        case ScreenId::Results: return "Results";
    }
    return "Unknown";
}

ScreenManager::ScreenManager(double idle_timeout_seconds)
    : idle_timeout_seconds_(idle_timeout_seconds) {
    ctx_.manager = this;
}

void ScreenManager::add_screen(std::unique_ptr<Screen> screen) {
    if (screen == nullptr) {
        return;
    }
    const ScreenId id = screen->id();
    for (std::unique_ptr<Screen>& existing : screens_) {
        if (existing->id() == id) {
            existing = std::move(screen); // last registration wins
            return;
        }
    }
    screens_.push_back(std::move(screen));
}

bool ScreenManager::has_screen(ScreenId id) const {
    for (const std::unique_ptr<Screen>& screen : screens_) {
        if (screen->id() == id) {
            return true;
        }
    }
    return false;
}

Screen* ScreenManager::active_screen() const {
    for (const std::unique_ptr<Screen>& screen : screens_) {
        if (screen->id() == active_id_) {
            return screen.get();
        }
    }
    return nullptr;
}

void ScreenManager::start(ScreenId initial) {
    if (started_) {
        if (Screen* outgoing = active_screen(); outgoing != nullptr) {
            outgoing->exit(ctx_);
        }
    }

    active_id_ = initial;
    pending_id_ = initial;
    has_pending_ = false;
    started_ = true;
    idle_seconds_ = 0.0;

    Screen* screen = active_screen();
    if (screen == nullptr) {
        std::cout << "[ScreenManager] no screen registered for " << screen_id_name(initial)
                  << "; start ignored\n";
        return;
    }
    screen->enter(ctx_);
    std::cout << "[ScreenManager] enter " << screen_id_name(active_id_) << "\n";
}

void ScreenManager::transition_to(ScreenId target) {
    pending_id_ = target;
    has_pending_ = true;
}

void ScreenManager::apply_pending() {
    if (!has_pending_) {
        return;
    }
    has_pending_ = false;

    if (pending_id_ == active_id_) {
        return; // no-op: never enter a screen twice without exiting it
    }

    Screen* target = nullptr;
    for (std::unique_ptr<Screen>& screen : screens_) {
        if (screen->id() == pending_id_) {
            target = screen.get();
            break;
        }
    }
    if (target == nullptr) {
        std::cout << "[ScreenManager] no screen registered for " << screen_id_name(pending_id_)
                  << "; transition ignored\n";
        return;
    }

    if (started_) {
        if (Screen* outgoing = active_screen(); outgoing != nullptr) {
            outgoing->exit(ctx_);
        }
    }

    const ScreenId previous = active_id_;
    active_id_ = pending_id_;
    idle_seconds_ = 0.0;
    target->enter(ctx_);
    std::cout << "[ScreenManager] " << screen_id_name(previous) << " -> "
              << screen_id_name(active_id_) << "\n";
}

void ScreenManager::handle_back() {
    // An in-screen modal (C4 options overlay) consumes Back first; returning
    // true suppresses the default navigation below.
    if (Screen* active = active_screen(); active != nullptr && active->handle_back(ctx_)) {
        return;
    }
    if (!default_back_navigates(active_id_)) {
        return; // Title/Results: no-op. The App handles Escape-quit on Title.
    }
    if (active_id_ == ScreenId::Attract) {
        transition_to(attract_return_);
    } else if (active_id_ == ScreenId::Select) {
        transition_to(ScreenId::Title);
    } else if (active_id_ == ScreenId::Gameplay) {
        transition_to(ScreenId::Select); // abort the run; Results/pause are C7
    }
}

bool ScreenManager::back_navigates() const {
    // Keep this exactly in sync with handle_back(): the same default navigation
    // set, plus any active-screen modal that consumes Back. Sharing
    // default_back_navigates() and Screen::back_consumed() stops a future screen
    // from making handle_back() act while the App still quits on Escape.
    if (const Screen* active = active_screen(); active != nullptr && active->back_consumed()) {
        return true;
    }
    return default_back_navigates(active_id_);
}

void ScreenManager::update(double fixed_dt, const std::vector<InputEvent>& events) {
    apply_pending();

    bool had_press = false;
    bool back_pressed = false;
    bool confirm_pressed = false;
    bool options_pressed = false;
    for (const InputEvent& event : events) {
        if (!event.pressed) {
            continue;
        }
        had_press = true;
        if (event.action == GameAction::Back) {
            back_pressed = true;
        } else if (event.action == GameAction::Confirm) {
            confirm_pressed = true;
        } else if (event.action == GameAction::Options) {
            options_pressed = true;
        }
    }

    if (had_press) {
        idle_seconds_ = 0.0;
    }

    // A same-tick [Options, Back] pair must not navigate on pre-update state:
    // update() is about to open (or close) the overlay. Defer Back to after
    // update() for that tick so handle_back() sees the state update() created.
    if (back_pressed && !options_pressed) {
        handle_back();
    }
    // Attract is "just another screen": the manager owns the exit-confirm policy
    // so the screen itself stays dumb (PRD section 6 pattern 4).
    if (active_id_ == ScreenId::Attract && confirm_pressed) {
        transition_to(attract_return_);
    }

    if (Screen* active = active_screen(); active != nullptr) {
        active->update(ctx_, fixed_dt, events);
    }

    if (back_pressed && options_pressed) {
        handle_back();
    }

    const ScreenId before_apply = active_id_;
    apply_pending();
    const bool active_changed = active_id_ != before_apply;

    // Idle-attract policy: measured only from the injected fixed_dt, and only
    // while Title/Select are active.
    if ((active_id_ == ScreenId::Title || active_id_ == ScreenId::Select) &&
        idle_timeout_seconds_ > 0.0) {
        if (!had_press && !active_changed) {
            idle_seconds_ += fixed_dt;
        }
        if (idle_seconds_ >= idle_timeout_seconds_ && has_screen(ScreenId::Attract)) {
            attract_return_ = active_id_;
            transition_to(ScreenId::Attract);
            apply_pending();
        }
    }
}

void ScreenManager::render(GlQuadRenderer& renderer, int screen_w, int screen_h) {
    if (Screen* active = active_screen(); active != nullptr) {
        active->render(ctx_, renderer, screen_w, screen_h);
    }
}

void ScreenManager::set_idle_timeout_seconds(double seconds) {
    idle_timeout_seconds_ = seconds;
}

} // namespace td