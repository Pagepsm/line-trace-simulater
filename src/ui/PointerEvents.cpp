#include "ui/PointerEvents.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <stdexcept>

namespace lt {
namespace {
std::vector<PointerEvent> events;
HWND target = nullptr;
WNDPROC previous = nullptr;

LRESULT CALLBACK inputProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    // イベントごとの座標を使い、速いドラッグでも途中の点を拾う。
    Vec2 p{static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
    if (message == WM_MOUSEMOVE)
        events.push_back({PointerKind::Move, p});
    else if (message == WM_LBUTTONDOWN) {
        events.push_back({PointerKind::Down, p});
        SetCapture(window);
    } else if (message == WM_LBUTTONUP) {
        events.push_back({PointerKind::Up, p});
        if (GetCapture() == window)
            ReleaseCapture();
    } else if (message == WM_MBUTTONDOWN) {
        events.push_back({PointerKind::MiddleDown, p});
        SetCapture(window);
    } else if (message == WM_MBUTTONUP) {
        events.push_back({PointerKind::MiddleUp, p});
        if (GetCapture() == window)
            ReleaseCapture();
    } else if (message == WM_KILLFOCUS)
        events.push_back({PointerKind::Cancel, {}});

    return CallWindowProcW(previous, window, message, wparam, lparam);
}
}

void installPointerEvents(void* window) {
    target = static_cast<HWND>(window);
    previous = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(target, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(inputProcedure)));
    if (!previous)
        throw std::runtime_error("マウス入力の初期化に失敗しました。");
}

void uninstallPointerEvents() {
    if (target && previous)
        SetWindowLongPtrW(target, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
    target = nullptr;
    previous = nullptr;
    events.clear();
}

std::vector<PointerEvent> takePointerEvents() {
    auto result = std::move(events);
    events.clear();
    return result;
}
}
