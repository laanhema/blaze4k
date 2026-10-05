#include "audio/sound_stream.hpp"
#include "audio/audio_engine.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <miniaudio.h>

namespace blaze4k {

namespace {

uint64_t monotonic_now_or_zero() {
    const MonotonicNowFn now = AudioEngine::instance().now_fn();
    return now != nullptr ? now() : 0;
}

} // namespace

SoundStream::SoundStream()
    : sound_(std::make_unique<ma_sound>()) {}

SoundStream::~SoundStream() {
    unload();
}

SoundStream::SoundStream(SoundStream&& other) noexcept
    : sound_(std::move(other.sound_)),
      is_loaded_(other.is_loaded_),
      sample_rate_(other.sample_rate_),
      last_position_(other.last_position_),
      tap_(std::move(other.tap_)),
      tap_attached_(other.tap_attached_),
      interp_(other.interp_),
      engine_rate_(other.engine_rate_),
      length_frames_(other.length_frames_) {
    other.is_loaded_ = false;
    other.last_position_ = 0.0;
    other.tap_attached_ = false;
    other.interp_ = ClockInterpolator{};
}

SoundStream& SoundStream::operator=(SoundStream&& other) noexcept {
    if (this != &other) {
        unload();
        sound_ = std::move(other.sound_);
        is_loaded_ = other.is_loaded_;
        sample_rate_ = other.sample_rate_;
        last_position_ = other.last_position_;
        tap_ = std::move(other.tap_);
        tap_attached_ = other.tap_attached_;
        interp_ = other.interp_;
        engine_rate_ = other.engine_rate_;
        length_frames_ = other.length_frames_;

        other.is_loaded_ = false;
        other.last_position_ = 0.0;
        other.tap_attached_ = false;
        other.interp_ = ClockInterpolator{};
    }
    return *this;
}

bool SoundStream::load(const std::string& filepath) {
    unload();

    if (!sound_) {
        sound_ = std::make_unique<ma_sound>();
    }

    AudioEngine& engine = AudioEngine::instance();
    if (!engine.is_initialized()) {
        if (!engine.init()) {
            std::cerr << "[SoundStream] Failed to initialize AudioEngine when loading " << filepath << "\n";
            return false;
        }
    }

    ma_result result = ma_sound_init_from_file(
        engine.raw_engine(),
        filepath.c_str(),
        MA_SOUND_FLAG_DECODE,
        nullptr,
        nullptr,
        sound_.get()
    );

    if (result != MA_SUCCESS) {
        std::cerr << "[SoundStream] Failed to load audio file '" << filepath
                  << "' (error code: " << static_cast<int>(result) << ")\n";
        return false;
    }

    ma_uint32 sample_rate = 0;
    if (ma_sound_get_data_format(sound_.get(), nullptr, nullptr, &sample_rate, nullptr, 0) == MA_SUCCESS && sample_rate > 0) {
        sample_rate_ = sample_rate;
    } else {
        sample_rate_ = ma_engine_get_sample_rate(engine.raw_engine());
    }

    is_loaded_ = true;
    last_position_ = 0.0;
    reset_interpolation(0);
    return true;
}

void SoundStream::unload() {
    if (tap_attached_ && tap_) {
        // Before ma_sound_uninit: after detach the audio thread cannot reach the sound.
        AudioEngine::instance().detach_clock_tap(tap_.get());
        tap_->sound.store(nullptr, std::memory_order_release);
    }
    tap_attached_ = false;
    if (is_loaded_ && sound_) {
        ma_sound_uninit(sound_.get());
        is_loaded_ = false;
        last_position_ = 0.0;
    }
    interp_.reset(0, 0);
    length_frames_ = 0;
}

void SoundStream::reset_interpolation(uint64_t floor_frames) {
    // Taken AFTER the miniaudio call: anchors captured before it are ignored.
    interp_.reset(tap_attached_ ? monotonic_now_or_zero() : 0, floor_frames);
}

bool SoundStream::enable_clock_interpolation() {
    if (!is_loaded_ || !sound_) {
        return false;
    }
    AudioEngine& engine = AudioEngine::instance();
    if (!engine.is_initialized() || engine.now_fn() == nullptr) {
        return false;
    }
    if (!tap_) {
        tap_ = std::make_unique<ClockTap>();
    }
    tap_->sound.store(sound_.get(), std::memory_order_release);
    if (!engine.attach_clock_tap(tap_.get())) {
        return false;
    }
    const bool was_attached = tap_attached_;
    tap_attached_ = true;
    engine_rate_ = engine.engine_sample_rate();
    ma_uint64 length = 0;
    length_frames_ =
        ma_sound_get_length_in_pcm_frames(sound_.get(), &length) == MA_SUCCESS ? length : 0;
    if (!was_attached) {
        reset_interpolation(get_raw_position_frames());
    }
    return true;
}

bool SoundStream::play() {
    if (!is_loaded_ || !sound_) {
        return false;
    }
    const bool ok = ma_sound_start(sound_.get()) == MA_SUCCESS;
    reset_interpolation(get_raw_position_frames());
    return ok;
}

void SoundStream::pause() {
    if (is_loaded_ && sound_) {
        ma_sound_stop(sound_.get());
    }
}

void SoundStream::resume() {
    if (is_loaded_ && sound_) {
        ma_sound_start(sound_.get());
        reset_interpolation(get_raw_position_frames());
    }
}

void SoundStream::stop() {
    if (is_loaded_ && sound_) {
        ma_sound_stop(sound_.get());
        ma_sound_seek_to_pcm_frame(sound_.get(), 0);
        last_position_ = 0.0;
        reset_interpolation(0);
    }
}

bool SoundStream::seek_seconds(double seconds) {
    if (!is_loaded_ || !sound_) {
        return false;
    }
    double clamped = std::max(0.0, seconds);
    auto target_frame = static_cast<ma_uint64>(clamped * static_cast<double>(sample_rate_));
    ma_result res = ma_sound_seek_to_pcm_frame(sound_.get(), target_frame);
    if (res == MA_SUCCESS) {
        last_position_ = clamped;
        reset_interpolation(static_cast<uint64_t>(target_frame));
        return true;
    }
    return false;
}

double SoundStream::get_position_seconds() const {
    if (!is_loaded_ || !sound_ || sample_rate_ == 0) {
        return 0.0;
    }
    return static_cast<double>(get_position_frames()) / static_cast<double>(sample_rate_);
}

uint64_t SoundStream::get_position_frames() const {
    return get_timed_position_frames().frames;
}

TimedFrames SoundStream::get_timed_position_frames() const {
    if (!is_loaded_ || !sound_ || sample_rate_ == 0) {
        return TimedFrames{};
    }
    const MonotonicNowFn now_fn = AudioEngine::instance().now_fn();
    if (!tap_attached_ || !tap_ || now_fn == nullptr) {
        return TimedFrames{get_raw_position_frames(), 0};
    }
    // Order: now, then the anchor, then the raw cursor (pinned in the #81 plan).
    InterpolationInput in;
    in.now_ns = now_fn();
    in.anchor_valid = tap_->slot.try_read(in.anchor);
    in.raw_cursor_frames = get_raw_position_frames();
    in.playing = is_playing();
    in.source_rate = sample_rate_;
    in.engine_rate = engine_rate_;
    in.length_frames = length_frames_;
    return TimedFrames{interp_.update(in), in.now_ns};
}

double SoundStream::get_raw_position_seconds() const {
    if (!is_loaded_ || !sound_ || sample_rate_ == 0) {
        return 0.0;
    }
    return static_cast<double>(get_raw_position_frames()) / static_cast<double>(sample_rate_);
}

uint64_t SoundStream::get_raw_position_frames() const {
    if (!is_loaded_ || !sound_ || sample_rate_ == 0) {
        return 0;
    }

    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(sound_.get(), &cursor) == MA_SUCCESS) {
        double pos = static_cast<double>(cursor) / static_cast<double>(sample_rate_);
        if (is_playing() && pos < last_position_) {
            return static_cast<uint64_t>(std::llround(last_position_ * static_cast<double>(sample_rate_)));
        }
        last_position_ = pos;
        return cursor;
    }
    return static_cast<uint64_t>(std::llround(last_position_ * static_cast<double>(sample_rate_)));
}

double SoundStream::get_length_seconds() const {
    if (!is_loaded_ || !sound_) {
        return 0.0;
    }

    ma_uint64 length_frames = 0;
    if (ma_sound_get_length_in_pcm_frames(sound_.get(), &length_frames) == MA_SUCCESS && sample_rate_ > 0) {
        return static_cast<double>(length_frames) / static_cast<double>(sample_rate_);
    }
    return 0.0;
}

bool SoundStream::is_playing() const {
    if (!is_loaded_ || !sound_) {
        return false;
    }
    return ma_sound_is_playing(sound_.get()) != 0;
}

void SoundStream::set_volume(float volume) {
    if (is_loaded_ && sound_) {
        ma_sound_set_volume(sound_.get(), volume);
    }
}

float SoundStream::get_volume() const {
    if (is_loaded_ && sound_) {
        return ma_sound_get_volume(sound_.get());
    }
    return 1.0f;
}

} // namespace blaze4k
