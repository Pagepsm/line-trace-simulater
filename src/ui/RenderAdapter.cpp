#include "ui/RenderAdapter.h"
#include <raylib.h>
#include <stdexcept>
#include "ui/PointerEvents.h"

namespace lt {
RenderAdapter::RenderAdapter() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1500, 940, "ライントレース・シミュレータ");
    if (!IsWindowReady())
        throw std::runtime_error("ウィンドウを作成できませんでした。");
    SetWindowMinSize(800, 520);
    SetTargetFPS(60);
    installPointerEvents(GetWindowHandle());
}

RenderAdapter::~RenderAdapter() {
    uninstallPointerEvents();
    CloseWindow();
}

bool RenderAdapter::shouldClose() const {
    return WindowShouldClose();
}

double RenderAdapter::frameTime() const {
    return GetFrameTime();
}

void RenderAdapter::beginFrame() {
    BeginDrawing();
    ClearBackground({22, 27, 36, 255});
}

void RenderAdapter::endFrame() {
    EndDrawing();
}
}
