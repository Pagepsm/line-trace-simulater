#pragma once
#include "simulation/DriveModel.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace lt {
class ControllerRuntime {
  public:
    ControllerRuntime();
    ~ControllerRuntime();
    ControllerRuntime(const ControllerRuntime&) = delete;
    ControllerRuntime& operator=(const ControllerRuntime&) = delete;
    bool load(const std::filesystem::path& dll, std::string& error);
    bool loaded() const;
    void reset();
    bool step(double time, double dt, const std::vector<float>& sensors, MotorCommand& result,
              std::string& error);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
