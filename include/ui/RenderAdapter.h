#pragma once

namespace lt {
class RenderAdapter {
  public:
    RenderAdapter();
    ~RenderAdapter();
    RenderAdapter(const RenderAdapter&) = delete;
    RenderAdapter& operator=(const RenderAdapter&) = delete;
    bool shouldClose() const;
    double frameTime() const;
    void beginFrame();
    void endFrame();
};
}
