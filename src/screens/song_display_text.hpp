#pragma once

#include <cstddef>
#include <string>

#include "chart/chart.hpp"
#include "chart/song_metadata.hpp"

namespace blaze4k {

// Display text for song metadata and chart difficulty labels. Draw-only:
// identity (high-score keys, library lookup) and log lines keep the raw native
// title and the raw passthrough difficulty label. Edit charts show their
// description (SM5 StepsDisplay.cpp:199-202, OpenITG DifficultyMeter.cpp:158).

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

// Narrowest name budget for a difficulty label, in cells: the width of
// "Challenge", the longest standard label, so an edit name is never shortened
// below what a standard label already takes. Shared by select and results.
inline constexpr std::size_t kMinDifficultyLabelCells = 9;

// Difficulty label to draw for `chart`: for an Edit chart (the resolved
// difficulty, SM5 Steps::IsAnEdit) with a non-empty description, that
// description shortened to `max_name_cells` cells (truncate_to_cells);
// otherwise the passthrough `chart.difficulty`, unchanged and untruncated.
// Chart names have no translit, so the text is drawn natively.
[[nodiscard]] std::string chart_display_label(const Chart& chart, std::size_t max_name_cells);

} // namespace blaze4k
