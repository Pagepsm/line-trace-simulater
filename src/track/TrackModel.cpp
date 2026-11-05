#include "track/TrackModel.h"
#include <algorithm>
#include <numbers>

namespace lt {
bool TrackModel::hit(Vec2 p) const {
    return pick(p) >= 0;
}

int TrackModel::pick(Vec2 p, float tolerance) const {
    for (int index = static_cast<int>(strokes.size()) - 1; index >= 0; --index) {
        const auto& stroke = strokes[index];
        for (size_t i = 0; i < stroke.points.size(); ++i) {
            auto a = stroke.points[i];
            auto b = stroke.points[i ? i - 1 : i];
            auto d = b.position - a.position;
            float n = dot(d, d);
            float t = n > 0 ? std::clamp(dot(p - a.position, d) / n, 0.f, 1.f) : 0;
            float radius = (a.width + (b.width - a.width) * t) * .5f + tolerance;
            auto offset = p - (a.position + d * t);
            if (dot(offset, offset) <= radius * radius)
                return index;
        }
    }
    return -1;
}

TrackModel TrackModel::demonstration() {
    TrackModel t;
    Stroke s;
    for (int i = 0; i <= 256; ++i) {
        float a = 2 * std::numbers::pi_v<float> * i / 256;
        s.points.push_back({{.6f * std::cos(a), .6f * std::sin(a)}, .025f});
    }
    t.strokes.push_back(s);
    return t;
}
}
