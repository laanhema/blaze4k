#include "chart/simfile_parser.hpp"
#include "chart/msd_file.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

namespace td {

namespace {

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

double parse_double_safe(std::string_view s, double fallback = 0.0) {
    try {
        return std::stod(std::string(s));
    } catch (...) {
        return fallback;
    }
}

} // namespace

bool SimfileParser::parse_file(const std::string& filepath) {
    MsdFile msd;
    if (!msd.read_file(filepath)) {
        std::cerr << "[SimfileParser] Failed to read simfile: " << filepath << "\n";
        return false;
    }

    std::string ext = "";
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = filepath.substr(dot_pos);
    }

    return parse_msd(msd, ext);
}

bool SimfileParser::parse_string(std::string_view content, const std::string& file_extension) {
    MsdFile msd;
    if (!msd.read_string(content)) {
        std::cerr << "[SimfileParser] Failed to parse MSD string content\n";
        return false;
    }
    return parse_msd(msd, file_extension);
}

bool SimfileParser::parse_msd(const MsdFile& msd, const std::string& file_extension) {
    metadata_ = SongMetadata{};
    timing_.clear();
    is_ssc_ = iequals(file_extension, ".ssc");

    for (const auto& tag : msd.tags()) {
        if (iequals(tag.name, "VERSION")) {
            is_ssc_ = true;
        } else if (iequals(tag.name, "TITLE")) {
            metadata_.title = tag.value();
        } else if (iequals(tag.name, "SUBTITLE")) {
            metadata_.subtitle = tag.value();
        } else if (iequals(tag.name, "ARTIST")) {
            metadata_.artist = tag.value();
        } else if (iequals(tag.name, "TITLETRANSLIT")) {
            metadata_.title_translit = tag.value();
        } else if (iequals(tag.name, "SUBTITLETRANSLIT")) {
            metadata_.subtitle_translit = tag.value();
        } else if (iequals(tag.name, "ARTISTTRANSLIT")) {
            metadata_.artist_translit = tag.value();
        } else if (iequals(tag.name, "GENRE")) {
            metadata_.genre = tag.value();
        } else if (iequals(tag.name, "CREDIT")) {
            metadata_.credit = tag.value();
        } else if (iequals(tag.name, "BANNER")) {
            metadata_.banner_path = tag.value();
        } else if (iequals(tag.name, "BACKGROUND")) {
            metadata_.background_path = tag.value();
        } else if (iequals(tag.name, "CDTITLE")) {
            metadata_.cdtitle_path = tag.value();
        } else if (iequals(tag.name, "MUSIC")) {
            metadata_.music_path = tag.value();
        } else if (iequals(tag.name, "LYRICSPATH")) {
            metadata_.lyrics_path = tag.value();
        } else if (iequals(tag.name, "SELECTABLE")) {
            metadata_.selectable = tag.value();
        } else if (iequals(tag.name, "OFFSET")) {
            metadata_.offset = parse_double_safe(tag.value(), 0.0);
            timing_.set_offset(metadata_.offset);
        } else if (iequals(tag.name, "SAMPLESTART")) {
            metadata_.sample_start = parse_double_safe(tag.value(), 0.0);
        } else if (iequals(tag.name, "SAMPLELENGTH")) {
            metadata_.sample_length = parse_double_safe(tag.value(), 12.0);
        } else if (iequals(tag.name, "BPMS")) {
            timing_.parse_bpms_string(tag.value());
        } else if (iequals(tag.name, "STOPS")) {
            timing_.parse_stops_string(tag.value());
        }
    }

    return true;
}

} // namespace td
