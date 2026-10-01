#include "data/data_paths.hpp"

#include <SDL3/SDL.h>

namespace blaze4k {

ResolvedDataPaths resolve_data_paths(const std::filesystem::path& executable_dir, bool prefer_xdg,
                                     const std::string& xdg_data_home, const std::string& home_dir,
                                     const std::filesystem::path& explicit_data_dir) {
    std::filesystem::path data_dir;
    if (!explicit_data_dir.empty()) {
        data_dir = explicit_data_dir;
    } else if (prefer_xdg) {
        std::filesystem::path base;
        if (!xdg_data_home.empty()) {
            base = xdg_data_home;
        } else if (!home_dir.empty()) {
            base = std::filesystem::path(home_dir) / ".local" / "share";
        }
        data_dir = base.empty() ? (executable_dir / "data") : (base / "blaze-4k");
    } else {
        data_dir = executable_dir / "data";
    }

    return ResolvedDataPaths{
        data_dir,
        data_dir / "config.json",
        data_dir / "scores.json",
    };
}

std::filesystem::path default_executable_dir() {
    // SDL3 caches this path internally (SDL_GetBasePath -> CachedBasePath) and
    // keeps ownership; unlike SDL2 it must NOT be SDL_free'd by the caller.
    const char* base = SDL_GetBasePath();
    if (base == nullptr || base[0] == '\0') {
        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        return ec ? std::filesystem::path(".") : cwd;
    }
    // SDL guarantees a directory path (with a trailing separator) for the
    // executable; it denotes the containing directory directly.
    return std::filesystem::path(base);
}

std::filesystem::path resolve_first_existing(
    const std::vector<std::filesystem::path>& candidates) {
    for (const std::filesystem::path& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

} // namespace blaze4k
