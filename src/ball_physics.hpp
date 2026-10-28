#pragma once

#include "match_state.hpp"

namespace tennis {

struct BallPhysicsConfig {
    double radiusM = 0.0335;
    double massKg = 0.057;
    double airDensity = 1.225;
    double areaM2 = 0.003526;
    double dragCoefficient = 0.55;
    double gravityMps2 = 9.81;
    double verticalSpinDecay = 0.045;
    double sideSpinDecay = 0.065;
    double bounceRestitutionBase = 0.68;
    double bounceRestitutionSpin = 0.16;
    double bounceForwardBase = 0.72;
    double bounceForwardSpin = 0.20;
    double netTapeThicknessM = 0.050;
    double netMeshNormalRetention = 0.035;
};

enum class NetCollisionOutcome {
    None,
    CordForward,
    CordBack,
    MeshStopped
};

// Effective racket/ball contact coefficients. They intentionally describe the
// whole racket + strings + swing rather than a rigid-body collision alone.
struct RacketImpactConfig {
    double horizontalVelocityRetention = 0.24;
    double verticalVelocityRetention = 0.14;
    double verticalSpinRetention = 0.18;
    double sideSpinRetention = 0.16;
    double maxOutgoingSpeedMps = 38.0;
    double maxVerticalSpinRpm = 5200.0;
    double maxSideSpinRpm = 3600.0;
    // Compact strokes such as volleys present a firmer, more closed racket
    // face than a full groundstroke even when the gesture itself is level.
    double neutralLaunchAngleOffsetDegrees = 0.0;
};

// Pure ball mechanics. It knows nothing about scoring, players, menus or SDL.
// The caller owns rule decisions after a net crossing or court impact.
class BallPhysicsSystem {
public:
    explicit BallPhysicsSystem(BallPhysicsConfig config = {});

    BallPhysicsConfig &config() { return config_; }
    const BallPhysicsConfig &config() const { return config_; }
    double aerodynamicLiftMps2(double speedMps, double rpm) const;
    void launch(BallState &ball, const Vec2 &target, double shotSpeedMps,
                double spinRpm, double sideSpinRpm, double loft,
                double pixelsPerMeter,
                double neutralLaunchAngleOffsetDegrees = 0.0) const;
    void strike(BallState &ball, const Vec2 &target, double racketShotSpeedMps,
                double generatedSpinRpm, double generatedSideSpinRpm, double loft,
                double pixelsPerMeter, const RacketImpactConfig &impact) const;
    void integrateAirborne(BallState &ball, double delta, double pixelsPerMeter) const;
    NetCollisionOutcome resolveNetCollision(BallState &ball,
                                            const Vec2 &previousPosition,
                                            double previousHeightM,
                                            double netY,
                                            double netHeightM,
                                            double netLeftX,
                                            double netRightX,
                                            double pixelsPerMeter) const;
    void applyCourtBounce(BallState &ball, double pixelsPerMeter) const;
    Vec2 predictFirstBounce(BallState ball, double pixelsPerMeter,
                            double step = 1.0 / 120.0, int maxFrames = 420) const;
    double predictedPeakHeight(const BallState &ball) const;

private:
    BallPhysicsConfig config_;
};

} // namespace tennis
