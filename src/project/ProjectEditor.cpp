#include "project/ProjectEditor.h"
#include <cmath>

namespace lt {
static bool valid(Vec2 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 10000 &&
           std::abs(p.y) <= 10000;
}

static bool validWidth(float w) {
    return std::isfinite(w) && w >= .001f && w <= .2f;
}

bool ProjectEditor::apply(const EditCommand& c) {
    if (c.kind == EditKind::Undo) {
        if (undo_.empty())
            return false;
        redo_.push_back(model_);
        model_ = std::move(undo_.back());
        undo_.pop_back();
        return true;
    }
    if (c.kind == EditKind::Redo) {
        if (redo_.empty())
            return false;
        undo_.push_back(model_);
        model_ = std::move(redo_.back());
        redo_.pop_back();
        return true;
    }
    auto next = model_;
    auto& strokes = next.track.strokes;
    auto selected = [&] { return c.index >= 0 && static_cast<size_t>(c.index) < strokes.size(); };
    switch (c.kind) {
    case EditKind::Add:
        if (c.stroke.points.empty())
            return false;
        for (auto p : c.stroke.points)
            if (!valid(p.position) || !validWidth(p.width))
                return false;
        strokes.push_back(c.stroke);
        break;
    case EditKind::Remove:
        if (!selected())
            return false;
        strokes.erase(strokes.begin() + c.index);
        break;
    case EditKind::Move:
        if (!selected() || !valid(c.delta) || dot(c.delta, c.delta) < 1e-12f)
            return false;
        for (auto& p : strokes[c.index].points) {
            p.position = p.position + c.delta;
            if (!valid(p.position))
                return false;
        }
        break;
    case EditKind::Width: {
        if (!selected() || !validWidth(c.width))
            return false;
        bool changed = false;
        for (auto& p : strokes[c.index].points) {
            changed |= p.width != c.width;
            p.width = c.width;
        }
        if (!changed)
            return false;
        break;
    }
    case EditKind::Start:
        if (!valid(c.pose.position) || !std::isfinite(c.pose.angle))
            return false;
        if (c.pose.position.x == next.start.position.x &&
            c.pose.position.y == next.start.position.y && c.pose.angle == next.start.angle)
            return false;
        next.start = c.pose;
        break;
    case EditKind::Clear:
        if (strokes.empty())
            return false;
        strokes.clear();
        break;
    default:
        return false;
    }
    if (undo_.size() == historyLimit)
        undo_.erase(undo_.begin());
    undo_.push_back(model_);
    model_ = std::move(next);
    redo_.clear();
    return true;
}
}
