#pragma once

#include <cstddef>
#include <vector>

#include "chart/chart.hpp"

namespace td {

// Pure assist-tick timeline: one tick per chart row holding a tap, hold head or
// roll head (mines never tick; a jump/hand ticks once). Mirrors StepMania's
// ScreenGameplay::PlayTicks row rule. Time-parameterized like NoteField: the
// caller passes raw stream time (music clock without the global offset), so
// no wall-clock or frame timing enters here.
class AssistTickSchedule {
public:
    void reset(const Chart& chart);

    // Appends every not-yet-emitted tick time <= `horizon_seconds` to `out`, in
    // ascending order, and advances past them (each tick is emitted once).
    void collect_due(double horizon_seconds, std::vector<double>& out);

    [[nodiscard]] const std::vector<double>& tick_times() const { return times_; }
    [[nodiscard]] std::size_t emitted() const { return next_; }

private:
    std::vector<double> times_;
    std::size_t next_ = 0;
};

} // namespace td
