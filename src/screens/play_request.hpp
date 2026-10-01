#pragma once

#include "gameplay/gameplay_options.hpp"

namespace blaze4k {
struct Song;
struct Chart;

// Handoff published by SelectScreen and consumed by GameplayScreen. The pointed-
// to song/chart are owned by the `SongLibrary` that main keeps alive for the
// whole process, so the pointers stay valid across screens.
struct PlayRequest {
    const Song* song = nullptr;
    const Chart* chart = nullptr;
    GameplayOptions options{};
};

} // namespace blaze4k
