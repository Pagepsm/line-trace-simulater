#include "controller/CodeWorkspace.h"
#include <fstream>

namespace lt {
CodeWorkspace::CodeWorkspace(const std::filesystem::path& root)
    : sourcePath(root / "workspace/user_logic.cpp"),
      compiler(ControllerCompiler::findCompiler(root)), api_(root / "controller/controller_api.h"),
      builds_(root / "workspace/builds") {
    std::filesystem::create_directories(sourcePath.parent_path());
    if (!std::filesystem::exists(sourcePath))
        std::filesystem::copy_file(root / "controller/user_logic.cpp", sourcePath);
    load();
    if (compiler.empty())
        log += "\nGCCが見つかりません。ビルドできません。";
}

bool CodeWorkspace::save() {
    try {
        std::filesystem::create_directories(sourcePath.parent_path());
        std::ofstream out(sourcePath, std::ios::binary);
        out.write(source.data(), source.size());
        if (!out)
            throw std::runtime_error("Cannot save source");
        log = "C++ソースを保存しました。";
        return true;
    } catch (const std::exception& e) {
        log = e.what();
        return false;
    }
}

void CodeWorkspace::replaceSource(std::string text, std::filesystem::path path) {
    source = std::move(text);
    sourcePath = std::move(path);
    runtime = std::make_shared<ControllerRuntime>();
    activeGeneration.clear();
    requiresBuild = true;
    buildFailed = false;
    log = "コードを復元しました。「実行」でビルドしてから走行します。";
}

bool CodeWorkspace::load() {
    try {
        std::ifstream in(sourcePath, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot load source");
        std::string next((std::istreambuf_iterator<char>(in)), {});
        if (next.size() > 1024 * 1024)
            throw std::runtime_error("Source exceeds 1 MiB editor limit");
        source = std::move(next);
        log = "C++ソースを読み込みました。ビルドで反映します。";
        return true;
    } catch (const std::exception& e) {
        log = e.what();
        return false;
    }
}

void CodeWorkspace::build() {
    if (building_ || !save())
        return;
    log = "制御コードをビルド中です。シミュレーションは停止しています。";
    buildFailed = false;
    auto compilerCopy = compiler, apiCopy = api_, directory = builds_;
    auto text = source;
    future_ = std::async(std::launch::async, [compilerCopy, apiCopy, directory, text] {
        return ControllerCompiler::build(compilerCopy, directory, apiCopy, text);
    });
    building_ = true;
}

bool CodeWorkspace::poll() {
    if (!building_ || future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return false;
    auto result = future_.get();
    building_ = false;
    log = result.log;
    if (!result.success) {
        buildFailed = true;
        log += "\nビルド失敗。以前の正常な制御コードを保持しています。";
        return false;
    }
    std::string error;
    if (!runtime->load(result.dll, error)) {
        buildFailed = true;
        log += "\n" + error;
        return false;
    }
    requiresBuild = false;
    activeGeneration = result.dll.parent_path().filename().string();
    log += "\nビルド・再読込に成功しました。「実行」で走行できます。";
    return true;
}
}
