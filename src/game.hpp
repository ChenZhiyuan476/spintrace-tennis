#pragma once

#include "audio_system.hpp"
#include "ball_physics.hpp"
#include "gameplay_config.hpp"
#include "match_state.hpp"
#include "player_input.hpp"
#include "scoring_system.hpp"
#include "vec2.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <deque>
#include <random>
#include <string>
#include <vector>

class Game {
public:
    static constexpr int Width = 1280;
    static constexpr int Height = 720;

    Game();

    void handleEvent(const SDL_Event &event);
    void update(double delta, const bool *keys);
    void render(SDL_Renderer *renderer) const;
    bool wantsToQuit() const { return quitRequested; }

    const tennis::MatchState &state() const { return match; }
    void restoreState(const tennis::MatchState &state) { match = state; }
    tennis::PlayerProfile &playerConfiguration() { return playerProfile; }
    const tennis::PlayerProfile &playerConfiguration() const { return playerProfile; }
    tennis::PlayerProfile &opponentConfiguration() { return opponentProfile; }
    const tennis::PlayerProfile &opponentConfiguration() const { return opponentProfile; }
    tennis::BallPhysicsConfig &physicsConfiguration() { return ballPhysics.config(); }
    const tennis::BallPhysicsConfig &physicsConfiguration() const { return ballPhysics.config(); }
    tennis::CourtTheme &courtConfiguration() { return courtTheme; }
    const tennis::CourtTheme &courtConfiguration() const { return courtTheme; }
    tennis::HudSettings &hudConfiguration() { return hudSettings; }
    const tennis::HudSettings &hudConfiguration() const { return hudSettings; }
    tennis::ControlSettings &controlConfiguration() { return controlSettings; }
    const tennis::ControlSettings &controlConfiguration() const { return controlSettings; }
    tennis::CourtPreset courtPreset() const { return activeCourtPreset; }
    void setCourtPreset(tennis::CourtPreset preset);
    tennis::StaminaTuning &staminaConfiguration() { return staminaTuning; }
    const tennis::StaminaTuning &staminaConfiguration() const { return staminaTuning; }
    tennis::ShotSpeedTuning &shotSpeedConfiguration() { return shotSpeedTuning; }
    const tennis::ShotSpeedTuning &shotSpeedConfiguration() const { return shotSpeedTuning; }
    tennis::AiDifficulty opponentDifficulty() const { return aiDifficulty; }
    std::array<tennis::RacketImpactConfig, 5> &racketImpactConfigurations() { return racketImpactProfiles; }
    const std::array<tennis::RacketImpactConfig, 5> &racketImpactConfigurations() const { return racketImpactProfiles; }
    tennis::RacketImpactConfig &opponentImpactConfiguration() { return opponentImpactProfile; }
    const tennis::RacketImpactConfig &opponentImpactConfiguration() const { return opponentImpactProfile; }
private:
    enum class PausePage { Main, SwingGuide, MappingSettings, Rules, Scoring, Style };

    struct ShotRecord {
        std::string shot;
        std::vector<Vec2> pattern;
        double power = 0.0;
        double verticalSpinRpm = 0.0;
        double sideSpinRpm = 0.0;
        double loft = 0.0;
        double direction = 0.0;
        double flatDrive = 0.0;
        double accuracy = 0.0;
        double stamina = 0.0;
        int mappingProfile = 0;
    };

    struct MappingProfile {
        double power = 1.0;
        double powerMin = 0.02;
        double powerMax = 1.0;
        double verticalSpin = 1.0;
        double sideSpin = 1.0;
        double flatDrive = 1.0;
        double direction = 1.0;
        double height = 1.0;
        double heightMin = 0.0;
        double heightMax = 1.0;
    };

    struct SurfaceParticle {
        Vec2 position;
        Vec2 velocity;
        double height = 0.0;
        double verticalVelocity = 0.0;
        double life = 0.0;
        double lifetime = 0.0;
        double size = 2.0;
        double tone = 0.0;
    };

    struct OpponentContactPrediction {
        Vec2 position;
        double height = 0.0;
        double time = 0.0;
        int bounceCount = 0;
        bool valid = false;
    };

    static constexpr double CourtTop = 75.0;
    static constexpr double CourtBottom = 625.0;
    static constexpr double CourtHeight = CourtBottom - CourtTop;
    static constexpr double CourtWidth = CourtHeight * 10.97 / 23.77;
    static constexpr double CourtLeft = 640.0 - CourtWidth * 0.5;
    static constexpr double CourtRight = 640.0 + CourtWidth * 0.5;
    static constexpr double SinglesInset = CourtWidth * 1.37 / 10.97;
    static constexpr double SinglesLeft = CourtLeft + SinglesInset;
    static constexpr double SinglesRight = CourtRight - SinglesInset;
    static constexpr double NetY = (CourtTop + CourtBottom) * 0.5;
    static constexpr double ServiceOffset = CourtHeight * 6.40 / 23.77;
    static constexpr double PixelsPerMeter = CourtHeight / 23.77;
    static constexpr double BallRadiusM = 0.0335;
    static constexpr double LineHalfWidthM = 0.025;
    static constexpr double GravityMps2 = 9.81;
    static constexpr double NetHeightM = 0.914;
    static constexpr int MaxGesturePoints = 96;
    static constexpr int SurfaceWearColumns = 64;
    static constexpr int SurfaceWearRows = 96;
    static constexpr double SurfaceWearLeft = CourtLeft - 150.0;
    static constexpr double SurfaceWearRight = CourtRight + 150.0;
    static constexpr double SurfaceWearTop = CourtTop - 80.0;
    static constexpr double SurfaceWearBottom = CourtBottom + 140.0;

    tennis::MatchState match;
    tennis::BallPhysicsSystem ballPhysics;
    tennis::AudioSystem audio;
    tennis::PlayerProfile playerProfile;
    tennis::PlayerProfile opponentProfile = tennis::makeDefaultOpponentProfile();
    tennis::CourtTheme courtTheme;
    tennis::HudSettings hudSettings;
    tennis::ControlSettings controlSettings;
    tennis::CourtPreset activeCourtPreset = tennis::CourtPreset::BouncyHard;
    tennis::StaminaTuning staminaTuning;
    tennis::ShotSpeedTuning shotSpeedTuning;
    tennis::AiDifficulty aiDifficulty = tennis::AiDifficulty::Normal;
    std::array<tennis::RacketImpactConfig, 5> racketImpactProfiles{{
        {0.24, 0.14, 0.18, 0.16, 38.0, 5200.0, 3600.0},
        {0.00, 0.00, 0.00, 0.00, 38.0, 5200.0, 3600.0},
        {0.38, 0.08, 0.34, 0.30, 34.0, 4800.0, 3200.0, -6.5},
        {0.28, 0.16, 0.22, 0.20, 35.0, 5000.0, 3400.0},
        {0.10, 0.08, 0.08, 0.06, 42.0, 5200.0, 3600.0}
    }};
    tennis::RacketImpactConfig opponentImpactProfile{
        0.24, 0.14, 0.18, 0.16, 38.0, 4600.0, 3000.0
    };
    bool forehand = true;
    bool dragging = false;
    bool swingArmed = false;
    bool swingChargeLocked = false;
    bool splitStepAvailable = false;
    bool splitStepSpeedBuffActive = false;
    bool movingNow = false;
    bool sprintingNow = false;
    bool serveTossing = false;
    bool serveWindowOpened = false;
    bool opponentMovingNow = false;
    bool paused = false;
    bool pauseDrawing = false;
    bool quitRequested = false;
    PausePage pausePage = PausePage::Main;
    int selectedMappingProfile = 0;
    Uint64 pauseDrawStartedNs = 0;
    int playerAction = 0;
    int opponentAction = 0;
    double dragDuration = 0.0;
    double swingChargeTime = 0.0;
    double previousSwingContactQuality = 0.0;
    bool swingReachedYellowZone = false;
    double gesturePathLength = 0.0;
    double splitStepInputTime = 0.0;
    double splitStepReactionTime = 0.0;
    double splitStepBuffTime = 0.0;
    double serveHitWindowTime = 0.0;
    double playerActionTime = 0.0;
    double opponentActionTime = 0.0;
    double playerRunPhase = 0.0;
    double opponentRunPhase = 0.0;
    double playerSurfaceEffectTimer = 0.0;
    double opponentSurfaceEffectTimer = 0.0;
    double impactEffectTime = 0.0;
    double hitStopTime = 0.0;
    double impactHeight = 0.0;
    double cpuServeDelay = 0.8;
    int impactHitter = 0;
    Vec2 playerMoveDirection;
    Vec2 opponentMoveDirection;
    Vec2 playerVelocity;
    Vec2 opponentVelocity;
    Vec2 impactPosition;
    std::array<Vec2, MaxGesturePoints> gesturePoints{};
    std::array<MappingProfile, 5> mappingProfiles{{
        {1.00, 0.02, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, -0.70, 1.00},
        {1.03, 0.05, 1.00, 0.95, 1.02, 1.00, 1.00, 1.00, -0.22, 0.90},
        {0.94, 0.06, 0.96, 0.88, 0.90, 0.95, 1.00, 0.95, -0.70, 0.85},
        {0.97, 0.04, 0.98, 0.94, 0.94, 0.98, 1.00, 1.00, -0.70, 0.90},
        {1.05, 0.10, 1.00, 0.88, 0.90, 1.05, 1.00, 1.05, -0.70, 0.75}
    }};
    int gesturePointCount = 0;
    Vec2 dragStart;
    Vec2 dragCurrent;
    Vec2 pauseMouse;
    std::string statusMessage = "READY";
    double statusTime = 0.0;
    std::deque<ShotRecord> shotHistory;
    std::vector<SurfaceParticle> surfaceParticles;
    std::array<float, SurfaceWearColumns * SurfaceWearRows> surfaceWear{};
    mutable std::mt19937 random;

    void resetRally(bool selectServer = true);
    void updatePlayer(double delta, const tennis::PlayerInputCommand &input);
    void updateOpponent(double delta);
    void updateBall(double delta);
    void updateSurfaceParticles(double delta);
    void spawnSurfaceBallImpact(const Vec2 &position, const Vec2 &velocity,
                                double verticalImpactSpeed);
    void spawnSurfaceRunEffect(const Vec2 &position, const Vec2 &velocity,
                               bool fastMovement);
    void addSurfaceWear(const Vec2 &position, const Vec2 &velocity, double delta);
    void updateServeToss(double delta);
    void updateChargedSwing();
    void releasePlayerSwing();
    void recordGesturePoint(const Vec2 &point);
    void tryPlayerHit();
    void opponentHit();
    void cpuServe();
    void launchBall(const Vec2 &target, double shotSpeedMps, double spinRpm,
                    double sideSpinRpm = 0.0, double loft = 0.0);
    void strikeBall(const Vec2 &target, double racketShotSpeedMps, double spinRpm,
                    double sideSpinRpm, double loft,
                    const tennis::RacketImpactConfig &impact);
    void awardPoint(bool playerWon, const std::string &reason);
    void handleServeFault(const std::string &reason);
    bool ballInCourtHalf(bool topHalf) const;
    bool ballInServiceBox() const;
    double currentHeightQuality() const;
    double currentPositionQuality() const;
    Vec2 predictFirstBounce() const;
    OpponentContactPrediction predictOpponentContact(double movementSpeed,
                                                      double contactReach) const;
    bool aiShotIsSafe(const tennis::BallState &shot) const;
    bool opponentShouldLeaveBall() const;
    double gesturePower() const;
    double swingChargeProgress() const;
    double estimatedHitStaminaCost(double power, double complexity,
                                   double charge, bool smash) const;
    double gestureVerticalSpin() const;
    double gestureSideCurve() const;
    double gestureLoft() const;
    double gestureFlatDrive() const;
    double gestureSmoothness() const;
    double gestureDirection() const;
    tennis::PlayerInputCommand keyboardInput(const bool *keys) const;
    tennis::StrokeInputCommand mappedGesture(int profileIndex) const;
    void activateSplitStep();
    void updateForehand();
    void addShotRecord(const std::string &shot, double power, double verticalSpin,
                       double sideSpin, double loft, double direction, double flatDrive,
                       double accuracy, double staminaAtHit, int mappingProfile);
    Vec2 projectPoint(const Vec2 &worldPosition, double heightM = 0.0) const;
    double perspectiveScale(double worldY) const;
    double randomRange(double minimum, double maximum);
    void handlePauseClick(double x, double y);
    void renderPauseOverlay(SDL_Renderer *renderer) const;
    void applyServePositionRule();
    void applyMatchPreset();
    void applyCourtSurface();
};
