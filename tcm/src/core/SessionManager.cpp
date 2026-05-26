#include "core/SessionManager.hpp"

#include <algorithm>

namespace tcm {

SessionManager::SessionManager(EventBus& bus) : m_bus(bus) {}

// ---------------------------------------------------------------------------
std::string SessionManager::add(Session session) {
    std::string id = session.id;
    m_sessions.push_back(std::move(session));
    m_bus.publish(SessionAddedEvent{id});
    return id;
}

// ---------------------------------------------------------------------------
bool SessionManager::remove(const std::string& id) {
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                           [&id](const Session& s) { return s.id == id; });
    if (it == m_sessions.end()) return false;

    // Disconnect before erasing
    if (it->conn && it->conn->getState() == ConnectionState::Connected) {
        it->conn->disconnect();
    }

    m_sessions.erase(it);

    // Clear active if it was this session
    if (m_activeId && *m_activeId == id) {
        m_activeId = std::nullopt;
    }

    m_bus.publish(SessionRemovedEvent{id});
    return true;
}

// ---------------------------------------------------------------------------
bool SessionManager::setActive(const std::string& id) {
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                           [&id](const Session& s) { return s.id == id; });
    if (it == m_sessions.end()) return false;

    m_activeId = id;
    return true;
}

// ---------------------------------------------------------------------------
Session* SessionManager::getActive() {
    if (!m_activeId) return nullptr;
    return findById(*m_activeId);
}

// ---------------------------------------------------------------------------
const std::string& SessionManager::getActiveId() const {
    static const std::string kEmpty;
    if (!m_activeId) return kEmpty;
    return *m_activeId;
}

// ---------------------------------------------------------------------------
Session* SessionManager::findById(const std::string& id) {
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                           [&id](const Session& s) { return s.id == id; });
    if (it == m_sessions.end()) return nullptr;
    return &(*it);
}

// ---------------------------------------------------------------------------
void SessionManager::forEach(std::function<void(Session&)> cb) {
    for (auto& s : m_sessions) {
        cb(s);
    }
}

// ---------------------------------------------------------------------------
size_t SessionManager::count() const {
    return m_sessions.size();
}

} // namespace tcm
