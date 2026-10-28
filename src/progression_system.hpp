#pragma once

#include "gameplay_config.hpp"

#include <cstddef>

namespace tennis {

// Campaign lessons are the levels: clearing one teaches its technique and
// applies the listed permanent growth rewards to the player's profile.
class ProgressionSystem {
public:
    static PlayerProfile makeNewCampaignPlayer();
    static ProgressionState makeNewCampaignProgress();
    static bool canEnterLesson(const ProgressionState &progress, std::size_t lessonIndex);
    static bool completeLesson(PlayerProfile &profile, ProgressionState &progress,
                               std::size_t lessonIndex);
};

} // namespace tennis
