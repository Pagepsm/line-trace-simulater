#include "app/App.h"
#include <exception>
#include <iostream>
#include <string_view>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

int main(int argc, char** argv) {
    try {
        return lt::runApp(argc > 1 && std::string_view(argv[1]) == "--smoke");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        auto size = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
        std::wstring message(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, message.data(), size);
        MessageBoxW(nullptr, message.c_str(), L"起動できません", MB_OK | MB_ICONERROR);
        return 1;
    }
}
