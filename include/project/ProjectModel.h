#pragma once
#include "track/TrackModel.h"
#include "core/Geometry.h"
#include "project/VehicleConfig.h"

namespace lt {
struct ProjectModel {
    TrackModel track;
    Pose start;
    VehicleConfig vehicle;
};
}
