#include "project/VehicleConfig.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace lt {
void VehicleConfig::validate() const {
    auto positive = [](float v) { return std::isfinite(v) && v > 0; };
    for (float v : {length, width, mass, wheelTrack, wheelLength, wheelWidth, maxForce})
        if (!positive(v))
            throw std::runtime_error(
                "Vehicle dimensions, mass and force must be positive and finite");
    for (float v : {forwardDrag, lateralDrag, angularDrag})
        if (!std::isfinite(v) || v < 0)
            throw std::runtime_error("Invalid drag coefficient");
    if (!std::isfinite(fixedDt) || fixedDt < .0001 || fixedDt > .02 || substeps < 1 ||
        substeps > 16)
        throw std::runtime_error("Invalid physics timestep/substeps");
    if (!std::isfinite(start.position.x) || !std::isfinite(start.position.y) ||
        !std::isfinite(start.angle))
        throw std::runtime_error("Invalid start pose");
    if (sensors.empty() || sensors.size() > 256)
        throw std::runtime_error(
            "Sensor count must be 1..256 (internal resource limit, not DLL ABI)");
    for (auto s : sensors)
        if (!std::isfinite(s.x) || !std::isfinite(s.y))
            throw std::runtime_error("Invalid sensor position");
}

VehicleConfig loadVehicleConfig(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Missing vehicle.cfg beside executable");
    VehicleConfig c;
    c.sensors.clear();
    std::string line;
    while (std::getline(file, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream in(line);
        std::string key;
        if (!(in >> key))
            continue;
        if (key == "sensor") {
            Vec2 p;
            if (!(in >> p.x >> p.y))
                throw std::runtime_error("Invalid sensor entry");
            c.sensors.push_back(p);
        } else if (key == "start") {
            if (!(in >> c.start.position.x >> c.start.position.y >> c.start.angle))
                throw std::runtime_error("Invalid start entry");
        } else {
            float* value = nullptr;
            if (key == "length")
                value = &c.length;
            else if (key == "width")
                value = &c.width;
            else if (key == "mass")
                value = &c.mass;
            else if (key == "wheel_track")
                value = &c.wheelTrack;
            else if (key == "wheel_length")
                value = &c.wheelLength;
            else if (key == "wheel_width")
                value = &c.wheelWidth;
            else if (key == "max_force")
                value = &c.maxForce;
            else if (key == "forward_drag")
                value = &c.forwardDrag;
            else if (key == "lateral_drag")
                value = &c.lateralDrag;
            else if (key == "angular_drag")
                value = &c.angularDrag;
            if (value) {
                if (!(in >> *value))
                    throw std::runtime_error("Invalid config value: " + key);
            } else if (key == "fixed_dt") {
                if (!(in >> c.fixedDt))
                    throw std::runtime_error("Invalid dt");
            } else if (key == "substeps") {
                if (!(in >> c.substeps))
                    throw std::runtime_error("Invalid substeps");
            } else
                throw std::runtime_error("Unknown config key: " + key);
        }
        std::string extra;
        if (in >> extra)
            throw std::runtime_error("Extra config data: " + key);
    }
    c.validate();
    return c;
}
}
