#include "tui/SessionListView.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "tui/Renderer.hpp"

// ncurses key codes (avoid pulling in ncurses.h into headers)
static constexpr int K_DOWN      = 258;
static constexpr int K_UP        = 259;
static constexpr int K_ENTER     = 343;
static constexpr int K_BACKSPACE = 263;
static constexpr int K_ESC       = 27;

namespace tcm {

SessionListView::SessionListView(std::vector<SessionInfo>& sessions, EventBus& bus)
    : m_sessions(sessions), m_bus(bus) {}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------
bool SessionListView::matchesFilter(const SessionInfo& s) const {
    if (m_searchQuery.empty()) return true;

    auto toLower = [](const std::string& in) {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    };

    std::string q    = toLower(m_searchQuery);
    std::string name = toLower(s.name);
    std::string host = toLower(s.host);
    return name.find(q) != std::string::npos || host.find(q) != std::string::npos;
}

std::vector<SessionInfo> SessionListView::getFilteredSessions() const {
    std::vector<SessionInfo> result;
    for (const auto& s : m_sessions) {
        if (matchesFilter(s)) result.push_back(s);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
void SessionListView::clampSelected() {
    auto filtered = getFilteredSessions();
    int  count    = static_cast<int>(filtered.size());
    if (count == 0) {
        m_selected = 0;
        return;
    }
    if (m_selected < 0) m_selected = 0;
    if (m_selected >= count) m_selected = count - 1;
}

void SessionListView::moveDown() {
    ++m_selected;
    clampSelected();
}

void SessionListView::moveUp() {
    --m_selected;
    clampSelected();
}

int SessionListView::getSelectedIndex() const { return m_selected; }

std::string SessionListView::getSelectedId() const {
    auto filtered = getFilteredSessions();
    if (filtered.empty()) return "";
    if (m_selected < 0 || m_selected >= static_cast<int>(filtered.size())) return "";
    return filtered[m_selected].id;
}

// ---------------------------------------------------------------------------
// Search mode
// ---------------------------------------------------------------------------
void SessionListView::enterSearchMode() {
    m_searchMode = true;
}

void SessionListView::exitSearchMode() {
    m_searchMode  = false;
    m_searchQuery = "";
    clampSelected();
}

bool SessionListView::isSearchMode() const { return m_searchMode; }

void SessionListView::appendSearchChar(char c) {
    m_searchQuery += c;
    clampSelected();
}

void SessionListView::backspaceSearch() {
    if (!m_searchQuery.empty()) {
        m_searchQuery.pop_back();
        clampSelected();
    }
}

const std::string& SessionListView::getSearchQuery() const { return m_searchQuery; }

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------
void SessionListView::renderRow(Renderer& r, int row, const SessionInfo& s,
                                 bool selected, int cols) {
    if (selected) {
        r.fillLine(row, ColorPair::Highlighted);
    }

    ColorPair textColor = selected ? ColorPair::Highlighted : ColorPair::Normal;

    // Column 1: [TYPE]
    std::string typeTag = "[" + s.typeStr + "]";
    r.drawText(1, row, typeTag, textColor);

    // Column 8: name (truncated to 15 chars)
    r.drawTextTruncated(8, row, 15, s.name, textColor);

    // Column 25: host (truncated to 20 chars)
    r.drawTextTruncated(25, row, 20, s.host, textColor);

    // Column 47: status indicator (colored unless selected)
    std::string  statusStr;
    ColorPair    statusColor;
    switch (s.state) {
        case SessionState::Connected:
            statusStr   = "\xe2\x97\x8f Connected";  // ●
            statusColor = selected ? ColorPair::Highlighted : ColorPair::Connected;
            break;
        case SessionState::Error:
            statusStr   = "\xe2\x9c\x97 Error";  // ✗
            statusColor = selected ? ColorPair::Highlighted : ColorPair::Error;
            break;
        default:  // Idle / Connecting
            statusStr   = "\xe2\x97\x8b Idle";  // ○
            statusColor = selected ? ColorPair::Highlighted : ColorPair::Idle;
            break;
    }
    r.drawText(47, row, statusStr, statusColor);

    (void)cols;  // reserved for future truncation
}

// ---------------------------------------------------------------------------
// render()
// ---------------------------------------------------------------------------
void SessionListView::render(Renderer& renderer) {
    renderer.drawTitleBar("TCM v1.0 \xe2\x80\x94 Terminal Connection Manager");

    auto [cols, rows] = renderer.getSize();
    auto filtered     = getFilteredSessions();

    if (filtered.empty()) {
        const std::string msg = "No sessions. Press [a] to add.";
        int x = (cols - static_cast<int>(msg.size())) / 2;
        int y = rows / 2;
        if (x < 0) x = 0;
        if (y < 1) y = 1;
        renderer.drawText(x, y, msg, ColorPair::Normal);
    } else {
        for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
            int rowY = 1 + i;
            if (rowY >= rows - 1) break;
            renderRow(renderer, rowY, filtered[i], i == m_selected, cols);
        }
    }

    // Status bar
    if (m_searchMode) {
        renderer.drawStatusBar("Search: " + m_searchQuery + "_");
    } else {
        renderer.drawStatusBar("[Enter] connect  [a] add  [d] delete  [/] search  [q] quit");
    }
}

// ---------------------------------------------------------------------------
// handleKey()
// ---------------------------------------------------------------------------
bool SessionListView::handleKey(int key) {
    // Search mode intercepts printable chars first
    if (m_searchMode) {
        if (key == K_ESC) {
            exitSearchMode();
            return true;
        }
        if (key == K_BACKSPACE || key == 127 || key == 8) {
            backspaceSearch();
            return true;
        }
        if (key >= 32 && key < 127) {
            appendSearchChar(static_cast<char>(key));
            return true;
        }
        // fall through for navigation keys
    }

    switch (key) {
        case 'j':
        case K_DOWN:
            moveDown();
            return true;

        case 'k':
        case K_UP:
            moveUp();
            return true;

        case '/':
            enterSearchMode();
            return true;

        case K_ESC:
            if (m_searchMode) {
                exitSearchMode();
                return true;
            }
            return false;

        case 10:        // '\n'
        case K_ENTER:   // 343
            // Do NOT consume Enter — let Application handle connect
            return false;

        default:
            return false;
    }
}

}  // namespace tcm
