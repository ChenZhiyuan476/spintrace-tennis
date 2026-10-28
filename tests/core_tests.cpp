#include "ball_physics.hpp"
#include "progression_system.hpp"
#include "scoring_system.hpp"

#include <cassert>
#include <cmath>

int main() {
    const tennis::StaminaTuning stamina;
    assert(stamina.runningCostPerSecond > 7.0);
    assert(stamina.sprintCostPerSecond > stamina.runningCostPerSecond * 2.0);
    assert(stamina.idleRecoveryPerSecond < stamina.runningCostPerSecond);
    assert(stamina.baseHitCost > 0.0);
    assert(stamina.splitStepSpeedMultiplier > 1.0);
    assert(std::abs(stamina.lowStaminaThreshold - 0.20) < 1e-12);
    assert(stamina.exhaustedSpeedScale < 0.70);
    assert(std::abs(stamina.movementScaleAt(0.20) - 1.0) < 1e-12);
    assert(std::abs(stamina.movementScaleAt(0.0) - 0.62) < 1e-12);
    assert(std::abs(stamina.accuracyQualityAt(0.0) - 0.15) < 1e-12);

    const tennis::ShotSpeedTuning speeds;
    assert(speeds.playerGroundBase + speeds.playerGroundPower >= 35.0);
    assert(speeds.playerPowerExponent > 1.0);
    assert(speeds.cpuGroundMin >= 22.0);
    assert(speeds.cpuServeMin >= 40.0);
    assert(speeds.cpuServeMax > speeds.cpuServeMin);
    const tennis::AiDifficultyProfile easy =
        tennis::aiDifficultyProfile(tennis::AiDifficulty::Easy);
    const tennis::AiDifficultyProfile expert =
        tennis::aiDifficultyProfile(tennis::AiDifficulty::Expert);
    assert(expert.ballSpeedScale > easy.ballSpeedScale);
    assert(expert.movementSpeedScale > easy.movementSpeedScale);
    assert(expert.targetErrorPixels < easy.targetErrorPixels);

    const tennis::ControlSettings controls;
    assert(controls.autoHit);
    assert(std::abs(controls.rallyFullChargeSeconds - 0.85) < 1e-9);
    assert(std::abs(controls.serveFullChargeSeconds - 0.48) < 1e-9);
    assert(std::abs(controls.movingChargeRate - 0.45) < 1e-9);
    assert(controls.serveFullChargeSeconds < controls.rallyFullChargeSeconds);
    assert(controls.movingChargeRate > 0.0 && controls.movingChargeRate < 0.5);
    assert(controls.sprintChargeRate == 0.0);

    const tennis::CourtSurfaceProfile &bouncyHard =
        tennis::courtSurfaceProfile(tennis::CourtPreset::BouncyHard);
    const tennis::CourtSurfaceProfile &clay =
        tennis::courtSurfaceProfile(tennis::CourtPreset::Clay);
    const tennis::CourtSurfaceProfile &grass =
        tennis::courtSurfaceProfile(tennis::CourtPreset::Grass);
    const tennis::CourtSurfaceProfile &fastHard =
        tennis::courtSurfaceProfile(tennis::CourtPreset::FastHard);
    assert(bouncyHard.material == tennis::CourtMaterial::Hard);
    assert(fastHard.material == tennis::CourtMaterial::Hard);
    assert(bouncyHard.bounceRestitutionBase > fastHard.bounceRestitutionBase);
    assert(fastHard.bounceForwardBase > bouncyHard.bounceForwardBase);
    assert(clay.bounceRestitutionBase > bouncyHard.bounceRestitutionBase);
    assert(grass.bounceRestitutionBase < bouncyHard.bounceRestitutionBase);
    assert(clay.bounceForwardBase < bouncyHard.bounceForwardBase);
    assert(grass.bounceForwardBase > fastHard.bounceForwardBase);
    assert(clay.decelerationMps2 < bouncyHard.decelerationMps2);
    assert(clay.courtColor.r == clay.surroundColor.r);
    assert(grass.courtColor.g == grass.surroundColor.g);
    assert(grass.courtColor.g > 0.45f);
    assert(grass.courtColor.g - grass.courtColor.r < 0.15f);
    assert(grass.drawMowingStripes);
    assert(bouncyHard.name == "BOUNCY HARD");
    assert(clay.name == "CLAY");
    assert(grass.name == "GRASS");
    assert(fastHard.name == "FAST HARD");
    assert(grass.scoreboardBase.b > grass.scoreboardBase.r);
    assert(grass.scoreboardAccent.b > grass.scoreboardAccent.r);
    assert(grass.scoreboardAccent.r > grass.scoreboardBase.r);
    assert(grass.scoreboardPoint.r > 0.95f && grass.scoreboardPoint.b > 0.95f);
    assert(clay.effects.particleKind == tennis::SurfaceParticleKind::Dust);
    assert(clay.effects.ballImpactAmount > 0.0);
    assert(clay.effects.movementAmount > 0.0);
    assert(bouncyHard.effects.particleKind == tennis::SurfaceParticleKind::None);
    assert(grass.effects.particleKind == tennis::SurfaceParticleKind::GrassClipping);
    assert(grass.effects.ballImpactAmount < clay.effects.ballImpactAmount);
    assert(grass.effects.movementAmount < clay.effects.movementAmount);
    assert(grass.effects.wearRate > 0.0);
    assert(grass.effects.wearColor.a > 0.0f);

    auto surfacePhysics = [](const tennis::CourtSurfaceProfile &surface) {
        tennis::BallPhysicsConfig config;
        config.bounceRestitutionBase = surface.bounceRestitutionBase;
        config.bounceRestitutionSpin = surface.bounceRestitutionSpin;
        config.bounceForwardBase = surface.bounceForwardBase;
        config.bounceForwardSpin = surface.bounceForwardSpin;
        return tennis::BallPhysicsSystem(config);
    };
    tennis::BallState impactBall;
    impactBall.height = 0.0335;
    impactBall.velocity = {0.0, 18.0 * 23.1384};
    impactBall.verticalVelocity = -8.0;
    tennis::BallState hardBounce = impactBall;
    tennis::BallState clayBounce = impactBall;
    tennis::BallState grassBounce = impactBall;
    surfacePhysics(bouncyHard).applyCourtBounce(hardBounce, 23.1384);
    surfacePhysics(clay).applyCourtBounce(clayBounce, 23.1384);
    surfacePhysics(grass).applyCourtBounce(grassBounce, 23.1384);
    assert(clayBounce.verticalVelocity > hardBounce.verticalVelocity);
    assert(grassBounce.verticalVelocity < hardBounce.verticalVelocity);
    assert(clayBounce.velocity.length() < hardBounce.velocity.length());
    assert(grassBounce.velocity.length() > hardBounce.velocity.length());

    tennis::BallPhysicsSystem physics;
    constexpr double pixelsPerMeter = 23.1384;
    constexpr double netY = 350.0;
    constexpr double netHeightM = 0.914;
    constexpr double netLeft = 510.0;
    constexpr double netRight = 770.0;
    const Vec2 beforeNet{640.0, 360.0};

    tennis::BallState cleanNetClearance;
    cleanNetClearance.position = {640.0, 340.0};
    cleanNetClearance.height = 1.05;
    cleanNetClearance.velocity = {0.0, -20.0 * pixelsPerMeter};
    assert(physics.resolveNetCollision(cleanNetClearance, beforeNet, 1.05,
        netY, netHeightM, netLeft, netRight, pixelsPerMeter) ==
        tennis::NetCollisionOutcome::None);
    assert(!cleanNetClearance.netHit);

    tennis::BallState forwardCord;
    forwardCord.position = {640.0, 340.0};
    forwardCord.height = 0.94;
    forwardCord.verticalVelocity = -2.0;
    forwardCord.velocity = {1.5 * pixelsPerMeter, -20.0 * pixelsPerMeter};
    assert(physics.resolveNetCollision(forwardCord, beforeNet, 0.94,
        netY, netHeightM, netLeft, netRight, pixelsPerMeter) ==
        tennis::NetCollisionOutcome::CordForward);
    assert(forwardCord.netHit && forwardCord.position.y < netY);
    assert(forwardCord.velocity.y < 0.0);

    tennis::BallState backwardCord;
    backwardCord.position = {640.0, 340.0};
    backwardCord.height = 0.895;
    backwardCord.verticalVelocity = -1.0;
    backwardCord.velocity = {0.0, -18.0 * pixelsPerMeter};
    assert(physics.resolveNetCollision(backwardCord, beforeNet, 0.895,
        netY, netHeightM, netLeft, netRight, pixelsPerMeter) ==
        tennis::NetCollisionOutcome::CordBack);
    assert(backwardCord.netHit && backwardCord.position.y > netY);
    assert(backwardCord.velocity.y > 0.0 && backwardCord.verticalVelocity < 0.0);

    tennis::BallState meshHit;
    meshHit.position = {640.0, 340.0};
    meshHit.height = 0.70;
    meshHit.velocity = {2.0 * pixelsPerMeter, -22.0 * pixelsPerMeter};
    assert(physics.resolveNetCollision(meshHit, beforeNet, 0.70,
        netY, netHeightM, netLeft, netRight, pixelsPerMeter) ==
        tennis::NetCollisionOutcome::MeshStopped);
    assert(meshHit.netHit && meshHit.position.y > netY);
    assert(meshHit.velocity.y > 0.0 && meshHit.verticalVelocity < 0.0);

    tennis::BallState first;
    first.position = {640.0, 600.0};
    first.height = 1.0;
    first.lastHitter = 0;
    physics.launch(first, {620.0, 120.0}, 24.0, 1800.0, -450.0, 0.2, 23.1384);
    tennis::BallState second = first;
    for (int i = 0; i < 120; ++i) {
        physics.integrateAirborne(first, 1.0 / 120.0, 23.1384);
        physics.integrateAirborne(second, 1.0 / 120.0, 23.1384);
    }
    assert(std::abs(first.position.x - second.position.x) < 1e-12);
    assert(std::abs(first.position.y - second.position.y) < 1e-12);
    assert(std::abs(first.height - second.height) < 1e-12);

    tennis::BallState downwardSmash;
    downwardSmash.position = {640.0, 560.0};
    downwardSmash.height = 2.0;
    downwardSmash.lastHitter = 0;
    physics.launch(downwardSmash, {640.0, 120.0}, 25.0, 0.0, 0.0, -0.85, 23.1384);
    assert(downwardSmash.verticalVelocity < 0.0);

    tennis::BallState downwardVolley;
    downwardVolley.position = {640.0, 390.0};
    downwardVolley.height = 1.1;
    downwardVolley.lastHitter = 0;
    physics.launch(downwardVolley, {640.0, 120.0}, 18.0, 0.0, 0.0, -0.20, 23.1384);
    assert(downwardVolley.verticalVelocity < 0.0);

    auto verifyOverhandServe = [&](double speedMps, double spinRpm, double loft) {
        tennis::BallState serve;
        serve.position = {640.0, 717.0};
        serve.height = speeds.serveContactHeight;
        serve.serviceShot = true;
        serve.serviceFromBottom = true;
        physics.launch(serve, {600.0, 255.0}, speedMps, spinRpm, 0.0,
                       loft, pixelsPerMeter);
        double heightAtNet = -1.0;
        double firstBounceY = serve.position.y;
        for (int frame = 0; frame < 600; ++frame) {
            const Vec2 previousPosition = serve.position;
            const double previousHeight = serve.height;
            physics.integrateAirborne(serve, 1.0 / 120.0, pixelsPerMeter);
            if (previousPosition.y > netY && serve.position.y <= netY) {
                const double t = (previousPosition.y - netY) /
                    (previousPosition.y - serve.position.y);
                heightAtNet = previousHeight + (serve.height - previousHeight) * t;
            }
            if (serve.height <= 0.0335 && serve.verticalVelocity < 0.0) {
                firstBounceY = serve.position.y;
                break;
            }
        }
        assert(heightAtNet > netHeightM);
        assert(firstBounceY > netY - 550.0 * 6.40 / 23.77);
        assert(firstBounceY < netY);
    };
    verifyOverhandServe(25.0, 0.0, 0.05);
    verifyOverhandServe(30.0, 900.0, 0.05);
    verifyOverhandServe(34.0, 1800.0, 0.05);
    verifyOverhandServe(42.0, 500.0, 0.05);
    verifyOverhandServe(51.0, 900.0, 0.05);

    auto verifyCpuServe = [&](double speedMps, double spinRpm,
                              double loft, double targetY) {
        tennis::BallState serve;
        serve.position = {640.0, 35.0};
        serve.height = speeds.cpuServeContactHeight;
        serve.serviceShot = true;
        serve.serviceFromBottom = false;
        physics.launch(serve, {680.0, targetY}, speedMps, spinRpm, 0.0,
                       loft, pixelsPerMeter);
        double heightAtNet = -1.0;
        double firstBounceY = serve.position.y;
        for (int frame = 0; frame < 600; ++frame) {
            const Vec2 previousPosition = serve.position;
            const double previousHeight = serve.height;
            physics.integrateAirborne(serve, 1.0 / 120.0, pixelsPerMeter);
            if (previousPosition.y < netY && serve.position.y >= netY) {
                const double t = (netY - previousPosition.y) /
                    (serve.position.y - previousPosition.y);
                heightAtNet = previousHeight + (serve.height - previousHeight) * t;
            }
            if (serve.height <= 0.0335 && serve.verticalVelocity < 0.0) {
                firstBounceY = serve.position.y;
                break;
            }
        }
        assert(heightAtNet > netHeightM);
        assert(firstBounceY > netY);
        assert(firstBounceY < netY + 550.0 * 6.40 / 23.77);
    };
    verifyCpuServe(42.0 * 0.76, -250.0, 0.03, 421.0);
    verifyCpuServe(51.0 * 0.88, 1700.0, 0.08, 477.0);
    verifyCpuServe(42.0, 0.0, 0.03, 421.0);
    verifyCpuServe(51.0 * 1.08, 1700.0, 0.08, 477.0);

    auto verifyCpuRallyBall = [&](double racketSpeed, double loft) {
        tennis::BallState rally;
        rally.position = {640.0, 220.0};
        rally.height = 1.05;
        rally.velocity = {0.0, -24.0 * pixelsPerMeter};
        rally.verticalVelocity = -2.0;
        rally.lastHitter = 0;
        tennis::RacketImpactConfig cpuImpact;
        cpuImpact.maxOutgoingSpeedMps = 38.0;
        physics.strike(rally, {640.0, 545.0}, racketSpeed, 1400.0, 0.0,
                       loft, pixelsPerMeter, cpuImpact);
        const Vec2 bounce = physics.predictFirstBounce(rally, pixelsPerMeter);
        assert(bounce.y > netY);
        assert(bounce.y < 625.0);
    };
    verifyCpuRallyBall(29.0 * 0.88, 0.04);
    verifyCpuRallyBall(29.0 * 1.08, 0.00);

    tennis::BallState fallingSmash;
    fallingSmash.position = {640.0, 410.0};
    fallingSmash.height = 2.05;
    fallingSmash.velocity = {0.0, 15.0 * 23.1384};
    fallingSmash.verticalVelocity = -11.0;
    fallingSmash.lastHitter = 0;
    tennis::RacketImpactConfig smashImpact;
    smashImpact.verticalVelocityRetention = 0.30;
    physics.strike(fallingSmash, {640.0, 120.0}, 21.0, 0.0, 0.0,
                   -0.18, 23.1384, smashImpact);
    assert(fallingSmash.verticalVelocity < 0.0);

    tennis::BallState incomingReturn;
    incomingReturn.position = {640.0, 580.0};
    incomingReturn.height = 1.0;
    incomingReturn.velocity = {3.0 * 23.1384, 18.0 * 23.1384};
    incomingReturn.verticalVelocity = -4.0;
    incomingReturn.spinRpm = 2000.0;
    incomingReturn.sideSpinRpm = -600.0;
    incomingReturn.lastHitter = 0;
    tennis::RacketImpactConfig returnImpact;
    returnImpact.horizontalVelocityRetention = 0.25;
    returnImpact.verticalVelocityRetention = 0.20;
    returnImpact.verticalSpinRetention = 0.20;
    returnImpact.sideSpinRetention = 0.25;
    returnImpact.maxOutgoingSpeedMps = 40.0;
    tennis::BallState activeReturn = incomingReturn;
    physics.strike(activeReturn, {640.0, 120.0}, 20.0, 1000.0, 300.0,
                   0.10, 23.1384, returnImpact);
    tennis::BallState noIncomingReturn = incomingReturn;
    noIncomingReturn.velocity = {};
    noIncomingReturn.verticalVelocity = 0.0;
    noIncomingReturn.spinRpm = 0.0;
    noIncomingReturn.sideSpinRpm = 0.0;
    physics.launch(noIncomingReturn, {640.0, 120.0}, 20.0, 1000.0, 300.0,
                   0.10, 23.1384);
    assert(activeReturn.velocity.y < noIncomingReturn.velocity.y);
    assert(activeReturn.velocity.length() > noIncomingReturn.velocity.length());
    assert(std::abs(activeReturn.verticalVelocity - noIncomingReturn.verticalVelocity) > 0.1);
    assert(std::abs(activeReturn.spinRpm - 600.0) < 1e-9);
    assert(std::abs(activeReturn.sideSpinRpm - 450.0) < 1e-9);

    // A level compact volley uses a more closed neutral racket face than a
    // level groundstroke. Falling pace is absorbed vertically rather than
    // being reflected into an unintended lob.
    tennis::BallState neutralGroundstroke = incomingReturn;
    tennis::RacketImpactConfig groundImpact;
    physics.strike(neutralGroundstroke, {640.0, 180.0}, 20.0, 0.0, 0.0,
                   0.0, 23.1384, groundImpact);
    tennis::BallState neutralVolley = incomingReturn;
    tennis::RacketImpactConfig volleyImpact;
    volleyImpact.verticalVelocityRetention = 0.08;
    volleyImpact.neutralLaunchAngleOffsetDegrees = -6.5;
    physics.strike(neutralVolley, {640.0, 180.0}, 20.0, 0.0, 0.0,
                   0.0, 23.1384, volleyImpact);
    assert(neutralVolley.verticalVelocity > 0.0);
    assert(neutralVolley.verticalVelocity < neutralGroundstroke.verticalVelocity * 0.65);

    tennis::BallState cappedReturn = incomingReturn;
    cappedReturn.velocity = {0.0, 55.0 * 23.1384};
    returnImpact.horizontalVelocityRetention = 0.80;
    returnImpact.maxOutgoingSpeedMps = 32.0;
    physics.strike(cappedReturn, {640.0, 120.0}, 30.0, 0.0, 0.0,
                   0.0, 23.1384, returnImpact);
    const double cappedHorizontalMps = cappedReturn.velocity.length() / 23.1384;
    const double cappedSpeedMps = std::sqrt(cappedHorizontalMps * cappedHorizontalMps +
                                            cappedReturn.verticalVelocity * cappedReturn.verticalVelocity);
    assert(cappedSpeedMps <= 32.0 + 1e-9);

    const tennis::PlayerProfile computer = tennis::makeDefaultOpponentProfile();
    assert(computer.displayName == "COMPUTER");

    tennis::PlayerProfile player = tennis::ProgressionSystem::makeNewCampaignPlayer();
    tennis::ProgressionState progress = tennis::ProgressionSystem::makeNewCampaignProgress();
    assert(!tennis::ProgressionSystem::canEnterLesson(progress, 1));
    const double originalStability = player.attributes.stability;
    assert(tennis::ProgressionSystem::completeLesson(player, progress, 0));
    assert(progress.completedLessons == 1);
    assert(player.attributes.stability > originalStability);
    assert(tennis::ProgressionSystem::canEnterLesson(progress, 1));
    const double rewardedStability = player.attributes.stability;
    assert(tennis::ProgressionSystem::completeLesson(player, progress, 0));
    assert(player.attributes.stability == rewardedStability);

    tennis::ScoringState score;
    score.mode = tennis::ScoreMode::Sets;
    tennis::ScoringSystem::recordPoint(score, true);
    tennis::ScoringSystem::recordPoint(score, true);
    tennis::ScoringSystem::recordPoint(score, true);
    tennis::ScoringSystem::recordPoint(score, false);
    tennis::ScoringSystem::recordPoint(score, false);
    tennis::ScoringSystem::recordPoint(score, false);
    assert(tennis::ScoringSystem::displayPoint(score.playerPoints, score.opponentPoints,
                                               score.mode) == "40");
    tennis::ScoringSystem::recordPoint(score, true);
    assert(tennis::ScoringSystem::displayPoint(score.playerPoints, score.opponentPoints,
                                               score.mode) == "AD");
    const tennis::ScoreResult game = tennis::ScoringSystem::recordPoint(score, true);
    assert(game.milestone == tennis::ScoreMilestone::Game);
    assert(score.playerGames == 1 && score.playerPoints == 0 && score.opponentPoints == 0);

    tennis::ScoringState setScore;
    assert(setScore.mode == tennis::ScoreMode::Sets);
    tennis::ScoreResult setResult;
    for (int gameIndex = 0; gameIndex < 6; ++gameIndex) {
        for (int pointIndex = 0; pointIndex < 4; ++pointIndex) {
            setResult = tennis::ScoringSystem::recordPoint(setScore, true);
        }
    }
    assert(setResult.milestone == tennis::ScoreMilestone::Set);
    assert(setScore.playerSets == 1 && setScore.playerGames == 0);
    assert(setScore.completedSetCount == 1);
    assert(setScore.playerSetGames[0] == 6 && setScore.opponentSetGames[0] == 0);
    for (int gameIndex = 0; gameIndex < 6; ++gameIndex) {
        for (int pointIndex = 0; pointIndex < 4; ++pointIndex) {
            setResult = tennis::ScoringSystem::recordPoint(setScore, true);
        }
    }
    assert(setResult.milestone == tennis::ScoreMilestone::Match);
    assert(setScore.newMatchPending && setScore.completedSetCount == 2);
    tennis::ScoringSystem::recordPoint(setScore, false);
    assert(!setScore.newMatchPending && setScore.completedSetCount == 0);
    assert(setScore.opponentPoints == 1);
    return 0;
}
