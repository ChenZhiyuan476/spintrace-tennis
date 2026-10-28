#pragma once

#include <algorithm>
#include <cmath>

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    Vec2() = default;
    Vec2(double px, double py) : x(px), y(py) {}

    Vec2 operator+(const Vec2 &other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2 &other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(double scalar) const { return {x * scalar, y * scalar}; }
    Vec2 operator/(double scalar) const { return {x / scalar, y / scalar}; }
    Vec2 &operator+=(const Vec2 &other) { x += other.x; y += other.y; return *this; }
    Vec2 &operator*=(double scalar) { x *= scalar; y *= scalar; return *this; }

    double lengthSquared() const { return x * x + y * y; }
    double length() const { return std::sqrt(lengthSquared()); }
    double distanceTo(const Vec2 &other) const { return (*this - other).length(); }
    Vec2 normalized() const {
        const double len = length();
        return len > 1e-9 ? *this / len : Vec2{};
    }
};

inline Vec2 operator*(double scalar, const Vec2 &value) { return value * scalar; }
