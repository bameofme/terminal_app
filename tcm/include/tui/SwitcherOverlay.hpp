#pragma once

#include <vector>

#include "core/EventBus.hpp"
#include "tui/IRenderable.hpp"
#include "tui/SessionListView.hpp"  // SessionInfo

namespace tcm {

class SwitcherOverlay : public IRenderable {
public:
    SwitcherOverlay(const std::vector<SessionInfo>& sessions, EventBus& bus);

    void render(Renderer& renderer) override;
    bool handleKey(int key) override;

    void show();
    void hide();
    bool isVisible() const;

private:
    const std::vector<SessionInfo>& m_sessions;
    EventBus&                       m_bus;
    bool                            m_visible = false;

    // Chỉ trả về sessions đang Connected
    std::vector<const SessionInfo*> getConnectedSessions() const;
};

}  // namespace tcm
