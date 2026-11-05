#pragma once
#include "core/Geometry.h"
#include <vector>

namespace lt {
enum class PointerKind { Move, Down, Up, MiddleDown, MiddleUp, Cancel };

struct PointerEvent {
    PointerKind kind;
    Vec2 screen;
};

std::vector<PointerEvent> takePointerEvents();
void installPointerEvents(void* window);
void uninstallPointerEvents();

struct CameraPan {
    bool active = false;
    Vec2 pointerStart, centerStart;

    void process(PointerEvent event, bool inside, bool allowed, Camera& camera) {
        if (event.kind == PointerKind::Cancel) {
            active = false;
            return;
        }
        if (event.kind == PointerKind::MiddleDown && inside && allowed) {
            active = true;
            pointerStart = event.screen;
            centerStart = camera.center;
        }
        if (active && (event.kind == PointerKind::Move || event.kind == PointerKind::MiddleUp)) {
            auto d = (event.screen - pointerStart) * (1 / camera.pixelsPerMeter);
            camera.center = centerStart + Vec2{-d.x, d.y};
        }
        if (event.kind == PointerKind::MiddleUp)
            active = false;
    }
};
}
