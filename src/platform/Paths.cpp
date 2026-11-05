#include "platform/Paths.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace lt {
std::filesystem::path applicationDirectory() {
    std::vector<wchar_t> buffer(32768);
    auto count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!count || count >= buffer.size())
        throw std::runtime_error("実行ファイルの場所を取得できませんでした。");
    return std::filesystem::path(std::wstring(buffer.data(), count)).parent_path();
}
}
