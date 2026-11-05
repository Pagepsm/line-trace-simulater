#pragma once
#include "project/VehicleConfig.h"
#include <algorithm>

namespace lt {
struct MotorCommand {
    float left = 0, right = 0;
};

struct BodyState {
    Pose pose;
    Vec2 velocity;
    float angularVelocity = 0;
};

struct DriveForces {
    Vec2 left, right, resistance;
    float torque = 0;
};

class DriveModel {
  public:
    static DriveForces forces(const VehicleConfig& c, BodyState b, MotorCommand command) {
        auto forward = rotate({1, 0}, b.pose.angle), lateral = rotate({0, 1}, b.pose.angle);
        return {forward * (c.maxForce * std::clamp(command.left, -1.f, 1.f)),
                forward * (c.maxForce * std::clamp(command.right, -1.f, 1.f)),
                forward * (-c.forwardDrag * dot(b.velocity, forward)) +
                    lateral * (-c.lateralDrag * dot(b.velocity, lateral)),
                -c.angularDrag * b.angularVelocity};
    }
};
}
