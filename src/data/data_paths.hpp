#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace td {

// Where config.json and scores.json live for this run.
struct ResolvedDataPaths {
    std::filesystem::path data_dir;
    std::filesystem::path config_file; // data_dir / "config.json"
    std::filesystem::path scores_file; // data_dir / "scores.json"
};

// Pure, deterministic resolution (no environment or platform access) so it is
// directly unit-testable. Precedence:
//   1. `explicit_data_dir` when non-empty (e.g. `--data-dir`, tests, smoke runs);
//   2. XDG when `prefer_xdg` is set -- `${xdg_data_home:-<home>/.local/share}/tundra-dance`;
//   3. portable default -- `executable_dir/data` next to the binary.
// If XDG is requested but neither an XDG data home nor a home directory is
// available, the portable default is used.
[[nodiscard]] ResolvedDataPaths resolve_data_paths(
    const std::filesystem::path& executable_dir, bool prefer_xdg,
    const std::string& xdg_data_home, const std::string& home_dir,
    const std::filesystem::path& explicit_data_dir = {});

// The directory containing the running executable, via the SDL base-path query.
// This is the only SDL touchpoint in src/data. Falls back to the current working
// directory when SDL cannot report the base path.
[[nodiscard]] std::filesystem::path default_executable_dir();

// Returns the first candidate that exists as a regular file, else an empty
// path. Mirrors the startup candidate resolution used for data assets:
// candidates are probed in order, so callers can list cwd-relative paths first
// and an executable-relative path last.
[[nodiscard]] std::filesystem::path resolve_first_existing(
    const std::vector<std::filesystem::path>& candidates);

} // namespace td
