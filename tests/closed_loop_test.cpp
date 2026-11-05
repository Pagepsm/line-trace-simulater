#include "simulation/Simulation.h"
#include <iostream>
#include <stdexcept>
#include <numbers>
using namespace lt;

void check(bool ok, const char* msg) {
    if (!ok)
        throw std::runtime_error(msg);
}

bool near(float a, float b, float eps = 1e-5f) {
    return std::abs(a - b) < eps;
}

void steps(Simulation& s, int n) {
    for (int i = 0; i < n; ++i)
        s.command(SimulationAction::Step);
}

int main() {
    try {
        Camera camera{{.3f, -.4f}, {400, 250}, 125};
        Vec2 original{-.12f, .81f};
        for (float zoom : {50.f, 125.f, 800.f}) {
            camera.pixelsPerMeter = zoom;
            auto p = camera.toWorld(camera.toScreen(original));
            check(near(p.x, original.x) && near(p.y, original.y), "coordinate roundtrip");
        }
        TrackModel line;
        line.strokes = {{{{{0, 0}, .02f}, {{1, 0}, .02f}}}};
        check(line.hit({.5f, .009f}) && !line.hit({.5f, .011f}), "line width hit boundary");
        check(line.hit({-.009f, 0}) && !line.hit({-.011f, 0}), "round line endpoint");
        auto samples = SensorSystem::sample(line, {{.5f, -.1f}, std::numbers::pi_v<float> / 2},
                                            {{.1f, 0}, {0, 0}});
        check(samples.size() == 2 && samples[0].black && !samples[1].black,
              "rotated sensor world sampling");
        VehicleConfig c = loadVehicleConfig("vehicle.cfg");
        Simulation equal(c);
        equal.command(SimulationAction::Equal);
        steps(equal, 240);
        auto e = equal.body();
        check(e.pose.position.x > .1f && near(e.pose.position.y, c.start.position.y) &&
                  near(e.pose.angle, 0),
              "equal forces straight");
        Simulation differential(c);
        differential.command(SimulationAction::Differential);
        steps(differential, 240);
        auto d = differential.body();
        check(d.pose.angle > .2f && d.pose.position.y > c.start.position.y + .02f,
              "differential forces turn left");
        Simulation feedback(c);
        auto initial = feedback.sensors();
        steps(feedback, 1200);
        auto f = feedback.body();
        check(std::abs(f.pose.angle) > .1f, "feedback turns");
        Simulation crossing(c);
        crossing.command(SimulationAction::Equal);
        auto before = crossing.sensors();
        steps(crossing, 240);
        bool crossed = false;
        for (size_t i = 0; i < before.size(); ++i)
            crossed |= before[i].black != crossing.sensors()[i].black;
        check(crossed, "sensors change when crossing fixed track");
        PrototypeController pc;
        auto left = initial, right = initial;
        for (auto& s : left)
            s.black = false;
        right = left;
        left.front().black = true;
        right.back().black = true;
        auto ml = pc.step(PrototypeMode::FollowLine, left, c);
        pc.reset();
        auto mr = pc.step(PrototypeMode::FollowLine, right, c);
        check(ml.right > ml.left && mr.left > mr.right, "sensor feedback changes drive direction");
        feedback.reset();
        check(feedback.time() == 0 && !feedback.running() && feedback.motors().left == 0 &&
                  feedback.motors().right == 0,
              "reset time state commands");
        check(near(feedback.body().pose.position.x, c.start.position.x) &&
                  near(feedback.body().pose.position.y, c.start.position.y) &&
                  near(feedback.body().angularVelocity, 0) &&
                  near(dot(feedback.body().velocity, feedback.body().velocity), 0),
              "reset body velocity pose");
        Simulation fresh(c);
        steps(fresh, 240);
        steps(feedback, 240);
        check(near(fresh.body().pose.position.x, feedback.body().pose.position.x) &&
                  near(fresh.body().pose.angle, feedback.body().pose.angle),
              "reset reproduces controller state");
        auto paused = feedback.body();
        auto t = feedback.time();
        feedback.advance(10);
        check(feedback.time() == t && near(feedback.body().pose.position.x, paused.pose.position.x),
              "pause freezes physics");
        Simulation low(c), high(c);
        low.command(SimulationAction::Run);
        high.command(SimulationAction::Run);
        for (int i = 0; i < 60; ++i)
            low.advance(1.0 / 30);
        for (int i = 0; i < 288; ++i)
            high.advance(1.0 / 144);
        check(std::abs(low.time() - high.time()) < 1e-9 &&
                  near(low.body().pose.position.x, high.body().pose.position.x) &&
                  near(low.body().pose.angle, high.body().pose.angle),
              "30 vs 144 FPS invariant");
        c.sensors = {{.1f, 0}};
        Simulation one(c);
        steps(one, 10);
        check(one.sensors().size() == 1, "sensor count configurable");
        std::cout << "PASS coordinate/track/sensor/straight/turn/feedback/reset/pause/FPS/count\n";
        std::cout << "straight x=" << e.pose.position.x << " heading=" << e.pose.angle
                  << "; differential x=" << d.pose.position.x << " y=" << d.pose.position.y
                  << " heading=" << d.pose.angle << "; feedback x=" << f.pose.position.x
                  << " y=" << f.pose.position.y << " heading=" << f.pose.angle << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
