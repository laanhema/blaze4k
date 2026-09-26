#include "gameplay/life_keeper.hpp"

#include <algorithm>
#include <limits>

namespace td {

namespace {

// Local copy of OpenITG's SCALE macro (RageUtil.h:38) so the purity check never
// sees a RageUtil include: maps x from [l1,h1] onto [l2,h2].
constexpr double scale(double x, double l1, double h1, double l2, double h2) {
    return (x - l1) * (h2 - l2) / (h1 - l1) + l2;
}

} // namespace

void LifeKeeper::reset(const Chart* chart, const JudgmentConstants* constants) {
    chart_ = chart;
    constants_ = constants;
    state_ = LifeState{};
    note_row_.clear();
    rows_.clear();
    note_scored_.clear();
    hold_scored_.clear();
    combo_to_regain_life_ = 0;

    if (chart_ == nullptr) {
        return;
    }

    note_row_.assign(chart_->notes.size(), -1);
    note_scored_.assign(chart_->notes.size(), false);
    hold_scored_.assign(chart_->notes.size(), false);

    // Row identity is the exact `Note.beat` (not `time_seconds`), the B5-pinned
    // rule: a stop/warp can map two distinct rows to the same seconds and OpenITG
    // scores them separately. `chart.notes` is beat-sorted, so equal beats are
    // contiguous. Mines never define or join a scoring row.
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
}

void LifeKeeper::consume(const JudgmentEvent& event) {
    if (chart_ == nullptr || constants_ == nullptr) {
        return;
    }
    consume_event(event);
    evaluate_fail();
}

void LifeKeeper::consume(const std::vector<JudgmentEvent>& events) {
    if (chart_ == nullptr || constants_ == nullptr) {
        return;
    }
    // Apply the whole batch before evaluating fail so a hit later in the same
    // frame can still rescue, matching OpenITG's once-per-frame bFailed check.
    for (const JudgmentEvent& event : events) {
        consume_event(event);
    }
    evaluate_fail();
}

void LifeKeeper::consume_event(const JudgmentEvent& event) {
    switch (event.kind) {
        case JudgmentKind::Tap:
            consume_tap_like(event, false);
            break;
        case JudgmentKind::Miss:
            consume_tap_like(event, true);
            break;
        case JudgmentKind::HoldOk:
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollOk:
        case JudgmentKind::RollNg:
            consume_hold_outcome(event); // one delta per hold when the tail resolves
            break;
        case JudgmentKind::HitMine:
            apply(delta_for(event)); // immediate on trigger
            break;
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit:
            // No life change: OpenITG never calls ChangeLife for these (RollHit is
            // visual only; avoided mines only appear in stats). Consuming 0.0
            // through apply() would also falsely count as a win for combo-to-regain.
            break;
    }
}

void LifeKeeper::consume_tap_like(const JudgmentEvent& event, bool miss) {
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
        if (chart_->notes[static_cast<std::size_t>(index)].is_hold_or_roll()) {
            // A missed hold/roll head resolves with no further hold outcome.
            hold_scored_[static_cast<std::size_t>(index)] = true;
        }
    } else if (event.delta_ms > aggregate.last_delta_ms ||
               (event.delta_ms == aggregate.last_delta_ms && event.column > aggregate.last_column)) {
        // Last-tap semantics: greatest offset wins, ties -> later column (B5-pinned).
        aggregate.last_delta_ms = event.delta_ms;
        aggregate.last_column = event.column;
        aggregate.last_window = event.window;
    }

    if (aggregate.judged == aggregate.expected) {
        resolve_row(row);
    }
}

void LifeKeeper::consume_hold_outcome(const JudgmentEvent& event) {
    const int index = event.note_index;
    if (index < 0 || static_cast<std::size_t>(index) >= hold_scored_.size()) {
        return;
    }
    if (!chart_->notes[static_cast<std::size_t>(index)].is_hold_or_roll()) {
        return; // only hold/roll heads produce hold outcomes
    }
    if (hold_scored_[static_cast<std::size_t>(index)]) {
        return; // duplicate guard: one outcome per hold/roll
    }
    hold_scored_[static_cast<std::size_t>(index)] = true;
    apply(delta_for(event));
}

void LifeKeeper::resolve_row(int row) {
    const RowAggregate& aggregate = rows_[static_cast<std::size_t>(row)];
    const TapJudgment score = aggregate.has_miss ? TapJudgment::Miss : aggregate.last_window;
    if (score == TapJudgment::Num) {
        return; // defensive: a completed row always has a graded tap
    }
    apply(delta_for_tap(score)); // exactly one life delta per OpenITG row
}

double LifeKeeper::delta_for_tap(TapJudgment window) const {
    double delta = 0.0;
    switch (window) {
        case TapJudgment::Fantastic: delta = constants_->life.fantastic; break;
        case TapJudgment::Excellent: delta = constants_->life.excellent; break;
        case TapJudgment::Great:     delta = constants_->life.great; break;
        case TapJudgment::Decent:    delta = constants_->life.decent; break;
        case TapJudgment::WayOff:    delta = constants_->life.way_off; break;
        case TapJudgment::Miss:      delta = constants_->life.miss; break;
        case TapJudgment::HitMine:   delta = constants_->life.hit_mine; break;
        case TapJudgment::Num:       return 0.0;
    }
    // Hot downgrade: while the bar is full, WayOff/Miss/mine are forced to the
    // hot penalty (OpenITG `IsHot() && score < TNS_GOOD`, LifeMeterBar.cpp:118-119).
    if (is_hot() && (window == TapJudgment::WayOff || window == TapJudgment::Miss ||
                     window == TapJudgment::HitMine)) {
        return constants_->life.hot_downgrade;
    }
    return delta;
}

double LifeKeeper::delta_for(const JudgmentEvent& event) const {
    switch (event.kind) {
        case JudgmentKind::Tap:
            return delta_for_tap(event.window);
        case JudgmentKind::Miss:
            return delta_for_tap(TapJudgment::Miss);
        case JudgmentKind::HitMine:
            return delta_for_tap(TapJudgment::HitMine);
        case JudgmentKind::HoldOk:
        case JudgmentKind::RollOk:
            return constants_->life.hold_ok;
        case JudgmentKind::HoldNg:
        case JudgmentKind::RollNg:
            // Hot downgrade also covers hold NG (LifeMeterBar.cpp:174-175).
            return is_hot() ? constants_->life.hot_downgrade : constants_->life.hold_ng;
        case JudgmentKind::AvoidedMine:
        case JudgmentKind::RollHit:
            return 0.0;
    }
    return 0.0;
}

void LifeKeeper::apply(double delta) {
    // Mirrors LifeMeterBar::ChangeLife(float) (LifeMeterBar.cpp:202-261), restricted
    // to the behaviors the constants table carries (progressive lifebar and life
    // difficulty are omitted: both are inert at the arcade defaults 0 and 1.0).
    if (constants_->life.merciful_drain && delta < 0.0) {
        delta *= scale(state_.life, 0.0, 1.0, 0.5, 1.0);
    }

    // Combo-to-regain-life (LifeMeterBar.cpp:208-227): a positive delta first
    // consumes one unit of the regain debt and, while debt remains, grants nothing;
    // a loss accumulates the debt by `regen_combo_after_miss` up to
    // `max_regen_combo_after_miss` (never reducing an already-higher debt).
    if (delta >= 0.0) {
        combo_to_regain_life_ = std::max(combo_to_regain_life_ - 1, 0);
        if (combo_to_regain_life_ > 0) {
            delta = 0.0;
        }
    } else {
        const int accumulated = std::min(constants_->life.max_regen_combo_after_miss,
                                         combo_to_regain_life_ + constants_->life.regen_combo_after_miss);
        combo_to_regain_life_ = std::max(combo_to_regain_life_, accumulated);
    }

    // Once failed, all further deltas are zeroed - life is frozen
    // (LifeMeterBar.cpp:229-231).
    if (state_.failed) {
        delta = 0.0;
    }

    // A delta that would cross the fail threshold additionally bumps the regain
    // debt by `regen_combo_after_fail` up to `max_regen_combo_after_fail`
    // (LifeMeterBar.cpp:244-253). OpenITG's ChangeLife does not consult the fail
    // type here, so this applies under Fail-Off too.
    if (state_.life + delta <= 0.0 && state_.life > 0.0) {
        const int accumulated = std::min(constants_->life.max_regen_combo_after_fail,
                                         combo_to_regain_life_ + constants_->life.regen_combo_after_fail);
        combo_to_regain_life_ = std::max(combo_to_regain_life_, accumulated);
    }

    state_.life = std::clamp(state_.life + delta, 0.0, 1.0);
}

void LifeKeeper::evaluate_fail() {
    // Fail detection (FAIL_THRESHOLD=0, IsFailing=life<=0; LifeMeterBar.cpp:18,
    // 290-293) is deferred to the end of the event batch, because OpenITG sets
    // `bFailed` once per frame in ScreenGameplay::Update (ScreenGameplay.cpp:1471-
    // 1493). Fail-Off never sets `failed`.
    if (fail_enabled_ && !state_.failed && state_.life <= 0.0) {
        state_.failed = true;
    }
}

} // namespace td
