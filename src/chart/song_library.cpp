#include "chart/song_library.hpp"
#include "chart/simfile_parser.hpp"
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

bool icontains(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        }
    );
    return it != haystack.end();
}

bool is_image_extension(std::string_view ext) {
    return iequals(ext, ".png") || iequals(ext, ".jpg") ||
           iequals(ext, ".jpeg") || iequals(ext, ".bmp");
}

bool is_audio_extension(std::string_view ext) {
    return iequals(ext, ".mp3") || iequals(ext, ".ogg") ||
           iequals(ext, ".wav");
}

} // namespace

size_t SongLibrary::total_songs() const {
    size_t count = 0;
    for (const auto& pack : packs_) {
        count += pack.songs.size();
    }
    return count;
}

size_t SongLibrary::total_charts() const {
    size_t count = 0;
    for (const auto& pack : packs_) {
        for (const auto& song : pack.songs) {
            count += song.charts.size();
        }
    }
    return count;
}

const Song* SongLibrary::find_song(std::string_view pack_name, std::string_view title) const {
    for (const auto& pack : packs_) {
        if (!pack_name.empty() && !iequals(pack.name, pack_name)) {
            continue;
        }
        for (const auto& song : pack.songs) {
            if (iequals(song.metadata.title, title)) {
                return &song;
            }
        }
    }
    return nullptr;
}

void SongLibrary::clear() {
    packs_.clear();
}

std::filesystem::path SongLibrary::resolve_file_case_insensitive(
    const std::filesystem::path& dir,
    std::string_view filename
) const {
    if (filename.empty()) {
        return {};
    }

    std::filesystem::path target = dir / filename;
    std::error_code ec;
    if (std::filesystem::exists(target, ec)) {
        return target.lexically_normal();
    }

    // Try case-insensitive lookup in dir
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return {};
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (iequals(entry.path().filename().string(), filename)) {
            return entry.path().lexically_normal();
        }
    }

    return {};
}

std::filesystem::path SongLibrary::find_fallback_art(
    const std::filesystem::path& dir,
    const std::vector<std::string>& keywords
) const {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return {};
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;

        std::string ext = entry.path().extension().string();
        if (!is_image_extension(ext)) continue;

        std::string stem = entry.path().stem().string();
        for (const auto& kw : keywords) {
            if (icontains(stem, kw)) {
                return entry.path().lexically_normal();
            }
        }
    }

    return {};
}

std::optional<Song> SongLibrary::process_song_folder(
    const std::filesystem::path& song_dir,
    const std::string& pack_name
) {
    std::error_code ec;
    if (!std::filesystem::is_directory(song_dir, ec)) {
        return std::nullopt;
    }

    // Find .ssc or .sm file (preferring .ssc)
    std::filesystem::path simfile_path;
    std::filesystem::path sm_path;

    for (const auto& entry : std::filesystem::directory_iterator(song_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        if (iequals(ext, ".ssc")) {
            simfile_path = entry.path();
            break; // .ssc takes highest priority
        } else if (iequals(ext, ".sm") && sm_path.empty()) {
            sm_path = entry.path();
        }
    }

    if (simfile_path.empty()) {
        simfile_path = sm_path;
    }

    if (simfile_path.empty()) {
        return std::nullopt;
    }

    SimfileParser parser;
    try {
        if (!parser.parse_file(simfile_path.string())) {
            std::cerr << "[SongLibrary] Failed to parse simfile: " << simfile_path.string() << "\n";
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SongLibrary] Exception while parsing simfile " << simfile_path.string()
                  << ": " << e.what() << "\n";
        return std::nullopt;
    } catch (...) {
        std::cerr << "[SongLibrary] Unknown exception while parsing simfile " << simfile_path.string() << "\n";
        return std::nullopt;
    }

    // Check if song has any valid 4-panel charts
    if (parser.charts().empty()) {
        std::cout << "[SongLibrary] Skipping song '" << parser.metadata().title
                  << "' at " << song_dir.string() << ": no valid 4-panel charts found.\n";
        return std::nullopt;
    }

    Song song;
    song.pack_name = pack_name;
    song.song_dir = song_dir.string();
    song.simfile_path = simfile_path.string();
    song.metadata = parser.metadata();
    song.timing = parser.timing();
    song.charts = parser.charts();

    // 1. Resolve Banner Art
    std::filesystem::path banner;
    if (!song.metadata.banner_path.empty()) {
        banner = resolve_file_case_insensitive(song_dir, song.metadata.banner_path);
    }
    if (banner.empty()) {
        banner = find_fallback_art(song_dir, {"bn", "banner"});
    }
    if (!banner.empty()) {
        song.resolved_banner_path = banner.string();
        song.has_custom_banner = true;
    } else if (!fallback_banner_.empty()) {
        song.resolved_banner_path = fallback_banner_.string();
        song.has_custom_banner = false;
    }

    // 2. Resolve Background Art
    std::filesystem::path bg;
    if (!song.metadata.background_path.empty()) {
        bg = resolve_file_case_insensitive(song_dir, song.metadata.background_path);
    }
    if (bg.empty()) {
        bg = find_fallback_art(song_dir, {"bg", "background"});
    }
    if (!bg.empty()) {
        song.resolved_background_path = bg.string();
        song.has_custom_background = true;
    } else if (!fallback_background_.empty()) {
        song.resolved_background_path = fallback_background_.string();
        song.has_custom_background = false;
    }

    // 3. Resolve Music
    std::filesystem::path music;
    if (!song.metadata.music_path.empty()) {
        music = resolve_file_case_insensitive(song_dir, song.metadata.music_path);
    }
    if (music.empty()) {
        // Fallback: first audio file in directory
        for (const auto& entry : std::filesystem::directory_iterator(song_dir, ec)) {
            if (entry.is_regular_file(ec) && is_audio_extension(entry.path().extension().string())) {
                music = entry.path().lexically_normal();
                break;
            }
        }
    }
    if (!music.empty()) {
        song.resolved_music_path = music.string();
        song.has_custom_music = true;
    }

    return song;
}

bool SongLibrary::scan_directory(const std::filesystem::path& root_path) {
    std::error_code ec;
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        std::cerr << "[SongLibrary] Directory does not exist: " << root_path.string() << "\n";
        return false;
    }

    // Check if root_path itself is a song directory (contains .sm or .ssc)
    bool root_is_song = false;
    for (const auto& entry : std::filesystem::directory_iterator(root_path, ec)) {
        if (entry.is_regular_file(ec)) {
            std::string ext = entry.path().extension().string();
            if (iequals(ext, ".ssc") || iequals(ext, ".sm")) {
                root_is_song = true;
                break;
            }
        }
    }

    if (root_is_song) {
        std::string pack_name = root_path.filename().string();
        if (auto song = process_song_folder(root_path, pack_name)) {
            auto it = std::find_if(packs_.begin(), packs_.end(), [&](const SongPack& p) {
                return p.name == pack_name;
            });
            if (it == packs_.end()) {
                packs_.push_back({pack_name, root_path.string(), "", {*song}});
            } else {
                it->songs.push_back(*song);
            }
            return true;
        }
        return false;
    }

    // Search recursively for all song directories
    // A directory is a song folder if it contains .ssc or .sm
    std::vector<std::pair<std::filesystem::path, std::string>> song_folders;

    for (std::filesystem::recursive_directory_iterator it(root_path, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_directory(ec)) continue;

        const auto& dir = it->path();
        bool has_simfile = false;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.is_regular_file(ec)) {
                std::string ext = entry.path().extension().string();
                if (iequals(ext, ".ssc") || iequals(ext, ".sm")) {
                    has_simfile = true;
                    break;
                }
            }
        }

        if (has_simfile) {
            // Determine pack name
            auto rel = std::filesystem::relative(dir, root_path, ec);
            std::string pack_name;
            if (!rel.empty() && rel != ".") {
                auto first_comp = rel.begin();
                if (first_comp != rel.end()) {
                    pack_name = first_comp->string();
                }
            }
            if (pack_name.empty() || pack_name == dir.filename().string()) {
                // If song folder is directly under root_path or matches, use parent or root name
                if (dir.parent_path() != root_path && !dir.parent_path().empty()) {
                    pack_name = dir.parent_path().filename().string();
                } else {
                    pack_name = root_path.filename().string();
                }
            }

            song_folders.push_back({dir, pack_name});
            it.disable_recursion_pending(); // Do not recurse deeper into a song directory
        }
    }

    // Process all discovered song folders
    for (const auto& [dir, pack_name] : song_folders) {
        if (auto song = process_song_folder(dir, pack_name)) {
            auto it = std::find_if(packs_.begin(), packs_.end(), [&](const SongPack& p) {
                return p.name == pack_name;
            });

            if (it == packs_.end()) {
                std::filesystem::path pack_dir = std::filesystem::path(song->song_dir).parent_path();
                std::string pack_banner;
                auto bn = resolve_file_case_insensitive(pack_dir, "banner.png");
                if (bn.empty()) {
                    bn = find_fallback_art(pack_dir, {"bn", "banner"});
                }
                if (!bn.empty()) {
                    pack_banner = bn.string();
                }

                packs_.push_back({pack_name, pack_dir.string(), pack_banner, {*song}});
            } else {
                it->songs.push_back(*song);
            }
        }
    }

    return true;
}

} // namespace td
