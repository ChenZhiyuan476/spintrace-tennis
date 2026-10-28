#include "ball_physics.hpp"

#include <algorithm>
#include <cmath>

namespace tennis {

namespace {
constexpr double Pi = 3.141592653589793;
}

BallPhysicsSystem::BallPhysicsSystem(BallPhysicsConfig config) : config_(config) {}

double BallPhysicsSystem::aerodynamicLiftMps2(double speedMps, double rpm) const {
    if (speedMps < 0.1 || std::abs(rpm) < 1.0) return 0.0;
    const double omega = std::abs(rpm) * 2.0 * Pi / 60.0;
    const double spinRatio = config_.radiusM * omega / speedMps;
    const double liftCoefficient = std::clamp(0.50 * spinRatio, 0.0, 0.35);
    return 0.5 * config_.airDensity * config_.areaM2 * liftCoefficient * speedMps * speedMps /
           config_.massKg;
}

void BallPhysicsSystem::launch(BallState &ball, const Vec2 &target, double shotSpeedMps,
                               double spinRpm, double sideSpinRpm, double loft,
                               double pixelsPerMeter,
                               double neutralLaunchAngleOffsetDegrees) const {
    ball.spinRpm = spinRpm;
    ball.sideSpinRpm = sideSpinRpm;
    ball.netHit = false;
    Vec2 aimDirection = (target - ball.position).normalized();
    if (aimDirection.lengthSquared() < 1e-8) {
        aimDirection = ball.lastHitter == 0 ? Vec2{0.0, -1.0} : Vec2{0.0, 1.0};
    }

    const double spinInput = std::clamp(spinRpm / 3600.0, -1.0, 1.0);
    // Treat the requested court target as an intended landing distance. Start
    // from the lower ballistic solution, then layer racket-face loft and spin
    // on top. This prevents faster strokes from sailing long merely because a
    // fixed launch angle was calibrated for a slower ball.
    double neutralAngleDegrees = 13.0;
    const double horizontalDistanceM = (target - ball.position).length() /
                                       std::max(1e-6, pixelsPerMeter);
    const double speedSquared = shotSpeedMps * shotSpeedMps;
    const double discriminant = speedSquared * speedSquared -
        config_.gravityMps2 * (config_.gravityMps2 * horizontalDistanceM * horizontalDistanceM -
                               2.0 * ball.height * speedSquared);
    if (horizontalDistanceM > 0.05 && discriminant >= 0.0) {
        const double tangent = (speedSquared - std::sqrt(discriminant)) /
            (config_.gravityMps2 * horizontalDistanceM);
        neutralAngleDegrees = std::atan(tangent) * 180.0 / Pi;
        neutralAngleDegrees += std::clamp((20.0 - shotSpeedMps) * 0.18, 0.0, 2.0);
    }
    double launchAngleDegrees = neutralAngleDegrees + loft * 12.0;
    if (spinInput > 0.0) launchAngleDegrees += spinInput * 2.2;
    if (spinInput < 0.0) launchAngleDegrees += -spinInput * 3.0;
    if (ball.serviceShot && ball.height <= 1.70) {
        launchAngleDegrees = 24.0 - (shotSpeedMps - 12.0) * 0.60 + loft * 8.0;
    } else if (ball.serviceShot) {
        // Use the actual contact-to-box distance. Player and computer stand at
        // different depths behind the baseline, so a fixed serve angle makes
        // one side systematically long.
        const double netClearanceDegrees = std::max(
            1.20, 2.30 - std::max(0.0, shotSpeedMps - 25.0) * 0.06);
        launchAngleDegrees = neutralAngleDegrees + netClearanceDegrees + loft * 5.0;
        const double serveSpinLiftScale = 3.0 -
            std::clamp((shotSpeedMps - 51.0) / 4.0, 0.0, 1.0) * 1.40;
        launchAngleDegrees += std::max(0.0, spinInput) * serveSpinLiftScale;
        // Flat and slice serves receive a little geometric clearance because
        // they do not get the higher launch that accompanies topspin.
        const double lowSpinClearance = 1.0 -
            std::clamp(std::max(0.0, spinInput) / 0.15, 0.0, 1.0);
        launchAngleDegrees += lowSpinClearance * 0.60;
    } else if (ball.height > 1.70) {
        launchAngleDegrees = 4.0 + loft * 8.0;
        launchAngleDegrees += std::max(0.0, spinInput) * 2.5;
    }
    if (loft >= 0.0 && !ball.serviceShot) {
        launchAngleDegrees += neutralLaunchAngleOffsetDegrees;
    }
    // A negative mapping value is an explicit closed racket face. Keep the
    // regular arc model for neutral/upward shots, but let serve, front-court
    // and overhead profiles produce a genuine downward initial velocity.
    if (loft < 0.0 && !ball.serviceShot) launchAngleDegrees = loft * 9.0;
    launchAngleDegrees = std::clamp(launchAngleDegrees, -7.0, 28.0);
    const double launchAngle = launchAngleDegrees * Pi / 180.0;
    const double horizontalSpeedMps = shotSpeedMps * std::cos(launchAngle);
    ball.velocity = aimDirection * horizontalSpeedMps * pixelsPerMeter;
    ball.verticalVelocity = shotSpeedMps * std::sin(launchAngle);
}

void BallPhysicsSystem::strike(BallState &ball, const Vec2 &target,
                               double racketShotSpeedMps, double generatedSpinRpm,
                               double generatedSideSpinRpm, double loft,
                               double pixelsPerMeter, const RacketImpactConfig &impact) const {
    const Vec2 incomingHorizontal = ball.velocity;
    const double incomingVertical = ball.verticalVelocity;
    const double incomingSpin = ball.spinRpm;
    const double incomingSideSpin = ball.sideSpinRpm;

    launch(ball, target, racketShotSpeedMps, generatedSpinRpm,
           generatedSideSpinRpm, loft, pixelsPerMeter,
           impact.neutralLaunchAngleOffsetDegrees);
    const double commandedVerticalVelocity = ball.verticalVelocity;

    // Reversing a share of the incoming velocity models energy returned by the
    // string bed. Cross-court pace therefore also nudges the outgoing direction.
    ball.velocity += incomingHorizontal * -impact.horizontalVelocityRetention;
    ball.verticalVelocity += -incomingVertical * impact.verticalVelocityRetention;
    if (loft >= 0.0) {
        // Incoming pace raises the final horizontal speed after launch().
        // Re-solve the vertical component using that actual outgoing speed so
        // the intended landing distance remains meaningful on fast returns.
        const double distanceM = (target - ball.position).length() /
                                 std::max(1e-6, pixelsPerMeter);
        const double horizontalSpeedMps = ball.velocity.length() / pixelsPerMeter;
        if (distanceM > 0.05 && horizontalSpeedMps > 0.5) {
            const double flightTime = distanceM / horizontalSpeedMps;
            const double ballisticVertical = -ball.height / flightTime +
                0.5 * config_.gravityMps2 * flightTime;
            const double faceAngle = (loft * 8.0 +
                                      impact.neutralLaunchAngleOffsetDegrees * 0.30) *
                                     Pi / 180.0;
            ball.verticalVelocity = ballisticVertical +
                horizontalSpeedMps * std::tan(faceAngle) -
                incomingVertical * impact.verticalVelocityRetention;
        }
    } else {
        // A deliberately closed face must remain a downward shot after the
        // incoming vertical momentum is combined with the racket impulse.
        // Otherwise a deeply falling lob can turn a requested smash back into
        // an upward launch simply because -incomingVertical is positive.
        const double retainedDownwardCommand =
            std::min(-0.05, commandedVerticalVelocity * 0.65);
        ball.verticalVelocity = std::min(ball.verticalVelocity, retainedDownwardCommand);
    }

    // Spin signs are relative to travel direction. When the ball reverses, the
    // physically retained rotation opposes the same named outgoing spin.
    ball.spinRpm = std::clamp(generatedSpinRpm - incomingSpin * impact.verticalSpinRetention,
                              -impact.maxVerticalSpinRpm, impact.maxVerticalSpinRpm);
    ball.sideSpinRpm = std::clamp(generatedSideSpinRpm -
                                  incomingSideSpin * impact.sideSpinRetention,
                                  -impact.maxSideSpinRpm, impact.maxSideSpinRpm);

    const Vec2 horizontalMps = ball.velocity / pixelsPerMeter;
    const double outgoingSpeed = std::sqrt(horizontalMps.lengthSquared() +
                                           ball.verticalVelocity * ball.verticalVelocity);
    if (outgoingSpeed > impact.maxOutgoingSpeedMps) {
        const double scale = impact.maxOutgoingSpeedMps / outgoingSpeed;
        ball.velocity *= scale;
        ball.verticalVelocity *= scale;
    }
}

void BallPhysicsSystem::integrateAirborne(BallState &ball, double delta,
                                          double pixelsPerMeter) const {
    Vec2 horizontalVelocityMps = ball.velocity / pixelsPerMeter;
    const double horizontalSpeed = horizontalVelocityMps.length();
    const double airSpeed = std::sqrt(horizontalSpeed * horizontalSpeed +
                                      ball.verticalVelocity * ball.verticalVelocity);
    const double dragK = 0.5 * config_.airDensity * config_.areaM2 * config_.dragCoefficient /
                         config_.massKg;
    if (airSpeed > 0.01) {
        const double dragFactor = std::max(0.0, 1.0 - dragK * airSpeed * delta);
        horizontalVelocityMps *= dragFactor;
        ball.verticalVelocity *= dragFactor;
    }

    const double postDragHorizontalSpeed = horizontalVelocityMps.length();
    const double postDragAirSpeed = std::sqrt(postDragHorizontalSpeed * postDragHorizontalSpeed +
                                              ball.verticalVelocity * ball.verticalVelocity);
    if (postDragHorizontalSpeed > 0.05) {
        const Vec2 travelDirection = horizontalVelocityMps / postDragHorizontalSpeed;
        const double verticalLift = aerodynamicLiftMps2(postDragAirSpeed, ball.spinRpm);
        const double verticalSpinSign = ball.spinRpm >= 0.0 ? 1.0 : -1.0;
        if (verticalLift > 0.0 && postDragAirSpeed > 0.05) {
            horizontalVelocityMps += travelDirection *
                (verticalSpinSign * verticalLift * ball.verticalVelocity / postDragAirSpeed * delta);
            ball.verticalVelocity += -verticalSpinSign * verticalLift *
                                     postDragHorizontalSpeed / postDragAirSpeed * delta;
        }

        const double sideLift = aerodynamicLiftMps2(postDragAirSpeed, ball.sideSpinRpm);
        if (sideLift > 0.0) {
            const Vec2 sideways{-travelDirection.y, travelDirection.x};
            const double sideSign = ball.sideSpinRpm >= 0.0 ? 1.0 : -1.0;
            horizontalVelocityMps += sideways * (sideSign * sideLift * delta);
        }
    }

    ball.verticalVelocity -= config_.gravityMps2 * delta;
    ball.velocity = horizontalVelocityMps * pixelsPerMeter;
    ball.position += ball.velocity * delta;
    ball.height += ball.verticalVelocity * delta;
    ball.spinRpm *= std::exp(-config_.verticalSpinDecay * delta);
    ball.sideSpinRpm *= std::exp(-config_.sideSpinDecay * delta);
}

NetCollisionOutcome BallPhysicsSystem::resolveNetCollision(
        BallState &ball, const Vec2 &previousPosition, double previousHeightM,
        double netY, double netHeightM, double netLeftX, double netRightX,
        double pixelsPerMeter) const {
    if (ball.netHit || pixelsPerMeter <= 0.0) return NetCollisionOutcome::None;

    const double travelY = ball.position.y - previousPosition.y;
    if (std::abs(travelY) < 1e-9) return NetCollisionOutcome::None;
    const double crossingTime = (netY - previousPosition.y) / travelY;
    if (crossingTime < 0.0 || crossingTime > 1.0) return NetCollisionOutcome::None;

    const double contactX = previousPosition.x +
                            (ball.position.x - previousPosition.x) * crossingTime;
    const double radiusPixels = config_.radiusM * pixelsPerMeter;
    if (contactX < netLeftX - radiusPixels || contactX > netRightX + radiusPixels)
        return NetCollisionOutcome::None;

    const double contactHeight = previousHeightM +
                                 (ball.height - previousHeightM) * crossingTime;
    // The ball clears only when its bottom passes above the top tape. Equality
    // counts as contact, matching the finite ball radius used for court lines.
    if (contactHeight - config_.radiusM > netHeightM)
        return NetCollisionOutcome::None;

    ball.netHit = true;
    const double incomingNormalVelocity = ball.velocity.y;
    const double direction = incomingNormalVelocity >= 0.0 ? 1.0 : -1.0;
    const double normalSpeedMps = std::abs(incomingNormalVelocity) / pixelsPerMeter;
    const double spinSurfaceSpeed = config_.radiusM * ball.spinRpm * 2.0 * Pi / 60.0;
    const bool contactedTape = contactHeight + config_.radiusM >=
                               netHeightM - config_.netTapeThicknessM;

    if (contactedTape) {
        // Grazing is zero when the centre is one radius below the cord and one
        // when it is one radius above it. Pace, upward motion and slice make a
        // forward tumble slightly more likely; topspin presses the ball down.
        const double grazing = std::clamp(
            (contactHeight - (netHeightM - config_.radiusM)) /
                (2.0 * config_.radiusM), 0.0, 1.0);
        const double paceAssist = std::clamp(normalSpeedMps / 40.0, 0.0, 0.12);
        const double verticalAssist = std::clamp(ball.verticalVelocity / 12.0, -0.08, 0.08);
        const double spinAssist = std::clamp(-ball.spinRpm / 5000.0, -0.04, 0.04);
        const double forwardScore = grazing + paceAssist + verticalAssist + spinAssist;

        if (forwardScore >= 0.52) {
            const double normalRetention = 0.18 + 0.48 *
                std::clamp((forwardScore - 0.52) / 0.60, 0.0, 1.0);
            ball.velocity.y = incomingNormalVelocity * normalRetention;
            ball.velocity.x *= 0.62 + 0.18 * grazing;
            const double upwardImpulse = normalSpeedMps * (0.08 + 0.16 * grazing) -
                                         spinSurfaceSpeed * 0.05;
            ball.verticalVelocity = std::clamp(ball.verticalVelocity * 0.20 + upwardImpulse,
                                               -2.5, 6.0);
            ball.position = {contactX, netY + direction * radiusPixels * 1.05};
            ball.height = std::max(config_.radiusM, contactHeight);
            ball.spinRpm *= 0.78;
            ball.sideSpinRpm *= 0.72;
            return NetCollisionOutcome::CordForward;
        }

        ball.velocity.y = -incomingNormalVelocity * (0.045 + 0.075 * grazing);
        ball.velocity.x *= 0.38;
        ball.verticalVelocity = std::min(-0.55, ball.verticalVelocity * 0.15 - 0.35);
        ball.position = {contactX, netY - direction * radiusPixels * 1.05};
        ball.height = std::max(config_.radiusM, contactHeight);
        ball.spinRpm *= 0.62;
        ball.sideSpinRpm *= 0.55;
        return NetCollisionOutcome::CordBack;
    }

    // The loose mesh absorbs nearly all normal energy and returns the ball to
    // the hitter's half. Gravity then decides the short drop to the ground.
    ball.velocity.y = -incomingNormalVelocity * config_.netMeshNormalRetention;
    ball.velocity.x *= 0.16;
    ball.verticalVelocity = std::min(-0.80, ball.verticalVelocity * 0.10 - 0.55);
    ball.position = {contactX, netY - direction * radiusPixels * 1.05};
    ball.height = std::max(config_.radiusM, contactHeight);
    ball.spinRpm *= 0.48;
    ball.sideSpinRpm *= 0.42;
    return NetCollisionOutcome::MeshStopped;
}

void BallPhysicsSystem::applyCourtBounce(BallState &ball, double pixelsPerMeter) const {
    ball.height = config_.radiusM;
    const double impactSpeed = std::max(ball.velocity.length() / pixelsPerMeter, 0.1);
    const double omega = ball.spinRpm * 2.0 * Pi / 60.0;
    const double spinRatio = config_.radiusM * omega / impactSpeed;
    ball.verticalVelocity = -ball.verticalVelocity *
        std::clamp(config_.bounceRestitutionBase + config_.bounceRestitutionSpin * spinRatio,
                   0.50, 0.85);
    ball.velocity *= std::clamp(config_.bounceForwardBase +
                                config_.bounceForwardSpin * spinRatio, 0.52, 0.92);
    ball.velocity.x += ball.sideSpinRpm / 2300.0 * 34.0;
    ball.spinRpm *= 0.72;
    ball.sideSpinRpm *= 0.68;
    ++ball.bounceCount;
}

Vec2 BallPhysicsSystem::predictFirstBounce(BallState ball, double pixelsPerMeter,
                                           double step, int maxFrames) const {
    for (int frame = 0; frame < maxFrames; ++frame) {
        integrateAirborne(ball, step, pixelsPerMeter);
        if (ball.height <= config_.radiusM && ball.verticalVelocity < 0.0) return ball.position;
    }
    return ball.position;
}

double BallPhysicsSystem::predictedPeakHeight(const BallState &ball) const {
    return config_.radiusM + ball.verticalVelocity * ball.verticalVelocity /
           (2.0 * config_.gravityMps2);
}

} // namespace tennis
