#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "timing/judgment_constants.hpp"

namespace td {

// Explicit outcome of a load, so callers can route the message without
// inspecting its wording. `UsedDefaults` means every candidate was missing or
// invalid and the compiled table was returned.
enum class ConstantsLoadStatus { LoadedFromFile, UsedDefaults };

// Loads constants from `path`. On any failure (missing, unreadable, malformed,
// or invalid) returns the compiled defaults. When `message` is non-null it is
// set to a single "[JudgmentConstants] ..." line describing success or the
// fallback reason; when `status` is non-null it records which outcome occurred.
// Never throws.
[[nodiscard]] JudgmentConstants load_judgment_constants(
    const std::filesystem::path& path, std::string* message = nullptr,
    ConstantsLoadStatus* status = nullptr);

// Startup discovery order; the first existing file wins, otherwise compiled
// defaults with a fallback message.
[[nodiscard]] JudgmentConstants load_judgment_constants_from_candidates(
    const std::vector<std::filesystem::path>& candidates, std::string* message = nullptr,
    ConstantsLoadStatus* status = nullptr);

} // namespace td
