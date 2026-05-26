#pragma once

#include <string>
#include <vector>

#include "core/EventBus.hpp"
#include "core/Session.hpp"
#include "tui/IRenderable.hpp"

namespace tcm {

struct SessionInfo {
    std::string id;
    std::string name;
    std::string host;     // host/device path
    std::string typeStr;  // "SSH", "SER", "TEL"
    SessionState state;
};

class SessionListView : public IRenderable {
public:
    SessionListView(std::vector<SessionInfo>& sessions, EventBus& bus);

    void render(Renderer& renderer) override;
    bool handleKey(int key) override;

    // Navigation
    void moveDown();
    void moveUp();
    int  getSelectedIndex() const;
    std::string getSelectedId() const;  // "" nếu empty

    // Search mode
    void enterSearchMode();
    void exitSearchMode();
    bool isSearchMode() const;
    void appendSearchChar(char c);
    void backspaceSearch();
    const std::string& getSearchQuery() const;

    // Get filtered sessions
    std::vector<SessionInfo> getFilteredSessions() const;

private:
    std::vector<SessionInfo>& m_sessions;
    EventBus&                 m_bus;
    int                       m_selected   = 0;
    bool                      m_searchMode = false;
    std::string               m_searchQuery;

    void clampSelected();
    bool matchesFilter(const SessionInfo& s) const;
    void renderRow(Renderer& r, int row, const SessionInfo& s, bool selected, int cols);
};

}  // namespace tcm
