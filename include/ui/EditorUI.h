#pragma once
#include "simulation/Simulation.h"
#include "project/ProjectEditor.h"
#include "ui/PointerEvents.h"
#include "controller/CodeWorkspace.h"

namespace lt {
enum class CodeAction { None, Save, Load, Build };
enum class ProjectAction { None, New, Save, Load };

struct ProjectPanel {
    std::string path, message;
};

struct UIActions {
    SimulationAction simulation = SimulationAction::None;
    std::vector<EditCommand> edits;
    CodeAction code = CodeAction::None;
    ProjectAction project = ProjectAction::None;
};

class EditorUI {
  public:
    EditorUI();
    ~EditorUI();
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;
    UIActions draw(const Simulation& simulation, const ProjectEditor& editor, CodeWorkspace& code,
                   ProjectPanel& project);

    void projectChanged() {
        selected_ = -1;
        drawing_ = moving_ = placing_ = false;
        draft_.points.clear();
        poseDirty_ = false;
        camera_.center = {};
        zoom_ = 1;
    }

  private:
    enum class Tool { Draw, Select, Erase, Start };
    Tool tool_ = Tool::Draw;
    int page_ = 0;
    Camera camera_;
    CameraPan pan_;
    float zoom_ = 1, width_ = .025f;
    int selected_ = -1;
    bool drawing_ = false, moving_ = false, placing_ = false;
    Vec2 dragStart_, moveDelta_, pendingStart_;
    Stroke draft_;
    float poseFields_[3] = {0, -.6f, 0};
    bool poseDirty_ = false;
};
}
