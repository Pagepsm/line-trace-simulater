#pragma once
#include "project/ProjectModel.h"
#include <filesystem>
#include <string>

namespace lt {
struct LoadedProject {
    ProjectModel model;
    std::string source;
};

class ProjectSerializer {
  public:
    static void save(const std::filesystem::path& file, const ProjectModel& model,
                     const std::string& source);
    static LoadedProject load(const std::filesystem::path& file);
};
}
