#pragma once
#include <memory>
#include "simulation/DriveModel.h"

namespace lt {
class PhysicsWorld {
  public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    bool valid() const;
    void createVehicle(const VehicleConfig& config);
    BodyState bodyState() const;
    void drive(const VehicleConfig& config, MotorCommand command);
    void step(double dt, int substeps);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
