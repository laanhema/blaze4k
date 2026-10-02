#include "gameplay/judgment_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace blaze4k {

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
    mine_cursor_ = -std::numeric_limits<double>::infinity();
    // No press recorded yet: a held column counts as held long enough
    // (e.g. a panel already down before the song started).
    last_press_time_.fill(-std::numeric_limits<double>::infinity());

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

    // Remember the press instant: a held crossing only counts once the panel
    // has been down for `pad_stick` (OpenITG GetSecsHeld, Player.cpp:1474-1478).
    last_press_time_[static_cast<std::size_t>(column)] = music_time_seconds;
    step(column, music_time_seconds);
}

void JudgmentEngine::step(int column, double music_time_seconds) {
    // A roll is refreshed by any button-down in its column while active,
    // independent of the closest-note search (Player.cpp:1164-1222).
    refresh_active_rolls(column, music_time_seconds);

    // Closest ungraded note (mines included) within +/- StepSearchDistance;
    // ties prefer the later note (Player.cpp:779-829, 903-905).
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
    // OpenITG ADJUSTED_WINDOW_TAP(TW_Mine): base * scale + add (Player.cpp:34-58, 948).
    const TimingWindows w = constants_->effective_windows();
    if (delta_seconds > w.hit_mine) {
        return; // Outside the mine window: no event (Player.cpp:946-949).
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
    // Miss expiry uses the adjusted Boo window, like OpenITG
    // GetMaxStepDistanceSeconds = ADJUSTED_WINDOW_TAP(TW_Boo) (Player.cpp:1710-1713).
    const TimingWindows w = constants_->effective_windows();
    const double threshold = music_time - w.way_off;
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
                event.hit_time_seconds = note.time_seconds + w.way_off;
                event.delta_ms = w.way_off * 1000.0;
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
            event.hit_time_seconds = note.time_seconds + w.way_off;
            event.delta_ms = w.way_off * 1000.0;
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
    const TimingWindows w = constants_->effective_windows();

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
        // OpenITG ADJUSTED_WINDOW_HOLD(HW_OK / HW_Roll): base * scale + add
        // (Player.cpp:60-74, 563, 574).

        double life = 0.0;
        if (is_hold) {
            if (held_now) {
                state.hold_satisfied_time = music_time;
            }
            life = held_now
                       ? 1.0
                       : std::clamp(1.0 - (music_time - state.hold_satisfied_time) /
                                              w.hold_ok,
                                    0.0, 1.0);
        } else {
            life = std::clamp(1.0 - (music_time - state.hold_satisfied_time) /
                                        w.hold_roll,
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

// Held-over-mine crossing, mirroring OpenITG Player::Update's mine-row cursor
// (Player.cpp:632-646) and Player::CrossedMineRow (Player.cpp:1461-1488):
//  - The cursor runs `pad_stick` (PadStickSeconds) behind the music, and only
//    mines crossed since the previous update, i.e. in (prev_cursor, cursor],
//    are checked. The cursor only moves forward (a backward clock resync does
//    not rewind it), so each mine is evaluated exactly once, when it crosses.
//    A mine already in the past is never re-checked, so a later press in the column
//    (e.g. on the next arrow) cannot explode it. Re-checking every past,
//    unexpired mine was the root cause of #56.
//  - A held column counts only if the panel has been down for at least
//    `pad_stick` (GetSecsHeld >= PadStickSeconds); with pad_stick == 0 this is
//    OpenITG's IsButtonDown branch.
//  - The crossing goes through the shared step routine at `cursor`
//    (Step(t, now - PadStickSeconds, bHeld=true)), so selection, the mine window
//    and roll refresh behave exactly like a deliberate press. Like OpenITG, no
//    "already graded" check is made before stepping; `step` skips graded notes.
// OpenITG works in note rows; seconds are equivalent at constant BPM.
void JudgmentEngine::cross_mines(double music_time, const std::array<bool, 4>& held_columns) {
    const double pad_stick = constants_->windows.pad_stick;
    const double cursor = music_time - pad_stick;
    const double lower = mine_cursor_;
    mine_cursor_ = std::max(mine_cursor_, cursor);

    for (int column = 0; column < 4; ++column) {
        const auto col = static_cast<std::size_t>(column);
        if (!held_columns[col] || last_press_time_[col] > cursor) {
            continue; // Not held, or not held for pad_stick yet: mines are avoided.
        }
        for (int i : column_mines_[col]) {
            const double t = chart_->notes[static_cast<std::size_t>(i)].time_seconds;
            if (t <= lower) {
                continue; // Crossed during an earlier update.
            }
            if (t > cursor) {
                // Not crossed yet. `continue` rather than `break`: notes are
                // beat-ordered, and a warp (negative stop) can make seconds
                // non-monotonic in beat order.
                continue;
            }
            step(column, cursor); // Once per crossed mine (Player.cpp:1466-1484).
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

bool JudgmentEngine::is_hold_head_hit(int note_index) const {
    if (note_index < 0 || static_cast<std::size_t>(note_index) >= states_.size()) {
        return false;
    }
    return states_[static_cast<std::size_t>(note_index)].hold_head_hit;
}

HoldJudgment JudgmentEngine::hold_judgment(int note_index) const {
    if (note_index < 0 || static_cast<std::size_t>(note_index) >= states_.size()) {
        return HoldJudgment::Num;
    }
    return states_[static_cast<std::size_t>(note_index)].hold;
}

double JudgmentEngine::hold_last_held_seconds(int note_index) const {
    if (note_index < 0 || static_cast<std::size_t>(note_index) >= states_.size()) {
        return 0.0;
    }
    return states_[static_cast<std::size_t>(note_index)].hold_satisfied_time;
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

} // namespace blaze4k
