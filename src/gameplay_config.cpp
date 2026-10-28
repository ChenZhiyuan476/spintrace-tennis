#include "gameplay_config.hpp"

#include <algorithm>

namespace tennis {

double StaminaTuning::movementScaleAt(double staminaRatio) const {
    const double ratio = std::clamp(staminaRatio, 0.0, 1.0);
    const double threshold = std::max(0.01, lowStaminaThreshold);
    if (ratio >= threshold) return 1.0;
    return exhaustedSpeedScale + (1.0 - exhaustedSpeedScale) * ratio / threshold;
}

double StaminaTuning::accuracyQualityAt(double staminaRatio) const {
    const double ratio = std::clamp(staminaRatio, 0.0, 1.0);
    const double threshold = std::max(0.01, lowStaminaThreshold);
    if (ratio >= threshold) return 1.0;
    return exhaustedAccuracyQuality +
           (1.0 - exhaustedAccuracyQuality) * ratio / threshold;
}

double StrokePreferences::gripPower() const {
    switch (grip) {
        case GripStyle::Continental: return 0.95;
        case GripStyle::Eastern: return 1.02;
        case GripStyle::SemiWestern: return 0.98;
        case GripStyle::Western: return 0.92;
        default: return 1.0;
    }
}

double StrokePreferences::gripTopspin() const {
    switch (grip) {
        case GripStyle::Continental: return 0.75;
        case GripStyle::Eastern: return 1.00;
        case GripStyle::SemiWestern: return 1.20;
        case GripStyle::Western: return 1.35;
        default: return 1.0;
    }
}

double StrokePreferences::gripSlice() const {
    switch (grip) {
        case GripStyle::Continental: return 1.25;
        case GripStyle::Eastern: return 1.00;
        case GripStyle::SemiWestern: return 0.85;
        case GripStyle::Western: return 0.70;
        default: return 1.0;
    }
}

double StrokePreferences::motionPower() const {
    switch (motion) {
        case SwingMotion::Compact: return 0.88;
        case SwingMotion::Modern: return 1.05;
        case SwingMotion::Whip: return 0.97;
        default: return 1.0;
    }
}

double StrokePreferences::motionSpin() const {
    switch (motion) {
        case SwingMotion::Compact: return 0.82;
        case SwingMotion::Modern: return 1.08;
        case SwingMotion::Whip: return 1.20;
        default: return 1.0;
    }
}

double StrokePreferences::motionControl() const {
    switch (motion) {
        case SwingMotion::Compact: return 1.15;
        case SwingMotion::Modern: return 0.98;
        case SwingMotion::Whip: return 0.88;
        default: return 1.0;
    }
}

PlayerProfile makeDefaultOpponentProfile() {
    PlayerProfile profile;
    profile.displayName = "COMPUTER";
    profile.avatar.primary = {0.941f, 0.424f, 0.392f, 1.0f};
    return profile;
}

} // namespace tennis
