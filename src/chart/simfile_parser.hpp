#pragma once

#include <string>
#include <string_view>
#include "chart/song_metadata.hpp"
#include "chart/timing_data.hpp"

namespace td {

class MsdFile;

class SimfileParser {
public:
    SimfileParser() = default;

    bool parse_file(const std::string& filepath);
    bool parse_string(std::string_view content, const std::string& file_extension = ".sm");

    [[nodiscard]] const SongMetadata& metadata() const { return metadata_; }
    [[nodiscard]] const TimingData& timing() const { return timing_; }
    [[nodiscard]] bool is_ssc() const { return is_ssc_; }

private:
    bool parse_msd(const MsdFile& msd, const std::string& file_extension);

    SongMetadata metadata_;
    TimingData timing_;
    bool is_ssc_ = false;
};

} // namespace td
