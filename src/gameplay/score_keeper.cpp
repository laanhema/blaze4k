#include "gameplay/score_keeper.hpp"

#include <algorithm>
#include <limits>

namespace blaze4k {

void ScoreKeeper::reset(const Chart* chart, const JudgmentConstants* constants) {
    chart_ = chart;
    constants_ = constants;
    state_ = ScoreState{};
    note_row_.clear();
    rows_.clear();
    note_scored_.clear();
    hold_scored_.clear();

    if (chart_ == nullptr) {
        return;
    }

    note_row_.assign(chart_->notes.size(), -1);
    note_scored_.assign(chart_->notes.size(), false);
    hold_scored_.assign(chart_->notes.size(), false);

    // Row identity is the exact `Note.beat` (not `time_seconds`): a stop/warp can
    // map two distinct rows to the same seconds, and OpenITG scores them
    // separately (Plan "Pinned Semantics"). `chart.notes` is beat-sorted, so equal
    // beats are contiguous. Mines never define or join a scoring row.
    bool has_prev_beat = false;
    double prev_beat = 0.0;
    int current_row = -1;
    for (std::size_t i = 0; i < chart_->notes.size(); ++i) {
        const Note& note = chart_->notes[i];
        if (note.type == NoteType::Mine) {
            continue;
        }
        if (!has_prev_beat || note.beat != prev_beat) {
            rows_.push_back(RowAggregate{});
            rows_.back().last_delta_ms = std::numeric_limits<double>::lowest();
            current_row = static_cast<int>(rows_.size()) - 1;
            prev_beat = note.beat;
            has_prev_beat = true;
        }
        note_row_[i] = current_row;
        rows_[static_cast<std::size_t>(current_row)].expected++;
    }

    // Chart-derived maximum (OpenITG `GetPossibleDancePoints`): one Fantastic-weight
    // tap row per row-with-tap-or-hold-head, plus hold/roll heads at the OK weight.
    if (constants_ != nullptr) {
        const int row_count = static_cast<int>(rows_.size());
        state_.possible_dp = row_count * constants_->dp_weights.fantastic +
                             chart_->hold_count * constants_->dp_weights.hold_ok +
                             chart_->roll_count * constants_->dp_weights.hold_ok;
    }

    recompute_derived();
}

void ScoreKeeper::consume(const JudgmentEvent& event) {
    if (chart_ == nullptr || constants_ == nullptr) {
        return;
    }

    switch (event.kind) {
        case JudgmentKind::Tap:
            apply_tap_like(event, false);
            break;
        case JudgmentKind::Miss:
            apply_tap_like(event, true);
            break;
        case JudgmentKind::HoldOk:
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollOk:
        case JudgmentKind::RollNg:
            apply_hold(event);
            break;
        case JudgmentKind::HitMine:
            // Hit mine scores but never changes combo (ScoreKeeperMAX2.cpp:333-341).
            state_.actual_dp += constants_->dp_weights.hit_mine;
            state_.tap_counts[static_cast<std::size_t>(TapJudgment::HitMine)]++;
            break;
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit:
            break; // stats/visual only; no scoring effect
    }

    recompute_derived();
}

void ScoreKeeper::consume(const std::vector<JudgmentEvent>& events) {
    for (const JudgmentEvent& event : events) {
        consume(event);
    }
}

void ScoreKeeper::apply_tap_like(const JudgmentEvent& event, bool miss) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= note_row_.size()) {
        return;
    }
    if (note_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard: one event per note (idempotent per note)
    }
    const int row = note_row_[static_cast<std::size_t>(index)];
    if (row < 0) {
        return; // mine / unscored note
    }
    note_scored_[static_cast<std::size_t>(index)] = true;

    RowAggregate& aggregate = rows_[static_cast<std::size_t>(row)];
    aggregate.judged++;

    const bool is_miss = miss || event.window == TapJudgment::Miss;
    if (is_miss) {
        aggregate.has_miss = true; // a miss anywhere makes the whole row a miss
        const Note& note = chart_->notes[static_cast<std::size_t>(index)];
        if (note.is_hold_or_roll()) {
            // A missed hold/roll head resolves with no further hold outcome.
            hold_scored_[static_cast<std::size_t>(index)] = true;
        }
    } else if (event.delta_ms > aggregate.last_delta_ms ||
               (event.delta_ms == aggregate.last_delta_ms && event.column > aggregate.last_column)) {
        // Last-tap semantics: greatest offset wins, ties -> later column.
        aggregate.last_delta_ms = event.delta_ms;
        aggregate.last_column = event.column;
        aggregate.last_window = event.window;
    }

    if (aggregate.judged == aggregate.expected) {
        resolve_row(row);
    }
}

void ScoreKeeper::resolve_row(int row) {
    const RowAggregate& aggregate = rows_[static_cast<std::size_t>(row)];
    const TapJudgment score = aggregate.has_miss ? TapJudgment::Miss : aggregate.last_window;
    if (score == TapJudgment::Num) {
        return; // defensive: a completed row always has a graded tap
    }

    state_.actual_dp += tap_weight(score);
    state_.tap_counts[static_cast<std::size_t>(score)]++;

    // Combo uses the constant's monotonic predicate, never the (non-monotonic) enum.
    if (constants_->continues_combo(score)) {
        state_.combo += aggregate.expected; // ComboIsPerRow=false
        state_.miss_combo = 0;
    } else {
        state_.combo = 0;
        if (score == TapJudgment::Miss) {
            state_.miss_combo++;
        }
    }
    state_.max_combo = std::max(state_.max_combo, state_.combo);
}

void ScoreKeeper::apply_hold(const JudgmentEvent& event) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= hold_scored_.size()) {
        return;
    }
    if (!chart_->notes[static_cast<std::size_t>(index)].is_hold_or_roll()) {
        return; // only hold/roll heads produce hold outcomes
    }
    if (hold_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard
    }
    hold_scored_[static_cast<std::size_t>(index)] = true;

    switch (event.hold) {
        case HoldJudgment::Ok:
            state_.actual_dp += constants_->dp_weights.hold_ok;
            state_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)]++;
            break;
        case HoldJudgment::Ng:
            state_.actual_dp += constants_->dp_weights.hold_ng;
            state_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)]++;
            break;
        case HoldJudgment::Num:
            break;
    }
    // Hold/roll outcomes never change combo (ScoreKeeperMAX2.cpp:414-437).
}

void ScoreKeeper::recompute_derived() {
    if (state_.possible_dp == 0) {
        state_.percent = 0.0;
    } else if (state_.actual_dp == state_.possible_dp) {
        state_.percent = 1.0; // OpenITG rounding correction
    } else {
        state_.percent =
            static_cast<double>(state_.actual_dp) / static_cast<double>(state_.possible_dp);
    }

    if (constants_ != nullptr) {
        state_.grade = &constants_->grade_for_percent(state_.percent);
    } else {
        state_.grade = nullptr;
    }
}

int ScoreKeeper::tap_weight(TapJudgment j) const {
    if (constants_ == nullptr) {
        return 0;
    }
    const Weights& w = constants_->dp_weights;
    switch (j) {
        case TapJudgment::Fantastic: return w.fantastic;
        case TapJudgment::Excellent: return w.excellent;
        case TapJudgment::Great: return w.great;
        case TapJudgment::Decent: return w.decent;
        case TapJudgment::WayOff: return w.way_off;
        case TapJudgment::Miss: return w.miss;
        case TapJudgment::HitMine: return w.hit_mine;
        case TapJudgment::Num: return 0;
    }
    return 0;
}

const GradeTier& ScoreKeeper::grade() const {
    if (constants_ == nullptr) {
        static const GradeTier fallback{-1000.0, "D"};
        return fallback;
    }
    return constants_->grade_for_percent(state_.percent);
}

bool ScoreKeeper::is_complete() const {
    for (const RowAggregate& row : rows_) {
        if (row.judged < row.expected) {
            return false;
        }
    }
    if (chart_ != nullptr) {
        for (std::size_t i = 0; i < chart_->notes.size(); ++i) {
            if (chart_->notes[i].is_hold_or_roll() && !hold_scored_[i]) {
                return false;
            }
        }
    }
    return true;
}

} // namespace blaze4k
