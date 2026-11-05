#pragma once
#include "core/Geometry.h"
#include <vector>

namespace lt {
struct TrackPoint {
    Vec2 position;
    float width = .025f;
};

struct Stroke {
    std::vector<TrackPoint> points;
};

class TrackQuery {
  public:
    virtual ~TrackQuery() = default;
    virtual bool hit(Vec2 p) const = 0;
};

class TrackModel : public TrackQuery {
  public:
    std::vector<Stroke> strokes;
    bool hit(Vec2 p) const override;
    int pick(Vec2 p, float tolerance = 0) const;
    static TrackModel demonstration();
};
}
