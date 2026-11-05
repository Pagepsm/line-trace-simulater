#include "simulation/PhysicsWorld.h"
#include <box2d/box2d.h>
#include <stdexcept>

namespace lt {
struct PhysicsWorld::Impl {
    b2WorldId id = b2_nullWorldId;
    b2BodyId body = b2_nullBodyId;
};

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {
    auto definition = b2DefaultWorldDef();
    definition.gravity = {0.0f, 0.0f};
    impl_->id = b2CreateWorld(&definition);
    if (!valid())
        throw std::runtime_error("Box2D world creation failed");
}

PhysicsWorld::~PhysicsWorld() {
    if (valid())
        b2DestroyWorld(impl_->id);
}

bool PhysicsWorld::valid() const {
    return b2World_IsValid(impl_->id);
}

void PhysicsWorld::createVehicle(const VehicleConfig& c) {
    c.validate();
    if (b2Body_IsValid(impl_->body))
        b2DestroyBody(impl_->body);
    auto def = b2DefaultBodyDef();
    def.type = b2_dynamicBody;
    def.position = {c.start.position.x, c.start.position.y};
    // Preserve the user-specified angle rather than using approximate b2MakeRot.
    def.rotation = {std::cos(c.start.angle), std::sin(c.start.angle)};
    def.enableSleep = false;
    impl_->body = b2CreateBody(impl_->id, &def);
    auto shape = b2DefaultShapeDef();
    shape.density = c.mass / (c.length * c.width);
    auto polygon = b2MakeBox(c.length * .5f, c.width * .5f);
    b2CreatePolygonShape(impl_->body, &shape, &polygon);
}

BodyState PhysicsWorld::bodyState() const {
    if (!b2Body_IsValid(impl_->body))
        throw std::logic_error("No vehicle body");
    auto p = b2Body_GetPosition(impl_->body), v = b2Body_GetLinearVelocity(impl_->body);
    auto rotation = b2Body_GetRotation(impl_->body);
    // Derive the display/sensor heading from the actual body axes, avoiding the
    // approximate b2Atan2 polynomial used by b2Rot_GetAngle.
    return {{{p.x, p.y}, std::atan2(rotation.s, rotation.c)},
            {v.x, v.y},
            b2Body_GetAngularVelocity(impl_->body)};
}

void PhysicsWorld::drive(const VehicleConfig& c, MotorCommand command) {
    auto b = bodyState();
    auto f = DriveModel::forces(c, b, command);
    auto l = worldPoint(b.pose, {0, c.wheelTrack * .5f}),
         r = worldPoint(b.pose, {0, -c.wheelTrack * .5f});
    b2Body_ApplyForce(impl_->body, {f.left.x, f.left.y}, {l.x, l.y}, true);
    b2Body_ApplyForce(impl_->body, {f.right.x, f.right.y}, {r.x, r.y}, true);
    b2Body_ApplyForceToCenter(impl_->body, {f.resistance.x, f.resistance.y}, true);
    b2Body_ApplyTorque(impl_->body, f.torque, true);
}

void PhysicsWorld::step(double dt, int substeps) {
    b2World_Step(impl_->id, static_cast<float>(dt), substeps);
}
}
