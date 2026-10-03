#include "screens/song_display_text.hpp"

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
