#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/EventBus.hpp"
#include "core/Session.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// SessionManager — owns a list of Sessions and tracks the active one
// ---------------------------------------------------------------------------
class SessionManager {
public:
    explicit SessionManager(EventBus& bus);

    /// Appends session to the list and returns its id.
    std::string add(Session session);

    /// Removes session by id. Calls disconnect() first if Connected.
    /// Resets active id if the removed session was active.
    /// Returns false if id not found.
    bool remove(const std::string& id);

    /// Sets the active session. Returns false if id not found.
    bool setActive(const std::string& id);

    /// Returns pointer to active session, nullptr if none.
    Session* getActive();

    /// Returns current active id, empty string if none.
    const std::string& getActiveId() const;

    /// Returns pointer to session with given id, nullptr if not found.
    Session* findById(const std::string& id);

    /// Calls cb for every session.
    void forEach(std::function<void(Session&)> cb);

    size_t count() const;

private:
    std::vector<Session>        m_sessions;
    std::optional<std::string>  m_activeId;
    EventBus&                   m_bus;
};

} // namespace tcm
