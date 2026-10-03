#include "chart/simfile_parser.hpp"
#include "chart/msd_file.hpp"
#include "chart/note_parser.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace blaze4k {

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

// SM5 Song.h:25 (STEPFILE_VERSION_NUMBER): the SSC version assumed until a
// #VERSION tag says otherwise (Song.cpp:78).
constexpr double kSscDefaultVersion = 0.83;
// SM5 NotesLoaderSSC.h:32 (VERSION_CHART_NAME_TAG): below this, an SSC
// #DESCRIPTION is the chart name rather than the description.
constexpr double kSscChartNameTagVersion = 0.74;

// Mirrors SM5 StringToFloat (RageUtil.cpp:1861-1869): strtof, non-finite -> 0.
// Unparsable or out-of-range input also maps to 0.0 (the catch-all fallback).
double parse_ssc_version(std::string_view s) {
    const double v = parse_double_safe(s, 0.0);
    return std::isfinite(v) ? v : 0.0;
}

int parse_int_safe(std::string_view s, int fallback = 1) {
    try {
        return std::stoi(std::string(s));
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
    charts_.clear();
    is_ssc_ = iequals(file_extension, ".ssc");

    // Pass 1: Parse global metadata and timing
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

    // Pass 2: Parse charts
    if (is_ssc_) {
        // SSC chart blocks
        std::string cur_stepstype;
        std::string cur_desc;
        std::string cur_diff = "Beginner";
        int cur_meter = 1;
        TimingData cur_timing = timing_;
        // Song-wide, not per block: SM5 stores both the header and the
        // steps #VERSION on the song (NotesLoaderSSC.cpp:74-78,315-318), so
        // each tag applies to the chart blocks after it, in file order.
        double ssc_version = kSscDefaultVersion;

        for (const auto& tag : msd.tags()) {
            if (iequals(tag.name, "VERSION")) {
                ssc_version = parse_ssc_version(tag.value());
            } else if (iequals(tag.name, "NOTEDATA")) {
                cur_stepstype.clear();
                cur_desc.clear();
                cur_diff = "Beginner";
                cur_meter = 1;
                cur_timing = timing_;
            } else if (iequals(tag.name, "STEPSTYPE")) {
                cur_stepstype = tag.value();
            } else if (iequals(tag.name, "CHARTNAME")) {
                // Chart name: not the edit display name (SM5
                // StepsDisplay.cpp:199-202 shows the description); per
                // NotesLoaderSSC.cpp:319-324 it has its own slot, so it never
                // overwrites the description. Deliberately not stored.
            } else if (iequals(tag.name, "DESCRIPTION")) {
                // SM5 NotesLoaderSSC.cpp:336-349: before 0.74 #DESCRIPTION is
                // the chart name (not stored, see above); otherwise it is the
                // description.
                if (ssc_version >= kSscChartNameTagVersion) {
                    cur_desc = tag.value();
                }
            } else if (iequals(tag.name, "DIFFICULTY")) {
                cur_diff = tag.value();
            } else if (iequals(tag.name, "METER")) {
                cur_meter = parse_int_safe(tag.value(), 1);
            } else if (iequals(tag.name, "BPMS") && !cur_stepstype.empty()) {
                cur_timing.parse_bpms_string(tag.value());
            } else if (iequals(tag.name, "STOPS") && !cur_stepstype.empty()) {
                cur_timing.parse_stops_string(tag.value());
            } else if (iequals(tag.name, "OFFSET") && !cur_stepstype.empty()) {
                cur_timing.set_offset(parse_double_safe(tag.value(), metadata_.offset));
            } else if (iequals(tag.name, "NOTES")) {
                auto chart_opt = NoteParser::parse_4panel_notedata(
                    cur_stepstype,
                    cur_desc,
                    cur_diff,
                    cur_meter,
                    tag.value(),
                    cur_timing
                );
                if (chart_opt.has_value()) {
                    charts_.push_back(std::move(chart_opt.value()));
                }
            }
        }
    } else {
        // SM format: #NOTES:stepstype:desc:diff:meter:radar:notedata;
        for (const auto& tag : msd.tags()) {
            if (iequals(tag.name, "NOTES")) {
                if (tag.params.size() < 6) {
                    std::cerr << "[SimfileParser] Warning: #NOTES tag has fewer than 6 parameters\n";
                    continue;
                }
                std::string steps_type = tag.value(0);
                std::string desc = tag.value(1);
                std::string diff = tag.value(2);
                int meter = parse_int_safe(tag.value(3), 1);
                std::string note_data = tag.params.back();

                auto chart_opt = NoteParser::parse_4panel_notedata(
                    steps_type,
                    desc,
                    diff,
                    meter,
                    note_data,
                    timing_
                );
                if (chart_opt.has_value()) {
                    charts_.push_back(std::move(chart_opt.value()));
                }
            }
        }
    }

    return true;
}

} // namespace blaze4k
