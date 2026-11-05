#include "project/ProjectEditor.h"
#include "simulation/Simulation.h"
#include "ui/PointerEvents.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace lt;

void check(bool ok, const char* text) {
    if (!ok)
        throw std::runtime_error(text);
}

bool near(float a, float b) {
    return std::abs(a - b) < 1e-5f;
}

int main() {
    try {
        ProjectEditor editor({{}, {{0, 0}, 0}});
        EditCommand add{EditKind::Add};
        add.stroke.points = {{{0, 0}, .02f}, {{1, 0}, .02f}};
        check(editor.apply(add) && editor.model().track.hit({.5f, 0}), "draw hit");
        check(!editor.model().track.hit({.5f, .025f}), "narrow stroke");
        EditCommand width{EditKind::Width};
        width.index = 0;
        width.width = .08f;
        check(editor.apply(width) && editor.model().track.hit({.5f, .025f}),
              "width changes sensor geometry");
        check(editor.apply({EditKind::Undo}) && !editor.model().track.hit({.5f, .025f}),
              "undo width");
        check(editor.apply({EditKind::Redo}) && editor.model().track.hit({.5f, .025f}),
              "redo width");
        EditCommand move{EditKind::Move};
        move.index = 0;
        move.delta = {0, .5f};
        check(editor.apply(move) && !editor.model().track.hit({.5f, 0}) &&
                  editor.model().track.hit({.5f, .5f}),
              "move world geometry");
        check(editor.apply({EditKind::Undo}) && editor.model().track.hit({.5f, 0}), "undo move");
        check(editor.apply({EditKind::Redo}), "redo move");
        EditCommand pose{EditKind::Start};
        pose.pose = {{.4f, .5f}, .25f};
        check(editor.apply(pose), "start edit");
        check(editor.apply({EditKind::Undo}) && near(editor.model().start.angle, 0), "undo start");
        check(editor.apply({EditKind::Redo}) && near(editor.model().start.angle, .25f),
              "redo start");
        auto snapshot = editor.model();
        check(editor.apply({EditKind::Clear}) && editor.model().track.strokes.empty(), "clear");
        check(editor.apply({EditKind::Undo}) && editor.model().track.hit({.5f, .5f}) &&
                  near(editor.model().start.angle, .25f),
              "clear undo keeps pose");
        EditCommand remove{EditKind::Remove};
        remove.index = 0;
        check(editor.apply(remove) && !editor.canRedo(), "delete invalidates redo branch");
        check(editor.apply({EditKind::Undo}) && editor.model().track.strokes.size() == 1,
              "undo delete");
        check(!editor.apply({EditKind::Remove}) && editor.canRedo(),
              "invalid command preserves history");
        auto invalid = add;
        invalid.stroke.points[0].position.x = std::numeric_limits<float>::quiet_NaN();
        check(!editor.apply(invalid) && editor.canRedo(), "nonfinite rejected transactionally");
        Simulation sim;
        sim.command(SimulationAction::Run);
        sim.advance(.2);
        sim.applyProject(snapshot);
        check(!sim.running() && sim.time() == 0 && sim.motors().left == 0 &&
                  sim.trajectory().size() == 1,
              "edit resets running simulation");
        std::cout << "start body=" << sim.body().pose.position.x << ","
                  << sim.body().pose.position.y << "," << sim.body().pose.angle
                  << " expected .4,.5,.25\n";
        check(near(sim.body().pose.position.x, .4f) && near(sim.body().pose.position.y, .5f) &&
                  near(sim.body().pose.angle, .25f),
              "edited start applied to Box2D");
        for (const auto& sensor : sim.sensors())
            check(sensor.black == snapshot.track.hit(sensor.position),
                  "edited track sensor agreement");
        Camera camera;
        for (float zoom : {50.f, 300.f, 1800.f}) {
            camera.pixelsPerMeter = zoom;
            camera.center = {2, -3};
            camera.origin = {111, 333};
            auto p = camera.toWorld(camera.toScreen({.5f, .5f}));
            check(near(p.x, .5f) && near(p.y, .5f) && snapshot.track.hit(p),
                  "pan zoom leaves world sampling unchanged");
        }
        ProjectEditor history({{}, {}});
        for (int i = 0; i < 110; ++i)
            check(history.apply(add), "history add");
        Camera panCamera{{1, 2}, {}, 100};
        CameraPan pan;
        pan.process({PointerKind::MiddleDown, {100, 200}}, true, true, panCamera);
        pan.process({PointerKind::Move, {140, 230}}, true, true, panCamera);
        pan.process({PointerKind::MiddleUp, {200, 250}}, true, true, panCamera);
        check(!pan.active && near(panCamera.center.x, 0) && near(panCamera.center.y, 2.5f),
              "queued middle-button pan");
        int count = 0;
        while (history.apply({EditKind::Undo}))
            ++count;
        check(count == 100 && history.model().track.strokes.size() == 10, "bounded undo");
        while (history.apply({EditKind::Redo})) {
        }
        check(history.model().track.strokes.size() == 110, "bounded redo");
        std::cout << "PASS draw/width/move/delete/start/clear undo-redo, branching, validation, simulation reset, sensor agreement, camera invariance, history limit\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
