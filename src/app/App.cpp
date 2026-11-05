#include "app/App.h"
#include "simulation/Simulation.h"
#include "ui/RenderAdapter.h"
#include "ui/EditorUI.h"
#include "platform/Paths.h"
#include "project/ProjectSerializer.h"
#include <fstream>
#include <iostream>

namespace lt {
int runApp(bool smoke) {
    const auto root = applicationDirectory();
    for (const auto* file :
         {"vehicle.cfg", "controller/controller_api.h", "controller/user_logic.cpp",
          "assets/fonts/NotoSansCJKjp-Regular.otf"}) {
        if (!std::filesystem::is_regular_file(root / file))
            throw std::runtime_error(
                std::string("必要なファイルがありません。ZIP全体を展開し直してください: ") + file);
    }
    if (std::filesystem::exists(root / "portable.txt")) {
        for (const auto* file :
             {"compiler/bin/g++.exe", "compiler/bin/as.exe", "compiler/bin/ld.exe",
              "compiler/lib/gcc/x86_64-w64-mingw32/16.1.0/cc1plus.exe"}) {
            if (!std::filesystem::is_regular_file(root / file))
                throw std::runtime_error(
                    std::string("同梱コンパイラが不足しています。ZIP全体を展開し直してください: ") +
                    file);
        }
    }

    auto config = loadVehicleConfig(applicationDirectory() / "vehicle.cfg");
    Simulation simulation(config);
    ProjectEditor editor({simulation.track(), config.start, config});
    CodeWorkspace code(applicationDirectory());
    RenderAdapter renderer;
    EditorUI ui;
    auto defaultPath = (applicationDirectory() / "projects/my_project/project.json").u8string();
    ProjectPanel project{std::string(defaultPath.begin(), defaultPath.end()), ""};

    bool runAfterBuild = false;
    int frames = 0;

    while (!renderer.shouldClose()) {
        if (code.poll()) {
            simulation.setController(code.runtime);
            if (runAfterBuild)
                simulation.command(SimulationAction::Run);
            runAfterBuild = false;
        } else if (!code.busy())
            runAfterBuild = false;

        simulation.advance(renderer.frameTime());
        renderer.beginFrame();
        auto actions = ui.draw(simulation, editor, code, project);
        renderer.endFrame();

        if (actions.simulation == SimulationAction::Run && code.requiresBuild) {
            simulation.command(SimulationAction::Pause);
            code.build();
            runAfterBuild = code.busy();
        } else {
            simulation.command(actions.simulation);
            if (actions.simulation == SimulationAction::Follow ||
                actions.simulation == SimulationAction::Equal ||
                actions.simulation == SimulationAction::Differential)
                code.requiresBuild = false;
        }

        bool changed = false;
        for (const auto& edit : actions.edits)
            changed = editor.apply(edit) || changed;
        if (changed)
            simulation.applyProject(editor.model());

        if (actions.code == CodeAction::Save)
            code.save();
        if (actions.code == CodeAction::Load) {
            simulation.command(SimulationAction::Pause);
            code.load();
        }
        if (actions.code == CodeAction::Build) {
            simulation.command(SimulationAction::Pause);
            code.build();
        }

        if (actions.project != ProjectAction::None && !code.busy()) {
            simulation.command(SimulationAction::Pause);
            try {
                auto path = std::filesystem::path(
                    std::u8string(reinterpret_cast<const char8_t*>(project.path.c_str())));
                if (path.empty() || path.filename().empty())
                    throw std::runtime_error("保存先のファイル名を指定してください");
                if (actions.project == ProjectAction::Save) {
                    ProjectSerializer::save(path, editor.model(), code.source);
                    project.message = "セーブしました。";
                } else {
                    LoadedProject loaded;
                    if (actions.project == ProjectAction::Load)
                        loaded = ProjectSerializer::load(path);
                    else {
                        loaded.model = {{}, config.start, config};
                        std::ifstream cpp(applicationDirectory() / "controller/user_logic.cpp",
                                          std::ios::binary);
                        if (!cpp)
                            throw std::runtime_error("制御コードのテンプレートがありません");
                        loaded.source.assign(std::istreambuf_iterator<char>(cpp), {});
                    }
                    simulation.setController(nullptr);
                    editor.replace(std::move(loaded.model));
                    simulation.applyProject(editor.model());
                    code.replaceSource(std::move(loaded.source),
                                       std::filesystem::absolute(path).parent_path() /
                                           "workspace/user_logic.cpp");
                    ui.projectChanged();
                    runAfterBuild = false;
                    project.message =
                        actions.project == ProjectAction::Load
                            ? "ロードしました。「実行」で保存したコードを再ビルドします。"
                            : "新しいコースです。まず線を描いてください。";
                }
            } catch (const std::exception& e) {
                project.message =
                    std::string("保存・読込に失敗しました（現在の内容は保持）：") + e.what();
            }
        }

        if (smoke && ++frames >= 30)
            break;
    }

    if (smoke && frames != 30)
        return 2;
    std::cout << "Simulation status: world=" << simulation.ready() << " smoke_frames=" << frames << '\n';
    return simulation.ready() ? 0 : 1;
}
}
