#pragma once

#include <string>

namespace blaze4k {

struct SongMetadata {
    std::string title;
    std::string subtitle;
    std::string artist;
    std::string title_translit;
    std::string subtitle_translit;
    std::string artist_translit;
    std::string genre;
    std::string credit;
    std::string banner_path;
    std::string background_path;
    std::string cdtitle_path;
    std::string music_path;
    std::string lyrics_path;
    double offset = 0.0;
    double sample_start = 0.0;
    double sample_length = 12.0;
    std::string selectable = "YES";
};

} // namespace blaze4k
