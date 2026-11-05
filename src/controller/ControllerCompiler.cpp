#include "controller/ControllerCompiler.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fstream>
#include <sstream>
#include <atomic>
#include <chrono>

namespace lt {
std::filesystem::path ControllerCompiler::findCompiler(const std::filesystem::path& appRoot) {
    if (!appRoot.empty() && (std::filesystem::exists(appRoot / "portable.txt") ||
                             std::filesystem::exists(appRoot / "compiler"))) {
        auto bundled = appRoot / "compiler/bin/g++.exe";
        return std::filesystem::is_regular_file(bundled) ? bundled : std::filesystem::path{};
    }
    std::filesystem::path known = L"C:/msys64/ucrt64/bin/g++.exe";
    if (std::filesystem::is_regular_file(known))
        return known;
    wchar_t buffer[32768];
    auto length = SearchPathW(nullptr, L"g++.exe", nullptr, 32768, buffer, nullptr);
    if (length > 0 && length < 32768)
        return buffer;
    return {};
}

BuildResult ControllerCompiler::build(const std::filesystem::path& compiler,
                                      const std::filesystem::path& directory,
                                      const std::filesystem::path& api, const std::string& source) {
    BuildResult result;
    try {
        if (compiler.empty() || !std::filesystem::is_regular_file(compiler)) {
            result.log = "GCCが見つかりません。MSYS2 UCRT64 GCCをインストールするか、PATHにg++.exeを設定してください。";
            return result;
        }
        static std::atomic<unsigned> sequence = 0;
        auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        auto generation =
            std::filesystem::absolute(directory) /
            ("controller_" + std::to_string(stamp) + "_" + std::to_string(GetCurrentProcessId()) +
             "_" + std::to_string(sequence++));
        std::filesystem::create_directories(generation);
        std::filesystem::copy_file(api, generation / "controller_api.h",
                                   std::filesystem::copy_options::overwrite_existing);
        {
            std::ofstream file(generation / "user_logic.cpp", std::ios::binary);
            file.write(source.data(), source.size());
            if (!file)
                throw std::runtime_error("制御コードを書き込めませんでした。");
        }
        result.dll = generation / "controller.dll";
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE log = CreateFileW((generation / "build.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log == INVALID_HANDLE_VALUE)
            throw std::runtime_error("ビルドログを作成できませんでした。");
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        std::wstring command = L"\"" + compiler.wstring() +
                               L"\" -std=c++20 -shared -static -static-libgcc -static-libstdc++ -O2 -Wall -Wextra user_logic.cpp -o controller.dll";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = log;
        startup.hStdError = log;
        PROCESS_INFORMATION process{};
        // Do not let installed compilers, injected include paths, or user GCC settings
        // supply missing pieces of the packaged compiler.
        std::wstring environment;
        auto original = GetEnvironmentStringsW();
        for (auto item = original; item && *item; item += wcslen(item) + 1) {
            std::wstring entry = item;
            auto key = entry.substr(0, entry.find(L'='));
            for (auto& ch : key)
                ch = towupper(ch);
            if (key == L"PATH" || key == L"GCC_EXEC_PREFIX" || key == L"COMPILER_PATH" ||
                key == L"LIBRARY_PATH" || key == L"CPATH" || key == L"CPLUS_INCLUDE_PATH" ||
                key == L"C_INCLUDE_PATH")
                continue;
            environment += entry;
            environment.push_back(0);
        }
        if (original)
            FreeEnvironmentStringsW(original);
        wchar_t windows[32768];
        GetWindowsDirectoryW(windows, 32768);
        environment +=
            L"PATH=" + compiler.parent_path().wstring() + L";" + windows + L"\\System32;" + windows;
        environment.push_back(0);
        environment.push_back(0);
        BOOL launched = CreateProcessW(compiler.c_str(), command.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                       environment.data(), generation.c_str(), &startup, &process);
        DWORD launchError = launched ? 0 : GetLastError();
        CloseHandle(log);
        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);
        if (!launched) {
            result.log = "コンパイラを起動できませんでした。Windowsエラー: " + std::to_string(launchError);
            return result;
        }
        auto wait = WaitForSingleObject(process.hProcess, 120000);
        DWORD code = 1;
        if (wait == WAIT_OBJECT_0)
            GetExitCodeProcess(process.hProcess, &code);
        else {
            TerminateProcess(process.hProcess, 1);
            WaitForSingleObject(process.hProcess, 5000);
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::ifstream file(generation / "build.log", std::ios::binary);
        result.log.assign(std::istreambuf_iterator<char>(file), {});
        if (wait != WAIT_OBJECT_0)
            result.log += "\nコンパイラが120秒以内に終了しませんでした。";
        result.success =
            wait == WAIT_OBJECT_0 && code == 0 && std::filesystem::is_regular_file(result.dll);
        result.log += "\nコンパイラの終了コード: " + std::to_string(code);
        if (result.success)
            result.log += "\nビルドが完了しました。DLLを検査しています。";
    } catch (const std::exception& e) {
        result.log = e.what();
    }
    return result;
}
}
