#pragma once
#include <cmath>

namespace lt {
struct Vec2 {
    float x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(Vec2 a, float s) {
    return {a.x * s, a.y * s};
}

inline float dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

struct Pose {
    Vec2 position;
    float angle = 0;
};

inline Vec2 rotate(Vec2 p, float a) {
    return {std::cos(a) * p.x - std::sin(a) * p.y, std::sin(a) * p.x + std::cos(a) * p.y};
}

inline Vec2 worldPoint(Pose p, Vec2 local) {
    return p.position + rotate(local, p.angle);
}

// World +x right, +y up; screen +y down. Camera never changes world geometry.
struct Camera {
    Vec2 center{}, origin{};
    float pixelsPerMeter = 300;

    Vec2 toScreen(Vec2 p) const {
        auto d = p - center;
        return origin + Vec2{d.x, -d.y} * pixelsPerMeter;
    }

    Vec2 toWorld(Vec2 p) const {
        auto d = (p - origin) * (1 / pixelsPerMeter);
        return center + Vec2{d.x, -d.y};
    }
};
}
