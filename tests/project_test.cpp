#include "project/ProjectSerializer.h"
#include "controller/ControllerCompiler.h"
#include "simulation/Simulation.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace lt;

void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}

int main() {
    try {
        auto root =
            std::filesystem::current_path() / std::filesystem::path(u8"project_tests_日本語");
        std::filesystem::create_directories(root);
        ProjectModel model;
        model.start = {{.123f, -.456f}, .25f};
        model.track = TrackModel::demonstration();
        model.track.strokes[0].points[0].position.x = .61f;
        model.vehicle.mass = .321f;
        model.vehicle.length = .19f;
        model.vehicle.width = .12f;
        model.vehicle.wheelTrack = .13f;
        model.vehicle.maxForce = .55f;
        model.vehicle.wheelLength = .051f;
        model.vehicle.wheelWidth = .021f;
        model.vehicle.forwardDrag = .7f;
        model.vehicle.lateralDrag = 9;
        model.vehicle.angularDrag = .02f;
        model.vehicle.fixedDt = .01;
        model.vehicle.substeps = 3;
        model.vehicle.sensors = {{.1f, .02f}, {.1f, 0}, {.1f, -.02f}};
        std::ifstream templateFile("controller/user_logic.cpp", std::ios::binary);
        std::string source((std::istreambuf_iterator<char>(templateFile)), {});
        auto file = root / "original/project.json";
        ProjectSerializer::save(file, model, source);
        auto first = ProjectSerializer::load(file);
        check(first.source == source && first.model.start.position.x == model.start.position.x &&
                  first.model.start.angle == model.start.angle,
              "source and start roundtrip");
        const auto& c = first.model.vehicle;
        const auto& v = model.vehicle;
        check(c.mass == v.mass && c.length == v.length && c.width == v.width &&
                  c.wheelTrack == v.wheelTrack && c.maxForce == v.maxForce &&
                  c.wheelLength == v.wheelLength && c.wheelWidth == v.wheelWidth &&
                  c.forwardDrag == v.forwardDrag && c.lateralDrag == v.lateralDrag &&
                  c.angularDrag == v.angularDrag && c.fixedDt == v.fixedDt &&
                  c.substeps == v.substeps && c.sensors.size() == 3,
              "all vehicle fields roundtrip");
        check(first.model.track.strokes[0].points.size() == 257 &&
                  first.model.track.strokes[0].points[0].position.x == .61f,
              "stroke point roundtrip");
        auto moved = root / "moved";
        std::filesystem::create_directories(moved);
        std::filesystem::copy(root / "original", moved,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        auto relocated = ProjectSerializer::load(moved / "project.json");
        check(relocated.source == source, "relocated Project source relative path");
        auto built = ControllerCompiler::build(
            ControllerCompiler::findCompiler(std::filesystem::current_path()), root / "builds",
            "controller/controller_api.h", relocated.source);
        check(built.success, "restored source builds");
        auto runtime = std::make_shared<ControllerRuntime>();
        std::string error;
        check(runtime->load(built.dll, error), "restored DLL loads");
        Simulation a, b;
        a.applyProject(first.model);
        b.applyProject(relocated.model);
        a.setController(runtime);
        for (int i = 0; i < 100; ++i)
            a.command(SimulationAction::Step);
        auto expected = a.body();
        runtime->reset();
        b.setController(runtime);
        for (int i = 0; i < 100; ++i)
            b.command(SimulationAction::Step);
        check(std::abs(expected.pose.position.x - b.body().pose.position.x) < 1e-6 &&
                  std::abs(expected.pose.angle - b.body().pose.angle) < 1e-6,
              "restored Project reproduces motion");
        ProjectSerializer::save(file, model, source + "\n// second snapshot\n");
        check(ProjectSerializer::load(file).source != source,
              "second save replaces source snapshot");
        check(ProjectSerializer::load(moved / "project.json").source == source,
              "independent relocated Project unchanged");
        std::ifstream manifest(file);
        std::string invalid((std::istreambuf_iterator<char>(manifest)), {});
        auto pos = invalid.find("\"format_version\": 1");
        check(pos != std::string::npos, "version present");
        invalid.replace(pos, 19, "\"format_version\": 9");
        {
            std::ofstream out(root / "bad.json");
            out << invalid;
        }
        bool rejected = false;
        try {
            ProjectSerializer::load(root / "bad.json");
        } catch (...) {
            rejected = true;
        }
        check(rejected, "unknown format rejected");
        std::cout << "PASS all config/stroke/start/source roundtrip, Japanese path, relocation, restored build/run, independent snapshots, invalid version\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
