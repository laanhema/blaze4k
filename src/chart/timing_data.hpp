#pragma once

#include <vector>
#include <string_view>

namespace blaze4k {

struct BpmSegment {
    double beat = 0.0;
    double bpm = 120.0;
};

struct StopSegment {
    double beat = 0.0;
    double length_seconds = 0.0;
};

class TimingData {
public:
    TimingData();

    void set_offset(double offset_seconds) { offset_ = offset_seconds; }
    [[nodiscard]] double offset() const { return offset_; }

    void add_bpm(double beat, double bpm);
    void add_stop(double beat, double length_seconds);

    bool parse_bpms_string(std::string_view bpms_str);
    bool parse_stops_string(std::string_view stops_str);

    [[nodiscard]] double beat_to_seconds(double beat) const;
    [[nodiscard]] double seconds_to_beat(double seconds) const;

    [[nodiscard]] double get_bpm_at_beat(double beat) const;
    [[nodiscard]] double get_bpm_at_seconds(double seconds) const;
    [[nodiscard]] bool is_in_stop(double seconds) const;

    [[nodiscard]] const std::vector<BpmSegment>& bpms() const { return bpms_; }
    [[nodiscard]] const std::vector<StopSegment>& stops() const { return stops_; }
    [[nodiscard]] bool has_exotic_timing() const { return has_exotic_timing_; }
    void set_exotic_timing(bool exotic = true) { has_exotic_timing_ = exotic; }

    void clear();

private:
    void ensure_sorted_and_valid();

    double offset_ = 0.0;
    bool has_exotic_timing_ = false;
    std::vector<BpmSegment> bpms_;
    std::vector<StopSegment> stops_;
};

} // namespace blaze4k
