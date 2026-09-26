#include "timing/music_clock.hpp"
#include <cmath>
#include <iostream>
#include <utility>

namespace td {

MusicClock::MusicClock(Source source)
    : source_(std::move(source)) {}

void MusicClock::set_source(Source source) {
    source_ = std::move(source);
}

void MusicClock::clear_source() {
    source_ = nullptr;
}

bool MusicClock::has_source() const {
    return static_cast<bool>(source_);
}

void MusicClock::set_global_offset_seconds(double offset) {
    if (std::isfinite(offset)) {
        global_offset_seconds_ = offset;
    } else {
        std::cerr << "[MusicClock] Rejected non-finite global offset; keeping previous value\n";
    }
}

double MusicClock::global_offset_seconds() const {
    return global_offset_seconds_;
}

SamplePosition MusicClock::sample_position() const {
    if (!source_) {
        return SamplePosition{};
    }
    return source_();
}

double MusicClock::seconds_from_pcm(uint64_t frames, uint32_t sample_rate) {
    if (sample_rate == 0) {
        return 0.0;
    }
    return static_cast<double>(frames) / static_cast<double>(sample_rate);
}

double MusicClock::apply_offset(double sample_seconds, double offset_seconds) {
    return sample_seconds + offset_seconds;
}

double MusicClock::sample_time_seconds() const {
    SamplePosition position = sample_position();
    return seconds_from_pcm(position.frames, position.sample_rate);
}

double MusicClock::time_seconds() const {
    return apply_offset(sample_time_seconds(), global_offset_seconds_);
}

int64_t MusicClock::time_nanoseconds() const {
    return static_cast<int64_t>(std::llround(time_seconds() * 1e9));
}

} // namespace td
