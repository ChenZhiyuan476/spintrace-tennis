#include "game.hpp"
#include "render_primitives.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace tennis::render;

namespace {

Color colorOf(tennis::Rgba color) {
    return {color.r, color.g, color.b, color.a};
}

Color strokeProfileColor(int profileIndex) {
    switch (profileIndex) {
        case 1: return {0.98f, 0.80f, 0.30f, 1.0f}; // serve / gold
        case 2: return {0.42f, 0.92f, 0.58f, 1.0f}; // volley / mint
        case 3: return {1.00f, 0.58f, 0.28f, 1.0f}; // front court / orange
        case 4: return {1.00f, 0.36f, 0.40f, 1.0f}; // smash / red
        default: return {0.32f, 0.82f, 1.00f, 1.0f}; // groundstroke / cyan
    }
}

void drawAvatar(SDL_Renderer *renderer, Vec2 center, double radius,
                const tennis::BallAvatarStyle &avatar) {
    const Color primary = colorOf(avatar.primary);
    const Color secondary = colorOf(avatar.secondary);
    const double r = radius * avatar.radiusScale;
    pixelTennisBall(renderer, center, r, primary, false);
    switch (avatar.pattern) {
        case tennis::BallPattern::CenterDot:
            fillRect(renderer, center.x - r * 0.22, center.y - r * 0.22,
                     r * 0.44, r * 0.44, secondary);
            break;
        case tennis::BallPattern::Stripe:
            fillRect(renderer, center.x - r * 0.72, center.y - r * 0.14,
                     r * 1.44, r * 0.28, secondary);
            break;
        case tennis::BallPattern::Split:
            fillRect(renderer, center.x, center.y - r * 0.60,
                     r * 0.72, r * 1.20, secondary);
            break;
        default:
            break;
    }
}

} // namespace

double Game::perspectiveScale(double worldY) const {
    const double t = std::clamp((worldY - CourtTop) / CourtHeight, 0.0, 1.0);
    return 0.72 + 0.58 * std::pow(t, 1.18);
}

Vec2 Game::projectPoint(const Vec2 &world, double heightM) const {
    const double rawT = (world.y - CourtTop) / CourtHeight;
    const double t = std::clamp(rawT, 0.0, 1.0);
    const double curved = std::pow(t, 1.18);
    constexpr double courtScreenLength = 370.0;
    double screenY = 170.0 + curved * courtScreenLength;
    if (rawT < 0.0) screenY += rawT * 170.0;
    else if (rawT > 1.0) screenY += (rawT - 1.0) * (courtScreenLength * 1.18);
    const double halfWidth = 170.0 + 125.0 * curved;
    const double screenX = 640.0 + (world.x - 640.0) / (CourtWidth * 0.5) * halfWidth;
    return {screenX, screenY - heightM * (28.0 + 18.0 * curved)};
}

void Game::render(SDL_Renderer *renderer) const {
    const tennis::CourtSurfaceProfile &surfaceProfile =
        tennis::courtSurfaceProfile(activeCourtPreset);
    const Color themeSurface = colorOf(courtTheme.surface);
    const Color themeSurround = colorOf(courtTheme.surround);
    const Color themeLines = colorOf(courtTheme.lines);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Letterbox presentation normally leaves physical pixels outside the
    // 1280x720 logical frame untouched. Paint those pixels first at native
    // output resolution, extending the same perspective stripe boundaries so
    // fullscreen keeps a continuous court without stretching the game view.
    int outputWidth = Width;
    int outputHeight = Height;
    SDL_FRect presentationRect{0.0f, 0.0f, static_cast<float>(Width),
                               static_cast<float>(Height)};
    const bool hasPhysicalBackdrop =
        SDL_GetCurrentRenderOutputSize(renderer, &outputWidth, &outputHeight) &&
        SDL_GetRenderLogicalPresentationRect(renderer, &presentationRect) &&
        outputWidth > 0 && outputHeight > 0 && presentationRect.w > 0.0f &&
        presentationRect.h > 0.0f;
    // On Retina/HiDPI backends the queried output is expressed in physical
    // pixels while render geometry and logical-presentation rectangles use
    // window coordinates. Infer that coordinate-space extent from the centred
    // presentation rect so bottom clips do not end up two times off-screen.
    const int backdropWidth = hasPhysicalBackdrop
        ? std::max(1, static_cast<int>(std::ceil(
            presentationRect.w + presentationRect.x * 2.0f)))
        : outputWidth;
    const int backdropHeight = hasPhysicalBackdrop
        ? std::max(1, static_cast<int>(std::ceil(
            presentationRect.h + presentationRect.y * 2.0f)))
        : outputHeight;
    // OVERSCAN deliberately crops the logical frame on displays that are
    // taller than 16:9. Convert the physical viewport edges back into logical
    // coordinates so HUD elements can anchor to what is actually visible.
    double visibleLogicalLeft = 0.0;
    double visibleLogicalRight = Width;
    if (hasPhysicalBackdrop) {
        const double logicalScale = presentationRect.w / Width;
        if (logicalScale > 1e-6) {
            visibleLogicalLeft = std::clamp(
                (0.0 - presentationRect.x) / logicalScale, 0.0,
                static_cast<double>(Width));
            visibleLogicalRight = std::clamp(
                (backdropWidth - presentationRect.x) / logicalScale, 0.0,
                static_cast<double>(Width));
        }
    }
    constexpr double hudSafeMargin = 8.0;
    const double leftHudX = visibleLogicalLeft + hudSafeMargin;
    constexpr double contactAssistWidth = 260.0;
    const double contactAssistX = visibleLogicalRight - hudSafeMargin - contactAssistWidth;
    if (hasPhysicalBackdrop) {
        SDL_SetRenderLogicalPresentation(renderer, 0, 0,
                                         SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderViewport(renderer, nullptr);
        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        setColor(renderer, themeSurround);
        SDL_RenderClear(renderer);
        if (surfaceProfile.drawMowingStripes) {
            const Color stripe = colorOf(surfaceProfile.mowingStripeColor);
            constexpr double stripeWidth = CourtWidth / 10.0;
            auto toOutput = [&](Vec2 logical) {
                return Vec2{
                    presentationRect.x + logical.x * presentationRect.w / Width,
                    presentationRect.y + logical.y * presentationRect.h / Height
                };
            };
            auto stripeBoundaryAtOutputY = [&](double worldX, double outputY) {
                const Vec2 farPoint = toOutput(projectPoint({worldX, CourtTop}));
                const Vec2 nearPoint = toOutput(projectPoint({worldX, CourtBottom}));
                const double t = (outputY - farPoint.y) /
                                 std::max(1e-6, nearPoint.y - farPoint.y);
                return farPoint + (nearPoint - farPoint) * t;
            };
            for (int band = -64; band < 64; band += 2) {
                const double left = 640.0 + band * stripeWidth;
                const double right = left + stripeWidth;
                fillQuad(renderer,
                         {stripeBoundaryAtOutputY(left, -4.0),
                          stripeBoundaryAtOutputY(right, -4.0),
                          stripeBoundaryAtOutputY(right, backdropHeight + 4.0),
                          stripeBoundaryAtOutputY(left, backdropHeight + 4.0)},
                         stripe);
            }
        }
        SDL_SetRenderLogicalPresentation(renderer, Width, Height,
                                         SDL_LOGICAL_PRESENTATION_OVERSCAN);
        SDL_SetRenderViewport(renderer, nullptr);
        SDL_SetRenderClipRect(renderer, nullptr);
    } else {
        setColor(renderer, themeSurround);
        SDL_RenderClear(renderer);
    }
    fillRect(renderer, 0.0, 0.0, Width, Height, themeSurround);

    auto quad = [this](double left, double right, double top, double bottom) {
        return std::array<Vec2, 4>{projectPoint({left, top}), projectPoint({right, top}),
                                   projectPoint({right, bottom}), projectPoint({left, bottom})};
    };
    fillQuad(renderer, quad(CourtLeft, CourtRight, CourtTop, CourtBottom), themeSurface);
    if (surfaceProfile.drawMowingStripes) {
        const Color stripe = colorOf(surfaceProfile.mowingStripeColor);
        constexpr double stripeWidth = CourtWidth / 10.0;
        constexpr double stripeScreenTop = -4.0;
        constexpr double stripeScreenBottom = Height + 4.0;
        auto stripeBoundary = [this](double worldX, double screenY) {
            const Vec2 farPoint = projectPoint({worldX, CourtTop});
            const Vec2 nearPoint = projectPoint({worldX, CourtBottom});
            const double t = (screenY - farPoint.y) / (nearPoint.y - farPoint.y);
            return farPoint + (nearPoint - farPoint) * t;
        };
        for (int band = -32; band < 32; ++band) {
            if (band % 2 != 0) continue;
            const double left = 640.0 + band * stripeWidth;
            const double right = left + stripeWidth;
            fillQuad(renderer,
                     {stripeBoundary(left, stripeScreenTop),
                      stripeBoundary(right, stripeScreenTop),
                      stripeBoundary(right, stripeScreenBottom),
                      stripeBoundary(left, stripeScreenBottom)},
                     stripe);
        }
    }

    if (surfaceProfile.effects.wearRate > 0.0) {
        auto wearAt = [this](int column, int row) {
            column = std::clamp(column, 0, SurfaceWearColumns - 1);
            row = std::clamp(row, 0, SurfaceWearRows - 1);
            return surfaceWear[static_cast<size_t>(row * SurfaceWearColumns + column)];
        };
        auto smoothWear = [&wearAt](int column, int row) {
            return wearAt(column, row) * 0.36f +
                (wearAt(column - 1, row) + wearAt(column + 1, row) +
                 wearAt(column, row - 1) + wearAt(column, row + 1)) * 0.12f +
                (wearAt(column - 1, row - 1) + wearAt(column + 1, row - 1) +
                 wearAt(column - 1, row + 1) + wearAt(column + 1, row + 1)) * 0.04f;
        };
        std::vector<SDL_Vertex> wearVertices;
        wearVertices.reserve(SurfaceWearColumns * SurfaceWearRows);
        for (int row = 0; row < SurfaceWearRows; ++row) {
            for (int column = 0; column < SurfaceWearColumns; ++column) {
                const double xRatio = static_cast<double>(column) /
                                      (SurfaceWearColumns - 1);
                const double yRatio = static_cast<double>(row) /
                                      (SurfaceWearRows - 1);
                const Vec2 screen = projectPoint({
                    SurfaceWearLeft + xRatio * (SurfaceWearRight - SurfaceWearLeft),
                    SurfaceWearTop + yRatio * (SurfaceWearBottom - SurfaceWearTop)});
                const float wear = smoothWear(column, row);
                const float alpha = wear < 0.006f ? 0.0f : static_cast<float>(
                    surfaceProfile.effects.wearColor.a * std::pow(wear, 0.86f));
                SDL_Vertex vertex{};
                vertex.position = {static_cast<float>(screen.x),
                                   static_cast<float>(screen.y)};
                vertex.color = {surfaceProfile.effects.wearColor.r,
                                surfaceProfile.effects.wearColor.g,
                                surfaceProfile.effects.wearColor.b, alpha};
                wearVertices.push_back(vertex);
            }
        }
        std::vector<int> wearIndices;
        wearIndices.reserve((SurfaceWearColumns - 1) *
                            (SurfaceWearRows - 1) * 6);
        for (int row = 0; row < SurfaceWearRows - 1; ++row) {
            for (int column = 0; column < SurfaceWearColumns - 1; ++column) {
                const int topLeft = row * SurfaceWearColumns + column;
                const int topRight = topLeft + 1;
                const int bottomLeft = topLeft + SurfaceWearColumns;
                const int bottomRight = bottomLeft + 1;
                wearIndices.insert(wearIndices.end(),
                    {topLeft, topRight, bottomRight,
                     topLeft, bottomRight, bottomLeft});
            }
        }
        SDL_RenderGeometry(renderer, nullptr, wearVertices.data(),
                           static_cast<int>(wearVertices.size()),
                           wearIndices.data(), static_cast<int>(wearIndices.size()));
    }

    auto line = [this, renderer, themeLines](Vec2 a, Vec2 b, double width = 2.2) {
        thickLine(renderer, projectPoint(a), projectPoint(b), width, themeLines);
    };
    line({CourtLeft, CourtTop}, {CourtRight, CourtTop}, 2.0);
    line({CourtLeft, CourtBottom}, {CourtRight, CourtBottom}, 3.2);
    line({CourtLeft, CourtTop}, {CourtLeft, CourtBottom});
    line({CourtRight, CourtTop}, {CourtRight, CourtBottom});
    line({SinglesLeft, CourtTop}, {SinglesLeft, CourtBottom});
    line({SinglesRight, CourtTop}, {SinglesRight, CourtBottom});
    line({SinglesLeft, NetY - ServiceOffset}, {SinglesRight, NetY - ServiceOffset});
    line({SinglesLeft, NetY + ServiceOffset}, {SinglesRight, NetY + ServiceOffset});
    line({640, NetY - ServiceOffset}, {640, NetY + ServiceOffset});
    line({640, CourtTop}, {640, CourtTop + 8.0}, 2.0);
    line({640, CourtBottom - 8.0}, {640, CourtBottom}, 2.0);

    const Vec2 netLG = projectPoint({CourtLeft - 8, NetY});
    const Vec2 netRG = projectPoint({CourtRight + 8, NetY});
    const Vec2 netLT = projectPoint({CourtLeft - 8, NetY}, NetHeightM);
    const Vec2 netRT = projectPoint({CourtRight + 8, NetY}, NetHeightM);
    auto netMix = [](Vec2 a, Vec2 b, double t) { return a + (b - a) * t; };
    auto drawNet = [&]() {
        fillQuad(renderer, {netLT, netRT, netRG, netLG}, {0.02f, 0.035f, 0.025f, 0.24f});
        const Color netMesh{0.025f, 0.045f, 0.03f, 0.92f};
        for (int i = 0; i <= 16; ++i) {
            const double t = static_cast<double>(i) / 16.0;
            thickLine(renderer, netMix(netLT, netRT, t), netMix(netLG, netRG, t), 2.4, netMesh);
        }
        for (int i = 1; i < 5; ++i) {
            const double t = static_cast<double>(i) / 5.0;
            thickLine(renderer, netMix(netLT, netLG, t), netMix(netRT, netRG, t), 2.4, netMesh);
        }
        thickLine(renderer, netLG, netLT, 3.0, Muted);
        thickLine(renderer, netRG, netRT, 3.0, Muted);
        thickLine(renderer, netLT, netRT, 5.0, White);
        thickLine(renderer, netLG, netRG, 1.5, {0.8f, 0.8f, 0.75f, 0.35f});
    };

    const Vec2 opponentGroundScreen = projectPoint(match.opponentPosition);
    const Vec2 playerGroundScreen = projectPoint(match.playerPosition);
    const double os = perspectiveScale(match.opponentPosition.y);
    const double ps = perspectiveScale(match.playerPosition.y);
    constexpr double AvatarCenterHeightM = 0.14;
    const Vec2 opponentScreen = projectPoint(match.opponentPosition, AvatarCenterHeightM);
    double splitStepHop = 0.0;
    constexpr double splitStepHopDuration = 0.22;
    constexpr double splitStepBuffDuration = 1.35;
    if (splitStepBuffTime > splitStepBuffDuration - splitStepHopDuration) {
        const double elapsed = splitStepBuffDuration - splitStepBuffTime;
        const double phase = std::clamp(elapsed / splitStepHopDuration, 0.0, 1.0);
        splitStepHop = std::sin(phase * 3.141592653589793) * 7.0 * ps;
    }
    const Vec2 playerScreen = projectPoint(match.playerPosition, AvatarCenterHeightM) -
                              Vec2{0.0, splitStepHop};
    const Vec2 impactScreen = projectPoint(impactPosition, std::max(0.0, impactHeight - BallRadiusM));
    auto drawBall = [&]() {
        const Vec2 shadow = projectPoint(match.ball.position);
        const Vec2 ballScreen = projectPoint(match.ball.position, std::max(0.0, match.ball.height - BallRadiusM));
        const double bs = perspectiveScale(match.ball.position.y);
        const double shadowCompression = std::clamp(1.0 - match.ball.height * 0.13, 0.58, 1.0);
        pixelShadow(renderer, shadow, 6.0 * bs * shadowCompression, 2.5 * bs * shadowCompression,
                    {0, 0, 0, 0.30f});
        if (match.ball.inPlay && match.ball.velocity.length() > 80.0) {
            for (int i = 4; i >= 1; --i) {
                const double age = 0.010 * i;
                const Vec2 oldWorld = match.ball.position - match.ball.velocity * age;
                const double oldHeight = std::max(BallRadiusM, match.ball.height - match.ball.verticalVelocity * age);
                const float alpha = static_cast<float>(0.045 + (4 - i) * 0.025);
                pixelTennisBall(renderer, projectPoint(oldWorld, oldHeight - BallRadiusM),
                                (2.5 + (4 - i) * 0.35) * perspectiveScale(oldWorld.y),
                                {0.91f, 1.0f, 0.44f, alpha});
            }
        }
        pixelTennisBall(renderer, ballScreen, 5.2 * bs, BallColor);
        if (impactEffectTime > 0.0) {
            const double effectScale = 1.0 + (0.16 - impactEffectTime) * 18.0;
            const Color flash{1.0f, 0.94f, 0.55f,
                              static_cast<float>(std::clamp(impactEffectTime / 0.16, 0.0, 1.0))};
            fillRect(renderer, impactScreen.x - 8.0 * effectScale, impactScreen.y - 1.5,
                     16.0 * effectScale, 3.0, flash);
            fillRect(renderer, impactScreen.x - 1.5, impactScreen.y - 8.0 * effectScale,
                     3.0, 16.0 * effectScale, flash);
        }
    };

    auto drawSurfaceParticles = [&]() {
        const tennis::SurfaceEffectProfile &effects = surfaceProfile.effects;
        if (effects.particleKind == tennis::SurfaceParticleKind::None) return;
        for (const SurfaceParticle &particle : surfaceParticles) {
            if (particle.life <= 0.0 || particle.lifetime <= 0.0) continue;
            const double age = particle.lifetime - particle.life;
            const double fadeIn = std::clamp(age / 0.045, 0.0, 1.0);
            const double fadeOut = std::clamp(particle.life / 0.18, 0.0, 1.0);
            const float alpha = static_cast<float>(effects.particleColor.a *
                                                    fadeIn * fadeOut * 0.82);
            const float tone = static_cast<float>(particle.tone);
            const Color dust{
                std::clamp(effects.particleColor.r + tone, 0.0f, 1.0f),
                std::clamp(effects.particleColor.g + tone * 0.72f, 0.0f, 1.0f),
                std::clamp(effects.particleColor.b + tone * 0.48f, 0.0f, 1.0f),
                alpha
            };
            const Vec2 screen = projectPoint(particle.position, particle.height);
            const double size = std::max(1.0, particle.size * perspectiveScale(particle.position.y));
            if (effects.particleKind == tennis::SurfaceParticleKind::GrassClipping) {
                const bool horizontal = std::abs(particle.velocity.x) >=
                                        std::abs(particle.velocity.y);
                const double width = horizontal ? std::max(1.0, size * 1.8)
                                                : std::max(1.0, size * 0.55);
                const double height = horizontal ? std::max(1.0, size * 0.55)
                                                 : std::max(1.0, size * 1.8);
                fillRect(renderer, std::floor(screen.x - width * 0.5),
                         std::floor(screen.y - height * 0.5), width, height, dust);
            } else {
                fillRect(renderer, std::floor(screen.x - size * 0.5),
                         std::floor(screen.y - size * 0.5), size, size, dust);
            }
            if (effects.particleKind == tennis::SurfaceParticleKind::Dust && size >= 2.8) {
                fillRect(renderer, std::floor(screen.x + size * 0.35),
                         std::floor(screen.y), std::max(1.0, size * 0.45),
                         std::max(1.0, size * 0.45), dust);
            }
        }
    };

    // Far-side objects are painted before the net; near-side objects after it.
    drawSurfaceParticles();
    pixelShadow(renderer, opponentGroundScreen + Vec2{0, 9 * os}, 12 * os, 4.2 * os, {0, 0, 0, 0.30f});
    drawAvatar(renderer, opponentScreen, 10.5 * os, opponentProfile.avatar);
    if (match.ball.position.y < NetY) drawBall();
    drawNet();
    if (match.rules.matchPresentationEnabled) {
        const bool courtOnLeft = !match.endsSwapped;
        const double umpireWorldX = courtOnLeft ? CourtRight + 56.0 : CourtLeft - 56.0;
        const Vec2 umpireGround = projectPoint({umpireWorldX, NetY + 6.0});
        pixelUmpire(renderer, umpireGround, perspectiveScale(NetY), courtOnLeft);
    }
    pixelShadow(renderer, playerGroundScreen + Vec2{0, 9 * ps}, 12 * ps, 4.2 * ps, {0, 0, 0, 0.30f});
    drawAvatar(renderer, playerScreen, 10.5 * ps, playerProfile.avatar);
    if (match.ball.position.y >= NetY) drawBall();

    if (hudSettings.showScoreboard) {
    const std::string playerPoint = tennis::ScoringSystem::displayPoint(
        match.score.playerPoints, match.score.opponentPoints, match.score.mode);
    const std::string opponentPoint = tennis::ScoringSystem::displayPoint(
        match.score.opponentPoints, match.score.playerPoints, match.score.mode);
    const double scoreX = leftHudX;
    constexpr double scoreY = 590.0;
    constexpr double rowH = 28.0;
    constexpr double scoreH = rowH * 2.0;
    constexpr double nameW = 150.0;
    constexpr double setW = 28.0;
    constexpr double gamesW = 34.0;
    constexpr double pointW = 42.0;
    const Color scoreBase = colorOf(surfaceProfile.scoreboardBase);
    const Color scoreAccent = colorOf(surfaceProfile.scoreboardAccent);
    const Color scorePoint = colorOf(surfaceProfile.scoreboardPoint);
    const Color scoreInk = colorOf(surfaceProfile.scoreboardInk);
    const Color scoreText = colorOf(surfaceProfile.scoreboardText);
    const int completedSets = match.score.mode == tennis::ScoreMode::Sets
        ? std::clamp(match.score.completedSetCount, 0, 5) : 0;
    const double scoreW = nameW + (match.score.mode == tennis::ScoreMode::Sets
        ? completedSets * setW + gamesW + pointW : pointW);
    fillRect(renderer, scoreX, scoreY, scoreW, scoreH, scoreBase);
    if (match.currentServerPlayer) {
        thickLine(renderer, {scoreX + 8, scoreY + 17}, {scoreX + 14, scoreY + 11}, 1.7, scoreText);
        thickLine(renderer, {scoreX + 13, scoreY + 17}, {scoreX + 19, scoreY + 11}, 1.7, scoreText);
    } else {
        thickLine(renderer, {scoreX + 8, scoreY + rowH + 17}, {scoreX + 14, scoreY + rowH + 11}, 1.7, scoreText);
        thickLine(renderer, {scoreX + 13, scoreY + rowH + 17}, {scoreX + 19, scoreY + rowH + 11}, 1.7, scoreText);
    }
    constexpr double scoreScale = 1.08;
    auto centeredScore = [renderer](double x, double top, double width,
                                    const std::string &value, Color color) {
        constexpr double scale = scoreScale;
        const double textWidth = value.size() * 8.0 * scale;
        const double textHeight = 8.0 * scale;
        text(renderer, x + (width - textWidth) * 0.5,
             top + (rowH - textHeight) * 0.5, value, color, scale);
    };
    text(renderer, scoreX + 26, scoreY + (rowH - 8.0 * scoreScale) * 0.5,
         playerProfile.displayName, scoreText, scoreScale);
    text(renderer, scoreX + 26, scoreY + rowH + (rowH - 8.0 * scoreScale) * 0.5,
         opponentProfile.displayName, scoreText, scoreScale);

    if (match.score.mode == tennis::ScoreMode::Sets) {
        double columnX = scoreX + nameW;
        for (int setIndex = 0; setIndex < completedSets; ++setIndex) {
            if (setIndex % 2 == 1) {
                Color completedAlternate = scoreAccent;
                completedAlternate.a = 0.48f;
                fillRect(renderer, columnX, scoreY, setW, scoreH, completedAlternate);
            }
            centeredScore(columnX, scoreY, setW,
                          std::to_string(match.score.playerSetGames[setIndex]), scoreText);
            centeredScore(columnX, scoreY + rowH, setW,
                          std::to_string(match.score.opponentSetGames[setIndex]), scoreText);
            columnX += setW;
        }
        fillRect(renderer, columnX, scoreY, gamesW, scoreH, scoreAccent);
        centeredScore(columnX, scoreY, gamesW, std::to_string(match.score.playerGames), scoreText);
        centeredScore(columnX, scoreY + rowH, gamesW,
                      std::to_string(match.score.opponentGames), scoreText);
        columnX += gamesW;
        fillRect(renderer, columnX, scoreY, pointW, scoreH, scorePoint);
        centeredScore(columnX, scoreY, pointW, playerPoint, scoreInk);
        centeredScore(columnX, scoreY + rowH, pointW, opponentPoint, scoreInk);
    } else {
        const double pointX = scoreX + nameW;
        fillRect(renderer, pointX, scoreY, pointW, scoreH, scorePoint);
        centeredScore(pointX, scoreY, pointW, playerPoint, scoreInk);
        centeredScore(pointX, scoreY + rowH, pointW, opponentPoint, scoreInk);
    }
    }

    if (hudSettings.showTopLeftInfo) {
    text(renderer, leftHudX, 98, format("BALL %.0f km/h | V-SPIN %.0f",
         match.ball.velocity.length() / PixelsPerMeter * 3.6, match.ball.spinRpm), Muted, 1.10);
    text(renderer, leftHudX, 116, format("SIDE SPIN %.0f RPM", match.ball.sideSpinRpm), Muted, 1.05);
    const char *movementState = sprintingNow ? "STAMINA | SPRINT" : (movingNow ? "STAMINA | RUN" : "STAMINA | RECOVER");
    text(renderer, leftHudX, 18, movementState, White, 1.55);
    const double staminaBarX = leftHudX + 226.0;
    fillRect(renderer, staminaBarX, 13, 176, 22, {0.14f, 0.23f, 0.19f, 1});
    const double staminaRatio = std::clamp(match.playerStamina /
        std::max(1.0, playerProfile.attributes.staminaCapacity), 0.0, 1.0);
    const Color staminaColor = staminaRatio <= 0.02 ? Red :
        (staminaRatio <= staminaTuning.lowStaminaThreshold ? Yellow :
         Color{0.44f, 0.84f, 0.48f, 1});
    fillRect(renderer, staminaBarX + 3.0, 16, 170 * staminaRatio, 16, staminaColor);
    if (swingArmed) {
        const bool previewServe = !match.ball.inPlay;
        const bool previewSmash = !previewServe &&
                                  playerProfile.hasTechnique(tennis::Technique::Smash) &&
                                  match.ball.height > 1.72 &&
                                  gestureVerticalSpin() > -0.20;
        const bool previewVolley = !previewServe && !previewSmash &&
                                   match.ball.bounceCount == 0 &&
                                   !match.ball.serveReturnPending &&
                                   playerProfile.hasTechnique(tennis::Technique::Volley);
        const bool previewFront = !previewServe && !previewSmash && !previewVolley &&
                                  (match.ball.bounceCount >= 1 ||
                                   match.ball.serveReturnPending) &&
                                  match.playerPosition.y < NetY + ServiceOffset + 23.0;
        const int previewProfile = previewServe ? 1 : (previewVolley ? 2 :
            (previewFront ? 3 : (previewSmash ? 4 : 0)));
        const tennis::StrokeInputCommand previewStroke = mappedGesture(previewProfile);
        const double previewComplexity = std::clamp(
            std::abs(previewStroke.verticalSpin) * 0.30 +
            std::abs(previewStroke.sideSpin) * 0.42 +
            std::abs(previewStroke.height) * 0.28, 0.0, 1.0);
        const double predictedCost = estimatedHitStaminaCost(
            previewStroke.power, previewComplexity, swingChargeProgress(), previewSmash);
        const double predictedRatio = std::clamp(
            (match.playerStamina - predictedCost) /
            std::max(1.0, playerProfile.attributes.staminaCapacity), 0.0, 1.0);
        const double predictedX = staminaBarX + 3.0 + 170.0 * predictedRatio;
        const double currentX = staminaBarX + 3.0 + 170.0 * staminaRatio;
        fillRect(renderer, predictedX, 16, std::max(0.0, currentX - predictedX), 16,
                 {0.96f, 0.62f, 0.20f, 0.72f});
        fillRect(renderer, predictedX - 1.0, 13, 2.0, 22, White);
        text(renderer, staminaBarX + 182.0, 18, format("-%.0f", predictedCost), Yellow, 0.86);
    }
    if (!swingArmed) {
        text(renderer, leftHudX, 46, forehand ? "AUTO FOREHAND" : "AUTO BACKHAND", Yellow, 1.30);
        if (splitStepSpeedBuffActive) {
            text(renderer, leftHudX, 136, "SPLIT STEP ACTIVE", Blue, 1.15);
        } else if (splitStepReactionTime > 0.0) {
            text(renderer, leftHudX, 136, "RMB NOW!", Yellow, 1.20);
        }
    }
    }

    if (hudSettings.showContactAssist) {
    fillRect(renderer, contactAssistX, 42, contactAssistWidth, 390, {0.14f, 0.15f, 0.15f, 0.80f});
    thickLine(renderer, {contactAssistX, 42}, {contactAssistX + contactAssistWidth, 42}, 1.5, {0.68f, 0.70f, 0.68f, 0.55f});
    thickLine(renderer, {contactAssistX, 432}, {contactAssistX + contactAssistWidth, 432}, 1.5, {0.68f, 0.70f, 0.68f, 0.55f});
    thickLine(renderer, {contactAssistX, 42}, {contactAssistX, 432}, 1.5, {0.68f, 0.70f, 0.68f, 0.55f});
    thickLine(renderer, {contactAssistX + contactAssistWidth, 42}, {contactAssistX + contactAssistWidth, 432}, 1.5, {0.68f, 0.70f, 0.68f, 0.55f});
    text(renderer, contactAssistX + 12.0, 52, "CONTACT ASSIST", White, 1.35);
    const bool heightActive = match.ball.inPlay || serveTossing;
    const double assistHeightQuality = serveTossing
        ? std::clamp(1.0 - std::abs(match.ball.height - shotSpeedTuning.serveContactHeight) /
                     0.58, 0.0, 1.0)
        : currentHeightQuality();
    const Color heightColor = heightActive ? qualityColor(assistHeightQuality) : Muted;

    char heightLabel[80];
    std::snprintf(heightLabel, sizeof(heightLabel), "H %.2fm %s", match.ball.height,
                  !heightActive ? "WAIT" : (assistHeightQuality >= 0.72 ? "GOOD" : (assistHeightQuality >= 0.40 ? "ADJUST" : "BAD")));
    text(renderer, contactAssistX + 160.0, 80, heightLabel, heightColor, 0.94);
    const double heightBarX = contactAssistX + 210.0;
    constexpr double heightBarTop = 104.0;
    constexpr double heightBarHeight = 184.0;
    constexpr double heightBarBottom = heightBarTop + heightBarHeight;
    auto previewHeightQuality = [this, isServeToss = serveTossing](double height) {
        if (isServeToss) return std::clamp(
            1.0 - std::abs(height - shotSpeedTuning.serveContactHeight) / 0.58, 0.0, 1.0);
        auto zone = [](double h, double greenMin, double greenMax, double redMin, double redMax) {
            if (h >= greenMin && h <= greenMax) return 1.0;
            if (h > redMin && h < greenMin) return (h - redMin) / (greenMin - redMin);
            if (h > greenMax && h < redMax) return (redMax - h) / (redMax - greenMax);
            return 0.0;
        };
        const tennis::ContactZoneConfig &contact = playerProfile.contactZone;
        double quality = std::max(zone(height, contact.normalGreenMin, contact.normalGreenMax,
                                       contact.normalReachMin, contact.normalReachMax),
                                  zone(height, contact.smashGreenMin, contact.smashGreenMax,
                                       contact.smashReachMin, contact.smashReachMax));
        if (height > contact.normalGreenMax && height < contact.smashGreenMin)
            quality = std::max(quality, contact.difficultBandQuality);
        return std::pow(quality, 1.0 / std::max(0.25, playerProfile.attributes.contactTolerance));
    };
    constexpr int heightStrips = 184;
    const double stripHeight = heightBarHeight / heightStrips;
    for (int i = 0; i < heightStrips; ++i) {
        const double previewHeight = (i + 0.5) / heightStrips * 2.8;
        Color segmentColor = qualityColor(previewHeightQuality(previewHeight));
        segmentColor.a = 0.90f;
        fillRect(renderer, heightBarX, heightBarBottom - (i + 1) * stripHeight,
                 16, stripHeight + 0.25, segmentColor);
    }
    const double heightMarkerY = heightBarBottom - std::clamp(match.ball.height / 2.8, 0.0, 1.0) * heightBarHeight;
    fillRect(renderer, heightBarX - 5, heightMarkerY - 2, 26, 4, White);
    text(renderer, contactAssistX + 204.0, 294, "0m", Muted, 0.92);
    text(renderer, contactAssistX + 199.0, 92, "2.8m", Muted, 0.92);

    const double mapOuterX = contactAssistX + 27.0;
    constexpr double mapOuterY = 104.0;
    constexpr double mapOuterW = 120.0;
    constexpr double mapOuterH = 184.0;
    constexpr double mapWorldLeft = CourtLeft - 138.0;
    constexpr double mapWorldRight = CourtRight + 138.0;
    constexpr double mapWorldTop = CourtTop - 132.0;
    constexpr double mapWorldBottom = CourtBottom + 132.0;
    constexpr double mapScaleX = mapOuterW / (mapWorldRight - mapWorldLeft);
    constexpr double mapScaleY = mapOuterH / (mapWorldBottom - mapWorldTop);
    const double mapCourtX = mapOuterX + (CourtLeft - mapWorldLeft) * mapScaleX;
    constexpr double mapCourtY = mapOuterY + (CourtTop - mapWorldTop) * mapScaleY;
    constexpr double mapCourtW = CourtWidth * mapScaleX;
    constexpr double mapCourtH = CourtHeight * mapScaleY;
    auto mapPoint = [=](Vec2 world) {
        return Vec2{mapOuterX + (world.x - mapWorldLeft) * mapScaleX,
                    mapOuterY + (world.y - mapWorldTop) * mapScaleY};
    };
    fillRect(renderer, mapOuterX, mapOuterY, mapOuterW, mapOuterH, themeSurround);
    fillRect(renderer, mapCourtX, mapCourtY, mapCourtW, mapCourtH, themeSurface);
    if (surfaceProfile.drawMowingStripes) {
        const Color stripe = colorOf(surfaceProfile.mowingStripeColor);
        constexpr int miniStripes = 10;
        const double miniStripeWidth = mapOuterW / miniStripes;
        for (int stripeIndex = 0; stripeIndex < miniStripes; stripeIndex += 2) {
            fillRect(renderer, mapOuterX + stripeIndex * miniStripeWidth, mapOuterY,
                     miniStripeWidth, mapOuterH, stripe);
        }
    }
    auto mapLine = [renderer, &mapPoint, themeLines](Vec2 a, Vec2 b, double width = 1.2) {
        thickLine(renderer, mapPoint(a), mapPoint(b), width, themeLines);
    };
    mapLine({CourtLeft, CourtTop}, {CourtRight, CourtTop});
    mapLine({CourtLeft, CourtBottom}, {CourtRight, CourtBottom});
    mapLine({CourtLeft, CourtTop}, {CourtLeft, CourtBottom});
    mapLine({CourtRight, CourtTop}, {CourtRight, CourtBottom});
    mapLine({SinglesLeft, CourtTop}, {SinglesLeft, CourtBottom});
    mapLine({SinglesRight, CourtTop}, {SinglesRight, CourtBottom});
    mapLine({SinglesLeft, NetY - ServiceOffset}, {SinglesRight, NetY - ServiceOffset});
    mapLine({SinglesLeft, NetY + ServiceOffset}, {SinglesRight, NetY + ServiceOffset});
    mapLine({640, NetY - ServiceOffset}, {640, NetY + ServiceOffset});
    mapLine({640, CourtTop}, {640, CourtTop + 8.0});
    mapLine({640, CourtBottom - 8.0}, {640, CourtBottom});
    mapLine({CourtLeft, NetY}, {CourtRight, NetY}, 2.0);

    const Vec2 mapPlayer = mapPoint(match.playerPosition);
    const tennis::ContactZoneConfig &contact = playerProfile.contactZone;
    for (double side : {-1.0, 1.0}) {
        const double zoneCenterX = mapPlayer.x + side * contact.idealLateralOffset * mapScaleX;
        const double zoneCenterY = mapPlayer.y + contact.idealForwardOffset * mapScaleY;
        const double tolerance = std::max(0.25, playerProfile.attributes.contactTolerance);
        fillRect(renderer, zoneCenterX - contact.lateralGreenRadius * contact.unreachableEllipse * tolerance * mapScaleX,
                 zoneCenterY - contact.forwardGreenRadius * contact.unreachableEllipse * tolerance * mapScaleY,
                 contact.lateralGreenRadius * contact.unreachableEllipse * tolerance * 2.0 * mapScaleX,
                 contact.forwardGreenRadius * contact.unreachableEllipse * tolerance * 2.0 * mapScaleY,
                 {0.58f, 0.39f, 0.07f, 0.82f});
        fillRect(renderer, zoneCenterX - contact.lateralGreenRadius * mapScaleX,
                 zoneCenterY - contact.forwardGreenRadius * mapScaleY,
                 contact.lateralGreenRadius * 2.0 * mapScaleX,
                 contact.forwardGreenRadius * 2.0 * mapScaleY,
                 {0.12f, 0.58f, 0.20f, 0.96f});
    }
    drawAvatar(renderer, mapPoint(match.opponentPosition), 4.8, opponentProfile.avatar);
    drawAvatar(renderer, mapPlayer, 4.8, playerProfile.avatar);
    if (match.ball.inPlay) {
        Vec2 mapBall = mapPoint(match.ball.position);
        mapBall.x = std::clamp(mapBall.x, mapOuterX + 4.0, mapOuterX + mapOuterW - 4.0);
        mapBall.y = std::clamp(mapBall.y, mapOuterY + 4.0, mapOuterY + mapOuterH - 4.0);
        pixelTennisBall(renderer, mapBall, 3.8, heightColor, false);
    }

    const double chargeProgress = swingArmed ? swingChargeProgress() : 0.0;
    const Color chargeColor = !swingArmed ? Muted :
        (chargeProgress >= 0.72 ? Color{0.42f, 0.82f, 0.48f, 1.0f} :
         (chargeProgress >= 0.35 ? Yellow : Blue));
    const std::string chargeLabel = !swingArmed ? "CHARGE" :
        (swingChargeLocked ? format("POWER LOCK %.0f%%", chargeProgress * 100.0) :
         (sprintingNow ? "CHARGE LOCK" : format("CHARGE %.0f%%", chargeProgress * 100.0)));
    text(renderer, contactAssistX + 12.0, 304, chargeLabel,
         swingArmed ? chargeColor : Muted, 0.88);
    fillRect(renderer, contactAssistX + 118.0, 304, 130, 12, {0.20f, 0.21f, 0.20f, 0.90f});
    fillRect(renderer, contactAssistX + 120.0, 306, 126 * chargeProgress, 8, chargeColor);

    thickLine(renderer, {contactAssistX + 12.0, 328}, {contactAssistX + 248.0, 328}, 1.0, {0.68f, 0.70f, 0.68f, 0.35f});
    text(renderer, contactAssistX + 12.0, 338, "RECENT SHOT", Muted, 0.98);
    if (!shotHistory.empty()) {
        const ShotRecord &record = shotHistory.front();
        fillRect(renderer, contactAssistX + 12.0, 360, 72, 56, {0.075f, 0.105f, 0.095f, 0.94f});
        const Color patternColor = strokeProfileColor(record.mappingProfile);
        for (size_t i = 1; i < record.pattern.size(); ++i) {
            const Vec2 a{contactAssistX + 48.0 + record.pattern[i - 1].x * 58.0,
                         388.0 + record.pattern[i - 1].y * 44.0};
            const Vec2 b{contactAssistX + 48.0 + record.pattern[i].x * 58.0,
                         388.0 + record.pattern[i].y * 44.0};
            thickLine(renderer, a, b, 2.0, patternColor);
        }
        text(renderer, contactAssistX + 94.0, 356, record.shot, patternColor, 0.92);
        text(renderer, contactAssistX + 94.0, 375,
             format("PWR %.0f | H%+.0f", record.power * 100.0, record.loft * 100.0),
             White, 0.84);
        text(renderer, contactAssistX + 94.0, 393,
             format("V%+.0f | S%+.0f", record.verticalSpinRpm, record.sideSpinRpm),
             Muted, 0.78);
        text(renderer, contactAssistX + 94.0, 410,
             format("ACC %.0f | STA %.0f", record.accuracy * 100.0, record.stamina),
             Muted, 0.78);
    } else {
        text(renderer, contactAssistX + 12.0, 370, "NO SHOT YET", Muted, 0.92);
    }
    }

    if (swingArmed) {
        const double power = gesturePower();
        const double vertical = gestureVerticalSpin();
        const double side = gestureSideCurve();
        const double loft = gestureLoft();
        const double smooth = gestureSmoothness();
        const double rpm = vertical >= 0.0 ? vertical * std::sqrt(power) * 3600.0 : vertical * std::sqrt(power) * 2700.0;
        const bool gestureServe = !match.ball.inPlay;
        const bool gestureSmash = !gestureServe &&
                                  playerProfile.hasTechnique(tennis::Technique::Smash) &&
                                  match.ball.height > 1.72 &&
                                  vertical > -0.20;
        const bool gestureVolley = !gestureServe && !gestureSmash &&
                                   match.ball.bounceCount == 0 &&
                                   !match.ball.serveReturnPending &&
                                   playerProfile.hasTechnique(tennis::Technique::Volley);
        const bool gestureFront = !gestureServe && !gestureSmash && !gestureVolley &&
                                  (match.ball.bounceCount >= 1 ||
                                   match.ball.serveReturnPending) &&
                                  match.playerPosition.y < NetY + ServiceOffset + 23.0;
        const int gestureProfile = gestureServe ? 1 : (gestureVolley ? 2 :
            (gestureFront ? 3 : (gestureSmash ? 4 : 0)));
        const Color gestureColor = strokeProfileColor(gestureProfile);
        for (int i = 1; i < gesturePointCount; ++i) thickLine(renderer, gesturePoints[i - 1], gesturePoints[i], 4.0, gestureColor);
        circle(renderer, dragCurrent, 7, gestureColor);
        if (hudSettings.showTopLeftInfo) {
            const char *shot = std::abs(vertical) < 0.08 ? "FLAT" : (vertical > 0.0 ? "TOPSPIN" : "SLICE");
            text(renderer, leftHudX, 136, std::string(shot) + format(" | POWER %.0f%%", power * 100), gestureColor, 1.55);
            text(renderer, leftHudX, 156, format("V-SPIN %.0f | SIDE %.0f%%", rpm, side * 100), White, 1.35);
            text(renderer, leftHudX, 174, format("HEIGHT %.0f%% | SMOOTH %.0f%%", loft * 100, smooth * 100), White, 1.30);
        }
    } else if (hudSettings.showTopLeftInfo && (statusTime > 0.0 || !match.ball.inPlay)) {
        constexpr double statusScale = 1.35;
        text(renderer, leftHudX, 70, statusMessage, Yellow, statusScale);
    }

    if (paused) renderPauseOverlay(renderer);

    // Some backends clear the physical letterbox pixels again while resolving
    // the logical target. Redraw only the outside bands as the final pass so
    // fullscreen grass stripes reach the last row without covering the HUD.
    if (hasPhysicalBackdrop && surfaceProfile.drawMowingStripes) {
        SDL_SetRenderLogicalPresentation(renderer, 0, 0,
                                         SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderViewport(renderer, nullptr);
        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        const Color stripe = colorOf(surfaceProfile.mowingStripeColor);
        constexpr double stripeWidth = CourtWidth / 10.0;
        auto toOutput = [&](Vec2 logical) {
            return Vec2{
                presentationRect.x + logical.x * presentationRect.w / Width,
                presentationRect.y + logical.y * presentationRect.h / Height
            };
        };
        auto stripeBoundaryAtOutputY = [&](double worldX, double outputY) {
            const Vec2 farPoint = toOutput(projectPoint({worldX, CourtTop}));
            const Vec2 nearPoint = toOutput(projectPoint({worldX, CourtBottom}));
            const double t = (outputY - farPoint.y) /
                             std::max(1e-6, nearPoint.y - farPoint.y);
            return farPoint + (nearPoint - farPoint) * t;
        };
        auto drawOutsideBand = [&](const SDL_Rect &clip) {
            if (clip.w <= 0 || clip.h <= 0) return;
            SDL_SetRenderClipRect(renderer, &clip);
            for (int band = -64; band < 64; band += 2) {
                const double left = 640.0 + band * stripeWidth;
                const double right = left + stripeWidth;
                fillQuad(renderer,
                         {stripeBoundaryAtOutputY(left, -4.0),
                          stripeBoundaryAtOutputY(right, -4.0),
                          stripeBoundaryAtOutputY(right, backdropHeight + 4.0),
                          stripeBoundaryAtOutputY(left, backdropHeight + 4.0)},
                         stripe);
            }
        };

        const int logicalTop = std::clamp(static_cast<int>(std::floor(presentationRect.y)),
                                          0, backdropHeight);
        const int logicalBottom = std::clamp(static_cast<int>(std::ceil(
            presentationRect.y + presentationRect.h)), 0, backdropHeight);
        const int logicalLeft = std::clamp(static_cast<int>(std::floor(presentationRect.x)),
                                           0, backdropWidth);
        const int logicalRight = std::clamp(static_cast<int>(std::ceil(
            presentationRect.x + presentationRect.w)), 0, backdropWidth);
        drawOutsideBand({0, 0, backdropWidth, logicalTop});
        // Overlap the resolved logical edge: high-DPI Metal backends can round
        // the final row inward, which otherwise leaves a solid unstriped band.
        // During pause the logical overlay already covers every visible row.
        // Do not repaint the overlap strip afterward, or the last rows become
        // bright grass again beneath the otherwise uniform grey veil.
        const int bottomStripeTop = paused
            ? logicalBottom
            : std::max(logicalTop, logicalBottom - 12);
        drawOutsideBand({0, bottomStripeTop, backdropWidth,
                         backdropHeight - bottomStripeTop});
        drawOutsideBand({0, logicalTop, logicalLeft, logicalBottom - logicalTop});
        drawOutsideBand({logicalRight, logicalTop, backdropWidth - logicalRight,
                         logicalBottom - logicalTop});

        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderLogicalPresentation(renderer, Width, Height,
                                         SDL_LOGICAL_PRESENTATION_OVERSCAN);
        SDL_SetRenderViewport(renderer, nullptr);
        SDL_SetRenderClipRect(renderer, nullptr);
    }
    SDL_RenderPresent(renderer);
}

void Game::renderPauseOverlay(SDL_Renderer *renderer) const {
    fillRect(renderer, 0, 0, Width, Height, {0.035f, 0.045f, 0.042f, 0.62f});
    constexpr double panelX = 380.0;
    constexpr double panelY = 80.0;
    constexpr double panelW = 520.0;
    constexpr double panelH = 560.0;
    fillRect(renderer, panelX, panelY, panelW, panelH, {0.14f, 0.15f, 0.15f, 0.94f});
    thickLine(renderer, {panelX, panelY}, {panelX + panelW, panelY}, 1.5, {0.72f, 0.73f, 0.69f, 0.62f});
    thickLine(renderer, {panelX, panelY + panelH}, {panelX + panelW, panelY + panelH}, 1.5,
              {0.72f, 0.73f, 0.69f, 0.62f});
    thickLine(renderer, {panelX, panelY}, {panelX, panelY + panelH}, 1.5, {0.72f, 0.73f, 0.69f, 0.62f});
    thickLine(renderer, {panelX + panelW, panelY}, {panelX + panelW, panelY + panelH}, 1.5,
              {0.72f, 0.73f, 0.69f, 0.62f});

    auto hovered = [this](double x, double y, double width, double height) {
        return pauseMouse.x >= x && pauseMouse.x <= x + width &&
               pauseMouse.y >= y && pauseMouse.y <= y + height;
    };
    auto optionText = [&](double x, double y, double width, double height,
                          const std::string &value, bool selected = false, double scale = 1.28) {
        const Color color = hovered(x, y, width, height) ? Yellow :
                            (selected ? Color{0.42f, 0.86f, 0.52f, 1.0f} : Muted);
        text(renderer, x, y + height * 0.5 - 6.0, value, color, scale);
    };
    auto centeredMainText = [&](double y, const std::string &value) {
        constexpr double hitX = 500.0;
        constexpr double hitW = 280.0;
        constexpr double height = 44.0;
        constexpr double scale = 1.42;
        const Color color = hovered(hitX, y, hitW, height) ? Yellow : Muted;
        const double glyphWidth = 8.0 * scale;
        const double x = panelX + (panelW - glyphWidth * value.size()) * 0.5;
        text(renderer, x, y + height * 0.5 - 6.0, value, color, scale);
    };
    auto selectorText = [&](double x, double y, double width, double height,
                            const std::string &value, double scale = 1.18) {
        constexpr double arrowWidth = 42.0;
        constexpr double arrowScale = 1.42;
        const double arrowGlyphWidth = 8.0 * arrowScale;
        const Color leftColor = hovered(x, y, arrowWidth, height) ? Yellow : Muted;
        const Color rightColor = hovered(x + width - arrowWidth, y,
                                         arrowWidth, height) ? Yellow : Muted;
        text(renderer, std::round(x + (arrowWidth - arrowGlyphWidth) * 0.5),
             y + height * 0.5 - 6.0, "<", leftColor, arrowScale);
        text(renderer, std::round(x + width - arrowWidth +
                                  (arrowWidth - arrowGlyphWidth) * 0.5),
             y + height * 0.5 - 6.0, ">", rightColor, arrowScale);
        const double centerLeft = x + arrowWidth;
        const double centerWidth = width - arrowWidth * 2.0;
        const double visualTextWidth = std::max(
            0.0, (static_cast<double>(value.size()) * 8.0 - 1.0) * scale);
        const double valueX = std::round(
            centerLeft + (centerWidth - visualTextWidth) * 0.5);
        text(renderer, valueX, y + height * 0.5 - 6.0, value, White, scale);
    };

    if (pausePage == PausePage::Main)
        text(renderer, 402, 104, "PAUSED", White, 2.15);
    else optionText(400, 96, pausePage == PausePage::MappingSettings ? 150 : 100, 36,
                    pausePage == PausePage::MappingSettings ? "< CONTROLS" : "< BACK");
    optionText(785, 96, 95, 36, "RESUME", true, 1.08);

    if (pausePage == PausePage::Main) {
        centeredMainText(210, "CONTROLS");
        centeredMainText(270, "RULES");
        centeredMainText(330, "SCORING");
        centeredMainText(390, "STYLE");
        centeredMainText(450, "QUIT");
        return;
    }

    if (pausePage == PausePage::SwingGuide) {
        const tennis::StrokeInputCommand mapped = mappedGesture(selectedMappingProfile);
        static constexpr const char *profileNames[] = {"GROUNDSTROKE", "SERVE", "VOLLEY", "FRONT COURT", "SMASH"};
        text(renderer, 402, 152, "CONTROLS + SWING TEST", Yellow, 1.38);
        optionText(690, 150, 180, 36, "MAPPING SETTINGS", true, 1.05);
        text(renderer, 402, 184, "WASD MOVE | SHIFT SPRINT | RMB TOSS/SPLIT", Muted, 1.02);
        text(renderer, 402, 206, "HOLD LMB TO CHARGE AND DRAW ANYWHERE", White, 1.12);
        text(renderer, 402, 230, std::string("TEST PROFILE: ") + profileNames[selectedMappingProfile],
             strokeProfileColor(selectedMappingProfile), 1.05);
        text(renderer, 402, 265, "HOLD TIME = POWER", Muted, 1.02);
        text(renderer, 402, 287, "UP/DOWN BEND = V-SPIN | SIDE BEND = SIDE SPIN", Muted, 1.02);
        text(renderer, 402, 309, "STRAIGHTER PATH = MORE FLAT DRIVE", Muted, 1.02);
        text(renderer, 402, 331, "ENDPOINT VECTOR = AIM-X + LAUNCH HEIGHT", Muted, 1.02);
        text(renderer, 402, 360, "AUTO HIT", Muted, 1.08);
        optionText(690, 350, 180, 34,
                   controlSettings.autoHit ? "ON" : "OFF",
                   controlSettings.autoHit, 1.05);
        text(renderer, 402, 405, "STILL 100% | MOVE 45% | SPRINT LOCKED", Muted, 0.94);
        text(renderer, 402, 429, "SERVE AUTO: HIT WHEN LEAVING GREEN", Yellow, 0.91);
        const double power = mapped.power;
        const double vertical = mapped.verticalSpin;
        const double side = mapped.sideSpin;
        const double loft = mapped.height;
        const double smooth = mapped.smoothness;
        const double lateralAimPixels = mapped.lateralAimPixels;
        const double flatDrive = mapped.flatDrive;
        const double verticalRpm = vertical >= 0.0 ? vertical * std::sqrt(power) * 3600.0
                                                   : vertical * std::sqrt(power) * 2700.0;
        const double sideRpm = side * std::sqrt(power) * 2300.0;
        char first[128];
        char second[128];
        char third[128];
        std::snprintf(first, sizeof(first), "POWER %.0f%%   AIM-X %+.0f PX   AIM-H %.0f%%",
                      power * 100.0, lateralAimPixels, loft * 100.0);
        std::snprintf(second, sizeof(second), "V-SPIN %+.0f RPM   SIDE %+.0f RPM", verticalRpm, sideRpm);
        std::snprintf(third, sizeof(third), "FLAT DRIVE %.0f%%   SMOOTH %.0f%%", flatDrive * 100.0, smooth * 100.0);
        fillRect(renderer, 400, 466, 480, 100, {0.075f, 0.085f, 0.082f, 0.88f});
        text(renderer, 416, 486, first, White, 1.20);
        text(renderer, 416, 512, second, White, 1.20);
        text(renderer, 416, 538, third, White, 1.20);
        // Keep the live gesture visible above the data panel instead of
        // allowing the opaque panel to cut the path into disconnected pieces.
        if (gesturePointCount >= 2) {
            const Color trace = strokeProfileColor(selectedMappingProfile);
            for (int i = 1; i < gesturePointCount; ++i) {
                thickLine(renderer, gesturePoints[i - 1], gesturePoints[i], 4.0, trace);
            }
            pixelTennisBall(renderer, dragCurrent, 6.0, trace, false);
        }
    } else if (pausePage == PausePage::MappingSettings) {
        static constexpr const char *profileNames[] = {"GROUNDSTROKE", "SERVE", "VOLLEY", "FRONT COURT", "SMASH"};
        const MappingProfile &profile = mappingProfiles[selectedMappingProfile];
        text(renderer, 402, 145, "MAPPING SETTINGS", Yellow, 1.55);
        text(renderer, 402, 168, "EACH SHOT TYPE KEEPS ITS OWN RANGE", Muted, 1.02);
        text(renderer, 402, 204, "SHOT PROFILE", Muted, 1.18);
        optionText(620, 195, 240, 30, std::string("<  ") + profileNames[selectedMappingProfile] + "  >", true, 1.10);

        auto ratioRow = [&](double y, const char *label, double value, bool percentage = false) {
            char ratio[48];
            if (percentage) std::snprintf(ratio, sizeof(ratio), "-    %+.0f%%    +", value * 100.0);
            else std::snprintf(ratio, sizeof(ratio), "-    %.2fX    +", value);
            text(renderer, 402, y + 7, label, Muted, 1.02);
            optionText(620, y, 240, 30, ratio, false, 1.06);
        };
        ratioRow(235, "POWER RESPONSE", profile.power);
        ratioRow(270, "POWER MIN", profile.powerMin, true);
        ratioRow(305, "POWER MAX", profile.powerMax, true);
        ratioRow(340, "VERTICAL SPIN", profile.verticalSpin);
        ratioRow(375, "SIDE SPIN", profile.sideSpin);
        ratioRow(410, "FLAT DRIVE SHARE", profile.flatDrive);
        ratioRow(445, "LEFT/RIGHT AIM", profile.direction);
        ratioRow(480, "HEIGHT RESPONSE", profile.height);
        ratioRow(515, "AIM HEIGHT MIN", profile.heightMin, true);
        ratioRow(550, "AIM HEIGHT MAX", profile.heightMax, true);
        optionText(620, 590, 240, 30, "RESET THIS PROFILE", false, 1.02);
        text(renderer, 402, 612, "DOWNWARD RANGE OPENS CONTINUOUSLY NEAR THE NET", Muted, 0.84);
    } else if (pausePage == PausePage::Rules) {
        text(renderer, 402, 145, "RULES", Yellow, 1.55);
        text(renderer, 402, 164, match.rules.customRules ? "PRESET: CUSTOM" : "PRESET: MATCH", Muted, 1.05);
        optionText(620, 155, 230, 36, "APPLY MATCH PRESET", !match.rules.customRules);
        text(renderer, 402, 213, "PLAY MODE", Muted, 1.25);
        optionText(620, 205, 230, 36, match.rules.matchPresentationEnabled ? "MATCH / OFFICIALS" : "PRACTICE / CLEAN", match.rules.matchPresentationEnabled);
        text(renderer, 402, 273, "SERVE RULES", Muted, 1.25);
        optionText(620, 265, 230, 36, match.rules.serveRulesEnabled ? "ON / TWO SERVES" : "OFF / FREE SERVE", match.rules.serveRulesEnabled);
        text(renderer, 402, 333, "BOUNDARIES", Muted, 1.25);
        const char *boundary = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? "SINGLES" :
                               (match.rules.boundaryMode == tennis::BoundaryMode::Doubles ? "DOUBLES" : "NO OUT");
        optionText(620, 325, 230, 36, boundary, match.rules.boundaryMode != tennis::BoundaryMode::None);
        text(renderer, 402, 393, "BOUNCE RULE", Muted, 1.25);
        optionText(620, 385, 230, 36, match.rules.multiBounceAllowed ? "MULTI / UNTIL DEAD" : "STANDARD / TWO", match.rules.multiBounceAllowed);
        text(renderer, 402, 453, "SERVER", Muted, 1.25);
        const char *server = match.rules.serveMode == tennis::ServeMode::Alternate ? "ALTERNATE" :
                             (match.rules.serveMode == tennis::ServeMode::Player ? "PLAYER ONLY" : "COMPUTER ONLY");
        optionText(620, 445, 230, 36, server, true);
        text(renderer, 402, 513, "AI DIFFICULTY", Muted, 1.25);
        const tennis::AiDifficultyProfile difficulty = tennis::aiDifficultyProfile(aiDifficulty);
        selectorText(620, 505, 230, 36, difficulty.name, 1.14);
        text(renderer, 402, 570, "MATCH: UMPIRE CHANGES SCREEN SIDE", Muted, 0.90);
        text(renderer, 402, 594, "SERVE OFF: RALLY STARTS AFTER NET CROSS", Muted, 0.90);
    } else if (pausePage == PausePage::Scoring) {
        text(renderer, 402, 145, "SCORING (INDEPENDENT FROM RULES)", Yellow, 1.45);
        text(renderer, 402, 228, "SYSTEM", Muted, 1.25);
        const char *scoring = match.score.mode == tennis::ScoreMode::Unlimited ? "UNLIMITED" :
                              (match.score.mode == tennis::ScoreMode::Race ? "RACE" : "SETS");
        optionText(620, 220, 230, 36, scoring, true);
        text(renderer, 402, 288, "RACE TARGET", Muted, 1.25);
        optionText(620, 280, 230, 36, std::to_string(match.score.raceTarget) + " POINTS", match.score.mode == tennis::ScoreMode::Race);
        text(renderer, 402, 348, "MATCH LENGTH", Muted, 1.25);
        optionText(620, 340, 230, 36, "BEST OF " + std::to_string(match.score.bestOfSets), match.score.mode == tennis::ScoreMode::Sets);
        text(renderer, 402, 410, "POINTS: 0 / 15 / 30 / 40 / ADVANTAGE", Muted, 1.06);
        text(renderer, 402, 434, "SET: FIRST TO 6 GAMES, WIN BY 2", Muted, 1.06);
        text(renderer, 402, 466, "RULE TOGGLES NEVER CHANGE THE SCORE SYSTEM", Yellow, 1.02);
    } else if (pausePage == PausePage::Style) {
        const tennis::CourtSurfaceProfile &surface =
            tennis::courtSurfaceProfile(activeCourtPreset);
        text(renderer, 402, 145, "STYLE", Yellow, 1.55);
        text(renderer, 402, 178, "SCOREBOARD", Muted, 1.18);
        optionText(650, 168, 200, 34,
                   hudSettings.showScoreboard ? "ON" : "OFF",
                   hudSettings.showScoreboard);
        text(renderer, 402, 238, "CONTACT ASSIST", Muted, 1.18);
        optionText(650, 228, 200, 34,
                   hudSettings.showContactAssist ? "ON" : "OFF",
                   hudSettings.showContactAssist);
        text(renderer, 402, 298, "TOP-LEFT INFO", Muted, 1.18);
        optionText(650, 288, 200, 34,
                   hudSettings.showTopLeftInfo ? "ON" : "OFF",
                   hudSettings.showTopLeftInfo);

        text(renderer, 402, 378, "COURT SURFACE", Muted, 1.18);
        selectorText(620, 366, 230, 38, std::string(surface.name), 1.10);
        if (surface.material == tennis::CourtMaterial::Clay) {
            text(renderer, 402, 432, "CLAY | HIGHER, SLOWER BOUNCE", White, 1.01);
            text(renderer, 402, 456, "LOWER TRACTION | LONGER BRAKING SLIDE", Muted, 1.00);
        } else if (surface.material == tennis::CourtMaterial::Grass) {
            text(renderer, 402, 432, "GRASS | LOW, FAST SKIDDING BOUNCE", White, 1.00);
            text(renderer, 402, 456, "LIGHT TRACTION | QUICK COURT PACE", Muted, 1.00);
        } else if (surface.preset == tennis::CourtPreset::BouncyHard) {
            text(renderer, 402, 432, "BOUNCY HARD | HIGH, TRUE REBOUND", White, 1.00);
            text(renderer, 402, 456, "FIRM TRACTION | MODERATE COURT PACE", Muted, 1.00);
        } else {
            text(renderer, 402, 432, "FAST HARD | LOWER, QUICK REBOUND", White, 1.00);
            text(renderer, 402, 456, "FIRM TRACTION | HIGH COURT PACE", Muted, 1.00);
        }
        text(renderer, 402, 510, "SURFACE CHANGES BOTH PLAYERS AND BALL BOUNCES", Yellow, 0.96);
        text(renderer, 402, 534, "AIR DRAG, GRAVITY AND MAGNUS SPIN STAY PHYSICAL", Muted, 0.91);
    }
}
