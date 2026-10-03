#include "screens/song_display_text.hpp"

#include "render/bitmap_font.hpp"

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

} // namespace blaze4k
