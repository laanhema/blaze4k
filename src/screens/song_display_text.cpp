#include "screens/song_display_text.hpp"

#include <algorithm>
#include <cmath>

#include "render/bitmap_font.hpp"
#include "render/ttf_font.hpp"

namespace blaze4k {

const std::string& select_display_text(const std::string& native, const std::string& translit,
                                       bool native_covered) {
    if (!native_covered && !translit.empty()) {
        return translit;
    }
    return native;
}

const std::string& song_display_title(const SongMetadata& metadata) {
    return select_display_text(metadata.title, metadata.title_translit,
                               font_covers_text(metadata.title));
}

const std::string& song_display_artist(const SongMetadata& metadata) {
    return select_display_text(metadata.artist, metadata.artist_translit,
                               font_covers_text(metadata.artist));
}

const std::string& song_display_title(const SongMetadata& metadata, const TextRenderer* text,
                                      theme::Font font) {
    if (text == nullptr) {
        return song_display_title(metadata);
    }
    return select_display_text(metadata.title, metadata.title_translit,
                               text->covers_text(metadata.title, font));
}

const std::string& song_display_artist(const SongMetadata& metadata, const TextRenderer* text,
                                       theme::Font font) {
    if (text == nullptr) {
        return song_display_artist(metadata);
    }
    return select_display_text(metadata.artist, metadata.artist_translit,
                               text->covers_text(metadata.artist, font));
}

const std::string& song_display_subtitle(const SongMetadata& metadata) {
    return select_display_text(metadata.subtitle, metadata.subtitle_translit,
                               font_covers_text(metadata.subtitle));
}

const std::string& song_display_subtitle(const SongMetadata& metadata, const TextRenderer* text,
                                         theme::Font font) {
    if (text == nullptr) {
        return song_display_subtitle(metadata);
    }
    return select_display_text(metadata.subtitle, metadata.subtitle_translit,
                               text->covers_text(metadata.subtitle, font));
}

TitleSubtitleText title_subtitle_text(std::string_view title, std::string_view subtitle) {
    if (title.empty()) {
        return TitleSubtitleText{subtitle, {}};
    }
    return TitleSubtitleText{title, subtitle};
}

namespace {

float finite_nonneg(float v) {
    return std::isfinite(v) && v > 0.0f ? v : 0.0f;
}

} // namespace

TitleSubtitleFit fit_title_subtitle(float title_w, float subtitle_w, float gap, float budget) {
    title_w = finite_nonneg(title_w);
    subtitle_w = finite_nonneg(subtitle_w);
    gap = finite_nonneg(gap);
    budget = finite_nonneg(budget);
    if (budget == 0.0f) {
        return TitleSubtitleFit{};
    }
    if (subtitle_w == 0.0f || budget <= gap) {
        return TitleSubtitleFit{std::min(title_w, budget), 0.0f};
    }
    if (title_w + gap + subtitle_w <= budget) {
        return TitleSubtitleFit{title_w, subtitle_w};
    }
    const float avail = budget - gap;
    const float subtitle_max_w =
        std::min(subtitle_w, std::max(avail - title_w, avail * kSubtitleMinShare));
    const float title_max_w = std::max(0.0f, std::min(title_w, avail - subtitle_max_w));
    return TitleSubtitleFit{title_max_w, subtitle_max_w};
}

namespace {

bool shows_edit_name(const Chart& chart) {
    return !chart.description.empty() &&
           resolve_difficulty(chart.difficulty, chart.description, chart.meter) ==
               StepsDifficulty::Edit;
}

} // namespace

std::string chart_display_label(const Chart& chart) {
    return shows_edit_name(chart) ? chart.description : chart.difficulty;
}

std::string chart_display_label(const Chart& chart, std::size_t max_name_cells) {
    if (shows_edit_name(chart)) {
        return truncate_to_cells(chart_display_label(chart), max_name_cells);
    }
    return chart_display_label(chart);
}

} // namespace blaze4k
