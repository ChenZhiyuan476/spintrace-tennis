#pragma once

#include "vec2.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <string>

namespace tennis::render {

struct Color {
    float r, g, b, a;
};

inline constexpr Color Background{0.063f, 0.106f, 0.090f, 1.0f};
inline constexpr Color CourtGreen{0.224f, 0.482f, 0.353f, 1.0f};
inline constexpr Color Clay{0.663f, 0.333f, 0.196f, 1.0f};
inline constexpr Color White{0.949f, 0.929f, 0.855f, 1.0f};
inline constexpr Color Muted{0.839f, 0.875f, 0.835f, 1.0f};
inline constexpr Color Yellow{1.0f, 0.941f, 0.651f, 1.0f};
inline constexpr Color Blue{0.357f, 0.753f, 0.922f, 1.0f};
inline constexpr Color Red{0.941f, 0.424f, 0.392f, 1.0f};
inline constexpr Color BallColor{0.914f, 1.0f, 0.439f, 1.0f};

Color qualityColor(double quality);
void setColor(SDL_Renderer *renderer, Color color);
void fillQuad(SDL_Renderer *renderer, const std::array<Vec2, 4> &points, Color color);
void thickLine(SDL_Renderer *renderer, Vec2 a, Vec2 b, double width, Color color);
void circle(SDL_Renderer *renderer, Vec2 center, double radius, Color color, int segments = 24);
void fillRect(SDL_Renderer *renderer, double x, double y, double width, double height, Color color);
void pixelShadow(SDL_Renderer *renderer, Vec2 center, double radiusX, double radiusY, Color color);
void pixelTennisBall(SDL_Renderer *renderer, Vec2 center, double radius, Color color,
                     bool drawSeam = true);
void pixelUmpire(SDL_Renderer *renderer, Vec2 ground, double scale, bool courtOnLeft);
void text(SDL_Renderer *renderer, double x, double y, const std::string &value,
          Color color = White, double scale = 2.0);
std::string format(const char *pattern, double first, double second = 0.0);

} // namespace tennis::render
