#include "audio/preview_player.hpp"

#include <algorithm>

namespace blaze4k {

PreviewPlayer::PreviewPlayer() = default;

PreviewPlayer::PreviewPlayer(IAudioStream& stream) : stream_override_(&stream) {}

void PreviewPlayer::request(const std::string& path, double start_seconds, double length_seconds) {
    stream().stop();
    path_ = path;
    start_ = start_seconds > 0.0 ? start_seconds : 0.0;
    length_ = length_seconds > 0.0 ? length_seconds : 0.0;
    timer_ = 0.0;

    if (path_.empty()) {
        state_ = PreviewState::Idle;
        return;
    }
    state_ = PreviewState::Waiting;
}

void PreviewPlayer::update(double fixed_dt) {
    if (state_ == PreviewState::Idle) {
        return;
    }

    if (state_ == PreviewState::Waiting) {
        timer_ += fixed_dt;
        if (timer_ < delay_seconds_) {
            return;
        }

        ++load_attempts_;
        if (!stream().load(path_)) {
            state_ = PreviewState::Idle;
            return;
        }

        stream().set_volume(volume_);
        if (start_ > 0.0) {
            stream().seek_seconds(start_);
        }
        stream().play();
        state_ = PreviewState::Active;
        return;
    }

    // Active: loop the [start_, start_ + length_) window while it plays.
    if (length_ > 0.0 && stream().is_playing() &&
        stream().get_position_seconds() >= start_ + length_) {
        stream().seek_seconds(start_);
    }
}

void PreviewPlayer::stop() {
    stream().stop();
    state_ = PreviewState::Idle;
    path_.clear();
    timer_ = 0.0;
}

void PreviewPlayer::set_volume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    stream().set_volume(volume_);
}

} // namespace blaze4k
