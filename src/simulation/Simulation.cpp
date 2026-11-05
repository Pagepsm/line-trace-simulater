#include "simulation/Simulation.h"
#include <cmath>

namespace lt {
Simulation::Simulation(VehicleConfig config) : config_(config) {
    config_.validate();
    reset();
}

bool Simulation::ready() const {
    return world_.valid();
}

void Simulation::setController(std::shared_ptr<ControllerRuntime> runtime) {
    runtime_ = std::move(runtime);
    useDll_ = runtime_ && runtime_->loaded();
    reset();
}

void Simulation::applyProject(const ProjectModel& p) {
    auto candidate = p.vehicle;
    candidate.start = p.start;
    candidate.validate();
    config_ = candidate;
    track_ = p.track;
    reset();
}

void Simulation::reset() {
    running_ = false;
    accumulator_ = 0;
    ticks_ = 0;
    motors_ = {};
    controller_.reset();
    error_.clear();
    if (useDll_ && runtime_)
        runtime_->reset();
    world_.createVehicle(config_);
    sensors_ = SensorSystem::sample(track_, body().pose, config_.sensors);
    trajectory_.clear();
    trajectory_.push_back(body().pose.position);
}

void Simulation::command(SimulationAction a) {
    switch (a) {
    case SimulationAction::Run:
        if (error_.empty())
            running_ = true;
        break;
    case SimulationAction::Pause:
        running_ = false;
        accumulator_ = 0;
        break;
    case SimulationAction::Reset:
        reset();
        break;
    case SimulationAction::Step:
        if (!running_)
            tick();
        break;
    case SimulationAction::Follow:
        useDll_ = false;
        mode_ = PrototypeMode::FollowLine;
        reset();
        break;
    case SimulationAction::Equal:
        useDll_ = false;
        mode_ = PrototypeMode::Equal;
        reset();
        break;
    case SimulationAction::Differential:
        useDll_ = false;
        mode_ = PrototypeMode::Differential;
        reset();
        break;
    case SimulationAction::UseDll:
        if (runtime_ && runtime_->loaded()) {
            useDll_ = true;
            reset();
        }
        break;
    default:
        break;
    }
}

void Simulation::tick() {
    if (useDll_) {
        std::vector<float> values;
        for (auto sensor : sensors_)
            values.push_back(sensor.black ? 1.f : 0.f);
        if (!runtime_->step(time(), config_.fixedDt, values, motors_, error_)) {
            running_ = false;
            motors_ = {};
            return;
        }
    } else
        motors_ = controller_.step(mode_, sensors_, config_);
    world_.drive(config_, motors_);
    world_.step(config_.fixedDt, config_.substeps);
    ++ticks_;
    sensors_ = SensorSystem::sample(track_, body().pose, config_.sensors);
    if (ticks_ % 4 == 0) {
        if (trajectory_.size() >= 20000)
            trajectory_.erase(trajectory_.begin(), trajectory_.begin() + 1000);
        trajectory_.push_back(body().pose.position);
    }
}

void Simulation::advance(double dt) {
    if (!running_ || !std::isfinite(dt) || dt < 0)
        return;
    // Bound catch-up after stalls; simulated time never includes dropped wall time.
    accumulator_ += std::min(dt, .25);
    while (running_ && accumulator_ + 1e-12 >= config_.fixedDt) {
        tick();
        accumulator_ -= config_.fixedDt;
    }
}
}
