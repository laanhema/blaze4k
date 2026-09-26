#include "chart/timing_data.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cmath>

namespace td {

namespace {

void trim(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
}

} // namespace

TimingData::TimingData() {
    clear();
}

void TimingData::clear() {
    offset_ = 0.0;
    has_exotic_timing_ = false;
    bpms_.clear();
    stops_.clear();
    bpms_.push_back({0.0, 120.0});
}

void TimingData::add_bpm(double beat, double bpm) {
    if (bpm <= 0.0) {
        has_exotic_timing_ = true;
        std::cerr << "[TimingData] Warning: Invalid non-positive BPM " << bpm << " at beat " << beat << "\n";
        return;
    }
    bpms_.push_back({beat, bpm});
    ensure_sorted_and_valid();
}

void TimingData::add_stop(double beat, double length_seconds) {
    if (length_seconds < 0.0) {
        has_exotic_timing_ = true;
        std::cerr << "[TimingData] Warning: Negative stop (warp) " << length_seconds << "s at beat " << beat << "\n";
        return;
    }
    if (length_seconds == 0.0) {
        return;
    }
    stops_.push_back({beat, length_seconds});
    ensure_sorted_and_valid();
}

void TimingData::ensure_sorted_and_valid() {
    std::sort(bpms_.begin(), bpms_.end(), [](const BpmSegment& a, const BpmSegment& b) {
        return a.beat < b.beat;
    });

    if (bpms_.empty() || bpms_.front().beat > 0.0) {
        bpms_.insert(bpms_.begin(), {0.0, 120.0});
    }

    std::sort(stops_.begin(), stops_.end(), [](const StopSegment& a, const StopSegment& b) {
        return a.beat < b.beat;
    });
}

bool TimingData::parse_bpms_string(std::string_view bpms_str) {
    bpms_.clear();
    std::string str(bpms_str);
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, ',')) {
        trim(token);
        if (token.empty()) {
            continue;
        }

        size_t eq_pos = token.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        try {
            double beat = std::stod(token.substr(0, eq_pos));
            double bpm = std::stod(token.substr(eq_pos + 1));
            if (!std::isfinite(beat) || !std::isfinite(bpm) || beat < 0.0) {
                has_exotic_timing_ = true;
                std::cerr << "[TimingData] Warning: Invalid or non-finite BPM segment.\n";
            } else if (bpm <= 0.0) {
                has_exotic_timing_ = true;
                std::cerr << "[TimingData] Warning: Non-positive BPM " << bpm << " detected.\n";
            } else if (bpms_.size() < 5000) {
                bpms_.push_back({beat, bpm});
            }
        } catch (...) {
            std::cerr << "[TimingData] Warning: Failed parsing BPM token '" << token << "'\n";
        }
    }

    ensure_sorted_and_valid();
    return !bpms_.empty();
}

bool TimingData::parse_stops_string(std::string_view stops_str) {
    stops_.clear();
    std::string str(stops_str);
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, ',')) {
        trim(token);
        if (token.empty()) {
            continue;
        }

        size_t eq_pos = token.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        try {
            double beat = std::stod(token.substr(0, eq_pos));
            double len = std::stod(token.substr(eq_pos + 1));
            if (!std::isfinite(beat) || !std::isfinite(len) || beat < 0.0) {
                has_exotic_timing_ = true;
                std::cerr << "[TimingData] Warning: Invalid or non-finite Stop segment.\n";
            } else if (len < 0.0) {
                has_exotic_timing_ = true;
                std::cerr << "[TimingData] Warning: Negative stop (warp) " << len << "s detected.\n";
            } else if (len > 0.0 && stops_.size() < 5000) {
                stops_.push_back({beat, len});
            }
        } catch (...) {
            std::cerr << "[TimingData] Warning: Failed parsing Stop token '" << token << "'\n";
        }
    }

    ensure_sorted_and_valid();
    return true;
}

double TimingData::beat_to_seconds(double beat) const {
    if (bpms_.empty()) {
        return -offset_;
    }

    if (beat <= 0.0) {
        double first_bpm = bpms_.front().bpm;
        return -offset_ + (beat / first_bpm) * 60.0;
    }

    double elapsed_time = -offset_;
    double current_beat = 0.0;

    for (size_t i = 0; i < bpms_.size(); ++i) {
        double segment_start = bpms_[i].beat;
        double next_segment_start = (i + 1 < bpms_.size()) ? bpms_[i + 1].beat : 1e12;
        double active_bpm = bpms_[i].bpm;

        if (beat <= segment_start) {
            break;
        }

        double end_beat = std::min(beat, next_segment_start);
        if (end_beat > current_beat) {
            elapsed_time += ((end_beat - current_beat) / active_bpm) * 60.0;
            current_beat = end_beat;
        }

        if (current_beat >= beat) {
            break;
        }
    }

    // Add all stops that occur strictly before the target beat
    for (const auto& stop : stops_) {
        if (stop.beat < beat) {
            elapsed_time += stop.length_seconds;
        } else {
            break;
        }
    }

    return elapsed_time;
}

double TimingData::seconds_to_beat(double seconds) const {
    if (bpms_.empty()) {
        return 0.0;
    }

    double start_time = -offset_;
    if (seconds <= start_time) {
        double first_bpm = bpms_.front().bpm;
        return ((seconds - start_time) / 60.0) * first_bpm;
    }

    // Unified simulation combining BPM changes and stops in chronological beat order
    struct Event {
        enum Type { Bpm, Stop } type;
        double beat;
        double value; // bpm value or stop length
    };

    std::vector<Event> events;
    for (size_t i = 1; i < bpms_.size(); ++i) {
        events.push_back({Event::Bpm, bpms_[i].beat, bpms_[i].bpm});
    }
    for (const auto& s : stops_) {
        events.push_back({Event::Stop, s.beat, s.length_seconds});
    }

    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.beat != b.beat) return a.beat < b.beat;
        // If at same beat, BPM change takes precedence before stop
        return a.type < b.type;
    });

    double current_time = start_time;
    double current_beat = 0.0;
    double current_bpm = bpms_.front().bpm;

    for (const auto& ev : events) {
        if (ev.beat > current_beat) {
            double dt = ((ev.beat - current_beat) / current_bpm) * 60.0;
            if (current_time + dt >= seconds) {
                return current_beat + ((seconds - current_time) / 60.0) * current_bpm;
            }
            current_time += dt;
            current_beat = ev.beat;
        }

        if (ev.type == Event::Bpm) {
            current_bpm = ev.value;
        } else if (ev.type == Event::Stop) {
            if (seconds <= current_time + ev.value) {
                // Currently in the middle of a stop: beat is frozen at this beat
                return current_beat;
            }
            current_time += ev.value;
        }
    }

    // Extrapolate beyond all events
    double remaining_time = seconds - current_time;
    return current_beat + (remaining_time / 60.0) * current_bpm;
}

double TimingData::get_bpm_at_beat(double beat) const {
    if (bpms_.empty()) {
        return 120.0;
    }
    double bpm = bpms_.front().bpm;
    for (const auto& seg : bpms_) {
        if (seg.beat <= beat) {
            bpm = seg.bpm;
        } else {
            break;
        }
    }
    return bpm;
}

double TimingData::get_bpm_at_seconds(double seconds) const {
    return get_bpm_at_beat(seconds_to_beat(seconds));
}

bool TimingData::is_in_stop(double seconds) const {
    for (const auto& s : stops_) {
        double stop_start_time = beat_to_seconds(s.beat);
        double stop_end_time = stop_start_time + s.length_seconds;
        if (seconds >= stop_start_time && seconds < stop_end_time) {
            return true;
        }
    }
    return false;
}

} // namespace td
