#include "gameplay/judgment_engine.hpp"

#include <algorithm>
#include <cmath>

namespace td {

namespace {

// OpenITG `StepSearchDistance = 1.0 s` (src/Player.cpp:27).
constexpr double kStepSearchDistanceSeconds = 1.0;

bool valid_column(int column) {
    return column >= 0 && column < 4;
}

} // namespace

void JudgmentEngine::reset(const Chart* chart, const JudgmentConstants* constants) {
    chart_ = chart;
    constants_ = constants;

    events_.clear();
    new_event_begin_ = 0;
    active_holds_.clear();
    column_notes_.assign(4, {});
    column_mines_.assign(4, {});
    has_last_update_ = false;
    last_update_time_ = 0.0;

    if (chart_ == nullptr) {
        states_.clear();
        return;
    }

    states_.assign(chart_->notes.size(), NoteState{});
    for (std::size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& note = chart_->notes[i];
        if (!valid_column(note.column)) {
            continue;
        }
        column_notes_[static_cast<std::size_t>(note.column)].push_back(static_cast<int>(i));
        if (note.type == NoteType::Mine) {
            column_mines_[static_cast<std::size_t>(note.column)].push_back(static_cast<int>(i));
        } else if (note.is_hold_or_roll()) {
            active_holds_.push_back(static_cast<int>(i));
        }
    }
}

void JudgmentEngine::handle_step(int column, double music_time_seconds) {
    if (chart_ == nullptr || constants_ == nullptr || !valid_column(column)) {
        return;
    }

    // A roll is refreshed by any button-down in its column while active,
    // independent of the closest-note search (Player.cpp:1190-1225).
    refresh_active_rolls(column, music_time_seconds);

    // Closest ungraded note within +/- StepSearchDistance; ties prefer the later
    // note (Player.cpp:791-823, 894-895).
    const std::vector<int>& notes = column_notes_[static_cast<std::size_t>(column)];
    int best = -1;
    double best_distance = 0.0;
    double best_time = 0.0;
    for (int i : notes) {
        const NoteState& state = states_[static_cast<std::size_t>(i)];
        if (state.complete || state.tap != TapJudgment::Num) {
            continue;
        }
        const Note& note = chart_->notes[static_cast<std::size_t>(i)];
        const double distance = std::fabs(note.time_seconds - music_time_seconds);
        if (distance > kStepSearchDistanceSeconds) {
            continue;
        }
        if (best < 0 || distance < best_distance ||
            (distance == best_distance && note.time_seconds > best_time)) {
            best = i;
            best_distance = distance;
            best_time = note.time_seconds;
        }
    }

    if (best < 0) {
        return; // OpenITG TNS_NONE: no event.
    }

    const Note& note = chart_->notes[static_cast<std::size_t>(best)];
    const double delta_seconds = std::fabs(note.time_seconds - music_time_seconds);
    if (note.type == NoteType::Mine) {
        handle_step_mine(best, delta_seconds, music_time_seconds);
    } else {
        handle_step_tap(best, delta_seconds, music_time_seconds);
    }
}

void JudgmentEngine::handle_step_tap(int note_index, double delta_seconds, double hit_time) {
    const TapJudgment judgment = constants_->classify_tap(delta_seconds);
    if (judgment == TapJudgment::Miss) {
        return; // A step beyond Way Off yields TNS_NONE, not a Miss (Player.cpp:938-947).
    }

    NoteState& state = states_[static_cast<std::size_t>(note_index)];
    const Note& note = chart_->notes[static_cast<std::size_t>(note_index)];
    state.tap = judgment;

    if (note.is_hold_or_roll()) {
        // Hold/roll heads are graded like taps; the hold outcome stays pending
        // (Player.cpp:938-947, 1117-1122).
        state.hold_head_hit = true;
        state.hold_satisfied_time = std::max(hit_time, note.time_seconds);
    } else {
        state.complete = true;
    }

    JudgmentEvent event;
    event.kind = JudgmentKind::Tap;
    event.column = note.column;
    event.note_time_seconds = note.time_seconds;
    event.hit_time_seconds = hit_time;
    event.delta_ms = (hit_time - note.time_seconds) * 1000.0;
    event.window = judgment;
    event.note_type = note.type;
    event.note_index = note_index;
    emit(event);
}

void JudgmentEngine::handle_step_mine(int note_index, double delta_seconds, double hit_time) {
    if (delta_seconds > constants_->windows.hit_mine) {
        return; // Outside the mine window: no event (Player.cpp:930-934).
    }

    NoteState& state = states_[static_cast<std::size_t>(note_index)];
    const Note& note = chart_->notes[static_cast<std::size_t>(note_index)];
    state.tap = TapJudgment::HitMine;
    state.complete = true;

    JudgmentEvent event;
    event.kind = JudgmentKind::HitMine;
    event.column = note.column;
    event.note_time_seconds = note.time_seconds;
    event.hit_time_seconds = hit_time;
    event.delta_ms = (hit_time - note.time_seconds) * 1000.0;
    event.window = TapJudgment::HitMine;
    event.note_type = note.type;
    event.note_index = note_index;
    emit(event);
}

void JudgmentEngine::refresh_active_rolls(int column, double music_time) {
    for (int i : active_holds_) {
        NoteState& state = states_[static_cast<std::size_t>(i)];
        if (state.complete || !state.hold_head_hit) {
            continue;
        }
        const Note& note = chart_->notes[static_cast<std::size_t>(i)];
        if (note.type != NoteType::RollHead || note.column != column) {
            continue;
        }
        if (music_time < note.time_seconds || music_time > note.hold_end_time_seconds) {
            continue;
        }

        state.hold_satisfied_time = music_time;

        JudgmentEvent event;
        event.kind = JudgmentKind::RollHit;
        event.column = note.column;
        event.note_time_seconds = note.time_seconds;
        event.hit_time_seconds = music_time;
        event.delta_ms = (music_time - note.time_seconds) * 1000.0;
        event.note_type = note.type;
        event.note_index = i;
        emit(event);
    }
}

void JudgmentEngine::update(double music_time_seconds, const std::array<bool, 4>& held_columns) {
    if (chart_ == nullptr || constants_ == nullptr) {
        return;
    }

    // No seeking in v1: a backward jump resyncs without emitting (documented).
    if (has_last_update_ && music_time_seconds < last_update_time_) {
        last_update_time_ = music_time_seconds;
        return;
    }

    expire_notes(music_time_seconds);
    update_holds(music_time_seconds, held_columns);
    cross_mines(music_time_seconds, held_columns);

    last_update_time_ = music_time_seconds;
    has_last_update_ = true;
}

void JudgmentEngine::expire_notes(double music_time) {
    const double threshold = music_time - constants_->windows.way_off;
    for (int column = 0; column < 4; ++column) {
        for (int i : column_notes_[static_cast<std::size_t>(column)]) {
            NoteState& state = states_[static_cast<std::size_t>(i)];
            if (state.complete || state.tap != TapJudgment::Num) {
                continue;
            }
            const Note& note = chart_->notes[static_cast<std::size_t>(i)];
            if (note.time_seconds >= threshold) {
                continue; // Not yet past its window.
            }

            if (note.type == NoteType::Mine) {
                // Untouched mine: no penalty, stats only (Player.cpp:1417-1419).
                state.complete = true;
                JudgmentEvent event;
                event.kind = JudgmentKind::AvoidedMine;
                event.column = note.column;
                event.note_time_seconds = note.time_seconds;
                event.hit_time_seconds = note.time_seconds + constants_->windows.way_off;
                event.delta_ms = constants_->windows.way_off * 1000.0;
                event.window = TapJudgment::Num;
                event.note_type = note.type;
                event.note_index = i;
                emit(event);
                continue;
            }

            // Untouched tap / hold-head / roll-head: Miss; a missed hold head
            // produces no hold OK/NG (Player.cpp:1403-1435).
            state.tap = TapJudgment::Miss;
            state.hold = HoldJudgment::Num;
            state.complete = true;

            JudgmentEvent event;
            event.kind = JudgmentKind::Miss;
            event.column = note.column;
            event.note_time_seconds = note.time_seconds;
            event.hit_time_seconds = note.time_seconds + constants_->windows.way_off;
            event.delta_ms = constants_->windows.way_off * 1000.0;
            event.window = TapJudgment::Miss;
            event.note_type = note.type;
            event.note_index = i;
            emit(event);
        }
    }
}

void JudgmentEngine::update_holds(double music_time, const std::array<bool, 4>& held_columns) {
    std::vector<int> still_active;
    still_active.reserve(active_holds_.size());

    for (int i : active_holds_) {
        NoteState& state = states_[static_cast<std::size_t>(i)];
        if (state.complete) {
            continue; // Resolved elsewhere (e.g. missed head).
        }
        if (!state.hold_head_hit) {
            still_active.push_back(i); // Head not graded yet.
            continue;
        }

        const Note& note = chart_->notes[static_cast<std::size_t>(i)];
        if (music_time < note.time_seconds) {
            still_active.push_back(i); // Hold has not started.
            continue;
        }

        const bool is_hold = note.type == NoteType::HoldHead;
        // OpenITG keeps fLife = 1.0 while the button is down (Player.cpp). A
        // coarse update that lands past the tail must not decay through the gap
        // and emit NG, so the held state alone drives the life term; the tail is
        // resolved by the OK branch below.
        const bool held_now = held_columns[static_cast<std::size_t>(note.column)];

        double life = 0.0;
        if (is_hold) {
            if (held_now) {
                state.hold_satisfied_time = music_time;
            }
            life = held_now
                       ? 1.0
                       : std::clamp(1.0 - (music_time - state.hold_satisfied_time) /
                                              constants_->windows.hold_ok,
                                    0.0, 1.0);
        } else {
            life = std::clamp(1.0 - (music_time - state.hold_satisfied_time) /
                                        constants_->windows.hold_roll,
                              0.0, 1.0);
        }

        if (life <= 0.0) {
            state.hold = HoldJudgment::Ng;
            state.complete = true;

            JudgmentEvent event;
            event.kind = is_hold ? JudgmentKind::HoldNg : JudgmentKind::RollNg;
            event.column = note.column;
            event.note_time_seconds = note.time_seconds;
            event.hit_time_seconds = music_time;
            event.delta_ms = 0.0;
            event.hold = HoldJudgment::Ng;
            event.note_type = note.type;
            event.note_index = i;
            emit(event);
        } else if (music_time >= note.hold_end_time_seconds) {
            state.hold = HoldJudgment::Ok;
            state.complete = true;

            JudgmentEvent event;
            event.kind = is_hold ? JudgmentKind::HoldOk : JudgmentKind::RollOk;
            event.column = note.column;
            event.note_time_seconds = note.time_seconds;
            event.hit_time_seconds = music_time;
            event.delta_ms = 0.0;
            event.hold = HoldJudgment::Ok;
            event.note_type = note.type;
            event.note_index = i;
            emit(event);
        } else {
            still_active.push_back(i);
        }
    }

    active_holds_ = std::move(still_active);
}

void JudgmentEngine::cross_mines(double music_time, const std::array<bool, 4>& held_columns) {
    for (int column = 0; column < 4; ++column) {
        const bool held = held_columns[static_cast<std::size_t>(column)];
        for (int i : column_mines_[static_cast<std::size_t>(column)]) {
            NoteState& state = states_[static_cast<std::size_t>(i)];
            if (state.complete) {
                continue;
            }
            const Note& note = chart_->notes[static_cast<std::size_t>(i)];
            if (note.time_seconds > music_time) {
                continue;
            }
            if (!held) {
                continue; // Left for expiry -> AvoidedMine.
            }

            state.tap = TapJudgment::HitMine;
            state.complete = true;

            JudgmentEvent event;
            event.kind = JudgmentKind::HitMine;
            event.column = note.column;
            event.note_time_seconds = note.time_seconds;
            event.hit_time_seconds = music_time;
            event.delta_ms = (music_time - note.time_seconds) * 1000.0;
            event.window = TapJudgment::HitMine;
            event.note_type = note.type;
            event.note_index = i;
            emit(event);
        }
    }
}

bool JudgmentEngine::is_note_judged(int note_index) const {
    if (note_index < 0 || static_cast<std::size_t>(note_index) >= states_.size()) {
        return false;
    }
    return states_[static_cast<std::size_t>(note_index)].complete;
}

bool JudgmentEngine::is_note_hidden(int note_index) const {
    if (chart_ == nullptr || note_index < 0 ||
        static_cast<std::size_t>(note_index) >= states_.size()) {
        return false;
    }
    const Note& note = chart_->notes[static_cast<std::size_t>(note_index)];
    const NoteState& state = states_[static_cast<std::size_t>(note_index)];
    if (note.type == NoteType::Mine) {
        return state.tap == TapJudgment::HitMine;
    }
    return is_tap_window_hidden(state.tap);
}

bool JudgmentEngine::is_hold_in_progress(int note_index) const {
    if (chart_ == nullptr || note_index < 0 ||
        static_cast<std::size_t>(note_index) >= states_.size()) {
        return false;
    }
    const Note& note = chart_->notes[static_cast<std::size_t>(note_index)];
    if (!note.is_hold_or_roll()) {
        return false;
    }
    const NoteState& state = states_[static_cast<std::size_t>(note_index)];
    return state.hold_head_hit && !state.complete;
}

const JudgmentEvent* JudgmentEngine::latest_event() const {
    if (events_.empty()) {
        return nullptr;
    }
    return &events_.back();
}

void JudgmentEngine::drain_new_events(std::vector<JudgmentEvent>& out) {
    if (new_event_begin_ < events_.size()) {
        out.insert(out.end(), events_.begin() + static_cast<std::ptrdiff_t>(new_event_begin_),
                   events_.end());
    }
    new_event_begin_ = events_.size();
}

void JudgmentEngine::emit(const JudgmentEvent& event) {
    events_.push_back(event);
}

} // namespace td
