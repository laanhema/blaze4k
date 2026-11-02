#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "chart/chart.hpp"
#include "chart/song_metadata.hpp"
#include "render/theme.hpp"

namespace blaze4k {

class TextRenderer;

// Edit charts show their description (SM5 StepsDisplay.cpp:199-202, OpenITG
// DifficultyMeter.cpp:158).
[[nodiscard]] const std::string& select_display_text(const std::string& native,
                                                     const std::string& translit,
                                                     bool native_covered);

[[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata);
[[nodiscard]] const std::string& song_display_artist(const SongMetadata& metadata);

[[nodiscard]] const std::string& song_display_title(const SongMetadata& metadata,
                                                    const TextRenderer* text, theme::Font font);
[[nodiscard]] const std::string& song_display_artist(const SongMetadata& metadata,
                                                     const TextRenderer* text, theme::Font font);

// SUBTITLE / SUBTITLETRANSLIT (SM5 Song::GetDisplaySubTitle).
[[nodiscard]] const std::string& song_display_subtitle(const SongMetadata& metadata);
[[nodiscard]] const std::string& song_display_subtitle(const SongMetadata& metadata,
                                                       const TextRenderer* text, theme::Font font);

// Title and subtitle drawn on one line with no added brackets (SM5 Song::GetDisplayFullTitle).
struct TitleSubtitleFit {
    float title_max_w = 0.0f;
    float subtitle_max_w = 0.0f;
};

struct TitleSubtitleText {
    std::string_view title;
    std::string_view subtitle;
};
[[nodiscard]] TitleSubtitleText title_subtitle_text(std::string_view title,
                                                    std::string_view subtitle);

inline constexpr float kSubtitleMinShare = 0.4f;

[[nodiscard]] TitleSubtitleFit fit_title_subtitle(float title_w, float subtitle_w, float gap,
                                                  float budget);

inline constexpr std::size_t kMinDifficultyLabelCells = 9;

// Chart difficulty display label (SM5 Steps::IsAnEdit for Edit charts).
[[nodiscard]] std::string chart_display_label(const Chart& chart, std::size_t max_name_cells);

[[nodiscard]] std::string chart_display_label(const Chart& chart);

} // namespace blaze4k
