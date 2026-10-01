#pragma once

#include <string>
#include <vector>
#include "chart/song_metadata.hpp"
#include "chart/timing_data.hpp"
#include "chart/chart.hpp"

namespace blaze4k {

struct Song {
    std::string pack_name;
    std::string song_dir;
    std::string simfile_path;
    
    SongMetadata metadata;
    TimingData timing;
    std::vector<Chart> charts;

    // Resolved asset paths
    std::string resolved_banner_path;
    std::string resolved_background_path;
    std::string resolved_music_path;

    bool has_custom_banner = false;
    bool has_custom_background = false;
    bool has_custom_music = false;
};

} // namespace blaze4k
