#include "render_primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace tennis::render {

Color qualityColor(double quality) {
    if (quality >= 0.72) return {0.38f, 0.91f, 0.48f, 1.0f};
    if (quality >= 0.40) return {1.0f, 0.78f, 0.27f, 1.0f};
    return {0.95f, 0.30f, 0.28f, 1.0f};
}

namespace {

SDL_FColor sdlColor(Color color) {
    return {color.r, color.g, color.b, color.a};
}

} // namespace

void setColor(SDL_Renderer *renderer, Color color) {
    SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, color.a);
}

void fillQuad(SDL_Renderer *renderer, const std::array<Vec2, 4> &points, Color color) {
    SDL_Vertex vertices[4]{};
    for (int i = 0; i < 4; ++i) {
        vertices[i].position = {static_cast<float>(points[i].x), static_cast<float>(points[i].y)};
        vertices[i].color = sdlColor(color);
    }
    const int indices[] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
}

void thickLine(SDL_Renderer *renderer, Vec2 a, Vec2 b, double width, Color color) {
    const Vec2 delta = b - a;
    const double length = delta.length();
    if (length < 1e-6) return;
    const Vec2 normal{-delta.y / length * width * 0.5, delta.x / length * width * 0.5};
    fillQuad(renderer, {a + normal, b + normal, b - normal, a - normal}, color);
}

void circle(SDL_Renderer *renderer, Vec2 center, double radius, Color color, int segments) {
    std::vector<SDL_Vertex> vertices(static_cast<size_t>(segments) + 1);
    vertices[0].position = {static_cast<float>(center.x), static_cast<float>(center.y)};
    vertices[0].color = sdlColor(color);
    for (int i = 0; i < segments; ++i) {
        const double angle = static_cast<double>(i) / segments * 6.283185307179586;
        vertices[static_cast<size_t>(i) + 1].position = {
            static_cast<float>(center.x + std::cos(angle) * radius),
            static_cast<float>(center.y + std::sin(angle) * radius)
        };
        vertices[static_cast<size_t>(i) + 1].color = sdlColor(color);
    }
    std::vector<int> indices;
    indices.reserve(static_cast<size_t>(segments) * 3);
    for (int i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back((i + 1) % segments + 1);
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void fillRect(SDL_Renderer *renderer, double x, double y, double width, double height, Color color) {
    setColor(renderer, color);
    const SDL_FRect rect{static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(width), static_cast<float>(height)};
    SDL_RenderFillRect(renderer, &rect);
}

void pixelShadow(SDL_Renderer *renderer, Vec2 center, double radiusX, double radiusY, Color color) {
    fillRect(renderer, center.x - radiusX * 0.58, center.y - radiusY,
             radiusX * 1.16, radiusY * 0.34, color);
    fillRect(renderer, center.x - radiusX * 0.86, center.y - radiusY * 0.66,
             radiusX * 1.72, radiusY * 1.32, color);
    fillRect(renderer, center.x - radiusX * 0.58, center.y + radiusY * 0.66,
             radiusX * 1.16, radiusY * 0.34, color);
}

void pixelTennisBall(SDL_Renderer *renderer, Vec2 center, double radius, Color color, bool drawSeam) {
    const double r = std::max(2.0, radius);
    fillRect(renderer, center.x - r * 0.38, center.y - r, r * 0.76, r * 0.16, color);
    fillRect(renderer, center.x - r * 0.72, center.y - r * 0.84, r * 1.44, r * 0.24, color);
    fillRect(renderer, center.x - r * 0.90, center.y - r * 0.60, r * 1.80, r * 1.20, color);
    fillRect(renderer, center.x - r * 0.72, center.y + r * 0.60, r * 1.44, r * 0.24, color);
    fillRect(renderer, center.x - r * 0.38, center.y + r * 0.84, r * 0.76, r * 0.16, color);
    if (drawSeam) {
        const Color seam{0.96f, 0.97f, 0.78f, 0.92f};
        fillRect(renderer, center.x - r * 0.72, center.y - r * 0.18,
                 r * 0.38, std::max(1.0, r * 0.18), seam);
        fillRect(renderer, center.x + r * 0.34, center.y + r * 0.04,
                 r * 0.38, std::max(1.0, r * 0.18), seam);
    }
}

void pixelUmpire(SDL_Renderer *renderer, Vec2 ground, double scale, bool courtOnLeft) {
    const Color frame{0.76f, 0.78f, 0.70f, 1.0f};
    const Color frameShade{0.39f, 0.43f, 0.39f, 1.0f};
    const Color seat{0.10f, 0.20f, 0.24f, 1.0f};
    const Color uniform{0.13f, 0.26f, 0.43f, 1.0f};
    const double thick = 4.0 * scale;
    const double seatY = ground.y - 48.0 * scale;
    const double towardCourt = courtOnLeft ? -1.0 : 1.0;

    pixelShadow(renderer, ground + Vec2{0, 2.0 * scale}, 22.0 * scale, 4.0 * scale,
                {0, 0, 0, 0.28f});

    // One forward leg carries short grey step marks instead of a separate ladder.
    const Vec2 frontGround{ground.x + towardCourt * 24.0 * scale, ground.y};
    const Vec2 frontSeat{ground.x + towardCourt * 8.0 * scale, seatY};
    thickLine(renderer, frontGround, frontSeat, thick, frame);
    for (int step = 1; step <= 4; ++step) {
        const double t = static_cast<double>(step) / 5.0;
        const Vec2 center = frontGround + (frontSeat - frontGround) * t;
        thickLine(renderer, center + Vec2{towardCourt * 1.0 * scale, 0},
                  center - Vec2{towardCourt * 8.0 * scale, 0},
                  3.0 * scale, frameShade);
    }
    const Vec2 rearGround{ground.x - towardCourt * 15.0 * scale, ground.y};
    const Vec2 rearSeat{ground.x - towardCourt * 6.0 * scale, seatY};
    thickLine(renderer, rearGround, rearSeat, thick, frame);
    thickLine(renderer, {ground.x + towardCourt * 12.0 * scale, seatY},
              {ground.x - towardCourt * 16.0 * scale, seatY}, 6.0 * scale, frame);
    // Overlapping rectangular steps form a deliberately pixelated rearward lean.
    for (int segment = 0; segment < 5; ++segment) {
        const double x = ground.x - towardCourt * (15.0 + segment * 1.5) * scale;
        const double y = seatY - (3.0 + segment * 5.2) * scale;
        fillRect(renderer, x - 4.0 * scale, y, 8.0 * scale, 7.0 * scale, seat);
    }

    // The umpire uses exactly the same base radius as a player at this depth.
    const Vec2 umpireCenter{ground.x, seatY - 15.0 * scale};
    const double umpireRadius = 10.5 * scale;
    pixelTennisBall(renderer, umpireCenter, umpireRadius, uniform, false);
}

void text(SDL_Renderer *renderer, double x, double y, const std::string &value,
          Color color, double scale) {
    setColor(renderer, color);
    SDL_SetRenderScale(renderer, static_cast<float>(scale), static_cast<float>(scale));
    SDL_RenderDebugText(renderer, static_cast<float>(x / scale),
                        static_cast<float>(y / scale), value.c_str());
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

std::string format(const char *pattern, double first, double second) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), pattern, first, second);
    return buffer;
}

} // namespace tennis::render
