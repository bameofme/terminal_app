#include "tui/SwitcherOverlay.hpp"

#include <string>

#include "core/EventBus.hpp"
#include "tui/Renderer.hpp"

namespace tcm {

SwitcherOverlay::SwitcherOverlay(const std::vector<SessionInfo>& sessions,
                                 EventBus&                       bus)
    : m_sessions(sessions), m_bus(bus) {}

void SwitcherOverlay::show() { m_visible = true; }

void SwitcherOverlay::hide() { m_visible = false; }

bool SwitcherOverlay::isVisible() const { return m_visible; }

std::vector<const SessionInfo*> SwitcherOverlay::getConnectedSessions() const {
    std::vector<const SessionInfo*> result;
    for (const auto& s : m_sessions) {
        if (s.state == SessionState::Connected) {
            result.push_back(&s);
        }
    }
    return result;
}

void SwitcherOverlay::render(Renderer& renderer) {
    if (!m_visible) return;

    auto connected = getConnectedSessions();

    // Popup dimensions
    const int popupW = 40;
    const int popupH = static_cast<int>(connected.size()) + 4;  // title + entries + footer + borders

    auto [cols, rows] = renderer.getSize();

    // Center the popup
    int x = (cols - popupW) / 2;
    int y = (rows - popupH) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    renderer.drawBox(x, y, popupW, popupH);

    // Title
    renderer.drawText(x + 2, y + 1, "Switch session", ColorPair::TitleBar);

    // Session list (1-based)
    for (int i = 0; i < static_cast<int>(connected.size()); ++i) {
        const SessionInfo* s = connected[static_cast<size_t>(i)];
        std::string line = "  " + std::to_string(i + 1) + "  ";
        // Pad name to fixed width
        std::string name = s->name;
        if (name.size() > 20) name = name.substr(0, 20);
        line += name;
        // Pad to align indicator
        while (static_cast<int>(line.size()) < 30) line += ' ';
        line += "\xe2\x97\x8f";  // UTF-8 '●' (U+25CF)
        renderer.drawTextTruncated(x + 1, y + 2 + i, popupW - 2, line,
                                   ColorPair::Connected);
    }

    // Footer
    renderer.drawText(x + 2, y + popupH - 2, "1-9: switch  Esc: cancel",
                      ColorPair::StatusBar);
}

bool SwitcherOverlay::handleKey(int key) {
    if (!m_visible) return false;

    // ESC
    if (key == 27) {
        hide();
        return true;
    }

    // Digit 1-9
    if (key >= '1' && key <= '9') {
        auto connected = getConnectedSessions();
        int  idx       = key - '1';  // 0-based
        if (idx < static_cast<int>(connected.size())) {
            SessionSwitchedEvent ev;
            ev.fromId = "";
            ev.toId   = connected[static_cast<size_t>(idx)]->id;
            m_bus.publish(ev);
        }
        // consume regardless of whether index is valid
        hide();
        return true;
    }

    // Other keys — not consumed
    return false;
}

}  // namespace tcm
