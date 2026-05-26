#pragma once

namespace tcm {

class Renderer; // forward declare

class IRenderable {
public:
    virtual void render(Renderer& renderer) = 0;
    virtual bool handleKey(int key) = 0; // return true nếu consumed
    virtual ~IRenderable() = default;
};

} // namespace tcm
