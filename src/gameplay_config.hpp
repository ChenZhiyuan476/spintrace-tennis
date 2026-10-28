#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace tennis {

struct Rgba {
    float r, g, b, a = 1.0f;
};

enum class GripStyle { Balanced, Continental, Eastern, SemiWestern, Western };
enum class SwingMotion { Classic, Compact, Modern, Whip };
enum class BallPattern { Solid, CenterDot, Stripe, Split };
enum class CourtMaterial { Hard, Clay, Grass };
enum class CourtPreset { BouncyHard, Clay, Grass, FastHard };
enum class AiDifficulty { Easy, Normal, Hard, Expert };
enum class SurfaceParticleKind { None, Dust, GrassClipping };

enum class Technique : std::uint32_t {
    Flat = 1u << 0,
    Direction = 1u << 1,
    Topspin = 1u << 2,
    Slice = 1u << 3,
    Serve = 1u << 4,
    Volley = 1u << 5,
    Smash = 1u << 6,
    SideSpin = 1u << 7,
    DropShot = 1u << 8,
    Lob = 1u << 9
};

constexpr std::uint32_t techniqueBit(Technique technique) {
    return static_cast<std::uint32_t>(technique);
}

inline constexpr std::uint32_t AllTechniques =
    techniqueBit(Technique::Flat) | techniqueBit(Technique::Direction) |
    techniqueBit(Technique::Topspin) | techniqueBit(Technique::Slice) |
    techniqueBit(Technique::Serve) | techniqueBit(Technique::Volley) |
    techniqueBit(Technique::Smash) | techniqueBit(Technique::SideSpin) |
    techniqueBit(Technique::DropShot) | techniqueBit(Technique::Lob);

struct PlayerAttributes {
    double movementSpeed = 1.0;
    double sprintSpeed = 1.0;
    double staminaCapacity = 100.0;
    double endurance = 1.0;
    double staminaRecovery = 1.0;
    double swingPower = 1.0;
    double maximumSwingPower = 1.0;
    double shotSpeed = 1.0;
    double stability = 1.0;
    double contactTolerance = 1.0;
};

struct StaminaTuning {
    double runningCostPerSecond = 7.5;
    double sprintCostPerSecond = 18.5;
    double idleRecoveryPerSecond = 5.5;
    double baseHitCost = 4.0;
    double splitStepSpeedMultiplier = 1.15;
    double lowStaminaThreshold = 0.20;
    double exhaustedSpeedScale = 0.62;
    double exhaustedAccuracyQuality = 0.15;

    double movementScaleAt(double staminaRatio) const;
    double accuracyQualityAt(double staminaRatio) const;
};

// Initial racket/serve speeds in metres per second. Air drag, spin, incoming
// pace retention and court bounce are applied after these values.
struct ShotSpeedTuning {
    double playerGroundBase = 9.0;
    double playerGroundPower = 29.0;
    double playerPowerExponent = 1.35;
    double playerSmashBonus = 7.0;
    double playerOverhandServeBonus = 10.0;
    double playerUnderhandBase = 10.0;
    double playerUnderhandPower = 18.0;
    double playerVolleyBase = 12.0;
    double playerVolleyPower = 17.0;
    double cpuGroundMin = 22.0;
    double cpuGroundMax = 29.0;
    double cpuServeMin = 42.0;
    double cpuServeMax = 51.0;
    double serveContactHeight = 2.62;
    double cpuServeContactHeight = 2.55;
    double serveTossVelocity = 5.85;
};

struct AiDifficultyProfile {
    const char *name;
    double ballSpeedScale;
    double movementSpeedScale;
    double targetErrorPixels;
    double serveErrorPixels;
    double rallyLoftMin;
    double rallyLoftMax;
};

inline constexpr AiDifficultyProfile aiDifficultyProfile(AiDifficulty difficulty) {
    switch (difficulty) {
        case AiDifficulty::Easy:
            return {"EASY", 0.76, 0.88, 30.0, 24.0, 0.00, 0.10};
        case AiDifficulty::Normal:
            return {"NORMAL", 0.88, 1.00, 18.0, 14.0, 0.00, 0.08};
        case AiDifficulty::Hard:
            return {"HARD", 1.00, 1.07, 10.0, 8.0, 0.00, 0.06};
        case AiDifficulty::Expert:
            return {"EXPERT", 1.08, 1.13, 5.0, 4.0, 0.00, 0.05};
    }
    return {"NORMAL", 0.88, 1.00, 18.0, 14.0, 0.00, 0.08};
}

struct ContactZoneConfig {
    double normalGreenMin = 0.55;
    double normalGreenMax = 1.42;
    double normalReachMin = 0.16;
    double normalReachMax = 1.72;
    double smashGreenMin = 1.88;
    double smashGreenMax = 2.10;
    double smashReachMin = 1.62;
    double smashReachMax = 2.38;
    double difficultBandQuality = 0.44;
    double idealLateralOffset = 34.0;
    double lateralGreenRadius = 18.0;
    double idealForwardOffset = -18.0;
    double forwardGreenRadius = 23.0;
    double unreachableEllipse = 2.15;
};

struct StrokePreferences {
    GripStyle grip = GripStyle::Balanced;
    SwingMotion motion = SwingMotion::Classic;
    double powerBias = 1.0;
    double topspinBias = 1.0;
    double sliceBias = 1.0;
    double sideSpinBias = 1.0;
    double controlBias = 1.0;

    double gripPower() const;
    double gripTopspin() const;
    double gripSlice() const;
    double motionPower() const;
    double motionSpin() const;
    double motionControl() const;
};

struct BallAvatarStyle {
    Rgba primary{0.357f, 0.753f, 0.922f, 1.0f};
    Rgba secondary{0.949f, 0.929f, 0.855f, 1.0f};
    BallPattern pattern = BallPattern::Solid;
    double radiusScale = 1.0;
};

struct HudSettings {
    bool showScoreboard = true;
    bool showContactAssist = true;
    bool showTopLeftInfo = true;
};

// Input feel remains independent from player attributes so accessibility
// options and future controller presets can change it without changing stats.
struct ControlSettings {
    bool autoHit = true;
    double rallyFullChargeSeconds = 0.85;
    double serveFullChargeSeconds = 0.48;
    double movingChargeRate = 0.45;
    double sprintChargeRate = 0.0;
};

struct SurfaceEffectProfile {
    SurfaceParticleKind particleKind = SurfaceParticleKind::None;
    Rgba particleColor{0.0f, 0.0f, 0.0f, 0.0f};
    Rgba wearColor{0.0f, 0.0f, 0.0f, 0.0f};
    double ballImpactAmount = 0.0;
    double movementAmount = 0.0;
    double wearRate = 0.0;
};

// Surface values are expressed in real-world units where practical. Player
// acceleration and braking use m/s^2; bounce coefficients are applied by the
// independent ball physics system at each court impact.
struct CourtSurfaceProfile {
    CourtPreset preset;
    CourtMaterial material;
    std::string_view name;
    Rgba courtColor;
    Rgba surroundColor;
    bool drawMowingStripes;
    Rgba mowingStripeColor;
    Rgba scoreboardBase;
    Rgba scoreboardAccent;
    Rgba scoreboardPoint;
    Rgba scoreboardInk;
    Rgba scoreboardText;
    SurfaceEffectProfile effects;
    double movementSpeedScale;
    double accelerationMps2;
    double decelerationMps2;
    double bounceRestitutionBase;
    double bounceRestitutionSpin;
    double bounceForwardBase;
    double bounceForwardSpin;
};

inline constexpr std::array<CourtSurfaceProfile, 4> CourtSurfaceProfiles{{
    {CourtPreset::BouncyHard, CourtMaterial::Hard, "BOUNCY HARD",
     {0.11f, 0.30f, 0.50f, 1.0f}, {0.20f, 0.47f, 0.68f, 1.0f},
     false, {0.0f, 0.0f, 0.0f, 0.0f},
     {0.025f, 0.13f, 0.30f, 0.96f}, {0.08f, 0.45f, 0.76f, 0.98f},
     {0.88f, 0.94f, 0.98f, 0.98f}, {0.025f, 0.10f, 0.23f, 1.0f},
     {0.95f, 0.97f, 0.98f, 1.0f},
     {},
     1.00, 35.0, 40.0, 0.73, 0.17, 0.70, 0.19},
    {CourtPreset::Clay, CourtMaterial::Clay, "CLAY",
     {0.72f, 0.36f, 0.22f, 1.0f}, {0.72f, 0.36f, 0.22f, 1.0f},
     false, {0.0f, 0.0f, 0.0f, 0.0f},
     {0.025f, 0.255f, 0.170f, 0.94f}, {0.25f, 0.62f, 0.38f, 0.98f},
     {0.94f, 0.92f, 0.84f, 0.98f}, {0.045f, 0.075f, 0.060f, 1.0f},
     {0.949f, 0.929f, 0.855f, 1.0f},
     {SurfaceParticleKind::Dust, {0.86f, 0.56f, 0.38f, 1.0f}, {}, 1.0, 1.0, 0.0},
     0.98, 28.0, 24.0, 0.74, 0.18, 0.63, 0.17},
    {CourtPreset::Grass, CourtMaterial::Grass, "GRASS",
     {0.37f, 0.49f, 0.31f, 1.0f}, {0.37f, 0.49f, 0.31f, 1.0f},
     true, {0.44f, 0.55f, 0.36f, 0.44f},
     {0.20f, 0.08f, 0.30f, 0.97f}, {0.58f, 0.39f, 0.72f, 0.99f},
     {0.98f, 0.97f, 1.00f, 0.99f}, {0.16f, 0.055f, 0.24f, 1.0f},
     {0.99f, 0.98f, 1.00f, 1.0f},
     {SurfaceParticleKind::GrassClipping,
      {0.69f, 0.73f, 0.36f, 0.86f}, {0.64f, 0.58f, 0.31f, 0.34f},
      0.08, 0.12, 0.24},
     1.01, 31.0, 29.0, 0.57, 0.13, 0.82, 0.16},
    {CourtPreset::FastHard, CourtMaterial::Hard, "FAST HARD",
     {0.23f, 0.31f, 0.57f, 1.0f}, {0.31f, 0.52f, 0.37f, 1.0f},
     false, {0.0f, 0.0f, 0.0f, 0.0f},
     {0.025f, 0.14f, 0.34f, 0.96f}, {0.10f, 0.38f, 0.73f, 0.98f},
     {0.77f, 0.87f, 0.98f, 0.99f}, {0.025f, 0.09f, 0.22f, 1.0f},
     {0.95f, 0.97f, 1.0f, 1.0f},
     {},
     1.01, 34.0, 38.0, 0.66, 0.14, 0.79, 0.17}
}};

inline constexpr const CourtSurfaceProfile &courtSurfaceProfile(CourtPreset preset) {
    switch (preset) {
        case CourtPreset::Clay: return CourtSurfaceProfiles[1];
        case CourtPreset::Grass: return CourtSurfaceProfiles[2];
        case CourtPreset::FastHard: return CourtSurfaceProfiles[3];
        default: return CourtSurfaceProfiles[0];
    }
}

struct PlayerProfile {
    std::string displayName = "PLAYER";
    PlayerAttributes attributes;
    ContactZoneConfig contactZone;
    StrokePreferences stroke;
    BallAvatarStyle avatar;
    std::uint32_t unlockedTechniques = AllTechniques;

    bool hasTechnique(Technique technique) const {
        return (unlockedTechniques & techniqueBit(technique)) != 0;
    }
};

struct LessonStage {
    std::string_view id;
    std::string_view title;
    Technique taughtTechnique;
    std::uint32_t requiredTechniques;
    double powerReward;
    double speedReward;
    double enduranceReward;
    double stabilityReward;
    double contactToleranceReward;
};

inline constexpr std::array<LessonStage, 10> DefaultLessonPath{{
    {"flat", "FLAT CONTACT", Technique::Flat, 0, 0.00, 0.00, 0.00, 0.05, 0.05},
    {"aim", "DIRECTION", Technique::Direction, techniqueBit(Technique::Flat), 0.00, 0.04, 0.00, 0.05, 0.05},
    {"topspin", "TOPSPIN", Technique::Topspin, techniqueBit(Technique::Direction), 0.05, 0.00, 0.00, 0.05, 0.05},
    {"slice", "SLICE", Technique::Slice, techniqueBit(Technique::Topspin), 0.00, 0.00, 0.05, 0.05, 0.08},
    {"serve", "SERVE", Technique::Serve, techniqueBit(Technique::Slice), 0.06, 0.00, 0.04, 0.04, 0.04},
    {"volley", "VOLLEY", Technique::Volley, techniqueBit(Technique::Serve), 0.00, 0.06, 0.04, 0.06, 0.05},
    {"smash", "SMASH", Technique::Smash, techniqueBit(Technique::Volley), 0.07, 0.00, 0.03, 0.04, 0.04},
    {"side_spin", "SIDE SPIN", Technique::SideSpin, techniqueBit(Technique::Smash), 0.05, 0.04, 0.05, 0.07, 0.08},
    {"drop", "DROP SHOT", Technique::DropShot, techniqueBit(Technique::SideSpin), 0.03, 0.03, 0.06, 0.08, 0.08},
    {"lob", "LOB", Technique::Lob, techniqueBit(Technique::DropShot), 0.04, 0.04, 0.06, 0.08, 0.10}
}};

struct ProgressionState {
    int completedLessons = 0;
    std::uint32_t unlockedTechniques = AllTechniques;
};

struct CourtTheme {
    Rgba background{0.063f, 0.106f, 0.090f, 1.0f};
    Rgba surface{0.224f, 0.482f, 0.353f, 1.0f};
    Rgba surround{0.663f, 0.333f, 0.196f, 1.0f};
    Rgba lines{0.949f, 0.929f, 0.855f, 1.0f};
    Rgba standDark{0.075f, 0.090f, 0.085f, 1.0f};
    Rgba standLight{0.15f, 0.18f, 0.16f, 1.0f};
    std::array<Rgba, 4> crowd{{
        {0.357f, 0.753f, 0.922f, 1.0f}, {0.941f, 0.424f, 0.392f, 1.0f},
        {1.0f, 0.78f, 0.27f, 1.0f}, {0.66f, 0.52f, 0.82f, 1.0f}
    }};
    int audienceRows = 1;
    bool drawAudience = true;
};

PlayerProfile makeDefaultOpponentProfile();

} // namespace tennis
