#include "gameplay/speed_mod.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

namespace td {

namespace {

std::string_view trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size()) {
        const char c = text[begin];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin) {
        const char c = text[end - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        --end;
    }

    return text.substr(begin, end - begin);
}

bool parse_positive_double(std::string_view text, double& out) {
    if (text.empty()) {
        return false;
    }

    std::string buffer(text);
    double value = 0.0;
    const char* first = buffer.data();
    const char* last = buffer.data() + buffer.size();
    const std::from_chars_result result = std::from_chars(first, last, value);

    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    if (!std::isfinite(value) || value <= 0.0) {
        return false;
    }

    out = value;
    return true;
}

} // namespace

bool parse_speed_mod(std::string_view text, SpeedMod& out) {
    const std::string_view trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }

    SpeedModType type = SpeedModType::XMod;
    std::string_view numeric;

    const char front = trimmed.front();
    const char back = trimmed.back();

    if (back == 'x' || back == 'X') {
        type = SpeedModType::XMod;
        numeric = trimmed.substr(0, trimmed.size() - 1);
    } else if (front == 'x' || front == 'X') {
        type = SpeedModType::XMod;
        numeric = trimmed.substr(1);
    } else if (front == 'c' || front == 'C') {
        type = SpeedModType::CMod;
        numeric = trimmed.substr(1);
    } else if (front == 'm' || front == 'M') {
        type = SpeedModType::MMod;
        numeric = trimmed.substr(1);
    } else {
        return false;
    }

    double value = 0.0;
    if (!parse_positive_double(trim(numeric), value)) {
        return false;
    }

    out.type = type;
    out.value = value;
    return true;
}

double max_chart_bpm(const TimingData& timing) {
    double maximum = 0.0;
    for (const BpmSegment& segment : timing.bpms()) {
        if (std::isfinite(segment.bpm) && segment.bpm > maximum) {
            maximum = segment.bpm;
        }
    }
    return maximum;
}

double resolve_x_speed(const SpeedMod& mod, const TimingData& timing) {
    switch (mod.type) {
        case SpeedModType::XMod:
            return mod.value;
        case SpeedModType::CMod:
            return mod.value;
        case SpeedModType::MMod: {
            const double max_bpm = max_chart_bpm(timing);
            if (max_bpm <= 0.0) {
                std::cerr << "[SpeedMod] Cannot resolve M-mod: no valid BPM in chart timing; "
                             "falling back to 1.0x\n";
                return 1.0;
            }
            return mod.value / max_bpm;
        }
    }
    return 1.0;
}

} // namespace td
