#pragma once
#include "track/TrackModel.h"
#include <vector>

namespace lt {
struct SensorReading {
    Vec2 position;
    bool black = false;
};

class SensorSystem {
  public:
    static std::vector<SensorReading> sample(const TrackQuery& track, Pose pose,
                                             const std::vector<Vec2>& local) {
        std::vector<SensorReading> result;
        for (auto p : local) {
            auto w = worldPoint(pose, p);
            result.push_back({w, track.hit(w)});
        }
        return result;
    }
};
}
