#pragma once
#include "simulation/DriveModel.h"
#include "simulation/SensorSystem.h"

namespace lt {
enum class PrototypeMode { FollowLine, Equal, Differential };

class PrototypeController {
    float lastError_ = 0;

  public:
    void reset() {
        lastError_ = 0;
    }

    MotorCommand step(PrototypeMode mode, const std::vector<SensorReading>& sensors,
                      const VehicleConfig& config) {
        if (mode == PrototypeMode::Equal)
            return {.3f, .3f};
        if (mode == PrototypeMode::Differential)
            return {.16f, .36f};
        float error = 0, scale = .001f;
        int count = 0;
        for (size_t i = 0; i < sensors.size(); ++i) {
            scale = std::max(scale, std::abs(config.sensors[i].y));
            if (sensors[i].black) {
                error += config.sensors[i].y;
                ++count;
            }
        }
        if (count)
            lastError_ = error / (count * scale);
        float correction = .22f * lastError_;
        return {.28f - correction, .28f + correction};
    }
};
}
