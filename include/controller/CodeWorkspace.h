#pragma once
#include "controller/ControllerCompiler.h"
#include "controller/ControllerRuntime.h"
#include <future>

namespace lt {
class CodeWorkspace {
  public:
    explicit CodeWorkspace(const std::filesystem::path& appDirectory);
    std::string source, log;
    std::filesystem::path sourcePath, compiler;
    std::shared_ptr<ControllerRuntime> runtime = std::make_shared<ControllerRuntime>();
    bool save();
    bool load();
    void build();
    bool poll();

    bool busy() const {
        return building_;
    }

    std::string activeGeneration;
    bool requiresBuild = true, buildFailed = false;
    void replaceSource(std::string text, std::filesystem::path path);

  private:
    std::filesystem::path api_, builds_;
    std::future<BuildResult> future_;
    bool building_ = false;
};
}
