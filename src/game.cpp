#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr double Pi = 3.141592653589793;

Vec2 approachVelocity(Vec2 current, Vec2 target, double maximumChange) {
    const Vec2 change = target - current;
    const double distance = change.length();
    if (distance <= maximumChange || distance < 1e-9) return target;
    return current + change * (maximumChange / distance);
}

} // namespace

Game::Game() : random(std::random_device{}()) {
    surfaceParticles.reserve(240);
    applyCourtSurface();
    resetRally();
}

void Game::setCourtPreset(tennis::CourtPreset preset) {
    activeCourtPreset = preset;
    applyCourtSurface();
}

void Game::applyCourtSurface() {
    const tennis::CourtSurfaceProfile &surface =
        tennis::courtSurfaceProfile(activeCourtPreset);
    courtTheme.surface = surface.courtColor;
    courtTheme.surround = surface.surroundColor;
    tennis::BallPhysicsConfig &physics = ballPhysics.config();
    physics.bounceRestitutionBase = surface.bounceRestitutionBase;
    physics.bounceRestitutionSpin = surface.bounceRestitutionSpin;
    physics.bounceForwardBase = surface.bounceForwardBase;
    physics.bounceForwardSpin = surface.bounceForwardSpin;
    surfaceParticles.clear();
    surfaceWear.fill(0.0f);
    playerSurfaceEffectTimer = 0.0;
    opponentSurfaceEffectTimer = 0.0;
}

double Game::randomRange(double minimum, double maximum) {
    return std::uniform_real_distribution<double>(minimum, maximum)(random);
}

void Game::updateSurfaceParticles(double delta) {
    for (SurfaceParticle &particle : surfaceParticles) {
        particle.life -= delta;
        particle.position += particle.velocity * delta;
        particle.height += particle.verticalVelocity * delta;
        particle.verticalVelocity -= 3.8 * delta;
        particle.velocity *= std::exp(-4.6 * delta);
        if (particle.height < 0.0) {
            particle.height = 0.0;
            particle.verticalVelocity = 0.0;
            particle.velocity *= std::exp(-5.0 * delta);
        }
    }
    surfaceParticles.erase(
        std::remove_if(surfaceParticles.begin(), surfaceParticles.end(),
                       [](const SurfaceParticle &particle) { return particle.life <= 0.0; }),
        surfaceParticles.end());
}

void Game::spawnSurfaceBallImpact(const Vec2 &position, const Vec2 &velocity,
                                  double verticalImpactSpeed) {
    const tennis::SurfaceEffectProfile &effects =
        tennis::courtSurfaceProfile(activeCourtPreset).effects;
    if (effects.particleKind == tennis::SurfaceParticleKind::None ||
        effects.ballImpactAmount <= 0.0) return;

    const double horizontalSpeedMps = velocity.length() / PixelsPerMeter;
    const bool grassClipping = effects.particleKind == tennis::SurfaceParticleKind::GrassClipping;
    const int count = std::clamp(static_cast<int>(std::lround(
        (5.0 + verticalImpactSpeed * 0.75 + horizontalSpeedMps * 0.22) *
        effects.ballImpactAmount)), 1, 22);
    const Vec2 travel = velocity.lengthSquared() > 1e-6 ? velocity.normalized() : Vec2{};
    for (int index = 0; index < count; ++index) {
        if (surfaceParticles.size() >= 240) surfaceParticles.erase(surfaceParticles.begin());
        const double angle = randomRange(0.0, Pi * 2.0);
        const Vec2 radial{std::cos(angle), std::sin(angle)};
        SurfaceParticle particle;
        particle.position = position + radial * randomRange(0.0, 3.5);
        particle.velocity = radial * randomRange(grassClipping ? 7.0 : 10.0,
                                                 grassClipping ? 24.0 : 42.0) +
                            travel * randomRange(grassClipping ? 3.0 : 6.0,
                                                 grassClipping ? 10.0 : 20.0);
        particle.height = randomRange(0.0, grassClipping ? 0.012 : 0.025);
        particle.verticalVelocity = randomRange(grassClipping ? 0.10 : 0.18,
                                                grassClipping ? 0.32 : 0.62) +
                                    std::clamp(verticalImpactSpeed, 0.0, 12.0) *
                                    (grassClipping ? 0.008 : 0.018);
        particle.lifetime = randomRange(grassClipping ? 0.16 : 0.28,
                                        grassClipping ? 0.34 : 0.58);
        particle.life = particle.lifetime;
        particle.size = randomRange(grassClipping ? 1.1 : 2.0,
                                    grassClipping ? 2.0 : 4.4);
        particle.tone = randomRange(grassClipping ? -0.06 : -0.08,
                                    grassClipping ? 0.10 : 0.18);
        surfaceParticles.push_back(particle);
    }
}

void Game::spawnSurfaceRunEffect(const Vec2 &position, const Vec2 &velocity,
                                 bool fastMovement) {
    const tennis::SurfaceEffectProfile &effects =
        tennis::courtSurfaceProfile(activeCourtPreset).effects;
    if (effects.particleKind == tennis::SurfaceParticleKind::None ||
        effects.movementAmount <= 0.0 || velocity.lengthSquared() < 1.0) return;

    const Vec2 direction = velocity.normalized();
    const Vec2 sideways{-direction.y, direction.x};
    const bool grassClipping = effects.particleKind == tennis::SurfaceParticleKind::GrassClipping;
    const int count = std::max(1, static_cast<int>(std::lround(
        (fastMovement ? 3.0 : 2.0) * effects.movementAmount)));
    for (int index = 0; index < count; ++index) {
        if (surfaceParticles.size() >= 240) surfaceParticles.erase(surfaceParticles.begin());
        SurfaceParticle particle;
        particle.position = position - direction * randomRange(4.0, 9.0) +
                            sideways * randomRange(-5.0, 5.0);
        particle.velocity = direction * -randomRange(grassClipping ? 5.0 : 8.0,
                                                     fastMovement ? (grassClipping ? 19.0 : 32.0)
                                                                  : (grassClipping ? 14.0 : 23.0)) +
                            sideways * randomRange(grassClipping ? -10.0 : -18.0,
                                                   grassClipping ? 10.0 : 18.0);
        particle.height = randomRange(0.0, grassClipping ? 0.008 : 0.015);
        particle.verticalVelocity = randomRange(grassClipping ? 0.08 : 0.12,
                                                fastMovement ? (grassClipping ? 0.28 : 0.46)
                                                             : (grassClipping ? 0.22 : 0.34));
        particle.lifetime = randomRange(grassClipping ? 0.14 : 0.18,
                                        fastMovement ? (grassClipping ? 0.30 : 0.42)
                                                     : (grassClipping ? 0.24 : 0.34));
        particle.life = particle.lifetime;
        particle.size = randomRange(grassClipping ? 1.0 : 1.5,
                                    fastMovement ? (grassClipping ? 1.8 : 3.4)
                                                 : (grassClipping ? 1.5 : 2.8));
        particle.tone = randomRange(grassClipping ? -0.05 : -0.10,
                                    grassClipping ? 0.08 : 0.14);
        surfaceParticles.push_back(particle);
    }
}

void Game::addSurfaceWear(const Vec2 &position, const Vec2 &velocity, double delta) {
    const tennis::CourtSurfaceProfile &surface =
        tennis::courtSurfaceProfile(activeCourtPreset);
    if (surface.effects.wearRate <= 0.0 || velocity.length() < 28.0) return;
    if (position.x < SurfaceWearLeft || position.x > SurfaceWearRight ||
        position.y < SurfaceWearTop || position.y > SurfaceWearBottom) return;

    const double xRatio = (position.x - SurfaceWearLeft) /
                          (SurfaceWearRight - SurfaceWearLeft);
    const double yRatio = (position.y - SurfaceWearTop) /
                          (SurfaceWearBottom - SurfaceWearTop);
    const int centerColumn = std::clamp(static_cast<int>(xRatio * SurfaceWearColumns),
                                        0, SurfaceWearColumns - 1);
    const int centerRow = std::clamp(static_cast<int>(yRatio * SurfaceWearRows),
                                     0, SurfaceWearRows - 1);
    const double speedRatio = std::clamp(velocity.length() /
        std::max(1.0, 120.0 * surface.movementSpeedScale), 0.20, 1.55);
    const float deposit = static_cast<float>(delta * surface.effects.wearRate *
                                              (0.08 + speedRatio * 0.16));
    // The wear texture is deliberately denser than the old 32x48 grid. Spread
    // each footprint over a similar physical radius so repeated paths merge
    // into soft lanes rather than revealing individual cells.
    const int radius = speedRatio > 0.92 ? 4 : 3;
    for (int rowOffset = -radius; rowOffset <= radius; ++rowOffset) {
        for (int columnOffset = -radius; columnOffset <= radius; ++columnOffset) {
            const int column = centerColumn + columnOffset;
            const int row = centerRow + rowOffset;
            if (column < 0 || column >= SurfaceWearColumns ||
                row < 0 || row >= SurfaceWearRows) continue;
            const double distanceSquared = columnOffset * columnOffset +
                                           rowOffset * rowOffset;
            const float falloff = static_cast<float>(std::exp(-0.18 * distanceSquared));
            float &wear = surfaceWear[static_cast<size_t>(row * SurfaceWearColumns + column)];
            wear = std::min(0.92f, wear + deposit * falloff);
        }
    }
}

void Game::resetRally(bool selectServer) {
    if (selectServer) {
        match.serveFaultCount = 0;
        const int serverRotation = match.score.mode == tennis::ScoreMode::Sets
            ? match.completedGames : match.completedRallies;
        match.currentServerPlayer = match.rules.serveMode == tennis::ServeMode::Player ||
                              (match.rules.serveMode == tennis::ServeMode::Alternate && serverRotation % 2 == 0);
        if (match.rules.serveMode == tennis::ServeMode::Cpu) match.currentServerPlayer = false;
    }
    match.playerPosition = {640, CourtBottom + 92.0};
    match.opponentPosition = {640, 35};
    if (!match.currentServerPlayer) {
        const bool deuceSide = ((match.score.playerPoints + match.score.opponentPoints) % 2 == 0);
        match.opponentPosition.x = deuceSide ? 675.0 : 605.0;
        // Stand on the quarter of the receiving baseline that corresponds to
        // the diagonal service box instead of resetting in the back-centre.
        match.playerPosition.x = deuceSide ? (SinglesLeft + 640.0) * 0.5
                                           : (640.0 + SinglesRight) * 0.5;
        match.playerPosition.y = CourtBottom + 14.0;
    }
    forehand = true;
    match.ball.position = match.currentServerPlayer ? match.playerPosition + Vec2{22, -6}
                                        : match.opponentPosition + Vec2{-16, 6};
    match.ball.velocity = {};
    match.ball.height = 0.9;
    match.ball.verticalVelocity = 0.0;
    match.ball.spinRpm = 0.0;
    match.ball.sideSpinRpm = 0.0;
    match.ball.bounceCount = 1;
    match.ball.lastHitter = match.currentServerPlayer ? 1 : 0;
    match.ball.inPlay = false;
    match.ball.serviceShot = false;
    match.ball.serviceFromBottom = match.currentServerPlayer;
    match.ball.serveReturnPending = false;
    match.ball.netHit = false;
    playerVelocity = {};
    opponentVelocity = {};
    dragging = false;
    swingArmed = false;
    swingChargeLocked = false;
    swingChargeTime = 0.0;
    previousSwingContactQuality = 0.0;
    swingReachedYellowZone = false;
    splitStepAvailable = false;
    splitStepSpeedBuffActive = false;
    splitStepInputTime = 0.0;
    splitStepReactionTime = 0.0;
    splitStepBuffTime = 0.0;
    serveTossing = false;
    serveWindowOpened = false;
    serveHitWindowTime = 0.0;
    playerAction = 0;
    opponentAction = 0;
    playerActionTime = 0.0;
    opponentActionTime = 0.0;
    impactEffectTime = 0.0;
    hitStopTime = 0.0;
    cpuServeDelay = 0.8;
    applyServePositionRule();
}

void Game::update(double delta, const bool *keys) {
    if (paused) return;
    if (dragging) dragDuration += delta;
    impactEffectTime = std::max(0.0, impactEffectTime - delta);
    updateSurfaceParticles(delta);
    if (hitStopTime > 0.0) {
        hitStopTime = std::max(0.0, hitStopTime - delta);
        return;
    }
    const bool reactionWasOpen = splitStepReactionTime > 0.0;
    splitStepInputTime = std::max(0.0, splitStepInputTime - delta);
    splitStepReactionTime = std::max(0.0, splitStepReactionTime - delta);
    if (reactionWasOpen && splitStepReactionTime <= 0.0 && splitStepBuffTime <= 0.0) splitStepAvailable = false;
    splitStepBuffTime = std::max(0.0, splitStepBuffTime - delta);
    playerActionTime = std::max(0.0, playerActionTime - delta);
    opponentActionTime = std::max(0.0, opponentActionTime - delta);
    updatePlayer(delta, keyboardInput(keys));
    if (dragging && swingArmed && !swingChargeLocked) {
        const double chargeRate = sprintingNow
            ? controlSettings.sprintChargeRate
            : (movingNow ? controlSettings.movingChargeRate : 1.0);
        swingChargeTime += delta * chargeRate;
    }
    updateOpponent(delta);
    // Check once before and once after physics. The first check catches a ball
    // at the yellow edge before a large frame step can move it out of reach;
    // the second catches entry into the green region during this frame.
    updateChargedSwing();
    if (match.ball.inPlay) {
        updateBall(delta);
    } else if (serveTossing) {
        updateServeToss(delta);
    } else if (!match.currentServerPlayer) {
        cpuServeDelay = std::max(0.0, cpuServeDelay - delta);
        match.ball.position = match.opponentPosition + Vec2{-16, 6};
        match.ball.height = 1.05;
        if (cpuServeDelay <= 0.0) cpuServe();
    } else {
        match.ball.position = match.playerPosition + Vec2{22, -6};
        match.ball.height = 0.9;
    }
    updateChargedSwing();
    if (statusTime > 0.0) statusTime -= delta;
}

void Game::updatePlayer(double delta, const tennis::PlayerInputCommand &input) {
    Vec2 direction = input.movement;
    if (direction.lengthSquared() > 1.0) direction = direction.normalized();
    const bool moving = direction.lengthSquared() > 0.01;
    const bool sprinting = input.sprintHeld && moving && match.playerStamina > 0.5;
    const Vec2 projectedHere = projectPoint(match.playerPosition);
    const double horizontalProjection = std::max(0.01, projectPoint(match.playerPosition + Vec2{1.0, 0.0}).distanceTo(projectedHere));
    const double verticalProjection = std::max(0.01, projectPoint(match.playerPosition + Vec2{0.0, 1.0}).distanceTo(projectedHere));
    const double forwardBackCompensation = std::clamp(horizontalProjection / verticalProjection, 1.0, 3.2);
    const Vec2 visualSpeedDirection{direction.x, direction.y * forwardBackCompensation};
    const double staminaRatio = std::clamp(match.playerStamina /
        std::max(1.0, playerProfile.attributes.staminaCapacity), 0.0, 1.0);
    const double fatigueSpeedScale = staminaTuning.movementScaleAt(staminaRatio);
    const tennis::CourtSurfaceProfile &surface =
        tennis::courtSurfaceProfile(activeCourtPreset);
    const double splitStepSpeedScale = splitStepSpeedBuffActive
        ? staminaTuning.splitStepSpeedMultiplier : 1.0;
    const double movementScale = (sprinting ? playerProfile.attributes.sprintSpeed
                                            : playerProfile.attributes.movementSpeed) *
                                 fatigueSpeedScale * surface.movementSpeedScale *
                                 splitStepSpeedScale;
    const Vec2 targetVelocity = visualSpeedDirection *
        (sprinting ? 172.0 : 120.0) * movementScale;
    const double directionLength = std::max(0.01, direction.length());
    const double inputProjectionScale = moving
        ? visualSpeedDirection.length() / directionLength : 1.0;
    const double unprojectedVelocityLength = std::sqrt(
        playerVelocity.x * playerVelocity.x +
        (playerVelocity.y / forwardBackCompensation) *
        (playerVelocity.y / forwardBackCompensation));
    const double coastProjectionScale = playerVelocity.length() /
        std::max(0.01, unprojectedVelocityLength);
    const double projectionScale = std::clamp(
        moving ? inputProjectionScale : coastProjectionScale, 1.0, 3.2);
    const double responseMps2 = moving ? surface.accelerationMps2
                                       : surface.decelerationMps2;
    playerVelocity = approachVelocity(
        playerVelocity, targetVelocity,
        responseMps2 * PixelsPerMeter * projectionScale * delta);
    match.playerPosition += playerVelocity * delta;

    const Vec2 beforeBoundaryClamp = match.playerPosition;
    match.playerPosition.x = std::clamp(match.playerPosition.x, CourtLeft - 138.0, CourtRight + 138.0);
    match.playerPosition.y = std::clamp(match.playerPosition.y, NetY + 8.0, CourtBottom + 132.0);
    if (match.playerPosition.x != beforeBoundaryClamp.x) playerVelocity.x = 0.0;
    if (match.playerPosition.y != beforeBoundaryClamp.y) playerVelocity.y = 0.0;
    const Vec2 beforeServeClamp = match.playerPosition;
    applyServePositionRule();
    if (match.playerPosition.x != beforeServeClamp.x) playerVelocity.x = 0.0;
    if (match.playerPosition.y != beforeServeClamp.y) playerVelocity.y = 0.0;

    movingNow = playerVelocity.lengthSquared() > 4.0;
    sprintingNow = sprinting && movingNow;
    playerMoveDirection = movingNow ? playerVelocity.normalized() : Vec2{};
    if (movingNow) {
        const double animationSpeed = std::clamp(
            playerVelocity.length() / std::max(1.0, targetVelocity.length()), 0.25, 1.2);
        playerRunPhase += delta * (sprintingNow ? 13.0 : 9.0) * animationSpeed;
    }
    const tennis::SurfaceEffectProfile &effects = surface.effects;
    if (effects.movementAmount > 0.0 && movingNow && playerVelocity.length() > 34.0) {
        playerSurfaceEffectTimer -= delta;
        if (playerSurfaceEffectTimer <= 0.0) {
            spawnSurfaceRunEffect(match.playerPosition, playerVelocity, sprintingNow);
            playerSurfaceEffectTimer = (sprintingNow ? 0.075 : 0.115) /
                                         std::max(0.25, effects.movementAmount);
        }
    } else {
        playerSurfaceEffectTimer = 0.0;
    }
    addSurfaceWear(match.playerPosition, playerVelocity, delta);
    updateForehand();

    const double endurance = std::max(0.20, playerProfile.attributes.endurance);
    if (sprintingNow) {
        match.playerStamina = std::max(0.0, match.playerStamina -
            staminaTuning.sprintCostPerSecond / endurance * delta);
    } else if (movingNow) {
        match.playerStamina = std::max(0.0, match.playerStamina -
            staminaTuning.runningCostPerSecond / endurance * delta);
    } else {
        match.playerStamina = std::min(playerProfile.attributes.staminaCapacity,
            match.playerStamina + staminaTuning.idleRecoveryPerSecond *
                                  playerProfile.attributes.staminaRecovery * delta);
    }
}

void Game::updateServeToss(double delta) {
    match.ball.position = match.playerPosition + Vec2{forehand ? 15.0 : -15.0, -8.0};
    match.ball.verticalVelocity -= GravityMps2 * delta;
    match.ball.height += match.ball.verticalVelocity * delta;

    if (!serveWindowOpened && match.ball.height >= shotSpeedTuning.serveContactHeight - 0.14) {
        serveWindowOpened = true;
        // Keep the valid window open through the descending edge of the green
        // zone so auto-hit can release exactly as the toss leaves it.
        serveHitWindowTime = 0.64;
        statusMessage = "SERVE GREEN WINDOW | AUTO SWING ARMED";
        statusTime = 0.34;
    }
    if (serveHitWindowTime > 0.0) serveHitWindowTime = std::max(0.0, serveHitWindowTime - delta);

    if (match.ball.height <= 0.90 && match.ball.verticalVelocity < 0.0) {
        serveTossing = false;
        serveWindowOpened = false;
        serveHitWindowTime = 0.0;
        match.ball.height = 0.90;
        match.ball.verticalVelocity = 0.0;
        statusMessage = "TOSS MISSED | RIGHT CLICK TO TOSS AGAIN";
        statusTime = 1.2;
    }
}

void Game::updateForehand() {
    if (!match.ball.inPlay || match.ball.lastHitter != 1) return;
    constexpr double switchMargin = 10.0;
    bool desired = forehand;
    if (match.ball.position.x > match.playerPosition.x + switchMargin) desired = true;
    if (match.ball.position.x < match.playerPosition.x - switchMargin) desired = false;
    if (desired != forehand) {
        forehand = desired;
        match.playerStamina = std::max(0.0, match.playerStamina - 1.1 /
                                       std::max(0.20, playerProfile.attributes.endurance));
    }
}

void Game::activateSplitStep() {
    splitStepAvailable = false;
    splitStepInputTime = 0.0;
    splitStepReactionTime = 0.0;
    splitStepBuffTime = 1.35;
    splitStepSpeedBuffActive = true;
    statusMessage = "GOOD SPLIT STEP | SPEED UP UNTIL HIT";
    statusTime = 1.1;
}

void Game::updateOpponent(double delta) {
    const tennis::AiDifficultyProfile difficulty = tennis::aiDifficultyProfile(aiDifficulty);
    const tennis::CourtSurfaceProfile &surface =
        tennis::courtSurfaceProfile(activeCourtPreset);
    const double staminaRatio = std::clamp(match.opponentStamina /
        std::max(1.0, opponentProfile.attributes.staminaCapacity), 0.0, 1.0);
    const double fatigueSpeedScale = staminaTuning.movementScaleAt(staminaRatio);
    const double contactReach = 50.0 + difficulty.movementSpeedScale * 8.0;
    const double predictionMoveSpeed = 148.0 * opponentProfile.attributes.sprintSpeed *
        fatigueSpeedScale * difficulty.movementSpeedScale * surface.movementSpeedScale;
    const OpponentContactPrediction contactPrediction =
        predictOpponentContact(predictionMoveSpeed, contactReach);
    Vec2 target{640, CourtTop - 35.0};
    const bool leaveIncomingBall = opponentShouldLeaveBall();
    if (match.ball.inPlay && match.ball.lastHitter == 0 && !leaveIncomingBall) {
        if (contactPrediction.valid) {
            // Run to a future reachable contact point, not to the ball's current
            // position. This leaves enough preparation time against fast shots
            // and prevents the AI from swinging at ankle- or shoulder-height
            // balls merely because they entered a fixed radius.
            target.x = std::clamp(contactPrediction.position.x,
                                  CourtLeft - 60.0, CourtRight + 60.0);
            target.y = std::clamp(contactPrediction.position.y,
                                  25.0, NetY - 8.0);
        } else if (match.ball.bounceCount == 0) {
            const Vec2 landing = predictFirstBounce();
            target.x = std::clamp(landing.x,
                                  CourtLeft - 55.0, CourtRight + 55.0);
            target.y = std::clamp(landing.y - 10.0,
                                  CourtTop + 18.0, NetY - 8.0);
        } else {
            // After the bounce, follow the live trajectory with a small lead
            // that scales with difficulty.
            const double leadSeconds = 0.055 + 0.035 * difficulty.movementSpeedScale;
            target.x = std::clamp(match.ball.position.x +
                                  match.ball.velocity.x * leadSeconds,
                                  CourtLeft - 60.0, CourtRight + 60.0);
            target.y = std::clamp(match.ball.position.y +
                                  match.ball.velocity.y * leadSeconds - 10.0,
                                  28.0, NetY - 8.0);
        }
    }
    const Vec2 targetDelta = target - match.opponentPosition;
    const double targetDistance = targetDelta.length();
    const Vec2 direction = targetDistance > 1e-9 ? targetDelta / targetDistance : Vec2{};
    const bool opponentSprinting = targetDistance > 78.0 && match.opponentStamina > 0.5;
    const double baseSpeed = opponentSprinting ? 148.0 : 108.0;
    const double attributeSpeed = opponentSprinting ? opponentProfile.attributes.sprintSpeed
                                                    : opponentProfile.attributes.movementSpeed;
    const double desiredSpeed = std::min(baseSpeed * attributeSpeed * fatigueSpeedScale *
                                         difficulty.movementSpeedScale *
                                         surface.movementSpeedScale, targetDistance * 6.0);
    const Vec2 targetVelocity = direction * desiredSpeed;
    const bool activelyMoving = targetDistance > 0.5;
    const double responseMps2 = activelyMoving ? surface.accelerationMps2
                                               : surface.decelerationMps2;
    opponentVelocity = approachVelocity(opponentVelocity, targetVelocity,
        responseMps2 * PixelsPerMeter * delta);
    match.opponentPosition += opponentVelocity * delta;
    const Vec2 beforeBoundaryClamp = match.opponentPosition;
    match.opponentPosition.x = std::clamp(match.opponentPosition.x, CourtLeft - 70.0, CourtRight + 70.0);
    match.opponentPosition.y = std::clamp(match.opponentPosition.y, 25.0, NetY - 8.0);
    if (match.opponentPosition.x != beforeBoundaryClamp.x) opponentVelocity.x = 0.0;
    if (match.opponentPosition.y != beforeBoundaryClamp.y) opponentVelocity.y = 0.0;
    opponentMovingNow = opponentVelocity.lengthSquared() > 4.0;
    opponentMoveDirection = opponentMovingNow ? opponentVelocity.normalized() : Vec2{};
    if (opponentMovingNow) {
        opponentRunPhase += delta * 8.5 *
            std::clamp(opponentVelocity.length() / std::max(1.0, 108.0 * surface.movementSpeedScale),
                       0.25, 1.2);
    }
    const tennis::SurfaceEffectProfile &effects = surface.effects;
    if (effects.movementAmount > 0.0 && opponentMovingNow && opponentVelocity.length() > 34.0) {
        opponentSurfaceEffectTimer -= delta;
        if (opponentSurfaceEffectTimer <= 0.0) {
            const bool fastMovement = opponentVelocity.length() > 92.0 * surface.movementSpeedScale;
            spawnSurfaceRunEffect(match.opponentPosition, opponentVelocity, fastMovement);
            opponentSurfaceEffectTimer = (fastMovement ? 0.080 : 0.120) /
                                           std::max(0.25, effects.movementAmount);
        }
    } else {
        opponentSurfaceEffectTimer = 0.0;
    }
    addSurfaceWear(match.opponentPosition, opponentVelocity, delta);
    const double opponentEndurance = std::max(0.20, opponentProfile.attributes.endurance);
    if (opponentMovingNow) {
        const double movementCost = opponentSprinting ? staminaTuning.sprintCostPerSecond
                                                      : staminaTuning.runningCostPerSecond;
        match.opponentStamina = std::max(0.0, match.opponentStamina -
            movementCost / opponentEndurance * delta);
    } else {
        match.opponentStamina = std::min(opponentProfile.attributes.staminaCapacity,
            match.opponentStamina + staminaTuning.idleRecoveryPerSecond *
                                      opponentProfile.attributes.staminaRecovery * delta);
    }
    const double ballDistance = match.opponentPosition.distanceTo(match.ball.position);
    const bool predictedWindow = contactPrediction.valid && contactPrediction.time <= 0.115;
    const bool emergencyWindow = !contactPrediction.valid && ballDistance < contactReach * 0.62 &&
        match.ball.height > 0.28 && match.ball.height < 2.30;
    const bool reachable = match.ball.inPlay && match.ball.lastHitter == 0 &&
            match.ball.position.y < NetY && !match.ball.serveReturnPending &&
            !leaveIncomingBall && ballDistance < contactReach &&
            match.ball.height > 0.30 && match.ball.height < 2.35 &&
            (predictedWindow || emergencyWindow);
    if (reachable) opponentHit();
}

void Game::opponentHit() {
    const tennis::AiDifficultyProfile difficulty = tennis::aiDifficultyProfile(aiDifficulty);
    const double staminaRatio = std::clamp(match.opponentStamina /
        std::max(1.0, opponentProfile.attributes.staminaCapacity), 0.0, 1.0);
    const double staminaQuality = staminaTuning.accuracyQualityAt(staminaRatio);
    Vec2 target{match.playerPosition.x + randomRange(-65.0, 65.0), randomRange(NetY + 120.0, CourtBottom - 18.0)};
    const double fatigueError = (1.0 - staminaQuality) * 145.0;
    target.x = std::clamp(target.x + randomRange(-20.0, 20.0),
                          SinglesLeft + 16.0, SinglesRight - 16.0);
    target.x = std::clamp(target.x + randomRange(-difficulty.targetErrorPixels,
                                                  difficulty.targetErrorPixels),
                          SinglesLeft + 12.0, SinglesRight - 12.0);
    target.y = std::clamp(target.y + randomRange(-difficulty.targetErrorPixels * 0.55,
                                                  difficulty.targetErrorPixels * 0.55),
                          NetY + 82.0, CourtBottom - 14.0);
    // Only genuine fatigue can push an otherwise sensible AI aim outside.
    target.x += randomRange(-fatigueError, fatigueError);
    target.y += randomRange(-fatigueError * 0.75, fatigueError);
    tennis::BallState incoming = match.ball;
    incoming.lastHitter = 1;
    incoming.bounceCount = 0;
    incoming.serviceShot = false;
    incoming.serveReturnPending = false;
    incoming.netHit = false;
    opponentAction = match.ball.position.x < match.opponentPosition.x ? 1 : 2;
    opponentActionTime = 0.36;
    impactPosition = match.ball.position;
    impactHeight = match.ball.height;
    impactHitter = 1;
    impactEffectTime = 0.12;
    hitStopTime = 0.028;
    if (splitStepAvailable && splitStepInputTime > 0.0) {
        activateSplitStep();
    } else if (splitStepAvailable) {
        splitStepReactionTime = 0.16;
    }
    const double fatiguePowerScale = 0.72 +
        0.28 * staminaTuning.movementScaleAt(staminaRatio);
    const double shotSpeed = randomRange(shotSpeedTuning.cpuGroundMin,
                                         shotSpeedTuning.cpuGroundMax) * fatiguePowerScale *
                             difficulty.ballSpeedScale;
    const double verticalSpin = randomRange(-250.0, 2200.0);
    const double sideSpin = randomRange(-450.0, 450.0);
    const double baseLoft = randomRange(difficulty.rallyLoftMin,
                                        difficulty.rallyLoftMax);
    const std::array<double, 6> loftAdjustments{{0.0, 0.045, 0.090, 0.140, 0.200, 0.270}};
    bool safeShotFound = false;
    for (double loftAdjustment : loftAdjustments) {
        tennis::BallState candidate = incoming;
        ballPhysics.strike(candidate, target, shotSpeed, verticalSpin, sideSpin,
                           baseLoft + loftAdjustment, PixelsPerMeter,
                           opponentImpactProfile);
        if (aiShotIsSafe(candidate)) {
            match.ball = candidate;
            safeShotFound = true;
            break;
        }
    }
    if (!safeShotFound) {
        // A very fast or deeply falling incoming ball can make the requested
        // aggressive target geometrically unsafe. Use a central, topspin-heavy
        // recovery ball rather than knowingly driving it into the net.
        const Vec2 recoveryTarget{640.0, NetY + 150.0};
        const double recoverySpeed = std::min(shotSpeed, 26.0);
        const std::array<double, 5> recoveryLofts{{0.08, 0.14, 0.20, 0.27, 0.34}};
        for (double recoveryLoft : recoveryLofts) {
            tennis::BallState candidate = incoming;
            ballPhysics.strike(candidate, recoveryTarget, recoverySpeed, 1800.0,
                               sideSpin * 0.25, recoveryLoft, PixelsPerMeter,
                               opponentImpactProfile);
            if (aiShotIsSafe(candidate)) {
                match.ball = candidate;
                safeShotFound = true;
                break;
            }
        }
    }
    if (!safeShotFound) {
        // This final candidate remains physically simulated; it only removes
        // incoming-pace retention so an extreme return cannot force a net ball.
        tennis::RacketImpactConfig controlledImpact = opponentImpactProfile;
        controlledImpact.horizontalVelocityRetention = 0.08;
        controlledImpact.verticalVelocityRetention = 0.05;
        match.ball = incoming;
        ballPhysics.strike(match.ball, {640.0, NetY + 145.0}, 22.0, 2100.0,
                           0.0, 0.22, PixelsPerMeter, controlledImpact);
    }
    const double soundPower = std::clamp((shotSpeed - 16.0) / 20.0, 0.0, 1.0);
    const double soundSpin = std::clamp(std::abs(verticalSpin) / 3600.0 * 0.68 +
                                        std::abs(sideSpin) / 2300.0 * 0.48,
                                        0.0, 1.0);
    audio.playRacketHit(soundPower, soundSpin, 0.82 * staminaQuality, false);
    const double power = std::clamp((shotSpeed - shotSpeedTuning.playerGroundBase) /
                                    shotSpeedTuning.playerGroundPower, 0.0, 1.0);
    const double complexity = std::clamp(std::abs(verticalSpin) / 3600.0 * 0.55 +
                                         std::abs(sideSpin) / 2300.0 * 0.45, 0.0, 1.0);
    const double hitCost = (staminaTuning.baseHitCost + 7.0 * std::pow(power, 1.35) +
                            7.5 * std::pow(0.62, 1.55) + complexity * 3.0) /
                           std::max(0.20, opponentProfile.attributes.endurance);
    match.opponentStamina = std::max(0.0, match.opponentStamina - hitCost);
}

void Game::cpuServe() {
    const tennis::AiDifficultyProfile difficulty = tennis::aiDifficultyProfile(aiDifficulty);
    if (splitStepAvailable && splitStepInputTime > 0.0) {
        activateSplitStep();
    } else {
        splitStepAvailable = true;
        splitStepReactionTime = 0.16;
    }
    const double staminaRatio = std::clamp(match.opponentStamina /
        std::max(1.0, opponentProfile.attributes.staminaCapacity), 0.0, 1.0);
    const double staminaQuality = staminaTuning.accuracyQualityAt(staminaRatio);
    const bool deuceSide = ((match.score.playerPoints + match.score.opponentPoints) % 2 == 0);
    match.opponentPosition.x = deuceSide ? 675.0 : 605.0;
    match.ball.position = match.opponentPosition + Vec2{deuceSide ? -14.0 : 14.0, 6.0};
    match.ball.height = shotSpeedTuning.cpuServeContactHeight;
    match.ball.verticalVelocity = 0.0;
    match.ball.inPlay = true;
    match.ball.serviceShot = true;
    match.ball.serviceFromBottom = false;
    match.ball.serveReturnPending = true;
    match.ball.serviceTargetLeft = deuceSide;
    match.ball.lastHitter = 1;
    match.ball.bounceCount = 0;
    match.ball.netHit = false;
    const double targetX = match.rules.serveRulesEnabled
        ? (match.ball.serviceTargetLeft ? (SinglesLeft + 640.0) * 0.5 : (640.0 + SinglesRight) * 0.5)
        : randomRange(CourtLeft + 24.0, CourtRight - 24.0);
    const double fatigueError = (1.0 - staminaQuality) * 72.0;
    const double serveError = difficulty.serveErrorPixels + fatigueError;
    Vec2 target{targetX + randomRange(-serveError, serveError), match.rules.serveRulesEnabled
        ? NetY + ServiceOffset * randomRange(0.48, 0.86)
        : randomRange(NetY + 120.0, CourtBottom - 20.0)};
    target.y += randomRange(-fatigueError * 0.65, fatigueError);
    impactPosition = match.ball.position;
    impactHeight = match.ball.height;
    impactHitter = 1;
    impactEffectTime = 0.12;
    opponentAction = 5;
    opponentActionTime = 0.50;
    const double fatiguePowerScale = 0.72 +
        0.28 * staminaTuning.movementScaleAt(staminaRatio);
    const double serveSpeed = randomRange(shotSpeedTuning.cpuServeMin,
                                          shotSpeedTuning.cpuServeMax) * fatiguePowerScale *
                              difficulty.ballSpeedScale;
    const double verticalSpin = randomRange(-250.0, 1700.0);
    const double sideSpin = randomRange(-550.0, 550.0);
    launchBall(target, serveSpeed, verticalSpin, sideSpin, randomRange(0.03, 0.08));
    audio.playRacketHit(std::clamp((serveSpeed - 32.0) / 22.0, 0.0, 1.0),
                        std::clamp(std::abs(verticalSpin) / 3600.0 * 0.68 +
                                   std::abs(sideSpin) / 2300.0 * 0.48,
                                   0.0, 1.0),
                        0.88 * staminaQuality, true);
    const double power = std::clamp((serveSpeed - 30.0) / 22.0, 0.0, 1.0);
    const double complexity = std::clamp(std::abs(verticalSpin) / 3600.0 * 0.55 +
                                         std::abs(sideSpin) / 2300.0 * 0.45, 0.0, 1.0);
    const double hitCost = (staminaTuning.baseHitCost + 7.0 * std::pow(power, 1.35) +
                            7.5 * std::pow(0.72, 1.55) + complexity * 3.0) /
                           std::max(0.20, opponentProfile.attributes.endurance);
    match.opponentStamina = std::max(0.0, match.opponentStamina - hitCost);
    statusMessage = match.serveFaultCount == 0 ? "COMPUTER SERVE" : "COMPUTER SECOND SERVE";
    statusTime = 0.8;
}

Vec2 Game::predictFirstBounce() const {
    return ballPhysics.predictFirstBounce(match.ball, PixelsPerMeter);
}

Game::OpponentContactPrediction Game::predictOpponentContact(
        double movementSpeed, double contactReach) const {
    OpponentContactPrediction emergency;
    if (!match.ball.inPlay || match.ball.lastHitter != 0) return emergency;

    tennis::BallState preview = match.ball;
    constexpr double Step = 1.0 / 120.0;
    constexpr int MaxFrames = 360;
    // A baseline player normally lets the ball bounce. A player already inside
    // the court may take a legal volley, while a serve return must always bounce.
    const bool alreadyAtNet = match.opponentPosition.y > CourtTop + 105.0;
    const bool requireBounce = preview.serveReturnPending ||
                               (preview.bounceCount == 0 && !alreadyAtNet);

    for (int frame = 1; frame <= MaxFrames; ++frame) {
        const Vec2 previousPosition = preview.position;
        const double previousHeight = preview.height;
        ballPhysics.integrateAirborne(preview, Step, PixelsPerMeter);
        ballPhysics.resolveNetCollision(preview, previousPosition, previousHeight,
                                        NetY, NetHeightM, CourtLeft - 8.0,
                                        CourtRight + 8.0, PixelsPerMeter);
        if (preview.height <= BallRadiusM && preview.verticalVelocity < 0.0) {
            ballPhysics.applyCourtBounce(preview, PixelsPerMeter);
            if (preview.bounceCount >= 2 && !match.rules.multiBounceAllowed) break;
        }

        const double time = frame * Step;
        const bool legalContact = preview.position.y < NetY - 2.0 &&
            (!requireBounce || preview.bounceCount > match.ball.bounceCount);
        if (!legalContact) continue;
        if (preview.position.y < -35.0 ||
            preview.position.x < CourtLeft - 105.0 ||
            preview.position.x > CourtRight + 105.0) break;

        // Include current momentum, a short reaction delay, and the racket's
        // reach. This is intentionally conservative so prediction does not
        // become teleportation at higher difficulty levels.
        const double reactionDelay = 0.055;
        const double travelTime = std::max(0.0, time - reactionDelay);
        const Vec2 momentumAssist = opponentVelocity * std::min(time, 0.20) * 0.35;
        const Vec2 estimatedOrigin = match.opponentPosition + momentumAssist;
        const double reachableDistance = contactReach + movementSpeed * travelTime * 0.88;
        if (estimatedOrigin.distanceTo(preview.position) > reachableDistance) continue;

        OpponentContactPrediction candidate;
        candidate.position = preview.position;
        candidate.height = preview.height;
        candidate.time = time;
        candidate.bounceCount = preview.bounceCount;
        candidate.valid = true;

        // Shoulder-to-knee contact is preferred. Keep the first reachable
        // emergency contact in reserve for skidding, very short balls.
        if (preview.height >= 0.55 && preview.height <= 1.62) return candidate;
        if (!emergency.valid && preview.height >= 0.28 && preview.height <= 2.30) {
            emergency = candidate;
        }
    }
    return emergency;
}

bool Game::aiShotIsSafe(const tennis::BallState &shot) const {
    tennis::BallState preview = shot;
    constexpr double Step = 1.0 / 180.0;
    bool crossedNet = false;
    for (int frame = 0; frame < 720; ++frame) {
        const Vec2 previousPosition = preview.position;
        const double previousHeight = preview.height;
        ballPhysics.integrateAirborne(preview, Step, PixelsPerMeter);

        if (previousPosition.y < NetY && preview.position.y >= NetY) {
            const double span = preview.position.y - previousPosition.y;
            const double fraction = span > 1e-9 ? (NetY - previousPosition.y) / span : 0.0;
            const double crossingX = previousPosition.x +
                (preview.position.x - previousPosition.x) * fraction;
            const double crossingHeight = previousHeight +
                (preview.height - previousHeight) * fraction;
            const bool crossesNetBody = crossingX >= CourtLeft - 8.0 &&
                                        crossingX <= CourtRight + 8.0;
            // Reserve a small safety margin above the tape; an intentional net
            // cord remains possible for the player, but not as an AI aim error.
            if (crossesNetBody && crossingHeight - BallRadiusM <= NetHeightM + 0.045)
                return false;
            crossedNet = true;
        }

        if (preview.height <= BallRadiusM && preview.verticalVelocity < 0.0) {
            if (!crossedNet) return false;
            if (match.rules.boundaryMode == tennis::BoundaryMode::None)
                return preview.position.y > NetY + 5.0;
            const double left = match.rules.boundaryMode == tennis::BoundaryMode::Singles
                ? SinglesLeft : CourtLeft;
            const double right = match.rules.boundaryMode == tennis::BoundaryMode::Singles
                ? SinglesRight : CourtRight;
            const double margin = 4.0;
            return preview.position.x >= left + margin &&
                   preview.position.x <= right - margin &&
                   preview.position.y >= NetY + 8.0 &&
                   preview.position.y <= CourtBottom - margin;
        }
    }
    return false;
}

bool Game::opponentShouldLeaveBall() const {
    if (match.ball.serviceShot && match.ball.netHit) return true;
    if (!match.ball.inPlay || match.ball.lastHitter != 0 || match.ball.bounceCount != 0) {
        return false;
    }
    const Vec2 landing = predictFirstBounce();
    const double obviousMargin = 0.30 * PixelsPerMeter;
    if (match.ball.serviceShot && match.rules.serveRulesEnabled) {
        const bool xClearlyOut = match.ball.serviceTargetLeft
            ? landing.x < SinglesLeft - obviousMargin || landing.x > 640.0 + obviousMargin
            : landing.x < 640.0 - obviousMargin || landing.x > SinglesRight + obviousMargin;
        const bool yClearlyOut = landing.y < NetY - ServiceOffset - obviousMargin ||
                                 landing.y >= NetY;
        return xClearlyOut || yClearlyOut;
    }
    if (match.rules.boundaryMode == tennis::BoundaryMode::None) return false;
    const double left = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? SinglesLeft : CourtLeft;
    const double right = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? SinglesRight : CourtRight;
    return landing.x < left - obviousMargin || landing.x > right + obviousMargin ||
           landing.y < CourtTop - obviousMargin || landing.y >= NetY;
}

void Game::launchBall(const Vec2 &target, double shotSpeedMps, double spinRpm,
                      double sideSpinRpm, double loft) {
    ballPhysics.launch(match.ball, target, shotSpeedMps, spinRpm, sideSpinRpm, loft,
                       PixelsPerMeter);
}

void Game::strikeBall(const Vec2 &target, double racketShotSpeedMps, double spinRpm,
                      double sideSpinRpm, double loft,
                      const tennis::RacketImpactConfig &impact) {
    ballPhysics.strike(match.ball, target, racketShotSpeedMps, spinRpm,
                       sideSpinRpm, loft, PixelsPerMeter, impact);
}

void Game::updateBall(double delta) {
    const Vec2 previous = match.ball.position;
    const double previousHeight = match.ball.height;
    ballPhysics.integrateAirborne(match.ball, delta, PixelsPerMeter);

    const bool crossedNet = (previous.y - NetY) * (match.ball.position.y - NetY) <= 0.0;
    const double preNetHorizontalMps = match.ball.velocity.length() / PixelsPerMeter;
    const double preNetSpeedMps = std::sqrt(preNetHorizontalMps * preNetHorizontalMps +
                                            match.ball.verticalVelocity * match.ball.verticalVelocity);
    const tennis::NetCollisionOutcome netOutcome = ballPhysics.resolveNetCollision(
        match.ball, previous, previousHeight, NetY, NetHeightM,
        CourtLeft - 8.0, CourtRight + 8.0, PixelsPerMeter);
    if (netOutcome != tennis::NetCollisionOutcome::None) {
        audio.playNetHit(netOutcome, preNetSpeedMps);
        impactPosition = {match.ball.position.x, NetY};
        impactHeight = std::min(match.ball.height, NetHeightM);
        impactHitter = -1;
        impactEffectTime = 0.08;
        if (match.ball.serviceShot) {
            statusMessage = "SERVE TOUCHED NET";
        } else if (netOutcome == tennis::NetCollisionOutcome::CordForward) {
            statusMessage = "NET CORD | PLAY ON";
        } else {
            statusMessage = "NET | BALL DROPPING";
        }
        statusTime = 1.0;
    }
    if (crossedNet && netOutcome == tennis::NetCollisionOutcome::None &&
        !match.ball.netHit && match.ball.serviceShot && !match.rules.serveRulesEnabled) {
        match.ball.serviceShot = false;
        match.serveFaultCount = 0;
    }

    if (match.ball.height <= BallRadiusM && match.ball.verticalVelocity < 0.0) {
        match.ball.height = BallRadiusM;
        const bool touchedNet = match.ball.netHit;
        const double bounceVerticalImpactMps = -match.ball.verticalVelocity;
        const double bounceHorizontalSpeedMps = match.ball.velocity.length() / PixelsPerMeter;
        spawnSurfaceBallImpact(match.ball.position, match.ball.velocity,
                               -match.ball.verticalVelocity);
        audio.playCourtBounce(activeCourtPreset, bounceVerticalImpactMps,
                              bounceHorizontalSpeedMps);
        ballPhysics.applyCourtBounce(match.ball, PixelsPerMeter);
        if (match.ball.bounceCount == 1) match.ball.serveReturnPending = false;
        if (match.ball.bounceCount == 1 && match.ball.serviceShot) {
            if (!match.rules.serveRulesEnabled) {
                const bool landedAcrossNet = match.ball.serviceFromBottom
                    ? match.ball.position.y < NetY : match.ball.position.y > NetY;
                statusMessage = touchedNet && landedAcrossNet
                    ? "LET | SERVE AGAIN" : "SERVE DID NOT CROSS | TRY AGAIN";
                statusTime = 1.2;
                resetRally(false);
                return;
            }
            if (!ballInServiceBox()) {
                handleServeFault(touchedNet ? "NET FAULT" : "SERVE FAULT");
                return;
            }
            if (touchedNet) {
                statusMessage = "LET | SERVE AGAIN";
                statusTime = 1.2;
                resetRally(false);
                return;
            }
            match.ball.serviceShot = false;
            match.serveFaultCount = 0;
        } else if (match.ball.bounceCount == 1) {
            const bool landedOnExpectedHalf = match.ball.lastHitter == 0
                ? match.ball.position.y < NetY : match.ball.position.y > NetY;
            if (touchedNet && !landedOnExpectedHalf) {
                awardPoint(match.ball.lastHitter == 1, "NET");
                return;
            }
            if (!ballInCourtHalf(match.ball.lastHitter == 0)) {
                awardPoint(match.ball.lastHitter == 1, "OUT");
                return;
            }
        }
        if (match.ball.bounceCount >= 2 && !match.rules.multiBounceAllowed) {
            awardPoint(match.ball.lastHitter == 0, "SECOND BOUNCE");
            return;
        }
        if (match.ball.bounceCount >= 2 && match.rules.multiBounceAllowed) {
            const double peakHeight = ballPhysics.predictedPeakHeight(match.ball);
            if (peakHeight < 0.16 || match.ball.velocity.length() / PixelsPerMeter < 0.18) {
                awardPoint(match.ball.lastHitter == 0, "BALL DEAD");
                return;
            }
        }
    }
}

bool Game::ballInCourtHalf(bool topHalf) const {
    if (match.rules.boundaryMode == tennis::BoundaryMode::None) return true;
    const double margin = (BallRadiusM + LineHalfWidthM) * PixelsPerMeter;
    const double left = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? SinglesLeft : CourtLeft;
    const double right = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? SinglesRight : CourtRight;
    const bool xIn = match.ball.position.x >= left - margin && match.ball.position.x <= right + margin;
    const bool yIn = topHalf ? match.ball.position.y >= CourtTop - margin && match.ball.position.y < NetY
                             : match.ball.position.y > NetY && match.ball.position.y <= CourtBottom + margin;
    return xIn && yIn;
}

bool Game::ballInServiceBox() const {
    const double margin = (BallRadiusM + LineHalfWidthM) * PixelsPerMeter;
    const bool xIn = match.ball.serviceTargetLeft
        ? match.ball.position.x >= SinglesLeft - margin && match.ball.position.x <= 640.0 + margin
        : match.ball.position.x >= 640.0 - margin && match.ball.position.x <= SinglesRight + margin;
    const bool yIn = match.ball.serviceFromBottom
        ? match.ball.position.y >= NetY - ServiceOffset - margin && match.ball.position.y < NetY
        : match.ball.position.y > NetY && match.ball.position.y <= NetY + ServiceOffset + margin;
    return xIn && yIn;
}

void Game::handleServeFault(const std::string &reason) {
    ++match.serveFaultCount;
    if (match.serveFaultCount < 2) {
        statusMessage = reason + " | SECOND SERVE";
        statusTime = 1.6;
        resetRally(false);
        return;
    }
    const bool playerWon = !match.ball.serviceFromBottom;
    awardPoint(playerWon, "DOUBLE FAULT");
}

void Game::awardPoint(bool playerWon, const std::string &reason) {
    const tennis::ScoreResult result = tennis::ScoringSystem::recordPoint(match.score, playerWon);
    ++match.completedRallies;
    if (match.rules.matchPresentationEnabled && match.score.mode == tennis::ScoreMode::Sets &&
        (result.milestone == tennis::ScoreMilestone::Game ||
         result.milestone == tennis::ScoreMilestone::Set)) {
        int gamesInSet = match.score.playerGames + match.score.opponentGames;
        if (result.milestone == tennis::ScoreMilestone::Set && match.score.completedSetCount > 0) {
            const int setIndex = match.score.completedSetCount - 1;
            gamesInSet = match.score.playerSetGames[setIndex] + match.score.opponentSetGames[setIndex];
        }
        if (gamesInSet % 2 == 1) {
            match.endsSwapped = !match.endsSwapped;
        }
    }
    switch (result.milestone) {
        case tennis::ScoreMilestone::Game:
            ++match.completedGames;
            statusMessage = result.playerWon ? "GAME | PLAYER" : "GAME | COMPUTER";
            break;
        case tennis::ScoreMilestone::Set:
            ++match.completedGames;
            statusMessage = result.playerWon ? "SET | PLAYER" : "SET | COMPUTER";
            break;
        case tennis::ScoreMilestone::Match:
            ++match.completedGames;
            match.completedGames = 0;
            match.endsSwapped = false;
            statusMessage = result.playerWon ? "MATCH WON | PLAYER" : "MATCH WON | COMPUTER";
            break;
        case tennis::ScoreMilestone::Race:
            statusMessage = result.playerWon ? "RACE WON | PLAYER" : "RACE WON | COMPUTER";
            break;
        default:
            statusMessage = reason + (playerWon ? " | PLAYER POINT" : " | COMPUTER POINT");
            break;
    }
    statusTime = 2.0;
    // A completed point starts the next rally with both players fully rested.
    // Lets and first-serve faults call resetRally(false) directly, so they do
    // not pass through this refill.
    match.playerStamina = playerProfile.attributes.staminaCapacity;
    match.opponentStamina = opponentProfile.attributes.staminaCapacity;
    resetRally();
}

void Game::applyServePositionRule() {
    if (!match.rules.serveRulesEnabled || !match.currentServerPlayer || match.ball.inPlay || serveTossing) return;
    const bool deuceSide = ((match.score.playerPoints + match.score.opponentPoints) % 2 == 0);
    if (deuceSide) {
        match.playerPosition.x = std::clamp(match.playerPosition.x, 650.0, SinglesRight - 6.0);
    } else {
        match.playerPosition.x = std::clamp(match.playerPosition.x, SinglesLeft + 6.0, 630.0);
    }
    match.playerPosition.y = std::clamp(match.playerPosition.y, CourtBottom + 8.0, CourtBottom + 132.0);
}

void Game::applyMatchPreset() {
    match.rules.serveRulesEnabled = true;
    match.rules.boundaryMode = tennis::BoundaryMode::Singles;
    match.rules.multiBounceAllowed = false;
    match.rules.serveMode = tennis::ServeMode::Alternate;
    match.rules.customRules = false;
    match.rules.matchPresentationEnabled = true;
    match.endsSwapped = false;
    if (!match.ball.inPlay) resetRally(true);
}

double Game::currentHeightQuality() const {
    auto zoneQuality = [](double height, double greenMin, double greenMax,
                          double redMin, double redMax) {
        if (height >= greenMin && height <= greenMax) return 1.0;
        if (height > redMin && height < greenMin) return (height - redMin) / (greenMin - redMin);
        if (height > greenMax && height < redMax) return (redMax - height) / (redMax - greenMax);
        return 0.0;
    };
    const tennis::ContactZoneConfig &zone = playerProfile.contactZone;
    const double normalQuality = zoneQuality(match.ball.height, zone.normalGreenMin, zone.normalGreenMax,
                                             zone.normalReachMin, zone.normalReachMax);
    const double smashQuality = zoneQuality(match.ball.height, zone.smashGreenMin, zone.smashGreenMax,
                                            zone.smashReachMin, zone.smashReachMax);
    double quality = std::max(normalQuality, smashQuality);
    if (match.ball.height > zone.normalGreenMax && match.ball.height < zone.smashGreenMin)
        quality = std::max(quality, zone.difficultBandQuality);
    return std::pow(quality, 1.0 / std::max(0.25, playerProfile.attributes.contactTolerance));
}

double Game::currentPositionQuality() const {
    if (!match.ball.inPlay || match.ball.lastHitter != 1 || match.ball.position.y <= NetY) return 0.0;
    const tennis::ContactZoneConfig &zone = playerProfile.contactZone;
    const Vec2 relative = match.ball.position - match.playerPosition;
    const double lateralError = (std::abs(relative.x) - zone.idealLateralOffset) / zone.lateralGreenRadius;
    const double forwardError = (relative.y - zone.idealForwardOffset) / zone.forwardGreenRadius;
    const double ellipseError = std::sqrt(lateralError * lateralError + forwardError * forwardError);
    if (ellipseError <= 1.0) return 1.0;
    const double unreachable = std::max(1.01, zone.unreachableEllipse *
        std::max(0.25, playerProfile.attributes.contactTolerance));
    if (ellipseError >= unreachable) return 0.0;
    return 1.0 - (ellipseError - 1.0) / (unreachable - 1.0);
}

void Game::addShotRecord(const std::string &shot, double power, double verticalSpin,
                         double sideSpin, double loft, double direction, double flatDrive,
                         double accuracy, double staminaAtHit, int mappingProfile) {
    ShotRecord record;
    record.shot = shot;
    record.power = power;
    record.verticalSpinRpm = verticalSpin;
    record.sideSpinRpm = sideSpin;
    record.loft = loft;
    record.direction = direction;
    record.flatDrive = flatDrive;
    record.accuracy = accuracy;
    record.stamina = staminaAtHit;
    record.mappingProfile = std::clamp(mappingProfile, 0, 4);

    if (gesturePointCount > 0) {
        double minX = gesturePoints[0].x;
        double maxX = minX;
        double minY = gesturePoints[0].y;
        double maxY = minY;
        for (int i = 1; i < gesturePointCount; ++i) {
            minX = std::min(minX, gesturePoints[i].x);
            maxX = std::max(maxX, gesturePoints[i].x);
            minY = std::min(minY, gesturePoints[i].y);
            maxY = std::max(maxY, gesturePoints[i].y);
        }
        const double scale = std::max({maxX - minX, maxY - minY, 30.0});
        const Vec2 center{(minX + maxX) * 0.5, (minY + maxY) * 0.5};
        for (int i = 0; i < gesturePointCount; i += 2) {
            record.pattern.push_back((gesturePoints[i] - center) / scale);
        }
        if ((gesturePointCount - 1) % 2 != 0) {
            record.pattern.push_back((gesturePoints[gesturePointCount - 1] - center) / scale);
        }
    }

    shotHistory.push_front(std::move(record));
    while (shotHistory.size() > 1) shotHistory.pop_back();
}
