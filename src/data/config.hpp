#pragma once

#include <string>
#include <utility>
#include <vector>

namespace blaze4k {

// Local player configuration (PRD section 7.6 / section 9). This is the pure
// in-memory model C3 (song select), C4 (options), C5 (offset calibration) and C7
// (results) mutate; persistence lives in src/data/config_loader.*.
//
// This module must remain pure: it includes only standard-library string/vector
// headers, with no platform, audio, filesystem, or JSON dependencies (mirroring
// src/timing/judgment_constants.hpp). `speed_mod`/`scroll` are stored as the
// human strings accepted by the gameplay layer so this header never includes
// src/gameplay/.
inline constexpr int kConfigVersion = 1;

inline constexpr int kMinWindowDimension = 320;
inline constexpr int kMaxWindowDimension = 16384;

struct VideoSettings {
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool fullscreen = false;
};

// #81: audio engine period request (`audio.period_size_frames`). Mirrors
// blaze4k::kDefaultAudioPeriodFrames in src/audio/audio_engine.hpp (tied by a
// static_assert in src/main.cpp; this header stays audio-free).
inline constexpr int kDefaultAudioPeriodFramesConfig = 480;
inline constexpr int kMinAudioPeriodFrames = 128;
inline constexpr int kMaxAudioPeriodFrames = 4096;

struct AudioSettings {
    double master_volume = 1.0;
    double music_volume = 1.0;
    double preview_volume = 0.8;
    double ui_volume = 1.0;
    // 0 = miniaudio/backend default; otherwise clamped to [128, 4096] frames.
    // Raise this (e.g. 960, or 0) if you hear crackles or underruns; applies on
    // next start.
    int period_size_frames = kDefaultAudioPeriodFramesConfig;
};

struct OffsetSettings {
    double global_offset_seconds = 0.0;
};

struct GameplaySettings {
    std::string speed_mod = "1x";
    std::string scroll = "up";
    bool fail_enabled = true;
    bool assist_tick = false; // timing-practice tick on every note row
};

// `action -> [binding names]`. C2 models and persists the bindings but does not
// apply them (remapping is a later ticket), so the values are opaque strings.
using InputBinding = std::pair<std::string, std::vector<std::string>>;

[[nodiscard]] std::vector<InputBinding> default_key_bindings();
[[nodiscard]] std::vector<InputBinding> default_gamepad_bindings();

struct InputSettings {
    std::vector<InputBinding> key_bindings = default_key_bindings();
    std::vector<InputBinding> gamepad_bindings = default_gamepad_bindings();

    static InputSettings defaults() { return InputSettings{}; }
};

struct GameConfig {
    int version = kConfigVersion;
    VideoSettings video;
    AudioSettings audio;
    OffsetSettings offset;
    GameplaySettings gameplay;
    InputSettings input;
};

// Pure, side-effect-free validation of the documented ranges (volumes and window
// dimensions in range, `speed_mod` non-empty, `scroll` in {"up","down"}). The
// loader additionally clamps numeric fields to these ranges as it reads them, so
// a config loaded from disk is always valid; this predicate is the shared range
// authority and is directly unit-testable. Never throws.
[[nodiscard]] bool validate_game_config(const GameConfig& config, std::string* error = nullptr);

} // namespace blaze4k
