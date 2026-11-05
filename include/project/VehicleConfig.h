#pragma once
#include "core/Geometry.h"
#include <filesystem>
#include <vector>

namespace lt {
struct VehicleConfig {
    float length = .16f, width = .10f, mass = .15f, wheelTrack = .11f;
    float wheelLength = .045f, wheelWidth = .018f;
    float maxForce = .4f, forwardDrag = .6f, lateralDrag = 8, angularDrag = .01f;
    double fixedDt = 1.0 / 120.0;
    int substeps = 4;
    Pose start{{0, -.6f}, 0};
    std::vector<Vec2> sensors{{.09f, .03f}, {.09f, .01f}, {.09f, -.01f}, {.09f, -.03f}};
    void validate() const;
};

VehicleConfig loadVehicleConfig(const std::filesystem::path& path);
}
