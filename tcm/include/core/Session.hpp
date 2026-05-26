#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/IConnection.hpp"

namespace tcm {

// ---------------------------------------------------------------------------
// Session type and state enums
// ---------------------------------------------------------------------------
enum class SessionType  { SSH, Serial, Telnet };
enum class SessionState { Idle, Connecting, Connected, Error };

// ---------------------------------------------------------------------------
// Session — move-only (owns a unique_ptr<IConnection>)
// ---------------------------------------------------------------------------
struct Session {
    std::string                  id;
    std::string                  name;
    SessionType                  type  = SessionType::SSH;
    std::unique_ptr<IConnection> conn;
    SessionState                 state = SessionState::Idle;
    std::string                  notes;
    std::vector<std::string>     tags;

    Session()                          = default;
    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&)                 = default;
    Session& operator=(Session&&)      = default;
};

// ---------------------------------------------------------------------------
// UUID v4 generator — defined in Session.cpp
// ---------------------------------------------------------------------------
std::string generateUUID();

} // namespace tcm
