#include "gameplay/assist_tick_schedule.hpp"

#include <algorithm>

namespace td {

void AssistTickSchedule::reset(const Chart& chart) {
    times_.clear();
    next_ = 0;
    for (const Note& note : chart.notes) {
        if (note.type != NoteType::Mine) {
            times_.push_back(note.time_seconds);
        }
    }
    std::sort(times_.begin(), times_.end());
    // Notes on one row share a beat and therefore an identical time_seconds.
    times_.erase(std::unique(times_.begin(), times_.end()), times_.end());
}

void AssistTickSchedule::collect_due(double horizon_seconds, std::vector<double>& out) {
    while (next_ < times_.size() && times_[next_] <= horizon_seconds) {
        out.push_back(times_[next_]);
        ++next_;
    }
}

} // namespace td
