#pragma once

#include <string>

#include "chart/song_metadata.hpp"

namespace blaze4k {

// Display text for song metadata. Draw-only: identity (high-score keys,
// library lookup) and log lines keep the raw native title.

// Pure choice: `translit` when the native text is not fully drawable by the
// active font and a translit exists; otherwise `native`. The result aliases
// one of the arguments.
[[nodiscard]] const std::string& select_display_text(const std::string& native,
                                                     const std::string& translit,
                                                     bool native_covered);

// TITLE / TITLETRANSLIT and ARTIST / ARTISTTRANSLIT through the active font's
// coverage. The returned reference aliases `metadata`, so do not keep it past
// the song's lifetime.
[[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata);
[[nodiscard]] const std::string& song_display_artist(const SongMetadata& metadata);

} // namespace blaze4k
