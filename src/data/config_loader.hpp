#pragma once

#include <filesystem>
#include <string>

#include "data/config.hpp"

namespace blaze4k {

// Explicit outcome of a config load, so callers route the message without
// inspecting its wording. `UsedDefaults` means the file was missing or unusable
// and the compiled defaults were returned.
enum class ConfigLoadStatus { LoadedFromFile, UsedDefaults };

// Loads the player config from `path`. Never throws.
//
// - Missing file -> defaults, `UsedDefaults`, "[Config] ... not found" message.
// - File larger than 1 MiB -> defaults + warning (untrusted-input cap).
// - Unparseable / non-object document -> defaults + warning.
// - Well-formed document with an invalid field -> that field keeps its default
//   (numeric fields are clamped to range) and a warning is appended; the
//   remaining valid fields still load. Unknown keys are ignored.
//
// When `message` is non-null it is set to a single "[Config] ..." line; when
// `status` is non-null it records which outcome occurred.
[[nodiscard]] GameConfig load_config(const std::filesystem::path& path,
                                     std::string* message = nullptr,
                                     ConfigLoadStatus* status = nullptr);

// Saves `config` to `path` atomically: parent directories are created, the JSON
// is written to a unique `<path>.tmp.<suffix>` in the same directory, then
// renamed over `path`. On any failure the temp file is removed and the previous
// file is left untouched; returns false with a warning message. Never throws.
[[nodiscard]] bool save_config(const std::filesystem::path& path, const GameConfig& config,
                               std::string* message = nullptr);

} // namespace blaze4k
