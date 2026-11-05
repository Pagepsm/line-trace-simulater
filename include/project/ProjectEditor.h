#pragma once
#include "project/ProjectModel.h"
#include <vector>

namespace lt {
enum class EditKind { Add, Remove, Move, Width, Start, Clear, Undo, Redo };

struct EditCommand {
    EditKind kind;
    int index = -1;
    Stroke stroke;
    Vec2 delta;
    float width = .025f;
    Pose pose;
};

class ProjectEditor {
  public:
    explicit ProjectEditor(ProjectModel model) : model_(std::move(model)) {}

    const ProjectModel& model() const {
        return model_;
    }

    void replace(ProjectModel model) {
        model_ = std::move(model);
        undo_.clear();
        redo_.clear();
    }

    bool apply(const EditCommand& command);

    bool canUndo() const {
        return !undo_.empty();
    }

    bool canRedo() const {
        return !redo_.empty();
    }

    static constexpr size_t historyLimit = 100;

  private:
    ProjectModel model_;
    std::vector<ProjectModel> undo_, redo_;
};
}
