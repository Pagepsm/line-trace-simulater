#pragma once
#include "simulation/PhysicsWorld.h"
#include "controller/PrototypeController.h"
#include <cstdint>
#include "project/ProjectModel.h"
#include "controller/ControllerRuntime.h"

namespace lt {
enum class SimulationAction { None, Run, Pause, Reset, Step, Follow, Equal, Differential, UseDll };

class Simulation {
  public:
    explicit Simulation(VehicleConfig config = {});
    bool ready() const;
    void command(SimulationAction action);
    void advance(double renderDt);
    void reset();
    void applyProject(const ProjectModel& project);
    void setController(std::shared_ptr<ControllerRuntime> runtime);

    bool usingDll() const {
        return useDll_;
    }

    const std::string& error() const {
        return error_;
    }

    const VehicleConfig& config() const {
        return config_;
    }

    const TrackModel& track() const {
        return track_;
    }

    const std::vector<SensorReading>& sensors() const {
        return sensors_;
    }

    const std::vector<Vec2>& trajectory() const {
        return trajectory_;
    }

    BodyState body() const {
        return world_.bodyState();
    }

    MotorCommand motors() const {
        return motors_;
    }

    double time() const {
        return ticks_ * config_.fixedDt;
    }

    bool running() const {
        return running_;
    }

    PrototypeMode mode() const {
        return mode_;
    }

  private:
    void tick();
    VehicleConfig config_;
    PhysicsWorld world_;
    TrackModel track_ = TrackModel::demonstration();
    PrototypeController controller_;
    PrototypeMode mode_ = PrototypeMode::FollowLine;
    std::vector<SensorReading> sensors_;
    std::vector<Vec2> trajectory_;
    MotorCommand motors_;
    std::uint64_t ticks_ = 0;
    double accumulator_ = 0;
    bool running_ = false;
    bool useDll_ = false;
    std::shared_ptr<ControllerRuntime> runtime_;
    std::string error_;
};
}
