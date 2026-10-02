#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include "chart/note.hpp"
#include "chart/timing_data.hpp"

namespace blaze4k {

// OpenITG `Difficulty` (src/Difficulty.h). Resolved only to detect Beginner
// for MercifulBeginner; the UI keeps showing the passthrough label.
enum class StepsDifficulty { Beginner, Easy, Medium, Hard, Challenge, Edit, Invalid };

// Mirrors OpenITG `StringToDifficulty` (src/Difficulty.cpp:22-44): `MakeLower`,
// then the full alias table; anything else (e.g. "Novice", "") is Invalid.
// Labels arrive already trimmed by the MSD reader.
[[nodiscard]] inline StepsDifficulty string_to_difficulty(std::string_view label) {
    struct Alias {
        std::string_view name;
        StepsDifficulty difficulty;
    };
    static constexpr Alias kAliases[] = {
        {"beginner", StepsDifficulty::Beginner},   {"easy", StepsDifficulty::Easy},
        {"basic", StepsDifficulty::Easy},          {"light", StepsDifficulty::Easy},
        {"medium", StepsDifficulty::Medium},       {"another", StepsDifficulty::Medium},
        {"trick", StepsDifficulty::Medium},        {"standard", StepsDifficulty::Medium},
        {"difficult", StepsDifficulty::Medium},    {"hard", StepsDifficulty::Hard},
        {"ssr", StepsDifficulty::Hard},            {"maniac", StepsDifficulty::Hard},
        {"heavy", StepsDifficulty::Hard},          {"smaniac", StepsDifficulty::Challenge},
        {"challenge", StepsDifficulty::Challenge}, {"expert", StepsDifficulty::Challenge},
        {"oni", StepsDifficulty::Challenge},       {"edit", StepsDifficulty::Edit},
    };
    for (const Alias& alias : kAliases) {
        if (alias.name.size() != label.size()) {
            continue;
        }
        bool match = true;
        for (std::size_t i = 0; i < label.size() && match; ++i) {
            match = std::tolower(static_cast<unsigned char>(label[i])) == alias.name[i];
        }
        if (match) {
            return alias.difficulty;
        }
    }
    return StepsDifficulty::Invalid;
}

// Mirrors the load-time resolution chain: `StringToDifficulty(label)`
// (NotesLoaderSM.cpp:33), then `Steps::TidyUpData` (Steps.cpp:130-141, called
// at NotesLoaderSM.cpp:63): an Invalid label falls back to the description,
// then to the meter (1 = Beginner, <=3 Easy, <=6 Medium, else Hard). The SM
// loader's smaniac/challenge description override (NotesLoaderSM.cpp:35-42) is
// deliberately not mirrored.
[[nodiscard]] inline StepsDifficulty resolve_difficulty(std::string_view label,
                                                        std::string_view description,
                                                        int meter) {
    StepsDifficulty difficulty = string_to_difficulty(label);
    if (difficulty == StepsDifficulty::Invalid) {
        difficulty = string_to_difficulty(description);
    }
    if (difficulty == StepsDifficulty::Invalid) {
        if (meter == 1) {
            difficulty = StepsDifficulty::Beginner;
        } else if (meter <= 3) {
            difficulty = StepsDifficulty::Easy;
        } else if (meter <= 6) {
            difficulty = StepsDifficulty::Medium;
        } else {
            difficulty = StepsDifficulty::Hard;
        }
    }
    return difficulty;
}

struct Chart {
    std::string steps_type = "dance-single";
    std::string description;
    // Beginner, Easy, Medium, Hard, Challenge, Edit (passthrough label). The
    // parser always sets it and the meter. The empty label and meter 0 defaults
    // mirror OpenITG `Steps::Steps` (Steps.cpp:29-30: DIFFICULTY_INVALID,
    // m_iMeter = 0), which resolve to Easy, so hand-built charts are never
    // MercifulBeginner charts by accident.
    std::string difficulty;
    int meter = 0;                       // Foot rating

    std::vector<Note> notes;
    TimingData timing;

    int tap_count = 0;
    int hold_count = 0;
    int roll_count = 0;
    int mine_count = 0;

    // OpenITG `Player::IsPlayingBeginner` (src/Player.cpp:1724-1737) on the
    // load-time resolved difficulty (see resolve_difficulty).
    [[nodiscard]] bool is_beginner() const {
        return resolve_difficulty(difficulty, description, meter) == StepsDifficulty::Beginner;
    }

    [[nodiscard]] int total_stream_notes() const {
        return tap_count + hold_count + roll_count;
    }
};

} // namespace blaze4k
