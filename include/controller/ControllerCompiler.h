#pragma once
#include <filesystem>
#include <string>

namespace lt {
struct BuildResult {
    bool success = false;
    std::filesystem::path dll;
    std::string log;
};

class ControllerCompiler {
  public:
    static std::filesystem::path findCompiler(const std::filesystem::path& appRoot = {});
    static BuildResult build(const std::filesystem::path& compiler,
                             const std::filesystem::path& directory,
                             const std::filesystem::path& api, const std::string& source);
};
}
