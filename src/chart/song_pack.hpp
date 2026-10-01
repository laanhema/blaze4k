#pragma once

#include <string>
#include <vector>
#include "chart/song.hpp"

namespace blaze4k {

struct SongPack {
    std::string name;
    std::string pack_dir;
    std::string banner_path;
    std::vector<Song> songs;
};

} // namespace blaze4k
