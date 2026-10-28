#include "progression_system.hpp"

#include <algorithm>

namespace tennis {

PlayerProfile ProgressionSystem::makeNewCampaignPlayer() {
    PlayerProfile profile;
    profile.displayName = "PLAYER";
    profile.unlockedTechniques = techniqueBit(Technique::Flat);
    profile.attributes.movementSpeed = 0.88;
    profile.attributes.sprintSpeed = 0.88;
    profile.attributes.endurance = 0.82;
    profile.attributes.swingPower = 0.82;
    profile.attributes.maximumSwingPower = 0.78;
    profile.attributes.shotSpeed = 0.90;
    profile.attributes.stability = 0.78;
    profile.attributes.contactTolerance = 0.82;
    return profile;
}

ProgressionState ProgressionSystem::makeNewCampaignProgress() {
    return {0, techniqueBit(Technique::Flat)};
}

bool ProgressionSystem::canEnterLesson(const ProgressionState &progress, std::size_t lessonIndex) {
    if (lessonIndex >= DefaultLessonPath.size()) return false;
    if (lessonIndex > static_cast<std::size_t>(progress.completedLessons)) return false;
    const LessonStage &lesson = DefaultLessonPath[lessonIndex];
    return (progress.unlockedTechniques & lesson.requiredTechniques) == lesson.requiredTechniques;
}

bool ProgressionSystem::completeLesson(PlayerProfile &profile, ProgressionState &progress,
                                       std::size_t lessonIndex) {
    if (!canEnterLesson(progress, lessonIndex)) return false;
    const LessonStage &lesson = DefaultLessonPath[lessonIndex];
    const std::uint32_t unlock = techniqueBit(lesson.taughtTechnique);
    const bool firstClear = lessonIndex >= static_cast<std::size_t>(progress.completedLessons);
    progress.unlockedTechniques |= unlock;
    profile.unlockedTechniques |= unlock;
    progress.completedLessons = std::max(progress.completedLessons,
                                         static_cast<int>(lessonIndex + 1));
    if (!firstClear) return true;

    profile.attributes.swingPower += lesson.powerReward;
    profile.attributes.maximumSwingPower = std::min(1.0,
        profile.attributes.maximumSwingPower + lesson.powerReward);
    profile.attributes.movementSpeed += lesson.speedReward;
    profile.attributes.sprintSpeed += lesson.speedReward;
    profile.attributes.endurance += lesson.enduranceReward;
    profile.attributes.staminaCapacity += lesson.enduranceReward * 12.0;
    profile.attributes.staminaRecovery += lesson.enduranceReward * 0.5;
    profile.attributes.stability += lesson.stabilityReward;
    profile.attributes.contactTolerance += lesson.contactToleranceReward;
    return true;
}

} // namespace tennis
