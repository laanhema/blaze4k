#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>
#include "chart/song_pack.hpp"

namespace td {

class SongLibrary {
public:
    SongLibrary() = default;

    // Scan a songs folder, pack folder, or collection root
    bool scan_directory(const std::filesystem::path& root_path);

    // Fallback image asset paths for songs lacking banner/background
    void set_fallback_banner(const std::filesystem::path& path) { fallback_banner_ = path; }
    void set_fallback_background(const std::filesystem::path& path) { fallback_background_ = path; }

    [[nodiscard]] const std::filesystem::path& fallback_banner() const { return fallback_banner_; }
    [[nodiscard]] const std::filesystem::path& fallback_background() const { return fallback_background_; }

    [[nodiscard]] const std::vector<SongPack>& packs() const { return packs_; }
    [[nodiscard]] size_t total_songs() const;
    [[nodiscard]] size_t total_charts() const;

    [[nodiscard]] const Song* find_song(std::string_view pack_name, std::string_view title) const;

    void clear();

private:
    std::vector<SongPack> packs_;
    std::filesystem::path fallback_banner_;
    std::filesystem::path fallback_background_;

    // Helpers
    std::optional<Song> process_song_folder(
        const std::filesystem::path& song_dir,
        const std::string& pack_name
    );

    std::filesystem::path resolve_file_case_insensitive(
        const std::filesystem::path& dir,
        std::string_view filename
    ) const;

    std::filesystem::path find_fallback_art(
        const std::filesystem::path& dir,
        const std::vector<std::string>& keywords
    ) const;
};

} // namespace td
