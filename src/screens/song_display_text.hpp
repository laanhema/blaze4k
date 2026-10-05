#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "chart/chart.hpp"
#include "chart/song_metadata.hpp"
#include "render/theme.hpp"

namespace blaze4k {

class TextRenderer;

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

// The same choice through TrueType coverage (#94): the native text is covered
// when `text` has a native glyph for every visible code point in `font`
// (TextRenderer::covers_text). A null `text` falls back to the bitmap rule
// above. The returned reference aliases `metadata`.
[[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata,
                                                    const TextRenderer* text, theme::Font font);
[[nodiscard]] const std::string& song_display_artist(const SongMetadata& metadata,
                                                     const TextRenderer* text, theme::Font font);

// SUBTITLE / SUBTITLETRANSLIT (SM5 Song::GetDisplaySubTitle), chosen independently
// of the title: the same coverage rule as title and artist, applied to the
// subtitle on its own. Bitmap and TrueType overloads as above (a null `text`
// falls back to the bitmap rule). The returned reference aliases `metadata`.
[[nodiscard]] const std::string& song_display_subtitle(const SongMetadata& metadata);
[[nodiscard]] const std::string& song_display_subtitle(const SongMetadata& metadata,
                                                       const TextRenderer* text, theme::Font font);

// Title and subtitle are drawn on one line, "Title Subtitle", with no added
// brackets (SM5 Song::GetDisplayFullTitle): the simfile's subtitle already
// carries its own -...-, (...) or ~...~. The two share one width budget.
struct TitleSubtitleFit {
    float title_max_w = 0.0f;
    float subtitle_max_w = 0.0f;
};

// The text drawn in the title place and after it. An empty display title gives
// its place to the subtitle: the subtitle is drawn as the title (title style, no
// gap) and nothing follows. Otherwise both pass through unchanged. The views
// alias the arguments.
struct TitleSubtitleText {
    std::string_view title;
    std::string_view subtitle;
};
[[nodiscard]] TitleSubtitleText title_subtitle_text(std::string_view title,
                                                    std::string_view subtitle);

// When both do not fit, the subtitle keeps at least this share of the budget
// (after the gap): it is what tells same-title songs apart.
inline constexpr float kSubtitleMinShare = 0.4f;

// Splits `budget` between a title `title_w` wide and a subtitle `subtitle_w`
// wide, `gap` apart (any one unit). Non-finite or negative inputs count as 0.
//  - budget 0 -> {0, 0}; no subtitle, or budget <= gap -> {min(title, budget), 0}
//  - both fit (title + gap + subtitle <= budget) -> {title_w, subtitle_w}
//  - otherwise, avail = budget - gap:
//      subtitle = min(subtitle_w, max(avail - title_w, avail * kSubtitleMinShare))
//      title    = min(title_w, avail - subtitle)
// With a subtitle, title + gap + subtitle <= budget.
[[nodiscard]] TitleSubtitleFit fit_title_subtitle(float title_w, float subtitle_w, float gap,
                                                  float budget);

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

// The same label, untruncated: callers that truncate by measured width (the
// Cabinet select screen, #94) shorten it themselves.
[[nodiscard]] std::string chart_display_label(const Chart& chart);

} // namespace blaze4k
