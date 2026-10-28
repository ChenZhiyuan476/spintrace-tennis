#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

tennis::PlayerInputCommand Game::keyboardInput(const bool *keys) const {
    return {
        {
            (keys[SDL_SCANCODE_D] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_A] ? 1.0 : 0.0),
            (keys[SDL_SCANCODE_S] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_W] ? 1.0 : 0.0)
        },
        keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]
    };
}

void Game::handleEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && !event.key.repeat) {
        paused = !paused;
        pausePage = PausePage::Main;
        pauseDrawing = false;
        dragging = false;
        swingArmed = false;
        swingChargeLocked = false;
        swingChargeTime = 0.0;
        previousSwingContactQuality = 0.0;
        swingReachedYellowZone = false;
        return;
    }
    if (paused) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) pauseMouse = {event.motion.x, event.motion.y};
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
            pauseMouse = {event.button.x, event.button.y};
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            const bool wasSwingGuide = pausePage == PausePage::SwingGuide;
            const bool clickedSwingControl =
                (event.button.x >= 690.0 && event.button.x <= 870.0 &&
                 ((event.button.y >= 150.0 && event.button.y <= 186.0) ||
                  (event.button.y >= 350.0 && event.button.y <= 384.0) ||
                  (event.button.y >= 390.0 && event.button.y <= 424.0))) ||
                (event.button.x >= 400.0 && event.button.x <= 500.0 &&
                 event.button.y >= 96.0 && event.button.y <= 132.0) ||
                (event.button.x >= 785.0 && event.button.x <= 880.0 &&
                 event.button.y >= 96.0 && event.button.y <= 132.0);
            handlePauseClick(event.button.x, event.button.y);
            if (wasSwingGuide && pausePage == PausePage::SwingGuide &&
                !clickedSwingControl) {
                pauseDrawing = true;
                dragStart = {event.button.x, event.button.y};
                dragCurrent = dragStart;
                dragDuration = 0.0;
                gesturePathLength = 0.0;
                gesturePointCount = 0;
                pauseDrawStartedNs = SDL_GetTicksNS();
                recordGesturePoint(dragStart);
            }
        } else if (event.type == SDL_EVENT_MOUSE_MOTION && pauseDrawing) {
            dragCurrent = {event.motion.x, event.motion.y};
            dragDuration = std::max(0.01, (SDL_GetTicksNS() - pauseDrawStartedNs) / 1'000'000'000.0);
            recordGesturePoint(dragCurrent);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT && pauseDrawing) {
            dragCurrent = {event.button.x, event.button.y};
            dragDuration = std::max(0.01, (SDL_GetTicksNS() - pauseDrawStartedNs) / 1'000'000'000.0);
            recordGesturePoint(dragCurrent);
            pauseDrawing = false;
        }
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT) {
        if (!match.ball.inPlay && match.currentServerPlayer && !serveTossing) {
            serveTossing = true;
            serveWindowOpened = false;
            serveHitWindowTime = 0.0;
            match.ball.position = match.playerPosition + Vec2{forehand ? 15.0 : -15.0, -8.0};
            match.ball.height = 1.02;
            match.ball.verticalVelocity = shotSpeedTuning.serveTossVelocity;
            statusMessage = "TOSS UP | PREPARE LEFT-MOUSE SWING";
            statusTime = 1.0;
        } else if (!match.ball.inPlay && !match.currentServerPlayer) {
            // Allow a split-step input while waiting for the computer serve.
            // cpuServe() resolves whether it landed inside the timing window.
            splitStepAvailable = true;
            splitStepInputTime = 0.20;
        } else if (match.ball.inPlay && splitStepAvailable && (splitStepInputTime <= 0.0 || splitStepReactionTime > 0.0)) {
            splitStepInputTime = 0.20;
            if (splitStepReactionTime > 0.0) activateSplitStep();
        }
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
        (match.ball.inPlay || match.currentServerPlayer)) {
        dragging = true;
        swingArmed = true;
        swingChargeLocked = false;
        dragStart = {event.button.x, event.button.y};
        dragCurrent = dragStart;
        dragDuration = 0.0;
        swingChargeTime = 0.0;
        previousSwingContactQuality = 0.0;
        swingReachedYellowZone = false;
        gesturePathLength = 0.0;
        gesturePointCount = 0;
        recordGesturePoint(dragStart);
    } else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging) {
        dragCurrent = {event.motion.x, event.motion.y};
        recordGesturePoint(dragCurrent);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT && dragging) {
        dragCurrent = {event.button.x, event.button.y};
        recordGesturePoint(dragCurrent);
        // In auto-hit mode, releasing early during a rally locks both the
        // finished gesture and its current charge until the incoming ball
        // reaches the automatic contact zone. Serves always retain the direct
        // release-to-hit behaviour and are never stored as a locked swing.
        if (controlSettings.autoHit && match.ball.inPlay) {
            dragging = false;
            swingChargeLocked = true;
            char lockedMessage[64];
            std::snprintf(lockedMessage, sizeof(lockedMessage), "POWER LOCKED %.0f%%",
                          swingChargeProgress() * 100.0);
            statusMessage = lockedMessage;
            statusTime = 0.8;
        } else {
            releasePlayerSwing();
        }
    }
}

void Game::recordGesturePoint(const Vec2 &point) {
    if (gesturePointCount > 0) {
        const double segment = gesturePoints[gesturePointCount - 1].distanceTo(point);
        if (segment < 1.5) return;
        gesturePathLength += segment;
    }
    if (gesturePointCount < MaxGesturePoints) {
        gesturePoints[gesturePointCount++] = point;
    } else {
        std::move(gesturePoints.begin() + 1, gesturePoints.end(), gesturePoints.begin());
        gesturePoints.back() = point;
    }
}

double Game::gesturePower() const {
    // Power is intentionally independent of gesture speed. The mouse path now
    // owns direction, spin and height while holding time owns racket power.
    return 0.07 + 0.93 * std::pow(swingChargeProgress(), 1.18);
}

double Game::swingChargeProgress() const {
    const bool serveCharge = paused && pausePage == PausePage::SwingGuide
        ? selectedMappingProfile == 1
        : (!match.ball.inPlay && match.currentServerPlayer);
    const double fullChargeSeconds = serveCharge
        ? controlSettings.serveFullChargeSeconds
        : controlSettings.rallyFullChargeSeconds;
    if (paused && pausePage == PausePage::SwingGuide) {
        double previewTime = dragDuration;
        if (pauseDrawing && pauseDrawStartedNs > 0) {
            previewTime = std::max(previewTime,
                (SDL_GetTicksNS() - pauseDrawStartedNs) / 1'000'000'000.0);
        }
        return std::clamp(previewTime / fullChargeSeconds, 0.0, 1.0);
    }
    return std::clamp(swingChargeTime / fullChargeSeconds, 0.0, 1.0);
}

double Game::estimatedHitStaminaCost(double power, double complexity,
                                     double charge, bool smash) const {
    return (staminaTuning.baseHitCost + 7.0 * std::pow(power, 1.35) +
            7.5 * std::pow(charge, 1.55) + complexity * 3.0 +
            (smash ? 2.0 : 0.0)) /
           std::max(0.20, playerProfile.attributes.endurance);
}

void Game::releasePlayerSwing() {
    if (!swingArmed) return;
    tryPlayerHit();
    dragging = false;
    swingArmed = false;
    swingChargeLocked = false;
    previousSwingContactQuality = 0.0;
    swingReachedYellowZone = false;
}

void Game::updateChargedSwing() {
    if (!swingArmed) return;

    // With auto-hit enabled, a serve releases only after it has crossed the
    // whole green band on the way down. Manual release remains available.
    if (!match.ball.inPlay) {
        if (!controlSettings.autoHit || !serveTossing) return;
        const double serveQuality = std::clamp(
            1.0 - std::abs(match.ball.height - shotSpeedTuning.serveContactHeight) / 0.58,
            0.0, 1.0);
        constexpr double serveGreenThreshold = 0.72;
        if (serveQuality >= serveGreenThreshold) swingReachedYellowZone = true;
        const bool leftGreenOnDescent = swingReachedYellowZone &&
            match.ball.verticalVelocity < 0.0 && serveQuality < serveGreenThreshold &&
            previousSwingContactQuality >= serveGreenThreshold;
        previousSwingContactQuality = serveQuality;
        if (leftGreenOnDescent) releasePlayerSwing();
        return;
    }
    if (!controlSettings.autoHit) return;
    if (match.ball.lastHitter != 1 || match.ball.position.y <= NetY) return;
    if (match.ball.serveReturnPending && match.ball.bounceCount == 0) return;

    const double contactQuality = std::min(currentHeightQuality(),
                                           currentPositionQuality());
    constexpr double greenThreshold = 0.72;
    constexpr double yellowExitThreshold = 0.18;
    if (contactQuality > 0.01) swingReachedYellowZone = true;

    const bool enteredGreen = contactQuality >= greenThreshold &&
                              previousSwingContactQuality < greenThreshold;
    const bool leavingYellow = swingReachedYellowZone &&
                               contactQuality > 0.01 &&
                               contactQuality <= yellowExitThreshold &&
                               contactQuality < previousSwingContactQuality;
    previousSwingContactQuality = contactQuality;
    if (enteredGreen || leavingYellow) releasePlayerSwing();
}

double Game::gestureVerticalSpin() const {
    if (gesturePointCount < 4 || gesturePathLength < 30.0) return 0.0;
    const Vec2 start = gesturePoints.front();
    const Vec2 end = gesturePoints[gesturePointCount - 1];
    double weightedDeviation = 0.0;
    double weightTotal = 0.0;
    for (int i = 1; i < gesturePointCount - 1; ++i) {
        const double t = static_cast<double>(i) / (gesturePointCount - 1);
        const double weight = std::sin(t * 3.141592653589793);
        const double lineY = start.y + (end.y - start.y) * t;
        weightedDeviation += (lineY - gesturePoints[i].y) * weight;
        weightTotal += weight;
    }
    const double verticalBend = weightedDeviation / std::max(weightTotal, 1.0);
    if (std::abs(verticalBend) < 4.0) return 0.0;
    return std::clamp(verticalBend / 145.0, -1.0, 1.0);
}

double Game::gestureDirection() const {
    if (gesturePointCount < 2) return 0.0;
    return gesturePoints[gesturePointCount - 1].x - gesturePoints.front().x;
}

double Game::gestureSideCurve() const {
    if (gesturePointCount < 4 || gesturePathLength < 30.0) return 0.0;
    const Vec2 start = gesturePoints.front();
    const Vec2 end = gesturePoints[gesturePointCount - 1];
    double weightedDeviation = 0.0;
    double weightTotal = 0.0;
    for (int i = 1; i < gesturePointCount - 1; ++i) {
        const double t = static_cast<double>(i) / (gesturePointCount - 1);
        const double weight = std::sin(t * 3.141592653589793);
        const double lineX = start.x + (end.x - start.x) * t;
        weightedDeviation += (gesturePoints[i].x - lineX) * weight;
        weightTotal += weight;
    }
    return std::clamp(weightedDeviation / std::max(weightTotal, 1.0) / 165.0, -1.0, 1.0);
}

double Game::gestureLoft() const {
    if (gesturePointCount < 2) return 0.0;
    const double deltaY = gesturePoints[gesturePointCount - 1].y - gesturePoints.front().y;
    // Screen Y grows downward: an upward endpoint requests lift, while even a
    // modest downward endpoint requests a closed racket face. Keep this raw
    // signal centred on zero; each shot profile supplies its own neutral arc.
    return std::clamp(-deltaY / 420.0, -1.0, 1.0);
}

double Game::gestureFlatDrive() const {
    const double curvatureLoad = std::abs(gestureVerticalSpin()) * 0.68 +
                                 std::abs(gestureSideCurve()) * 0.46;
    return std::clamp(1.0 - curvatureLoad, 0.20, 1.0);
}

tennis::StrokeInputCommand Game::mappedGesture(int profileIndex) const {
    const MappingProfile &profile = mappingProfiles[std::clamp(profileIndex, 0, 4)];
    const tennis::StrokePreferences &preference = playerProfile.stroke;
    tennis::StrokeInputCommand result;
    const double rawPower = gesturePower();
    const double powerScale = playerProfile.attributes.swingPower * preference.powerBias *
                              preference.gripPower() * preference.motionPower();
    const double powerMinimum = std::clamp(profile.powerMin, 0.0, 1.25);
    const double powerMaximum = std::max(powerMinimum,
        std::min(profile.powerMax, playerProfile.attributes.maximumSwingPower));
    result.power = std::clamp((0.08 + (rawPower - 0.08) * profile.power) * powerScale,
                              powerMinimum, powerMaximum);
    const double rawVertical = gestureVerticalSpin();
    const double verticalPreference = rawVertical >= 0.0
        ? preference.topspinBias * preference.gripTopspin()
        : preference.sliceBias * preference.gripSlice();
    result.verticalSpin = std::clamp(rawVertical * profile.verticalSpin * verticalPreference *
                                     preference.motionSpin(), -1.0, 1.0);
    result.sideSpin = std::clamp(gestureSideCurve() * profile.sideSpin *
                                 preference.sideSpinBias * preference.motionSpin(), -1.0, 1.0);
    if (result.verticalSpin > 0.0 && !playerProfile.hasTechnique(tennis::Technique::Topspin))
        result.verticalSpin = 0.0;
    if (result.verticalSpin < 0.0 && !playerProfile.hasTechnique(tennis::Technique::Slice))
        result.verticalSpin = 0.0;
    if (!playerProfile.hasTechnique(tennis::Technique::SideSpin)) result.sideSpin = 0.0;
    const double curvatureLoad = std::abs(result.verticalSpin) * 0.68 +
                                 std::abs(result.sideSpin) * 0.46;
    const double rawFlatDrive = std::clamp(1.0 - curvatureLoad, 0.20, 1.0);
    result.flatDrive = std::clamp(0.20 + (rawFlatDrive - 0.20) * profile.flatDrive, 0.20, 1.0);
    result.lateralAimPixels = gestureDirection() * profile.direction;
    result.direction = std::clamp(result.lateralAimPixels /
                                  ((SinglesRight - SinglesLeft) * 0.5), -1.0, 1.0);
    if (!playerProfile.hasTechnique(tennis::Technique::Direction)) {
        result.direction = 0.0;
        result.lateralAimPixels = 0.0;
    }
    const double rawHeight = gestureLoft();
    // Groundstrokes keep a small safety arc. Serves and front-court strokes
    // start lower, while volleys and smashes are neutral at an exactly level
    // gesture so drawing downward produces a negative value immediately.
    static constexpr std::array<double, 5> neutralHeight{
        0.10, 0.05, 0.00, 0.04, 0.00
    };
    const double mappedHeight = neutralHeight[std::clamp(profileIndex, 0, 4)] +
                                rawHeight * profile.height;
    double effectiveHeightMin = profile.heightMin;
    if (profileIndex != 1) {
        // Downward hitting is a continuous court-position affordance rather
        // than a sudden property of a named stroke. It is closed at and behind
        // the baseline, then opens progressively as the player approaches the
        // net, reaching the configured profile limit only near the tape.
        const double netApproach = std::clamp(
            (CourtBottom - match.playerPosition.y) / (CourtBottom - NetY), 0.0, 1.0);
        const double distanceLimitedMinimum = -0.70 * std::pow(netApproach, 1.35);
        effectiveHeightMin = std::max(profile.heightMin, distanceLimitedMinimum);
    }
    result.height = std::clamp(mappedHeight, effectiveHeightMin, profile.heightMax);
    result.smoothness = gestureSmoothness();
    return result;
}

double Game::gestureSmoothness() const {
    if (gesturePointCount < 5) return 0.72;
    std::vector<double> turns;
    for (int i = 2; i < gesturePointCount; ++i) {
        const Vec2 a = (gesturePoints[i - 1] - gesturePoints[i - 2]).normalized();
        const Vec2 b = (gesturePoints[i] - gesturePoints[i - 1]).normalized();
        const double cross = a.x * b.y - a.y * b.x;
        const double dot = std::clamp(a.x * b.x + a.y * b.y, -1.0, 1.0);
        turns.push_back(std::atan2(cross, dot));
    }
    if (turns.size() < 2) return 0.72;
    double variation = 0.0;
    for (size_t i = 1; i < turns.size(); ++i) variation += std::abs(turns[i] - turns[i - 1]);
    variation /= static_cast<double>(turns.size() - 1);
    return std::clamp(1.0 - variation / 0.72, 0.12, 1.0);
}

void Game::tryPlayerHit() {
    const double chargeAtHit = swingChargeProgress();
    const bool serveAttempt = !match.ball.inPlay;
    const bool overhandServe = serveAttempt && serveTossing && serveHitWindowTime > 0.0;
    const bool underhandServe = serveAttempt && !serveTossing;
    const bool canServe = overhandServe || underhandServe;
    const int incomingBounces = match.ball.bounceCount;
    const double contactHeightQuality = currentHeightQuality();
    const double contactPositionQuality = currentPositionQuality();
    const bool reachable = match.ball.inPlay && match.ball.lastHitter == 1 && match.ball.position.y > NetY &&
            contactHeightQuality > 0.01 && contactPositionQuality > 0.01;
    if (serveAttempt && !canServe) {
        statusMessage = serveTossing ? "SERVE CONTACT MISSED" : "RIGHT CLICK TO TOSS FIRST";
        statusTime = 1.2;
        return;
    }
    if (!canServe && match.ball.serviceShot && match.ball.netHit && incomingBounces == 0) {
        statusMessage = "SERVE TOUCHED NET | WAIT FOR LET CALL";
        statusTime = 0.8;
        return;
    }
    if (!canServe && match.ball.serveReturnPending && incomingBounces == 0) {
        statusMessage = "LET THE SERVE BOUNCE";
        statusTime = 1.0;
        return;
    }
    if (!canServe && !reachable) {
        statusMessage = "MISS - MOVE CLOSER AND WATCH BALL HEIGHT";
        statusTime = 1.4;
        return;
    }

    const double rawVerticalInput = gestureVerticalSpin();
    const bool smashCandidate = !canServe && playerProfile.hasTechnique(tennis::Technique::Smash) &&
                                match.ball.height > 1.72 && rawVerticalInput > -0.20;
    const bool volleyCandidate = !canServe && !smashCandidate && incomingBounces == 0 &&
                                 playerProfile.hasTechnique(tennis::Technique::Volley);
    const bool frontCourtCandidate = !canServe && !smashCandidate && !volleyCandidate &&
                                     incomingBounces >= 1 &&
                                     match.playerPosition.y < NetY + ServiceOffset + 23.0;
    const int mappingIndex = canServe ? 1 : (volleyCandidate ? 2 :
                             (frontCourtCandidate ? 3 : (smashCandidate ? 4 : 0)));
    const tennis::StrokeInputCommand mapped = mappedGesture(mappingIndex);
    const double power = mapped.power;
    const double incomingSpeedMps = match.ball.inPlay ? match.ball.velocity.length() / PixelsPerMeter : 0.0;
    const double incomingVerticalSpinRpm = match.ball.inPlay ? match.ball.spinRpm : 0.0;
    const double incomingSideSpinRpm = match.ball.inPlay ? match.ball.sideSpinRpm : 0.0;
    const double verticalInput = mapped.verticalSpin;
    const double sideCurve = mapped.sideSpin;
    double loft = mapped.height;
    const double requestedLoft = loft;
    const double smoothness = mapped.smoothness;
    const double horizontal = mapped.direction;
    const double lateralAimPixels = mapped.lateralAimPixels;
    forehand = canServe || match.ball.position.x >= match.playerPosition.x;

    const double flatAllocation = 0.52 + mapped.flatDrive * 0.48;
    const double travelPower = power * (0.78 + flatAllocation * 0.22);
    const double depth = 98.0 + travelPower * 158.0;
    const double legalAimLeft = match.rules.boundaryMode == tennis::BoundaryMode::Singles
        ? SinglesLeft : CourtLeft;
    const double legalAimRight = match.rules.boundaryMode == tennis::BoundaryMode::Singles
        ? SinglesRight : CourtRight;
    Vec2 target{std::clamp(640.0 + lateralAimPixels,
                           legalAimLeft + 10.0, legalAimRight - 10.0),
                NetY - depth};
    if (canServe) {
        if (match.rules.serveRulesEnabled) {
            // Aim through the interior of the service box rather than along
            // its net/line edges. Gesture direction still moves the serve, but
            // the default target keeps a realistic safety margin.
            target.y = NetY - (60.0 + travelPower * (ServiceOffset - 88.0));
            match.ball.serviceTargetLeft = ((match.score.playerPoints + match.score.opponentPoints) % 2 == 0);
            const double boxT = 0.18 + (horizontal + 1.0) * 0.32;
            if (match.ball.serviceTargetLeft) {
                target.x = SinglesLeft + 18.0 + boxT * (640.0 - SinglesLeft - 36.0);
            } else {
                target.x = 640.0 + 18.0 + boxT * (SinglesRight - 640.0 - 36.0);
            }
        } else {
            target.x = std::clamp(640.0 + lateralAimPixels,
                                  CourtLeft + 10.0, CourtRight - 10.0);
            target.y = NetY - (115.0 + travelPower * 165.0);
        }
    }
    const double heightQuality = overhandServe
        ? std::clamp(1.0 - std::abs(match.ball.height - shotSpeedTuning.serveContactHeight) /
                     0.58, 0.12, 1.0)
        : currentHeightQuality();
    const double staminaRatio = std::clamp(match.playerStamina /
        std::max(1.0, playerProfile.attributes.staminaCapacity), 0.0, 1.0);
    const double staminaQuality = staminaTuning.accuracyQualityAt(staminaRatio);
    const double lengthQuality = std::clamp(gesturePathLength / 150.0, 0.25, 1.0);
    const double complexity = std::clamp(std::abs(verticalInput) * 0.30 + std::abs(sideCurve) * 0.42 +
                                         std::abs(loft) * 0.28, 0.0, 1.0);
    const double complexityError = complexity * (1.0 - smoothness) * 0.30;
    const double powerError = std::max(0.0, power - 0.68) * 0.28;
    const double positionQuality = canServe ? 1.0 : contactPositionQuality;
    const double incomingPacePressure = canServe ? 0.0 :
        std::clamp((incomingSpeedMps - 12.0) / 20.0, 0.0, 1.0);
    const double incomingSpinPressure = canServe ? 0.0 :
        std::clamp((std::abs(incomingVerticalSpinRpm) +
                    std::abs(incomingSideSpinRpm) * 0.65) / 4200.0, 0.0, 1.0);
    const double pressureExposure = (1.0 - positionQuality) * 0.58 +
                                    (1.0 - smoothness) * 0.42;
    const double incomingPressureError = incomingPacePressure * (0.025 + pressureExposure * 0.13) +
                                         incomingSpinPressure * (0.018 + pressureExposure * 0.09);
    double accuracy = 0.08 + heightQuality * 0.23 + positionQuality * 0.16 +
                      staminaQuality * 0.20 + smoothness * 0.22 + lengthQuality * 0.11 -
                      complexityError - powerError - incomingPressureError;
    const double control = playerProfile.attributes.stability * playerProfile.stroke.controlBias *
                           playerProfile.stroke.motionControl();
    accuracy = 1.0 - (1.0 - accuracy) / std::max(0.25, control);
    if (canServe) accuracy += 0.12;
    accuracy = std::clamp(accuracy, 0.08, 0.97);

    const double error = 1.0 - accuracy;
    target.x += randomRange(canServe ? -48.0 : -105.0,
                            canServe ? 48.0 : 105.0) * error;
    target.y += randomRange(canServe ? -45.0 : -100.0,
                            canServe ? 55.0 : 125.0) * error;
    const MappingProfile &activeMapping = mappingProfiles[mappingIndex];
    loft = std::clamp(loft + randomRange(canServe ? -0.20 : -0.52,
                                         canServe ? 0.16 : 0.24) * error,
                      activeMapping.heightMin, activeMapping.heightMax);
    // Inaccuracy may make a downward command steeper, shallower or wider, but
    // should not silently reverse a clearly closed racket face into an upward
    // launch. That made identical downward gestures feel inconsistent.
    if (requestedLoft < -0.015) loft = std::min(loft, -0.01);

    impactPosition = match.ball.position;
    impactHeight = match.ball.height;
    impactHitter = 0;
    impactEffectTime = 0.16;
    hitStopTime = 0.045;

    match.ball.inPlay = true;
    match.ball.serviceShot = canServe;
    match.ball.serviceFromBottom = true;
    match.ball.serveReturnPending = canServe;
    match.ball.lastHitter = 0;
    match.ball.bounceCount = 0;
    const double verticalRpm = verticalInput >= 0.0
        ? verticalInput * std::sqrt(power) * 3600.0
        : verticalInput * std::sqrt(power) * 2700.0;
    const double sideRpm = sideCurve * std::sqrt(power) * 2300.0;

    const bool smash = smashCandidate;
    const bool volley = volleyCandidate;
    std::string shot;
    if (underhandServe) shot = "UNDERHAND SERVE";
    else if (overhandServe && verticalInput > 0.15) shot = "KICK SERVE";
    else if (overhandServe && (verticalInput < -0.10 || std::abs(sideCurve) > 0.18)) shot = "SLICE SERVE";
    else if (overhandServe) shot = "FLAT SERVE";
    else if (smash) shot = "SMASH";
    else if (volley && verticalInput < -0.18 && power < 0.45) shot = "DROP VOLLEY";
    else if (volley) shot = "VOLLEY";
    else if (loft > 0.56) shot = "LOB";
    else if (verticalInput < -0.18 && power < 0.38) shot = "DROP SHOT";
    else if (verticalInput > 0.08) shot = "TOPSPIN";
    else if (verticalInput < -0.08) shot = "SLICE";
    else shot = "FLAT";

    if (canServe) {
        playerAction = 5;
        playerActionTime = 0.55;
    } else if (smash) {
        playerAction = 4;
        playerActionTime = 0.44;
    } else if (volley) {
        playerAction = 3;
        playerActionTime = 0.30;
    } else if (verticalInput < -0.08) {
        playerAction = 6;
        playerActionTime = 0.40;
    } else {
        playerAction = forehand ? 1 : 2;
        playerActionTime = 0.40;
    }

    const double speedPower = std::pow(power, shotSpeedTuning.playerPowerExponent);
    double shotSpeed = shotSpeedTuning.playerGroundBase +
                       speedPower * shotSpeedTuning.playerGroundPower * flatAllocation;
    if (smash) {
        shotSpeed += shotSpeedTuning.playerSmashBonus;
    } else if (overhandServe) {
        shotSpeed += shotSpeedTuning.playerOverhandServeBonus;
    } else if (underhandServe) {
        shotSpeed = shotSpeedTuning.playerUnderhandBase +
                    speedPower * shotSpeedTuning.playerUnderhandPower * flatAllocation;
        loft = std::max(loft, 0.06);
    } else if (volley) {
        shotSpeed = shotSpeedTuning.playerVolleyBase +
                    speedPower * shotSpeedTuning.playerVolleyPower;
    }
    shotSpeed *= playerProfile.attributes.shotSpeed;
    // Accuracy changes the landing point and launch height. Keep pace tied
    // closely to charge so the same hold duration reliably repeats a speed.
    shotSpeed *= 1.0 + randomRange(-0.025, 0.025) * error;
    if (canServe) {
        launchBall(target, shotSpeed, verticalRpm, sideRpm, loft);
    } else {
        strikeBall(target, shotSpeed, verticalRpm, sideRpm, loft,
                   racketImpactProfiles[mappingIndex]);
    }
    const double soundSpinAmount = std::clamp(
        std::abs(verticalRpm) / 3600.0 * 0.68 +
        std::abs(sideRpm) / 2300.0 * 0.48, 0.0, 1.0);
    audio.playRacketHit(power, soundSpinAmount, accuracy, canServe);
    serveTossing = false;
    serveWindowOpened = false;
    serveHitWindowTime = 0.0;
    splitStepAvailable = true;
    splitStepSpeedBuffActive = false;
    splitStepReactionTime = 0.0;
    splitStepInputTime = 0.0;

    const double staminaAtHit = match.playerStamina;
    const double hitCost = estimatedHitStaminaCost(power, complexity,
                                                   chargeAtHit, smash);
    match.playerStamina = std::max(0.0, match.playerStamina - hitCost);
    addShotRecord(shot, power, match.ball.spinRpm, match.ball.sideSpinRpm, loft, horizontal,
                  mapped.flatDrive, accuracy, staminaAtHit, mappingIndex);
    char message[128];
    std::snprintf(message, sizeof(message), "%s | PWR %.0f%% | ACC %.0f%% | COST %.1f", shot.c_str(), power * 100.0, accuracy * 100.0, hitCost);
    statusMessage = message;
    statusTime = 1.0;
}
